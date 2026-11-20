#include "mainwindow.h"

FloatingWidgetPosition MW::infoPositionFromIndex(int index) {
    switch(index) {
        case 1: return TOP;
        case 2: return TOPRIGHT;
        case 3: return BOTTOMLEFT;
        case 4: return BOTTOM;
        case 5: return BOTTOMRIGHT;
        default: return TOPLEFT;
    }
}

// TODO: nuke this and rewrite

MW::MW(QWidget *parent)
    : FloatingWidgetContainer(parent),
      currentDisplay(0),
      maximized(false),
      activeSidePanel(SIDEPANEL_NONE),
      copyOverlay(nullptr),
      saveOverlay(nullptr),
      renameOverlay(nullptr),
      infoBarFullscreen(nullptr),
      windowedInfoBarOverlay(nullptr),
      imageInfoOverlay(nullptr),
      floatingMessage(nullptr),
      cropPanel(nullptr),
      cropOverlay(nullptr)
{
    setAttribute(Qt::WA_TranslucentBackground, true);
    layout.setContentsMargins(0,0,0,0);
    layout.setSpacing(0);

    setMinimumSize(10,10);

    // do not steal focus when clicked
    // this is just a container. accept key events only
    // via passthrough from child widgets
    setFocusPolicy(Qt::NoFocus);

    this->setLayout(&layout);

    setWindowTitle(QCoreApplication::applicationName() + " " +
                   QCoreApplication::applicationVersion());

    this->setMouseTracking(true);
    this->setAcceptDrops(true);
    this->setAccessibleName("mainwindow");
    windowGeometryChangeTimer.setSingleShot(true);
    windowGeometryChangeTimer.setInterval(30);
    mZoomResizeTimer.setSingleShot(true);
    mZoomResizeTimer.setInterval(16);
    mWindowedInfoTimer.setSingleShot(true);
    mWindowedInfoTimer.setInterval(5000);
    setupUi();

    connect(settings, &Settings::settingsChanged, this, &MW::readSettings);
    connect(&windowGeometryChangeTimer, &QTimer::timeout, this, &MW::onWindowGeometryChanged);
    connect(&mZoomResizeTimer, &QTimer::timeout, this, [this]() {
        if(mPendingZoomResizeSize.isEmpty())
            return;
        QSize size = mPendingZoomResizeSize;
        mPendingZoomResizeSize = QSize();
        viewerWidget->setFitUpdatesEnabled(false);
        applyAutoResize(size);
        viewerWidget->setFitUpdatesEnabled(true);
        viewerWidget->centerForZoom();
    });
    connect(&mWindowedInfoTimer, &QTimer::timeout, this, [this]() {
        if(windowedInfoBarOverlay)
            windowedInfoBarOverlay->hide();
    });
    connect(this, &MW::fullscreenStateChanged, this, &MW::adaptToWindowState);

    readSettings();
    currentDisplay = settings->lastDisplay();
    maximized = settings->maximizedWindow();
    restoreWindowGeometry();
}

/*                                                             |--[ImageViewer]
 *                        |--[DocumentWidget]--[ViewerWidget]--|
 * [MW]--[CentralWidget]--|                                    |--[VideoPlayer]
 *                        |--[FolderView]
 *
 *  (not counting floating widgets)
 *  ViewerWidget exists for input handling reasons (correct overlay hover handling)
 */
void MW::setupUi() {
    viewerWidget.reset(new ViewerWidget(this));
    infoBarWindowed.reset(new InfoBarProxy(this));
    docWidget.reset(new DocumentWidget(viewerWidget, infoBarWindowed));
    folderView.reset(new FolderViewProxy(this));
    connect(folderView.get(), &FolderViewProxy::sortingSelected, this, &MW::sortingSelected);
    connect(folderView.get(), &FolderViewProxy::directorySelected, this, &MW::opened);
    connect(folderView.get(), &FolderViewProxy::copyUrlsRequested, this, &MW::copyUrlsRequested);
    connect(folderView.get(), &FolderViewProxy::moveUrlsRequested, this, &MW::moveUrlsRequested);
    connect(folderView.get(), &FolderViewProxy::showFoldersChanged, this, &MW::showFoldersChanged);

    centralWidget.reset(new CentralWidget(docWidget, folderView, this));
    layout.addWidget(centralWidget.get());
    controlsOverlay = new ControlsOverlay(docWidget.get());
    infoBarFullscreen = new FullscreenInfoOverlayProxy(viewerWidget.get());
    windowedInfoBarOverlay = new FullscreenInfoOverlayProxy(viewerWidget.get());
    windowedInfoBarOverlay->setWindowedStyle(true);
    sidePanel = new SidePanel(this);
    layout.addWidget(sidePanel);
    imageInfoOverlay = new ImageInfoOverlayProxy(viewerWidget.get());
    floatingMessage = new FloatingMessageProxy(viewerWidget.get()); // todo: use additional one for folderview?
    connect(viewerWidget.get(), &ViewerWidget::scalingRequested, this, &MW::scalingRequested);
    connect(viewerWidget.get(), &ViewerWidget::zoomResizeRequested, this, &MW::resizeForZoom);
    connect(viewerWidget.get(), &ViewerWidget::nativeFitRequested, this, &MW::resizeForNativeFit);
    connect(viewerWidget.get(), &ViewerWidget::draggedOut, this, qOverload<>(&MW::draggedOut));
    connect(viewerWidget.get(), &ViewerWidget::playbackFinished, this, &MW::playbackFinished);
    connect(viewerWidget.get(), &ViewerWidget::showScriptSettings, this, &MW::showScriptSettings);
    connect(this, &MW::zoomIn,        viewerWidget.get(), &ViewerWidget::zoomIn);
    connect(this, &MW::zoomOut,       viewerWidget.get(), &ViewerWidget::zoomOut);
    connect(this, &MW::zoomInCursor,  viewerWidget.get(), &ViewerWidget::zoomInCursor);
    connect(this, &MW::zoomOutCursor, viewerWidget.get(), &ViewerWidget::zoomOutCursor);
    connect(this, &MW::scrollUp,    viewerWidget.get(), &ViewerWidget::scrollUp);
    connect(this, &MW::scrollDown,  viewerWidget.get(), &ViewerWidget::scrollDown);
    connect(this, &MW::scrollLeft,  viewerWidget.get(), &ViewerWidget::scrollLeft);
    connect(this, &MW::scrollRight, viewerWidget.get(), &ViewerWidget::scrollRight);
    connect(this, &MW::pauseVideo,     viewerWidget.get(), &ViewerWidget::pauseResumePlayback);
    connect(this, &MW::stopPlayback,   viewerWidget.get(), &ViewerWidget::stopPlayback);
    connect(this, &MW::seekVideoForward, viewerWidget.get(), &ViewerWidget::seekForward);
    connect(this, &MW::seekVideoBackward,  viewerWidget.get(), &ViewerWidget::seekBackward);
    connect(this, &MW::frameStep,      viewerWidget.get(), &ViewerWidget::frameStep);
    connect(this, &MW::frameStepBack,  viewerWidget.get(), &ViewerWidget::frameStepBack);
    connect(this, &MW::toggleMute,  viewerWidget.get(), &ViewerWidget::toggleMute);
    connect(this, &MW::volumeUp,  viewerWidget.get(), &ViewerWidget::volumeUp);
    connect(this, &MW::volumeDown,  viewerWidget.get(), &ViewerWidget::volumeDown);
    connect(this, &MW::toggleTransparencyGrid, viewerWidget.get(), &ViewerWidget::toggleTransparencyGrid);
    connect(this, &MW::setLoopPlayback,  viewerWidget.get(), &ViewerWidget::setLoopPlayback);
}

