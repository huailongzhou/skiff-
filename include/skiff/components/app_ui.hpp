// AppUi:应用级 UI 的基类(纯 DSL,后端无关)。
//
// 集中持有一个应用 UI 的三件公共资产:
//   1. platform_ —— 平台能力/事件(引用,由平台入口持有)
//   2. router_   —— 页面路由
//   3. states_   —— 全局状态(StateView,注册 + 一次性 bindAll)
//
// 具体应用 UI(如 PndUi)继承它,在构造函数里:
//   states().create<T>("名字", 初值)   注册全局状态
//   platform.declare(...) / platform.on(...)  声明能力、订阅事件
//   router().add(...)                注册页面
// 平台入口处的调用不变:ui.render() / ui.bind(app)。
#pragma once

#include <cstdio>
#include <cstdlib>
#include <functional>
#include <initializer_list>
#include <string>
#include <utility>

#include "../backend.hpp"
#include "../element.hpp"
#include "../platform.hpp"
#include "page_view.hpp"
#include "router.hpp"

namespace skiff {
namespace components {

// globalStatesInit 的初值包装:按构造时的类型打标签(bool/int/string),
// 运行时与传入的 Tag 校验,防止标签与值类型不匹配。
struct StateInit {
    state::Tag tag;
    bool b;
    int i;
    std::string str;
    StateInit(bool v) : tag(state::BOOL), b(v), i(0) {}
    StateInit(int v) : tag(state::INT), b(false), i(v) {}
    StateInit(const char* v) : tag(state::STRING), b(false), i(0), str(v) {}
    StateInit(const std::string& v) : tag(state::STRING), b(false), i(0), str(v) {}
};

class AppUi {
public:
    // platform:平台能力接口(由平台入口持有,AppUi 仅引用)
    // initialRoute:初始路由名(须有对应 add() 的页面,否则走 fallback)
    explicit AppUi(Platform& platform, std::string initialRoute = "home")
        : platform_(platform), router_(std::move(initialRoute)) {}
    virtual ~AppUi() {}

    // 渲染当前页面(供 App 的 body 使用)
    Element render() { return router_.render(); }

    // 一次性绑定:全局状态 + 所有页面状态 + 路由状态
    virtual void bind(App& app) {
        states_.bindAll(app);
        router_.bind(app);
    }

    // 批量注册全局状态:类型标签 + {名字, 初值} 列表,如:
    //   globalStatesInit(skiff::components::state::BOOL,
    //                    {{"menuExpanded", false}, {"musicPlaying", false}});
    //   globalStatesInit(skiff::components::state::INT,
    //                    {{"brightness", 80}});
    // 标签与初值类型不匹配属于编程错误,直接终止。
    void globalStatesInit(state::Tag tag,
                          std::initializer_list<std::pair<std::string, StateInit> > s) {
        for (std::initializer_list<std::pair<std::string, StateInit> >::iterator
                 it = s.begin(); it != s.end(); ++it) {
            requireTag(it->second.tag, tag);
            switch (tag) {
            case state::BOOL:
                states_.create<bool>(it->first, it->second.b);
                break;
            case state::INT:
                states_.create<int>(it->first, it->second.i);
                break;
            case state::STRING:
                states_.create<std::string>(it->first, it->second.str);
                break;
            }
        }
    }

protected:
    Platform& platform() { return platform_; }
    Router& router() { return router_; }
    StateView& states() { return states_; }

private:
    static void requireTag(state::Tag got, state::Tag want) {
        if (got != want) {
            std::fprintf(stderr,
                         "AppUi: globalStatesInit tag/type mismatch\n");
            std::abort();
        }
    }

    Platform& platform_;
    Router router_;
    StateView states_;
};

} // namespace components
} // namespace skiff
