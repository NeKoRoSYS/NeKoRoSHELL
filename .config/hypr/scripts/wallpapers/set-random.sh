#!/bin/bash

choose_random="${XDG_CONFIG_HOME:-$HOME/.config}/hypr/scripts/wallpapers/random.sh"
${XDG_CONFIG_HOME:-$HOME/.config}/hypr/scripts/wallpapers/set-wallpaper.sh $($choose_random)
