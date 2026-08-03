// DropDown:下拉菜单面板,支持按钮和滑条两种条目,数量由调用方决定。
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
//       .width(240).itemHeight(48).bg(kTile).ttf(kFont, 16);
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

class DropDown : public Element {
public:
    DropDown(const std::vector<DropDownItem>& items, State<bool>& expanded)
        : items_(items), expanded_(&expanded) {
        initDefaults_();
    }

    // 无初始条目的构造,条目通过 .adapter({{按钮...}, {滑条...}}) 后续设置
    explicit DropDown(State<bool>& expanded)
        : expanded_(&expanded) {
        initDefaults_();
    }

    // 批量设置条目:{{按钮组}, {滑条组}}
    DropDown& adapter(std::initializer_list<std::initializer_list<DropDownItem> > sections) {
        items_.clear();
        for (std::initializer_list<std::initializer_list<DropDownItem> >::const_iterator sec = sections.begin();
             sec != sections.end(); ++sec) {
            for (std::initializer_list<DropDownItem>::const_iterator it = sec->begin();
                 it != sec->end(); ++it) {
                items_.push_back(*it);
            }
        }
        ensureDefaultLayout_();
        rebuild();
        return *this;
    }

    DropDown& layout(const skiff::layout::LayoutItem& l) {
        layout_ = l;
        customLayout_ = true;
        rebuild();
        return *this;
    }

    DropDown& width(int w) {
        options.width = w; rebuild(); return *this;
    }

    DropDown& itemHeight(int h) {
        options.itemHeightPx = h; rebuild(); return *this;
    }

    DropDown& bg(uint32_t c) {
        options.bgColor = c;
        options.hasBg = true;
        rebuild();
        return *this;
    }

    DropDown& fg(uint32_t c) {
        options.fgColor = c;
        options.hasFg = true;
        rebuild();
        return *this;
    }

    DropDown& font(int px) {
        options.ttfPath.clear();
        options.fontPx = px;
        rebuild();
        return *this;
    }

    DropDown& ttf(const char* path, int px) {
        options.ttfPath = path;
        options.fontPx = px;
        rebuild();
        return *this;
    }

    // grid 单元之间的间距,按 Dropdown 宽度的百分比计算
    DropDown& gapPct(int pct) {
        dropOpts_.gapPct_ = pct; rebuild(); return *this;
    }

    // grid 内容与边界之间的外边距,按 Dropdown 宽度的百分比计算
    DropDown& marginPct(int pct) {
        dropOpts_.marginPct_ = pct; rebuild(); return *this;
    }

    // 下拉菜单在父容器中右对齐(依赖 Element 的 hAlign)
    DropDown& alignRight() {
        options.hAlign = elements::HAlignEnd;
        rebuild();
        return *this;
    }

private:
    std::vector<DropDownItem> items_;
    State<bool>* expanded_;

    skiff::layout::LayoutItem layout_;
    DropDownOptions dropOpts_;
    bool customLayout_;

    void initDefaults_() {
        customLayout_ = false;
        // 默认样式通过 Element::options 统一保存
        options.width = 240;
        options.itemHeightPx = 48;
        options.bgColor = 0x26303B;
        options.hasBg = true;
        options.fgColor = 0xFFFFFF;
        options.hasFg = true;
        options.fontPx = 16;
        ensureDefaultLayout_();
        rebuild();
    }

