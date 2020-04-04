AUTOMAKE_OPTIONS = -Wall -Werror foreign

EXTRA_DIST= \
			Authors.md \
			ChangeLog.md \
			Contributing.md \
			MID.conf \
			README.md \
			Reporting.md
			
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
