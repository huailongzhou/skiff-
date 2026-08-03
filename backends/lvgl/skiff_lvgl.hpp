// skiff 的 LVGL 8 后端:把 Element 树映射为 lv_obj 控件树。
//
// 这是核心 Backend 接口的一种实现;只有这个目录里的代码包含 lvgl.h,
// 页面代码(只 include "skiff/skiff.hpp")不感知 LVGL 的存在。
#pragma once

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "skiff/backend.hpp"
#include "lvgl.h"

namespace skiff {
namespace lvgl {

class LvglBackend : public Backend {
public:
    // parent:挂载根节点,一般用 lv_scr_act()
    explicit LvglBackend(lv_obj_t* parent);
    ~LvglBackend() override;

    void mount(const Element& root) override;

private:
    // 一棵已挂载的节点:Element 描述 + lv_obj + 子节点
    struct MountedNode {
        Element element;
        lv_obj_t* obj;
        std::vector<MountedNode> children;
        // 该节点注册的事件回调(节点销毁时随 lv_obj 一起释放)
        std::vector<std::unique_ptr<std::function<void()> > > callbacks;
    };

    struct NewAnim {
        enum Dir { Right, Down } dir;
        lv_obj_t* obj;
        lv_obj_t* parent;
    };

    void clearNode(MountedNode& node);
    lv_obj_t* buildNode(const Element& e, lv_obj_t* parent, MountedNode* out);
    void updateNode(MountedNode& old, const Element& newE);
    void updateContainerStyle(lv_obj_t* obj, const Element& oldE, const Element& newE);
    void diffChildren(MountedNode& parentNode, const std::vector<Element>& newChildren);
    static MountedNode* findMatch(std::vector<MountedNode>& oldChildren,
                                  const Element& newChild, size_t preferredIdx,
                                  std::vector<bool>& used);
    void applyTextStyle(lv_obj_t* label, const Element& e);
    void applyTabBarStyle(lv_obj_t* tabview, const Element& e);
    lv_font_t* getFtFont(const std::string& path, int px);

    static void onClicked(lv_event_t* e);
    static void onSliderChanged(lv_event_t* e);
    static void animSetX(void* obj, int32_t v);
    static void animSetY(void* obj, int32_t v);
    void playNewAnimations();

    lv_obj_t* parent_;
    MountedNode root_;
    std::vector<NewAnim> newAnims_;
    // TTF 字体缓存:"path@px" -> font,后端存活期间复用
    std::map<std::string, lv_font_t*> ftFonts_;
};

// 确保 FreeType 已初始化(幂等)。后端用到 .ttf() 时会自动调,一般无需手动调用。
bool ensureFreetype();

// 注册一块"无显示"的假屏幕(flush 直接丢弃),用于宿主机冒烟测试/开发预览。
// 嵌入式实机不要用这个,接自己的真实显示驱动。
lv_disp_t* createHeadlessDisplay(int horRes, int verRes);

} // namespace lvgl
} // namespace skiff
