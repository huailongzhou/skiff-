// Canvas:后端无关的 2D 画布绘制接口。
//
// 页面代码只通过 CanvasContext 画点线面;真正写像素的是后端
// (LVGL 里是 lv_canvas)。颜色与其它控件一样,uint32_t 0xRRGGBB。
#pragma once

#include <cstdint>
#include <functional>

namespace skiff {

struct CanvasPoint {
    int x;
    int y;
    CanvasPoint() : x(0), y(0) {}
    CanvasPoint(int x_, int y_) : x(x_), y(y_) {}
};

class CanvasContext {
public:
    virtual ~CanvasContext() {}

    virtual int width() const = 0;
    virtual int height() const = 0;

    // opa:0 全透明,255 不透明
    virtual void clear(uint32_t rgb, uint8_t opa = 255) = 0;

    // radius>0 时画圆角矩形(半径等于短边一半即椭圆/圆)
    virtual void fillRect(int x, int y, int w, int h, uint32_t rgb,
                          int radius = 0) = 0;

    virtual void fillCircle(int cx, int cy, int r, uint32_t rgb) = 0;

    virtual void fillPolygon(const CanvasPoint* pts, int n, uint32_t rgb) = 0;

    // 折线,width 为线宽像素
    virtual void strokeLine(const CanvasPoint* pts, int n, uint32_t rgb,
                            int width = 1) = 0;

    // 圆弧,角度单位为度,0° 为 3 点钟方向,顺时针
    virtual void strokeArc(int cx, int cy, int r, int startDeg, int endDeg,
                           uint32_t rgb, int width = 2) = 0;
};

typedef std::function<void(CanvasContext&)> CanvasPainter;

} // namespace skiff
