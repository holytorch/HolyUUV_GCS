#pragma once

#include <QVector>

// ─────────────────────────────────────────────────────────────────────────────
// TerrainTile
// A 2D height-array used to build a 3D terrain mesh.
// Stored as heights[row * width + col].
//
// Coordinate convention:
//   col(x) — west→east (3D X axis)
//   row(y) — north→south (3D Z axis)
//   height value: land = LAND_H (+1.0), water = SEA_DEPTH (−1000.0)
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
