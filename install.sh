#!/bin/bash

DEST_DIR="/usr/share/ibus-dkst"
COMPONENT_DIR="/usr/share/ibus/component"
USER_CONFIG_DIR="$HOME/.config/ibus-dkst"

echo "Installing DKST IME..."

# Create destination directory
if [ ! -d "$DEST_DIR" ]; then
    echo "Creating $DEST_DIR..."
    sudo mkdir -p "$DEST_DIR"
fi

# Copy Source Files
echo "Copying scripts..."
# Build with Make
echo "Building..."
make clean
if ! make; then
    echo "Build failed!"
    exit 1
fi

# Copy Binary
echo "Copying binary..."
echo "Stopping existing process if running..."
sudo pkill -f dinkisstyle-ime || true
sudo pkill -f dkst-ime || true

sudo cp dkst-ime "$DEST_DIR/"
sudo cp setup.py "$DEST_DIR/"
sudo cp icon.png "$DEST_DIR/"
sudo chmod +x "$DEST_DIR/dkst-ime"
sudo chmod +x "$DEST_DIR/setup.py"

# Copy Component XML
echo "Registering component..."
sudo cp dkst.xml "$COMPONENT_DIR/"

# User Config
echo "Setting up user configuration..."
REAL_USER=${SUDO_USER:-$USER}
USER_HOME=$(getent passwd "$REAL_USER" | cut -d: -f6)
USER_CONFIG_DIR="$USER_HOME/.config/ibus-dkst"

sudo mkdir -p "$USER_CONFIG_DIR"
if [ ! -f "$USER_CONFIG_DIR/config.ini" ]; then
    sudo cp config.ini "$USER_CONFIG_DIR/"
    echo "Created default config at $USER_CONFIG_DIR/config.ini"
fi

# Fix ownership
sudo chown -R "$REAL_USER":"$REAL_USER" "$USER_CONFIG_DIR"


echo "Installation Complete."
echo "Restarting IBus..."
ibus restart
echo "IBus restarted. Please add 'DKST' from Input Method settings if not already added."
