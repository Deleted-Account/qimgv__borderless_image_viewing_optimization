#!/bin/bash
set -euo pipefail

SRC=/home/luxxanna/Documents/__WIP__/_MyOwn_Projects_/__qimgv_fork__
BUILD_DIR="$SRC/build-native"
KWIN_SCRIPT_ID="qimgv-centering"
KWIN_SCRIPT_SRC="$SRC/kde/kwin-scripts/$KWIN_SCRIPT_ID"

USE_XWAYLAND=0
for arg in "$@"; do
    case "$arg" in
        --xwayland)
            USE_XWAYLAND=1
            ;;
        -h|--help)
            echo "Usage: sudo ./deploy-system.sh [--xwayland]"
            echo "  (default)   native Wayland + KWin centering script"
            echo "  --xwayland  run qimgv through XWayland (QT_QPA_PLATFORM=xcb)"
            exit 0
            ;;
        *)
            echo "Unknown option: $arg" >&2
            exit 1
            ;;
    esac
done

if [ "$(id -u)" -ne 0 ]; then
    echo "This script must be run with sudo." >&2
    exit 1
fi

echo "Installing qimgv binary..."
install -Dm755 "$BUILD_DIR/qimgv/qimgv" /usr/lib/qimgv/qimgv

echo "Installing mpv video plugin..."
install -Dm755 "$BUILD_DIR/plugins/player_mpv/player_mpv.so" /usr/lib64/qimgv/player_mpv.so
[ -d /usr/lib/qimgv ] && ln -sf /usr/lib64/qimgv/player_mpv.so /usr/lib/qimgv/player_mpv.so || true

echo "Installing translations..."
install -d /usr/local/share/qimgv/translations
cp -f "$BUILD_DIR"/qimgv/translations/*.qm /usr/local/share/qimgv/translations/

if [ "$USE_XWAYLAND" -eq 1 ]; then
    echo "Installing /usr/bin/qimgv wrapper (XWayland)..."
    cat > /usr/bin/qimgv <<'EOF'
#!/bin/sh
export QT_QPA_PLATFORM=xcb
exec /usr/lib/qimgv/qimgv "$@"
EOF
else
    echo "Installing /usr/bin/qimgv wrapper (native Wayland)..."
    cat > /usr/bin/qimgv <<'EOF'
#!/bin/sh
exec /usr/lib/qimgv/qimgv "$@"
EOF
fi
chmod 755 /usr/bin/qimgv

echo "Installing KWin centering script..."
# 不同发行版的系统级 KWin 脚本目录不同，存在的都装一份
KWIN_SCRIPT_DIRS=(/usr/share/kwin/scripts)
[ -d /usr/share/kwin-wayland/scripts ] && KWIN_SCRIPT_DIRS+=(/usr/share/kwin-wayland/scripts)
for d in "${KWIN_SCRIPT_DIRS[@]}"; do
    install -d "$d/$KWIN_SCRIPT_ID/contents/code"
    install -m644 "$KWIN_SCRIPT_SRC/metadata.json" "$d/$KWIN_SCRIPT_ID/metadata.json"
    install -m644 "$KWIN_SCRIPT_SRC/contents/code/main.js" "$d/$KWIN_SCRIPT_ID/contents/code/main.js"
done

# 以桌面用户身份写入 kwinrc 并让 KWin 重新加载
if [ -n "${SUDO_USER:-}" ] && [ "$SUDO_USER" != "root" ]; then
    USER_UID=$(id -u "$SUDO_USER")
    USER_HOME=$(getent passwd "$SUDO_USER" | cut -d: -f6)
    RUN_AS=(sudo -u "$SUDO_USER" env "HOME=$USER_HOME"
            "XDG_RUNTIME_DIR=/run/user/$USER_UID"
            "DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/$USER_UID/bus")

    if [ "$USE_XWAYLAND" -eq 1 ]; then
        KWIN_ENABLED=false
    else
        KWIN_ENABLED=true
    fi

    if command -v kwriteconfig6 >/dev/null 2>&1; then
        # 删除用户级同名脚本，避免遮蔽系统副本
        "${RUN_AS[@]}" rm -rf "$USER_HOME/.local/share/kwin/scripts/$KWIN_SCRIPT_ID" || true
        "${RUN_AS[@]}" kwriteconfig6 --file kwinrc --group Plugins \
            --key "${KWIN_SCRIPT_ID}Enabled" "$KWIN_ENABLED" || true
        if command -v qdbus >/dev/null 2>&1; then
            "${RUN_AS[@]}" qdbus org.kde.KWin /KWin org.kde.KWin.reconfigure || true
        elif command -v qdbus6 >/dev/null 2>&1; then
            "${RUN_AS[@]}" qdbus6 org.kde.KWin /KWin org.kde.KWin.reconfigure || true
        fi
        echo "KWin script '$KWIN_SCRIPT_ID' enabled=$KWIN_ENABLED for $SUDO_USER."
    else
        echo "kwriteconfig6 not found."
        echo "Enable '$KWIN_SCRIPT_ID' manually: System Settings > Window Management > KWin Scripts."
    fi
fi

echo "Done."
echo "Verify with: qimgv --help"
