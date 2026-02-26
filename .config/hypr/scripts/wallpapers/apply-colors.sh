#!/bin/bash

THUMB_CACHE="${XDG_CACHE_HOME:-$HOME/.cache}/nekoroshell/wallpaper-thumbs"
STATE_FILE="${XDG_CACHE_HOME:-$HOME/.cache}/nekoroshell/theme_mode"
WAYBAR_MODE_FILE="${XDG_CACHE_HOME:-$HOME/.cache}/nekoroshell/navbar_mode"
waybar_colors="${XDG_CACHE_HOME:-$HOME/.cache}/wallust/colors-waybar.css"
rofi_colors="${XDG_CACHE_HOME:-$HOME/.cache}/wallust/colors-rofi.rasi"

MANAGEMENT_MODE=$(cat "$WAYBAR_MODE_FILE" 2>/dev/null || echo "static")

img_path="${1:-$(cat "${XDG_CACHE_HOME:-$HOME/.cache}/wallust/wal")}"
current_theme="${2:-$(cat "$STATE_FILE")}"

FILENAME=$(basename "$img_path")
THUMB_PATH="$THUMB_CACHE/${FILENAME}.jpg"

if [ -f "$THUMB_PATH" ]; then
    TARGET_IMG="$THUMB_PATH"
else
    TARGET_IMG="$img_path"
fi

PARAMS="$TARGET_IMG -q -C"

if command -v wallust >/dev/null 2>&1; then
    echo "Updating system theme using: $(basename "$TARGET_IMG")"
	killall -q navbar-hover navbar-watcher 2>/dev/null || true
	killall -q waybar 2>/dev/null || true

	if [ "$current_theme" = "Dark" ]; then
		wallust run "$TARGET_IMG" -q -C ~/.config/wallust/wallust-dark.toml || wallust run $TARGET_IMG -q -C ~/.config/wallust/wallust-dark.toml -b full -t 5
		printf "@define-color text #F5F5F5;\n@define-color text-invert #121212;\n" >> "$waybar_colors"
		echo "* { text: #F5F5F5; text-invert: #121212; }" >> "$rofi_colors"
	else
		wallust run "$TARGET_IMG" -q -C ~/.config/wallust/wallust-light.toml || wallust run $TARGET_IMG -q -C ~/.config/wallust/wallust-light.toml -b full -t 5
		printf "@define-color text-invert #F5F5F5;\n@define-color text #121212;\n" >> "$waybar_colors"
		echo "* { text: #121212; text-invert: #F5F5F5; }" >> "$rofi_colors"
	fi

	mv ~/.cache/wallust/colors-hyprland-raw.conf ~/.cache/wallust/colors-hyprland.conf

	swaync-client -rs

	case "$MANAGEMENT_MODE" in
	"static")
		waybar &
		;;
	"hover")
		navbar-hover &
		;;
	*)
		navbar-watcher &
		;;
	esac
fi
