// freetype_check:验证 LVGL 的 FreeType 支持 + DSL 后端 .ttf() 链路。
// 在项目根目录运行:./build/freetype_check
#include <cstdio>

#include "lvgl.h"
#include "skiff/skiff.hpp"
#include "skiff_lvgl.hpp"

int main() {
    lv_init();
    skiff::lvgl::createHeadlessDisplay(240, 240);

    if (!skiff::lvgl::ensureFreetype()) {
        std::printf("FAIL: freetype 初始化失败\n");
        return 1;
    }

    // ---- 1) 裸 API:建 24px 字体,查字形覆盖(含 PND 页面用到的汉字) ----
    lv_ft_info_t info = {};
    info.name = "assets/fonts/Hiragino Sans GB.ttc";
    info.weight = 24;  // 字号(px)
    info.style = FT_FONT_STYLE_NORMAL;
    if (!lv_ft_font_init(&info) || info.font == nullptr) {
        std::printf("FAIL: lv_ft_font_init 失败(检查字体文件是否存在)\n");
        return 1;
    }
    const int ftLineHeight = info.font->line_height;
    std::printf("字体加载成功:24px, line_height = %d\n", ftLineHeight);

    // 导航/音乐/蓝牙电话/收音机/视频/图片/设置/返回主页/功能演示界面
    const char* sample = "导航音乐蓝牙电话收音机视频图片设置返回主页功能演示界面A";
    bool allFound = true;
    for (const char* p = sample; *p;) {  // UTF-8 解码逐字检查
        uint32_t cp = 0;
        const unsigned char b = (unsigned char)*p;
        if (b < 0x80)       { cp = b; p += 1; }
        else if (b < 0xE0)  { cp = b & 0x1F; p += 2; cp = (cp << 6) | ((unsigned char)p[-1] & 0x3F); }
        else                { cp = b & 0x0F; p += 3; cp = (cp << 6) | ((unsigned char)p[-2] & 0x3F); cp = (cp << 6) | ((unsigned char)p[-1] & 0x3F); }
        lv_font_glyph_dsc_t dsc;
        if (lv_font_get_glyph_dsc(info.font, &dsc, cp, 0) == 0) {
            std::printf("  缺字形 U+%04X\n", cp);
            allFound = false;
        }
    }
    std::printf("字形覆盖:%s\n", allFound ? "全部命中" : "有缺失(见上)");
    lv_ft_font_destroy(info.font);
    if (!allFound) return 1;

    // ---- 2) DSL 链路:Text(...).ttf(...) 经 LvglBackend 挂载后 label 应使用该字体 ----
    {
        skiff::lvgl::LvglBackend backend(lv_scr_act());
        backend.mount(skiff::VStack({
            skiff::Text("中文 Test").ttf("assets/fonts/Hiragino Sans GB.ttc", 24),
        }));

        lv_obj_t* col = lv_obj_get_child(lv_scr_act(), 0);
        lv_obj_t* label = lv_obj_get_child(col, 0);
        const lv_font_t* used = lv_obj_get_style_text_font(label, LV_PART_MAIN);
        std::printf("DSL 挂载:label 字体 line_height = %d\n", (int)used->line_height);
        if ((int)used->line_height != ftLineHeight) {
            std::printf("FAIL: DSL 挂载的字体不是刚加载的 TTF 字体\n");
            return 1;
        }
    }  // backend 析构 → 销毁缓存的字体

    std::printf("OK: freetype 全链路通\n");
    return 0;
}
