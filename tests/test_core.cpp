// 核心逻辑单元测试(纯头文件,不依赖 LVGL/SDL 后端)。
// 覆盖:Router 栈状态机、StateView 状态管理、TabView 样式合并、Tab() 标题分离。
#include <cstdio>
#include <string>
#include <vector>

#include "skiff/skiff.hpp"

static int failures = 0;

#define CHECK(cond)                                                       \
    do {                                                                  \
        if (!(cond)) {                                                    \
            std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__,  \
                         #cond);                                          \
            ++failures;                                                   \
        }                                                                 \
    } while (0)

namespace comp = skiff::components;

static skiff::Element pageBody(comp::StateView&) { return skiff::Text("p"); }

// 用于测试 Backend diff 算法的 Mock 后端
class TestBackend : public skiff::Backend {
public:
    TestBackend() : skiff::Backend(nullptr), nextId_(1),
                    createCount_(0), updateCount_(0),
                    destroyCount_(0), moveCount_(0) {}

    int createCount() const { return createCount_; }
    int updateCount() const { return updateCount_; }
    int destroyCount() const { return destroyCount_; }
    int moveCount() const { return moveCount_; }
    void resetCounts() {
        createCount_ = updateCount_ = destroyCount_ = moveCount_ = 0;
    }

private:
    void* createGraphicObject(const skiff::Element& e, void* parent,
                              MountedNode* node) override {
        ++createCount_;
        (void)e; (void)parent; (void)node;
        return reinterpret_cast<void*>(nextId_++);
    }

    void updateGraphicObject(MountedNode& node, const skiff::Element& new_e) override {
        ++updateCount_;
        (void)node; (void)new_e;
    }

    void destroyGraphicObject(void* obj) override {
        ++destroyCount_;
        (void)obj;
    }

    void* getGraphicParent(void* obj) override {
        (void)obj;
        return nullptr;
    }

    void moveGraphicChild(void* obj, void* parent, int index) override {
        ++moveCount_;
        (void)obj; (void)parent; (void)index;
    }

    void* getChildParent(void* obj, const skiff::Element& e,
                         size_t child_index) override {
        (void)e; (void)child_index;
        return obj;
    }

    bool needsRebuild(const MountedNode& old, const skiff::Element& new_e) override {
        return old.element.kind != new_e.kind;
    }

    void afterMount() override {}

    int nextId_;
    int createCount_;
    int updateCount_;
    int destroyCount_;
    int moveCount_;
};

// ---- Router:栈状态机 ----
static void test_router() {
    comp::Router r("home");
    r.add("home", {}, pageBody);
    r.add("A", {}, pageBody);
    r.add("B", {}, pageBody);

    // 初始:主页在栈底且激活
    CHECK(r.current() == "home");
    CHECK(r.getPageState("home") == comp::Router::Active);

    // push:当前页进入 Suspended,新页 Active
    r.push("A");
    CHECK(r.current() == "A");
    CHECK(r.getPageState("home") == comp::Router::Suspended);
    CHECK(r.getPageState("A") == comp::Router::Active);

    // push 栈顶同名:不重复入栈
    r.push("A");
    CHECK(r.current() == "A");

    // push 主页名:主页是常驻栈底页,等价于 home(),不重复入栈
    r.push("B");
    r.push("home");
    CHECK(r.current() == "home");
    CHECK(r.getPageState("home") == comp::Router::Active);

    // pop:回到上一页
    r.push("A");
    r.pop();
    CHECK(r.current() == "home");

    // Dead 页面:pop 时跳过,直接回到 home
    r.push("A");
    r.push("B");
    r.setPageState("A", comp::Router::Dead);
    r.pop();
    CHECK(r.current() == "home");

    // resetTo:清空栈切到指定页
    r.resetTo("B");
    CHECK(r.current() == "B");
    CHECK(r.getPageState("home") == comp::Router::Suspended);

    // home:清空栈回主页
    r.home();
    CHECK(r.current() == "home");
    CHECK(r.getPageState("home") == comp::Router::Active);

    // syncNav:外部 nav().set() 触发跳转
    r.nav().set("A");
    (void)r.render();
    CHECK(r.current() == "A");

    // syncNav:目标已是栈顶时只同步,不重复压栈
    r.nav().set("A");
    (void)r.render();
    CHECK(r.current() == "A");
    r.pop();
    CHECK(r.current() == "home");  // 若上面重复压栈,这里会失败

    // syncNav:外部 set 主页名等价 home()
    r.nav().set("B");
    (void)r.render();
    CHECK(r.current() == "B");
    r.nav().set("home");
    (void)r.render();
    CHECK(r.current() == "home");
    CHECK(r.getPageState("home") == comp::Router::Active);
}