void MW::setupFullUi() {
    setupCropPanel();
    docWidget->allowPanelInit();
    docWidget->setupMainPanel();
    infoBarWindowed->init();
    infoBarFullscreen->init();
    windowedInfoBarOverlay->init();
}

void MW::setupCropPanel() {
    if(cropPanel)
        return;
    cropOverlay = new CropOverlay(viewerWidget.get());
    cropPanel = new CropPanel(cropOverlay, this);
    connect(cropPanel, &CropPanel::cancel, this, &MW::hideCropPanel);
    connect(cropPanel, &CropPanel::crop,   this, &MW::hideCropPanel);
    connect(cropPanel, &CropPanel::crop,   this, &MW::cropRequested);
    connect(cropPanel, &CropPanel::cropAndSave, this, &MW::hideCropPanel);
    connect(cropPanel, &CropPanel::cropAndSave, this, &MW::cropAndSaveRequested);
}

void MW::setupCopyOverlay() {
    copyOverlay = new CopyOverlay(viewerWidget.get());
    connect(copyOverlay, &CopyOverlay::copyRequested, this, &MW::copyRequested);
    connect(copyOverlay, &CopyOverlay::moveRequested, this, &MW::moveRequested);
}

void MW::setupSaveOverlay() {
    saveOverlay = new SaveConfirmOverlay(viewerWidget.get());
    connect(saveOverlay, &SaveConfirmOverlay::saveClicked,    this, &MW::saveRequested);
    connect(saveOverlay, &SaveConfirmOverlay::saveAsClicked,  this, &MW::saveAsClicked);
    connect(saveOverlay, &SaveConfirmOverlay::discardClicked, this, &MW::discardEditsRequested);
}

void MW::setupRenameOverlay() {
    renameOverlay = new RenameOverlay(this);
    renameOverlay->setName(info.fileName);
    connect(renameOverlay, &RenameOverlay::renameRequested, this, &MW::renameRequested);
}

void MW::toggleFolderView() {
    hideCropPanel();
    if(copyOverlay)
        copyOverlay->hide();
    if(renameOverlay)
        renameOverlay->hide();
    docWidget->hideFloatingPanel();
    imageInfoOverlay->hide();
    centralWidget->toggleViewMode();
    onInfoUpdated();
}

void MW::enableFolderView() {
    hideCropPanel();
    if(copyOverlay)
        copyOverlay->hide();
    if(renameOverlay)
        renameOverlay->hide();
    docWidget->hideFloatingPanel();
    imageInfoOverlay->hide();
    centralWidget->showFolderView();
    onInfoUpdated();
}

void MW::enableDocumentView() {
    centralWidget->showDocumentView();
    onInfoUpdated();
}

ViewMode MW::currentViewMode() {
    return centralWidget->currentViewMode();
}

void MW::fitWindow() {
    if(viewerWidget->interactionEnabled()) {
        viewerWidget->fitWindow();
    } else {
        showMessage("Zoom temporary disabled");
    }
}

void MW::fitWidth() {
    if(viewerWidget->interactionEnabled()) {
        viewerWidget->fitWidth();
    } else {
        showMessage("Zoom temporary disabled");
    }
}

void MW::fitOriginal() {
    if(viewerWidget->interactionEnabled()) {
        viewerWidget->fitOriginal();
    } else {
        showMessage("Zoom temporary disabled");
    }
}

