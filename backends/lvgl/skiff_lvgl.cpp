#include "skiff_lvgl.hpp"

namespace skiff {
namespace lvgl {

namespace {

// 文本是否含 CJK 字符。LV_SYMBOL 等私有区图标(U+E000–U+F8FF,
// UTF-8 编码 EF 80..A3 xx)不算 CJK。
bool containsCjk(const std::string& s) {
    for (size_t i = 0; i < s.size();) {
        const unsigned char b = (unsigned char)s[i];
        if (b < 0x80) { ++i; continue; }
        if (b == 0xEF && i + 1 < s.size()) {
            const unsigned char b2 = (unsigned char)s[i + 1];
            if (b2 >= 0x80 && b2 <= 0xA3) { i += 3; continue; }
        }
        return true;
    }
    return false;
}

// 把 skiff 状态映射到 LVGL 状态 selector
lv_state_t toLvState(const elements::state& s) {
    lv_state_t state = LV_STATE_DEFAULT;
    if (s.has(elements::state::Pressed))  state |= LV_STATE_PRESSED;
    if (s.has(elements::state::Checked))  state |= LV_STATE_CHECKED;
    if (s.has(elements::state::Focused))  state |= LV_STATE_FOCUSED;
    if (s.has(elements::state::Disabled)) state |= LV_STATE_DISABLED;
    if (s.has(elements::state::Hovered))  state |= LV_STATE_HOVERED;
    return state;
}

// Element 的小众数据(无 rare 时返回共享的空实例,调用方免判空)
const RareData& rareOf(const Element& e) {
    static const RareData kEmpty;
    return e.rare ? *e.rare : kEmpty;
}

// 应用 Element 上各状态覆盖的样式(目前支持 bg/fg,后续可扩展 font/size 等)
void applyStateStyles(lv_obj_t* obj, const Element& e) {
    if (!e.rare) return;
    for (std::map<elements::state, elements::attrOptions>::const_iterator it =
             e.rare->stateStyles.begin(); it != e.rare->stateStyles.end(); ++it) {
        const lv_state_t state = toLvState(it->first);
        if (it->second.hasBg) {
            lv_obj_set_style_bg_color(obj, lv_color_hex(it->second.bgColor), state);
            lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, state);
        }
        if (it->second.hasFg) {
            lv_obj_set_style_text_color(obj, lv_color_hex(it->second.fgColor), state);
        }
    }
}

// 根据 skiff 滚动方向设置 LVGL 滚动方向
void applyScroll(lv_obj_t* obj, ScrollDir dir, ScrollSnap snap) {
    lv_dir_t d = LV_DIR_NONE;
    if (dir == ScrollHorizontal) d = LV_DIR_HOR;
    else if (dir == ScrollVertical) d = LV_DIR_VER;
    else if (dir == ScrollBoth) d = LV_DIR_ALL;

    if (d != LV_DIR_NONE) {
        lv_obj_add_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_scroll_dir(obj, d);
        lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLL_MOMENTUM);
    }

    lv_scroll_snap_t s = LV_SCROLL_SNAP_NONE;
    if (snap == SnapStart) s = LV_SCROLL_SNAP_START;
    else if (snap == SnapCenter) s = LV_SCROLL_SNAP_CENTER;
    else if (snap == SnapEnd) s = LV_SCROLL_SNAP_END;

