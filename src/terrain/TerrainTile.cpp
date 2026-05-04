#include "TerrainTile.h"
#include <QImage>
#include <QDebug>
#include <algorithm>
#include <cmath>

// ─────────────────────────────────────────────
// GEBCO(ESRI Ocean Base) 픽셀 1개 → 높이값(m)
//
// 육지/바다 판단:
//   B > (R+G)/2 + 15  AND  B > 70  →  바다
//   그 외                           →  육지 (1m 고정)
//
// 수심 추정 (바다 픽셀):
//   밝기(luminance)를 0~3000m 범위로 선형 매핑
//   밝은 파랑(lum≈0.80) → 얕음(~0m)
//   어두운 파랑(lum≈0.10) → 깊음(~3000m)
// ─────────────────────────────────────────────
static float pixelToHeight(int r, int g, int b)
{
    float warmth = (r + g) * 0.5f;
    bool isWater = (static_cast<float>(b) > warmth + 15.0f)
                && (b > 70);

    if (!isWater) return 1.0f;  // 육지: 1m 고정

    float lum = (r * 0.299f + g * 0.587f + b * 0.114f) / 255.0f;

    const float LUM_SHALLOW = 0.80f;   // 얕은 바다 밝기
    const float LUM_DEEP    = 0.10f;   // 깊은 바다 밝기
    const float MAX_DEPTH   = 3000.0f;

    float t = (lum - LUM_DEEP) / (LUM_SHALLOW - LUM_DEEP);
    t = std::max(0.0f, std::min(1.0f, t));

    // 바다 픽셀은 최소 -1m 보장 (h=0이 되면 buildMesh에서 육지로 오분류)
    return std::min(-1.0f, -MAX_DEPTH * (1.0f - t));
}

// ─────────────────────────────────────────────
// GEBCO PNG → TerrainTile
// ─────────────────────────────────────────────
TerrainTile TerrainTile::decodeGebco(int z, int x, int y, const QByteArray& pngData)
{
    TerrainTile tile;
    tile.tileZ = z;
    tile.tileX = x;
    tile.tileY = y;

    qDebug("TerrainTile::decodeGebco  z=%d x=%d y=%d  dataSize=%d bytes",
           z, x, y, pngData.size());

    if (pngData.isEmpty()) {
        qWarning("TerrainTile: 수신 데이터 없음");
        return tile;
    }

    QImage img;
    // 포맷 힌트 없이 자동 감지 (ESRI가 PNG/JPEG 혼용)
    if (!img.loadFromData(pngData)) {
        qWarning("TerrainTile: 이미지 로드 실패 (앞 4바이트: %02X %02X %02X %02X)",
                 (uint8_t)pngData[0], (uint8_t)pngData[1],
                 (uint8_t)pngData[2], (uint8_t)pngData[3]);
        return tile;
    }

    qDebug("TerrainTile: 이미지 로드 성공  %dx%d  format=%d",
           img.width(), img.height(), (int)img.format());

    img = img.convertToFormat(QImage::Format_RGB32);
    tile.width  = img.width();
    tile.height = img.height();
    tile.heights.resize(tile.width * tile.height);

    // 1차 패스: 모든 픽셀 높이값 계산
    for (int row = 0; row < tile.height; ++row) {
        const QRgb* line = reinterpret_cast<const QRgb*>(img.constScanLine(row));
        for (int col = 0; col < tile.width; ++col) {
            QRgb px = line[col];
            tile.heights[row * tile.width + col] =
                pixelToHeight(qRed(px), qGreen(px), qBlue(px));
        }
    }

    // 2차 패스: 육지 주변 1km 수심 → -10m 고정
    // 분리 가능한 팽창(separable dilation) — O(W×H), 기존 O(W×H×r²) 대비 ~70배 빠름
    const int W = tile.width;
    const int H = tile.height;
    const int pixRadius = 8;  // 1픽셀 ≈ 125m → 8픽셀 ≈ 1km

    // 육지 마스크 생성
    QVector<bool> isLand(W * H, false);
    for (int i = 0; i < W * H; ++i)
        isLand[i] = (tile.heights[i] > 0.0f);

    // 가로 팽창
    QVector<bool> dilated(W * H, false);
    for (int row = 0; row < H; ++row) {
        for (int col = 0; col < W; ++col) {
            if (!isLand[row * W + col]) continue;
            for (int c = std::max(0, col - pixRadius); c <= std::min(W-1, col + pixRadius); ++c)
                dilated[row * W + c] = true;
        }
    }

    // 세로 팽창 → nearLand 마스크 완성
    QVector<bool> nearLand(W * H, false);
    for (int col = 0; col < W; ++col) {
        for (int row = 0; row < H; ++row) {
            if (!dilated[row * W + col]) continue;
            for (int r = std::max(0, row - pixRadius); r <= std::min(H-1, row + pixRadius); ++r)
                nearLand[r * W + col] = true;
        }
    }

    // 적용
    for (int i = 0; i < W * H; ++i)
        if (tile.heights[i] < 0.0f && nearLand[i])
            tile.heights[i] = -10.0f;

    qDebug("TerrainTile: min=%.0fm max=%.0fm", tile.minHeight(), tile.maxHeight());
    return tile;
}

float TerrainTile::minHeight() const
{
    if (heights.isEmpty()) return 0.0f;
    return *std::min_element(heights.begin(), heights.end());
}

float TerrainTile::maxHeight() const
{
    if (heights.isEmpty()) return 0.0f;
    return *std::max_element(heights.begin(), heights.end());
}
