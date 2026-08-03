// Router:按名字管理任意多个页面,根据导航状态渲染当前页面(纯 DSL,后端无关)。
//
// 支持页面栈、页面生命周期状态(Dead/Suspended/Active),以及返回主页/上一页等导航接口。
//
// 用法:
//   skiff::components::Router router("home");        // 初始路由,也是主页
//   router.add("home", {}, homeBody);
//   router.add("设置", { state::of<int>("tab", 0) }, settingsBody);
//   // App 的 body 里:
//   return router.render();
//   // 启动前一次性绑定所有页面的状态:
//   router.bind(app);
//
// 页面内跳转:
//   router.push("设置");     // 打开新页面并压栈,当前页面变成 Suspended
//   router.pop();            // 返回上一页,跳过状态为 Dead 的页面
//   router.home();           // 清空栈,返回主页
//   router.resetTo("设置");  // 清空栈并切换到指定页面
//
// 页面状态控制:
//   router.setPageState("设置", Router::Dead);      // 标记页面已销毁,回退时会跳过
//   Router::PageState s = router.getPageState("设置");
//
// 兼容旧写法:页面内仍可用 nav().set("页面名") 跳转,效果等价于 push。
#pragma once

#include <functional>
#include <initializer_list>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "../backend.hpp"
#include "../element.hpp"
#include "../state.hpp"
#include "page_view.hpp"

namespace skiff {
namespace components {

class Router {
public:
    // 页面生命周期状态
    enum PageState {
        Dead,       // 页面已销毁,回退时会被跳过
        Suspended,  // 页面在栈中但不可见
        Active      // 当前正在显示的页面
    };

    // initial:初始路由,也是主页名(建议先 add 同名页面,否则走 fallback)
    explicit Router(std::string initial)
        : nav_(std::move(initial)), lastNav_(nav_.get()), homeName_(nav_.get()) {
        // 主页默认在栈底且激活
        pageStates_[homeName_] = Active;
        stack_.push_back(homeName_);
    }

    // 全局导航状态:页面内可直接 nav().set("页面名") 跳转,效果与 push 相同
    State<std::string>& nav() { return nav_; }

    // 注册一个页面:名字 + 状态声明 + 组件树 body。
    // 返回的引用在 Router 存活期间有效(不受后续 add 影响)
    PageView& add(const std::string& name,
                  std::initializer_list<StateDef> states,
                  PageView::Body body) {
        pages_.push_back(Entry());
        pages_.back().name = name;
        pages_.back().page.reset(new PageView(name, states));
        pages_.back().body = std::move(body);
        // 新页面默认状态:主页 Active,其余 Suspended(未被访问过)
        if (pageStates_.find(name) == pageStates_.end()) {
            pageStates_[name] = (name == homeName_) ? Active : Suspended;
        }
        return *pages_.back().page;
    }

    // 未匹配路由时的兜底页面(可选)
    void fallback(std::function<Element()> body) { fallback_ = std::move(body); }

    // ---- overlay 层 ----
    // overlay 是覆盖在当前页面之上的浮层(如下拉菜单、弹窗),由调用方提供 builder。
    // render() 每次会把当前页面和 overlay builder 返回的所有元素一起放在根容器里。
    // 返回的元素通常应自己设 .floating(),以保证覆盖在页面之上。

    typedef std::function<std::vector<Element>()> OverlayBuilder;

    void setOverlayBuilder(OverlayBuilder builder) {
        overlayBuilder_ = std::move(builder);
    }

    void clearOverlayBuilder() { overlayBuilder_ = OverlayBuilder(); }

    // ---- 导航接口 ----

    // 打开指定页面并压入页面栈;当前页面进入 Suspended
    void push(const std::string& name) {
        // 主页是栈底常驻页,不能入栈:等价于返回主页
        if (name == homeName_) { home(); return; }
        // 目标已在栈中(含栈顶):作废其上的页面,不重复压栈
        if (activateExisting(name)) return;
        suspendCurrent();
        stack_.push_back(name);
        setPageState(name, Active);
        nav_.set(name);
        lastNav_ = name;
    }

    // 返回上一页;如果上一页是 Dead,则继续往回跳过,直到找到非 Dead 页面。
    // 已在主页时调用无效果。
    void pop() {
        if (stack_.size() <= 1) return;
        suspendCurrent();
        stack_.pop_back();
        // 跳过已销毁页面
        while (!stack_.empty() && getPageState(stack_.back()) == Dead) {
            stack_.pop_back();
        }
        // 兜底:栈空则回到主页
        if (stack_.empty()) {
            stack_.push_back(homeName_);
        }
        setPageState(stack_.back(), Active);
        nav_.set(stack_.back());
        lastNav_ = stack_.back();
    }

