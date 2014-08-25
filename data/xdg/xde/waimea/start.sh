#!/bin/bash

XDE_USE_XDG_HOME=1

here=$(cd `dirname $0`;pwd)

name=waimea

XDG_CONFIG_HOME=${XDG_CONFIG_HOME:-$HOME/.config}
XDG_DATA_HOME=${XDG_DATA_HOME:-$HOME/.local/share}
XDG_CONFIG_DIRS=${XDG_CONFIG_DIRS:-/etc/xdg}
XDG_DATA_DIRS=${XDG_DATA_DIRS:-/usr/local/share:/usr/share}

prog=${1:-${XDE_WM_TYPE:-$name}}
if ! which $prog >/dev/null 2>&1; then
	echo "ERROR: cannot find usable $prog program" >&2
	exit 1
fi
vers=${2:-${XDE_WM_VERSION:-$(LANG= $prog --version 2>/dev/null|awk '/waimea/{print$2;exit}')}} || vers="0.70.2"
[ -n "$vers" ] || vers="0.70.2"
sdir=${XDE_WM_CONFIG_SDIR:-/usr/share/$name}
home="$HOME/.$name"
priv="$XDG_CONFIG_HOME/$name"
[ -n "$XDE_USE_XDG_HOME" ] || priv="$home"

export XDE_WM_PID="$$"
export XDE_WM_TYPE="$name"
export XDE_WM_VERSION="$vers"
export XDE_WM_CONFIG_PRIV="$priv"
export XDE_WM_CONFIG_HOME="$home"
export XDE_WM_CONFIG_SDIR="$sdir"
export XDE_WM_CONFIG_EDIR="$here"

for v in PID TYPE VERSION CONFIG_PRIV CONFIG_HOME CONFIG_SDIR CONFIG_EDIR; do
	eval "echo \"XDE_WM_$v => \$XDE_WM_$v\" >&2"
done

#Usage: waimea [OPTION...]
#Waimea - an X11 window manager designed for maximum efficiency
#
#   --display=DISPLAYNAME    X server to contact
#   --rcfile=CONFIGFILE      Config-file to use
#   --stylefile=STYLEFILE    Style-file to use
#   --actionfile=ACTIONFILE  Action-file to use
#   --menufile=MENUFILE      Menu-file to use
#   --usage                  Display brief usage message
#   --help                   Show this help message
#   --version                Output version information and exit
#
# Report bugs to <david@waimea.org>.

xde-setwm -N $name -p $$ -r $vers -c $prog --rcfile "$priv/config" || :

exec $prog --rcfile "$priv/config"

exit 127

