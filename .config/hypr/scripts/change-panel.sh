#!/usr/bin/env bash

pkill -x wofi

SKIN_DIR="$HOME/.config/swaync/skins"
MAIN_CONFIG="$HOME/.config/swaync/config.json"
MAIN_STYLE="$HOME/.config/swaync/style.css"

makenotif swaync preferences-desktop-theme SwayNC "Select a SwayNC style." true

CHOICE=$(ls "$SKIN_DIR" | wofi --dmenu --prompt "Select SwayNC Skin")

if [ -n "$CHOICE" ]; then
    SELECTED_LAYOUT="$SKIN_DIR/$CHOICE/config.json"
    SELECTED_STYLE="$SKIN_DIR/$CHOICE/style.css"

    cp "$SELECTED_LAYOUT" "$MAIN_CONFIG"
    
    echo "@import \"$SELECTED_STYLE\";" > "$MAIN_STYLE"

    swaync-client -R
    swaync-client -rs

    makenotif swaync preferences-desktop-theme SwayNC "Style changed to: $CHOICE" true
fi
