// layout: 声明式布局描述类,用于描述界面中容器的层级与占位比例。
//
// 用法:
//   DropDown.layout(skiff::layout::vstack({
//       skiff::layout::grid(2, 2).sizePct(30),  // 2 行 2 列,占 30% 高度
//       skiff::layout::grid(2, 1).sizePct(70),  // 2 行 1 列,占 70% 高度
//   }));
//
// 规则:
//   - grid(rows, cols): 行数 x 列数的网格占位。
//   - hstack/vstack: 水平/垂直排列子占位。
//   - sizePct(w, h=100): 设置宽高百分比;只写一个时另一个默认 100。
//   - 在 stack 中,子占位的主轴百分比会被当作权重进行归一化分配。
#pragma once

#include <cstddef>
#include <initializer_list>
#include <vector>

namespace skiff {
namespace layout {

struct LayoutItem {
    enum Kind { HStack, VStack, Grid, Spacer } kind;

    int rows;      // Grid: 行数
    int cols;      // Grid: 列数
    int widthPct;  // 宽度百分比(在 stack 中用作主轴/交叉轴权重)
    int heightPct; // 高度百分比(在 stack 中用作主轴/交叉轴权重)

    std::vector<LayoutItem> children;

    LayoutItem()
        : kind(VStack), rows(0), cols(0), widthPct(100), heightPct(100) {}

    explicit LayoutItem(Kind k, int r = 0, int c = 0)
        : kind(k), rows(r), cols(c), widthPct(100), heightPct(100) {}

    // sizePct(w, h): 设置宽高百分比;省略 h 时默认 100。
    LayoutItem& sizePct(int w, int h = 100) {
        widthPct = w;
        heightPct = h;
        return *this;
    }
};

// 水平排列子占位
inline LayoutItem hstack(std::initializer_list<LayoutItem> items) {
    LayoutItem root(LayoutItem::HStack);
    root.children.assign(items);
    return root;
}

// 垂直排列子占位
inline LayoutItem vstack(std::initializer_list<LayoutItem> items) {
    LayoutItem root(LayoutItem::VStack);
    root.children.assign(items);
    return root;
}

// rows x cols 的网格占位
inline LayoutItem grid(int rows, int cols) {
    return LayoutItem(LayoutItem::Grid, rows, cols);
}

// 弹性占位:填充 stack 中的剩余空间
inline LayoutItem spacer() {
    return LayoutItem(LayoutItem::Spacer);
}

} // namespace layout
} // namespace skiff
