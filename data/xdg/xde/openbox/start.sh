#!/bin/bash

XDE_USE_XDG_HOME=1

here=$(cd `dirname $0`;pwd)

name=openbox

XDG_CONFIG_HOME=${XDG_CONFIG_HOME:-$HOME/.config}
XDG_DATA_HOME=${XDG_DATA_HOME:-$HOME/.local/share}
XDG_CONFIG_DIRS=${XDG_CONFIG_DIRS:-/etc/xdg}
XDG_DATA_DIRS=${XDG_DATA_DIRS:-/usr/local/share:/usr/share}
XDG_RUNTIME_DIR=${XDG_RUNTIME_DIR:-/run/user/$(id -u)}

prog=${1:-${XDE_WM_TYPE:-$name}}
if ! which $prog >/dev/null 2>&1; then
	echo "ERROR: cannot find usable $prog program" >&2
	exit 1
fi
vers=${2:-${XDE_WM_VERSION:-$(LANG= $prog --version 2>/dev/null|awk '/Openbox/{print$2;exit}')}}
[ -n "$vers" ] || vers="3.5.2"
rdir=${XDE_WM_CONFIG_RDIR:-$XDG_RUNTIME_DIR/$name}
xdir=${XDE_WM_CONFIG_XDIR:-$XDG_CONFIG_HOME/$name}
sdir=${XDE_WM_CONFIG_SDIR:-/usr/share/$name}
home="$HOME/.$name"
priv="$rdir"
[ -d "$XDG_RUNTIME_DIR"  ] || priv="$xdir"
[ -n "$XDE_USE_XDG_HOME" ] || priv="$xdir"

export XDE_WM_PID="$$"
export XDE_WM_TYPE="$name"
export XDE_WM_VERSION="$vers"
export XDE_WM_CONFIG_PRIV="$priv"
export XDE_WM_CONFIG_RDIR="$rdir"
export XDE_WM_CONFIG_XDIR="$xdir"
export XDE_WM_CONFIG_HOME="$home"
export XDE_WM_CONFIG_SDIR="$sdir"
export XDE_WM_CONFIG_EDIR="$here"

for v in PID TYPE VERSION CONFIG_PRIV CONFIG_RDIR CONFIG_XDIR CONFIG_HOME CONFIG_SDIR CONFIG_EDIR; do
	eval "echo \"XDE_WM_$v => \$XDE_WM_$v\" >&2"
done

# Openbox 3.5.2
# Copyright (c) 2004   Mikael Magnusson
# Copyright (c) 2002   Dana Jansens
# 
# This program comes with ABSOLUTELY NO WARRANTY.
# This is free software, and you are welcome to redistribute it
# under certain conditions. See the file COPYING for details.
# 
# Syntax: openbox [options]
# 
# Options:
#   --help              Display this help and exit
#   --version           Display the version and exit
#   --replace           Replace the currently running window manager
#   --config-file FILE  Specify the path to the config file to use
#   --sm-disable        Disable connection to the session manager
# 
# Passing messages to a running Openbox instance:
#   --reconfigure       Reload Openbox's configuration
#   --restart           Restart Openbox
#   --exit              Exit Openbox
# 
# Debugging options:
#   --sync              Run in synchronous mode
#   --startup CMD       Run CMD after starting
#   --debug             Display debugging output
#   --debug-focus       Display debugging output for focus handling
#   --debug-session     Display debugging output for session management
#   --debug-xinerama    Split the display into fake xinerama screens
# 
# Please report bugs at http://bugzilla.icculus.org

xde-setwm -N $name -p $$ -r $vers -c $prog --replace --config-file "$priv/rc.xml" || :

[ -x /usr/bin/numlockx ] && /usr/bin/numlockx off

exec $prog --replace --config-file "$priv/rc.xml"

exit 127