void MW::fitWindowStretch() {
    if(viewerWidget->interactionEnabled()) {
        viewerWidget->fitWindowStretch();
    } else {
        showMessage("Zoom temporary disabled");
    }
}

// switch between 1:1 and Fit All
// TODO: move to viewerWidget?
void MW::switchFitMode() {
    if(viewerWidget->fitMode() == FIT_WINDOW)
        viewerWidget->setFitMode(FIT_ORIGINAL);
    else
        viewerWidget->setFitMode(FIT_WINDOW);
}

void MW::closeImage() {
    info.fileName = "";
    info.filePath = "";
    viewerWidget->closeImage();
}

void MW::applyAutoResize(QSize logicalSize) {
    if(mApplyingAutoResize)
        return;
    if(!settings->autoResizeWindow())
        return;
    if(!isVisible()) {
        mPendingAutoResizeSize = logicalSize;
        return;
    }
    if(windowState() != Qt::WindowNoState || isFullScreen() || isMaximized())
        return;

    QScreen *screen = this->screen() ? this->screen() : QGuiApplication::primaryScreen();
    if(!screen)
        return;
    QRect available = screen->availableGeometry();
    QRect frameRect = frameGeometry();
    QRect clientRect = geometry();
    QMargins frame(qMax(0, clientRect.left() - frameRect.left()),
                   qMax(0, clientRect.top() - frameRect.top()),
                   qMax(0, frameRect.right() - clientRect.right()),
                   qMax(0, frameRect.bottom() - clientRect.bottom()));

    QSizeF area(available.width() - frame.left() - frame.right(),
                available.height() - frame.top() - frame.bottom());
    QSizeF image = logicalSize.isEmpty() ? area : QSizeF(logicalSize);

    int scaleX2 = settings->expandScaleX2();
    qreal limit = scaleX2 / 2.0f;
    qreal scale = 1.0f;
    if(limit > 0 && image.width() > 0 && image.height() > 0) {
        if(settings->alwaysExpand()) {
            scale = qMin(limit, qMin(area.width() / image.width(), area.height() / image.height()));
        } else {
            qreal intLimit = qFloor(limit);
            qreal fitX = qFloor(area.width() / image.width());
            qreal fitY = qFloor(area.height() / image.height());
            scale = qMin(intLimit, qMin(fitX, fitY));
        }
        if(scale < 1.0f)
            scale = 1.0f;
    }

    QSizeF desired = image * scale;
    bool cropMode = settings->imageFitMode() == FIT_ORIGINAL;
    QSizeF finalSize;
    if(cropMode) {
        finalSize.setWidth(qMin(desired.width(), area.width()));
        finalSize.setHeight(qMin(desired.height(), area.height()));
    } else {
        qreal fit = qMin(1.0f, qMin(area.width() / desired.width(), area.height() / desired.height()));
        finalSize = desired * fit;
    }
    viewerWidget->setDisplayScale(static_cast<float>(scale));

    QSize clientSize = finalSize.toSize().expandedTo(QSize(1, 1));
    QSize frameSize(clientSize.width() + frame.left() + frame.right(),
                    clientSize.height() + frame.top() + frame.bottom());
    QRect newFrame(QPoint(0, 0), frameSize);
    newFrame.moveCenter(available.center());
    QRect newClient(newFrame.left() + frame.left(), newFrame.top() + frame.top(),
                    clientSize.width(), clientSize.height());

    mApplyingAutoResize = true;
    setGeometry(newClient);
    mApplyingAutoResize = false;
}

void MW::resizeForZoom(QSize logicalSize) {
    if(logicalSize.isEmpty())
        return;
    mPendingZoomResizeSize = logicalSize;
    if(!mZoomResizeTimer.isActive())
        mZoomResizeTimer.start();
}

void MW::resizeForNativeFit(QSize logicalSize) {
    if(logicalSize.isEmpty() || mApplyingAutoResize)
        return;
    if(windowState() != Qt::WindowNoState || isFullScreen() || isMaximized())
        return;

    QScreen *screen = this->screen() ? this->screen() : QGuiApplication::primaryScreen();
    if(!screen)
        return;
    QRect available = screen->availableGeometry();
    if(logicalSize.width() <= available.width() && logicalSize.height() <= available.height())
        return;

    QRect frameRect = frameGeometry();
    QRect clientRect = geometry();
    QMargins frame(qMax(0, clientRect.left() - frameRect.left()),
                   qMax(0, clientRect.top() - frameRect.top()),
                   qMax(0, frameRect.right() - clientRect.right()),
                   qMax(0, frameRect.bottom() - clientRect.bottom()));

    QSizeF area(available.width() - frame.left() - frame.right(),
                available.height() - frame.top() - frame.bottom());
    QSizeF finalSize(qMin(static_cast<qreal>(logicalSize.width()), area.width()),
                     qMin(static_cast<qreal>(logicalSize.height()), area.height()));
    QSize clientSize = finalSize.toSize().expandedTo(QSize(1, 1));
    QSize frameSize(clientSize.width() + frame.left() + frame.right(),
                    clientSize.height() + frame.top() + frame.bottom());
    QRect newFrame(QPoint(0, 0), frameSize);
    newFrame.moveCenter(available.center());
    QRect newClient(newFrame.left() + frame.left(), newFrame.top() + frame.top(),
                    clientSize.width(), clientSize.height());

    viewerWidget->setDisplayScale(1.0f);
    mApplyingAutoResize = true;
    setGeometry(newClient);
    mApplyingAutoResize = false;
}

