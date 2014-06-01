
AM_CFLAGS = $(X11_CFLAGS) $(XINERAMA_CFLAGS) $(XRANDR_CFLAGS) $(GDK_CFLAGS) $(GTK_CFLAGS) $(WNCK_CFLAGS)

bin_PROGRAMS = \
	xde-chooser \
	xde-input \
	xde-logout \
	xde-session \
	xde-xchooser

xde_chooser_CPPFLAGS = -DNAME=\"xde-chooser\"
xde_chooser_SOURCES = src/xde-chooser.c src/xde-chooser.h
xde_chooser_LDADD = $(WNCK_LIBS) $(GTK_LIBS) $(GDK_LIBS) $(XINERAMA_LIBS) $(XRANDR_LIBS) $(X11_LIBS)

xde_input_CPPFLAGS = -DNAME=\"xde-input\"
xde_input_SOURCES = src/xde-input.c src/xde-input.h
xde_input_LDADD = $(WNCK_LIBS) $(GTK_LIBS) $(GDK_LIBS) $(XINERAMA_LIBS) $(XRANDR_LIBS) $(X11_LIBS)

xde_logout_CPPFLAGS = -DNAME=\"xde-logout\"
xde_logout_SOURCES = src/xde-logout.c src/xde-logout.h
xde_logout_LDADD = $(WNCK_LIBS) $(GTK_LIBS) $(GDK_LIBS) $(XINERAMA_LIBS) $(XRANDR_LIBS) $(X11_LIBS)

xde_session_CPPFLAGS = -DNAME=\"xde-session\"
xde_session_SOURCES = src/xde-session.c src/xde-session.h
xde_session_LDADD = $(WNCK_LIBS) $(GTK_LIBS) $(GDK_LIBS) $(XINERAMA_LIBS) $(XRANDR_LIBS) $(X11_LIBS)

xde_xchooser_CPPFLAGS = -DNAME=\"xde-xchooser\"
xde_xchooser_SOURCES = src/xde-xchooser.c src/xde-xchooser.h
xde_xchooser_LDADD = $(WNCK_LIBS) $(GTK_LIBS) $(GDK_LIBS) $(XINERAMA_LIBS) $(XRANDR_LIBS) $(X11_LIBS)
