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
    test_i18n();

    if (failures == 0) {
        std::printf("all tests passed\n");
        return 0;
    }
    std::fprintf(stderr, "%d check(s) failed\n", failures);
    return 1;
}
