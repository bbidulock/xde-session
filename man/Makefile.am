
xde_man_files = \
	xde-autostart.man \
	xde-chooser.man \
	xde-input.man \
	xde-logout.man \
	xde-session.man \
	xde-smproxy.man \
	xde-xchooser.man \
	xde-xlogin.man \
	xde-wmproxy.man \
	xde-xsession.man \
	xde-xlock.man

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
	xde-xlogin.pod \
	xde-wmproxy.pod \
	xde-xsession.pod \
	xde-xlock.pod

SUFFIXES = .man .pod

.pod.man:
	pod2man --center="User Commands" \
		--date="$(DATE)" \
		--errors="none" \
		--fixed="CW" \
		--fixedbold="CB" \
		--fixeditalic="CI" \
		--fixedbolditalic="CX" \
		--nourls \
		--release="$(PACKAGE) $(VERSION)" \
		--section="1" \
		$< >$@

