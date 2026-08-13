#include "skiff_lvgl_sdl3.hpp"

#include <SDL3/SDL.h>

#include "skiff/backend.hpp"
#include "skiff/input.hpp"
#include "skiff/platform.hpp"

namespace skiff {
namespace lvgl {

namespace {

struct Sdl3Ctx {
    SDL_Window* win;
    SDL_Renderer* ren;
    SDL_Texture* tex;
    uint64_t lastTick;
    int horRes;
    int verRes;
    Sdl3Ctx() : win(nullptr), ren(nullptr), tex(nullptr),
                lastTick(0), horRes(0), verRes(0) {}
};

Sdl3Ctx g;

// LVGL 渲染完一块区域 → 拷贝进纹理并上屏。
// LV_COLOR_DEPTH=32 时 lv_color_t 内存序(B,G,R,A)即 SDL_PIXELFORMAT_ARGB8888。
void flushCb(lv_disp_drv_t* drv, const lv_area_t* area, lv_color_t* color_p) {
    SDL_Rect r;
    r.x = area->x1;
    r.y = area->y1;
    r.w = area->x2 - area->x1 + 1;
    r.h = area->y2 - area->y1 + 1;
    SDL_UpdateTexture(g.tex, &r, color_p, r.w * (int)sizeof(lv_color_t));
    SDL_RenderTexture(g.ren, g.tex, nullptr, nullptr);
    SDL_RenderPresent(g.ren);
    lv_disp_flush_ready(drv);
}

void mouseRead(lv_indev_drv_t*, lv_indev_data_t* data) {
    float x = 0.0f, y = 0.0f;
    SDL_MouseButtonFlags buttons = SDL_GetMouseState(&x, &y);
    data->point.x = (lv_coord_t)x;
    data->point.y = (lv_coord_t)y;
    data->state = (buttons & SDL_BUTTON_LMASK) ? LV_INDEV_STATE_PRESSED
                                               : LV_INDEV_STATE_RELEASED;
}

} // namespace

lv_disp_t* createSdl3Display(int horRes, int verRes, const char* title) {
    SDL_Init(SDL_INIT_VIDEO);
    g.horRes = horRes;
    g.verRes = verRes;
    g.win = SDL_CreateWindow(title, horRes, verRes, 0);
    g.ren = SDL_CreateRenderer(g.win, nullptr);
    g.tex = SDL_CreateTexture(g.ren, SDL_PIXELFORMAT_ARGB8888,
                              SDL_TEXTUREACCESS_STREAMING, horRes, verRes);
    g.lastTick = SDL_GetTicks();

    // 显示驱动(draw buffer 随进程生命周期存在,预览工具不回收)
    lv_color_t* pixels = new lv_color_t[(size_t)(horRes * verRes)];
    lv_disp_draw_buf_t* drawBuf = new lv_disp_draw_buf_t;
    lv_disp_draw_buf_init(drawBuf, pixels, nullptr, (uint32_t)(horRes * verRes));

    lv_disp_drv_t* drv = new lv_disp_drv_t;
    lv_disp_drv_init(drv);
    drv->draw_buf = drawBuf;
    drv->flush_cb = flushCb;
    drv->hor_res = (lv_coord_t)horRes;
    drv->ver_res = (lv_coord_t)verRes;
    lv_disp_t* disp = lv_disp_drv_register(drv);

    // 鼠标输入驱动
    lv_indev_drv_t* indev = new lv_indev_drv_t;
    lv_indev_drv_init(indev);
    indev->type = LV_INDEV_TYPE_POINTER;
    indev->read_cb = mouseRead;
    lv_indev_drv_register(indev);

    return disp;
}

bool sdl3Pump() {
    SDL_Event e;
    bool running = true;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_EVENT_QUIT) {
            running = false;
        } else if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
            skiff::input::pointerDown({e.button.x, e.button.y});
        } else if (e.type == SDL_EVENT_MOUSE_BUTTON_UP) {
            skiff::input::pointerUp({e.button.x, e.button.y});
        } else if (e.type == SDL_EVENT_MOUSE_MOTION) {
            skiff::input::pointerMove({e.motion.x, e.motion.y});
        } else if (e.type == SDL_EVENT_FINGER_DOWN) {
            skiff::input::pointerDown({e.tfinger.x * g.horRes,
                                       e.tfinger.y * g.verRes});
        } else if (e.type == SDL_EVENT_FINGER_UP) {
            skiff::input::pointerUp({e.tfinger.x * g.horRes,
                                     e.tfinger.y * g.verRes});
        } else if (e.type == SDL_EVENT_FINGER_MOTION) {
            skiff::input::pointerMove({e.tfinger.x * g.horRes,
                                       e.tfinger.y * g.verRes});
        }
    }

    const uint64_t now = SDL_GetTicks();
    lv_tick_inc((uint32_t)(now - g.lastTick));
    g.lastTick = now;

    SDL_Delay(5);
    return running;
}

void run(skiff::App& app, skiff::Platform* platform) {
    app.start();
    while (sdl3Pump()) {
        if (platform) platform->pumpEvents();
        lv_timer_handler();
        app.update();
        if (platform) platform->pumpDeferred();
    }
}

void destroySdl3Display() {
    if (g.tex) SDL_DestroyTexture(g.tex);
    if (g.ren) SDL_DestroyRenderer(g.ren);
    if (g.win) SDL_DestroyWindow(g.win);
    SDL_Quit();
    g = Sdl3Ctx();
}

} // namespace lvgl
} // namespace skiff