    if (s != LV_SCROLL_SNAP_NONE) {
        if (dir == ScrollHorizontal || dir == ScrollBoth) {
            lv_obj_set_scroll_snap_x(obj, s);
        }
        if (dir == ScrollVertical || dir == ScrollBoth) {
            lv_obj_set_scroll_snap_y(obj, s);
        }
    }
}

// 页签标题:优先 Tab() 设置的 tabTitle,兼容直接把标题写进 text 的旧写法
const std::string& tabTitleOf(const Element& e) {
    return e.tabTitle.empty() ? e.text : e.tabTitle;
}

// 按字号挑内置字体;CJK 内置字体只有 16px。
const lv_font_t* pickFont(int px, bool cjk) {
    if (cjk) return &lv_font_simsun_16_cjk;
    if (px >= 32) return &lv_font_montserrat_32;
    if (px >= 24) return &lv_font_montserrat_24;
    if (px >= 20) return &lv_font_montserrat_20;
    if (px >= 16) return &lv_font_montserrat_16;
    if (px > 0 && px <= 12) return &lv_font_montserrat_12;
    return &lv_font_montserrat_14;
}

void applyTextStyleRaw(lv_obj_t* label, const Element& e) {
    const bool cjk = containsCjk(e.text);
    if (e.options.fontPx > 0) {
        lv_obj_set_style_text_font(label, pickFont(e.options.fontPx, cjk), 0);
        // CJK 大字号:16px 字体渲染时放大(布局仍按 16px 计算)
        if (cjk && e.options.fontPx > 16) {
            lv_obj_set_style_transform_zoom(label,
                (lv_coord_t)(e.options.fontPx * 256 / 16), 0);
        }
    }
}

// 去掉 lv_obj 默认主题的"卡片"外观(白底/边框/阴影/内边距/最小尺寸)
void clearCard(lv_obj_t* obj) {
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_shadow_width(obj, 0, 0);
    lv_obj_set_style_radius(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_set_style_min_width(obj, 0, 0);
    lv_obj_set_style_min_height(obj, 0, 0);
    lv_obj_set_style_max_width(obj, LV_COORD_MAX, 0);
    lv_obj_set_style_max_height(obj, LV_COORD_MAX, 0);
    lv_obj_set_style_outline_width(obj, 0, 0);
    lv_obj_set_style_outline_pad(obj, 0, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

// 根据 Element 的 width/height 或 widthPct/heightPct 设置对象尺寸。
// pct 优先于 px;都没有则使用 LV_SIZE_CONTENT。
void applyObjectSize(lv_obj_t* obj, const Element& e, lv_obj_t* parent) {
    (void)parent;
    lv_coord_t w = LV_SIZE_CONTENT;
    lv_coord_t h = LV_SIZE_CONTENT;

    if (e.options.widthPct > 0) {
        w = lv_pct(e.options.widthPct);
    } else if (e.options.width > 0) {
        w = (lv_coord_t)e.options.width;
    }

    if (e.options.heightPct > 0) {
        h = lv_pct(e.options.heightPct);
    } else if (e.options.height > 0) {
        h = (lv_coord_t)e.options.height;
    }

    lv_obj_set_size(obj, w, h);
}

// 应用下边框
void applyBorderBottom(lv_obj_t* obj, const Element& e) {
    if (e.options.hasBorderBottom) {
        lv_obj_set_style_border_color(obj, lv_color_hex(e.options.borderBottomColor), 0);
        lv_obj_set_style_border_width(obj, (lv_coord_t)e.options.borderBottomWidth, 0);
        lv_obj_set_style_border_opa(obj, LV_OPA_COVER, 0);
        lv_obj_set_style_border_side(obj, LV_BORDER_SIDE_BOTTOM, 0);
    }
}

// 应用 Element 的方向性 padding。pad() 会同时设置 paddingPx 与四边;
// 单独 padTop() 等会覆盖对应边,优先级高于统一的 paddingPx。
void applyPadding(lv_obj_t* obj, const Element& e) {
    if (e.options.paddingTop == e.options.paddingBottom && e.options.paddingTop == e.options.paddingLeft &&
        e.options.paddingTop == e.options.paddingRight) {
        lv_obj_set_style_pad_all(obj, (lv_coord_t)e.options.paddingTop, 0);
    } else {
        lv_obj_set_style_pad_top(obj, (lv_coord_t)e.options.paddingTop, 0);
        lv_obj_set_style_pad_bottom(obj, (lv_coord_t)e.options.paddingBottom, 0);
        lv_obj_set_style_pad_left(obj, (lv_coord_t)e.options.paddingLeft, 0);
        lv_obj_set_style_pad_right(obj, (lv_coord_t)e.options.paddingRight, 0);
    }
}

// 纯文本 Button 在构造时只填了 text,没有 children。
// diff 时需要把 text 合成一个 Text 子节点,否则旧 Text 会被当成"已被删除"。
std::vector<Element> getChildElements(const Element& e) {
    if (e.kind != Element::Button || !e.children.empty()) return e.children;
    std::vector<Element> result;
    if (!e.text.empty()) {
        Element label;
        label.kind = Element::Text;
        label.text = e.text;
        label.options.fontPx = e.options.fontPx;
        label.options.ttfPath = e.options.ttfPath;
        if (e.options.hasFg) {
            label.options.hasFg = true;
            label.options.fgColor = e.options.fgColor;
        }
        result.push_back(label);
    }
    return result;
}

} // namespace

LvglBackend::LvglBackend(lv_obj_t* parent) : parent_(parent) {
    root_.obj = nullptr;
}

LvglBackend::~LvglBackend() {
    if (root_.obj) clearNode(root_);
    for (std::map<std::string, lv_font_t*>::iterator it = ftFonts_.begin();
         it != ftFonts_.end(); ++it) {
        lv_ft_font_destroy(it->second);
    }
}

void LvglBackend::clearNode(MountedNode& node) {
    for (size_t i = 0; i < node.children.size(); ++i) {
        clearNode(node.children[i]);
    }
    node.children.clear();
    if (node.obj) lv_obj_del(node.obj);
    node.callbacks.clear();  // lv_obj_del 已移除事件,这里释放 std::function
    // External 等不可见节点 obj 为 nullptr,不需要删除原生对象
}

bool ensureFreetype() {
    static bool ready = false;  // 进程内只初始化一次
    if (!ready) ready = lv_freetype_init(8, 8, 0);
    return ready;
}

lv_font_t* LvglBackend::getFtFont(const std::string& path, int px) {
    const std::string key = path + "@" + std::to_string(px);
    std::map<std::string, lv_font_t*>::iterator it = ftFonts_.find(key);
    if (it != ftFonts_.end()) return it->second;

    if (!ensureFreetype()) return nullptr;

    lv_ft_info_t info = {};
    info.name = path.c_str();  // v8.4 内部会复制路径(name_refer_save)
    info.weight = (uint16_t)px;
    info.style = FT_FONT_STYLE_NORMAL;
    if (!lv_ft_font_init(&info) || info.font == nullptr) {
        LV_LOG_WARN("skiff: TTF 字体加载失败 %s", path.c_str());
        return nullptr;
    }
    ftFonts_[key] = info.font;
    return info.font;
}

void LvglBackend::applyTextStyle(lv_obj_t* label, const Element& e) {
    if (!e.options.ttfPath.empty()) {
        lv_font_t* f = getFtFont(e.options.ttfPath, e.options.fontPx > 0 ? e.options.fontPx : 16);
        if (f != nullptr) lv_obj_set_style_text_font(label, f, 0);
    } else {
        applyTextStyleRaw(label, e);
    }
    if (e.options.hasFg) lv_obj_set_style_text_color(label, lv_color_hex(e.options.fgColor), 0);
}

// 页签栏(btnmatrix)样式:ttf()/font()/fg() 作用于页签文字,bg() 作用于页签栏底色
void LvglBackend::applyTabBarStyle(lv_obj_t* tabview, const Element& e) {
    lv_obj_t* btns = lv_tabview_get_tab_btns(tabview);
    lv_font_t* f = nullptr;
    if (!e.options.ttfPath.empty()) {
        f = getFtFont(e.options.ttfPath, e.options.fontPx > 0 ? e.options.fontPx : 16);
    } else if (e.options.fontPx > 0) {
        bool cjk = false;
        for (size_t i = 0; i < e.children.size(); ++i) {
            if (containsCjk(e.children[i].text)) { cjk = true; break; }
        }
        f = const_cast<lv_font_t*>(pickFont(e.options.fontPx, cjk));
    }
    if (f != nullptr) lv_obj_set_style_text_font(btns, f, 0);
    if (e.options.hasFg) lv_obj_set_style_text_color(btns, lv_color_hex(e.options.fgColor), 0);
    if (e.options.hasBg) {
        lv_obj_set_style_bg_color(btns, lv_color_hex(e.options.bgColor), 0);
        lv_obj_set_style_bg_opa(btns, LV_OPA_COVER, 0);
    }
}

void LvglBackend::updateContainerStyle(lv_obj_t* obj, const Element& oldE,
                                       const Element& newE) {
    const bool wasAnim = (oldE.options.animation != None);
    const bool isAnim = (newE.options.animation != None);
    bool styleChanged = false;

    // 动画类型发生变化:整个节点已在 updateNode 中重建,这里不处理
    // 对于已存在的动画节点,不更新其尺寸/浮动状态(创建时一次性确定)
    if (!wasAnim && !isAnim) {
        if (oldE.options.width != newE.options.width || oldE.options.height != newE.options.height ||
            oldE.options.widthPct != newE.options.widthPct || oldE.options.heightPct != newE.options.heightPct) {
            applyObjectSize(obj, newE, lv_obj_get_parent(obj));
            styleChanged = true;
        }
    }

    if (oldE.options.paddingTop != newE.options.paddingTop ||
        oldE.options.paddingBottom != newE.options.paddingBottom ||
        oldE.options.paddingLeft != newE.options.paddingLeft ||
        oldE.options.paddingRight != newE.options.paddingRight) {
        applyPadding(obj, newE);
        styleChanged = true;
    }

    if (oldE.options.hasBg != newE.options.hasBg || oldE.options.bgColor != newE.options.bgColor) {
        if (newE.options.hasBg) {
            lv_obj_set_style_bg_color(obj, lv_color_hex(newE.options.bgColor), 0);
            lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
        } else {
            lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, 0);
        }
        styleChanged = true;
    }

    if (oldE.options.hasRadiusPx != newE.options.hasRadiusPx ||
        oldE.options.radiusPx != newE.options.radiusPx) {
        lv_obj_set_style_radius(obj,
            newE.options.hasRadiusPx ? (lv_coord_t)newE.options.radiusPx : 0, 0);
        styleChanged = true;
    }

    if (oldE.options.flexGrow != newE.options.flexGrow) {
        lv_obj_set_flex_grow(obj, newE.options.flexGrow ? 1 : 0);
        styleChanged = true;
    }

    if (oldE.options.center != newE.options.center) {
        if (newE.options.center)
            lv_obj_set_flex_align(obj, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                                  LV_FLEX_ALIGN_CENTER);
        else
            lv_obj_set_flex_align(obj, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                                  LV_FLEX_ALIGN_START);
        styleChanged = true;
    }

    // 容器主轴/交叉轴间距
    if (oldE.options.spacingPx != newE.options.spacingPx) {
        if (newE.kind == Element::Column || newE.kind == Element::Button) {
            lv_obj_set_style_pad_row(obj, (lv_coord_t)newE.options.spacingPx, 0);
        } else {
            lv_obj_set_style_pad_column(obj, (lv_coord_t)newE.options.spacingPx, 0);
        }
        styleChanged = true;
    }

    // 水平对齐(仅 floating 节点生效)
    if (newE.options.isFloating && oldE.options.hAlign != newE.options.hAlign) {
        lv_align_t a = LV_ALIGN_TOP_LEFT;
        if (newE.options.hAlign == elements::HAlignCenter) a = LV_ALIGN_TOP_MID;
        else if (newE.options.hAlign == elements::HAlignEnd) a = LV_ALIGN_TOP_RIGHT;
        lv_obj_align(obj, a, 0, 0);
        styleChanged = true;
    }

    if (styleChanged) lv_obj_invalidate(obj);
}

lv_obj_t* LvglBackend::buildNode(Element e, lv_obj_t* parent,
                                 MountedNode* out) {
    lv_obj_t* obj = nullptr;
    std::vector<lv_obj_t*> tabPages;  // TabView:各页签的 page 容器
    switch (e.kind) {
    case Element::Column:
    case Element::Row: {
        const bool col = (e.kind == Element::Column);
        obj = lv_obj_create(parent);
        clearCard(obj);
        // 普通布局容器不拦截点击,让事件透到 Button/TapArea 等父级可点击对象
        lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICKABLE);
        applyObjectSize(obj, e, parent);
        lv_obj_set_flex_flow(obj, col ? LV_FLEX_FLOW_COLUMN : LV_FLEX_FLOW_ROW);
        if (e.options.flexGrow) lv_obj_set_flex_grow(obj, 1);
        applyPadding(obj, e);
        if (e.options.hasBg) {
            lv_obj_set_style_bg_color(obj, lv_color_hex(e.options.bgColor), 0);
            lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
        }
        if (e.options.spacingPx > 0) {
            if (col) lv_obj_set_style_pad_row(obj, (lv_coord_t)e.options.spacingPx, 0);
            else     lv_obj_set_style_pad_column(obj, (lv_coord_t)e.options.spacingPx, 0);
        }
        if (e.options.center) {
            lv_obj_set_flex_align(obj, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                                  LV_FLEX_ALIGN_CENTER);
        }
        applyBorderBottom(obj, e);
        applyStateStyles(obj, e);
        applyScroll(obj, e.options.scrollDir, e.options.scrollSnap);
        if (e.options.isFloating ||
            e.options.animation == SlideInRight ||
            e.options.animation == SlideInDown) {
            lv_obj_add_flag(obj, LV_OBJ_FLAG_FLOATING);
        }
        if (e.options.isFloating && e.options.hAlign != elements::HAlignStart) {
            lv_align_t a = LV_ALIGN_TOP_LEFT;
            if (e.options.hAlign == elements::HAlignCenter) a = LV_ALIGN_TOP_MID;
            else if (e.options.hAlign == elements::HAlignEnd) a = LV_ALIGN_TOP_RIGHT;
            lv_obj_align(obj, a, 0, 0);
        }
        break;
    }
    case Element::TapArea: {
        // 透明点击区:不渲染,只收点击事件
        obj = lv_obj_create(parent);
        clearCard(obj);
        applyObjectSize(obj, e, parent);
        lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
        if (e.options.isFloating) lv_obj_add_flag(obj, LV_OBJ_FLAG_FLOATING);
        if (e.onTap) {
            MountedNode tmp;
            tmp.callbacks.push_back(std::unique_ptr<std::function<void()> >(
                new std::function<void()>(e.onTap)));
            lv_obj_add_event_cb(obj, &LvglBackend::onClicked, LV_EVENT_CLICKED,
                                tmp.callbacks.back().get());
            if (out) out->callbacks.push_back(std::move(tmp.callbacks.back()));
        }
        break;
    }
    case Element::Spacer: {
        obj = lv_obj_create(parent);
        clearCard(obj);
        lv_obj_set_flex_grow(obj, 1);
        break;
    }
    case Element::Text: {
        obj = lv_label_create(parent);
        lv_label_set_text(obj, e.text.c_str());
        applyTextStyle(obj, e);
        applyStateStyles(obj, e);
        if (e.options.width > 0 || e.options.height > 0 ||
            e.options.widthPct > 0 || e.options.heightPct > 0) {
            applyObjectSize(obj, e, parent);
        }
        break;
    }
    case Element::Button: {
        obj = lv_btn_create(parent);
        lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(obj, LV_OBJ_FLAG_PRESS_LOCK);
        // 去掉默认主题的阴影/边框/最小尺寸,避免点击区域大于 size
        lv_obj_set_style_shadow_width(obj, 0, 0);
        lv_obj_set_style_border_width(obj, 0, 0);
        if (e.options.hasRadiusPx) {
            lv_obj_set_style_radius(obj, (lv_coord_t)e.options.radiusPx, 0);
        } else {
            lv_obj_set_style_radius(obj, 0, 0);
        }
        lv_obj_set_style_pad_all(obj, 0, 0);
        lv_obj_set_style_min_width(obj, 0, 0);
        lv_obj_set_style_min_height(obj, 0, 0);
        lv_obj_set_style_max_width(obj, LV_COORD_MAX, 0);
        lv_obj_set_style_max_height(obj, LV_COORD_MAX, 0);
        lv_obj_set_style_outline_width(obj, 0, 0);
        lv_obj_set_style_outline_pad(obj, 0, 0);
        applyObjectSize(obj, e, parent);
        lv_obj_set_flex_flow(obj, LV_FLEX_FLOW_COLUMN);
        if (e.options.flexGrow) lv_obj_set_flex_grow(obj, 1);
        applyPadding(obj, e);
        if (e.options.hasBg) {
            lv_obj_set_style_bg_color(obj, lv_color_hex(e.options.bgColor), 0);
            lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
        }
        if (e.options.spacingPx > 0) {
            lv_obj_set_style_pad_row(obj, (lv_coord_t)e.options.spacingPx, 0);
        }
        if (e.options.center) {
            lv_obj_set_flex_align(obj, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                                  LV_FLEX_ALIGN_CENTER);
        }
        applyBorderBottom(obj, e);
        applyStateStyles(obj, e);
        if (e.onTap) {
            MountedNode tmp;  // 临时占位,构建完再合并回调
            tmp.callbacks.push_back(std::unique_ptr<std::function<void()> >(
                new std::function<void()>(e.onTap)));
            lv_obj_add_event_cb(obj, &LvglBackend::onClicked, LV_EVENT_CLICKED,
                                tmp.callbacks.back().get());
            if (out) out->callbacks.push_back(std::move(tmp.callbacks.back()));
        }
        break;
    }
    case Element::Slider: {
        const RareData& rd = rareOf(e);
        obj = lv_slider_create(parent);
        lv_slider_set_range(obj, rd.min, rd.max);
        lv_slider_set_value(obj, rd.value, LV_ANIM_OFF);
        applyStateStyles(obj, e);
        applyObjectSize(obj, e, parent);
        if (rd.onValueChange) {
            MountedNode tmp;
            std::function<void(int)> cb = rd.onValueChange;
            tmp.callbacks.push_back(std::unique_ptr<std::function<void()> >(
                new std::function<void()>([obj, cb] {
                    cb(lv_slider_get_value(obj));
                })));
            lv_obj_add_event_cb(obj, &LvglBackend::onSliderChanged,
                                LV_EVENT_VALUE_CHANGED, tmp.callbacks.back().get());
            if (out) out->callbacks.push_back(std::move(tmp.callbacks.back()));
        }
        break;
    }
    case Element::TabView: {
        obj = lv_tabview_create(parent, LV_DIR_TOP, 48);
        clearCard(obj);  // 根对象去掉默认主题白底卡片样式
        applyObjectSize(obj, e, parent);
        if (e.options.flexGrow) lv_obj_set_flex_grow(obj, 1);
        applyTabBarStyle(obj, e);
        clearCard(lv_tabview_get_content(obj));  // 内容容器去掉默认主题白底
        // 每个 child 是一个页签:text 为标题,内容构建在该页签的 page 里
        // (记录在 tabPages 中,待 out 初始化后再递归构建)
        for (size_t i = 0; i < e.children.size(); ++i) {
            lv_obj_t* page = lv_tabview_add_tab(obj, tabTitleOf(e.children[i]).c_str());
            clearCard(page);  // 去掉默认主题的白底卡片样式,让页面底色透出来
            tabPages.push_back(page);
        }
        break;
    }
    }

    if (out) {
        out->element = e;
        out->obj = obj;
        out->children.clear();
    }

    // 容器/按钮:递归构建子节点
    if (e.kind == Element::Column || e.kind == Element::Row ||
        e.kind == Element::Button) {
        const std::vector<Element> childElements = getChildElements(e);
        for (size_t i = 0; i < childElements.size(); ++i) {
            if (out) {
                out->children.push_back(MountedNode());
                buildNode(childElements[i], obj, &out->children.back());
            } else {
                buildNode(childElements[i], obj, nullptr);
            }
        }
    } else if (e.kind == Element::TabView) {
        for (size_t i = 0; i < e.children.size(); ++i) {
            if (out) {
                out->children.push_back(MountedNode());
                buildNode(e.children[i], tabPages[i], &out->children.back());
            } else {
                buildNode(e.children[i], tabPages[i], nullptr);
            }
        }
    }

    if (e.options.animation == SlideInRight && obj) {
        newAnims_.push_back(NewAnim{NewAnim::Right, obj, parent});
    } else if (e.options.animation == SlideInDown && obj) {
        newAnims_.push_back(NewAnim{NewAnim::Down, obj, parent});
    }

    return obj;
}

void LvglBackend::updateNode(MountedNode& old, const Element& newE) {
    const Element& e = newE;

    // TabView 页签结构(数量/标题)变化时无法就地更新,需整子树重建
    bool tabStructureChanged = false;
    if (old.element.kind == Element::TabView &&
        e.kind == Element::TabView) {
        if (old.element.children.size() != e.children.size()) {
            tabStructureChanged = true;
        } else {
            for (size_t i = 0; i < e.children.size(); ++i) {
                if (tabTitleOf(old.element.children[i]) !=
                    tabTitleOf(e.children[i])) {
                    tabStructureChanged = true;
                    break;
                }
            }
        }
    }

    if (old.element.kind != e.kind || tabStructureChanged ||
        rareOf(old.element).stateStyles != rareOf(e).stateStyles ||
        old.element.options.scrollDir != e.options.scrollDir ||
        old.element.options.scrollSnap != e.options.scrollSnap ||
        (old.element.options.animation == SlideInRight) !=
            (e.options.animation == SlideInRight) ||
        (old.element.options.animation == SlideInDown) !=
            (e.options.animation == SlideInDown) ||
        (!old.element.onTap && e.onTap) || (old.element.onTap && !e.onTap) ||
        (!rareOf(old.element).onValueChange && rareOf(e).onValueChange) ||
        (rareOf(old.element).onValueChange && !rareOf(e).onValueChange)) {
        // 类型/动画/回调发生变化:整子树重建
        // TapArea 等不可见节点 obj 可能为 nullptr,直接用父容器作为挂载点
        lv_obj_t* parent = old.obj ? lv_obj_get_parent(old.obj) : nullptr;
        clearNode(old);
        old.element = e;
        old.obj = buildNode(e, parent, &old);
        return;
    }

    lv_obj_t* obj = old.obj;

    switch (e.kind) {
    case Element::Column:
    case Element::Row: {
        updateContainerStyle(obj, old.element, e);
        diffChildren(old, e.children);
        break;
    }
    case Element::Spacer:
        // spacer 无属性可更新
        break;
    case Element::TapArea:
        // TapArea 无样式/尺寸之外的属性需要更新;尺寸变化在初始条件里会触发重建
        break;
    case Element::Text: {
        if (old.element.text != e.text) {
            lv_label_set_text(obj, e.text.c_str());
        }
        if (old.element.text != e.text ||
            old.element.options.fontPx != e.options.fontPx ||
            old.element.options.ttfPath != e.options.ttfPath ||
            old.element.options.hasFg != e.options.hasFg ||
            old.element.options.fgColor != e.options.fgColor) {
            applyTextStyle(obj, e);
        }
        if (old.element.options.width != e.options.width || old.element.options.height != e.options.height ||
            old.element.options.widthPct != e.options.widthPct || old.element.options.heightPct != e.options.heightPct) {
            applyObjectSize(obj, e, lv_obj_get_parent(obj));
        }
        break;
    }
    case Element::Button: {
        // 复用旧节点时,lv_btn 上挂的还是旧的 onTap(user_data 指向
        // callbacks[0] 里的 std::function,地址稳定,直接改写内容即可)
        if (old.element.onTap && e.onTap && !old.callbacks.empty()) {
            *old.callbacks[0] = e.onTap;
        }
        updateContainerStyle(obj, old.element, e);
        diffChildren(old, getChildElements(e));
        break;
    }
    case Element::Slider: {
        const RareData& oldRd = rareOf(old.element);
        const RareData& newRd = rareOf(e);
        // 同 Button:复用节点时更新值变化回调
        if (oldRd.onValueChange && newRd.onValueChange &&
            !old.callbacks.empty()) {
            std::function<void(int)> cb = newRd.onValueChange;
            *old.callbacks[0] = [obj, cb] {
                cb(lv_slider_get_value(obj));
            };
        }
        if (oldRd.min != newRd.min || oldRd.max != newRd.max) {
            lv_slider_set_range(obj, newRd.min, newRd.max);
        }
        if (oldRd.value != newRd.value) {
            lv_slider_set_value(obj, newRd.value, LV_ANIM_OFF);
        }
        if (old.element.options.width != e.options.width || old.element.options.height != e.options.height ||
            old.element.options.widthPct != e.options.widthPct || old.element.options.heightPct != e.options.heightPct) {
            applyObjectSize(obj, e, lv_obj_get_parent(obj));
        }
        break;
    }
    case Element::TabView: {
        // 结构不变:更新尺寸、页签栏样式,各页内容就地 diff
        if (old.element.options.width != e.options.width || old.element.options.height != e.options.height ||
            old.element.options.widthPct != e.options.widthPct || old.element.options.heightPct != e.options.heightPct) {
            applyObjectSize(obj, e, lv_obj_get_parent(obj));
        }
        if (old.element.options.fontPx != e.options.fontPx ||
            old.element.options.ttfPath != e.options.ttfPath ||
            old.element.options.hasFg != e.options.hasFg || old.element.options.fgColor != e.options.fgColor ||
            old.element.options.hasBg != e.options.hasBg || old.element.options.bgColor != e.options.bgColor) {
            applyTabBarStyle(obj, e);
        }
        for (size_t i = 0; i < e.children.size(); ++i) {
            updateNode(old.children[i], e.children[i]);
        }
        break;
    }
    }

    old.element = e;
}

LvglBackend::MountedNode* LvglBackend::findMatch(
    std::vector<MountedNode>& oldChildren, const Element& newChild,
    size_t preferredIdx, std::vector<bool>& used) {
    // 1. key 匹配:有 key 的节点只按 key 复用
    if (!newChild.options.keyId.empty()) {
        for (size_t j = 0; j < oldChildren.size(); ++j) {
            if (!used[j] && oldChildren[j].element.options.keyId == newChild.options.keyId) {
                return &oldChildren[j];
            }
        }
        return nullptr;
    }

    // 2. 同位置同类型匹配(仅对无 key 节点)
    if (preferredIdx < oldChildren.size() && !used[preferredIdx] &&
        oldChildren[preferredIdx].element.kind == newChild.kind &&
        oldChildren[preferredIdx].element.options.keyId.empty()) {
        return &oldChildren[preferredIdx];
    }

    // 3. 任意同类型匹配(仅对无 key 节点)
    for (size_t j = 0; j < oldChildren.size(); ++j) {
        if (!used[j] && oldChildren[j].element.options.keyId.empty() &&
            oldChildren[j].element.kind == newChild.kind) {
            return &oldChildren[j];
        }
    }
    return nullptr;
}

void LvglBackend::diffChildren(MountedNode& parentNode,
                               const std::vector<Element>& newChildren) {
    std::vector<MountedNode>& oldChildren = parentNode.children;
    std::vector<MountedNode> nextChildren;
    nextChildren.reserve(newChildren.size());

    std::vector<bool> used(oldChildren.size(), false);

    for (size_t i = 0; i < newChildren.size(); ++i) {
        MountedNode* match = findMatch(oldChildren, newChildren[i], i, used);
        if (match) {
            size_t idx = static_cast<size_t>(match - oldChildren.data());
            used[idx] = true;
            MountedNode node = std::move(*match);
            if (node.obj) lv_obj_move_to_index(node.obj, (int32_t)i);
            updateNode(node, newChildren[i]);
            nextChildren.push_back(std::move(node));
        } else {
            MountedNode newNode;
            newNode.element = newChildren[i];
            newNode.obj = buildNode(newChildren[i], parentNode.obj, &newNode);
            if (newNode.obj) lv_obj_move_to_index(newNode.obj, (int32_t)i);
            nextChildren.push_back(std::move(newNode));
        }
    }

    for (size_t j = 0; j < oldChildren.size(); ++j) {
        if (!used[j]) clearNode(oldChildren[j]);
    }

    parentNode.children = std::move(nextChildren);
}

void LvglBackend::mount(const Element& root) {
    if (root_.obj == nullptr) {
        root_.element = root;
        root_.obj = buildNode(root, parent_, &root_);
    } else {
        updateNode(root_, root);
    }

    lv_obj_update_layout(parent_);
    playNewAnimations();
}

void LvglBackend::playNewAnimations() {
    for (size_t i = 0; i < newAnims_.size(); ++i) {
        const NewAnim& a = newAnims_[i];
        const lv_coord_t w = lv_obj_get_content_width(a.parent);
        const lv_coord_t h = lv_obj_get_content_height(a.parent);
        if (w <= 0 || h <= 0) continue;

        lv_anim_t anim;
        lv_anim_init(&anim);
        lv_anim_set_var(&anim, a.obj);
        lv_anim_set_time(&anim, 300);
        lv_anim_set_path_cb(&anim, lv_anim_path_ease_out);

        if (a.dir == NewAnim::Right) {
            // 右侧滑入:占满父容器,从右侧外进场
            lv_obj_set_size(a.obj, w, h);
            lv_obj_set_x(a.obj, w);
            lv_anim_set_values(&anim, w, 0);
            lv_anim_set_exec_cb(&anim, animSetX);
        } else { // Down
            // 顶部滑下:保持元素自身高度,从上方外进场
            const lv_coord_t ownH = lv_obj_get_height(a.obj);
            if (ownH <= 0) continue;
            lv_obj_set_y(a.obj, -ownH);
            lv_anim_set_values(&anim, -ownH, 0);
            lv_anim_set_exec_cb(&anim, animSetY);
        }
        lv_anim_start(&anim);
    }
    newAnims_.clear();
}

void LvglBackend::animSetX(void* obj, int32_t v) {
    lv_obj_set_x((lv_obj_t*)obj, (lv_coord_t)v);
}

void LvglBackend::animSetY(void* obj, int32_t v) {
    lv_obj_set_y((lv_obj_t*)obj, (lv_coord_t)v);
}

void LvglBackend::onClicked(lv_event_t* e) {
    std::function<void()>* cb =
        static_cast<std::function<void()>*>(lv_event_get_user_data(e));
    if (cb && *cb) (*cb)();
}

void LvglBackend::onSliderChanged(lv_event_t* e) {
    std::function<void()>* cb =
        static_cast<std::function<void()>*>(lv_event_get_user_data(e));
    if (cb && *cb) (*cb)();
}

namespace {

void flushDiscard(lv_disp_drv_t* drv, const lv_area_t*, lv_color_t*) {
    lv_disp_flush_ready(drv);
}

} // namespace

lv_disp_t* createHeadlessDisplay(int horRes, int verRes) {
    lv_color_t* pixels = new lv_color_t[(size_t)(horRes * verRes)];
    lv_disp_draw_buf_t* drawBuf = new lv_disp_draw_buf_t;
    lv_disp_draw_buf_init(drawBuf, pixels, nullptr, (uint32_t)(horRes * verRes));

    lv_disp_drv_t* drv = new lv_disp_drv_t;
    lv_disp_drv_init(drv);
    drv->draw_buf = drawBuf;
    drv->flush_cb = flushDiscard;
    drv->hor_res = (lv_coord_t)horRes;
    drv->ver_res = (lv_coord_t)verRes;
    return lv_disp_drv_register(drv);
}

} // namespace lvgl
} // namespace skiff
