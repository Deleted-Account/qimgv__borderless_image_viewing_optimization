#pragma once

#include "gui/overlays/fullscreeninfooverlay.h"

struct InfoOverlayStateBuffer {
    QString position;
    QString fileName;
    QString info;
    bool showImmediately = false;
    FloatingWidgetPosition widgetPosition = TOPLEFT;
    bool windowedStyle = false;
};

class FullscreenInfoOverlayProxy {
public:
    explicit FullscreenInfoOverlayProxy(FloatingWidgetContainer *parent = nullptr);
    ~FullscreenInfoOverlayProxy();
    void init();
    void show();
    void showWhenReady();
    void hide();
    void setInfo(QString position, QString fileName, QString info);
    void setPosition(FloatingWidgetPosition pos);
    void setWindowedStyle(bool enabled);

private:
    FloatingWidgetContainer *container;
    FullscreenInfoOverlay *infoOverlay;
    InfoOverlayStateBuffer stateBuf;
};
