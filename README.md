
## xde-session

Package `xde-session-1.0.68` was released under GPL license 2014-07-29.

This package provides a number of `C`-language tools for working with
the X Desktop Environment.  Some of these tools were previously written
in *perl(1)* and were part of the xde-tools package.  They have no been
codified in `C` for speed and to provide access to libraries not yet
available from *perl(1)*.  The major programs provided by the package are
as follows:

- **xde-autostart** -- a program to perform XDG autostart coordinated
  with window manager launch and supporting X Session Management.  This
  program is not meant to be run directly but forms part of the X
  Desktop Environment (XDE).
- **xde-chooser** -- a program that provides the ability to select and
  launch (or replace) an X session with a specific window manager.  This
  program is meant to be run from the *.xinitrc* file.
- **xde-input** -- a program run out of XDG autostart that coordinates
  *xset(1)* settings such as keyboard repeat rates, screen saver
  intervals, and such.  This program is meant to be run from autostart
  or from the applications menu.
- **xde-login**
- **xde-logout** -- a program that provides a dialog for logging out of
  an X Desktop Environment (XDE) session.  It permits performing machine
  actions, switching user (virtual terminals), performing X Session
  Management checkpoints, switching window managers, or simply logging
  of an X session.  This program is meant to be run from the panel or an
  applications menu or launcher.
- **xde-session** -- initiates an X Session under the X Desktop
  Environment.  This is meant to be run from the *.xsession* file or
  directly by the **xde-xlogin** program.  It performs X Session
  Management and coordinates the launching of window manager using the
  **xde-xsession** program.
- **xde-wmproxy** -- launches a window manager and provides X Session
  Management assistance to the window manager under the X Desktop
  Environment (XDE).  This program is not meant to be run directly but
  forms part of the X Desktop Environment (XDE).
- **xde-xchooser** -- a program that performs the functions of an XDMCP
  Chooser: choosing which machine to connect displays that issues
  XDMCP indirect queries to the current host.  This program is meant to be
  run with *xdm(1)*.
- **xde-xlock** -- a program that performs the functions of a screen
  locker fully integrated with the *systemd(8)* multi-session
  environment.
- **xde-xlogin** -- a program that performs the functions of an XDMCP
  Greeter: logging into a machine that issues XDMCP queries to the
  current host, or for local displays managed by *xdm(1)*.
- **xde-xsession** -- a program that coordinates the startup of window
  manager and destkop environment using the **xde-wmproxy** and
  **xde-autostart** programs.  This program is not meant to be run
  directly but is used by the **xde-session** program.

### Release

This is the `xde-session-1.0.68` package, released 2014-07-29.  This
release, and the latest version, can be obtained from the GitHub
repository at http://github.com/bbidulock/xde-session, using a command
such as:

```bash
git clone http://github.com/bbidulock/xde-session.git
```

Please see the [NEWS](NEWS) file for release notes and history of user visible
changes for the current version, and the [ChangeLog](ChangeLog) file for a more
detailed history of implementation changes.  The [TODO](TODO) file lists
features not yet implemented and other outstanding items.

Please see the [INSTALL](INSTALL) file for installation instructions.

When working from `git(1)', please see the [README-git](README-git) file.  An
abbreviated installation procedure that works for most applications
appears below.

This release is published under the GPL license that can be found in
the file [COPYING](COPYING).

### Quick Start

The quickest and easiest way to get xde-session up and running is to run
the following commands:

```bash
git clone http://github.com/bbidulock/xde-session.git xde-session
cd xde-session
./autogen.sh
./configure --prefix=/usr
make V=0
make DESTDIR="$pkgdir" install
```

This will configure, compile and install xde-session the quickest.  For
those who would like to spend the extra 15 seconds reading `./configure
--help`, some compile time options can be turned on and off before the
build.

For general information on GNU's `./configure`, see the file [INSTALL](INSTALL).

### Running xde-session

Read the manual pages after installation:

    man xde-session

### Issues

Report issues to https://github.com/bbidulock/xde-session/issues.
