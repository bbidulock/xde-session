
xde_man_files = \
	xde-autostart.man \
	xde-chooser.man \
	xde-dockmgr.man \
	xde-input.man \
	xde-logout.man \
	xde-session.man \
	xde-sm.man \
	xde-smproxy.man \
	xde-traymgr.man \
	xde-wait.man \
	xde-wmproxy.man \
	xde-xchooser.man \
	xde-xlock.man \
	xde-xlogin.man \
	xde-xsession.man

nodist_man1_MANS = $(xde_man_files)
CLEANFILES = $(xde_man_files)

EXTRA_DIST = \
	xde-autostart.pod \
	xde-chooser.pod \
	xde-dockmgr.pod \
	xde-input.pod \
	xde-logout.pod \
	xde-session.pod \
	xde-sm.pod \
	xde-smproxy.pod \
	xde-traymgr.pod \
	xde-wait.pod \
	xde-wmproxy.pod \
	xde-xchooser.pod \
	xde-xlock.pod \
	xde-xlogin.pod \
	xde-xsession.pod

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

