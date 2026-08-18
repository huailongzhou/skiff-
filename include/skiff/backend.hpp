// Backend:把 Element 树挂载成原生控件树的后端接口。
//
// LVGL 只是其中一种实现;以后做 PC 端预览后端(SDL / Qt / imgui ...),
// 实现同一套钩子即可,页面代码不用改。
//
// 本类同时提供后端无关的 diff 算法:子类只需实现 create/update/destroy/move
// 等原生操作,节点复用、子节点增删、key 匹配等逻辑由基类统一完成。
#pragma once

#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <vector>

#include "watchable.hpp"
#include "element.hpp"
#include "slot_host.hpp"
#include "state.hpp"

namespace skiff {

class Backend {
public:
    virtual ~Backend() {}

    // 用一棵新的 Element 树重建/更新整个原生控件树。
    // 通用流程:首次 build,后续 diff,最后调用子类的 afterMount()。
    void mount(const Element& root);

    // 按 key 只更新一棵已挂载子树。找不到对应节点时返回 false(例如该页未显示)。
    bool patch(const std::string& key, const Element& e);

protected:
    explicit Backend(void* root_parent);

    // 一棵已挂载的节点:Element 描述 + 原生图形对象 + 子节点 + 回调。
    // graphic_obj 是子类原生对象指针(LVGL 中是 lv_obj_t*),对基类不透明。
    struct MountedNode {
        Element element;
        void* graphic_obj;
        std::vector<MountedNode> children;
        // 该节点注册的事件回调(节点销毁时随原生对象一起释放)
        std::vector<std::unique_ptr<std::function<void()> > > callbacks;
    };

    // ---------- 子类必须实现的原生操作钩子 ----------

    // 根据 Element 创建原生对象,挂到 parent 下。
    // node 用于存储该节点的回调,子类把事件回调 push 到 node->callbacks。
    virtual void* createGraphicObject(const Element& e, void* parent,
                                      MountedNode* node) = 0;

    // 同 kind 节点就地的属性更新(不递归子节点)。
    virtual void updateGraphicObject(MountedNode& node, const Element& new_e) = 0;

    // 销毁一个原生对象(可为 nullptr)。
    virtual void destroyGraphicObject(void* obj) = 0;

    // 返回原生对象的父对象(用于重建时找回挂载点)。
    virtual void* getGraphicParent(void* obj) = 0;

    // 把子对象移动到父对象的第 index 个位置。
    virtual void moveGraphicChild(void* obj, void* parent, int index) = 0;

    // 返回 e 的第 child_index 个子节点应该挂载到哪个原生对象下。
    // 普通容器返回 obj 本身;TabView 等需要返回对应的 page。
    virtual void* getChildParent(void* obj, const Element& e,
                                 size_t child_index) = 0;

    // 判断 old 节点是否需要整子树重建(而非就地更新)。
    virtual bool needsRebuild(const MountedNode& old, const Element& new_e) = 0;

    // mount 流程结束后调用,默认空。LVGL 后端用它做 layout + 动画。
    virtual void afterMount() {}

    // ---------- 基类提供的通用 diff 算法 ----------

    void buildNode(const Element& e, void* parent, MountedNode* out);
    void updateNode(MountedNode& node, const Element& new_e);
    void diffChildren(MountedNode& node, const std::vector<Element>& new_children);
    MountedNode* findMatch(std::vector<MountedNode>& old_children,
                           const Element& new_child, size_t preferred_idx,
                           std::vector<bool>& used);
    MountedNode* findNodeByKey(MountedNode& node, const std::string& key);
    void clearNode(MountedNode& node);

    // Button 纯文本时,把 text 合成一个 Text 子节点,避免 diff 时把旧 Text 误判删除。
    static std::vector<Element> getChildElements(const Element& e);
    static bool isDiffableContainer(const Element& e);

