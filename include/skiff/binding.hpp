// Binding:State 局部更新的类型擦除接口。
//
// BindView 实现此接口;App::bindLocal 把它登记为订阅者,
// State::set() 时只 patch 对应的已挂载节点,不重跑整页 body()。
#pragma once

#include <functional>
#include <string>

#include "element.hpp"

namespace skiff {

class Binding {
public:
    virtual ~Binding() {}

    // 写入 Element.options.keyId,Backend::patch 用它定位 MountedNode
    virtual const std::string& bindingKey() const = 0;

    // 所绑定 State 的地址,App 用来把该 State 标成局部失效
    virtual const void* stateIdentity() const = 0;

    virtual bool isDirty() const = 0;

    // 按当前 State 重建子树(并清除 dirty)
    virtual Element rebuild() = 0;

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