// ---- Router:目标在栈中非栈顶时不重复压栈,作废其上页面 ----
static void test_router_middle_duplicate() {
    comp::Router r("home");
    r.add("home", {}, pageBody);
    r.add("A", {}, pageBody);
    r.add("B", {}, pageBody);

    // push 路径:栈 [home, A, B] 时 push("A") → 作废 B,栈 [home, A]
    r.push("A");
    r.push("B");
    r.push("A");
    CHECK(r.current() == "A");
    r.pop();
    CHECK(r.current() == "home");

    // syncNav 路径:页面内 nav().set("A") 同样不重复压栈
    r.push("A");
    r.push("B");
    r.nav().set("A");
    (void)r.render();
    CHECK(r.current() == "A");
    r.pop();
    CHECK(r.current() == "home");
}

// ---- Router:Dead 页在导航操作中保持 Dead,不被"复活" ----
static void test_router_dead_not_revived() {
    comp::Router r("home");
    r.add("home", {}, pageBody);
    r.add("A", {}, pageBody);
    r.add("B", {}, pageBody);
    r.add("X", {}, pageBody);

    // render 自动回退:栈 [home, A(Dead)] 时 A 被弹出并保持 Dead
    r.push("A");
    r.setPageState("A", comp::Router::Dead);
    r.nav().set("A");
    (void)r.render();
    CHECK(r.current() == "home");
    CHECK(r.getPageState("A") == comp::Router::Dead);

    // push 时 suspendCurrent 不复活 Dead 的 X
    r.push("X");
    r.setPageState("X", comp::Router::Dead);
    r.push("A");  // suspendCurrent:X(Dead) 保持 Dead
    CHECK(r.getPageState("X") == comp::Router::Dead);

    // activateExisting 弹出 Dead 页时保持 Dead
    r.push("B");           // 栈 [home, X(Dead), A, B]
    r.nav().set("A");      // 目标 A 在栈中:弹出 B 和 X(Dead)
    (void)r.render();
    CHECK(r.current() == "A");
    CHECK(r.getPageState("X") == comp::Router::Dead);  // X 未被改写成 Suspended
}

// ---- Router:push 到 Dead 目标不可复活,回落下方存活页 ----
static void test_router_dead_target() {
    // 场景1:栈 [home, A(Dead)],push("A") → 回落 home,A 保持 Dead
    {
        comp::Router r("home");
        r.add("home", {}, pageBody);
        r.add("A", {}, pageBody);
        r.push("A");
        r.setPageState("A", comp::Router::Dead);
        r.push("A");
        CHECK(r.current() == "home");
        CHECK(r.getPageState("A") == comp::Router::Dead);
    }
    // 场景2:栈 [home, A, X(Dead), B],push("X") → 弹 B、X,回落 A
    {
        comp::Router r("home");
        r.add("home", {}, pageBody);
        r.add("A", {}, pageBody);
        r.add("B", {}, pageBody);
        r.add("X", {}, pageBody);
        r.push("A");
        r.push("X");
        r.setPageState("X", comp::Router::Dead);
        r.push("B");
        r.push("X");
        CHECK(r.current() == "A");
        CHECK(r.getPageState("X") == comp::Router::Dead);
        CHECK(r.getPageState("A") == comp::Router::Active);
    }
    // 场景3:连续多 Dead 页——push Dead 目标全部弹出回落 home;pop 连续跳过
    {
        comp::Router r("home");
        r.add("home", {}, pageBody);
        r.add("D1", {}, pageBody);
        r.add("D2", {}, pageBody);
        r.add("B", {}, pageBody);
        r.push("D1");
        r.setPageState("D1", comp::Router::Dead);
        r.push("D2");  // suspendCurrent:D1(Dead) 保持 Dead
        r.setPageState("D2", comp::Router::Dead);
        r.push("D2");  // activateExisting:D2 是 Dead 目标 → 弹 D2、D1,回落 home
        CHECK(r.current() == "home");
        CHECK(r.getPageState("D1") == comp::Router::Dead);
        CHECK(r.getPageState("D2") == comp::Router::Dead);

        // pop 连续跳过多个 Dead 页
        r.push("D1");
        r.setPageState("D1", comp::Router::Dead);
        r.push("D2");
        r.setPageState("D2", comp::Router::Dead);
        r.push("B");
        r.pop();  // 弹 B,然后跳过 D2、D1,直接回 home
        CHECK(r.current() == "home");
    }
}

