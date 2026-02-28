#!/bin/bash
set -e
set -o pipefail

GREEN='\033[0;32m'
BLUE='\033[0;34m'
RED='\033[0;31m'
NC='\033[0m'

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" &> /dev/null && pwd)"
cd "$SCRIPT_DIR" || { echo -e "${RED}Failed to navigate to script directory.${NC}"; exit 1; }

XDG_CONFIG_HOME="${XDG_CONFIG_HOME:-$HOME/.config}"
XDG_DATA_HOME="${XDG_DATA_HOME:-$HOME/.local/share}"
XDG_STATE_HOME="${XDG_STATE_HOME:-$HOME/.local/state}"
XDG_CACHE_HOME="${XDG_CACHE_HOME:-$HOME/.cache}"

DRY_RUN=0
for arg in "$@"; do
    if [[ "$arg" == "--dry-run" || "$arg" == "-d" ]]; then
        DRY_RUN=1
        echo -e "${BLUE}============ DRY-RUN MODE ============${NC}"
        echo -e "${GREEN}No files will be modified or copied.${NC}"
        echo -e "${BLUE}======================================${NC}\n"
    fi
done

execute() {
    if [[ "$DRY_RUN" -eq 1 ]]; then
        echo -e "  ${BLUE}[DRY-RUN]${NC} Would execute: $*"
    else
        "$@"
    fi
}

echo -e "# ======================================================= #"
echo -e "#            NeKoRoSHELL Installation Wizard              #"
echo -e "# ======================================================= #\n "

# ==============================================================================
# ACTIVE BIN DIRECTORY DETECTION
# ==============================================================================

echo -e "${BLUE}Detecting active user bin directory...${NC}"
if [[ -d "$HOME/.local/bin" ]]; then
    BASE_BIN_DIR="$HOME/.local/bin"
elif [[ -d "$HOME/bin" ]]; then
    BASE_BIN_DIR="$HOME/bin"
else
    BASE_BIN_DIR="$HOME/.local/bin"
fi

USER_BIN_DIR="$BASE_BIN_DIR/nekoroshell"

echo -e "${GREEN}Using $USER_BIN_DIR as the target bin directory.${NC}\n"

echo -e "${BLUE}Please choose your installation type:${NC}"
echo -e "  ${GREEN}Minimal${NC}     - Backup existing config files as a tarball, deploy new dotfiles, and replace hardcoded directories. No dependencies."
echo -e "  ${GREEN}Compilation${NC} - Backup existing config files, deploy dotfiles, replace hardcoded directories, and install every dependency.\n"

INSTALL_TYPE=""
while true; do
    echo -ne "${BLUE}Type 'Minimal' or 'Compilation' to proceed (or 'exit' to abort): ${NC}"
    read -r choice
    choice="${choice,,}"

    if [[ "$choice" == "minimal" ]]; then
        INSTALL_TYPE="minimal"
        echo -e "${GREEN}Minimal installation selected.${NC}"
        break
    elif [[ "$choice" == "compilation" ]]; then
        INSTALL_TYPE="compilation"
        echo -e "${GREEN}Compilation installation selected.${NC}"
        
        echo -e "${BLUE}Caching sudo credentials for dependency installation...${NC}"
        sudo -v
        exec 9> >(
            while true; do
                read -r -t 60
                status=$?
                if [[ $status -gt 128 ]]; then
                    sudo -n true 2>/dev/null
                else
                    break
                fi
            done
        )
        break
    elif [[ "$choice" == "exit" ]]; then
        echo -e "${RED}Installation aborted.${NC}"
        exit 0
    else
        echo -e "${RED}Invalid input. Please type 'Minimal' or 'Compilation'.${NC}"
    fi
done

echo -e "${BLUE}Starting $INSTALL_TYPE installation...${NC}"

# ==============================================================================
# DEPENDENCY INSTALLATION (Compilation Mode Only)
# ==============================================================================

