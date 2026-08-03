// DropDown:下拉菜单面板,支持按钮和滑条两种条目,数量由调用方决定。
// DropDownView 继承 ElementView,可通过 .build() 展开成通用 Element 树。
//
// 用法:
//   State<bool> expanded(false);
//   State<int> brightness(80);
//
//   Element trigger = Button("下拉", [&expanded] { expanded.set(!expanded.get()); });
//
//   DropDown menu({
//       {"Wi-Fi", [&router]{ router.push("Wi-Fi"); }},
//       {"亮度", 0, 100, brightness, [&brightness](int v){ brightness.set(v); }},
//       {"蓝牙", []{ ... }},
//   }, expanded)
//       .layout(skiff::layout::vstack({
//           skiff::layout::grid(2, 2).sizePct(30),  // 2 行 2 列
//           skiff::layout::grid(2, 1).sizePct(70),  // 2 行 1 列
//       }))
//       .as<ElementView>().width(240).itemHeight(48).bg(kTile).ttf(kFont, 16);
//
//   return VStack({trigger, menu}, 0).size(800, 480);
#pragma once

#include <algorithm>
#include <functional>
#include <string>
#include <vector>

#include "../element.hpp"
#include "../state.hpp"
#include "../layout.hpp"

namespace skiff {
namespace components {

struct DropDownItem {
    enum Kind { Button, Slider } kind;

    std::string label;

    // Button 用
    std::function<void()> onTap;

    // Slider 用
    int min, max;
    State<int>* sliderState;                 // 由调用方管理生命周期
    std::function<void(int)> onValueChange;

    DropDownItem()
        : kind(Button), min(0), max(0), sliderState(nullptr) {}

    // Button 简写: {"Wi-Fi", [&]{ ... }}
    DropDownItem(const std::string& text, std::function<void()> tap)
        : kind(Button), label(text), onTap(tap), min(0), max(0),
          sliderState(nullptr) {}

    // Slider 简写: {"亮度", 0, 100, brightness, [&](int v){ ... }}
    DropDownItem(const std::string& text, int minimum, int maximum,
                 State<int>& state, std::function<void(int)> cb)
        : kind(Slider), label(text), min(minimum), max(maximum),
          sliderState(&state), onValueChange(cb) {}
};

// DropDown 专用属性描述类
struct DropDownOptions {
    int gapPct_, marginPct_;

    DropDownOptions() : gapPct_(3), marginPct_(3) {}

    DropDownOptions& gapPct(int pct) { gapPct_ = pct; return *this; }
    DropDownOptions& marginPct(int pct) { marginPct_ = pct; return *this; }
};

class DropDownView : public ElementView {
public:
    DropDownView(const std::vector<DropDownItem>& items, State<bool>& expanded)
        : items_(items), expanded_(&expanded) {
        initDefaults_();
    }

    explicit DropDownView(State<bool>& expanded)
        : expanded_(&expanded) {
        initDefaults_();
    }

    // 批量设置条目:{{按钮组}, {滑条组}}
    DropDownView& adapter(std::initializer_list<std::initializer_list<DropDownItem> > sections) {
        items_.clear();
        for (std::initializer_list<std::initializer_list<DropDownItem> >::const_iterator sec = sections.begin();
             sec != sections.end(); ++sec) {
            for (std::initializer_list<DropDownItem>::const_iterator it = sec->begin();
                 it != sec->end(); ++it) {
                items_.push_back(*it);
            }
        }
        ensureDefaultLayout_();
        return *this;
    }

    DropDownView& layout(const skiff::layout::LayoutItem& l) {
        layout_ = l;
        customLayout_ = true;
        return *this;
    }

    DropDownView& itemHeight(int h) {
        e_->options.itemHeightPx = h; return *this;
    }

    // grid 单元之间的间距,按 Dropdown 宽度的百分比计算
    DropDownView& gapPct(int pct) {
        dropOpts_.gapPct_ = pct; return *this;
    }

    // grid 内容与边界之间的外边距,按 Dropdown 宽度的百分比计算
    DropDownView& marginPct(int pct) {
        dropOpts_.marginPct_ = pct; return *this;
    }