// ---- Router:fallback ----
static void test_router_fallback() {
    comp::Router r("home");
    r.add("home", {}, pageBody);
    bool called = false;
    r.fallback([&called]() {
        called = true;
        return skiff::Text("fb");
    });
    r.resetTo("不存在");
    skiff::Element e = r.render();
    CHECK(called);
    CHECK(e.kind == skiff::Element::Column);  // 根容器
}

// ---- StateView:创建 / 引用外部状态 ----
static void test_stateview() {
    comp::StateView sv;
    skiff::State<int>& a = sv.create<int>("a", 3);
    CHECK(a.get() == 3);
    a.set(5);
    CHECK(sv.get<int>("a").get() == 5);

    skiff::State<std::string> ext("ext");
    sv.addRef<std::string>("ext", ext);
    CHECK(sv.get<std::string>("ext").get() == "ext");
    ext.set("changed");
    CHECK(sv.get<std::string>("ext").get() == "changed");
}

// ---- TabView:空页签列表不崩溃 ----
static void test_tabview_empty() {
    skiff::State<int> tab(0);
    std::vector<comp::TabViewItem> empty;
    skiff::Element tv = comp::TabView(empty, tab).build();
    CHECK(tv.children.empty());
    tv = comp::TabView(empty, tab)
             .applyBgOption(
                 {{comp::tabview::first(), skiff::elements::state(), 0x112233}})
             .build();
    CHECK(tv.children.empty());  // rebuild 对空列表无操作
}

// ---- TabView:first 部分 Default 背景色必须合并,不覆盖按钮样式 ----
static void test_tabview_first_default_bg() {
    skiff::State<int> tab(0);
    std::vector<comp::TabViewItem> items;
    comp::TabViewItem it;
    it.title = "一";
    it.content = skiff::Text("内容一");
    items.push_back(it);
    it.title = "二";
    it.content = skiff::Text("内容二");
    items.push_back(it);

    skiff::Element tv = comp::TabView(items, tab)
                           .applyBgOption(
                               {{comp::tabview::first(), skiff::elements::state(), 0x112233}})
                           .build();

    // children[0] = 左侧页签栏(VStack),其 children[0] = 第一个按钮
    const skiff::Element& bar = tv.children[0];
    const skiff::Element& btn = bar.children[0];
    CHECK(btn.kind == skiff::Element::Button);
    // 修复回归点:背景色合并后,尺寸/前景色/居中必须保留
    CHECK(btn.options.width == 180);
    CHECK(btn.options.height == 48);
    CHECK(btn.options.hasFg);
    CHECK(btn.options.fgColor == 0xFFFFFF);
    CHECK(btn.options.center);
    // 背景色已应用
    CHECK(btn.options.hasBg);
    CHECK(btn.options.bgColor == 0x112233);
}

// ---- TabView:selected/unselected 背景色 ----
static void test_tabview_select_bg() {
    skiff::State<int> tab(0);
    std::vector<comp::TabViewItem> items;
    comp::TabViewItem it;
    it.title = "一";
    it.content = skiff::Text("内容一");
    items.push_back(it);
    it.title = "二";
    it.content = skiff::Text("内容二");
    items.push_back(it);

    skiff::Element tv = comp::TabView(items, tab)
                           .applyBgOption(
                               {{comp::tabview::first(), skiff::elements::state::selected(), 0xAA0000},
                                {comp::tabview::first(), skiff::elements::state::unselected(), 0x00AA00}})
                           .build();

    const skiff::Element& bar = tv.children[0];
    const skiff::Element& active = bar.children[0];    // tab=0,选中
    const skiff::Element& inactive = bar.children[1];  // 未选中
    CHECK(active.options.hasBg && active.options.bgColor == 0xAA0000);
    CHECK(inactive.options.hasBg && inactive.options.bgColor == 0x00AA00);
    // 尺寸等样式不被覆盖
    CHECK(active.options.width == 180 && active.options.center);
    CHECK(inactive.options.width == 180 && inactive.options.center);
}

