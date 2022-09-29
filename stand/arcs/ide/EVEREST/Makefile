#
# Makefile to create IDE library.
#
# Usage: make
#ident  "arcs/ide/EVEREST/Makefile 
#$Revision: 1.8 $"
#
DIAGDEPTH=..
include ${DIAGDEPTH}/Makedefs

# Add any common directory name in the following line.
CMNDIRS=mc3 io4 ebus uif lib

IP19_X = \#
IP20_X = \#
IP25_X = \#

$(CPUBOARD)_X=

IP19DIRS=IP19 ${CMNDIRS}
IP21DIRS=IP21 ${CMNDIRS}
IP25DIRS=${CMNDIRS}

$(IP21_X)CPUSUBDIRS=${IP21DIRS}
$(IP25_X)CPUSUBDIRS=${IP25DIRS}
$(IP19_X)CPUSUBDIRS=${IP19DIRS}

LTARGETS= LIBNAME
TARGETS= ${LTARGETS}

CFILES= ${PRODUCT}_catalog.c

COMMONPREF=EVEREST
LINCLUDES= -I -I${DIAGDEPTH}/${PRODUCT}/include
SUBDIRS= ${CPUSUBDIRS}

include ${DIAGDEPTH}/Makerules

default: LIBNAME ${COMMONPREF}$$@

LIBNAME: $(TDIRS) ${_FORCE}
	@for d in ${CPUSUBDIRS}; do \
		echo "======\tcd $$d; ${MAKE} _lib"; \
		cd $$d; ${MAKE} _lib; cd ..; \
	    done

# Common rules which we implement at this level
# and also pass on to sub-makes
$(COMMONTARGS) clobber: 
	@for i in ${SUBDIRS}; do \
	echo "======\tcd $$i; ${MAKE} $@"; \
		cd $$i; ${MAKE} $@; cd ..; \
	done
	@if [ $@ = clobber ]; then rm -f $(LLDIRT); fi

${PRODUCT}_catalog.c: Catalog.sh ${_FORCE}
	${SHELL} Catalog.sh > $@
