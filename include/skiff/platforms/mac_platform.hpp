// MacPlatform:macOS 平台能力实现。
//
// 通过 shell/osascript 调用 macOS 系统功能,供开发/预览时使用。
// 嵌入式实机应实现自己的 Platform 子类,替换掉这些 shell 命令。
//
// 已支持的 external:
//   setBrightness <0-100>    设置屏幕亮度(依赖 AppleScript/System Events)
//   setVolume    <0-100>     设置系统输出音量
//   getVolume                 返回当前音量(0-100)
//   getBatteryLevel           返回电池百分比
//   toggleWifi   <on|off>     开关 Wi-Fi(依赖 networksetup)
//   toggleBluetooth <on|off>  开关蓝牙(依赖 blueutil)
#pragma once

#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <string>
#include <vector>

#include "../platform.hpp"

namespace skiff {
namespace platforms {

class MacPlatform : public Platform {
public:
    MacPlatform() {
        registerExternal("setBrightness", [](const std::vector<std::string>& args) {
            if (args.empty()) return;
            const int v = std::atoi(args[0].c_str());
            std::stringstream ss;
            ss << "osascript -e 'tell application \"System Events\"' "
               << "-e 'set brightness of first desktop to " << v / 100.0 << "' "
               << "-e 'end tell' >/dev/null 2>&1";
            std::system(ss.str().c_str());
            std::printf("[MacPlatform] setBrightness %d%%\n", v);
        });

        registerExternal("setVolume", [](const std::vector<std::string>& args) {
            if (args.empty()) return;
            const int v = std::atoi(args[0].c_str());
            std::stringstream ss;
            ss << "osascript -e 'set volume output volume " << v << "'";
            std::system(ss.str().c_str());
            std::printf("[MacPlatform] setVolume %d%%\n", v);
        });

        registerExternal("getVolume", [](const std::vector<std::string>&) {
            FILE* f = popen("osascript -e 'output volume of (get volume settings)'", "r");
            if (!f) return;
            char buf[32] = {0};
            if (fgets(buf, sizeof(buf), f)) {
                std::printf("[MacPlatform] getVolume = %s\n", buf);
            }
            pclose(f);
        });

        registerExternal("getBatteryLevel", [](const std::vector<std::string>&) {
            FILE* f = popen("pmset -g batt | grep -Eo '[0-9]+%' | head -1 | tr -d '%'", "r");
            if (!f) return;
            char buf[16] = {0};
            if (fgets(buf, sizeof(buf), f)) {
                std::printf("[MacPlatform] getBatteryLevel = %s%%\n", buf);
            }
            pclose(f);
        });

        registerExternal("toggleWifi", [](const std::vector<std::string>& args) {
            const std::string onoff = args.empty() ? "on" : args[0];
            std::stringstream ss;
            ss << "networksetup -setairportpower en0 " << onoff;
            std::system(ss.str().c_str());
            std::printf("[MacPlatform] toggleWifi %s\n", onoff.c_str());
        });

        registerExternal("toggleBluetooth", [](const std::vector<std::string>& args) {
            const std::string onoff = args.empty() ? "on" : args[0];
            const int p = (onoff == "on" || onoff == "1") ? 1 : 0;
            std::stringstream ss;
            ss << "blueutil -p " << p;
            std::system(ss.str().c_str());
            std::printf("[MacPlatform] toggleBluetooth %s\n", onoff.c_str());
        });
    }
};

} // namespace platforms
} // namespace skiff