    // 仅在用户未自定义 layout 时根据 items 重建默认布局
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
        // 下拉菜单按钮默认:灰色背景、白色文字、圆角、按下蓝色背景
        Element btn = skiff::Button(item.label, item.onTap)
            .bg(0x808080)
            .fg(0xFFFFFF)
            .radius(12)
            .centered()
            .applyOptions(elements::state::pressed(),
                          elements::attrOptions().bg(0x007AFF));
        if (!options.ttfPath.empty()) btn = btn.ttf(options.ttfPath.c_str(), options.fontPx);
        else btn = btn.font(options.fontPx);
        return btn;
    }

    Element makeSliderRow_(const DropDownItem& item) const {
        State<int>& st = *item.sliderState;
        std::function<void(int)> sliderCb = item.onValueChange;
        Element label = skiff::Text(item.label).fg(options.fgColor);
        if (!options.ttfPath.empty()) label = label.ttf(options.ttfPath.c_str(), options.fontPx);
        else label = label.font(options.fontPx);

        return skiff::HStack({
                label,
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
            .bg(options.bgColor)
            .centered();
    }

    // 收集 layout 树中的所有叶子 grid
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

    // 根据 grid 行列数把 items 排成网格,每个单元占 (100/cols)% 宽度、100% 高度
    Element buildGridContent_(const skiff::layout::LayoutItem& grid,
                              const std::vector<Element>& items) const {
        const int gap = options.width * dropOpts_.gapPct_ / 100;
        const int margin = options.width * dropOpts_.marginPct_ / 100;
        Element col = VStack({}, gap).pad(margin);
        int idx = 0;
        const int cellWPct = grid.cols > 0 ? (100 / grid.cols) : 100;
        const int rowHPct = grid.rows > 0 ? (100 / grid.rows) : 100;
        for (int r = 0; r < grid.rows; ++r) {
            Element row = HStack({}, gap).sizePct(100, rowHPct);
            for (int c = 0; c < grid.cols; ++c) {
                if (idx < (int)items.size()) {
                    row.children.push_back(
                        items[idx++].sizePct(cellWPct, 100));
                } else {
                    row.children.push_back(skiff::Spacer());
                }
            }
            if (!row.children.empty()) {
                col.children.push_back(row);
            }
        }
        return col;
    }

    // 估算 layout 展开后占用的总行数(hstack 取最大,vstack 取和)
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

    // 递归把 layout 描述转换成 Element 树
    Element buildLayoutTree_(const skiff::layout::LayoutItem& item,
                             const std::vector<Element>& gridContents,
                             size_t& gridIdx) const {
        using namespace skiff::layout;

        if (item.kind == LayoutItem::Grid) {
            if (gridIdx < gridContents.size()) {
                return gridContents[gridIdx++];
            }
            return VStack({});
        }

        if (item.kind == LayoutItem::Spacer) {
            return skiff::Spacer();
        }

        Element container = (item.kind == LayoutItem::HStack)
                                ? HStack({})
                                : VStack({});

        // 计算主轴方向总权重
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
                    childEl = childEl.sizePct(pct, child.heightPct);
                } else {
                    childEl = childEl.sizePct(child.widthPct, pct);
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

        // 把条目按类型分开
        std::vector<Element> buttons;
        std::vector<Element> sliders;
        for (size_t i = 0; i < items_.size(); ++i) {
            if (items_[i].kind == DropDownItem::Button) {
                buttons.push_back(makeButton_(items_[i]));
            } else if (items_[i].sliderState != nullptr) {
                sliders.push_back(makeSliderRow_(items_[i]));
            }
        }

        // 叶子 grid 按顺序填充:第 0 个放按钮,第 1 个放滑条,其余留空
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

    void rebuild() {
        if (!expanded_->get()) {
            // 收起状态:高度为 0 的透明容器,不占用布局空间
            kind = Column;
            options.spacingPx = 0;
            options.height = 0;
            options.hasBg = false;
            children.clear();
            return;
        }

        Element root = buildWithLayout_();
        const int totalRows = computeLayoutRows_(layout_);
        const int totalHeight = totalRows * options.itemHeightPx;

        // 保存 options,因为赋值 root 会覆盖它
        const elements::attrOptions savedOptions = options;
        static_cast<Element&>(*this) = root.size(options.width, totalHeight);
        options = savedOptions;
        options.height = totalHeight;
        options.hasBg = true;
    }
};

} // namespace components
} // namespace skiff
