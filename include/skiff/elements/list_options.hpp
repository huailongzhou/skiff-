// listOptions:List 组件的专用属性。
#pragma once

#include <functional>
#include <string>
#include <vector>

namespace skiff {
namespace elements {

struct ListItem {
    std::string title;       // 主标题
    std::string subtitle;    // 副标题(可选)
    std::function<void()> onTap; // 点击回调
    std::string key;         // 列表项 key,用于 diff 时稳定复用

    ListItem() {}
    ListItem(const std::string& t, std::function<void()> tap)
        : title(t), onTap(std::move(tap)) {}
    ListItem(const std::string& t, const std::string& sub, std::function<void()> tap)
        : title(t), subtitle(sub), onTap(std::move(tap)) {}
    ListItem(const std::string& t, const std::string& sub, const std::string& k,
             std::function<void()> tap)
        : title(t), subtitle(sub), onTap(std::move(tap)), key(k) {}
};

struct listOptions {
    std::vector<ListItem> items;
    uint32_t rowBgColor;
    uint32_t subFgColor;
    uint32_t dividerColor;
    bool hasDivider;
    int dividerWidth;

    listOptions()
        : rowBgColor(0x26303B),
          subFgColor(0x9AA4B0),
          dividerColor(0xFFFFFF),
          hasDivider(true),
          dividerWidth(1) {}
};

} // namespace elements
} // namespace skiff