    Element build() const override {
        if (!expanded_->get()) {
            // 收起状态:高度为 0 的透明容器,不占用布局空间
            Element collapsed;
            collapsed.kind = Element::Column;
            collapsed.options.spacingPx = 0;
            collapsed.options.height = 0;
            collapsed.options.hasBg = false;
            return collapsed;
        }

        Element root = buildWithLayout_();
        const int totalRows = computeLayoutRows_(layout_);
        const int totalHeight = totalRows * e_->options.itemHeightPx;
        root.options.width = e_->options.width;
        root.options.height = totalHeight;
        return root;
    }

private:
    std::vector<DropDownItem> items_;
    State<bool>* expanded_;

    skiff::layout::LayoutItem layout_;
    DropDownOptions dropOpts_;
    bool customLayout_;

    void initDefaults_() {
        customLayout_ = false;
        e_->options.width = 240;
        e_->options.itemHeightPx = 48;
        e_->options.bgColor = 0x26303B;
        e_->options.hasBg = true;
        e_->options.fgColor = 0xFFFFFF;
        e_->options.hasFg = true;
        e_->options.fontPx = 16;
        ensureDefaultLayout_();
    }

    void ensureDefaultLayout_() {
        if (customLayout_) return;
        int buttonCount = 0;
        int sliderCount = 0;
        for (size_t i = 0; i < items_.size(); ++i) {
            if (items_[i].kind == DropDownItem::Button) {
                ++buttonCount;
            } else if (items_[i].sliderState != nullptr) {
                ++sliderCount;
            }
        }
        const int buttonRows = std::max(1, (buttonCount + 1) / 2);
        const int sliderRows = std::max(1, sliderCount);
        layout_ = skiff::layout::vstack({
            skiff::layout::grid(buttonRows, 2).sizePct(100, buttonRows),
            skiff::layout::grid(sliderRows, 1).sizePct(100, sliderRows)
        });
    }

    Element makeButton_(const DropDownItem& item) const {
        ElementView btn = skiff::Button(item.label, item.onTap)
            .bg(0x808080)
            .fg(0xFFFFFF)
            .radius(12)
            .centered()
            .applyOptions(elements::state::pressed(),
                          elements::attrOptions().bg(0x007AFF));
        if (!e_->options.ttfPath.empty()) btn.ttf(e_->options.ttfPath.c_str(), e_->options.fontPx);
        else btn.font(e_->options.fontPx);
        return btn.build();
    }

    Element makeSliderRow_(const DropDownItem& item) const {
        State<int>& st = *item.sliderState;
        std::function<void(int)> sliderCb = item.onValueChange;
        ElementView label = skiff::Text(item.label).fg(e_->options.fgColor);
        if (!e_->options.ttfPath.empty()) label.ttf(e_->options.ttfPath.c_str(), e_->options.fontPx);
        else label.font(e_->options.fontPx);

        return skiff::HStack({
                label.build(),
                skiff::Spacer(),
                skiff::Slider(st.get(), item.min, item.max,
                              [&st, sliderCb](int v) {
                                  st.set(v);
                                  if (sliderCb) sliderCb(v);
                              })
                    .size(180, 24),
            }, 8)
            .sizePct(100, 100)
            .padLeft(12).padRight(12).padBottom(12)
            .bg(e_->options.bgColor)
            .centered()
            .build();
    }

    void collectLeafGrids_(const skiff::layout::LayoutItem& item,
                           std::vector<const skiff::layout::LayoutItem*>& out) const {
        if (item.kind == skiff::layout::LayoutItem::Grid) {
            out.push_back(&item);
            return;
        }
        for (size_t i = 0; i < item.children.size(); ++i) {
            collectLeafGrids_(item.children[i], out);
        }
    }

    Element buildGridContent_(const skiff::layout::LayoutItem& grid,
                              const std::vector<Element>& items) const {
        const int gap = e_->options.width * dropOpts_.gapPct_ / 100;
        const int margin = e_->options.width * dropOpts_.marginPct_ / 100;
        Element col = skiff::VStack({}, gap).pad(margin).build();
        int idx = 0;
        const int cellWPct = grid.cols > 0 ? (100 / grid.cols) : 100;
        const int rowHPct = grid.rows > 0 ? (100 / grid.rows) : 100;
        for (int r = 0; r < grid.rows; ++r) {
            Element row = skiff::HStack({}, gap).sizePct(100, rowHPct).build();
            for (int c = 0; c < grid.cols; ++c) {
                if (idx < (int)items.size()) {
                    Element cell = items[idx++];
                    cell.options.widthPct = cellWPct;
                    cell.options.heightPct = 100;
                    row.children.push_back(cell);
                } else {
                    row.children.push_back(skiff::Spacer().build());
                }
            }
            if (!row.children.empty()) {
                col.children.push_back(row);
            }
        }
        return col;
    }

