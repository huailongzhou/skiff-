// Platform:平台能力抽象,管理外部回调(external)的注册与触发,以及平台事件
// (event)的订阅与上报。
//
// 两个方向:
//   UI → 平台:invokeExternal("setBrightness", {"80"})   页面调平台功能
//   平台 → UI:emit("musicProgress", {"40"})             平台上报事件
// emit 可在任意线程调用(先入队);事件统一在 UI 线程的 pumpEvents() 里派发,
// handler 里可以安全地 set State 驱动界面重建。
//
// 用法:
//   skiff::Platform platform;
//   platform.registerExternal("setBrightness", [](const std::vector<std::string>& args) {
//       // 板子代码实现
//   });
//   platform.on("musicProgress", [](const std::vector<std::string>& args) {
//       // UI 侧:更新进度状态
//   });
//   platform.invokeExternal("setBrightness", {"80"});
//   platform.invokeLater("playMusic", {path});  // 勿在 LVGL 点击回调里同步做重活
//   platform.emit("musicProgress", {"40"});   // 后台线程
//   platform.pumpEvents();                    // 主循环每帧调用(SDL 宿主 run 已集成)
#pragma once

#include <functional>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace skiff {

class Platform {
public:
    using ExternalHandler = std::function<void(const std::vector<std::string>&)>;
    using EventHandler = std::function<void(const std::vector<std::string>&)>;

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

    // 延后到下一次 pumpEvents() 再 invokeExternal。
    // LVGL 点击回调里不要同步解码音频/开设备,否则主循环卡住、看起来像死机。
    void invokeLater(const std::string& name,
                     const std::vector<std::string>& args) {
        std::lock_guard<std::mutex> lk(eventMutex_);
        deferred_.push_back(std::make_pair(name, args));
    }

    // ---- 事件:平台 → UI ----

    // 订阅一个平台事件(按名字,后者覆盖前者)。handler 在 UI 线程执行。
    void on(const std::string& name, EventHandler handler) {
        eventHandlers_[name] = std::move(handler);
    }

    // 上报一个平台事件。线程安全:可在播放器/监听等任意后台线程调用,
    // 事件先入队,由 UI 线程的 pumpEvents() 统一派发。
    void emit(const std::string& name, const std::vector<std::string>& args) {
        std::lock_guard<std::mutex> lk(eventMutex_);
        eventQueue_.push_back(std::make_pair(name, args));
    }

    // 派发已入队的平台事件。必须在 UI 线程调用。
    // SDL 宿主 run() 每帧在 lv_timer_handler 之前调用。
    void pumpEvents() {
        std::vector<std::pair<std::string, std::vector<std::string> > > pending;
        {
            std::lock_guard<std::mutex> lk(eventMutex_);
            pending.swap(eventQueue_);
        }
        for (size_t i = 0; i < pending.size(); ++i) {
            std::map<std::string, EventHandler>::iterator it =
                eventHandlers_.find(pending[i].first);
            if (it != eventHandlers_.end() && it->second) {
                it->second(pending[i].second);
            }
        }
        pumpDeferred();
    }

    // 执行 invokeLater 排队的 external。SDL 宿主在 app.update() 之后再调一次,
    // 这样点击里排队的 playMusic 发生在页面切换之后,且不堵在 LVGL 事件回调里。
    void pumpDeferred() {
        std::vector<std::pair<std::string, std::vector<std::string> > > later;
        {
            std::lock_guard<std::mutex> lk(eventMutex_);
            later.swap(deferred_);
        }
        for (size_t i = 0; i < later.size(); ++i) {
            invokeExternal(later[i].first, later[i].second);
        }
    }

private:
    std::map<std::string, ExternalHandler> externals_;
    std::set<std::string> declared_;
    std::map<std::string, EventHandler> eventHandlers_;
    std::vector<std::pair<std::string, std::vector<std::string> > > eventQueue_;
    std::vector<std::pair<std::string, std::vector<std::string> > > deferred_;
    std::mutex eventMutex_;
};

} // namespace skiff
