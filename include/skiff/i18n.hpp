// 多国语言框架层:locale + 目录注册 + 按下标查找。
// 不含业务文案;应用自行定义枚举 key 并 registerCatalog。
//
// 用法(应用侧):
//   enum { hello = 0, bye, COUNT };
//   static std::string zh[COUNT] = {"你好", "再见"};
//   skiff::i18n::registerCatalog("zh-CN", zh, COUNT);
//   skiff::i18n::setLocale("zh-CN");
//   skiff::Text(SKIFF_TR(hello));   // 宏:任意整型/枚举 key
//
// 返回目录内稳定引用(locale 切换前指针有效)。
// 切换语言后应触发 UI 重建(如绑定 State locale)。
#pragma once

#include <map>
#include <string>

namespace skiff {
namespace i18n {

namespace detail {

struct CatalogSlot {
    const std::string* data;
    int count;
    CatalogSlot() : data(nullptr), count(0) {}
    CatalogSlot(const std::string* d, int n) : data(d), count(n) {}
};

inline std::map<std::string, CatalogSlot>& catalogs() {
    static std::map<std::string, CatalogSlot> m;
    return m;
}

inline std::string& localeId() {
    static std::string id = "zh-CN";
    return id;
}

inline CatalogSlot& current() {
    static CatalogSlot slot;
    return slot;
}

inline const std::string& emptyFallback() {
    static const std::string s;
    return s;
}

inline void refreshCurrent() {
    std::map<std::string, CatalogSlot>& m = catalogs();
    std::map<std::string, CatalogSlot>::iterator it = m.find(localeId());
    if (it != m.end()) {
        current() = it->second;
        return;
    }
    // 回退:任意已注册目录
    if (!m.empty()) {
        current() = m.begin()->second;
        return;
    }
    current() = CatalogSlot();
}

} // namespace detail

inline const std::string& locale() { return detail::localeId(); }

// 注册某语言的文案表(调用方保证 entries 生命周期覆盖整个运行期)
inline void registerCatalog(const std::string& localeId,
                            const std::string* entries, int count) {
    detail::catalogs()[localeId] = detail::CatalogSlot(entries, count);
    if (detail::localeId() == localeId || detail::current().data == nullptr) {
        detail::refreshCurrent();
    }
}

inline void setLocale(const std::string& id) {
    detail::localeId() = id;
    detail::refreshCurrent();
}

// 按下标取当前语言文案;越界或未注册返回空串
inline const std::string& t(int key) {
    const detail::CatalogSlot& slot = detail::current();
    if (slot.data == nullptr || key < 0 || key >= slot.count) {
        return detail::emptyFallback();
    }
    return slot.data[key];
}

} // namespace i18n
} // namespace skiff

// SKIFF_TR(my_key) → i18n::t(static_cast<int>(my_key)); key 可为应用枚举
#ifndef SKIFF_TR
#define SKIFF_TR(key) (::skiff::i18n::t(static_cast<int>(key)))
#endif
