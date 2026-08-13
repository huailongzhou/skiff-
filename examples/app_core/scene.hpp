// 应用核场景接口:无 UI、无 LVGL。场景自己持有状态,通过 notify 驱动观察者。
#pragma once

#include <functional>

namespace app {

class Scene {
public:
    virtual ~Scene() {}

    virtual const char* name() const = 0;
    virtual void tick(float dt) = 0;
    virtual void activate() {}
    virtual void deactivate() {}

    // 场景状态变化时回调(暂停/重置/步进/投放等)。观察者里不要再调会 notify 的命令。
    void onChange(std::function<void()> cb) { onChange_ = std::move(cb); }

protected:
    void notify() {
        if (onChange_) onChange_();
    }

private:
    std::function<void()> onChange_;
};

} // namespace app
