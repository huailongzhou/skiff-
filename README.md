# Skiff 轻舟

轻舟(Skiff)是一个 **C++11 声明式嵌入式 UI 框架**：像 SwiftUI 一样用代码描述界面，状态驱动更新，后端可插拔。

- 核心后端无关(纯头文件 `include/skiff/`)，换后端不改页面代码
- 元素:`ElementView` 包装 + `VStack` / `HStack` / `Text` / `Button` / `Spacer` / `Slider` / `TabView` / `TapArea` / `External`
- 组合组件(纯 DSL):`TopNav`(顶部导航条)、`TabView`(页签菜单 + 内容区)、`AppGrid`(分页应用网格)、`DropDown`(下拉菜单)、`List`(垂直列表)、`Router`(页面栈路由)、`AppUi`(应用 UI 基类)
- 多国语言框架:`include/skiff/i18n.hpp`（`registerCatalog` / `setLocale` / `t` / 宏 `SKIFF_TR`）
- PND 业务文案:`examples/pnd_i18n.hpp`（枚举 key + 中英表 + `pnd::i18n::init`）
- 当前后端：**LVGL 8**(嵌入式渲染) + **SDL3**(PC 预览宿主，仅出窗口/输入)
- 支持 **FreeType 矢量字体**(TTF，任意字号中文)

## 目录结构

```
include/skiff/          核心:Element / ElementView / State / Backend / Platform / i18n
include/skiff/components/ 组合组件(纯 DSL，后端无关)
include/skiff/elements/   属性描述类(attrOptions、listOptions 等)
backends/lvgl/          LVGL 后端 + SDL3 宿主
examples/               示例
  counter           无头冒烟测试
  counter_sdl       SDL3 窗口:计数器
  pnd_sdl           平台无关的车机 PND UI 定义(无 main,被平台入口包含)
  pnd_i18n          PND 业务文案(枚举 + 中英目录)
  freetype_check    FreeType / 字体覆盖检查
platforms/              平台入口(挂载 PND UI + 注册平台能力)
  mac/              pnd_mac(macOS:afplay 音乐 / DisplayServices 亮度)
  win/              pnd_win(Windows:winmm 音乐 / WMI 亮度,含 MinGW 交叉工具链)
  linux/            pnd_linux(Linux:aplay 音乐 / sysfs 背光)
third_party/            vendored 依赖
  lvgl(8.4) / SDL3(3.2.14) / freetype(2.13.2)
assets/fonts/           示例用字体(仅预览)
assets/music/           示例音乐
assets/media/           示例多媒体文件(按需放置)
```

## 构建

```bash
cmake -S . -B build
cmake --build build -j
```

> 需要 CMake ≥ 3.16。

## 运行(在项目根目录)

```bash
./build/counter          # 无头冒烟:状态 → 重建链路
./build/counter_sdl      # SDL3 窗口:计数器
./build/pnd_mac          # SDL3 窗口:车机 PND 主页(macOS 平台入口)
./build/pnd_linux        # SDL3 窗口:车机 PND 主页(Linux 平台入口)
./build/freetype_check   # FreeType 字体加载与字形覆盖检查
```

Windows 交叉编译(Linux 宿主机 + MinGW-w64):

```bash
cmake -S . -B build-win -DCMAKE_TOOLCHAIN_FILE=platforms/win/mingw-w64-x86_64.cmake
cmake --build build-win --target pnd_win -j   # 产出 build-win/pnd_win.exe
```

## 页面长这样

```cpp
#include "skiff/skiff.hpp"

skiff::State<int> count(0);

auto body = [&count]() -> skiff::Element {
    return skiff::VStack({
        skiff::Text("count = " + std::to_string(count.get())),
        skiff::Button("+1", [&] { count.set(count.get() + 1); }),
    }, 8).size(320, 240).centered();
};
```

状态变化会自动驱动视图重建，后端负责把新的 `Element` 树挂载成原生控件树。

## 复合组件与布局

```cpp
using namespace skiff;
using namespace skiff::components;

State<int> tab(0);

Element page = TabView({
    {"网络", networkSubmenu()},
    {"显示", displaySubmenu()},
}, tab)
    .as<TabViewView>()
    .applyBgOption({
        {tabview::first(), elements::state::selected(),   0x1565D8},
        {tabview::first(), elements::state::unselected(), 0x1A222B},
        {tabview::content(), elements::state(),           0x000000},
    })
    .ttf(kFont, 18)
    .size(800, 432);
```

- `ElementView` 提供链式 modifier(`.size()` / `.bg()` / `.ttf()` 等)
- 复杂组件继承 `ElementView` 并重写 `build()` 展开为通用 `Element` 树
- 组件专属方法返回组件自身类型;通用方法返回 `ElementView&`，需要继续调用组件方法时用 `.as<T>()` 转回来

## 平台能力 / 外部回调

页面代码只声明需要的能力，具体实现由平台入口注册:

```cpp
// 页面代码
platform.declare("setBrightness");
platform.invokeExternal("setBrightness", {"80"});

// macOS 平台入口
platform.registerExternal("setBrightness", [](const std::vector<std::string>& args) {
    // 调用 IOKit / DisplayServices 等具体 API
});
```

这样同一份页面代码可以在 PC、嵌入式板、模拟器上运行，只需替换平台入口。

## 为什么要隔离后端？

```cpp
// 页面代码:只 include 核心头文件
#include "skiff/skiff.hpp"

// SDL3 预览入口:选择 skiff_lvgl_sdl3 后端
#include "skiff_lvgl.hpp"
#include "skiff_lvgl_sdl3.hpp"
```

同样这份页面代码，在嵌入式板上改用 `skiff::lvgl::LvglBackend(your_display)` 即可，无需修改。