    void* root_parent_;
    MountedNode root_;
};

inline Backend::Backend(void* root_parent) : root_parent_(root_parent) {
    root_.graphic_obj = nullptr;
}

inline void Backend::mount(const Element& root) {
    if (root_.graphic_obj == nullptr) {
        buildNode(root, root_parent_, &root_);
    } else {
        updateNode(root_, root);
    }
    afterMount();
}

inline Backend::MountedNode* Backend::findNodeByKey(MountedNode& node,
                                                    const std::string& key) {
    if (!key.empty() && node.element.options.keyId == key) return &node;
    for (size_t i = 0; i < node.children.size(); ++i) {
        MountedNode* found = findNodeByKey(node.children[i], key);
        if (found) return found;
    }
    return nullptr;
}

inline bool Backend::patch(const std::string& key, const Element& e) {
    if (key.empty() || root_.graphic_obj == nullptr) return false;
    MountedNode* n = findNodeByKey(root_, key);
    if (!n) return false;
    updateNode(*n, e);
    afterMount();
    return true;
}

inline void Backend::buildNode(const Element& e, void* parent, MountedNode* out) {
    if (!out) return;
    out->element = e;
    out->graphic_obj = createGraphicObject(e, parent, out);
    out->children.clear();

    std::vector<Element> childElements;
    if (isDiffableContainer(e) || e.kind == Element::TabView) {
        childElements = getChildElements(e);
    }

    for (size_t i = 0; i < childElements.size(); ++i) {
        void* child_parent = getChildParent(out->graphic_obj, e, i);
        out->children.push_back(MountedNode());
        buildNode(childElements[i], child_parent, &out->children.back());
    }
}

inline void Backend::updateNode(MountedNode& node, const Element& e) {
    if (needsRebuild(node, e)) {
        void* parent = node.graphic_obj ? getGraphicParent(node.graphic_obj) : nullptr;
        clearNode(node);
        node.element = e;
        buildNode(e, parent, &node);
        return;
    }

    updateGraphicObject(node, e);

    if (isDiffableContainer(e)) {
        diffChildren(node, getChildElements(e));
    } else if (e.kind == Element::TabView) {
        for (size_t i = 0; i < e.children.size(); ++i) {
            updateNode(node.children[i], e.children[i]);
        }
    }

    node.element = e;
}

inline void Backend::diffChildren(MountedNode& parentNode,
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
            if (node.graphic_obj) {
                moveGraphicChild(node.graphic_obj, parentNode.graphic_obj, (int)i);
            }
            updateNode(node, newChildren[i]);
            nextChildren.push_back(std::move(node));
        } else {
            MountedNode newNode;
            newNode.element = newChildren[i];
            void* child_parent = getChildParent(parentNode.graphic_obj, parentNode.element, i);
            buildNode(newChildren[i], child_parent, &newNode);
            if (newNode.graphic_obj) {
                moveGraphicChild(newNode.graphic_obj, parentNode.graphic_obj, (int)i);
            }
            nextChildren.push_back(std::move(newNode));
        }
    }

    for (size_t j = 0; j < oldChildren.size(); ++j) {
        if (!used[j]) clearNode(oldChildren[j]);
    }

    parentNode.children = std::move(nextChildren);
}

inline Backend::MountedNode* Backend::findMatch(
    std::vector<MountedNode>& old_children, const Element& new_child,
    size_t preferred_idx, std::vector<bool>& used) {
    // 1. key 匹配:有 key 的节点只按 key 复用
    if (!new_child.options.keyId.empty()) {
        for (size_t j = 0; j < old_children.size(); ++j) {
            if (!used[j] &&
                old_children[j].element.options.keyId == new_child.options.keyId) {
                return &old_children[j];
            }
        }
        return nullptr;
    }

    // 2. 同位置同类型匹配(仅对无 key 节点)
    if (preferred_idx < old_children.size() && !used[preferred_idx] &&
        old_children[preferred_idx].element.kind == new_child.kind &&
        old_children[preferred_idx].element.options.keyId.empty()) {
        return &old_children[preferred_idx];
    }

    // 3. 任意同类型匹配(仅对无 key 节点)
    for (size_t j = 0; j < old_children.size(); ++j) {
        if (!used[j] && old_children[j].element.options.keyId.empty() &&
            old_children[j].element.kind == new_child.kind) {
            return &old_children[j];
        }
    }
    return nullptr;
}

inline void Backend::clearNode(MountedNode& node) {
    for (size_t i = 0; i < node.children.size(); ++i) {
        clearNode(node.children[i]);
    }
    node.children.clear();
    destroyGraphicObject(node.graphic_obj);
    node.callbacks.clear();
    node.graphic_obj = nullptr;
}

