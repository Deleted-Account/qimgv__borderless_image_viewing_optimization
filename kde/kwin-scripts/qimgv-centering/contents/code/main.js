// qimgv-centering
// Wayland 下客户端不能移动已映射的顶层窗口，qimgv 的“切图后窗口居中”
// 只能由合成器完成。本脚本在 qimgv 主窗口尺寸变化（即切换图片）后，
// 把窗口中心对齐到它所在屏幕的工作区中心。
// 只在尺寸变化时动作，用户手动拖动窗口（只改位置）不会被抢回去。

var APP_ID = "qimgv";

// internalId -> 上次看到的尺寸；internalId -> 是否已连接信号
var lastSizes = {};
var watched = {};

function isQimgv(win) {
    if (!win || win.deleted)
        return false;
    // 跳过对话框、弹窗等临时窗口，只处理主窗口
    if (win.transient === true || win.transientFor || win.modal === true)
        return false;
    if (!win.normalWindow)
        return false;
    var rc = String(win.resourceClass || "").toLowerCase();
    var rn = String(win.resourceName || "").toLowerCase();
    var df = String(win.desktopFileName || "").toLowerCase();
    return rc === APP_ID || rn === APP_ID || df.indexOf(APP_ID) === 0;
}

function workArea(win) {
    if (typeof KWin.WorkArea !== "undefined")
        return workspace.clientArea(KWin.WorkArea, win);
    return workspace.clientArea(KWin.MaximizeArea, win);
}

function onGeometryChanged(win) {
    if (!isQimgv(win))
        return;
    if (win.fullScreen || win.minimized)
        return;
    // maximizeMode: 3 = 水平+垂直都最大化
    if (win.maximizeMode === 3)
        return;
    // 平铺/贴边状态下不抢
    var tile = win.tile;
    if (tile && (tile.width > 0 || tile.height > 0))
        return;

    var geo = win.frameGeometry;
    var key = Math.round(geo.width) + "x" + Math.round(geo.height);
    var id = String(win.internalId || "");
    if (lastSizes[id] === key)
        return;              // 尺寸没变：用户在拖动窗口，不干预
    lastSizes[id] = key;

    var area = workArea(win);
    var x = Math.round(area.x + (area.width - geo.width) / 2);
    var y = Math.round(area.y + (area.height - geo.height) / 2);
    if (Math.abs(x - geo.x) < 1 && Math.abs(y - geo.y) < 1)
        return;

    var target = {
        x: x,
        y: y,
        width: Math.round(geo.width),
        height: Math.round(geo.height)
    };
    // 改尺寸与移动都在下一帧绘制前完成；临时透明可避免看到旧位置的中间帧
    var opacity = win.opacity;
    win.opacity = 0;
    win.frameGeometry = target;
    win.opacity = opacity;
}

function setup(win) {
    var id = String(win.internalId || "");
    if (!id || watched[id])
        return;
    watched[id] = true;
    win.frameGeometryChanged.connect(function () {
        onGeometryChanged(win);
    });
}

workspace.windowAdded.connect(setup);
workspace.windowList().forEach(setup);
