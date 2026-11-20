#!/bin/bash
set -euo pipefail

SRC=/home/luxxanna/Documents/__WIP__/_MyOwn_Projects_/__qimgv_fork__
BUILD_DIR="$SRC/build-native"

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

echo "Installing /usr/bin/qimgv wrapper..."
cat > /usr/bin/qimgv <<'EOF'
#!/bin/sh
export QT_QPA_PLATFORM=xcb
exec /usr/lib/qimgv/qimgv "$@"
EOF
chmod 755 /usr/bin/qimgv

echo "Done."
echo "Verify with: qimgv --help"
