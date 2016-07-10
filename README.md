[xde-session -- read me first file.  @DATE]: #

xde-session
===============

Package `xde-session-1.3` was released under GPLv3 license 2016-07-09.

This package provides a number of "C"-language tools for working with
the _X Desktop Environment_.  Most of these tools were previously written
in `perl(1)` and were part of the `xde-tools` package.  They have now been
codified in "C" for speed and to provide access to libraries not
available from `perl(1)`.

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

This is the `xde-session-1.3` package, released 2016-07-09.  This
release, and the latest version, can be obtained from the [GitHub
repository][1], using a command such as:

    $> git clone https://github.com/bbidulock/xde-session.git

Please see the [NEWS][2] file for release notes and history of user
visible changes for the current version, and the [ChangeLog][3]
file for a more detailed history of implementation changes.  The
[TODO][4] file lists features not yet implemented and other
outstanding items.

Please see the [INSTALL][5] file for installation instructions.

When working from `git(1)`, please use this file.  An abbreviated
installation procedure that works for most applications appears below.

This release is published under GPLv3.  Please see the license in
the file [COPYING][6].


Quick Start
-----------

The quickest and easiest way to get `xde-session` up and running
is to run the following commands:

    $> git clone https://github.com/bbidulock/xde-session.git
    $> cd xde-session
    $> ./autogen.sh
    $> ./configure --prefix=/usr --sysconfdir=/etc
    $> make V=0
    $> make DESTDIR="$pkgdir" install

This will configure, compile and install `xde-session` the quickest.
For those who would like to spend the extra 15 seconds reading
the output of `./configure --help`, some compile options can be
turned on and off before the build.

For general information on GNU's `./configure`, see the file
[INSTALL][5].


Issues
------

Report problems at GitHub [here][7].



[1]: https://github.com/bbidulock/xde-session
[2]: NEWS
[3]: ChangeLog
[4]: TODO
[5]: INSTALL
[6]: COPYING
[7]: https://github.com/bbidulock/xde-session/issues

[ vim: set ft=markdown sw=4 tw=72 nocin nosi fo+=tcqlorn spell: ]: #
