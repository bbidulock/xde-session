#!/bin/bash

XDE_USE_XDG_HOME=1

here=$(cd `dirname $0`;pwd)

name=icewm

XDG_CONFIG_HOME=${XDG_CONFIG_HOME:-$HOME/.config}
XDG_DATA_HOME=${XDG_DATA_HOME:-$HOME/.local/share}
XDG_CONFIG_DIRS=${XDG_CONFIG_DIRS:-/etc/xdg}
XDG_DATA_DIRS=${XDG_DATA_DIRS:-/usr/local/share:/usr/share}

prog=${1:-${XDE_WM_TYPE:-$name}}
if ! which $prog >/dev/null 2>&1; then
	echo "ERROR: cannot find usable $prog program" >&2
	exit 1
fi
vers=${2:-${XDE_WM_VERSION:-$(LANG= $prog --version 2>/dev/null|awk '/IcewWM/{print$2;exit}')}} || vers="0.70.2"
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

# Usage: icewm [OPTIONS]
# Starts the IceWM window manager.
# 
# Options:
#   --display=NAME      NAME of the X server to use.
#   --client-id=ID      Client id to use when contacting session manager.
#   --sync              Synchronize X11 commands.
# 
#   -c, --config=FILE   Load preferences from FILE.
#   -t, --theme=FILE    Load theme from FILE.
#   -n, --no-configure  Ignore preferences file.
# 
#   -v, --version       Prints version information and exits.
#   -h, --help          Prints this usage screen and exits.
#   --replace           Replace an existing window manager.
#   --restart           Don't use this: It's an internal flag.
# 
# Environment variables:
#   ICEWM_PRIVCFG=PATH  Directory to use for user private configuration files,
#                       "$HOME/.icewm/" by default.
#   DISPLAY=NAME        Name of the X server to use, depends on Xlib by default.
#   MAIL=URL            Location of your mailbox. If the schema is omitted
#                       the local "file" schema is assumed.

Visit http://www.icewm.org/ for report bugs, support requests, comments...


export ICEWM_PRIVCFG="$priv"

xde-setwm -N $name -p $$ -r $vers -c $prog --replace -c "$priv/preferences" || :

exec $prog --replace -c "$priv/preferences"

exit 127

