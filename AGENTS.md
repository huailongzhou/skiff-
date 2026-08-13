# Skiff (轻舟)

C++11 declarative embedded UI framework. Page code is backend-agnostic; the reference backends are LVGL 8 (rendering) + SDL3 (PC preview host only). README and code comments are in Chinese — match that style for new comments.

## Build & test

```bash
cmake -S . -B build          # CMake ≥ 3.16 required
cmake --build build -j
ctest --test-dir build       # `core` + `physics_headless` + `music_headless`
```

- Tests use a hand-rolled `CHECK` macro in `tests/test_core.cpp`, no framework.
- Run binaries from the project **root**: examples load assets via relative paths (`assets/fonts/...`, `assets/music/...`). Running elsewhere fails font/media loading.
- `-DSKIFF_BACKEND_LVGL=OFF` → core-only build (interface lib + tests + `physics_headless` + `music_headless`).
- `-DSKIFF_LVGL_SDL3=OFF` → skip the SDL3 PC host.
- Windows cross-build from Linux: `cmake -S . -B build-win -DCMAKE_TOOLCHAIN_FILE=platforms/win/mingw-w64-x86_64.cmake` then build target `pnd_win`.
- Headless physics: `./build/physics_headless` (no LVGL/SDL; proves `examples/app_core` runs without UI).
- Headless music: `./build/music_headless` (playlist / repeat / pause, no audio device).

## Architecture rules

- Core is pure header-only (`include/skiff/`), backend-agnostic. Page code must include **only** `skiff/skiff.hpp`; only files in `backends/lvgl/` may include `lvgl.h`. PC entry points additionally include `skiff_lvgl.hpp` then `skiff_lvgl_sdl3.hpp`.
- `examples/app_core/` is the **headless app core** (scenes). It must **not** include `skiff/skiff.hpp` or `lvgl.h`. UI talks to it via commands + `onChange`; PND/physics pages may include these headers.
- `examples/pnd_sdl.cpp` has **no main**; it is a platform-agnostic UI definition that each platform entry `#include`s (e.g. `platforms/mac/mac_platform.cpp` does `#include "examples/pnd_sdl.cpp"`). Platform entries register capabilities via `platform.registerExternal(...)`; page code only `declare`/`invokeExternal`.
- i18n is two layers: framework `skiff/i18n.hpp` (`registerCatalog`/`setLocale`/`t`/`SKIFF_TR`), business catalogs in `examples/pnd_i18n.hpp` (`pnd::i18n::init`). Route IDs are stable English strings; labels are i18n enums.
- `third_party/` is vendored (lvgl v8.4, SDL3-3.2.14, freetype-2.13.2, box2d 2.4.1) — do not edit. `third_party/lvgl-release-v8.3/` is an unused leftover, not referenced by CMake.
- Linux build needs `libmpg123` (dev package); macOS links private `DisplayServices.framework`; Windows links `winmm`.

## Gotchas

- Strict C++11 (`CMAKE_CXX_EXTENSIONS OFF`): no C++14+ features. `.clangd` already sets `-std=c++11 -Iinclude`.
- Fonts: `assets/fonts/default.ttf` is a cropped subset missing many CJK glyphs; use `assets/fonts/Hiragino Sans GB.ttc` for full Chinese coverage.
- `lv_conf.h` at repo root is the LVGL config (32-bit color, flex enabled — `VStack`/`HStack` depend on it). `LV_CONF_PATH` is passed to CMake **without quotes** (LVGL stringifies it).
- CMake 4.x needs `set(CMAKE_POLICY_VERSION_MINIMUM 3.5)` already in `CMakeLists.txt` for freetype's old minimum; keep it if touching that section.
- If `third_party/lvgl/CMakeLists.txt` is missing, CMake fails with instructions to `git clone --depth 1 --branch release/v8.4 https://github.com/lvgl/lvgl.git third_party/lvgl`.
- Component DSL: complex components are `*View` classes overriding `build()`. Component-specific methods return the component type; generic modifiers return `ElementView&`, so chain with `.as<T>()` to keep calling component methods.
- Colors are `uint32_t` hex `0xRRGGBB` passed straight to LVGL.