// ---- TabView:Default 与 selected/unselected 并存时,状态色优先于 Default ----
// 无论条目顺序如何,选中/未选中按钮都必须显示状态色而非 Default 兜底色。
static void test_tabview_bg_precedence() {
    skiff::State<int> tab(0);
    std::vector<comp::TabViewItem> items;
    comp::TabViewItem it;
    it.title = "一";
    it.content = skiff::Text("内容一");
    items.push_back(it);
    it.title = "二";
    it.content = skiff::Text("内容二");
    items.push_back(it);

    // selected 写在 Default 前面(顺序反转也应得到正确结果)
    skiff::Element tv = comp::TabView(items, tab)
                           .applyBgOption(
                               {{comp::tabview::first(), skiff::elements::state::selected(), 0xAA0000},
                                {comp::tabview::first(), skiff::elements::state(), 0x111111}})
                           .build();

    const skiff::Element& bar = tv.children[0];
    const skiff::Element& active = bar.children[0];    // tab=0,选中
    const skiff::Element& inactive = bar.children[1];  // 未选中
    CHECK(active.options.bgColor == 0xAA0000);   // 选中色优先于 Default
    CHECK(inactive.options.bgColor == 0x111111); // 未选中吃 Default 兜底
    // 样式保留
    CHECK(active.options.width == 180 && active.options.center);
    CHECK(inactive.options.width == 180 && inactive.options.center);
}

// ---- TabView:content 部分 Default 背景色合并到 contentWrap ----
static void test_tabview_content_bg() {
    skiff::State<int> tab(0);
    std::vector<comp::TabViewItem> items;
    comp::TabViewItem it;
    it.title = "一";
    it.content = skiff::Text("内容一");
    items.push_back(it);

    skiff::Element tv = comp::TabView(items, tab)
                           .applyBgOption(
                               {{comp::tabview::content(), skiff::elements::state(), 0x445566}})
                           .build();

    // children[1] = contentWrap:合并后保留 size/expand,同时获得背景色
    const skiff::Element& wrap = tv.children[1];
    CHECK(wrap.options.hasBg);
    CHECK(wrap.options.bgColor == 0x445566);
    CHECK(wrap.options.flexGrow);
}

// ---- Tab():标题存 tabTitle,不覆盖内容文字 ----
static void test_tab_title() {
    skiff::Element content = skiff::Text("内容");
    skiff::Element t = skiff::Tab("标题", content);
    CHECK(t.tabTitle == "标题");
    CHECK(t.text == "内容");  // 内容文字不被覆盖
}

// ---- Element::applyOptions:Default 整体赋值语义(设计如此,测试固化) ----
static void test_apply_options_default() {
    skiff::Element e = skiff::Text("x").size(100, 20).fg(0xFFFFFF);
    e.applyOptions(skiff::elements::state(),
                   skiff::elements::attrOptions().bg(0x123456));
    // Default 状态是整体赋值(与 tabview 的 .bg() 合并形成对比)
    CHECK(e.options.width == 0);
    CHECK(e.options.hasBg);
    CHECK(e.options.bgColor == 0x123456);

    // 非 Default 状态写入 stateStyles(存于 rare),不影响 options
    skiff::Element f = skiff::Text("y").size(100, 20);
    f.applyOptions(skiff::elements::state::pressed(),
                   skiff::elements::attrOptions().bg(0x999999));
    CHECK(f.options.width == 100);
    CHECK(f.rare->stateStyles[skiff::elements::state::pressed()].hasBg);
    CHECK(f.rare->stateStyles[skiff::elements::state::pressed()].bgColor == 0x999999);
}

// ---- Backend diff:key 稳定复用,无 key 按位置/类型复用 ----
static void test_backend_diff_key() {
    TestBackend backend;

    // 第一帧:Column 下三个 Text,分别带 key a/b/c
    skiff::Element root1 = skiff::VStack({
        skiff::Text("A").key("a"),
        skiff::Text("B").key("b"),
        skiff::Text("C").key("c"),
    }, 0).build();
    backend.mount(root1);
    CHECK(backend.createCount() == 4);  // Column + 3 Text
    CHECK(backend.updateCount() == 0);
    backend.resetCounts();

    // 第二帧:顺序打乱为 c/a/b,但 key 不变,应只移动不重建
    skiff::Element root2 = skiff::VStack({
        skiff::Text("C2").key("c"),
        skiff::Text("A2").key("a"),
        skiff::Text("B2").key("b"),
    }, 0).build();
    backend.mount(root2);
    CHECK(backend.createCount() == 0);  // 无新建
    CHECK(backend.destroyCount() == 0); // 无删除
    CHECK(backend.updateCount() == 4);  // Column + 三个 Text 更新
    CHECK(backend.moveCount() == 3);    // 三个子节点都 move_to_index
}

