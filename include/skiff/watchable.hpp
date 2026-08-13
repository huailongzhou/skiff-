// Watchable:State 局部更新的类型擦除接口。
//
// WatchView 实现此接口;App::watchLocal / SlotHost 把它登记为订阅者,
// State::set() 时只 patch 对应的已挂载节点,不重跑整页 body()。
#pragma once

#include <functional>
#include <string>

#include "element.hpp"

namespace skiff {

class SlotHost;

class Watchable {
public:
    virtual ~Watchable() {}

    // 写入 Element.options.keyId,Backend::patch 用它定位 MountedNode
    virtual const std::string& watchKey() const = 0;

    // 所监听 State 的地址,App 用来把该 State 标成局部失效
    virtual const void* stateIdentity() const = 0;

    virtual bool isDirty() const = 0;

    // 按当前 State 重建子树(并清除 dirty)
    virtual Element rebuild() = 0;

    // 只清缓存,不通知 App。整页失效时用,避免 Watch 命中旧 i18n。
    virtual void clearCache() {}

    // Watch 构造器里的嵌套 Watch() 落到这个 SlotHost
    virtual SlotHost* nestedSlots() { return 0; }

    void setInvalidator(std::function<void()> fn) { invalidator_ = std::move(fn); }
    void setUnregister(std::function<void()> fn) { unregister_ = std::move(fn); }

protected:
    void notifyInvalidator() {
        if (invalidator_) invalidator_();
    }

    void runUnregister() {
        if (!unregister_) return;
        std::function<void()> fn = std::move(unregister_);
        unregister_ = std::function<void()>();
        fn();
    }

private:
    std::function<void()> invalidator_;
    std::function<void()> unregister_;
};

} // namespace skiff