inline std::vector<Element> Backend::getChildElements(const Element& e) {
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

inline bool Backend::isDiffableContainer(const Element& e) {
    return e.kind == Element::Column || e.kind == Element::Row ||
           e.kind == Element::Button;
}

class App {
public:
    App(Backend& backend, std::function<Element()> body)
        : backend_(backend), body_(std::move(body)), dirty_(true),
          localDirty_(false), updating_(false) {}

    // 每帧在 mount/patch 之前调用(物理步进、动画时钟等)。
    // 回调里可以 set State,随后本次 update() 会消化 dirty。
    void setTick(std::function<void()> fn) { tick_ = std::move(fn); }

    ~App() {
        for (size_t i = 0; i < watchables_.size(); ++i) {
            watchables_[i]->setInvalidator(std::function<void()>());
            watchables_[i]->setUnregister(std::function<void()>());
        }
    }

    // 绑定一个状态:默认整页失效(重跑 body + mount)。
    // 若该 State 已 watchLocal,则改为只 patch 对应 WatchView。
    template <typename T>
    void bind(State<T>& s) {
        const void* id = static_cast<const void*>(&s);
        s.subscribe([this, id] {
            if (isLocalOnly(id)) invalidateLocal();
            else invalidate();
        });
    }

    // 把 WatchView 登记为局部订阅:该 State 变化时不重跑 body(),
    // 只按 watchKey 更新已挂载子树。须在 start() 之前调用。
    void watchLocal(Watchable& w) {
        for (size_t i = 0; i < watchables_.size(); ++i) {
            if (watchables_[i] == &w) return;
        }
        watchables_.push_back(&w);
        for (size_t i = 0; i < w.watchedStateCount(); ++i) {
            ++localOnlyCount_[w.watchedStateAt(i)];
        }
        w.setInvalidator([this] { invalidateLocal(); });
        Watchable* ptr = &w;
        w.setUnregister([this, ptr] { removeWatchable(ptr); });
        if (SlotHost* nested = w.nestedSlots()) nested->attach(*this);
    }

    // 首次挂载。
    void start() { update(); }

    // 标记整页待刷新。真正的重建延迟到 update(),
    // 避免在后端的事件回调里边派发事件边删控件。
    void invalidate() { dirty_ = true; }

    void invalidateLocal() { localDirty_ = true; }

    // 主循环里定期调用。根失效时重跑 body();仅局部失效时 patch WatchView。
    void update() {
        if (updating_) return;
        if (tick_) tick_();
        updating_ = true;
        if (dirty_) {
            dirty_ = false;
            localDirty_ = false;
            for (size_t i = 0; i < watchables_.size(); ++i) {
                watchables_[i]->clearCache();
            }
            backend_.mount(body_());
        } else if (localDirty_) {
            localDirty_ = false;
            for (size_t i = 0; i < watchables_.size(); ++i) {
                Watchable* w = watchables_[i];
                if (!w->isDirty()) continue;
                backend_.patch(w->watchKey(), w->rebuild());
            }
        }
        updating_ = false;
    }

private:
    bool isLocalOnly(const void* id) const {
        std::map<const void*, int>::const_iterator it = localOnlyCount_.find(id);
        return it != localOnlyCount_.end() && it->second > 0;
    }

    void removeWatchable(Watchable* w) {
        if (!w) return;
        for (size_t i = 0; i < watchables_.size(); ++i) {
            if (watchables_[i] == w) {
                watchables_.erase(watchables_.begin() + static_cast<std::ptrdiff_t>(i));
                break;
            }
        }
        for (size_t i = 0; i < w->watchedStateCount(); ++i) {
            const void* id = w->watchedStateAt(i);
            std::map<const void*, int>::iterator it = localOnlyCount_.find(id);
            if (it != localOnlyCount_.end()) {
                --it->second;
                if (it->second <= 0) localOnlyCount_.erase(it);
            }
        }
    }

    Backend& backend_;
    std::function<Element()> body_;
    std::function<void()> tick_;
    bool dirty_;
    bool localDirty_;
    bool updating_;
    std::vector<Watchable*> watchables_;
    std::map<const void*, int> localOnlyCount_;
};

inline void SlotHost::attach(App& app) {
    app_ = &app;
    for (size_t i = 0; i < ordered_.size(); ++i) app.watchLocal(*ordered_[i]);
    for (size_t i = 0; i < named_.size(); ++i) app.watchLocal(*named_[i]);
}

inline void SlotHost::clearCaches() {
    for (size_t i = 0; i < ordered_.size(); ++i) ordered_[i]->clearCache();
    for (size_t i = 0; i < named_.size(); ++i) named_[i]->clearCache();
}

} // namespace skiff
