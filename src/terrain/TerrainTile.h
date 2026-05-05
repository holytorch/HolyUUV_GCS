#pragma once

#include <QVector>

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
