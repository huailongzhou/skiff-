// UI 侧:把 Box2D 调试绘制接到 skiff::CanvasContext。应用核不依赖本文件。
#pragma once

#include "box2d/box2d.h"
#include "skiff/skiff.hpp"
#include "app_core/physics_scene.hpp"

namespace pnd {
namespace physics {

inline uint32_t toRgb(const b2Color& c) {
    int r = (int)(c.r * 255.0f + 0.5f);
    int g = (int)(c.g * 255.0f + 0.5f);
    int b = (int)(c.b * 255.0f + 0.5f);
    if (r < 0) r = 0; if (r > 255) r = 255;
    if (g < 0) g = 0; if (g > 255) g = 255;
    if (b < 0) b = 0; if (b > 255) b = 255;
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

class CanvasDebugDraw : public b2Draw {
public:
    CanvasDebugDraw() : ctx_(0), scale_(40.0f), height_(0) {}

    void begin(skiff::CanvasContext& ctx, float scale) {
        ctx_ = &ctx;
        scale_ = scale;
        height_ = (float)ctx.height();
    }
    void end() { ctx_ = 0; }

    void DrawPolygon(const b2Vec2* vertices, int32 vertexCount,
                     const b2Color& color) {
        strokePoly(vertices, vertexCount, color, 1);
    }

    void DrawSolidPolygon(const b2Vec2* vertices, int32 vertexCount,
                          const b2Color& color) {
        if (!ctx_ || !vertices || vertexCount < 3) return;
        enum { kMax = 16 };
        skiff::CanvasPoint pts[kMax];
        const int n = vertexCount > kMax ? kMax : vertexCount;
        for (int i = 0; i < n; ++i) pts[i] = toPt(vertices[i]);
        ctx_->fillPolygon(pts, n, toRgb(color));
        strokePoly(vertices, vertexCount, color, 1);
    }

    void DrawCircle(const b2Vec2& center, float radius, const b2Color& color) {
        if (!ctx_) return;
        const skiff::CanvasPoint c = toPt(center);
        ctx_->strokeArc(c.x, c.y, toPx(radius), 0, 360, toRgb(color), 2);
    }

    void DrawSolidCircle(const b2Vec2& center, float radius, const b2Vec2& axis,
                         const b2Color& color) {
        (void)axis;
        if (!ctx_) return;
        const skiff::CanvasPoint c = toPt(center);
        const int r = toPx(radius);
        ctx_->fillCircle(c.x, c.y, r, toRgb(color));
        ctx_->strokeArc(c.x, c.y, r, 0, 360, toRgb(color), 1);
    }

    void DrawSegment(const b2Vec2& p1, const b2Vec2& p2, const b2Color& color) {
        if (!ctx_) return;
        skiff::CanvasPoint line[2] = { toPt(p1), toPt(p2) };
        ctx_->strokeLine(line, 2, toRgb(color), 2);
    }

    void DrawTransform(const b2Transform& xf) {
        if (!ctx_) return;
        const float k = 0.35f;
        skiff::CanvasPoint o = toPt(xf.p);
        skiff::CanvasPoint x[2] = { o, toPt(xf.p + k * xf.q.GetXAxis()) };
        skiff::CanvasPoint y[2] = { o, toPt(xf.p + k * xf.q.GetYAxis()) };
        ctx_->strokeLine(x, 2, 0xE85D4C, 1);
        ctx_->strokeLine(y, 2, 0x3DDC84, 1);
    }

    void DrawPoint(const b2Vec2& p, float size, const b2Color& color) {
        if (!ctx_) return;
        const skiff::CanvasPoint c = toPt(p);
        int r = toPx(size * 0.5f);
        if (r < 2) r = 2;
        ctx_->fillCircle(c.x, c.y, r, toRgb(color));
    }

private:
    skiff::CanvasPoint toPt(const b2Vec2& v) const {
        return skiff::CanvasPoint((int)(v.x * scale_ + 0.5f),
                                  (int)(height_ - v.y * scale_ + 0.5f));
    }
    int toPx(float meters) const {
        int p = (int)(meters * scale_ + 0.5f);
        return p < 1 ? 1 : p;
    }
    void strokePoly(const b2Vec2* vertices, int32 vertexCount,
                    const b2Color& color, int width) {
        if (!ctx_ || !vertices || vertexCount < 2) return;
        enum { kMax = 16 };
        skiff::CanvasPoint pts[kMax + 1];
        const int n = vertexCount > kMax ? kMax : vertexCount;
        for (int i = 0; i < n; ++i) pts[i] = toPt(vertices[i]);
        pts[n] = pts[0];
        ctx_->strokeLine(pts, n + 1, toRgb(color), width);
    }

    skiff::CanvasContext* ctx_;
    float scale_;
    float height_;
};

inline void paintScene(skiff::CanvasContext& c, app::PhysicsScene& scene) {
    c.clear(0x1A1F2E);
    CanvasDebugDraw draw;
    draw.begin(c, scene.scale());
    scene.debugDraw(draw);
    draw.end();
}

} // namespace physics
} // namespace pnd