void MW::showImage(std::unique_ptr<QPixmap> pixmap) {
    if(pixmap) {
        qreal dpr = devicePixelRatioF() > 0 ? devicePixelRatioF() : 1.0;
        pixmap->setDevicePixelRatio(dpr);
        QSize logicalSize = pixmap->deviceIndependentSize().toSize();
        viewerWidget->setFitUpdatesEnabled(false);
        applyAutoResize(logicalSize);
        viewerWidget->showImage(std::move(pixmap));
        viewerWidget->setFitUpdatesEnabled(true);
        viewerWidget->applyCurrentFitMode();
    }
    updateCropPanelData();
}

void MW::showAnimation(std::shared_ptr<QMovie> movie) {
    if(movie) {
        qreal dpr = devicePixelRatioF() > 0 ? devicePixelRatioF() : 1.0;
        QSize logicalSize = QSizeF(movie->frameRect().size() / dpr).toSize();
        viewerWidget->setFitUpdatesEnabled(false);
        applyAutoResize(logicalSize);
        viewerWidget->showAnimation(movie);
        viewerWidget->setFitUpdatesEnabled(true);
        viewerWidget->applyCurrentFitMode();
    }
    updateCropPanelData();
}

void MW::showVideo(QString file) {
    viewerWidget->setFitUpdatesEnabled(false);
    applyAutoResize(QSize());
    viewerWidget->setFitUpdatesEnabled(true);
    viewerWidget->showVideo(file);
}

void MW::showContextMenu() {
    viewerWidget->showContextMenu();
}

void MW::onSortingChanged(SortingMode mode) {
    folderView.get()->onSortingChanged(mode);
    if(centralWidget.get()->currentViewMode() == ViewMode::MODE_DOCUMENT) {
        switch(mode) {
            case SortingMode::SORT_NAME:      showMessage("Sorting: By Name");              break;
            case SortingMode::SORT_NAME_DESC: showMessage("Sorting: By Name (desc.)");      break;
            case SortingMode::SORT_TIME:      showMessage("Sorting: By Time");              break;
            case SortingMode::SORT_TIME_DESC: showMessage("Sorting: By Time (desc.)");      break;
            case SortingMode::SORT_SIZE:      showMessage("Sorting: By File Size");         break;
            case SortingMode::SORT_SIZE_DESC: showMessage("Sorting: By File Size (desc.)"); break;
        }
    }
}

void MW::setDirectoryPath(QString path) {
    //closeImage();
    info.directoryPath = path;
    info.directoryName = path.split("/").last();
    folderView->setDirectoryPath(path);
    onInfoUpdated();
}

void MW::toggleLockZoom() {
    viewerWidget->toggleLockZoom();
    if(viewerWidget->lockZoomEnabled())
        showMessage("Zoom lock: ON");
    else
        showMessage("Zoom lock: OFF");
    onInfoUpdated();
}

void MW::toggleLockView() {
    viewerWidget->toggleLockView();
    if(viewerWidget->lockViewEnabled())
        showMessage("View lock: ON");
    else
        showMessage("View lock: OFF");
    onInfoUpdated();
}

void MW::toggleFullscreenInfoBar() {
    if(!this->isFullScreen())
        return;
    showInfoBarFullscreen = !showInfoBarFullscreen;
    if(showInfoBarFullscreen)
        infoBarFullscreen->showWhenReady();
    else
        infoBarFullscreen->hide();
}

void MW::toggleImageInfoOverlay() {
    if(centralWidget->currentViewMode() == MODE_FOLDERVIEW)
        return;
    if(imageInfoOverlay->isHidden())
        imageInfoOverlay->show();
    else
        imageInfoOverlay->hide();
}

void MW::toggleRenameOverlay(QString currentName) {
    if(!renameOverlay)
        setupRenameOverlay();
    if(renameOverlay->isHidden()) {
        renameOverlay->setBackdropEnabled((centralWidget->currentViewMode() == MODE_FOLDERVIEW));
        renameOverlay->setName(currentName);
        renameOverlay->show();
    } else {
        renameOverlay->hide();
    }
}

void MW::toggleScalingFilter() {
    ScalingFilter configuredFilter = settings->scalingFilter();
    if(viewerWidget->scalingFilter() == configuredFilter) {
        setFilterNearest();
    }
    else {
        setFilter(configuredFilter);
    }
}

void MW::setFilterNearest() {
    showMessage("Filter: nearest", 600);
    viewerWidget->setFilterNearest();
}

void MW::setFilterBilinear() {
    showMessage("Filter: bilinear", 600);
    viewerWidget->setFilterBilinear();
}

void MW::setFilter(ScalingFilter filter) {
    QString filterName;
    switch (filter) {
        case QI_FILTER_NEAREST:
            filterName = "nearest";
            break;
        case ScalingFilter::QI_FILTER_BILINEAR:
            filterName = "bilinear";
            break;
        case QI_FILTER_CV_BILINEAR_SHARPEN:
            filterName = "bilinear + sharpen";
            break;
        case QI_FILTER_CV_CUBIC:
            filterName = "bicubic";
            break;
        case QI_FILTER_CV_CUBIC_SHARPEN:
            filterName = "bicubic + sharpen";
            break;
        default:
            filterName = "configured " + QString::number(static_cast<int>(filter));
            break;
    }
    showMessage("Filter " + filterName, 600);
    viewerWidget->setScalingFilter(filter);
}

bool MW::isCropPanelActive() {
    return (activeSidePanel == SIDEPANEL_CROP);
}

void MW::onScalingFinished(std::unique_ptr<QPixmap> scaled) {
    viewerWidget->onScalingFinished(std::move(scaled));
}

