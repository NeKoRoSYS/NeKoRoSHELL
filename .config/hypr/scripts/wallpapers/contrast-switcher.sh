#!/usr/bin/env bash
set -euo pipefail

STATE_FILE="${XDG_CACHE_HOME:-$HOME/.cache}/nekoroshell/theme_mode"
SCRIPT_DIR="${XDG_CONFIG_HOME:-$HOME/.config}/hypr/scripts/wallpapers"

if [[ ! -f "$STATE_FILE" ]]; then
    mkdir -p "$(dirname "$STATE_FILE")"
    echo "Dark" > "$STATE_FILE"
fi

CURRENT_MODE=$(cat "$STATE_FILE" 2>/dev/null || echo "Dark")
NEW_MODE=""

if [[ -n "${1:-}" ]]; then
    INPUT="${1,,}" 
    
    if [[ "$INPUT" == "dark" ]]; then
        NEW_MODE="Dark"
    elif [[ "$INPUT" == "light" ]]; then
        NEW_MODE="Light"
    else
        echo "Error: Invalid argument '${1}'. Use 'dark' or 'light'."
        exit 1
    fi
else
    if [[ "$CURRENT_MODE" == "Dark" ]]; then
        NEW_MODE="Light"
    else
        NEW_MODE="Dark"
    fi
fi

echo "$NEW_MODE" > "$STATE_FILE"
bash "$SCRIPT_DIR/apply-colors.sh" "" "$NEW_MODE" >/dev/null
