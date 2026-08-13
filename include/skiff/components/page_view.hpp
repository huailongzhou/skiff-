// PageView:页面(纯 DSL,后端无关)。
//
// 一个 PageView 持有:
//   1. element_   —— 代表页面的 Element 树(最近一次 render() 的结果)
//   2. stateView_ —— StateView,持有并管理本页面的所有状态
//   3. name_      —— 页面名,导航匹配用
//
// 用法(无需子类,整页代码一处写完):
//   skiff::components::PageView settingsPage("设置", {
//       skiff::components::state::ref("page", nav),   // 外部状态,仅注册引用
//       skiff::components::state::of<int>("tab", 0),  // 本页状态,StateView 持有
//   });
//   // App 的 body 里:
//   settingsPage.render([](skiff::components::StateView& st) -> Element {
//       State<int>& tab = st.get<int>("tab");
//       return skiff::VStack({ ... });
//   });
//   // 启动前:
//   settingsPage.bind(app);   // 一次性绑定本页所有状态
#pragma once

#include <cstdio>
#include <cstdlib>
#include <functional>
#include <initializer_list>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "../backend.hpp"
#include "../element.hpp"
#include "../state.hpp"

namespace skiff {
namespace components {

// StateView:持有并集中管理一个页面的所有 State。
//
//   create<T>(name, initial) —— 创建页面状态,本体由 StateView 持有
//   addRef<T>(name, state)   —— 注册外部持有的状态(仅管理其绑定)
//   get<T>(name)             —— 按名字取用(类型须与创建时一致)
//   bindAll(app)             —— 把所有状态一次性绑定到 App
class StateView {
public:
    // 创建并持有一个页面状态;返回的引用在 StateView 存活期间有效
    template <typename T>
    State<T>& create(const std::string& name, const T& initial) {
        TypedEntry<T>* e = new TypedEntry<T>(name, initial);
        entries_.push_back(std::unique_ptr<Entry>(e));
        return *e->state;
    }

    // 注册一个外部持有的状态(如全局导航状态),StateView 不持有本体
    template <typename T>
    void addRef(const std::string& name, State<T>& s) {
        entries_.push_back(std::unique_ptr<Entry>(new TypedEntry<T>(name, s)));
    }

    // 按名字取用状态;找不到或类型不匹配直接终止(编程错误,尽早暴露)
    template <typename T>
    State<T>& get(const std::string& name) {
        for (size_t i = 0; i < entries_.size(); ++i) {
            if (entries_[i]->name == name) {
                TypedEntry<T>* t = dynamic_cast<TypedEntry<T>*>(entries_[i].get());
                if (t != nullptr) return *t->state;
            }
        }
        std::fprintf(stderr, "StateView: state '%s' not found or type mismatch\n",
                     name.c_str());
        std::abort();
    }

    void bindAll(App& app) {
        for (size_t i = 0; i < entries_.size(); ++i) entries_[i]->bind(app);
    }

private:
    struct Entry {
        std::string name;
        virtual ~Entry() {}
        virtual void bind(App&) = 0;
    };

    // 类型擦除的状态条目:owned 时持有本体,否则仅引用外部状态
    template <typename T>
    struct TypedEntry : Entry {
        State<T>* state;
        bool owned;
        TypedEntry(const std::string& n, const T& v)
            : state(new State<T>(v)), owned(true) { name = n; }
        TypedEntry(const std::string& n, State<T>& s)
            : state(&s), owned(false) { name = n; }
        ~TypedEntry() { if (owned) delete state; }
        void bind(App& app) override { app.bind(*state); }
    };

    std::vector<std::unique_ptr<Entry> > entries_;
};

// 状态声明:配合 PageView/Router 的 { ... } 状态列表使用
struct StateDef {
    std::function<void(StateView&)> apply;
};

// 状态声明与类型标签。
//   标签(配合 AppUi::globalStatesInit):
//     state::BOOL / state::INT / state::STRING
//   StateDef 工厂(配合 PageView/Router 的 { ... } 状态列表):
//     state::of<T>("名", 初值)   本页状态,StateView 持有
//     state::ref("名", 外部状态)  仅注册引用
struct state {
    enum Tag { BOOL, INT, STRING };

    // 声明一个本页状态(本体由 StateView 持有)
    template <typename T>
    static StateDef of(const std::string& name, const T& initial) {
        StateDef d;
        d.apply = [name, initial](StateView& sv) { sv.create<T>(name, initial); };
        return d;
    }

    // 声明一个外部状态引用(如全局导航状态)
    template <typename T>
    static StateDef ref(const std::string& name, State<T>& s) {
        StateDef d;
        d.apply = [name, &s](StateView& sv) { sv.addRef(name, s); };
        return d;
    }
};

class PageView {
public:
    // 页面 body:从 StateView 取状态,返回组件树
    typedef std::function<Element(StateView&)> Body;

    explicit PageView(std::string name) : name_(std::move(name)) {}

    // name + 本页状态声明列表
    PageView(std::string name, std::initializer_list<StateDef> states)
        : name_(std::move(name)) {
        for (std::initializer_list<StateDef>::iterator it = states.begin();
             it != states.end(); ++it) {
            it->apply(stateView_);
        }
    }

    // 页面名(如 "设置"),导航时与路由值比较
    const std::string& name() const { return name_; }

    // 本页状态的持有者/管理器
    StateView& stateView() { return stateView_; }

    // 渲染页面:执行 body;body 内可写 Bind(state, builder),按调用顺序复用槽位
    const Element& render(Body body) {
        SlotHost::Guard g(slots_);
        element_ = body(stateView_);
        return element_;
    }

    // 把本页所有状态绑定到 App,并把本页 SlotHost 挂上(后续 Bind() 自动 bindLocal)
    void bind(App& app) {
        stateView_.bindAll(app);
        slots_.attach(app);
    }

    SlotHost& slots() { return slots_; }

private:
    std::string name_;
    StateView stateView_;
    Element element_;
    SlotHost slots_;
};

} // namespace components
} // namespace skiff
