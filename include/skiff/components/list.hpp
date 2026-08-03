// List:垂直列表组件,用于文件列表、设置项等场景。
//
// 用法:
//   List({
//       {"item1", "subtitle", [] {}},
//       {"item2", "",       [] {}},
//   })
//   .itemHeight(56)
//   .ttf(kFont, 16)
//   .bg(kTile);
#pragma once

#include <string>
#include <vector>

#include "../element.hpp"

namespace skiff {
namespace components {

using ListItem = elements::ListItem;

class ListView : public ElementView {
public:
    ListView(const std::vector<ListItem>& items) {
        e_->kind = Element::Column;  // build() 也返回 Column,这里先标记
        e_->options.itemHeightPx = 56;
        e_->options.fontPx = 16;
        e_->options.bgColor = 0x1A222B;   // List 容器背景
        e_->options.hasBg = true;
        listOptions_.items = items;
    }

    ListView& itemHeight(int h) {
        e_->options.itemHeightPx = h;
        return *this;
    }

    ListView& subFg(uint32_t c) {
        subFg_ = c;
        return *this;
    }

    ListView& rowBg(uint32_t c) {
        rowBg_ = c;
        return *this;
    }

    Element build() const override {
        Element col;
        col.kind = Element::Column;
        col.options = e_->options;
        col.options.spacingPx = 0;

        for (size_t i = 0; i < listOptions_.items.size(); ++i) {
            const elements::ListItem& item = listOptions_.items[i];
            std::vector<Element> rowChildren;

            Element titleLabel;
            titleLabel.kind = Element::Text;
            titleLabel.text = item.title;
            titleLabel.options.fontPx = e_->options.fontPx;
            titleLabel.options.fgColor = e_->options.hasFg ? e_->options.fgColor : 0xFFFFFF;
            titleLabel.options.hasFg = true;
            if (!e_->options.ttfPath.empty()) {
                titleLabel.options.ttfPath = e_->options.ttfPath;
            }
            rowChildren.push_back(titleLabel);

            if (!item.subtitle.empty()) {
                Element subLabel;
                subLabel.kind = Element::Text;
                subLabel.text = item.subtitle;
                subLabel.options.fontPx = e_->options.fontPx - 2;
                subLabel.options.fgColor = subFg_;
                subLabel.options.hasFg = true;
                if (!e_->options.ttfPath.empty()) {
                    subLabel.options.ttfPath = e_->options.ttfPath;
                }
                rowChildren.push_back(subLabel);
            }

            Element content;
            content.kind = Element::Column;
            content.children = std::move(rowChildren);
            content.options.spacingPx = 2;
            content.options.widthPct = 100;
            content.options.height = e_->options.itemHeightPx - 12;
            content.options.paddingLeft = 16;
            content.options.center = true;

            Element row;
            row.kind = Element::Button;
            row.children.push_back(content);
            row.onTap = item.onTap;
            row.options.center = true;
            row.options.widthPct = 100;
            row.options.height = e_->options.itemHeightPx;
            row.options.bgColor = rowBg_;
            row.options.hasBg = true;

            col.children.push_back(row);
        }
        return col;
    }

private:
    elements::listOptions listOptions_;
    uint32_t subFg_ = 0x9AA4B0;
    uint32_t rowBg_ = 0x26303B;
};

// 兼容旧写法的工厂函数
inline ListView List(const std::vector<ListItem>& items) {
    return ListView(items);
}

} // namespace components
} // namespace skiff
