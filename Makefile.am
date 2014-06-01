
ACLOCAL_AMFLAGS = -I m4

SUBDIRS = src man

EXTRA_DIST = INSTALL COPYING AUTHORS AUTHORS.in NEWS NEWS.in README README.in THANKS THANKS.in TODO TODO.in ChangeLog

ChangeLog:
	( \
	  echo -e "# created with git log --stat=76 -M -C|fmt -sct -w80\n" ; \
	  git log --stat=76 -M -C|fmt -sct -w80 ; \
	)>$@

AUTHORS.in:
	( \
	  echo "\@PACKAGE\@ -- authors file.  \@DATE\@" ; \
	  echo "" ; \
	  git log|grep '^Author:'|awk '{if(!authors[$$0]){print$$0;authors[$$0]=1;}}'|tac \
	)>$@

README: README.in
	sed -r -e 's:\@PACKAGE\@:$(PACKAGE):g;s:\@VERSION\@:$(VERSION):g;s:\@DATE\@:$(DATE):g' $< >$@

NEWS: NEWS.in
	sed -r -e 's:\@PACKAGE\@:$(PACKAGE):g;s:\@VERSION\@:$(VERSION):g;s:\@DATE\@:$(DATE):g' $< >$@

AUTHORS: AUTHORS.in
	sed -r -e 's:\@PACKAGE\@:$(PACKAGE):g;s:\@VERSION\@:$(VERSION):g;s:\@DATE\@:$(DATE):g' $< >$@

THANKS: THANKS.in
	sed -r -e 's:\@PACKAGE\@:$(PACKAGE):g;s:\@VERSION\@:$(VERSION):g;s:\@DATE\@:$(DATE):g' $< >$@

TODO: TODO.in
	sed -r -e 's:\@PACKAGE\@:$(PACKAGE):g;s:\@VERSION\@:$(VERSION):g;s:\@DATE\@:$(DATE):g' $< >$@

dist_noinst_SCRIPTS = autogen.sh

nobase_dist_xdgconf_DATA = \
	xde/autostart/bbrun.desktop \
	xde/autostart/cbatticon.desktop \
	xde/autostart/compton.desktop \
	xde/autostart/compton-fluxbox.desktop \
	xde/autostart/dhcpcd-gtk.desktop \
	xde/autostart/docker.desktop \
	xde/autostart/gatotray.desktop \
	xde/autostart/icewmtray.desktop \
	xde/autostart/idesk.desktop \
	xde/autostart/lxpanel.desktop \
	xde/autostart/menutray.desktop \
	xde/autostart/nm-applet.desktop \
	xde/autostart/pnmixer.desktop \
	xde/autostart/remmina-applet.desktop \
	xde/autostart/retrovol.desktop \
	xde/autostart/smproxy.desktop \
	xde/autostart/wmappl.desktop \
	xde/autostart/wmbattery.desktop \
	xde/autostart/wmblob.desktop \
	xde/autostart/wmbubble.desktop \
	xde/autostart/wmCalClock.desktop \
	xde/autostart/wmclockmon.desktop \
	xde/autostart/wmcore.desktop \
	xde/autostart/wmcpuload.desktop \
	xde/autostart/wmmixer.desktop \
	xde/autostart/wmnd.desktop \
	xde/autostart/wmnet.desktop \
	xde/autostart/wmsysmon.desktop \
	xde/autostart/wmsystemtray.desktop \
	xde/autostart/wmtimer.desktop \
	xde/autostart/wmWeather.desktop \
	xde/autostart/wmweather+.desktop \
	xde/autostart/xde-input.desktop \
	xde/autostart/xde-panel.desktop \
	xde/autostart/xde-setbg.desktop

