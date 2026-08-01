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
    if (e.fontPx > 0) {
        lv_obj_set_style_text_font(label, pickFont(e.fontPx, cjk), 0);
        // CJK 大字号:16px 字体渲染时放大(布局仍按 16px 计算)
        if (cjk && e.fontPx > 16) {
            lv_obj_set_style_transform_zoom(label,
                (lv_coord_t)(e.fontPx * 256 / 16), 0);
        }
    }
}

// 去掉 lv_obj 默认主题的"卡片"外观(白底/边框/阴影/内边距)
void clearCard(lv_obj_t* obj) {
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_shadow_width(obj, 0, 0);
    lv_obj_set_style_radius(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

// 应用 Element 的方向性 padding。pad() 会同时设置 paddingPx 与四边;
// 单独 padTop() 等会覆盖对应边,优先级高于统一的 paddingPx。
void applyPadding(lv_obj_t* obj, const Element& e) {
    if (e.paddingTop == e.paddingBottom && e.paddingTop == e.paddingLeft &&
        e.paddingTop == e.paddingRight) {
        lv_obj_set_style_pad_all(obj, (lv_coord_t)e.paddingTop, 0);
    } else {
        lv_obj_set_style_pad_top(obj, (lv_coord_t)e.paddingTop, 0);
        lv_obj_set_style_pad_bottom(obj, (lv_coord_t)e.paddingBottom, 0);
        lv_obj_set_style_pad_left(obj, (lv_coord_t)e.paddingLeft, 0);
        lv_obj_set_style_pad_right(obj, (lv_coord_t)e.paddingRight, 0);
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
        label.fontPx = e.fontPx;
        label.ttfPath = e.ttfPath;
        if (e.hasFg) {
            label.hasFg = true;
            label.fgColor = e.fgColor;
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
    if (!e.ttfPath.empty()) {
        lv_font_t* f = getFtFont(e.ttfPath, e.fontPx > 0 ? e.fontPx : 16);
        if (f != nullptr) lv_obj_set_style_text_font(label, f, 0);
    } else {
        applyTextStyleRaw(label, e);
    }
    if (e.hasFg) lv_obj_set_style_text_color(label, lv_color_hex(e.fgColor), 0);
}

// 页签栏(btnmatrix)样式:ttf()/font()/fg() 作用于页签文字,bg() 作用于页签栏底色
void LvglBackend::applyTabBarStyle(lv_obj_t* tabview, const Element& e) {
    lv_obj_t* btns = lv_tabview_get_tab_btns(tabview);
    lv_font_t* f = nullptr;
    if (!e.ttfPath.empty()) {
        f = getFtFont(e.ttfPath, e.fontPx > 0 ? e.fontPx : 16);
    } else if (e.fontPx > 0) {
        bool cjk = false;
        for (size_t i = 0; i < e.children.size(); ++i) {
            if (containsCjk(e.children[i].text)) { cjk = true; break; }
        }
        f = const_cast<lv_font_t*>(pickFont(e.fontPx, cjk));
    }
    if (f != nullptr) lv_obj_set_style_text_font(btns, f, 0);
    if (e.hasFg) lv_obj_set_style_text_color(btns, lv_color_hex(e.fgColor), 0);
    if (e.hasBg) {
        lv_obj_set_style_bg_color(btns, lv_color_hex(e.bgColor), 0);
        lv_obj_set_style_bg_opa(btns, LV_OPA_COVER, 0);
    }
}

void LvglBackend::updateContainerStyle(lv_obj_t* obj, const Element& oldE,
                                       const Element& newE) {
    const bool wasAnim = (oldE.animation == Element::SlideInRight);
    const bool isAnim = (newE.animation == Element::SlideInRight);
    bool styleChanged = false;

    // 动画类型发生变化:整个节点已在 updateNode 中重建,这里不处理
    // 对于已存在的动画节点,不更新其尺寸/浮动状态(创建时一次性确定)
    if (!wasAnim && !isAnim) {
        if (oldE.width != newE.width || oldE.height != newE.height) {
            lv_obj_set_size(obj,
                newE.width > 0 ? (lv_coord_t)newE.width : LV_SIZE_CONTENT,
                newE.height > 0 ? (lv_coord_t)newE.height : LV_SIZE_CONTENT);
            styleChanged = true;
        }
    }

    if (oldE.paddingTop != newE.paddingTop ||
        oldE.paddingBottom != newE.paddingBottom ||
        oldE.paddingLeft != newE.paddingLeft ||
        oldE.paddingRight != newE.paddingRight) {
        applyPadding(obj, newE);
        styleChanged = true;
    }

    if (oldE.hasBg != newE.hasBg || oldE.bgColor != newE.bgColor) {
        if (newE.hasBg) {
            lv_obj_set_style_bg_color(obj, lv_color_hex(newE.bgColor), 0);
            lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
        } else {
            lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, 0);
        }
        styleChanged = true;
    }

    if (oldE.flexGrow != newE.flexGrow) {
        lv_obj_set_flex_grow(obj, newE.flexGrow ? 1 : 0);
        styleChanged = true;
    }

    if (oldE.center != newE.center) {
        if (newE.center)
            lv_obj_set_flex_align(obj, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                                  LV_FLEX_ALIGN_CENTER);
        else
            lv_obj_set_flex_align(obj, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                                  LV_FLEX_ALIGN_START);
        styleChanged = true;
    }

    // 容器主轴/交叉轴间距
    if (oldE.spacing != newE.spacing) {
        if (newE.kind == Element::Column || newE.kind == Element::Button) {
            lv_obj_set_style_pad_row(obj, (lv_coord_t)newE.spacing, 0);
        } else {
            lv_obj_set_style_pad_column(obj, (lv_coord_t)newE.spacing, 0);
        }
        styleChanged = true;
    }

    if (styleChanged) lv_obj_invalidate(obj);
}

lv_obj_t* LvglBackend::buildNode(const Element& e, lv_obj_t* parent,
                                 MountedNode* out) {
    lv_obj_t* obj = nullptr;
    std::vector<lv_obj_t*> tabPages;  // TabView:各页签的 page 容器
    switch (e.kind) {
    case Element::Column:
    case Element::Row: {
        const bool col = (e.kind == Element::Column);
        obj = lv_obj_create(parent);
        clearCard(obj);
        lv_obj_set_size(obj,
            e.width > 0 ? (lv_coord_t)e.width : LV_SIZE_CONTENT,
            e.height > 0 ? (lv_coord_t)e.height : LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(obj, col ? LV_FLEX_FLOW_COLUMN : LV_FLEX_FLOW_ROW);
        if (e.flexGrow) lv_obj_set_flex_grow(obj, 1);
        applyPadding(obj, e);
        if (e.hasBg) {
            lv_obj_set_style_bg_color(obj, lv_color_hex(e.bgColor), 0);
            lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
        }
        if (e.spacing > 0) {
            if (col) lv_obj_set_style_pad_row(obj, (lv_coord_t)e.spacing, 0);
            else     lv_obj_set_style_pad_column(obj, (lv_coord_t)e.spacing, 0);
        }
        if (e.center) {
            lv_obj_set_flex_align(obj, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                                  LV_FLEX_ALIGN_CENTER);
        }
        if (e.animation == Element::SlideInRight) {
            lv_obj_add_flag(obj, LV_OBJ_FLAG_FLOATING);
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
        if (e.width > 0 || e.height > 0) {
            lv_obj_set_size(obj,
                e.width > 0 ? (lv_coord_t)e.width : LV_SIZE_CONTENT,
                e.height > 0 ? (lv_coord_t)e.height : LV_SIZE_CONTENT);
        }
        break;
    }
    case Element::Button: {
        obj = lv_btn_create(parent);
        lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_size(obj,
            e.width > 0 ? (lv_coord_t)e.width : LV_SIZE_CONTENT,
            e.height > 0 ? (lv_coord_t)e.height : LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(obj, LV_FLEX_FLOW_COLUMN);
        if (e.flexGrow) lv_obj_set_flex_grow(obj, 1);
        applyPadding(obj, e);
        if (e.hasBg) {
            lv_obj_set_style_bg_color(obj, lv_color_hex(e.bgColor), 0);
        }
        if (e.spacing > 0) {
            lv_obj_set_style_pad_row(obj, (lv_coord_t)e.spacing, 0);
        }
        if (e.center) {
            lv_obj_set_flex_align(obj, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                                  LV_FLEX_ALIGN_CENTER);
        }
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
        obj = lv_slider_create(parent);
        lv_slider_set_range(obj, e.min, e.max);
        lv_slider_set_value(obj, e.value, LV_ANIM_OFF);
        if (e.width > 0 || e.height > 0) {
            lv_obj_set_size(obj,
                e.width > 0 ? (lv_coord_t)e.width : LV_SIZE_CONTENT,
                e.height > 0 ? (lv_coord_t)e.height : LV_SIZE_CONTENT);
        }
        if (e.onValueChange) {
            MountedNode tmp;
            std::function<void(int)> cb = e.onValueChange;
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
        lv_obj_set_size(obj,
            e.width > 0 ? (lv_coord_t)e.width : LV_SIZE_CONTENT,
            e.height > 0 ? (lv_coord_t)e.height : LV_SIZE_CONTENT);
        if (e.flexGrow) lv_obj_set_flex_grow(obj, 1);
        applyTabBarStyle(obj, e);
        clearCard(lv_tabview_get_content(obj));  // 内容容器去掉默认主题白底
        // 每个 child 是一个页签:text 为标题,内容构建在该页签的 page 里
        // (记录在 tabPages 中,待 out 初始化后再递归构建)
        for (size_t i = 0; i < e.children.size(); ++i) {
            lv_obj_t* page = lv_tabview_add_tab(obj, e.children[i].text.c_str());
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

    if (e.animation == Element::SlideInRight && obj) {
        newAnims_.push_back(NewAnim{obj, parent});
    }

    return obj;
}

void LvglBackend::updateNode(MountedNode& old, const Element& newE) {
    // TabView 页签结构(数量/标题)变化时无法就地更新,需整子树重建
    bool tabStructureChanged = false;
    if (old.element.kind == Element::TabView &&
        newE.kind == Element::TabView) {
        if (old.element.children.size() != newE.children.size()) {
            tabStructureChanged = true;
        } else {
            for (size_t i = 0; i < newE.children.size(); ++i) {
                if (old.element.children[i].text != newE.children[i].text) {
                    tabStructureChanged = true;
                    break;
                }
            }
        }
    }

    if (old.element.kind != newE.kind || tabStructureChanged ||
        (old.element.animation == Element::SlideInRight) !=
            (newE.animation == Element::SlideInRight) ||
        (!old.element.onTap && newE.onTap) || (old.element.onTap && !newE.onTap) ||
        (!old.element.onValueChange && newE.onValueChange) ||
        (old.element.onValueChange && !newE.onValueChange)) {
        // 类型/动画/回调有无发生变化:整子树重建
        lv_obj_t* parent = lv_obj_get_parent(old.obj);  // 先保存父节点
        clearNode(old);
        old.element = newE;
        old.obj = buildNode(newE, parent, &old);
        return;
    }

    lv_obj_t* obj = old.obj;

    switch (newE.kind) {
    case Element::Column:
    case Element::Row: {
        updateContainerStyle(obj, old.element, newE);
        diffChildren(old, newE.children);
        break;
    }
    case Element::Spacer:
        // spacer 无属性可更新
        break;
    case Element::Text: {
        if (old.element.text != newE.text) {
            lv_label_set_text(obj, newE.text.c_str());
        }
        if (old.element.text != newE.text ||
            old.element.fontPx != newE.fontPx ||
            old.element.ttfPath != newE.ttfPath ||
            old.element.hasFg != newE.hasFg ||
            old.element.fgColor != newE.fgColor) {
            applyTextStyle(obj, newE);
        }
        if (old.element.width != newE.width || old.element.height != newE.height) {
            lv_obj_set_size(obj,
                newE.width > 0 ? (lv_coord_t)newE.width : LV_SIZE_CONTENT,
                newE.height > 0 ? (lv_coord_t)newE.height : LV_SIZE_CONTENT);
        }
        break;
    }
    case Element::Button: {
        // 复用旧节点时,lv_btn 上挂的还是旧的 onTap(user_data 指向
        // callbacks[0] 里的 std::function,地址稳定,直接改写内容即可)
        if (old.element.onTap && newE.onTap && !old.callbacks.empty()) {
            *old.callbacks[0] = newE.onTap;
        }
        updateContainerStyle(obj, old.element, newE);
        diffChildren(old, getChildElements(newE));
        break;
    }
    case Element::Slider: {
        // 同 Button:复用节点时更新值变化回调
        if (old.element.onValueChange && newE.onValueChange &&
            !old.callbacks.empty()) {
            std::function<void(int)> cb = newE.onValueChange;
            *old.callbacks[0] = [obj, cb] {
                cb(lv_slider_get_value(obj));
            };
        }
        if (old.element.min != newE.min || old.element.max != newE.max) {
            lv_slider_set_range(obj, newE.min, newE.max);
        }
        if (old.element.value != newE.value) {
            lv_slider_set_value(obj, newE.value, LV_ANIM_OFF);
        }
        if (old.element.width != newE.width || old.element.height != newE.height) {
            lv_obj_set_size(obj,
                newE.width > 0 ? (lv_coord_t)newE.width : LV_SIZE_CONTENT,
                newE.height > 0 ? (lv_coord_t)newE.height : LV_SIZE_CONTENT);
        }
        break;
    }
    case Element::TabView: {
        // 结构不变:更新尺寸、页签栏样式,各页内容就地 diff
        if (old.element.width != newE.width || old.element.height != newE.height) {
            lv_obj_set_size(obj,
                newE.width > 0 ? (lv_coord_t)newE.width : LV_SIZE_CONTENT,
                newE.height > 0 ? (lv_coord_t)newE.height : LV_SIZE_CONTENT);
        }
        if (old.element.fontPx != newE.fontPx ||
            old.element.ttfPath != newE.ttfPath ||
            old.element.hasFg != newE.hasFg || old.element.fgColor != newE.fgColor ||
            old.element.hasBg != newE.hasBg || old.element.bgColor != newE.bgColor) {
            applyTabBarStyle(obj, newE);
        }
        for (size_t i = 0; i < newE.children.size(); ++i) {
            updateNode(old.children[i], newE.children[i]);
        }
        break;
    }
    }

    old.element = newE;
}

LvglBackend::MountedNode* LvglBackend::findMatch(
    std::vector<MountedNode>& oldChildren, const Element& newChild,
    size_t preferredIdx, std::vector<bool>& used) {
    // 1. key 匹配:有 key 的节点只按 key 复用
    if (!newChild.keyId.empty()) {
        for (size_t j = 0; j < oldChildren.size(); ++j) {
            if (!used[j] && oldChildren[j].element.keyId == newChild.keyId) {
                return &oldChildren[j];
            }
        }
        return nullptr;
    }

    // 2. 同位置同类型匹配(仅对无 key 节点)
    if (preferredIdx < oldChildren.size() && !used[preferredIdx] &&
        oldChildren[preferredIdx].element.kind == newChild.kind &&
        oldChildren[preferredIdx].element.keyId.empty()) {
        return &oldChildren[preferredIdx];
    }

    // 3. 任意同类型匹配(仅对无 key 节点)
    for (size_t j = 0; j < oldChildren.size(); ++j) {
        if (!used[j] && oldChildren[j].element.keyId.empty() &&
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
            lv_obj_move_to_index(node.obj, (int32_t)i);
            updateNode(node, newChildren[i]);
            nextChildren.push_back(std::move(node));
        } else {
            MountedNode newNode;
            newNode.element = newChildren[i];
            newNode.obj = buildNode(newChildren[i], parentNode.obj, &newNode);
            lv_obj_move_to_index(newNode.obj, (int32_t)i);
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
        lv_obj_set_size(a.obj, w, h);
        lv_obj_set_x(a.obj, w);
        lv_anim_t anim;
        lv_anim_init(&anim);
        lv_anim_set_var(&anim, a.obj);
        lv_anim_set_values(&anim, w, 0);
        lv_anim_set_exec_cb(&anim, animSetX);
        lv_anim_set_time(&anim, 300);
        lv_anim_set_path_cb(&anim, lv_anim_path_ease_out);
        lv_anim_start(&anim);
    }
    newAnims_.clear();
}

void LvglBackend::animSetX(void* obj, int32_t v) {
    lv_obj_set_x((lv_obj_t*)obj, (lv_coord_t)v);
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
