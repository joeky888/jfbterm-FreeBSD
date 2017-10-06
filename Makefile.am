bin_PROGRAMS = jfbterm
jfbterm_SOURCES = \
		main.c main.h \
		sequence.h \
		console.c console.h \
		framebuffer.c framebuffer.h \
		memctl.c memctl.h \
		accessor.c accessor.h \
		palette.c palette.h \
		eastasianwidth.c eastasianwidth.h \
		font.c font.h \
		picofont.c picofont.h \
		getcap.c getcap.h \
		pcf.c pcf.h \
		pen.c pen.h \
		term.c term.h \
		utilities.c utilities.h \
		privilege.c privilege.h \
		csv.c csv.h \
		vterm.c vterm.h \
		vtermlow.c vtermlow.h \
		cursor.c cursor.h \
		splash-bmp.c splash-bmp.h \
		splash-png.c splash-png.h \
		splash.c splash.h \
		bell.c bell.h \
		screensaver.c screensaver.h \
		mouse.c mouse.h \
		clipboard.c clipboard.h \
		keyboard.c keyboard.h
jfbterm_LDADD = $(LIBPNG_LIBS)
man_MANS = jfbterm.1 jfbterm.conf.5

install-exec-hook:
	chown 0:0 $(DESTDIR)$(bindir)/jfbterm
	chmod ug+s $(DESTDIR)$(bindir)/jfbterm

install-data-local:
	$(INSTALL_DATA) $(srcdir)/jfbterm.conf.sample.`uname -s` $(DESTDIR)$(sysconfdir)/jfbterm.conf.sample
	$(mkinstalldirs) $(DESTDIR)$(pkgdatadir)/fonts
	$(INSTALL_DATA) $(srcdir)/fonts/*.pcf.gz $(DESTDIR)$(pkgdatadir)/fonts
	$(INSTALL_DATA) $(srcdir)/termcap.jfbterm $(DESTDIR)$(pkgdatadir)/termcap.jfbterm
	$(INSTALL_DATA) $(srcdir)/terminfo.jfbterm $(DESTDIR)$(pkgdatadir)/terminfo.jfbterm