// ---- Backend diff:无 key 时按位置复用,顺序打乱会重建 ----
static void test_backend_diff_no_key() {
    TestBackend backend;

    skiff::Element root1 = skiff::VStack({
        skiff::Text("A"),
        skiff::Text("B"),
        skiff::Text("C"),
    }, 0).build();
    backend.mount(root1);
    backend.resetCounts();

    // 无 key 打乱顺序:按同位置同类型匹配,节点被复用并更新文本/移动位置
    skiff::Element root2 = skiff::VStack({
        skiff::Text("C"),
        skiff::Text("A"),
        skiff::Text("B"),
    }, 0).build();
    backend.mount(root2);
    CHECK(backend.createCount() == 0);  // 无新建
    CHECK(backend.destroyCount() == 0); // 无删除
    CHECK(backend.updateCount() == 4);  // Column + 三个 Text 更新文本
    CHECK(backend.moveCount() == 3);    // 三个子节点都 move_to_index
}

// ---- MemoView:相同 key 复用缓存,不同 key 或 invalidate 重新 build ----
static void test_memo_view() {
    int buildCount = 0;
    comp::MemoView view("v1", [&buildCount]() {
        ++buildCount;
        return skiff::Text("x");
    });

    skiff::Element e1 = view.build();
    CHECK(buildCount == 1);
    skiff::Element e2 = view.build();
    CHECK(buildCount == 1);  // 同 key 不复建
    CHECK(e1.text == e2.text);

    view.reset("v2", [&buildCount]() {
        ++buildCount;
        return skiff::Text("y");
    });
    skiff::Element e3 = view.build();
    CHECK(buildCount == 2);
    CHECK(e3.text == "y");

    view.invalidate();
    skiff::Element e4 = view.build();
    CHECK(buildCount == 3);  // invalidate 后强制重建
}

// ---- Backend diff:TapArea onTap 变化时节点复用,回调被就地替换 ----
static void test_backend_diff_taparea_callback() {
    TestBackend backend;

    int callA = 0;
    int callB = 0;
    skiff::Element root1 = skiff::VStack({
        skiff::TapArea([&callA] { ++callA; }),
    }, 0).build();
    backend.mount(root1);
    backend.resetCounts();

    // onTap 从 A 换成 B,kind 不变,应走 updateGraphicObject 而不是重建
    skiff::Element root2 = skiff::VStack({
        skiff::TapArea([&callB] { ++callB; }),
    }, 0).build();
    backend.mount(root2);
    CHECK(backend.createCount() == 0);
    CHECK(backend.destroyCount() == 0);
    // VStack 父节点 + TapArea 子节点各一次 updateGraphicObject
    CHECK(backend.updateCount() == 2);
}

// ---- BindView:State 版本变化才重建子树,set 同值也会触发版本更新 ----
static void test_bind_view() {
    skiff::State<int> count(0);
    int buildCount = 0;

    comp::BindView<int> view(count, [&buildCount](int c) -> skiff::Element {
        ++buildCount;
        return skiff::Text(std::to_string(c));
    });

    skiff::Element e1 = view.build();
    CHECK(buildCount == 1);
    CHECK(e1.text == "0");

    skiff::Element e2 = view.build();
    CHECK(buildCount == 1);  // State 未变,复用缓存

    count.set(5);
    skiff::Element e3 = view.build();
    CHECK(buildCount == 2);
    CHECK(e3.text == "5");

    count.set(5);  // 值相同但版本自增
    skiff::Element e4 = view.build();
    CHECK(buildCount == 3);  // BindView 按版本判断,会重建

    view.invalidate();
    skiff::Element e5 = view.build();
    CHECK(buildCount == 4);
    CHECK(!e1.options.keyId.empty());
    CHECK(e1.options.keyId == e5.options.keyId);
}

