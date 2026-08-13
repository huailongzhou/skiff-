// Canvas + Box2D 物理场景(PND 物理页 / physics_sdl 共用)。
// 页面代码通过 Sim 操作世界,不直接碰 LVGL。
#pragma once

#include "box2d/box2d.h"
#include "skiff/skiff.hpp"

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

// Box2D 调试绘制 → skiff::CanvasContext(Y 轴翻转:物理朝上,画布朝下)
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
        (void)axis;  // Box2D 调试绘制会给朝向轴,画面上不必画半径线
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

class Sim {
public:
    Sim(int canvasW, int canvasH, float scale = 40.0f)
        : world_(0), canvasW_(canvasW), canvasH_(canvasH), scale_(scale) {
        reset();
    }
    ~Sim() { delete world_; }

    int canvasW() const { return canvasW_; }
    int canvasH() const { return canvasH_; }

    void reset() {
        delete world_;
        world_ = new b2World(b2Vec2(0.0f, -10.0f));
        world_->SetDebugDraw(&draw_);
        draw_.SetFlags(b2Draw::e_shapeBit | b2Draw::e_jointBit);

        const float worldW = canvasW_ / scale_;
        const float worldH = canvasH_ / scale_;

        addStaticBox(worldW * 0.5f, 0.25f, worldW * 0.5f, 0.25f);
        addStaticBox(0.25f, worldH * 0.5f, 0.25f, worldH * 0.5f);
        addStaticBox(worldW - 0.25f, worldH * 0.5f, 0.25f, worldH * 0.5f);

        for (int row = 0; row < 5; ++row) {
            const int count = 5 - row;
            for (int i = 0; i < count; ++i) {
                const float x = worldW * 0.5f + (i - (count - 1) * 0.5f) * 1.05f;
                const float y = 1.2f + row * 1.05f;
                addBox(x, y, 0.5f, 0.5f, 0.15f);
            }
        }

        addCircle(worldW * 0.20f, worldH * 0.62f, 0.4f, 0.65f);
        addCircle(worldW * 0.26f, worldH * 0.74f, 0.35f, 0.8f);
        addCircle(worldW * 0.78f, worldH * 0.56f, 0.45f, 0.5f);
        addPendulum(worldW * 0.33f, worldH * 0.90f, 2.4f);
    }

    void step(float dt) {
        if (world_ && dt > 0.0f) world_->Step(dt, 8, 3);
    }

    void spawnAtCanvas(int px, int py, bool circle) {
        const float worldW = canvasW_ / scale_;
        const float worldH = canvasH_ / scale_;
        const float x = (float)px / scale_;
        const float y = (canvasH_ - (float)py) / scale_;
        if (x < 0.6f || x > worldW - 0.6f || y < 0.6f || y > worldH - 0.4f) {
            return;
        }
        if (circle) addCircle(x, y, 0.35f, 0.55f);
        else addBox(x, y, 0.4f, 0.4f, 0.25f);
    }

    void paint(skiff::CanvasContext& c) {
        c.clear(0x1A1F2E);
        draw_.begin(c, scale_);
        world_->DebugDraw();
        draw_.end();
    }

private:
    void addStaticBox(float x, float y, float hx, float hy) {
        b2BodyDef def;
        def.position.Set(x, y);
        b2Body* body = world_->CreateBody(&def);
        b2PolygonShape shape;
        shape.SetAsBox(hx, hy);
        body->CreateFixture(&shape, 0.0f);
    }

    void addBox(float x, float y, float hx, float hy, float restitution) {
        b2BodyDef def;
        def.type = b2_dynamicBody;
        def.position.Set(x, y);
        def.angle = 0.05f;
        b2Body* body = world_->CreateBody(&def);
        b2PolygonShape shape;
        shape.SetAsBox(hx, hy);
        b2FixtureDef fd;
        fd.shape = &shape;
        fd.density = 1.0f;
        fd.friction = 0.4f;
        fd.restitution = restitution;
        body->CreateFixture(&fd);
    }

    void addCircle(float x, float y, float r, float restitution) {
        b2BodyDef def;
        def.type = b2_dynamicBody;
        def.position.Set(x, y);
        b2Body* body = world_->CreateBody(&def);
        b2CircleShape shape;
        shape.m_radius = r;
        b2FixtureDef fd;
        fd.shape = &shape;
        fd.density = 0.8f;
        fd.friction = 0.3f;
        fd.restitution = restitution;
        body->CreateFixture(&fd);
    }

    void addPendulum(float ax, float ay, float length) {
        b2BodyDef anchorDef;
        anchorDef.position.Set(ax, ay);
        b2Body* anchor = world_->CreateBody(&anchorDef);

        b2BodyDef bobDef;
        bobDef.type = b2_dynamicBody;
        bobDef.position.Set(ax + length, ay);
        b2Body* bob = world_->CreateBody(&bobDef);
        b2CircleShape bobShape;
        bobShape.m_radius = 0.4f;
        b2FixtureDef bobFd;
        bobFd.shape = &bobShape;
        bobFd.density = 1.2f;
        bobFd.friction = 0.2f;
        bobFd.restitution = 0.2f;
        bob->CreateFixture(&bobFd);

        b2RevoluteJointDef jd;
        jd.Initialize(anchor, bob, b2Vec2(ax, ay));
        world_->CreateJoint(&jd);
    }

    b2World* world_;
    CanvasDebugDraw draw_;
    int canvasW_;
    int canvasH_;
    float scale_;
};

} // namespace physics
} // namespace pnd
