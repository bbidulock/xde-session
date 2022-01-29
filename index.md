---
layout: default
---
[xde-session -- read me first file.  2022-01-28]: #

xde-session
===============

Package `xde-session-1.14` was released under GPLv3 license
2022-01-28.

This package provides a number of "C"-language tools for working with
the _X Desktop Environment_.  Most of these tools were previously
written in `perl(1)` and were part of the `xde-tools` package.  They
have now been codified in "C" for speed and to provide access to
libraries not available from `perl(1)`.

Primary tools included are:

 - __`xde-session`__   -- an X session manager for XDE
 - __`xde-chooser`__   -- an X session (window manager) chooser for XDE
 - __`xde-input`__     -- an X11 input settings tracker for XDE
 - __`xde-xchooser`__  -- an X11 XDMCP chooser for XDE
 - __`xde-logout`__    -- an X session logout facility
 - __`xde-autostart`__ -- an XDG autostart utility
 - __`xde-xsession`__  -- an X session launcher facility
 - __`xde-wmproxy`__   -- an X Session Management proxy for window managers
 - __`xde-xlogin`__    -- an X11 XDMCP greeter for XDE
 - __`xde-xlock`__     -- an X screen locker


Release
-------

This is the `xde-session-1.14` package, released 2022-01-28.
This release, and the latest version, can be obtained from [GitHub][1],
using a command such as:

    $> git clone https://github.com/bbidulock/xde-session.git

Please see the [RELEASE][3] and [NEWS][4] files for release notes and
history of user visible changes for the current version, and the
[ChangeLog][5] file for a more detailed history of implementation
changes.  The [TODO][6] file lists features not yet implemented and
other outstanding items.

Please see the [INSTALL][8] file for installation instructions.

When working from `git(1)`, please use this file.  An abbreviated
installation procedure that works for most applications appears below.

This release is published under GPLv3.  Please see the license in the
file [COPYING][10].


Quick Start
-----------

The quickest and easiest way to get `xde-session` up and
running is to run the following commands:

    $> git clone https://github.com/bbidulock/xde-session.git
    $> cd xde-session
    $> ./autogen.sh
    $> ./configure
    $> make
    $> make DESTDIR="$pkgdir" install

This will configure, compile and install `xde-session` the
quickest.  For those who like to spend the extra 15 seconds reading
`./configure --help`, some compile time options can be turned on and off
before the build.

For general information on GNU's `./configure`, see the file
[INSTALL][8].


Running
-------

Read the manual page after installation:

    $> man xde-session


Issues
------

Report issues on GitHub [here][2].



[1]: https://github.com/bbidulock/xde-session
[2]: https://github.com/bbidulock/xde-session/issues
[3]: https://github.com/bbidulock/xde-session/blob/1.14/RELEASE
[4]: https://github.com/bbidulock/xde-session/blob/1.14/NEWS
[5]: https://github.com/bbidulock/xde-session/blob/1.14/ChangeLog
[6]: https://github.com/bbidulock/xde-session/blob/1.14/TODO
[7]: https://github.com/bbidulock/xde-session/blob/1.14/COMPLIANCE
[8]: https://github.com/bbidulock/xde-session/blob/1.14/INSTALL
[9]: https://github.com/bbidulock/xde-session/blob/1.14/LICENSE
[10]: https://github.com/bbidulock/xde-session/blob/1.14/COPYING

[ vim: set ft=markdown sw=4 tw=72 nocin nosi fo+=tcqlorn spell: ]: #
