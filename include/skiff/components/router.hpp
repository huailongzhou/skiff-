// Router:按名字管理任意多个页面,根据导航状态渲染当前页面(纯 DSL,后端无关)。
//
// 用法:
//   skiff::components::Router router("home");        // 初始路由
//   router.add("home", { stateRef("page", router.nav()) }, homeBody);
//   router.add("设置", { stateRef("page", router.nav()),
//                        state<int>("tab", 0) }, settingsBody);
//   // App 的 body 里:
//   return router.render();
//   // 启动前一次性绑定所有页面的状态:
//   router.bind(app);
// 页面内跳转:nav.set("目标页面名")。
#pragma once

#include <functional>
#include <initializer_list>
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
    // initial:初始路由(必须先 add 同名页面,否则走 fallback)
    explicit Router(std::string initial) : nav_(std::move(initial)) {}

    // 全局导航状态:页面用 nav().set("页面名") 跳转;
    // add() 时通常以 stateRef("page", router.nav()) 注册给页面
    State<std::string>& nav() { return nav_; }

    // 注册一个页面:名字 + 状态声明 + 组件树 body。
    // 返回的引用在 Router 存活期间有效(不受后续 add 影响)
    PageView& add(const std::string& name,
                  std::initializer_list<StateDef> states,
                  PageView::Body body) {
        pages_.push_back(Entry());
        pages_.back().page.reset(new PageView(name, states));
        pages_.back().body = std::move(body);
        return *pages_.back().page;
    }

    // 未匹配路由时的兜底页面(可选)
    void fallback(std::function<Element()> body) { fallback_ = std::move(body); }

    // 渲染当前路由对应的页面
    Element render() {
        const std::string& cur = nav_.get();
        for (size_t i = 0; i < pages_.size(); ++i) {
            if (pages_[i].page->name() == cur) {
                return pages_[i].page->render(pages_[i].body);
            }
        }
        if (fallback_) return fallback_();
        return VStack({});
    }

    // 把导航状态和所有页面的状态一次性绑定到 App
    void bind(App& app) {
        app.bind(nav_);
        for (size_t i = 0; i < pages_.size(); ++i) pages_[i].page->bind(app);
    }

private:
    struct Entry {
        std::unique_ptr<PageView> page;  // 堆上持有,add 后引用稳定
        PageView::Body body;
    };

    State<std::string> nav_;
    std::vector<Entry> pages_;
    std::function<Element()> fallback_;
};

} // namespace components
} // namespace skiff
