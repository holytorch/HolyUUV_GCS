#pragma once

#include <QVector>
#include <QImage>

// GEBCO(ESRI Ocean Base) 픽셀 색 → 높이값 배열로 디코딩
// 색 분석 기준:
//   파란 계열 (B 우세) → 수심, 밝을수록 얕음 / 어두울수록 깊음
//   육지 계열 (R/G 우세) → +5m 고정
struct TerrainTile {
    int tileZ = 0;
    int tileX = 0;
    int tileY = 0;

    // 256×256 격자의 높이값 (미터, 음수=해저, 양수=육지)
    QVector<float> heights;
    int width  = 0;
    int height = 0;

    bool isValid() const { return !heights.isEmpty(); }

    // GEBCO 타일 PNG → 색 기반 높이값 디코딩
    static TerrainTile decodeGebco(int z, int x, int y, const QByteArray& pngData);

    float heightAt(int col, int row) const {
        if (col < 0 || row < 0 || col >= width || row >= height) return 0.0f;
        return heights[row * width + col];
    }

    float minHeight() const;
    float maxHeight() const;
};
