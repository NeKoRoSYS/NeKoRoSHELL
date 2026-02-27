#
# ~/.bashrc
#

#export XDG_CONFIG_HOME="$HOME/.config"
#export XDG_CACHE_HOME="$HOME/.cache"
#export XDG_DATA_HOME="$HOME/.local/share"
#export XDG_STATE_HOME="$HOME/.local/state"
#export XDG_CONFIG_DIRS="/etc/xdg"
#export XDG_DATA_DIRS="/usr/local/share:/usr/share"
#export XDG_DATA_DIRS=$XDG_DATA_DIRS:$HOME/.local/share/flatpak/exports/share:/var/lib/flatpak/exports/share

# If not running interactively, don't do anything
[[ $- != *i* ]] && return

alias ls='ls --color=auto'
alias grep='grep --color=auto'
PS1='[\u@\h \W]\$ '

export PATH="$HOME/bin:$HOME/.local/bin:$HOME/.cargo/bin:$HOME/go/bin:$PATH"
