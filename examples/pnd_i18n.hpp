// PND 示例业务文案:枚举 key + 中英目录,基于 skiff::i18n 框架层。
// 仅供 pnd_sdl / 平台入口使用,不属于框架核心。
#pragma once

#include <string>

#include "skiff/i18n.hpp"

namespace pnd {
namespace i18n {

enum Key {
    nav_home = 0,
    nav_back,
    nav_back_page,

    app_navi,
    app_music,
    app_phone,
    app_phone_short,
    app_radio,
    app_media,
    app_settings,
    app_apps,
    app_wifi,
    app_bluetooth,
    app_gallery,
    app_battery,
    app_map,
    app_contacts,
    app_video,
    app_camera,
    app_recorder,
    app_calendar,
    app_weather,
    app_calculator,
    app_files,
    app_clock,
    app_brightness,

    common_demo,
    common_on,
    common_off,
    common_connected,
    common_not_enabled,
    common_dash,

    settings_network,
    settings_display,
    settings_sound,
    settings_system,
    settings_wifi,
    settings_bluetooth,
    settings_mobile_data,
    settings_airplane,
    settings_hotspot,
    settings_auto_brightness,
    settings_night_mode,
    settings_resolution,
    settings_theme,
    settings_theme_dark,
    settings_media_volume,
    settings_navi_volume,
    settings_beep,
    settings_eq,
    settings_eq_pop,
    settings_version,
    settings_storage,
    settings_language,
    settings_reset,
    settings_about,
    settings_lang_zh,
    settings_lang_en,

    music_playing,
    music_stopped,

    media_video,
    media_music,
    media_image,
    media_ebook,

