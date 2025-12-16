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
sudo cp engine.py hangul.py "$DEST_DIR/"
sudo chmod +x "$DEST_DIR/engine.py"

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
echo "Please restart IBus (ibus restart) and add 'DINKIssTyle' from Input Method settings."
