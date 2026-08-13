/* skiff 的 LVGL 8 配置(宿主开发/示例用)。
 * 嵌入式实机请按目标平台修改:颜色深度、内存池、字体、日志等。 */
#ifndef LV_CONF_H
#define LV_CONF_H

#define LV_COLOR_DEPTH 32

/* 日志走 printf,方便宿主调试 */
#define LV_USE_LOG 1
#if LV_USE_LOG
#  define LV_LOG_LEVEL LV_LOG_LEVEL_WARN
#  define LV_LOG_PRINTF 1
#endif

/* flex 布局(VStack/HStack 的底层实现) */
#define LV_USE_FLEX 1

/* 像素画布(skiff::Canvas → lv_canvas);缓冲由后端用 new[] 分配,不走本内存池 */
#define LV_USE_CANVAS 1

/* 演示页面用到的字体;内置 CJK 字体只有 16px,大字号由后端缩放 */
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_MONTSERRAT_32 1
#define LV_FONT_SIMSUN_16_CJK 1

/* FreeType:TTF 矢量字体,任意字号(库在 third_party/freetype-2.13.2) */
#define LV_USE_FREETYPE 1
#define LV_FREETYPE_CACHE_SIZE (64 * 1024)

/* 内置内存池大小(字节)。Canvas 像素缓冲在池外;绘制时 LVGL 会要一些临时块 */
#define LV_MEM_SIZE (256U * 1024U)

#endif /* LV_CONF_H */
