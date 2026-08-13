// AppGrid:手机桌面风格的网格应用入口。
// 支持多页排列,可配置行列数、滑动方向,内容超出时自动分页并可滑动切换。
// AppGridView 继承 ElementView,可通过 .build() 展开成通用 Element 树。
//
// 用法:
//   AppGrid(apps)
//       .cols(4).rows(3)
//       .horizontal()
//       .pageSize(w, h)   // 可选;不设则按 .size() / 占满父容器
//       .iconSize(40)
//       .labelSize(16)
//       .ttf(kFont, 16)
//       .sizePct(100, 100)
#pragma once

#include <functional>
#include <string>
#include <vector>

#include "../element.hpp"

namespace skiff {
namespace components {

struct AppIcon {
    const char* icon;                // 图标:可以是 LVGL 符号字体或任意 UTF-8 文本
    std::string label;               // 应用名称(支持 SKIFF_TR 结果)
    std::function<void()> onTap;     // 点击回调
};

// AppGrid 专用属性描述类
struct AppGridOptions {
    int cols_, rows_;          // 每页列数/行数
    bool horizontal_;          // 水平分页还是垂直分页
    int pageW_, pageH_;        // 单页尺寸
    int hSpacing_, vSpacing_;  // 单元水平/垂直间距
    int iconSize_, labelSize_; // 图标与文字字号
    uint32_t iconColor_, labelColor_; // 图标与文字颜色

    AppGridOptions()
        : cols_(4), rows_(3), horizontal_(true),
          pageW_(0), pageH_(0),
          hSpacing_(16), vSpacing_(24),
          iconSize_(40), labelSize_(16),
          iconColor_(0xFFFFFF), labelColor_(0xFFFFFF) {}

    AppGridOptions& cols(int c) { cols_ = c; return *this; }
    AppGridOptions& rows(int r) { rows_ = r; return *this; }
    AppGridOptions& horizontal(bool h = true) { horizontal_ = h; return *this; }
    AppGridOptions& vertical(bool v = true) { horizontal_ = !v; return *this; }
    AppGridOptions& pageSize(int w, int h) { pageW_ = w; pageH_ = h; return *this; }
    AppGridOptions& spacing(int h, int v) { hSpacing_ = h; vSpacing_ = v; return *this; }
    AppGridOptions& iconSize(int px) { iconSize_ = px; return *this; }
    AppGridOptions& labelSize(int px) { labelSize_ = px; return *this; }
    AppGridOptions& iconColor(uint32_t c) { iconColor_ = c; return *this; }
    AppGridOptions& labelColor(uint32_t c) { labelColor_ = c; return *this; }
};

class AppGridView : public ElementView {
public:
    AppGridView(const std::vector<AppIcon>& apps)
        : apps_(apps) {
        e_->options.paddingPx = 16;
        e_->options.fontPx = 16;
    }

    // AppGrid 专属配置
    AppGridView& cols(int c)       { gridOpts_.cols_ = c; return *this; }
    AppGridView& rows(int r)       { gridOpts_.rows_ = r; return *this; }
    AppGridView& horizontal()      { gridOpts_.horizontal_ = true;  return *this; }
    AppGridView& vertical()        { gridOpts_.horizontal_ = false; return *this; }
    AppGridView& pageSize(int w, int h) { gridOpts_.pageSize(w, h); return *this; }
    AppGridView& spacing(int h, int v) { gridOpts_.spacing(h, v); return *this; }
    AppGridView& iconSize(int px)  { gridOpts_.iconSize_ = px;  return *this; }
    AppGridView& labelSize(int px) { gridOpts_.labelSize_ = px; return *this; }
    AppGridView& iconColor(uint32_t c)  { gridOpts_.iconColor_ = c;  return *this; }
    AppGridView& labelColor(uint32_t c) { gridOpts_.labelColor_ = c; return *this; }

