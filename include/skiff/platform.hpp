// Platform:平台能力抽象,管理外部回调(external)的注册与触发。
//
// 页面代码通过 Platform 声明需要调用板子/平台功能,但具体实现由平台代码注册。
// 这与 Backend(UI 渲染)解耦:Backend 只负责控件树,Platform 负责平台功能。
//
// 用法:
//   skiff::Platform platform;
//   platform.registerExternal("setBrightness", [](const std::vector<std::string>& args) {
//       // 板子代码实现
//   });
//   platform.invokeExternal("setBrightness", {"80"});
#pragma once

#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace skiff {

class Platform {
public:
    using ExternalHandler = std::function<void(const std::vector<std::string>&)>;

    // 页面代码声明需要某个平台能力(仅记录,不绑定实现)。
    void declare(const std::string& name) {
        declared_.insert(name);
    }

    // 查询某个能力是否已被声明。
    bool hasDeclared(const std::string& name) const {
        return declared_.find(name) != declared_.end();
    }

    // 注册一个 external 处理器(按名字)。可重复注册,后者覆盖前者。
    void registerExternal(const std::string& name, ExternalHandler handler) {
        externals_[name] = std::move(handler);
    }

    // 触发 external 回调。如果名字未注册,默认无操作(避免页面代码直接崩溃)。
    void invokeExternal(const std::string& name,
                        const std::vector<std::string>& args) const {
        std::map<std::string, ExternalHandler>::const_iterator it = externals_.find(name);
        if (it != externals_.end() && it->second) {
            it->second(args);
        }
    }

private:
    std::map<std::string, ExternalHandler> externals_;
    std::set<std::string> declared_;
};

} // namespace skiff
