#!/bin/bash

WALLPAPERS_DIR="$XDG_CONFIG_HOME/wallpapers"
NEW_WALL=$(find $WALLPAPERS_DIR -type f | shuf -n 1)

echo $NEW_WALL
