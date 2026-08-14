// SceneHost:管理一组无头场景。可同时激活多个(音乐后台常驻,物理仅前台步进)。
#pragma once

#include <cstddef>
#include <cstring>
#include <vector>

#include "scene.hpp"

namespace app {

class SceneHost {
public:
    void add(Scene& scene) { scenes_.push_back(&scene); }

    Scene* find(const char* name) const {
        if (!name) return 0;
        for (size_t i = 0; i < scenes_.size(); ++i) {
            const char* n = scenes_[i]->name();
            if (n && std::strcmp(n, name) == 0) return scenes_[i];
        }
        return 0;
    }

    bool isActive(const char* name) const {
        Scene* s = find(name);
        return s && indexOf_(active_, s) >= 0;
    }

    // 加入激活集;已激活则忽略。
    void activate(const char* name) {
        Scene* s = find(name);
        if (!s || indexOf_(active_, s) >= 0) return;
        s->activate();
        active_.push_back(s);
    }

    // 从激活集移除;name 为空则全部停掉。
    void deactivate(const char* name = 0) {
        if (!name) {
            for (size_t i = 0; i < active_.size(); ++i) {
                active_[i]->deactivate();
            }
            active_.clear();
            return;
        }
        Scene* s = find(name);
        const int i = s ? indexOf_(active_, s) : -1;
        if (i < 0) return;
        s->deactivate();
        active_.erase(active_.begin() + static_cast<std::ptrdiff_t>(i));
    }

    void tick(float dt) {
        for (size_t i = 0; i < active_.size(); ++i) {
            active_[i]->tick(dt);
        }
    }

private:
    static int indexOf_(const std::vector<Scene*>& v, Scene* s) {
        for (size_t i = 0; i < v.size(); ++i) {
            if (v[i] == s) return static_cast<int>(i);
        }
        return -1;
    }

    std::vector<Scene*> scenes_;
    std::vector<Scene*> active_;
};

} // namespace app
