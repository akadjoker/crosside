#pragma once
#include "math.hpp"
#include <raylib.h>
#include <rlgl.h>

const bool FIX_ARTIFACTS_BY_STRECHING_TEXEL = true;

struct rVertex
{
    float x, y, z;
    Color col;
    float tx, ty;
};

struct rQuad
{
    rVertex v[4];
    Texture2D tex;
    int blend;
};

void RenderQuad(const rQuad *quad);
void RenderClipSize(Texture2D texture, float x, float y, float width, float height, Rectangle clip, bool flipX, bool flipY, Color color, int blend);
void RenderTexturePivotVertices(Texture2D texture, int x_pivot, int y_pivot, float spin, Rectangle clip, bool flipx, bool flipy, float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4, Color c);
void RenderNormal(Texture2D texture, float x, float y, int blend);
void RenderTexturePivotRotateSize(Texture2D texture, int x_pivot, int y_pivot, float x, float y, float spin, float size, bool flipx, bool flipy, Color c);
void RenderQuadUV(float x, float y, float w, float h, float uv_x1, float uv_y1, float uv_x2, float uv_y2, Texture2D tex, Color tint = WHITE);
void RenderTexturePivotRotateSizeXY(Texture2D texture, int x_pivot, int y_pivot, Rectangle clip, float x, float y, float spin, float size_x, float size_y, bool flipx, bool flipy, Color c);
void RenderTransform(Texture2D texture, const Matrix2D *matrix, int blend);
void RenderTransformSizeClip(Texture2D texture, int width, int height, Rectangle clip, bool flipX, bool flipY, Color color, const Matrix2D *matrix, int blend);
void RenderTransformFlip(Texture2D texture, Rectangle clip, bool flipX, bool flipY, Color color, const Matrix2D *matrix, int blend);
void RenderTransformFlipClip(Texture2D texture, Rectangle clip, bool flipX, bool flipY, Color color, const Matrix2D *matrix, int blend);
void RenderTransformFlipClipOffset(Texture2D texture, const Vec2 &offset, float width, float height, Rectangle clip, bool flipX, bool flipY, Color color, const Matrix2D *matrix, int blend);