
xde_man1_files = \
	xde-autostart.man \
	xde-chooser.man \
	xde-dockapps.man \
	xde-dockmgr.man \
	xde-greeter.man \
	xde-login.man \
	xde-logout.man \
	xde-session.man \
	xde-sm.man \
	xde-smproxy.man \
	xde-startwm.man \
	xde-trayicons.man \
	xde-traymgr.man \
	xde-wait.man \
	xde-wmproxy.man \
	xde-xchooser.man \
	xde-xlock.man \
	xde-xlogin.man \
	xde-xsession.man \
	xde-xswitch.man

xde_man5_files = \
	XSession.desktop.man \
	Autostart.desktop.man \
	Application.desktop.man

nodist_man5_MANS = $(xde_man5_files)
nodist_man1_MANS = $(xde_man1_files)
CLEANFILES = $(xde_man1_files) $(xde_man5_files)

EXTRA_DIST = \
	xde-autostart.pod \
	xde-chooser.pod \
	xde-dockapps.pod \
	xde-dockmgr.pod \
	xde-greeter.pod \
	xde-login.pod \
	xde-logout.pod \
	xde-session.pod \
	xde-sm.pod \
	xde-smproxy.pod \
	xde-startwm.pod \
	xde-trayicons.pod \
	xde-traymgr.pod \
	xde-wait.pod \
	xde-wmproxy.pod \
	xde-xchooser.pod \
	xde-xlock.pod \
	xde-xlogin.pod \
	xde-xsession.pod \
	xde-xswitch.pod \
	XSession.desktop.pod \
	Autostart.desktop.pod \
	Application.desktop.pod

$(xde_man1_files): %.man: %.pod
	$(AM_V_GEN)pod2man --center="User Commands" \
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

$(xde_man5_files): %.man: %.pod
	$(AM_V_GEN)pod2man --center="Standards, Environments and Macros" \
		--date="$(DATE)" \
		--errors="none" \
		--fixed="CW" \
		--fixedbold="CB" \
		--fixeditalic="CI" \
		--fixedbolditalic="CX" \
		--nourls \
		--release="$(PACKAGE) $(VERSION)" \
		--section="5" \
		$< >$@