detect_and_bootstrap() {
    source /etc/os-release
    echo "Detected OS: $ID"
    
    if [ "$ID" = "arch" ] || [ "$ID_LIKE" = "arch" ]; then
        echo "Bootstrapping Arch dependencies..."
        execute sudo pacman -Syu --needed --noconfirm base-devel git cargo go flatpak
        if ! command -v yay &> /dev/null; then
            if [[ "$DRY_RUN" -eq 1 ]]; then
                echo -e "  ${BLUE}[DRY-RUN]${NC} Would clone and install yay from AUR"
            else
                git clone https://aur.archlinux.org/yay.git /tmp/yay && cd /tmp/yay && makepkg -si --noconfirm && cd "$SCRIPT_DIR"
            fi
        fi
    elif [ "$ID" = "fedora" ]; then
        echo "Bootstrapping Fedora dependencies..."
        execute sudo dnf install -y @development-tools git cargo golang flatpak
    elif [ "$ID" = "debian" ] || [ "$ID" = "ubuntu" ]; then
        echo "Bootstrapping Debian dependencies..."
        execute sudo apt update
        execute sudo apt install -y build-essential git cargo golang flatpak
    else
        echo "Unsupported OS for automatic bootstrap. Please install prerequisites manually."
    fi
}

if [[ "$INSTALL_TYPE" == "compilation" ]]; then
    detect_and_bootstrap

    echo -e "${BLUE}Detecting operating system details...${NC}"

    if [[ -f /etc/os-release ]]; then
        . /etc/os-release
        OS=$ID
        if [[ "$OS" == "linuxmint" ]] || [[ "$OS" == "pop" ]]; then
            OS="ubuntu"
        fi
    else
        echo -e "${RED}Cannot detect operating system. /etc/os-release not found.${NC}"
        exit 1
    fi

    echo -e "${GREEN}Detected OS: $OS${NC}"
    echo -e "${BLUE}Installing system dependencies...${NC}"

    case "$OS" in
        arch|endeavouros|manjaro)
            if command -v paru &> /dev/null; then
                AUR_HELPER="paru"
            elif command -v yay &> /dev/null; then
                AUR_HELPER="yay"
            else
                echo -e "${RED}Error: yay or paru is required for Arch-based systems.${NC}"
                exit 1
            fi
            
            if [[ -f "packages/pkglist-arch.txt" ]]; then
                packages=$(sed 's/["'\'']//g' packages/pkglist-arch.txt | tr ' ' '\n' | grep -v -E '^\s*$|^#')
                pkg_array=($packages)
                if [[ ${#pkg_array[@]} -gt 0 ]]; then
                    echo -e "${BLUE}Installing Arch packages in bulk...${NC}"
                    execute $AUR_HELPER -S --needed --noconfirm "${pkg_array[@]}" || echo -e "${RED}Warning: Bulk install failed. Check output above.${NC}"
                fi
            else
                echo -e "${RED}Warning: packages/pkglist-arch.txt not found!${NC}"
            fi
            ;;
        fedora)
            if [[ -f "packages/pkglist-fedora.txt" ]]; then
                packages=$(sed 's/["'\'']//g' packages/pkglist-fedora.txt | tr ' ' '\n' | grep -v -E '^\s*$|^#')
                pkg_array=($packages)
                if [[ ${#pkg_array[@]} -gt 0 ]]; then
                    echo -e "${BLUE}Installing Fedora packages in bulk...${NC}"
                    execute sudo dnf install -y "${pkg_array[@]}" || echo -e "${RED}Warning: Bulk install failed. Check output above.${NC}"
                fi
            else
                echo -e "${RED}Warning: packages/pkglist-fedora.txt not found!${NC}"
            fi
            ;;
        ubuntu|debian)
            echo -e "${RED}WARNING: Debian/Ubuntu do not provide Hyprland or its ecosystem natively.${NC}"
            echo -e "${RED}Ensure you have installed them via a 3rd party PPA/script first.${NC}"
            sleep 3
            if [[ -f "packages/pkglist-debian.txt" ]]; then
                execute sudo apt-get update
                packages=$(sed 's/["'\'']//g' packages/pkglist-debian.txt | tr ' ' '\n' | grep -v -E '^\s*$|^#')
                pkg_array=($packages)
                if [[ ${#pkg_array[@]} -gt 0 ]]; then
                    echo -e "${BLUE}Installing Debian/Ubuntu packages in bulk...${NC}"
                    execute sudo apt-get install -y "${pkg_array[@]}" || echo -e "${RED}Warning: Bulk install failed. Check output above.${NC}"
                fi
            else
                echo -e "${RED}Warning: packages/pkglist-debian.txt not found!${NC}"
            fi
            ;;
        gentoo)
            if [[ -f "packages/pkglist-gentoo.txt" ]]; then
                packages=$(sed 's/["'\'']//g' packages/pkglist-gentoo.txt | tr ' ' '\n' | grep -v -E '^\s*$|^#')
                pkg_array=($packages)
                if [[ ${#pkg_array[@]} -gt 0 ]]; then
                    echo -e "${BLUE}Installing Gentoo packages in bulk...${NC}"
                    execute sudo emerge -av --noreplace "${pkg_array[@]}" || echo -e "${RED}Warning: Bulk install failed. Check output above.${NC}"
                fi
            else
                echo -e "${RED}Warning: packages/pkglist-gentoo.txt not found!${NC}"
            fi
            ;;
        *)
            echo -e "${RED}Unsupported OS: $OS. Please install dependencies manually.${NC}"
            echo -ne "Do you wish to continue with config deployment anyway? (y/n): "
            read -r continue_ans
            if [[ ! "$continue_ans" =~ ^[Yy]$ ]]; then
                exit 1
            fi
            ;;
    esac

    echo -e "${BLUE}Checking for packages that require Cargo (Rust)...${NC}"
    if command -v cargo &> /dev/null; then
        export PATH="$HOME/.cargo/bin:$PATH"
        for pkg in wallust swww; do
            if ! command -v "$pkg" &> /dev/null; then
                echo -e "${BLUE}Installing $pkg via cargo...${NC}"
                if [[ "$pkg" == "swww" ]]; then
                    execute cargo install --git https://github.com/LGFae/swww.git || echo -e "${RED}Failed to install swww.${NC}"
                else
                    execute cargo install "$pkg" || echo -e "${RED}Failed to install $pkg.${NC}"
                fi
            else
                echo -e "${GREEN}$pkg is already installed.${NC}"
            fi
        done
    else
        echo -e "${RED}Cargo is not installed. Skipping wallust and swww.${NC}"
    fi

    echo -e "${BLUE}Checking for packages that require Go...${NC}"
    if command -v go &> /dev/null; then
        GOPATH=$(go env GOPATH 2>/dev/null || echo "$HOME/go")
        export PATH="$GOPATH/bin:$PATH"
        if ! command -v cliphist &> /dev/null; then
            echo -e "${BLUE}Installing cliphist via Go...${NC}"
            execute go install go.senan.xyz/cliphist@latest || echo -e "${RED}Failed to install cliphist.${NC}"
        else
            echo -e "${GREEN}cliphist is already installed.${NC}"
        fi
    else
        echo -e "${RED}Go is not installed. Skipping cliphist.${NC}"
    fi

    if command -v flatpak &> /dev/null; then
        if [[ -f "flatpak.txt" ]]; then
            echo -e "${BLUE}Installing flatpak packages...${NC}"
            execute sudo flatpak remote-add --if-not-exists flathub https://flathub.org/repo/flathub.flatpakrepo
            if [[ "$DRY_RUN" -eq 1 ]]; then
                echo -e "  ${BLUE}[DRY-RUN]${NC} Would install flatpaks listed in flatpak.txt"
            else
                grep -vE '^\s*#|^\s*$' flatpak.txt | xargs -r sudo flatpak install -y flathub || echo -e "${RED}Warning: Some flatpaks failed to install.${NC}"
            fi
        else
            echo -e "${RED}Warning: flatpak.txt not found!${NC}"
        fi
    fi
fi

# ==============================================================================
# CONFIG DEPLOYMENT & BACKUP (Idempotent Tarball)
# ==============================================================================

CONFIGS=(btop cava fastfetch hypr hypremoji kitty rofi swaync systemd wallpapers wallust waybar wlogout themes)

echo -e "${BLUE}Creating backup of existing configs...${NC}"
execute mkdir -p "$XDG_CONFIG_HOME"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
BACKUP_ARCHIVE="$HOME/nekoroshell_backup_$TIMESTAMP.tar.gz"
BACKUP_ITEMS=()

for conf in "${CONFIGS[@]}"; do
    if [[ -e "$XDG_CONFIG_HOME/$conf" ]]; then
        BACKUP_ITEMS+=("$conf")
    fi
done

if [[ ${#BACKUP_ITEMS[@]} -gt 0 ]]; then
    execute tar -czf "$BACKUP_ARCHIVE" -C "$XDG_CONFIG_HOME" "${BACKUP_ITEMS[@]}"
    echo -e "${GREEN}Created backup archive at: $BACKUP_ARCHIVE${NC}"
else
    echo -e "${GREEN}No existing configs found. Skipping backup.${NC}"
fi

echo -e "${BLUE}Deploying NeKoRoSHELL configuration files...${NC}"

for conf in "${CONFIGS[@]}"; do
    if [[ -d ".config/$conf" || -f ".config/$conf" ]]; then
        execute rm -rf "$XDG_CONFIG_HOME/$conf"
        execute cp -a ".config/$conf" "$XDG_CONFIG_HOME/"
        echo -e "  [✔] Copied $conf"
    else
        echo -e "${RED}  [!] Warning: .config/$conf missing in source directory.${NC}"
    fi
done

echo -e "${BLUE}Initializing user configuration sandbox...${NC}"

USER_CONF_DIR="$XDG_CONFIG_HOME/hypr/user/configs"
USER_SCRIPT_DIR="$XDG_CONFIG_HOME/hypr/user/scripts"
USER_HOOKS_DIR="$CONF_DIR/hypr/user/hooks"
TEMPLATE_DIR="$XDG_CONFIG_HOME/hypr/user/templates"

execute mkdir -p "$USER_CONF_DIR"
execute mkdir -p "$USER_SCRIPT_DIR"
execute mkdir -p "$USER_HOOKS_DIR"

if [[ -d "$TEMPLATE_DIR" ]]; then
    for file in "$TEMPLATE_DIR"/*.conf; do
        [ -e "$file" ] || continue 
        filename=$(basename "$file")
        if [ ! -f "$USER_CONF_DIR/$filename" ]; then
            execute cp "$file" "$USER_CONF_DIR/$filename"
            echo -e "  Initialized user config: $filename"
        fi
    done
fi

if [ ! -f "$USER_SCRIPT_DIR/autostart.sh" ]; then
    if [[ "$DRY_RUN" -eq 1 ]]; then
        echo -e "  ${BLUE}[DRY-RUN]${NC} Would create initialized user autostart.sh"
    else
        echo "#!/bin/bash" > "$USER_SCRIPT_DIR/autostart.sh"
        echo "# Add your personal startup commands here" >> "$USER_SCRIPT_DIR/autostart.sh"
        chmod +x "$USER_SCRIPT_DIR/autostart.sh"
        echo -e "  Initialized user autostart script."
    fi
fi

if [ ! -f "$USER_HOOKS_DIR/post-install.sh" ]; then
    echo -e "#!/usr/bin/env bash\n# Runs once after NeKoRoSHELL finishes a fresh install." > "$USER_HOOKS_DIR/post-install.sh"
    chmod +x "$USER_HOOKS_DIR/post-install.sh"
fi

if [ ! -f "$USER_HOOKS_DIR/post-update.sh" ]; then
    echo -e "#!/usr/bin/env bash\n# Runs every time 'nekoroshell update' completes successfully." > "$USER_HOOKS_DIR/post-update.sh"
    chmod +x "$USER_HOOKS_DIR/post-update.sh"
fi

if [ ! -f "$USER_HOOKS_DIR/on-theme-change.sh" ]; then
    echo -e "#!/usr/bin/env bash\n# Runs when a new theme is applied. \$1 is the theme name." > "$USER_HOOKS_DIR/on-theme-change.sh"
    chmod +x "$USER_HOOKS_DIR/on-theme-change.sh"
fi

echo -e "${BLUE}Detecting monitors...${NC}"
declare -a MONITOR_LIST

if [[ -n "$HYPRLAND_INSTANCE_SIGNATURE" ]] && command -v hyprctl &> /dev/null; then
    echo -e "${BLUE}Active Hyprland session detected. Using hyprctl...${NC}"
    mapfile -t MONITOR_LIST < <(hyprctl monitors | awk '/Monitor/ {print $2}') || true
else
    echo -e "${BLUE}Hyprland is not running. Querying sysfs for hardware...${NC}"
    for f in /sys/class/drm/*/status; do
        if grep -q "^connected$" "$f" 2>/dev/null; then
            dir=$(dirname "$f")
            name=$(basename "$dir")
            monitor_name=$(echo "$name" | sed -E 's/^card[0-9]+-//')
            if [[ ! " ${MONITOR_LIST[*]} " =~ " ${monitor_name} " ]]; then
                MONITOR_LIST+=("$monitor_name")
            fi
        fi
    done
fi

MONITOR_COUNT=${#MONITOR_LIST[@]}

if [[ "$MONITOR_COUNT" -gt 0 ]]; then
    PRIMARY_MONITOR=${MONITOR_LIST[0]}
    SECONDARY_MONITOR=${MONITOR_LIST[1]:-$PRIMARY_MONITOR}

    echo -e "${GREEN}Detected $MONITOR_COUNT monitor(s). Primary: $PRIMARY_MONITOR${NC}"

    if [[ -d "$XDG_CONFIG_HOME/hypr" ]]; then
        execute find "$XDG_CONFIG_HOME/hypr" -type d -path "*/user/templates" -prune -o -type f -name "*.conf" -exec sed -i "s/__PRIMARY_MONITOR__/$PRIMARY_MONITOR/g" {} + || true
        
        if [[ "$MONITOR_COUNT" -ge 2 ]]; then
            echo -e "${GREEN}Secondary monitor detected: $SECONDARY_MONITOR${NC}"
            execute find "$XDG_CONFIG_HOME/hypr" -type d -path "*/user/templates" -prune -o -type f -name "*.conf" -exec sed -i "s/__SECONDARY_MONITOR__/$SECONDARY_MONITOR/g" {} + || true
        else
            echo -e "${BLUE}Only one monitor detected. Commenting out secondary monitor lines...${NC}"
            execute find "$XDG_CONFIG_HOME/hypr" -type d -path "*/user/templates" -prune -o -type f -name "*.conf" -exec sed -i '/monitor=__SECONDARY_MONITOR__/s/^/#/' {} + || true
        fi
    fi
else
    echo -e "${RED}Warning: Could not automatically detect any monitors. Placeholders will remain unchanged.${NC}"
fi

SEARCH="/home/nekorosys"
REPLACE="$HOME"

REPLACE_ESCAPED=$(echo "$REPLACE" | sed 's/|/\\|/g; s/\\/\\\\/g')

echo -e "${BLUE}Replacing hardcoded paths... ($SEARCH -> $REPLACE)...${NC}"
if [[ "$DRY_RUN" -eq 1 ]]; then
    echo -e "  ${BLUE}[DRY-RUN]${NC} Would replace hardcoded paths across config files."
else
    find "$XDG_CONFIG_HOME" -type d -name "*_backup_*" -prune -o -type f \( -name "*.config" -o -name "*.css" -o -name "*.rasi" -o -name "*.conf" -o -name "*.sh" -o -name "*.json" -o -name "*.jsonc" -o -name "*.lua" -o -name "*.py" -o -name "*.yaml" \) -print0 2>/dev/null | xargs -0 -r sed -i "s|$SEARCH|$REPLACE_ESCAPED|g" || true
fi

inject_shell_config() {
    local shell_rc="$1"
    local source_rc="$2"
    
    local go_bin_path
    if command -v go &> /dev/null; then
        go_bin_path="$(go env GOPATH 2>/dev/null || echo "$HOME/go")/bin"
    else
        go_bin_path="$HOME/go/bin"
    fi
    
    local export_bin_dir="${USER_BIN_DIR/$HOME/\$HOME}"
    
    if [[ -f "$shell_rc" ]]; then
        if [[ "$DRY_RUN" -eq 1 ]]; then
            echo -e "  ${BLUE}[DRY-RUN]${NC} Would inject NeKoRoSHELL path/config blocks into $shell_rc"
        else
            sed -i '/# --- NeKoRoSHELL START ---/,/# --- NeKoRoSHELL END ---/d' "$shell_rc"
            echo -e "\n# --- NeKoRoSHELL START ---" >> "$shell_rc"
            [[ -f "$source_rc" ]] && cat "$source_rc" >> "$shell_rc"
            echo "export PATH=\"$export_bin_dir:\$HOME/.cargo/bin:$go_bin_path:\$PATH\"" >> "$shell_rc"
            echo -e "# --- NeKoRoSHELL END ---" >> "$shell_rc"
            echo -e "${GREEN}Updated $shell_rc${NC}"
        fi
    fi
}

inject_shell_config "$HOME/.bashrc" "home/.bashrc"
inject_shell_config "$HOME/.zshrc" "home/.zshrc"

[[ -f home/.p10k.zsh ]] && execute cp home/.p10k.zsh "$HOME/"
[[ -f home/.face.icon ]] && execute cp home/.face.icon "$HOME/"
[[ -f home/change-avatar.sh ]] && execute cp home/change-avatar.sh "$HOME/"

if [[ -d bin ]]; then
    echo -e "${BLUE}Copying scripts to $USER_BIN_DIR...${NC}"
    execute mkdir -p "$USER_BIN_DIR"
    execute cp -r bin/* "$USER_BIN_DIR/" 2>/dev/null || true
fi

# ==============================================================================
# DOWNLOADING & COMPILING (Compilation Mode Only)
# ==============================================================================

if [[ "$INSTALL_TYPE" == "compilation" ]]; then
    if ! command -v hyprshot &> /dev/null; then
        echo -e "${BLUE}Downloading hyprshot...${NC}"
        execute mkdir -p "$USER_BIN_DIR"
        execute curl -sLo "$USER_BIN_DIR/hyprshot" https://raw.githubusercontent.com/Gustash/Hyprshot/main/hyprshot || true
        execute chmod +x "$USER_BIN_DIR/hyprshot" || true
    fi

    if [[ ! -d "$HOME/powerlevel10k" ]]; then
        echo -e "${BLUE}Cloning Powerlevel10k theme...${NC}"
        execute git clone --depth=1 https://github.com/romkatv/powerlevel10k.git "$HOME/powerlevel10k" || true
    fi

    if ! command -v g++ &> /dev/null; then
        echo -e "${RED}g++ is not installed. Please install build tools to compile C++ daemons.${NC}"
    elif ! command -v pkg-config &> /dev/null; then
        echo -e "${RED}pkg-config is not installed. Cannot verify C++ header dependencies.${NC}"
    else
        echo -e "${BLUE}Checking C++ build dependencies...${NC}"
        
        REQUIRED_LIBS="wayland-client" 
        
        if ! pkg-config --exists $REQUIRED_LIBS; then
            echo -e "${RED}Missing required C++ development headers: $REQUIRED_LIBS${NC}"
            echo -e "${RED}Please install the corresponding -dev / -devel packages. Compilation aborted.${NC}"
        else
            echo -e "${BLUE}Compiling C++ Daemons via Make...${NC}"
            execute mkdir -p "$USER_BIN_DIR"
            
            if execute make clean all; then
                echo -e "${GREEN}Successfully compiled all C++ daemons.${NC}"
                execute cp build/* "$USER_BIN_DIR/" || echo -e "${RED}Warning: Failed to copy binaries to $USER_BIN_DIR${NC}"
            else
                echo -e "${RED}Compilation failed. Check the output above.${NC}"
            fi
        fi
    fi
fi

# ==============================================================================
# PERMISSIONS & SERVICES (Both Minimal & Compilation)
# ==============================================================================

echo -e "${BLUE}Setting script permissions...${NC}"
execute find "$XDG_CONFIG_HOME/" -type f -name "*.sh" -exec chmod +x {} + 2>/dev/null || true

for bin_dir in "$HOME/.local/bin/nekoroshell" "$HOME/bin/nekoroshell"; do
    if [[ -d "$bin_dir" ]]; then
        execute find "$bin_dir/" -type f -exec chmod +x {} + 2>/dev/null || true
    fi
done

# ==============================================================================
# FINAL VERIFICATION
# ==============================================================================

echo -e "\n${BLUE}Performing final system check...${NC}"
CORE_CMDS=("hyprland" "btop" "cava" "fastfetch" "hypremoji" "waybar" "swaync" "rofi" "kitty" "wallust" "swww")
MISSING=0

for cmd in "${CORE_CMDS[@]}"; do
    if command -v "$cmd" &> /dev/null; then
        echo -e "  [${GREEN}OK${NC}] $cmd is installed."
    else
        echo -e "  [${RED}!!${NC}] $cmd is missing from PATH."
        MISSING=$((MISSING + 1))
    fi
done

if [[ "$MISSING" -eq 0 ]]; then
    echo -e "\n${GREEN}Everything looks good! NeKoRoSHELL is ready.${NC}"
else
    echo -e "\n${RED}Warning: $MISSING core component(s) were not found.${NC}"
    echo -e "If you chose 'Minimal', this is expected. Otherwise, check the logs above."
fi

echo -e "${GREEN}Installation complete! Please restart your session to apply all changes.${NC}"
