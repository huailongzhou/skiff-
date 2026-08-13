// attrOptions:常用属性选项包,用于批量设置 Element 的样式与布局属性。
//
// 用法:
//   auto opts = skiff::elements::attrOptions()
//       .size(100, 48)
//       .bg(0x1A222B)
//       .ttf(kFont, 20);
//
//   TopNav({...}).title(...).applyOptions(skiff::elements::state(), opts);
//   Text("hello").applyOptions(skiff::elements::state::pressed, opts.fg(0xFFFFFF));
#pragma once

#include <cstdint>
#include <string>

namespace skiff {

// 入场动画类型
enum Animation {
    None,         // 无动画
    SlideInRight, // 从右侧滑入
    SlideInDown   // 从顶部向下滑入(下拉菜单效果)
};

// 滚动方向
enum ScrollDir {
    ScrollNone,      // 不可滚动
    ScrollHorizontal,// 水平滚动
    ScrollVertical,  // 垂直滚动
    ScrollBoth       // 双向滚动
};

// 滚动停止时对齐方式
enum ScrollSnap {
    SnapNone,   // 不对齐
    SnapStart,  // 对齐到起始位置
    SnapCenter, // 对齐到中心
    SnapEnd     // 对齐到末尾
};

namespace elements {

// 水平对齐方式
enum HAlign {
    HAlignStart,  // 居左/默认
    HAlignCenter, // 居中
    HAlignEnd     // 居右
};

// state:控件状态,参考 LVGL 的 selector/状态位设计。
// 支持组合状态,例如 state::pressed | state::checked。
struct state {
    enum bits {
        Default    = 0,
        Pressed    = 1 << 0,
        Checked    = 1 << 1,
        Focused    = 1 << 2,
        Disabled   = 1 << 3,
        Hovered    = 1 << 4,
        Selected   = 1 << 15,  // 供复合组件(如 TabView)表示"选中"
        Unselected = 1 << 16,  // 供复合组件(如 TabView)表示"未选中"
    };

    uint32_t value;

    state(uint32_t v = Default) : value(v) {}

    bool operator==(const state& other) const { return value == other.value; }
    bool operator!=(const state& other) const { return value != other.value; }
    bool operator<(const state& other) const { return value < other.value; }

    state operator|(const state& other) const { return state(value | other.value); }
    state operator|(bits b) const { return state(value | b); }
    state operator&(const state& other) const { return state(value & other.value); }
    state operator&(bits b) const { return state(value & b); }

    bool has(bits b) const { return (value & b) != 0; }