void MW::saveWindowGeometry() {
    if(this->windowState() == Qt::WindowNoState)
        settings->setWindowGeometry(geometry());
    settings->setMaximizedWindow(maximized);
}

// does not apply fullscreen; window size / maximized state only
void MW::restoreWindowGeometry() {
    if(!settings->autoResizeWindow())
        this->setGeometry(settings->windowGeometry());
    if(settings->maximizedWindow() && !settings->autoResizeWindow()) {
        this->setWindowState(Qt::WindowMaximized);
    } else {
        maximized = false;
    }
    updateCurrentDisplay();
}

void MW::updateCurrentDisplay() {
#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
    currentDisplay = desktopWidget.screenNumber(this);
#else
    auto screens = qApp->screens();
    currentDisplay = screens.indexOf(this->window()->screen());
#endif
}

void MW::onWindowGeometryChanged() {
    saveWindowGeometry();
    updateCurrentDisplay();
}

void MW::saveCurrentDisplay() {
#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
    settings->setLastDisplay(desktopWidget.screenNumber(this));
#else
    settings->setLastDisplay(qApp->screens().indexOf(this->window()->screen()));
#endif
}

//#############################################################
//######################### EVENTS ############################
//#############################################################

void MW::mouseMoveEvent(QMouseEvent *event) {
    event->ignore();
}

bool MW::event(QEvent *event) {
    // only save maximized state if we are already visible
    // this filter out out events while the window is still being set up
    if(event->type() == QEvent::WindowStateChange && this->isVisible() && !this->isFullScreen())
        maximized = isMaximized();
    if(event->type() == QEvent::Move || event->type() == QEvent::Resize)
        windowGeometryChangeTimer.start();
    return QWidget::event(event);
}

// hook up to actionManager
void MW::keyPressEvent(QKeyEvent *event) {
    event->accept();
    actionManager->processEvent(event);
}

void MW::wheelEvent(QWheelEvent *event) {
    event->accept();
    actionManager->processEvent(event);
}

void MW::mousePressEvent(QMouseEvent *event) {
    event->accept();
    actionManager->processEvent(event);
}

void MW::mouseReleaseEvent(QMouseEvent *event) {
    event->accept();
    actionManager->processEvent(event);
}

void MW::mouseDoubleClickEvent(QMouseEvent *event) {
    event->accept();
    QMouseEvent *fakePressEvent = new QMouseEvent(
        QEvent::MouseButtonPress,
        event->pos(),
        event->button(),
        event->buttons(),
        event->modifiers()
    );
    actionManager->processEvent(fakePressEvent);
    actionManager->processEvent(event);
}

void MW::close() {
    saveWindowGeometry();
    saveCurrentDisplay();
    // try to close window sooner
    // since qt6.3 QWidget::close() no longer works on hidden windows (bug?)
#if QT_VERSION < QT_VERSION_CHECK(6, 3, 0)
    this->hide();
#endif
    if(copyOverlay)
        copyOverlay->saveSettings();
    QWidget::close();
}

void MW::closeEvent(QCloseEvent *event) {
    // catch the close event when user presses X on the window itself
    event->accept();
    actionManager->invokeAction("exit");
}

void MW::dragEnterEvent(QDragEnterEvent *e) {
    if(e->mimeData()->hasUrls()) {
        e->acceptProposedAction();
    }
}

void MW::dropEvent(QDropEvent *event) {
    emit droppedIn(event->mimeData(), event->source());
}

void MW::resizeEvent(QResizeEvent *event) {
    if(activeSidePanel == SIDEPANEL_CROP) {
        cropOverlay->setImageScale(viewerWidget->currentScale());
        cropOverlay->setImageDrawRect(viewerWidget->imageRect());
    }
    FloatingWidgetContainer::resizeEvent(event);
}

void MW::showDefault() {
    if(!this->isVisible()) {
        if(settings->fullscreenMode())
            showFullScreen();
        else
            showWindowed();
    }
}

void MW::showSaveDialog(QString filePath) {
    QString newFilePath = getSaveFileName(filePath);
    if(!newFilePath.isEmpty())
        emit saveAsRequested(newFilePath);
}

QString MW::getSaveFileName(QString filePath) {
    docWidget->hideFloatingPanel();
    QStringList filters;
    // generate filter for writable images
    // todo: some may need to be blacklisted
    auto writerFormats = QImageWriter::supportedImageFormats();
    if(writerFormats.contains("jpg"))  filters.append("JPEG (*.jpg *.jpeg *jpe *jfif)");
    if(writerFormats.contains("png"))  filters.append("PNG (*.png)");
    if(writerFormats.contains("webp")) filters.append("WebP (*.webp)");
    // may not work..
    if(writerFormats.contains("jp2"))  filters.append("JPEG 2000 (*.jp2 *.j2k *.jpf *.jpx *.jpm *.jpgx)");
    if(writerFormats.contains("jxl"))  filters.append("JPEG-XL (*.jxl)");
    if(writerFormats.contains("avif")) filters.append("AVIF (*.avif *.avifs)");
    if(writerFormats.contains("tif"))  filters.append("TIFF (*.tif *.tiff)");
    if(writerFormats.contains("bmp"))  filters.append("BMP (*.bmp)");
#ifdef _WIN32
    if(writerFormats.contains("ico"))  filters.append("Icon Files (*.ico)");
#endif
    if(writerFormats.contains("ppm"))  filters.append("PPM (*.ppm)");
    if(writerFormats.contains("xbm"))  filters.append("XBM (*.xbm)");
    if(writerFormats.contains("xpm"))  filters.append("XPM (*.xpm)");
    if(writerFormats.contains("dds"))  filters.append("DDS (*.dds)");
    if(writerFormats.contains("wbmp")) filters.append("WBMP (*.wbmp)");
    // add everything else from imagewriter
    for(auto fmt : writerFormats) {
        if(filters.filter(fmt).isEmpty())
            filters.append(fmt.toUpper() + " (*." + fmt + ")");
    }
    QString filterString = filters.join(";; ");

    // find matching filter for the current image
    QString selectedFilter = "JPEG (*.jpg *.jpeg *jpe *jfif)";
    QFileInfo fi(filePath);
    for(auto filter : filters) {
        if(filter.contains(fi.suffix().toLower())) {
            selectedFilter = filter;
            break;
        }
    }
    QString newFilePath = QFileDialog::getSaveFileName(this, tr("Save File as..."), filePath, filterString, &selectedFilter);
    return newFilePath;
}

