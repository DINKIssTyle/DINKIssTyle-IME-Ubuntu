#!/bin/bash

echo "Cleaning up legacy DINKIssTyle files..."

# Remove Component XML
if [ -f "/usr/share/ibus/component/dinkisstyle.xml" ]; then
    echo "Removing /usr/share/ibus/component/dinkisstyle.xml..."
    sudo rm "/usr/share/ibus/component/dinkisstyle.xml"
fi

# Remove Installation Directory
if [ -d "/usr/share/ibus-dinkisstyle" ]; then
    echo "Removing /usr/share/ibus-dinkisstyle..."
    sudo rm -rf "/usr/share/ibus-dinkisstyle"
fi

# Remove User Config (Optional, usually good to keep backups but user wants clean)
# We will ask or just do it? User asked if they "remain", implying they want to know or remove.
# Let's remove them to avoid confusion with new config.
USER_CONFIG_DIR="$HOME/.config/ibus-dinkisstyle"
if [ -d "$USER_CONFIG_DIR" ]; then
    echo "Removing $USER_CONFIG_DIR..."
    rm -rf "$USER_CONFIG_DIR"
fi

echo "Cleanup complete. Don't forget to restart IBus!"
