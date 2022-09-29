#
# Makefile to create IDE library.
#
#ident  "arcs/ide/EVEREST/IP21/Makefile $Revision: 1.4 $"
#
DIAGDEPTH=../..
include ${DIAGDEPTH}/Makedefs

SUBDIRS= tfp

LTARGETS= ${LIBNAME}
TARGETS= ${LTARGETS}

COMMONPREF=IP21

CFILES=
HFILES=

LLCINCS=
LLCDEFS=


include ${DIAGDEPTH}/Makerules

default ${LIBNAME} _lib clobber: $(TDIRS) ${_FORCE}
	@for d in ${SUBDIRS}; do \
		echo "=======\tcd $$d; ${MAKE} $@"; \
		cd $$d; ${MAKE} $@; cd ..; \
	done
