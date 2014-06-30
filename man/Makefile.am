
xde_man_files = \
	xde-autostart.man \
	xde-chooser.man \
	xde-input.man \
	xde-logout.man \
	xde-session.man \
	xde-smproxy.man \
	xde-xchooser.man \
	xde-xlogin.man

nodist_man1_MANS = $(xde_man_files)
CLEANFILES = $(xde_man_files)

EXTRA_DIST = \
	xde-autostart.pod \
	xde-chooser.pod \
	xde-input.pod \
	xde-logout.pod \
	xde-session.pod \
	xde-smproxy.pod \
	xde-xchooser.pod \
	xde-xlogin.man

xde-autostart.man: MANNAME = XDE-AUTOSTART
xde-autostart.man: xde-autostart.pod

xde-chooser.man: MANNAME = XDE-CHOOSER
xde-chooser.man: xde-chooser.pod

xde-input.man: MANNAME = XDE-INPUT
xde-input.man: xde-input.pod

xde-logout.man: MANNAME = XDE-LOGOUT
xde-logout.man: xde-logout.pod

xde-session.man: MANNAME = XDE-SESSION
xde-session.man: xde-session.pod

xde-smproxy.man: MANNAME = XDE-SMPROXY
xde-smproxy.man: xde-smproxy.pod

xde-xchooser.man: MANNAME = XDE-XCHOOSER
xde-xchooser.man: xde-xchooser.pod

xde-xlogin.man: MANNAME = XDE-XLOGIN
xde-xlogin.man: xde-xlogin.pod

SUFFIXES = .man .pod

.pod.man:
	pod2man --center="User Commands" \
		--date="$(DATE)" \
		--errors="none" \
		--fixed="CW" \
		--fixedbold="CB" \
		--fixeditalic="CI" \
		--fixedbolditalic="CX" \
		--name="$(MANNAME)" \
		--nourls \
		--release="$(PACKAGE) $(VERSION)" \
		--section="1" \
		$< >$@