void MW::showOpenDialog(QString path) {
    docWidget->hideFloatingPanel();

    QFileDialog dialog(this);
    QStringList imageFilter;
    imageFilter.append(settings->supportedFormatsFilter());
    imageFilter.append("All Files (*)");
    dialog.setDirectory(path);
    dialog.setNameFilters(imageFilter);
    dialog.setWindowTitle("Open image");
    dialog.setWindowModality(Qt::ApplicationModal);
    connect(&dialog, &QFileDialog::fileSelected, this, &MW::opened);
    dialog.exec();
}

void MW::showResizeDialog(QSize initialSize) {
    ResizeDialog dialog(initialSize, this);
    connect(&dialog, &ResizeDialog::sizeSelected, this, &MW::resizeRequested);
    dialog.exec();
}

DialogResult MW::fileReplaceDialog(QString src, QString dst, FileReplaceMode mode, bool multiple) {
    FileReplaceDialog dialog(this);
    dialog.setModal(true);
    dialog.setSource(src);
    dialog.setDestination(dst);
    dialog.setMode(mode);
    dialog.setMulti(multiple);

    dialog.exec();

    return dialog.getResult();
}

void MW::showSettings() {
    docWidget->hideFloatingPanel();
    SettingsDialog settingsDialog(this);
    settingsDialog.exec();
}

void MW::showScriptSettings() {
    docWidget->hideFloatingPanel();
    SettingsDialog settingsDialog(this);
    settingsDialog.switchToPage(4);
    settingsDialog.exec();
}

void MW::triggerFullScreen() {
    if(!isFullScreen()) {
        showFullScreen();
    } else {
        showWindowed();
    }
}

void MW::showFullScreen() {
    //do not save immediately on application start
    if(!isHidden())
        saveWindowGeometry();
    auto screens = qApp->screens();
    // todo: why check the screen again?
#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
    int _currentDisplay = desktopWidget.screenNumber(this);
#else
    int _currentDisplay = screens.indexOf(this->window()->screen());
#endif
    //move to target screen
    if(screens.count() > currentDisplay && currentDisplay != _currentDisplay) {
        this->move(screens.at(currentDisplay)->geometry().x(),
                   screens.at(currentDisplay)->geometry().y());
    }
    QWidget::showFullScreen();
    // try to repaint sooner
    qApp->processEvents();
    emit fullscreenStateChanged(true);
}

void MW::showWindowed() {
    if(isFullScreen())
        QWidget::showNormal();
    restoreWindowGeometry();
    QWidget::show();
    if(!mPendingAutoResizeSize.isEmpty() && settings->autoResizeWindow()) {
        showNormal();
        qApp->processEvents();
        QSize size = mPendingAutoResizeSize;
        mPendingAutoResizeSize = QSize();
        viewerWidget->setFitUpdatesEnabled(false);
        applyAutoResize(size);
        viewerWidget->setFitUpdatesEnabled(true);
        viewerWidget->applyCurrentFitMode();
    }
    viewerWidget->refreshZoomIndicator();
    // try to repaint sooner
    qApp->processEvents();
    emit fullscreenStateChanged(false);
}

void MW::updateCropPanelData() {
    if(cropPanel && activeSidePanel == SIDEPANEL_CROP) {
        cropPanel->setImageRealSize(viewerWidget->sourceSize());
        cropOverlay->setImageDrawRect(viewerWidget->imageRect());
        cropOverlay->setImageScale(viewerWidget->currentScale());
        cropOverlay->setImageRealSize(viewerWidget->sourceSize());
    }
}

void MW::showSaveOverlay() {
    if(!settings->showSaveOverlay())
        return;
    if(!saveOverlay)
        setupSaveOverlay();
    saveOverlay->show();
}

void MW::hideSaveOverlay() {
    if(!saveOverlay)
        return;
    saveOverlay->hide();
}

void MW::showChangelogWindow() {
    changelogWindow->show();
}

void MW::showChangelogWindow(QString text) {
    changelogWindow->setText(text);
    changelogWindow->show();
}

void MW::triggerCropPanel() {
    if(activeSidePanel != SIDEPANEL_CROP) {
        showCropPanel();
    } else {
        hideCropPanel();
    }
}

void MW::showCropPanel() {
    if(centralWidget->currentViewMode() == MODE_FOLDERVIEW)
        return;

    if(activeSidePanel != SIDEPANEL_CROP) {
        docWidget->hideFloatingPanel();
        sidePanel->setWidget(cropPanel);
        sidePanel->show();
        cropOverlay->show();
        activeSidePanel = SIDEPANEL_CROP;
        // reset & lock zoom so CropOverlay won't go crazy
        viewerWidget->fitWindow();
        setInteractionEnabled(false);
        // feed the panel current image info
        updateCropPanelData();
    }
}

