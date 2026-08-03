# Skiff 轻舟

轻舟(Skiff)是一个 **C++11 声明式嵌入式 UI 框架**：像 SwiftUI 一样用函数描述界面，状态驱动更新，后端可插拔。

- 核心后端无关(纯头文件 `include/skiff/`)，换后端不改页面代码
- 元素:VStack / HStack / Text / Button / Spacer / Slider / TabView(页签容器)
- 当前后端：**LVGL 8**(嵌入式渲染) + **SDL3**(PC 预览宿主，仅出窗口/输入)
- 支持 **FreeType 矢量字体**(TTF，任意字号中文)

## 目录结构

```
include/skiff/          核心:Element / State / App / Backend 接口
include/skiff/components/ 组合组件(纯 DSL):TabView(页签菜单 + 内容区)、TopNav(顶部导航条)、PageView/StateView(页面 + 页面状态管理)、Router(按名字管理页面路由)、AppUi(应用 UI 基类:platform/router/states)
backends/lvgl/          LVGL 后端 + SDL3 宿主 + headless 显示器
examples/               示例
  counter           无头冒烟测试
  counter_sdl       SDL3 窗口:计数器
  pnd_sdl           平台无关的车机 PND UI 定义(无 main,被平台入口包含)
  freetype_check    FreeType / 字体覆盖检查
platforms/              平台入口(挂载 PND UI + 注册平台能力)
  mac/              pnd_mac(macOS:afplay 音乐 / DisplayServices 亮度)
  win/              pnd_win(Windows:winmm 音乐 / WMI 亮度,含 MinGW 交叉工具链)
  linux/            pnd_linux(Linux:aplay 音乐 / sysfs 背光)
third_party/            vendored 依赖
  lvgl(8.4) / SDL3(3.2.14) / freetype(2.13.2)
assets/fonts/           示例用字体(仅预览)
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
skiff::State<int> count(0);

skiff::App app(backend, [&count]() -> skiff::Element {
    return skiff::VStack({
        skiff::Text("count = " + std::to_string(count.get())),
        skiff::Button("+1", [&] { count.set(count.get() + 1); }),
    }, 8);
});
app.bind(count);
app.start();
```

状态变化会自动驱动视图重建，后端负责把新的 `Element` 树挂载成原生控件树。

## 为什么要隔离后端？

```cpp
// 页面代码:只 include 核心头文件
#include "skiff/skiff.hpp"

// SDL3 预览入口:选择 skiff_lvgl_sdl3 后端
#include "skiff_lvgl.hpp"
#include "skiff_lvgl_sdl3.hpp"
```

同样这份页面代码，在嵌入式板上改用 `skiff::lvgl::LvglBackend(your_display)` 即可，无需修改。
