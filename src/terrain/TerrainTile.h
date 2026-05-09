#pragma once

#include <QVector>

// ─────────────────────────────────────────────────────────────────────────────
// TerrainTile
// 3D 지형 메시 생성에 사용되는 2D 높이 배열 데이터.
// heights[row * width + col] 형태로 저장된다.
//
// 좌표 규약:
//   col(x) — 서→동 방향 (3D X축)
//   row(y) — 북→남 방향 (3D Z축)
//   높이값: 육지 = LAND_H(+1.0), 수역 = SEA_DEPTH(−1000.0)
// ─────────────────────────────────────────────────────────────────────────────
struct TerrainTile {
    int tileZ = 0;
    int tileX = 0;
    int tileY = 0;

    QVector<float> heights;
    int width  = 0;
    int height = 0;

    bool isValid() const { return !heights.isEmpty(); }

    float heightAt(int col, int row) const {
        if (col < 0 || row < 0 || col >= width || row >= height) return 0.0f;
        return heights[row * width + col];
    }
};
