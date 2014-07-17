
AM_CFLAGS = $(X11_CFLAGS) $(XEXT_CFLAGS) $(XSCRNSAVER_CFLAGS) $(XINERAMA_CFLAGS) $(XRANDR_CFLAGS) $(CAIRO_CFLAGS) $(GDK_CFLAGS) $(GTK_CFLAGS) $(WNCK_CFLAGS)
AM_LDFLAGS = $(WNCK_LIBS) $(GTK_LIBS) $(GDK_LIBS) $(CAIRO_LIBS) $(XINERAMA_LIBS) $(XRANDR_LIBS) $(XSCRNSAVER_LIBS) $(XEXT_LIBS) $(X11_LIBS)

bin_PROGRAMS = \
	xde-autostart \
	xde-chooser \
	xde-input \
	xde-logout \
	xde-session \
	xde-wmproxy \
	xde-xchooser \
	xde-xlogin \
        xde-xsession


xde_autostart_CPPFLAGS = -DNAME=\"xde-autostart\"
xde_autostart_SOURCES = xde-autostart.c xde-autostart.h
xde_autostart_CFLAGS = $(AM_CFLAGS) $(ICE_CFLAGS) $(SMLIB_CFLAGS)
xde_autostart_LDFLAGS = $(SMLIB_LIBS) $(ICE_LIBS) $(AM_LDFLAGS)

xde_chooser_CPPFLAGS = -DNAME=\"xde-chooser\"
xde_chooser_SOURCES = xde-chooser.c xde-chooser.h
xde_chooser_CFLAGS = $(AM_CFLAGS)
xde_chooser_LDFLAGS = $(AM_LDFLAGS)

xde_input_CPPFLAGS = -DNAME=\"xde-input\"
xde_input_SOURCES = xde-input.c xde-input.h
xde_input_CFLAGS = $(AM_CFLAGS)
xde_input_LDFLAGS = $(AM_LDFLAGS)

xde_logout_CPPFLAGS = -DNAME=\"xde-logout\"
xde_logout_SOURCES = xde-logout.c xde-logout.h
xde_logout_CFLAGS = $(AM_CFLAGS) $(DBUS_GLIB_CFLAGS)
xde_logout_LDFLAGS = $(DBUS_GLIB_LIBS) $(AM_LDFLAGS)

xde_session_CPPFLAGS = -DNAME=\"xde-session\"
xde_session_SOURCES = xde-session.c xde-session.h
xde_session_CFLAGS = $(AM_CFLAGS) $(GLIB_CFLAGS) $(GOBJ_CFLAGS) $(UNIQUE_CFLAGS)
xde_session_LDFLAGS = $(UNIQUE_LIBS) $(GOBJ_LIBS) $(GLIB_LIBS) $(AM_LDFLAGS)

xde_wmproxy_CPPFLAGS = -DNAME=\"xde-wmproxy\"
xde_wmproxy_SOURCES = xde-wmproxy.c xde-wmproxy.h
xde_wmproxy_CFLAGS = $(AM_CFLAGS) $(ICE_CFLAGS) $(SMLIB_CFLAGS)
xde_wmproxy_LDFLAGS = $(SMLIB_LIBS) $(ICE_LIBS) $(AM_LDFLAGS)

xde_xchooser_CPPFLAGS = -DNAME=\"xde-xchooser\"
xde_xchooser_SOURCES = xde-xchooser.c xde-xchooser.h
xde_xchooser_CFLAGS = $(AM_CFLAGS) $(XDMCP_CFLAGS)
xde_xchooser_LDFLAGS = $(XDMCP_LIBS) $(AM_LDFLAGS)

xde_xlogin_CPPFLAGS = -DNAME=\"xde-xlogin\"
xde_xlogin_SOURCES = xde-xlogin.c xde-xlogin.h
xde_xlogin_CFLAGS = $(AM_CFLAGS) $(XFT_CFLAGS)
xde_xlogin_LDFLAGS = $(XFT_LIBS) $(AM_LDFLAGS)

xde_xsession_CPPFLAGS = -DNAME=\"xde-xsession\"
xde_xsession_CFLAGS = $(AM_CFLAGS) $(XFT_CFLAGS)
xde_xsession_LDFLAGS = $(XFT_LIBS) $(AM_LDFLAGS)
xde_xsession_SOURCES = \
                xde-xsession.c xde-xsession.h \
                xsessions.c xsession.h \
                autostart.c autostart.h \
                display.c display.h