void MW::setInteractionEnabled(bool mode) {
    docWidget->setInteractionEnabled(mode);
    viewerWidget->setInteractionEnabled(mode);
}

void MW::hideCropPanel() {
    sidePanel->hide();
    if(activeSidePanel == SIDEPANEL_CROP) {
        cropOverlay->hide();
        setInteractionEnabled(true);
    }
    activeSidePanel = SIDEPANEL_NONE;
}

void MW::triggerCopyOverlay() {
    if(!viewerWidget->isDisplaying())
        return;
    if(!copyOverlay)
        setupCopyOverlay();

    if(centralWidget->currentViewMode() == MODE_FOLDERVIEW)
        return;
    if(copyOverlay->operationMode() == OVERLAY_COPY) {
        copyOverlay->isHidden() ? copyOverlay->show() : copyOverlay->hide();
    } else {
        copyOverlay->setDialogMode(OVERLAY_COPY);
        copyOverlay->show();
    }
}

void MW::triggerMoveOverlay() {
    if(!viewerWidget->isDisplaying())
        return;
    if(!copyOverlay)
        setupCopyOverlay();

    if(centralWidget->currentViewMode() == MODE_FOLDERVIEW)
        return;
    if(copyOverlay->operationMode() == OVERLAY_MOVE) {
        copyOverlay->isHidden() ? copyOverlay->show() : copyOverlay->hide();
    } else {
        copyOverlay->setDialogMode(OVERLAY_MOVE);
        copyOverlay->show();
    }
}

// quit fullscreen or exit the program
void MW::closeFullScreenOrExit() {
    if(this->isFullScreen()) {
        this->showWindowed();
    } else {
        actionManager->invokeAction("exit");
    }
}

// todo: this is crap, use shared state object
void MW::setCurrentInfo(int _index, int _fileCount, QString _filePath, QString _fileName, QSize _imageSize, qint64 _fileSize, bool slideshow, bool shuffle, bool edited) {
    info.index = _index;
    info.fileCount = _fileCount;
    info.fileName = _fileName;
    info.filePath = _filePath;
    info.imageSize = _imageSize;
    info.fileSize = _fileSize;
    info.slideshow = slideshow;
    info.shuffle = shuffle;
    info.edited = edited;
    onInfoUpdated();
}

// todo: nuke and rewrite
void MW::onInfoUpdated() {
    QString posString;
    if(info.fileCount)
        posString = "[ " + QString::number(info.index + 1) + "/" + QString::number(info.fileCount) + " ]";
    QString resString;
    if(info.imageSize.width())
        resString = QString::number(info.imageSize.width()) + " x " + QString::number(info.imageSize.height());
    QString sizeString;
    if(info.fileSize)
        sizeString = this->locale().formattedDataSize(info.fileSize, 1);

    if(renameOverlay)
        renameOverlay->setName(info.fileName);

    QString windowTitle;
    if(centralWidget->currentViewMode() == MODE_FOLDERVIEW) {
        windowTitle = tr("Folder view");
        infoBarFullscreen->setInfo("", tr("No file opened."), "");
        windowedInfoBarOverlay->setInfo("", tr("No file opened."), "");
        infoBarWindowed->setInfo("", tr("No file opened."), "");
    } else if(info.fileName.isEmpty()) {
        windowTitle = qApp->applicationName();
        infoBarFullscreen->setInfo("", tr("No file opened."), "");
        windowedInfoBarOverlay->setInfo("", tr("No file opened."), "");
        infoBarWindowed->setInfo("", tr("No file opened."), "");
    } else {
        windowTitle = info.fileName;
        if(settings->windowTitleExtendedInfo()) {
            windowTitle.prepend(posString + "  ");
            if(!resString.isEmpty())
                windowTitle.append("  -  " + resString);
            if(!sizeString.isEmpty())
                windowTitle.append("  -  " + sizeString);
        }

        // toggleable states
        QString states;
        if(info.slideshow)
            states.append(" [slideshow]");
        if(info.shuffle)
            states.append(" [shuffle]");
        if(viewerWidget->lockZoomEnabled())
            states.append(" [zoom lock]");
        if(viewerWidget->lockViewEnabled())
            states.append(" [view lock]");

        if(!settings->infoBarWindowed() && !states.isEmpty())
            windowTitle.append(" -" + states);
        if(info.edited)
            windowTitle.prepend("* ");

        infoBarFullscreen->setInfo(posString, info.fileName + (info.edited ? "  *" : ""), resString + "  " + sizeString);
        windowedInfoBarOverlay->setInfo(posString, info.fileName + (info.edited ? "  *" : ""), resString + "  " + sizeString);
        infoBarWindowed->setInfo(posString, info.fileName + (info.edited ? "  *" : ""), resString + "  " + sizeString + " " + states);
    }
    setWindowTitle(windowTitle);

    if(!isFullScreen() && settings->windowedInfoBar()) {
        if(info.filePath != mLastWindowedInfoPath) {
            windowedInfoBarOverlay->show();
            mLastWindowedInfoPath = info.filePath;
        }
        if(!settings->windowedInfoBarAlways())
            mWindowedInfoTimer.start();
    }
}

// TODO!!! buffer this in mw
void MW::setExifInfo(QMap<QString, QString> info) {
    if(imageInfoOverlay)
        imageInfoOverlay->setExifInfo(info);
}