    Element build() const override {
        const int perPage = gridOpts_.cols_ * gridOpts_.rows_;
        if (perPage <= 0) {
            Element empty;
            empty.kind = Element::Column;
            empty.options.spacingPx = 0;
            return empty;
        }

        const int pageCount =
            (static_cast<int>(apps_.size()) + perPage - 1) / perPage;

        const int padding = e_->options.paddingPx;
        const int pageW = gridOpts_.pageW_ > 0 ? gridOpts_.pageW_ : e_->options.width;
        const int pageH = gridOpts_.pageH_ > 0 ? gridOpts_.pageH_ : e_->options.height;
        const bool usePx = pageW > 0 && pageH > 0;
        const int hSpacing = gridOpts_.hSpacing_;
        const int vSpacing = gridOpts_.vSpacing_;

        const int availW = usePx ? pageW - 2 * padding : 0;
        const int availH = usePx ? pageH - 2 * padding : 0;
        const int cellW = (usePx && gridOpts_.cols_ > 0)
                              ? (availW - (gridOpts_.cols_ - 1) * hSpacing) / gridOpts_.cols_
                              : availW;
        const int cellH = (usePx && gridOpts_.rows_ > 0)
                              ? (availH - (gridOpts_.rows_ - 1) * vSpacing) / gridOpts_.rows_
                              : availH;

        std::vector<Element> pages;
        for (int p = 0; p < pageCount; ++p) {
            std::vector<Element> pageRows;
            for (int r = 0; r < gridOpts_.rows_; ++r) {
                std::vector<Element> rowItems;
                for (int c = 0; c < gridOpts_.cols_; ++c) {
                    const int idx = p * perPage + r * gridOpts_.cols_ + c;
                    if (idx >= static_cast<int>(apps_.size())) break;
                    const AppIcon& app = apps_[idx];
                    Element labelText;
                    labelText.kind = Element::Text;
                    labelText.text = app.label;
                    labelText.options.fontPx = gridOpts_.labelSize_;
                    labelText.options.fgColor = gridOpts_.labelColor_;
                    labelText.options.hasFg = true;
                    if (!e_->options.ttfPath.empty()) {
                        labelText.options.ttfPath = e_->options.ttfPath;
                    }

                    Element iconText;
                    iconText.kind = Element::Text;
                    iconText.text = app.icon;
                    iconText.options.fontPx = gridOpts_.iconSize_;
                    iconText.options.fgColor = gridOpts_.iconColor_;
                    iconText.options.hasFg = true;
                    if (!e_->options.ttfPath.empty()) {
                        iconText.options.ttfPath = e_->options.ttfPath;
                    }

                    ElementView item = skiff::Button({iconText, labelText}, app.onTap)
                        .centered();
                    if (usePx) item.size(cellW, cellH);
                    else item.expand();
                    rowItems.push_back(item.build());
                }
                if (!rowItems.empty()) {
                    ElementView row = skiff::HStack(rowItems, hSpacing);
                    if (usePx) row.size(availW, cellH);
                    else row.sizePct(100, 0).expand();
                    pageRows.push_back(row.build());
                }
            }

            ElementView page = skiff::VStack(pageRows, vSpacing).pad(padding);
            if (usePx) page.size(pageW, pageH);
            else page.sizePct(100, 100);
            pages.push_back(page.build());
        }

        Element root;
        root.kind = gridOpts_.horizontal_ ? Element::Row : Element::Column;
        root.options = e_->options;
        root.options.spacingPx = 0;
        if (usePx) {
            root.options.width = pageW;
            root.options.height = pageH;
        } else {
            if (root.options.width <= 0 && root.options.widthPct <= 0)
                root.options.widthPct = 100;
            if (root.options.height <= 0 && root.options.heightPct <= 0)
                root.options.heightPct = 100;
        }
        root.options.scrollDir = gridOpts_.horizontal_ ? ScrollHorizontal : ScrollVertical;
        root.options.scrollSnap = SnapStart;
        root.children = pages;
        return root;
    }

private:
    std::vector<AppIcon> apps_;
    AppGridOptions gridOpts_;
};

// 兼容旧写法的工厂函数
inline AppGridView AppGrid(const std::vector<AppIcon>& apps) {
    return AppGridView(apps);
}

} // namespace components
} // namespace skiff
