// PhysicsScene:Box2D 物理场景(无 UI)。命令改状态,tick 步进,notify 通知观察者。
#pragma once

#include <cstdint>

#include "box2d/box2d.h"
#include "scene.hpp"

namespace app {

class PhysicsScene : public Scene {
public:
    PhysicsScene(int canvasW, int canvasH, float scale = 40.0f)
        : world_(0), canvasW_(canvasW), canvasH_(canvasH), scale_(scale),
          acc_(0.0f), frame_(0), paused_(false), dropCircle_(false) {
        rebuildWorld_();
    }

    ~PhysicsScene() { delete world_; }

    PhysicsScene(const PhysicsScene&) = delete;
    PhysicsScene& operator=(const PhysicsScene&) = delete;

    const char* name() const { return "physics"; }

    int canvasW() const { return canvasW_; }
    int canvasH() const { return canvasH_; }
    float scale() const { return scale_; }
    uint64_t frame() const { return frame_; }
    bool paused() const { return paused_; }
    bool dropCircle() const { return dropCircle_; }

    int bodyCount() const {
        if (!world_) return 0;
        int n = 0;
        for (b2Body* b = world_->GetBodyList(); b; b = b->GetNext()) ++n;
        return n;
    }

    void setPaused(bool paused) {
        if (paused_ == paused) return;
        paused_ = paused;
        notify();
    }

    void togglePaused() { setPaused(!paused_); }

    void setDropCircle(bool circle) {
        if (dropCircle_ == circle) return;
        dropCircle_ = circle;
        notify();
    }

    void reset() {
        rebuildWorld_();
        bump_();
    }

    void spawnAtCanvas(int px, int py) {
        const float worldW = canvasW_ / scale_;
        const float worldH = canvasH_ / scale_;
        const float x = (float)px / scale_;
        const float y = (canvasH_ - (float)py) / scale_;
        if (x < 0.6f || x > worldW - 0.6f || y < 0.6f || y > worldH - 0.4f) {
            return;
        }
        if (dropCircle_) addCircle(x, y, 0.35f, 0.55f);
        else addBox(x, y, 0.4f, 0.4f, 0.25f);
        bump_();
    }

    void tick(float dt) {
        if (paused_ || !world_ || dt <= 0.0f) return;
        acc_ += dt;
        if (acc_ > 0.25f) acc_ = 0.25f;
        const float step = 1.0f / 60.0f;
        bool dirty = false;
        while (acc_ >= step) {
            acc_ -= step;
            world_->Step(step, 8, 3);
            dirty = true;
        }
        if (dirty) bump_();
    }

    // 由 UI 传入自己的 b2Draw 实现(例如 CanvasDebugDraw);场景不依赖 Skiff。
    void debugDraw(b2Draw& drawer) {
        if (!world_) return;
        drawer.SetFlags(b2Draw::e_shapeBit | b2Draw::e_jointBit);
        world_->SetDebugDraw(&drawer);
        world_->DebugDraw();
        world_->SetDebugDraw(0);
    }

private:
    void bump_() {
        ++frame_;
        notify();
    }

    void rebuildWorld_() {
        delete world_;
        world_ = new b2World(b2Vec2(0.0f, -10.0f));
        acc_ = 0.0f;

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
    int canvasW_;
    int canvasH_;
    float scale_;
    float acc_;
    uint64_t frame_;
    bool paused_;
    bool dropCircle_;
};

} // namespace app