std::shared_ptr<FolderViewProxy> MW::getFolderView() {
    return folderView;
}

std::shared_ptr<ThumbnailStripProxy> MW::getThumbnailPanel() {
    return docWidget->thumbPanel();
}

// todo: this is crap
void MW::showMessageDirectory(QString dirName) {
    floatingMessage->showMessage(dirName, FloatingMessageIcon::ICON_DIRECTORY, 1700);
}

void MW::showMessageDirectoryEnd() {
    // TODO replace with something nicer (integrate with click overlay?)
    //floatingMessage->showMessage("", FloatingWidgetPosition::RIGHT, FloatingMessageIcon::ICON_RIGHT_EDGE, 400);
}

void MW::showMessageDirectoryStart() {
    // TODO replace with something nicer (integrate with click overlay?)
    //floatingMessage->showMessage("", FloatingWidgetPosition::LEFT, FloatingMessageIcon::ICON_LEFT_EDGE, 400);
}

void MW::showMessageFitWindow() {
    floatingMessage->showMessage(tr("Fit Window"), FloatingMessageIcon::NO_ICON, 350);
}

void MW::showMessageFitWidth() {
    floatingMessage->showMessage(tr("Fit Width"), FloatingMessageIcon::NO_ICON, 350);
}

void MW::showMessageFitOriginal() {
    floatingMessage->showMessage(tr("Fit 1:1"), FloatingMessageIcon::NO_ICON, 350);
}

void MW::showMessage(QString text) {
    floatingMessage->showMessage(text,  FloatingMessageIcon::NO_ICON, 1500);
}

void MW::showMessage(QString text, int duration) {
    floatingMessage->showMessage(text, FloatingMessageIcon::NO_ICON, duration);
}

void MW::showMessageSuccess(QString text) {
    floatingMessage->showMessage(text,  FloatingMessageIcon::ICON_SUCCESS, 1500);
}

void MW::showWarning(QString text) {
    floatingMessage->showMessage(text,  FloatingMessageIcon::ICON_WARNING, 1500);
}

void MW::showError(QString text) {
    floatingMessage->showMessage(text,  FloatingMessageIcon::ICON_ERROR, 2800);
}

bool MW::showConfirmation(QString title, QString msg) {
    QMessageBox msgBox(this);
    msgBox.setWindowTitle(title);
    msgBox.setText(msg);
    msgBox.setIcon(QMessageBox::Warning);
    msgBox.setStandardButtons(QMessageBox::Yes);
    msgBox.addButton(QMessageBox::No);
    msgBox.setDefaultButton(QMessageBox::Yes);
    msgBox.setModal(true);
    if(msgBox.exec() == QMessageBox::Yes)
        return true;
    else
        return false;
}

void MW::readSettings() {
    showInfoBarFullscreen = settings->infoBarFullscreen();
    showInfoBarWindowed = settings->infoBarWindowed();
    adaptToWindowState();
}

// todo: remove/rename?
void MW::applyWindowedBackground() {
#ifdef USE_KDE_BLUR
    QWindow* window = this->windowHandle();
    if(window) {
        if(settings->backgroundOpacity() == 1.0)
            KWindowEffects::enableBlurBehind(window, false);
        else
            KWindowEffects::enableBlurBehind(window, settings->blurBackground());
    }
#endif
}

void MW::applyFullscreenBackground() {
#ifdef USE_KDE_BLUR
    QWindow* window = this->windowHandle();
    if(window)
        KWindowEffects::enableBlurBehind(window, false);
#endif
}

// changes ui elements according to fullscreen state
void MW::adaptToWindowState() {
    docWidget->hideFloatingPanel();
    if(isFullScreen()) { //-------------------------------------- fullscreen ---
        applyFullscreenBackground();
        infoBarWindowed->hide();
        mWindowedInfoTimer.stop();
        windowedInfoBarOverlay->hide();

        if(showInfoBarFullscreen)
            infoBarFullscreen->showWhenReady();
        else
            infoBarFullscreen->hide();    

        auto pos = settings->panelPosition();
        if(!settings->panelEnabled() || pos == PANEL_BOTTOM || pos == PANEL_LEFT)
            controlsOverlay->show();
        else
            controlsOverlay->hide();
    } else { //------------------------------------------------------ window ---
        applyWindowedBackground();
        infoBarFullscreen->hide();

        windowedInfoBarOverlay->setPosition(infoPositionFromIndex(settings->windowedInfoBarPosition()));
        if(settings->windowedInfoBar()) {
            windowedInfoBarOverlay->show();
            if(settings->windowedInfoBarAlways())
                mWindowedInfoTimer.stop();
            else
                mWindowedInfoTimer.start();
        } else {
            mWindowedInfoTimer.stop();
            windowedInfoBarOverlay->hide();
        }

        if(showInfoBarWindowed)
            infoBarWindowed->show();
        else
            infoBarWindowed->hide();

        controlsOverlay->hide();
    }
    folderView->onFullscreenModeChanged(isFullScreen());
    docWidget->onFullscreenModeChanged(isFullScreen());
    viewerWidget->onFullscreenModeChanged(isFullScreen());
}

void MW::paintEvent(QPaintEvent *event) {
    QPainter p(this);
    p.fillRect(rect(), Qt::black);
    FloatingWidgetContainer::paintEvent(event);
}

void MW::leaveEvent(QEvent *event) {
    QWidget::leaveEvent(event);
    docWidget->hideFloatingPanel(true);
}

// block native tab-switching so we can use it in shortcuts
//bool MW::focusNextPrevChild(bool) {
//    return false;
//}
