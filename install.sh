#!/bin/bash
set -euo pipefail

DEST_DIR="/usr/share/ibus-dkst"
COMPONENT_DIR="/usr/share/ibus/component"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

REAL_USER="${SUDO_USER:-$(id -un)}"
USER_HOME="$(getent passwd "$REAL_USER" | cut -d: -f6)"
REAL_UID="$(id -u "$REAL_USER")"
USER_CONFIG_DIR="$USER_HOME/.config/ibus-dkst"

run_as_root() {
    if [ "$(id -u)" -eq 0 ]; then
        "$@"
    else
        sudo "$@"
    fi
}

run_as_user() {
    if [ "$(id -u)" -eq "$REAL_UID" ]; then
        "$@"
    else
        sudo -u "$REAL_USER" \
            XDG_RUNTIME_DIR="/run/user/$REAL_UID" \
            DBUS_SESSION_BUS_ADDRESS="unix:path=/run/user/$REAL_UID/bus" \
            "$@"
    fi
}

install_build_dependencies() {
    if ! command -v apt-get >/dev/null 2>&1; then
        echo "Missing build dependencies, and apt-get was not found."
        echo "Please install: build-essential pkg-config libibus-1.0-dev libglib2.0-dev"
        exit 1
    fi

    echo "Installing missing build dependencies..."
    run_as_root apt-get update
    run_as_root apt-get install -y \
        build-essential \
        pkg-config \
        libibus-1.0-dev \
        libglib2.0-dev
}

check_build_dependencies() {
    local missing=0

    if ! command -v gcc >/dev/null 2>&1; then
        echo "Missing command: gcc"
        missing=1
    fi

    if ! command -v make >/dev/null 2>&1; then
        echo "Missing command: make"
        missing=1
    fi

    if ! command -v pkg-config >/dev/null 2>&1; then
        echo "Missing command: pkg-config"
        missing=1
    elif ! pkg-config --exists ibus-1.0 glib-2.0; then
        echo "Missing pkg-config packages: ibus-1.0 and/or glib-2.0"
        missing=1
    fi

    if [ "$missing" -eq 0 ]; then
        return
    fi

    if [ -t 0 ]; then
        read -r -p "Install required build packages now? [Y/n] " answer
        case "$answer" in
            [nN]|[nN][oO])
                echo "Aborted. Please install: build-essential pkg-config libibus-1.0-dev libglib2.0-dev"
                exit 1
                ;;
        esac
        install_build_dependencies
    else
        echo "Please install: build-essential pkg-config libibus-1.0-dev libglib2.0-dev"
        exit 1
    fi
}

echo "Installing DKST IME..."

# Build with Make
check_build_dependencies
echo "Building..."
make clean
make

echo "Stopping existing process if running..."
run_as_root pkill -x dinkisstyle-ime || true
run_as_root pkill -x dkst-ime || true

echo "Installing files..."
run_as_root install -d "$DEST_DIR"
run_as_root install -d "$COMPONENT_DIR"
run_as_root install -m 755 dkst-ime "$DEST_DIR/dkst-ime"
run_as_root install -m 755 setup.py "$DEST_DIR/setup.py"
run_as_root install -m 755 hanja_editor.py "$DEST_DIR/hanja_editor.py"
run_as_root install -m 644 hanja.txt "$DEST_DIR/hanja.txt"
run_as_root install -m 644 icon.png "$DEST_DIR/icon.png"

run_as_root install -m 644 config.ini "$DEST_DIR/config.ini"

# Copy Component XML
echo "Registering component..."
run_as_root install -m 644 dkst.xml "$COMPONENT_DIR/dkst.xml"

# User Config
echo "Setting up user configuration..."
run_as_root install -d "$USER_CONFIG_DIR"
if [ ! -f "$USER_CONFIG_DIR/config.ini" ]; then
    run_as_root install -m 644 config.ini "$USER_CONFIG_DIR/config.ini"
    echo "Created default config at $USER_CONFIG_DIR/config.ini"
fi

# Fix ownership
run_as_root chown -R "$REAL_USER:$REAL_USER" "$USER_CONFIG_DIR"

echo "Installation Complete."
echo "Restarting IBus..."
if run_as_user ibus restart; then
    echo "IBus restarted."
else
    echo "Could not restart IBus automatically. Please run 'ibus restart' in your desktop session."
fi
echo "Please add 'DKST' from Input Method settings if not already added."