    // 返回主页并清空页面栈
    void home() {
        suspendCurrent();
        stack_.clear();
        stack_.push_back(homeName_);
        setPageState(homeName_, Active);
        nav_.set(homeName_);
        lastNav_ = homeName_;
    }

    // 清空栈并切换到指定页面(不保留历史)
    void resetTo(const std::string& name) {
        suspendCurrent();
        stack_.clear();
        stack_.push_back(name);
        setPageState(name, Active);
        nav_.set(name);
        lastNav_ = name;
    }

    // ---- 页面状态接口 ----

    void setPageState(const std::string& name, PageState state) {
        pageStates_[name] = state;
    }

    PageState getPageState(const std::string& name) const {
        std::map<std::string, PageState>::const_iterator it = pageStates_.find(name);
        if (it != pageStates_.end()) return it->second;
        return Suspended;
    }

    // 当前页面名(栈顶)
    const std::string& current() const {
        return stack_.empty() ? homeName_ : stack_.back();
    }

    // ---- 渲染 ----

    // 渲染当前路由对应的页面 + overlay 层;会自动同步外部对 nav() 的修改
    Element render() {
        syncNav();
        // 当前页面若是 Dead,自动回退
        while (stack_.size() > 1 && getPageState(nav_.get()) == Dead) {
            pop();
        }

        const std::string& cur = nav_.get();
        Element page;
        bool found = false;
        for (size_t i = 0; i < pages_.size(); ++i) {
            if (pages_[i].name == cur) {
                page = pages_[i].page->render(pages_[i].body);
                found = true;
                break;
            }
        }
        if (!found) {
            if (fallback_) page = fallback_();
            else page = VStack({});
        }

        std::vector<Element> rootChildren;
        rootChildren.push_back(page);
        if (overlayBuilder_) {
            std::vector<Element> overlays = overlayBuilder_();
            rootChildren.insert(rootChildren.end(), overlays.begin(), overlays.end());
        }
        return skiff::VStack(rootChildren, 0).size(800, 480);
    }

    // 把导航状态和所有页面的状态一次性绑定到 App
    void bind(App& app) {
        app.bind(nav_);
        for (size_t i = 0; i < pages_.size(); ++i) pages_[i].page->bind(app);
    }

private:
    struct Entry {
        std::string name;
        std::unique_ptr<PageView> page;  // 堆上持有,add 后引用稳定
        PageView::Body body;
    };

    void suspendCurrent() {
        if (!stack_.empty()) {
            // Dead 页保持 Dead(已销毁,回退时跳过),不因导航操作被"复活"为 Suspended
            if (getPageState(stack_.back()) != Dead) {
                setPageState(stack_.back(), Suspended);
            }
        }
    }

    // 处理外部直接调用 nav().set() 的情况
    void syncNav() {
        if (nav_.get() == lastNav_) return;
        const std::string& target = nav_.get();
        if (target == homeName_) {
            home();
        } else if (activateExisting(target)) {
            // 目标已在栈中(含栈顶):复用该页,不重复压栈
        } else {
            // 效果等价于 push
            suspendCurrent();
            stack_.push_back(target);
            setPageState(target, Active);
            lastNav_ = target;
        }
    }

    // 目标已在栈中:弹出其上的所有页面,复用并激活目标,返回 true;
    // 未在栈中返回 false。用于防止同一页面在栈中重复出现。
    // 注意:Dead 目标(已销毁)不可激活——弹出它本身,并继续跳过下方
    // Dead 页,回落到下方最近的存活页面(与 pop 的 Dead 跳过语义一致)。
    bool activateExisting(const std::string& name) {
        for (size_t i = 0; i < stack_.size(); ++i) {
            if (stack_[i] == name) {
                const bool deadTarget = getPageState(name) == Dead;
                // 弹出目标之上的页面(被弹出的 Dead 页保持 Dead)
                while (stack_.back() != name) {
                    if (getPageState(stack_.back()) != Dead) {
                        setPageState(stack_.back(), Suspended);
                    }
                    stack_.pop_back();
                }
                if (deadTarget) {
                    // Dead 目标不可激活:弹出它本身,继续跳过下方 Dead 页
                    stack_.pop_back();  // 状态保持 Dead
                    while (stack_.size() > 1 &&
                           getPageState(stack_.back()) == Dead) {
                        stack_.pop_back();
                    }
                    if (stack_.empty()) stack_.push_back(homeName_);  // 防御:home 常驻栈底,正常不会为空
                }
                setPageState(stack_.back(), Active);
                nav_.set(stack_.back());
                lastNav_ = stack_.back();
                return true;
            }
        }
        return false;
    }

    State<std::string> nav_;
    std::string lastNav_;
    std::string homeName_;
    std::vector<std::string> stack_;
    std::map<std::string, PageState> pageStates_;
    std::vector<Entry> pages_;
    std::function<Element()> fallback_;
    OverlayBuilder overlayBuilder_;
};

} // namespace components
} // namespace skiff
