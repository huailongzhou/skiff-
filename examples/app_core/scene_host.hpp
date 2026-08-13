// SceneHost:管理一组无头场景,每帧只步进当前激活的那个。
#pragma once

#include <cstring>
#include <vector>

#include "scene.hpp"

namespace app {

class SceneHost {
public:
    SceneHost() : active_(0) {}

    void add(Scene& scene) { scenes_.push_back(&scene); }

    Scene* find(const char* name) const {
        if (!name) return 0;
        for (size_t i = 0; i < scenes_.size(); ++i) {
            const char* n = scenes_[i]->name();
            if (n && std::strcmp(n, name) == 0) return scenes_[i];
        }
        return 0;
    }

    Scene* active() const { return active_; }

    void activate(const char* name) {
        Scene* next = name ? find(name) : 0;
        if (active_ == next) return;
        if (active_) active_->deactivate();
        active_ = next;
        if (active_) active_->activate();
    }

    void deactivate() { activate(0); }

    void tick(float dt) {
        if (active_) active_->tick(dt);
    }

private:
    std::vector<Scene*> scenes_;
    Scene* active_;
};

} // namespace app