    // 预定义常量状态(方便书写)
    static state pressed()    { return state(Pressed); }
    static state checked()    { return state(Checked); }
    static state focused()    { return state(Focused); }
    static state disabled()   { return state(Disabled); }
    static state hovered()    { return state(Hovered); }
    static state selected()   { return state(Selected); }
    static state unselected() { return state(Unselected); }
};

inline state operator|(state::bits a, state::bits b) {
    return state(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

struct attrOptions {
    int width, height;              // 尺寸(px)
    int widthPct, heightPct;        // 尺寸(占父容器内容区的百分比,>0 时优先)
    int paddingPx;                  // 统一内边距
    int paddingTop, paddingBottom, paddingLeft, paddingRight; // 方向性内边距
    uint32_t bgColor;               // 背景色
    bool hasBg;
    uint32_t fgColor;               // 前景/文字色
    bool hasFg;
    uint32_t borderBottomColor;     // 下边框颜色
    bool hasBorderBottom;
    int borderBottomWidth;          // 下边框宽度(px)
    int fontPx;                     // 字号
    std::string ttfPath;            // TTF 字体路径
    int itemHeightPx;               // 列表/网格单项高度(px)
    bool center;                    // 居中
    bool flexGrow;                  // 填充剩余空间
    bool isFloating;                // 浮动节点:不参与父容器布局,可覆盖在其他内容之上
    int radiusPx;                   // 圆角半径(px)
    bool hasRadiusPx;
    int spacingPx;                  // Column/Row 子项间距(px)
    Animation animation;            // 入场动画
    ScrollDir scrollDir;            // 滚动方向
    ScrollSnap scrollSnap;          // 滚动对齐方式
    HAlign hAlign;                  // 水平对齐方式(仅在 floating 时生效)
    std::string keyId;              // 节点 key

    attrOptions()
        : width(0), height(0), widthPct(0), heightPct(0), paddingPx(0),
          paddingTop(0), paddingBottom(0), paddingLeft(0), paddingRight(0),
          bgColor(0), hasBg(false), fgColor(0), hasFg(false),
          borderBottomColor(0), hasBorderBottom(false), borderBottomWidth(0),
          fontPx(0), itemHeightPx(0), center(false), flexGrow(false), isFloating(false),
          radiusPx(0), hasRadiusPx(false), spacingPx(0),
          animation(None), scrollDir(ScrollNone), scrollSnap(SnapNone),
          hAlign(HAlignStart) {}

    attrOptions& size(int w, int h) {
        width = w; height = h; return *this;
    }

    // 按父容器内容区百分比设置尺寸(例如 sizePct(50, 100) 表示宽一半、高全满)
    attrOptions& sizePct(int w, int h) {
        widthPct = w; heightPct = h; return *this;
    }

    attrOptions& bg(uint32_t rgb) {
        bgColor = rgb; hasBg = true; return *this;
    }

    attrOptions& fg(uint32_t rgb) {
        fgColor = rgb; hasFg = true; return *this;
    }

    attrOptions& borderBottom(uint32_t rgb, int widthPx = 1) {
        borderBottomColor = rgb; hasBorderBottom = true; borderBottomWidth = widthPx; return *this;
    }

    attrOptions& font(int px) {
        fontPx = px; return *this;
    }

    attrOptions& itemHeight(int px) {
        itemHeightPx = px; return *this;
    }

    attrOptions& pad(int px) {
        paddingPx = px;
        paddingTop = paddingBottom = paddingLeft = paddingRight = px;
        return *this;
    }

    attrOptions& padTop(int px)    { paddingTop = px; return *this; }
    attrOptions& padBottom(int px) { paddingBottom = px; return *this; }
    attrOptions& padLeft(int px)   { paddingLeft = px; return *this; }
    attrOptions& padRight(int px)  { paddingRight = px; return *this; }

    attrOptions& ttf(const char* path, int px) {
        ttfPath = path; fontPx = px; return *this;
    }

    attrOptions& centered() { center = true; return *this; }
    attrOptions& expand()   { flexGrow = true; return *this; }

    attrOptions& floating() { isFloating = true; return *this; }

    attrOptions& radius(int px) {
        radiusPx = px; hasRadiusPx = true; return *this;
    }

    attrOptions& spacing(int px) {
        spacingPx = px; return *this;
    }

    attrOptions& slideInRight() {
        animation = SlideInRight; return *this;
    }

    attrOptions& slideInDown() {
        animation = SlideInDown; return *this;
    }

    attrOptions& key(const char* k) {
        keyId = k; return *this;
    }

    attrOptions& scrollHorizontal() {
        scrollDir = ScrollHorizontal; return *this;
    }

    attrOptions& scrollVertical() {
        scrollDir = ScrollVertical; return *this;
    }

    attrOptions& scrollSnapStart() {
        scrollSnap = SnapStart; return *this;
    }

    attrOptions& scrollSnapCenter() {
        scrollSnap = SnapCenter; return *this;
    }

    attrOptions& scrollSnapEnd() {
        scrollSnap = SnapEnd; return *this;
    }

    attrOptions& alignLeft()   { hAlign = HAlignStart;  return *this; }
    attrOptions& alignCenter() { hAlign = HAlignCenter; return *this; }
    attrOptions& alignRight()  { hAlign = HAlignEnd;    return *this; }

    bool operator==(const attrOptions& other) const {
        return width == other.width && height == other.height &&
               widthPct == other.widthPct && heightPct == other.heightPct &&
               paddingPx == other.paddingPx &&
               paddingTop == other.paddingTop &&
               paddingBottom == other.paddingBottom &&
               paddingLeft == other.paddingLeft &&
               paddingRight == other.paddingRight &&
               bgColor == other.bgColor && hasBg == other.hasBg &&
               fgColor == other.fgColor && hasFg == other.hasFg &&
               borderBottomColor == other.borderBottomColor &&
               hasBorderBottom == other.hasBorderBottom &&
               borderBottomWidth == other.borderBottomWidth &&
               fontPx == other.fontPx && ttfPath == other.ttfPath &&
               itemHeightPx == other.itemHeightPx &&
               center == other.center && flexGrow == other.flexGrow &&
               isFloating == other.isFloating &&
               radiusPx == other.radiusPx && hasRadiusPx == other.hasRadiusPx &&
               spacingPx == other.spacingPx &&
               animation == other.animation &&
               scrollDir == other.scrollDir && scrollSnap == other.scrollSnap &&
               keyId == other.keyId;
    }

    bool operator!=(const attrOptions& other) const {
        return !(*this == other);
    }
};

} // namespace elements

} // namespace skiff
