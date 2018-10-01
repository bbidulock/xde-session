#!/bin/bash

XDE_USE_XDG_HOME=1

here=$(cd `dirname $0`;pwd)

name=pekwm

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
vers=${2:-${XDE_WM_VERSION:-$(LANG= $prog --version 2>/dev/null|awk '/pekwm/{print$3;exit}')}}
[ -n "$vers" -o "$vers" = "GIT" ] || vers="0.1.17"
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

# pekwm: version GIT Built on Wed Jun 18 03:44:38 MDT 2014
# pekwm: version GIT Built on Wed Jun 18 03:44:38 MDT 2014
#  --help       show this info.
#  --version    show version info
#  --info       extended info. Use for bug reports.
#  --display    display to connect to
#  --config     alternative config file
#  --replace    replace running window manager

xde-setwm -N $name -p $$ -r $vers -c $prog --replace --config "$priv/config" || :

[ -x /usr/bin/numlockx ] && /usr/bin/numlockx off

exec $prog --replace --config "$priv/config"

exit 127