    int computeLayoutRows_(const skiff::layout::LayoutItem& item) const {
        if (item.kind == skiff::layout::LayoutItem::Grid) return item.rows;
        if (item.kind == skiff::layout::LayoutItem::HStack) {
            int maxRows = 0;
            for (size_t i = 0; i < item.children.size(); ++i) {
                maxRows = std::max(maxRows,
                                   computeLayoutRows_(item.children[i]));
            }
            return maxRows;
        }
        if (item.kind == skiff::layout::LayoutItem::VStack) {
            int sumRows = 0;
            for (size_t i = 0; i < item.children.size(); ++i) {
                sumRows += computeLayoutRows_(item.children[i]);
            }
            return sumRows;
        }
        return 0;
    }

    Element buildLayoutTree_(const skiff::layout::LayoutItem& item,
                             const std::vector<Element>& gridContents,
                             size_t& gridIdx) const {
        using namespace skiff::layout;

        if (item.kind == LayoutItem::Grid) {
            if (gridIdx < gridContents.size()) {
                return gridContents[gridIdx++];
            }
            return skiff::VStack({}).build();
        }

        if (item.kind == LayoutItem::Spacer) {
            return skiff::Spacer().build();
        }

        Element container;
        container.kind = (item.kind == LayoutItem::HStack) ? Element::Row : Element::Column;

        int total = 0;
        for (size_t i = 0; i < item.children.size(); ++i) {
            const LayoutItem& child = item.children[i];
            total += (item.kind == LayoutItem::HStack
                          ? child.widthPct : child.heightPct);
        }

        for (size_t i = 0; i < item.children.size(); ++i) {
            const LayoutItem& child = item.children[i];
            Element childEl = buildLayoutTree_(child, gridContents, gridIdx);

            if (total > 0) {
                int main = (item.kind == LayoutItem::HStack
                                ? child.widthPct : child.heightPct);
                int pct = main * 100 / total;
                if (item.kind == LayoutItem::HStack) {
                    childEl.options.widthPct = pct;
                    childEl.options.heightPct = child.heightPct;
                } else {
                    childEl.options.widthPct = child.widthPct;
                    childEl.options.heightPct = pct;
                }
            }
            container.children.push_back(childEl);
        }
        return container;
    }

    Element buildWithLayout_() const {
        using namespace skiff::layout;

        std::vector<const LayoutItem*> leafGrids;
        collectLeafGrids_(layout_, leafGrids);

        std::vector<Element> buttons;
        std::vector<Element> sliders;
        for (size_t i = 0; i < items_.size(); ++i) {
            if (items_[i].kind == DropDownItem::Button) {
                buttons.push_back(makeButton_(items_[i]));
            } else if (items_[i].sliderState != nullptr) {
                sliders.push_back(makeSliderRow_(items_[i]));
            }
        }

        std::vector<Element> gridContents;
        for (size_t i = 0; i < leafGrids.size(); ++i) {
            if (i == 0) {
                gridContents.push_back(buildGridContent_(*leafGrids[i], buttons));
            } else if (i == 1) {
                gridContents.push_back(buildGridContent_(*leafGrids[i], sliders));
            } else {
                gridContents.push_back(
                    buildGridContent_(*leafGrids[i], std::vector<Element>()));
            }
        }

        size_t gridIdx = 0;
        return buildLayoutTree_(layout_, gridContents, gridIdx);
    }
};

// 兼容旧写法的工厂函数
inline DropDownView DropDown(const std::vector<DropDownItem>& items, State<bool>& expanded) {
    return DropDownView(items, expanded);
}

inline DropDownView DropDown(State<bool>& expanded) {
    return DropDownView(expanded);
}

} // namespace components
} // namespace skiff
