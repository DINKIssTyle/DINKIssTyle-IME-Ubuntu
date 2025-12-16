#!/bin/bash
set -e

PACKAGE_NAME="ibus-dkst"
VERSION="1.0"
ARCH="amd64"
DIR_NAME="${PACKAGE_NAME}_${VERSION}_${ARCH}"

echo "Starting build process for $PACKAGE_NAME..."

# Clean up previous build
rm -rf "$DIR_NAME"
rm -f "${DIR_NAME}.deb"

# 1. Prepare Directory Structure
echo "Creating directory structure..."
mkdir -p "$DIR_NAME/DEBIAN"
mkdir -p "$DIR_NAME/usr/share/ibus-dkst"
mkdir -p "$DIR_NAME/usr/share/ibus/component"

# 2. Compile sources
echo "Compiling..."
make clean
if ! make; then
    echo "Build failed!"
    exit 1
fi

# 3. Copy Files
echo "Copying files..."
# Rename binary and XML to dkst for consistency inside the package
cp dkst-ime "$DIR_NAME/usr/share/ibus-dkst/dkst-ime"
cp setup.py "$DIR_NAME/usr/share/ibus-dkst/"
cp icon.png "$DIR_NAME/usr/share/ibus-dkst/"
# Copy dummy config for reference
cp config.ini "$DIR_NAME/usr/share/ibus-dkst/" 

# Copy Component XML
cp dkst.xml "$DIR_NAME/usr/share/ibus/component/dkst.xml"

# Set permissions
chmod 755 "$DIR_NAME/usr/share/ibus-dkst/dkst-ime"
chmod 755 "$DIR_NAME/usr/share/ibus-dkst/setup.py"
chmod 644 "$DIR_NAME/usr/share/ibus-dkst/icon.png"
chmod 644 "$DIR_NAME/usr/share/ibus-dkst/config.ini"
chmod 644 "$DIR_NAME/usr/share/ibus/component/dkst.xml"

# 4. Create Control File
echo "Creating control file..."
cat > "$DIR_NAME/DEBIAN/control" <<EOF
Package: $PACKAGE_NAME
Version: $VERSION
Section: utils
Priority: optional
Architecture: $ARCH
Depends: ibus, python3, python3-gi, gir1.2-gtk-3.0
Maintainer: Dinki <dinki@example.com>
Description: DKST Korean Input Method
 A custom Korean Input Method Engine (IME) for IBus.
 Features experimental hangul composition and customizable key bindings.
EOF

# 5. Build .deb
echo "Building package..."
dpkg-deb --build "$DIR_NAME"

echo "Done! Package created: ${DIR_NAME}.deb"
