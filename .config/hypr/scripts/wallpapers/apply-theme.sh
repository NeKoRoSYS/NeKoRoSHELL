#!/bin/bash

img_path="${1:-$(cat "$HOME/.cache/wal/wal")}"
current_theme="${2:-$(cat "$HOME/.cache/theme_mode")}"
waybar="$HOME/.cache/wal/colors-waybar.css"
if command -v wal >/dev/null 2>&1; then
	echo "Updating system theme..."
	rm -f "$HOME/.cache/wal/schemes/*"
	sed -i '/^@define-color text/d' "$waybar"
	sed -i '/^@define-color text-invert/d' "$waybar"
	sleep 0.05
	if [ "$current_theme" = "Dark" ]; then
		wal -i "$img_path" --backend haishoku -n -q || \
		wal -i "$img_path" --backend colorthief -n -q || \
		wal -i "$img_path" -n -q # Final fallback to default backend
		echo "@define-color text #F5F5F5;" >> $waybar
		echo "@define-color text-invert #121212;" >> $waybar	
	else
		wal -l -i "$img_path" --backend haishoku -n -q || \
		wal -l -i "$img_path" --backend colorthief -n -q || \
		wal -l -i "$img_path" -n -q # Final fallback to default backend
		echo "@define-color text-invert #F5F5F5;" >> $waybar	
		echo "@define-color text #121212;" >> $waybar
	fi
	killall waybar && sleep 0.05
	systemctl --user restart waybar &
        swaync-client -rs
        #"$HOME/.config/mako/update-colors.sh"
fi


