#!/usr/bin/env bash
set -euo pipefail

THUMB_CACHE="${XDG_CACHE_HOME:-$HOME/.cache}/nekoroshell/wallpaper-thumbs"
STATE_FILE="${XDG_CACHE_HOME:-$HOME/.cache}/nekoroshell/theme_mode"
WAYBAR_MODE_FILE="${XDG_CACHE_HOME:-$HOME/.cache}/nekoroshell/navbar_mode"
waybar_colors="${XDG_CACHE_HOME:-$HOME/.cache}/wallust/colors-waybar.css"
rofi_colors="${XDG_CACHE_HOME:-$HOME/.cache}/wallust/colors-rofi.rasi"

MANAGEMENT_MODE=$(cat "$WAYBAR_MODE_FILE" 2>/dev/null || echo "static")

img_path="${1:-$(cat "${XDG_CACHE_HOME:-$HOME/.cache}/wallust/wal" 2>/dev/null || echo "")}"
current_theme="${2:-$(cat "$STATE_FILE" 2>/dev/null || echo "Dark")}"

[[ -z "$img_path" ]] && exit 0

FILENAME=$(basename "$img_path")
THUMB_PATH="$THUMB_CACHE/${FILENAME}.jpg"

TARGET_IMG="$img_path"
[[ -f "$THUMB_PATH" ]] && TARGET_IMG="$THUMB_PATH"

if command -v wallust >/dev/null 2>&1; then
    if [ "$current_theme" = "Dark" ]; then
        wallust run "$TARGET_IMG" -q -C ~/.config/wallust/wallust-dark.toml || wallust run "$TARGET_IMG" -q -C ~/.config/wallust/wallust-dark.toml -b full -t 5
        printf "@define-color text #F5F5F5;\n@define-color text-invert #121212;\n" >> "$waybar_colors"
        echo "* { text: #F5F5F5; text-invert: #121212; }" >> "$rofi_colors"
    else
        wallust run "$TARGET_IMG" -q -C ~/.config/wallust/wallust-light.toml || wallust run "$TARGET_IMG" -q -C ~/.config/wallust/wallust-light.toml -b full -t 5
        printf "@define-color text-invert #F5F5F5;\n@define-color text #121212;\n" >> "$waybar_colors"
        echo "* { text: #121212; text-invert: #F5F5F5; }" >> "$rofi_colors"
    fi

    mv ~/.cache/wallust/colors-hyprland-raw.conf ~/.cache/wallust/colors-hyprland.conf

    killall -q navbar-hover navbar-watcher waybar 2>/dev/null || true
    swaync-client -rs 2>/dev/null || true

    case "$MANAGEMENT_MODE" in
        "static") waybar & ;;
        "hover")  navbar-hover & ;;
        *)        navbar-watcher & ;;
    esac
fi
