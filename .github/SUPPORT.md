> [!WARNING]
> These dotfiles still work as I am writing this on February 20, 2026. Future updates could break one or more of the packages and I may or may not be active enough to fix it for you (please do contact me on **Discord** - **@nekorosys** - and I'll be happy to entertain any of your concerns). Worry not! For as long as I keep using Linux, it's suffice to say that this repo will be maintained for a very long time because it has always been intended to store a clean slate of my desktop environment. I do recommend fixing it yourself just in case it happens because it might help you learn more about maintaining your desktop experience!
<br>

# FAQ / Troubleshooting

1. Help! My keybinds don't work!
   
   For a quick sanity check, do the following in order:
   - Assuming you've just installed NeKoRoSHELL and you're still in the cloned folder, do `cd` to return to `$HOME` (or just open another terminal).
   - Verify script permissions by typing `chmod +x $HOME/bin/nekoroshell/*` and/or `chmod +x $HOME/.local/bin/nekoroshell/*` then press Enter. This will allow the scripts used in the custom hyprland keybinds to be ran by your keyboard.
   If that didn't work, make sure the `environment` config file in `.config/hypr/configs/` has this line:
   ```bash
   env = PATH,$HOME/bin/nekoroshell:$HOME/.local/bin/nekoroshell:$PATH
   ```
   If not, type it in and then restart your PC.
<br>

# Guidelines for Opening an Issue

1. Avoid duplicates/spam. Please carefully search at the [Issue Tracker](https://github.com/NeKoRoSYS/NeKoRoSHELL/issues) if your issue has already been recorded/posted.
2. Make your title brief and concise.
3. Describe your issue and list the steps required to reproduce the issue, if applicable. (eg. bug, crashes, etc.)
4. Please be respectful with each other. ([Code of Conduct](https://github.com/NeKoRoSYS/NeKoRoSHELL/tree/main?tab=coc-ov-file))
