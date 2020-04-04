AUTOMAKE_OPTIONS = -Wall -Werror foreign

EXTRA_DIST= \
			Authors.md \
			ChangeLog.md \
			Contributing.md \
			MID.conf \
			README.md \
			Reporting.md

install-exec-local:
	ln -s -i $(bindir)/MID $(bindir)/mid
	ln -s -i $(man1dir)/MID.1 $(man1dir)/mid.1

uninstall-local: 
	rm -rf $(bindir)/mid
	rm -rf $(man1dir)/mid.1
	 
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
