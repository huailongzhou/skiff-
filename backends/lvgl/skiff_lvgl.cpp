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

// 通用修饰:size / padding / bg
void applyCommon(lv_obj_t* obj, const Element& e) {
    if (e.width > 0 || e.height > 0) {
        lv_obj_set_size(obj,
            e.width > 0 ? (lv_coord_t)e.width : LV_SIZE_CONTENT,
            e.height > 0 ? (lv_coord_t)e.height : LV_SIZE_CONTENT);
    }
    if (e.paddingPx > 0) lv_obj_set_style_pad_all(obj, (lv_coord_t)e.paddingPx, 0);
    if (e.hasBg) {
        lv_obj_set_style_bg_color(obj, lv_color_hex(e.bgColor), 0);
        lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    }
}

} // namespace

LvglBackend::LvglBackend(lv_obj_t* parent) : parent_(parent) {}

LvglBackend::~LvglBackend() {
    for (std::map<std::string, lv_font_t*>::iterator it = ftFonts_.begin();
         it != ftFonts_.end(); ++it) {
        lv_ft_font_destroy(it->second);
    }
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
        // TTF 矢量字体:任意字号,中英文同字体
        lv_font_t* f = getFtFont(e.ttfPath, e.fontPx > 0 ? e.fontPx : 16);
        if (f != nullptr) lv_obj_set_style_text_font(label, f, 0);
    } else {
        applyTextStyleRaw(label, e);
    }
    if (e.hasFg) lv_obj_set_style_text_color(label, lv_color_hex(e.fgColor), 0);
}

void LvglBackend::mount(const Element& root) {
    // 先删旧控件树(删除过程不会触发点击回调),再释放旧回调实体。
    lv_obj_clean(parent_);
    callbacks_.clear();
    buildNode(root, parent_);
}

lv_obj_t* LvglBackend::buildNode(const Element& e, lv_obj_t* parent) {
    switch (e.kind) {
    case Element::Column:
    case Element::Row: {
        const bool col = (e.kind == Element::Column);
        lv_obj_t* box = lv_obj_create(parent);
        lv_obj_set_size(box, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        clearCard(box);
        lv_obj_set_flex_flow(box, col ? LV_FLEX_FLOW_COLUMN : LV_FLEX_FLOW_ROW);
        applyCommon(box, e);
        if (e.spacing > 0) {
            if (col) lv_obj_set_style_pad_row(box, (lv_coord_t)e.spacing, 0);
            else     lv_obj_set_style_pad_column(box, (lv_coord_t)e.spacing, 0);
        }
        if (e.center) {
            lv_obj_set_flex_align(box, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                                  LV_FLEX_ALIGN_CENTER);
        }
        for (size_t i = 0; i < e.children.size(); ++i) {
            buildNode(e.children[i], box);
        }
        return box;
    }
    case Element::Spacer: {
        lv_obj_t* sp = lv_obj_create(parent);
        clearCard(sp);
        lv_obj_set_flex_grow(sp, 1);
        return sp;
    }
    case Element::Text: {
        lv_obj_t* label = lv_label_create(parent);
        lv_label_set_text(label, e.text.c_str());
        applyTextStyle(label, e);
        applyCommon(label, e);
        return label;
    }
    case Element::Button: {
        lv_obj_t* btn = lv_btn_create(parent);
        lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
        applyCommon(btn, e);
        if (e.children.empty()) {
            lv_obj_t* label = lv_label_create(btn);
            lv_label_set_text(label, e.text.c_str());
            applyTextStyle(label, e);
            lv_obj_center(label);
        } else {
            lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_COLUMN);
            lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                                  LV_FLEX_ALIGN_CENTER);
            lv_obj_set_style_pad_row(btn, 10, 0);
            for (size_t i = 0; i < e.children.size(); ++i) {
                buildNode(e.children[i], btn);
            }
        }
        if (e.onTap) {
            callbacks_.push_back(std::unique_ptr<std::function<void()> >(
                new std::function<void()>(e.onTap)));
            lv_obj_add_event_cb(btn, &LvglBackend::onClicked, LV_EVENT_CLICKED,
                                callbacks_.back().get());
        }
        return btn;
    }
    }
    return nullptr;
}

void LvglBackend::onClicked(lv_event_t* e) {
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
    // 这些对象随进程生命周期存在(仅供测试/宿主预览,不做回收)。
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
