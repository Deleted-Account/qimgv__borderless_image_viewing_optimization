#pragma once

#include <QWidget>
#include <QSpacerItem>
#include "gui/customwidgets/overlaywidget.h"

namespace Ui {
class FullscreenInfoOverlay;
}

class FullscreenInfoOverlay : public OverlayWidget {
    Q_OBJECT

public:
    explicit FullscreenInfoOverlay(FloatingWidgetContainer *parent = nullptr);
    ~FullscreenInfoOverlay();
    void setInfo(QString pos, QString fileName, QString info);
    void setWindowedStyle(bool enabled);
    void setPosition(FloatingWidgetPosition pos);

private:
    void setRightAligned(bool enabled);
    Ui::FullscreenInfoOverlay *ui;
    QSpacerItem *rightAlignSpacer = nullptr;
};