    KEY_COUNT
};

namespace detail {

inline void fillZhCN(std::string* c) {
    c[nav_home] = "返回主页";
    c[nav_back] = "返回";
    c[nav_back_page] = "返回上一页";

    c[app_navi] = "导航";
    c[app_music] = "音乐";
    c[app_phone] = "蓝牙电话";
    c[app_phone_short] = "电话";
    c[app_radio] = "收音机";
    c[app_media] = "多媒体";
    c[app_settings] = "设置";
    c[app_apps] = "应用";
    c[app_wifi] = "Wi-Fi";
    c[app_bluetooth] = "蓝牙";
    c[app_gallery] = "相册";
    c[app_battery] = "电量";
    c[app_map] = "地图";
    c[app_contacts] = "通讯录";
    c[app_video] = "视频";
    c[app_camera] = "相机";
    c[app_recorder] = "录音";
    c[app_calendar] = "日历";
    c[app_weather] = "天气";
    c[app_calculator] = "计算器";
    c[app_files] = "文件";
    c[app_clock] = "时钟";
    c[app_brightness] = "亮度";

    c[common_demo] = "功能演示界面";
    c[common_on] = "开启";
    c[common_off] = "关闭";
    c[common_connected] = "已连接";
    c[common_not_enabled] = "未开启";
    c[common_dash] = "-";

    c[settings_network] = "网络";
    c[settings_display] = "显示";
    c[settings_sound] = "声音";
    c[settings_system] = "系统";
    c[settings_wifi] = "Wi-Fi";
    c[settings_bluetooth] = "蓝牙";
    c[settings_mobile_data] = "移动数据";
    c[settings_airplane] = "飞行模式";
    c[settings_hotspot] = "热点";
    c[settings_auto_brightness] = "自动调节";
    c[settings_night_mode] = "夜间模式";
    c[settings_resolution] = "分辨率";
    c[settings_theme] = "主题";
    c[settings_theme_dark] = "深色";
    c[settings_media_volume] = "媒体音量";
    c[settings_navi_volume] = "导航音量";
    c[settings_beep] = "提示音";
    c[settings_eq] = "均衡器";
    c[settings_eq_pop] = "流行";
    c[settings_version] = "系统版本";
    c[settings_storage] = "存储空间";
    c[settings_language] = "语言";
    c[settings_reset] = "恢复出厂";
    c[settings_about] = "关于";
    c[settings_lang_zh] = "简体中文";
    c[settings_lang_en] = "English";

    c[music_playing] = "正在播放";
    c[music_stopped] = "已停止";

    c[media_video] = "视频";
    c[media_music] = "音乐";
    c[media_image] = "图片";
    c[media_ebook] = "电子书";
}

inline void fillEn(std::string* c) {
    c[nav_home] = "Home";
    c[nav_back] = "Back";
    c[nav_back_page] = "Go Back";

    c[app_navi] = "Navigation";
    c[app_music] = "Music";
    c[app_phone] = "BT Phone";
    c[app_phone_short] = "Phone";
    c[app_radio] = "Radio";
    c[app_media] = "Media";
    c[app_settings] = "Settings";
    c[app_apps] = "Apps";
    c[app_wifi] = "Wi-Fi";
    c[app_bluetooth] = "Bluetooth";
    c[app_gallery] = "Gallery";
    c[app_battery] = "Battery";
    c[app_map] = "Map";
    c[app_contacts] = "Contacts";
    c[app_video] = "Video";
    c[app_camera] = "Camera";
    c[app_recorder] = "Recorder";
    c[app_calendar] = "Calendar";
    c[app_weather] = "Weather";
    c[app_calculator] = "Calculator";
    c[app_files] = "Files";
    c[app_clock] = "Clock";
    c[app_brightness] = "Brightness";

    c[common_demo] = "Feature demo";
    c[common_on] = "On";
    c[common_off] = "Off";
    c[common_connected] = "Connected";
    c[common_not_enabled] = "Off";
    c[common_dash] = "-";

    c[settings_network] = "Network";
    c[settings_display] = "Display";
    c[settings_sound] = "Sound";
    c[settings_system] = "System";
    c[settings_wifi] = "Wi-Fi";
    c[settings_bluetooth] = "Bluetooth";
    c[settings_mobile_data] = "Mobile Data";
    c[settings_airplane] = "Airplane Mode";
    c[settings_hotspot] = "Hotspot";
    c[settings_auto_brightness] = "Auto Brightness";
    c[settings_night_mode] = "Night Mode";
    c[settings_resolution] = "Resolution";
    c[settings_theme] = "Theme";
    c[settings_theme_dark] = "Dark";
    c[settings_media_volume] = "Media Volume";
    c[settings_navi_volume] = "Navi Volume";
    c[settings_beep] = "Beep";
    c[settings_eq] = "Equalizer";
    c[settings_eq_pop] = "Pop";
    c[settings_version] = "Version";
    c[settings_storage] = "Storage";
    c[settings_language] = "Language";
    c[settings_reset] = "Factory Reset";
    c[settings_about] = "About";
    c[settings_lang_zh] = "简体中文";
    c[settings_lang_en] = "English";

    c[music_playing] = "Playing";
    c[music_stopped] = "Stopped";

    c[media_video] = "Video";
    c[media_music] = "Music";
    c[media_image] = "Images";
    c[media_ebook] = "E-books";
}

inline std::string* zhCN() {
    static std::string c[KEY_COUNT];
    static bool init = false;
    if (!init) {
        fillZhCN(c);
        init = true;
    }
    return c;
}

inline std::string* en() {
    static std::string c[KEY_COUNT];
    static bool init = false;
    if (!init) {
        fillEn(c);
        init = true;
    }
    return c;
}

} // namespace detail

// 注册中英目录并设默认语言(进程内调用一次即可)
inline void init(const char* defaultLocale = "zh-CN") {
    skiff::i18n::registerCatalog("zh-CN", detail::zhCN(), KEY_COUNT);
    skiff::i18n::registerCatalog("en", detail::en(), KEY_COUNT);
    skiff::i18n::registerCatalog("en-US", detail::en(), KEY_COUNT);
    skiff::i18n::setLocale(defaultLocale);
}

// 路由 ID → 展示文案
inline const std::string& routeTitle(const std::string& routeId) {
    if (routeId == "navi") return tr(app_navi);
    if (routeId == "music") return tr(app_music);
    if (routeId == "phone") return tr(app_phone);
    if (routeId == "radio") return tr(app_radio);
    if (routeId == "media") return tr(app_media);
    if (routeId == "settings") return tr(app_settings);
    if (routeId == "apps") return tr(app_apps);
    if (routeId == "wifi") return tr(app_wifi);
    if (routeId == "bluetooth") return tr(app_bluetooth);
    if (routeId == "gallery") return tr(app_gallery);
    if (routeId == "battery") return tr(app_battery);
    if (routeId == "map") return tr(app_map);
    if (routeId == "contacts") return tr(app_contacts);
    if (routeId == "video") return tr(app_video);
    if (routeId == "camera") return tr(app_camera);
    if (routeId == "recorder") return tr(app_recorder);
    if (routeId == "calendar") return tr(app_calendar);
    if (routeId == "weather") return tr(app_weather);
    if (routeId == "calculator") return tr(app_calculator);
    if (routeId == "files") return tr(app_files);
    if (routeId == "clock") return tr(app_clock);
    static const std::string empty;
    return empty;
}

} // namespace i18n
} // namespace pnd
