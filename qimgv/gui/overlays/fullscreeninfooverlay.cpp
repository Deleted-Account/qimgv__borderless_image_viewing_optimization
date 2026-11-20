#include "fullscreeninfooverlay.h"
#include "ui_fullscreeninfooverlay.h"

FullscreenInfoOverlay::FullscreenInfoOverlay(FloatingWidgetContainer *parent) :
    OverlayWidget(parent),
    ui(new Ui::FullscreenInfoOverlay)
{
    ui->setupUi(this);
    ui->horizontalLayout->setContentsMargins(0, 5, 0, 5);
    setPosition(FloatingWidgetPosition::TOPLEFT);
    this->setHorizontalMargin(0);
    this->setVerticalMargin(0);
    ui->nameLabel->setText("No file opened");
    if(parent)
        setContainerSize(parent->size());
}

FullscreenInfoOverlay::~FullscreenInfoOverlay() {
    delete ui;
}

void FullscreenInfoOverlay::setInfo(QString pos, QString fileName, QString info) {
    const QString nbsp = QString(QChar(0x00A0));
    ui->posLabel->setText(nbsp + pos);
    ui->nameLabel->setText(fileName);
    ui->infoLabel->setText(info + nbsp);
    this->adjustSize();
    OverlayWidget::recalculateGeometry();
}

void FullscreenInfoOverlay::setWindowedStyle(bool enabled) {
    setObjectName(enabled ? "WindowInfoOverlay" : "FullscreenInfoOverlay");
    style()->unpolish(this);
    for(QWidget *child : findChildren<QWidget*>()) {
        style()->unpolish(child);
        style()->polish(child);
    }
    style()->polish(this);
}

void FullscreenInfoOverlay::setPosition(FloatingWidgetPosition pos) {
    OverlayWidget::setPosition(pos);
    setRightAligned(pos == RIGHT || pos == TOPRIGHT || pos == BOTTOMRIGHT);
}

void FullscreenInfoOverlay::setRightAligned(bool enabled) {
    if(enabled && !rightAlignSpacer) {
        rightAlignSpacer = new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum);
        ui->horizontalLayout->insertSpacerItem(0, rightAlignSpacer);
    } else if(!enabled && rightAlignSpacer) {
        ui->horizontalLayout->removeItem(rightAlignSpacer);
        delete rightAlignSpacer;
        rightAlignSpacer = nullptr;
    }

    Qt::Alignment alignment = enabled ? Qt::AlignRight : Qt::AlignLeft;
    ui->posLabel->setAlignment(alignment);
    ui->nameLabel->setAlignment(alignment);
    ui->infoLabel->setAlignment(alignment);
    adjustSize();
    OverlayWidget::recalculateGeometry();
}
