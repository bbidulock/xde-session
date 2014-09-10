#!/usr/bin/m4
divert(-1)dnl
define(`PROLOGUE',`0')
define(`FONTS',`1')
define(`VARIABLES',`2')
define(`COLOR',`3')
define(`GRAYSCALE',`4')
define(`MONOCHROME',`5')
define(`FUNCTIONS',`6')
define(`BUTTONS',`7')
define(`BINDINGS',`8')
define(`MENUS',`9')
ifdef(`TWM_TYPE',`',`define(`TWM_TYPE',`etwm')')
ifdef(`TWM_VERSION',`',`define(`TWM_VERSION',`3.8.1')')
ifelse(TWM_TYPE, `twm', `', `define(`GRAYSCALE',`-1')')
define(`TWM_CONFIG_HOME',esyscmd(`echo -n "${TWM_CONFIG_HOME:-${HOME:-~}/.'TWM_TYPE`}"'))
define(`TWM_CONFIG_SDIR',esyscmd(`echo -n "${TWM_CONFIG_SDIR:-/usr/share/'TWM_TYPE`}"'))
ifdef(`HOME',`',`define(`HOME',esyscmd(`echo -n "${HOME:-~}"'))')
ifdef(`USER',`',`define(`USER',esyscmd(`echo -n "${LOGNAME:-${HOME:-~}}"'))')
syscmd(`test -x "'TWM_CONFIG_SDIR`/getstyles"')
ifelse(sysval, 0, `syscmd(`"'TWM_CONFIG_SDIR`/getstyles" 'TWM_TYPE` 'TWM_VERSION`>"'TWM_CONFIG_HOME`/stylemenu"')')
syscmd(`test -x "'TWM_CONFIG_HOME`/getstyles"')
ifelse(sysval, 0, `syscmd(`"'TWM_CONFIG_HOME`/getstyles" 'TWM_TYPE` 'TWM_VERSION`>"'TWM_CONFIG_HOME`/stylemenu"')')
divert(PROLOGUE)dnl
##
## twm configuration file
##
divert(FONTS)
#-----------#
#-- FONTS --#
#-----------#
divert(VARIABLES)
#---------------#
#-- VARIABLES --#
#---------------#
divert(COLOR)dnl
## Colors definitions
Color {
divert(GRAYSCALE)dnl
## Grayscale definitions
Grayscale {
divert(MONOCHROME)dnl
## Monochrome definitions
Monochrome {
divert(FUNCTIONS)
#---------------#
#-- FUNCTIONS --#
#---------------#
divert(BUTTONS)
#-------------#
#-- BUTTONS --#
#-------------#
divert(BINDINGS)
#--------------#
#-- BINDINGS --#
#--------------#
divert(MENUS)
#-----------#
#-- MENUS --#
#-----------#
divert(-1)dnl
syscmd(`test -r "'TWM_CONFIG_HOME`/defaults"')dnl
ifelse(sysval, 0,
	`pushdef(`SINCLUDE',TWM_CONFIG_HOME`/defaults')',
	`pushdef(`SINCLUDE',TWM_CONFIG_SDIR`/defaults')')dnl
sinclude(SINCLUDE)dnl
popdef(`SINCLUDE')dnl
divert(-1)dnl
syscmd(`test -L "'TWM_CONFIG_HOME`/stylerc"')dnl
ifelse(sysval, 0,
	`pushdef(`SINCLUDE',TWM_CONFIG_HOME`/stylerc')',
	`pushdef(`SINCLUDE',TWM_CONFIG_SDIR`/stylerc')')dnl
define(`TWM_STYLE_DIR',esyscmd(`readlink -n -f "'SINCLUDE`" 2>/dev/null|sed "s,/[^/]*$,,"'))dnl
sinclude(SINCLUDE)dnl
popdef(`SINCLUDE')dnl
divert(VARIABLES)dnl
syscmd(`test -r "'TWM_CONFIG_HOME`/apps"')dnl
ifelse(sysval, 0,
	`pushdef(`SINCLUDE',TWM_CONFIG_HOME`/apps')',
	`pushdef(`SINCLUDE',TWM_CONFIG_SDIR`/apps')')dnl
sinclude(SINCLUDE)dnl
popdef(`SINCLUDE')dnl
divert(BINDINGS)dnl
syscmd(`test -r "'TWM_CONFIG_HOME`/keys"')dnl
ifelse(sysval, 0,
	`pushdef(`SINCLUDE',TWM_CONFIG_HOME`/keys')',
	`pushdef(`SINCLUDE',TWM_CONFIG_SDIR`/keys')')dnl
sinclude(SINCLUDE)dnl
popdef(`SINCLUDE')dnl
divert(MENUS)dnl
syscmd(`test -r "'TWM_CONFIG_HOME`/stylemenu"')dnl
ifelse(sysval, 0,
	`pushdef(`SINCLUDE',TWM_CONFIG_HOME`/stylemenu')',
	`pushdef(`SINCLUDE',TWM_CONFIG_SDIR`/stylemenu')')dnl
sinclude(SINCLUDE)dnl
popdef(`SINCLUDE')dnl
divert(MENUS)dnl
syscmd(`test -r "'TWM_CONFIG_HOME`/winmenu"')dnl
ifelse(sysval, 0,
	`pushdef(`SINCLUDE',TWM_CONFIG_HOME`/winmenu')',
	`pushdef(`SINCLUDE',TWM_CONFIG_SDIR`/winmenu')')dnl
sinclude(SINCLUDE)dnl
popdef(`SINCLUDE')dnl
divert(MENUS)dnl
syscmd(`test -r "'TWM_CONFIG_HOME`/menu"')dnl
ifelse(sysval, 0,
	`pushdef(`SINCLUDE',TWM_CONFIG_HOME`/menu')',
	`pushdef(`SINCLUDE',TWM_CONFIG_SDIR`/menu')')dnl
`##' included file SINCLUDE
sinclude(SINCLUDE)dnl
popdef(`SINCLUDE')dnl
divert(COLOR)dnl
}
divert(GRAYSCALE)dnl
}
divert(MONOCHROME)dnl
}
divert(-1)dnl
dnl vim: set ft=m4:
