#!/bin/bash

XDE_USE_XDG_HOME=1

here=$(cd `dirname $0`;pwd)

name=blackbox

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
vers=${2:-${XDE_WM_VERSION:-$(LANG= $prog -version 2>/dev/null|awk '/Blackbox/{print$2;exit}')}}
[ -n "$vers" ] || vers="0.70.2"
rdir=${XDE_WM_CONFIG_RDIR:-$XDG_RUNTIME_DIR/$name}
xdir=${XDE_WM_CONFIG_XDIR:-$XDG_CONFIG_HOME/$name}
sdir=${XDE_WM_CONFIG_SDIR:-/usr/share/$name}
home="$HOME/.$name"
priv="$rdir"
[ -d "$XDG_RUNTIME_DIR"  ] || priv="$xdir"
[ -n "$XDE_USE_XDG_HOME" ] || priv="$home"

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

# Blackbox 0.70.2.20
# Copyright (c) 2001 - 2005 Sean 'Shaleh' Perry
# Copyright (c) 1997 - 2000, 2002 - 2005 Bradley T Hughes
#  -display <string>		use display connection.
#  -single <string>		manage the default screen only
#  -rc <string>			use alternate resource file.
#  -version			display version and exit.
#  -help			display this help text and exit.
#
# Compile time options:
#  Debugging:			no
#  Shape:			yes
#  Xft:				yes

xde-setwm -N $name -p $$ -r $vers -c $prog -rc "$priv/rc" || :

[ -x /usr/bin/numlockx ] && /usr/bin/numlockx off

exec $prog -rc "$priv/rc"

exit 127

