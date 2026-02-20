#!/bin/bash

WALLPAPER=$(swww query | grep -oP '/.*\.(jpg|png|jpeg|webp)' | head -n 1)
CONF="$HOME/.config/hypr/hyprlock.conf"

if [ -n "$WALLPAPER" ]; then
    TARGET_SKIN=$(grep "^source =" "$CONF" | cut -d '=' -f2 | tr -d ' ')

    if [ -n "$TARGET_SKIN" ] && [ -f "$TARGET_SKIN" ]; then
        sed -i "/^background {/,/^}/{s|^[[:space:]]*path =.*|    path = $WALLPAPER|}" "$TARGET_SKIN"
    else
        sed -i "/^background {/,/^}/{s|^[[:space:]]*path =.*|    path = $WALLPAPER|}" "$CONF"
    fi
fi
