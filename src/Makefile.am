
AM_CFLAGS = $(X11_CFLAGS) $(XEXT_CFLAGS) $(XSCRNSAVER_CFLAGS) $(XINERAMA_CFLAGS) $(XRANDR_CFLAGS) $(CAIRO_CFLAGS) $(GDK_CFLAGS) $(GTK_CFLAGS) $(WNCK_CFLAGS)
AM_LDFLAGS = $(WNCK_LIBS) $(GTK_LIBS) $(GDK_LIBS) $(CAIRO_LIBS) $(XINERAMA_LIBS) $(XRANDR_LIBS) $(XSCRNSAVER_LIBS) $(XEXT_LIBS) $(X11_LIBS)

bin_PROGRAMS = \
	xde-chooser \
	xde-input \
	xde-logout \
	xde-session \
	xde-xchooser

xde_chooser_CPPFLAGS = -DNAME=\"xde-chooser\"
xde_chooser_SOURCES = xde-chooser.c xde-chooser.h

xde_input_CPPFLAGS = -DNAME=\"xde-input\"
xde_input_SOURCES = xde-input.c xde-input.h

xde_logout_CPPFLAGS = -DNAME=\"xde-logout\"
xde_logout_SOURCES = xde-logout.c xde-logout.h
xde_logout_CFLAGS = $(AM_CFLAGS) $(DBUS_GLIB_CFLAGS)
xde_logout_LDFLAGS = $(DBUS_GLIB_LIBS) $(AM_LDFLAGS)

xde_session_CPPFLAGS = -DNAME=\"xde-session\"
xde_session_SOURCES = xde-session.c xde-session.h
xde_session_CFLAGS = $(AM_CFLAGS) $(GLIB_CFLAGS) $(GOBJ_CFLAGS) $(UNIQUE_CFLAGS)
xde_session_LDFLAGS = $(UNIQUE_LIBS) $(GOBJ_LIBS) $(GLIB_LIBS) $(AM_LDFLAGS)

xde_xchooser_CPPFLAGS = -DNAME=\"xde-xchooser\"
xde_xchooser_SOURCES = xde-xchooser.c xde-xchooser.h
xde_xchooser_CFLAGS = $(AM_CFLAGS) $(XDMCP_CFLAGS)
xde_xchooser_LDFLAGS = $(XDMCP_LIBS) $(AM_LDFLAGS)
