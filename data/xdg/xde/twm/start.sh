#!/bin/bash

XDE_USE_XDG_HOME=1

here=$(cd `dirname $0`;pwd)

name=twm

XDG_CONFIG_HOME=${XDG_CONFIG_HOME:-$HOME/.config}
XDG_DATA_HOME=${XDG_DATA_HOME:-$HOME/.local/share}
XDG_CONFIG_DIRS=${XDG_CONFIG_DIRS:-/etc/xdg}
XDG_DATA_DIRS=${XDG_DATA_DIRS:-/usr/local/share:/usr/share}

prog=${1:-${XDE_WM_TYPE:-$name}}
if ! which $prog >/dev/null 2>&1; then
	echo "ERROR: cannot find usable $prog program" >&2
	exit 1
fi
vers=${2:-${XDE_WM_VERSION}} || vers="1.0.8"
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

#
#

xde-setwm -N $name -p $$ -r $vers -c $prog -rc "$priv/init" || :

if true; then
	exec $prog -f "$priv/rc"
else
	export TWM_TYPE="$prog"
	export TWM_VERSION="$vers"
	export TWM_CONFIG_HOME="$priv"
	export TWM_CONFIG_SDIR="$sdir"
	export TWM_RCFILE="$priv/rc"
	export TWM_M4FILE="$priv/rc.m4"

	echo "ERROR: twm cannot M4 preprocess rc file" >&2
fi

exit 127

