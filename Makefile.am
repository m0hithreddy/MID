AUTOMAKE_OPTIONS = -Wall -Werror foreign

EXTRA_DIST= \
			Authors.md \
			ChangeLog.md \
			Contributing.md \
			MID.conf \
			README.md \
			Reporting.md


install-exec-local:
	$(MKDIR_P) "$(DESTDIR)$(bindir)"
	$(LN_S) $(DESTDIR)$(bindir)/MID $(DESTDIR)$(bindir)/mid
	$(MKDIR_P) "$(DESTDIR)$(man1dir)"
	$(LN_S) $(DESTDIR)$(man1dir)/MID.1 $(DESTDIR)$(man1dir)/mid.1

uninstall-local: 
	rm -rf $(DESTDIR)$(bindir)/mid
	rm -rf $(DESTDIR)$(man1dir)/mid.1
	 
clean-local:
	rm -rf doc/*.1

distclean-local:
	rm -rf doc/*.1
	rm -rf  *.m4
	rm -rf *.in~
	rm -rf autom4te.cache
	rm -rf $(distdir)*

include src/Makefile.am

if TXT2MAN
include doc/Makefile.am
endif

sysconf_DATA = MID.conf
