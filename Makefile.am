AUTOMAKE_OPTIONS = -Wall -Werror foreign

distclean-local:
	rm -rf doc/*.1
	rm -rf  *.m4
	rm -rf *.in~
	rm -rf autom4te.cache

include src/Makefile.am

if TXT2MAN
include doc/Makefile.am
endif
