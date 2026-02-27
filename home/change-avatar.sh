#!/bin/bash

# Target path using the $HOME variable
TARGET="$HOME/.face.icon"

# Check if an image path was provided
if [ -z "$1" ]; then
    echo "Usage: $0 /path/to/image.png"
    exit 1
fi

# Verify the source file exists
if [ ! -f "$1" ]; then
    echo "Error: File '$1' not found."
    exit 1
fi

# Copy image to ~/.face.icon
cp "$1" "$TARGET"
chmod 644 "$TARGET"

echo "Done! Icon updated at $TARGET"
