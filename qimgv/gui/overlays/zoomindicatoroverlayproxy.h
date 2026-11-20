#pragma once

#include "gui/overlays/zoomindicatoroverlay.h"

struct ZoomIndicatorOverlayStateBuffer {
    qreal scale;
    FloatingWidgetPosition position = BOTTOMLEFT;
};

class ZoomIndicatorOverlayProxy {
public:
    explicit ZoomIndicatorOverlayProxy(FloatingWidgetContainer *parent = nullptr);
    ~ZoomIndicatorOverlayProxy();
    void init();
    void show();
    void show(int duration);
    void hide();
    void setScale(qreal scale);
    void setPosition(FloatingWidgetPosition pos);

private:
    FloatingWidgetContainer *container;
    ZoomIndicatorOverlay *overlay;
    ZoomIndicatorOverlayStateBuffer stateBuf;
};