// ---- State:多个 subscribe 并存,setOnChange 只替换自己那一档 ----
static void test_state_multi_subscribe() {
    skiff::State<int> s(0);
    int a = 0;
    int b = 0;
    int c = 0;
    uint64_t idA = s.subscribe([&a] { ++a; });
    s.subscribe([&b] { ++b; });
    s.setOnChange([&c] { ++c; });

    s.set(1);
    CHECK(a == 1);
    CHECK(b == 1);
    CHECK(c == 1);

    s.setOnChange([&c] { c += 10; });
    s.set(2);
    CHECK(a == 2);
    CHECK(b == 2);
    CHECK(c == 11);

    s.unsubscribe(idA);
    s.set(3);
    CHECK(a == 2);
    CHECK(b == 3);
}

// ---- BindView + bindLocal:只 patch 绑定子树,不重跑 body ----
static void test_bind_local_patch() {
    TestBackend backend;
    skiff::State<int> count(0);
    int buildCount = 0;
    int bodyCount = 0;

    comp::BindView<int> view(count, [&buildCount](int c) -> skiff::Element {
        ++buildCount;
        return skiff::Text(std::to_string(c));
    });

    skiff::App app(backend, [&bodyCount, &view]() -> skiff::Element {
        ++bodyCount;
        return skiff::VStack({
            view.build(),
            skiff::Text("static"),
        }, 0);
    });
    app.bind(count);
    app.bindLocal(view);
    app.start();

    CHECK(bodyCount == 1);
    CHECK(buildCount == 1);
    backend.resetCounts();

    count.set(1);
    app.update();
    CHECK(bodyCount == 1);
    CHECK(buildCount == 2);
    CHECK(backend.createCount() == 0);
    CHECK(backend.destroyCount() == 0);
    CHECK(backend.updateCount() == 1);
    CHECK(backend.moveCount() == 0);
}

// ---- 未 bindLocal 时 State 仍触发整页 body + mount ----
static void test_app_root_invalidate() {
    TestBackend backend;
    skiff::State<int> count(0);
    int bodyCount = 0;

    skiff::App app(backend, [&bodyCount, &count]() -> skiff::Element {
        ++bodyCount;
        return skiff::Text(std::to_string(count.get()));
    });
    app.bind(count);
    app.start();
    CHECK(bodyCount == 1);

    backend.resetCounts();
    count.set(1);
    app.update();
    CHECK(bodyCount == 2);
    CHECK(backend.createCount() == 0);
    CHECK(backend.updateCount() >= 1);
}

// ---- i18n 框架层:注册目录 / 切换语言 / 按下标查找 ----
static void test_i18n() {
    enum { k_hi = 0, k_bye, k_count };
    static std::string zh[k_count];
    static std::string en[k_count];
    zh[k_hi] = "你好";
    zh[k_bye] = "再见";
    en[k_hi] = "Hello";
    en[k_bye] = "Bye";

    skiff::i18n::registerCatalog("zh-CN", zh, k_count);
    skiff::i18n::registerCatalog("en", en, k_count);

    skiff::i18n::setLocale("zh-CN");
    CHECK(skiff::i18n::locale() == "zh-CN");
    CHECK(tr(k_hi) == "你好");
    CHECK(tr(k_bye) == "再见");
    CHECK(skiff::i18n::t(k_hi) == "你好");

    skiff::i18n::setLocale("en");
    CHECK(skiff::i18n::locale() == "en");
    CHECK(tr(k_hi) == "Hello");
    CHECK(tr(k_bye) == "Bye");

    const std::string& a = skiff::i18n::t(k_hi);
    const std::string& b = skiff::i18n::t(k_hi);
    CHECK(&a == &b);

    // 越界 → 空串
    CHECK(skiff::i18n::t(-1).empty());
    CHECK(skiff::i18n::t(k_count).empty());
}

int main() {
    test_router();
    test_router_middle_duplicate();
    test_router_dead_not_revived();
    test_router_dead_target();
    test_router_fallback();
    test_stateview();
    test_tabview_empty();
    test_tabview_first_default_bg();
    test_tabview_select_bg();
    test_tabview_bg_precedence();
    test_tabview_content_bg();
    test_tab_title();
    test_apply_options_default();
    test_backend_diff_key();
    test_backend_diff_no_key();
    test_backend_diff_taparea_callback();
    test_memo_view();
    test_bind_view();
    test_state_multi_subscribe();
    test_bind_local_patch();
    test_app_root_invalidate();
    test_i18n();

    if (failures == 0) {
        std::printf("all tests passed\n");
        return 0;
    }
    std::fprintf(stderr, "%d check(s) failed\n", failures);
    return 1;
}