nobase_dist_xdgconf_DATA += \
	xde/adwm/adwmrc \
	xde/adwm/keysrc \
	xde/adwm/stylerc \
	xde/adwm/version \
	xde/blackbox/keys \
	xde/blackbox/menu \
	xde/blackbox/rc \
	xde/blackbox/setup.pl \
	xde/blackbox/setup.sh \
	xde/blackbox/start.sh \
	xde/blackbox/version \
	xde/blackbox/xde-rc \
	xde/ctwm/apps \
	xde/ctwm/buttons \
	xde/ctwm/config \
	xde/ctwm/defaults \
	xde/ctwm/getstyles \
	xde/ctwm/keys \
	xde/ctwm/menu \
	xde/ctwm/rc \
	xde/ctwm/rc.m4 \
	xde/ctwm/setstyle \
	xde/ctwm/setup \
	xde/ctwm/setup.sh \
	xde/ctwm/start \
	xde/ctwm/start.sh \
	xde/ctwm/stylemenu \
	xde/ctwm/version \
	xde/ctwm/winmenu \
	xde/echinus/version \
	xde/fluxbox/apps \
	xde/fluxbox/fbdesk \
	xde/fluxbox/fbdesk.icons \
	xde/fluxbox/fbpager \
	xde/fluxbox/init \
	xde/fluxbox/keys \
	xde/fluxbox/menu \
	xde/fluxbox/menuconfig \
	xde/fluxbox/overlay \
	xde/fluxbox/setup.sh \
	xde/fluxbox/slitlist \
	xde/fluxbox/start.sh \
	xde/fluxbox/startup \
	xde/fluxbox/unexicon-menu \
	xde/fluxbox/usermenu \
	xde/fluxbox/version \
	xde/fluxbox/windowmenu \
	xde/fluxbox/xde-init \
	xde/fvwm/version \
	xde/icewm/focus_mode \
	xde/icewm/keys \
	xde/icewm/menu \
	xde/icewm/preferences \
	xde/icewm/preferences.bak \
	xde/icewm/prefoverride \
	xde/icewm/programs \
	xde/icewm/recent.menu \
	xde/icewm/setup.sh \
	xde/icewm/start.sh \
	xde/icewm/theme \
	xde/icewm/toolbar \
	xde/icewm/version \
	xde/icewm/winoptions \
	xde/images/arch-banner.png \
	xde/images/unexicon-banner.png \
	xde/jwm/config \
	xde/jwm/defaults \
	xde/jwm/desktop \
	xde/jwm/getstyles \
	xde/jwm/groups \
	xde/jwm/keys \
	xde/jwm/menu \
	xde/jwm/paths \
	xde/jwm/rc \
	xde/jwm/root \
	xde/jwm/setstyle \
	xde/jwm/setup.sh \
	xde/jwm/start.sh \
	xde/jwm/style \
	xde/jwm/tray \
	xde/jwm/version \
	xde/lxde/version \
	xde/metacity/version \
	xde/openbox/lxde-rc.xml \
	xde/openbox/lxde-rc.xml.new \
	xde/openbox/menu \
	xde/openbox/menu.bak \
	xde/openbox/menu.xml \
	xde/openbox/menu.xml.back \
	xde/openbox/menu.xml.new \
	xde/openbox/menu.xml.old \
	xde/openbox/rc.xml \
	xde/openbox/rc.xml.orig \
	xde/openbox/unx-rc.xml \
	xde/openbox/version \
	xde/openbox/xdg-menu.xml \
	xde/pekwm/autoproperties \
	xde/pekwm/config \
	xde/pekwm/config_system \
	xde/pekwm/history \
	xde/pekwm/keys \
	xde/pekwm/menu \
	xde/pekwm/mouse \
	xde/pekwm/start \
	xde/pekwm/vars \
	xde/pekwm/version \
	xde/pekwm/window \
	xde/twm/apps \
	xde/twm/buttons \
	xde/twm/config \
	xde/twm/defaults \
	xde/twm/getstyles \
	xde/twm/keys \
	xde/twm/menu \
	xde/twm/rc \
	xde/twm/rc.m4 \
	xde/twm/setstyle \
	xde/twm/setup.sh \
	xde/twm/start \
	xde/twm/start.sh \
	xde/twm/stylemenu \
	xde/twm/version \
	xde/twm/winmenu \
	xde/uwm/buttons \
	xde/uwm/keys \
	xde/uwm/menu \
	xde/uwm/panel \
	xde/uwm/setup \
	xde/uwm/start \
	xde/uwm/uwmrc \
	xde/uwm/version \
	xde/uwm/winmenu \
	xde/vtwm/apps \
	xde/vtwm/buttons \
	xde/vtwm/config \
	xde/vtwm/defaults \
	xde/vtwm/getstyles \
	xde/vtwm/keys \
	xde/vtwm/menu \
	xde/vtwm/rc \
	xde/vtwm/rc.m4 \
	xde/vtwm/setstyle \
	xde/vtwm/setup.sh \
	xde/vtwm/start \
	xde/vtwm/start.sh \
	xde/vtwm/stylemenu \
	xde/vtwm/version \
	xde/vtwm/winmenu \
	xde/waimea/actionrc \
	xde/waimea/config \
	xde/waimea/menu \
	xde/waimea/setup \
	xde/waimea/setup.sh \
	xde/waimea/start \
	xde/waimea/start.sh \
	xde/waimea/version \
	xde/wmaker/version

