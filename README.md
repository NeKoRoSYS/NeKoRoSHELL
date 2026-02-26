> [!TIP]
> Please star the repo if you like the dotfiles. Thank you 🙏
<br>

<div align="center">

![Screenshot](showcase/image.png) 
<br>
<br>

# NeKoRoSHELL 

![GitHub Repo stars](https://img.shields.io/github/stars/NeKoRoSYS/NeKoRoSHELL?style=for-the-badge&color=%23FFD700)
 ![GitHub Release](https://img.shields.io/github/v/release/NeKoRoSYS/NeKoRoSHELL?display_name=tag&style=for-the-badge) ![Size](https://img.shields.io/github/repo-size/NeKoRoSYS/NeKoRoSHELL?style=for-the-badge) ![GitHub last commit](https://img.shields.io/github/last-commit/NeKoRoSYS/NeKoRoSHELL?style=for-the-badge) ![GitHub contributors](https://img.shields.io/github/contributors/NeKoRoSYS/NeKoRoSHELL?style=for-the-badge) ![Discord](https://img.shields.io/discord/774473787394752532?style=for-the-badge&label=Discord&color=%235865F2)
 <br>
 <br>

</div>

The best way to say "I use Linux btw 🤓" is if your desktop environment looks sleek and suave.

**NeKoRoSHELL** aims to provide an out-of-the-box, clean and reliable, generic, and modular framework that lets you easily customize your desktop experience with simple UI design philosophy in mind.
<br>
<br>

<div align="center">
 
| 📌 **Table of Contents** |
| :---: |
| 🚀 [Features](#features) |
| 🔗 [Dependencies](#dependencies) |
| ♥ [Acknowledgements](#acknowledgements) |

</div>
<br>

## [Features](CHANGELOG.md)

NeKoRoSHELL focuses on simplicity and modularity.
<br>

The following are what NeKoRoSHELL currently offers:
- **Portable and Distro-agnostic**
  - Use NeKoRoSHELL in any **supported** distro!
  - Init-agnostic.
  - XDG-compliant.
  - Features an advanced installer script.
    - Use `git clone https://github.com/NeKoRoSYS/NeKoRoSHELL`
    - Then `cd NeKoRoSHELL`
    - and finally, `bash install.sh` to install the dotfiles.
    - `install.sh` assumes you already have `git` and a distro-specific `g++` compiler.
    - `install.sh` requires you to have `cargo`, `paru`/`yay`, `go`, and `flatpak`.
    - You can freely customize `flatpak.txt` and `pkglist-DISTRO.txt` before running `install.sh`.
    - **The installer is safe.** It backs up your pre-existing .config folders. (If you have any)
    - The installer automatically handles assigning your monitors at `~/.config/hypr/configs/monitors.conf/` and replaces every occurence of `/home/nekorosys/` with your username for your own convenience.
    - SOME distros don't have hyprland or other dependencies on their package manager's repository and you may have to manually build them from source via script or something else.

- **NeKoRoSHELL as a Service**
  - Update your copy of NeKoRoSHELL simply by running the `nekoroshell update` command on your terminal.
  - Uses Vim or your preferred text editor to assist in reviewing file updates and gives the ability to overwrite, keep, and merge.
   
- **Window Controls**
  - Maximize
  - Fullscreen
  - Toggle Opacity
  - Toggle Floating Window
  - Pseudo-floating/organized windows
  - Change tile placement

- **Copying and Pasting**
  - Screenshot support via `hyprshot` .
  - Clipboard history via `cliphist`.
 
- **Notificications Handling**
  - Uses `SwayNC` for a dedicated notification center with customizable buttons and options.
 
- **Smart Navbar**
  - Uses portable WM-agnostic C++ wrappers for `waybar` to toggle visibility modes: Static, Dynamic, and Hover.
<p align="center">
  <img src="https://github.com/NeKoRoSYS/NeKoRoSHELL/blob/main/showcase/navbar-modes.gif" alt="Navbar Demo" />
</p>

- **Advanced Customization - Make NeKoRoSHELL YOURS!**
  - NeKoRoSHELL is not just an identity, it is a framework. This repo gives you at most 2 pre-installed out-of-the-box layouts/styling for waybar, hyprlock, and SwayNC. The best part? You can make your own!
  - Switch to Dark and Light contrast modes
  - [Dedicated Theming System](THEMING.md):
    - Select individual skins for waybar, rofi, hyprlock, SwayNC, and wlogout.
    - **Wallpaper Handling**
      - Supports both online and offline image (via `swww`) and video (via `mpvpaper`) formats.
        - `mpvpaper` automatically stops if an app is on fullscreen mode to save CPU/RAM and GPU space.
        - Paste image or video links with valid file extensions in the rofi prompt and the download will automatically be processed, saved, and set as your new wallpaper.
      - Uses `wallust` to dynamically update border and UI colors based on the percieved colors of from the wallpaper.
    - Make and select your own Themes that automatically apply skins and wallpapers.
<br>

![Screenshot](showcase/image-5.png) 
<br>
<br>
<br>
![Screenshot](showcase/image-3.png) 
<br>
<br>
<br>
![Screenshot](showcase/image-1.png) 
<br>
<br>
<br>
![Screenshot](showcase/image-2.png) 
<br>
<br>

### Roadmap

NeKoRoSHELL is currently being developed by one person (*cough* [CONTRIBUTING](https://github.com/NeKoRoSYS/NeKoRoSHELL/tree/main?tab=contributing-ov-file#) *cough*) and is constantly under rigorous quality assurance for improvement. We always aim to keep a "no-break" promise for every update so that you can safely update to later versions without expecting any breakages.

<br>
<div align="center">

| 📋 **TODO** | **STATUS** |
| :---: | :---: |
| Improve base "legacy" theme | ✅ |
| Implement base functionality | ✅ |
| Implement base QOL features | ✅ |
| Optimizations | ✅ |
| Color Handling - Replace pywal6 with wallust | ✅ |
| Dmenu Overhaul - Replace wofi with rofi | ✅ |
| Theme System - Set all skins in one go | ✅ |
| wlogout integration | ✅ |
| BETA<br>Support for other distros; Verified to be working on: Arch | 🔍 |
| BETA<br>Make NeKoRoSHELL init-agnostic; Verified to be working on: Arch | 🔍 |
| BETA<br>Make NeKoRoSHELL WM-agnostic; Verified to be working on: Hyprland | 🔍 |
| Qt and Kvantum integration | 🤔 |

</div>
<br>

## Dependencies

> [!CAUTION]
> **HARDWARE SPECIFIC CONFIGURATION**<br>
>
> Some environment variables and params at `~/.config/hypr/configs/environment.conf/` and `~/.config/hypr/scripts/set-wallpaper.sh/` (also check the `check-video.sh` script, `mpvpaper` uses a "hwdec=nvdec" param) **require an NVIDIA graphics card**. Although it may be generally safe to leave it as is upon installing to a machine without such GPU, I recommend commenting it out or replacing it with a variable that goes according to your GPU.

> [!WARNING]
> **SOFTWARE SPECIFIC CONFIGURATION**<br>
>
> This project of mine was originally built only for Arch Linux but is now capable of claiming itself to be Distro-agnostic. However, **installation of this repo in other Linux Distros aside from Arch is more or less UNTESTED.** Please verify using `nano` or your preferred text editor if your distro supports the packages listed at `pkglist-DISTRO.txt` or if the packages are named correctly.
>
> The installation system that I implemented can be improved. If you're willing to help, please make a pull request. Your contributions are welcome and will be appreciated! :D

- `update-nekoroshell` uses Vim to compare, overwrite, or merge files when updating.
- Auto-pause animated wallpapers via [mpvpaper-stop](https://github.com/pvtoari/mpvpaper-stop) (dependencies: cmake, cjson)
  - Used at `set-wallpaper.sh` and `check-video.sh` in `~/.config/hypr/scripts/wallpapers/` to save CPU/RAM usage.
- Install [Hypremoji](https://github.com/Musagy/hypremoji)
- Fix waybar tray disappearing after a certain amount of time by installing `sni-qt`.
  Make sure you're not killing waybar using -SIGUSER2 when refreshing the config.
<br>

## Acknowledgements
- Amelie ([@S-e-r-a-p-h-i-n-e](https://github.com/S-e-r-a-p-h-i-n-e)) for helping me transition the project from using pywal16 to wallust and letting me borrow a few scripts. Go check out [SeraDOTS](https://github.com/S-e-r-a-p-h-i-n-e/SeraDOTS)!

- April for helping me figure out the cause of the now-fixed "keybinds not working" issue.

- [@MiroBG](https://github.com/MiroBG) for helping me track issues within the repo.

- Credits to [iyiolacak](https://github.com/iyiolacak/iyiolacak-swaync-config?tab=readme-ov-file), [justinmdickey](https://github.com/justinmdickey/publicdots/blob/main/.config/hypr/hyprlock.conf), and [mkhmtolzhas](https://github.com/mkhmtolzhas/mkhmtdots) for their amazing designs.
  - The `legacy` theme was based on mkhmtolzha's waybar stylesheet and layout, just heavily modified and made to be thematically-consistent across packages like SwayNC.
<br>

## Star History
<br>

<div align="center">
<a href="https://www.star-history.com/#nekorosys/nekoroshell&type=date&legend=bottom-right">
 <picture>
   <source media="(prefers-color-scheme: dark)" srcset="https://api.star-history.com/svg?repos=nekorosys/nekoroshell&type=date&theme=dark&legend=bottom-right" />
   <source media="(prefers-color-scheme: light)" srcset="https://api.star-history.com/svg?repos=nekorosys/nekoroshell&type=date&legend=bottom-right" />
   <img alt="Star History Chart" src="https://api.star-history.com/svg?repos=nekorosys/nekoroshell&type=date&legend=bottom-right" />
 </picture>
</a>
</div>
