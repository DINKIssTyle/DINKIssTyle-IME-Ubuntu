#!/bin/bash

DEST_DIR="/usr/share/ibus-dinkisstyle"
COMPONENT_DIR="/usr/share/ibus/component"
USER_CONFIG_DIR="$HOME/.config/dinkisstyle"

echo "Installing DINKIssTyle IME..."

# Create destination directory
if [ ! -d "$DEST_DIR" ]; then
    echo "Creating $DEST_DIR..."
    sudo mkdir -p "$DEST_DIR"
fi

# Copy Source Files
echo "Copying scripts..."
# Build with Make
echo "Building..."
if ! make; then
    echo "Build failed!"
    exit 1
fi

# Copy Binary
echo "Copying binary..."
echo "Stopping existing process if running..."
sudo pkill -f dinkisstyle-ime || true
sudo cp dinkisstyle-ime "$DEST_DIR/"
sudo chmod +x "$DEST_DIR/dinkisstyle-ime"

# Copy Icon (if exists, creating dummy if not for now)
# sudo cp icon.png "$DEST_DIR/" 

# Copy Component XML
echo "Registering component..."
sudo cp dinkisstyle.xml "$COMPONENT_DIR/"

# User Config
echo "Setting up user configuration..."
mkdir -p "$USER_CONFIG_DIR"
if [ ! -f "$USER_CONFIG_DIR/config.json" ]; then
    cp config.json "$USER_CONFIG_DIR/"
    echo "Created default config at $USER_CONFIG_DIR/config.json"
fi

echo "Installation Complete."
echo "Restarting IBus..."
ibus restart
echo "IBus restarted. Please add 'DINKIssTyle' from Input Method settings if not already added."