nobase_dist_xdgdata_DATA = \
	xde/xsessions/2bwm.desktop \
	xde/xsessions/adwm.desktop \
	xde/xsessions/aewm.desktop \
	xde/xsessions/aewm++.desktop \
	xde/xsessions/AfterStep.desktop \
	xde/xsessions/awesome.desktop \
	xde/xsessions/blackbox.desktop \
	xde/xsessions/bspwm.desktop \
	xde/xsessions/ctwm.desktop \
	xde/xsessions/cwm.desktop \
	xde/xsessions/dwm.desktop \
	xde/xsessions/e16-gnome2-session.desktop \
	xde/xsessions/e16-gnome3-session.desktop \
	xde/xsessions/e16-kde-session.desktop \
	xde/xsessions/e16-session.desktop \
	xde/xsessions/echinus.desktop \
	xde/xsessions/ede.desktop \
	xde/xsessions/evilwm.desktop \
	xde/xsessions/failsafewm.desktop \
	xde/xsessions/fluxbox.desktop \
	xde/xsessions/flwm.desktop \
	xde/xsessions/Fvwm1.desktop \
	xde/xsessions/fvwm-crystal.desktop \
	xde/xsessions/Fvwm.desktop \
	xde/xsessions/fvwm.desktop \
	xde/xsessions/gnome.desktop \
	xde/xsessions/herbstluftwm.desktop \
	xde/xsessions/i3.desktop \
	xde/xsessions/i3-with-shmlog.desktop \
	xde/xsessions/icewm.desktop \
	xde/xsessions/icewm-session.desktop \
	xde/xsessions/Jwm.desktop \
	xde/xsessions/larswm.desktop \
	xde/xsessions/LXDE.desktop \
	xde/xsessions/mantis-wm.desktop \
	xde/xsessions/matwm2.desktop \
	xde/xsessions/metacity.desktop \
	xde/xsessions/mutter.desktop \
	xde/xsessions/mwm.desktop \
	xde/xsessions/openbox.desktop \
	xde/xsessions/openbox-gnome.desktop \
	xde/xsessions/openbox-kde.desktop \
	xde/xsessions/pawm.desktop \
	xde/xsessions/pekwm.desktop \
	xde/xsessions/spectrwm.desktop \
	xde/xsessions/ssh.desktop \
	xde/xsessions/twm.desktop \
	xde/xsessions/uwm.desktop \
	xde/xsessions/velox.desktop \
	xde/xsessions/vtwm.desktop \
	xde/xsessions/waimea.desktop \
	xde/xsessions/wind.desktop \
	xde/xsessions/windwm.desktop \
	xde/xsessions/wm2.desktop \
	xde/xsessions/wmaker.desktop \
	xde/xsessions/wmii.desktop \
	xde/xsessions/wmx.desktop \
	xde/xsessions/xdwm.desktop \
	xde/xsessions/xfce.desktop \
	xde/xsessions/yeahwm.desktop

