#ident	libcurses/Makefile $Revision: 1.1 $

DIRS = screen # demos only done if specifically requested

include	$(ROOT)/usr/include/make/commondefs

default install .DEFAULT:
	@for d in ${DIRS}; do \
		${ECHO} "\tcd $$d; ${MAKE} $@"; \
		cd $$d; ${MAKE} LCOPTS="$(LIBGNUM)" $@; cd ..; \
	done

demos:
	cd demo;LCOPTS="$(LIBGNUM)" $(MAKE)
