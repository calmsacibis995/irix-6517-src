#
# Makefile to create IDE libraries.
#
#ident  "arcs/ide/EVEREST/IP19/r4k/Makefile $Revision: 1.2 $"
#
DIAGDEPTH=../../..
include ${DIAGDEPTH}/Makedefs

R4KD= cache tlb

COMMONPREF=R4000

CFILES=
HFILES=

LLCINCS= -I -I${DIAGDEPTH}/../include
LLCDEFS=

_lib ${COMMONTARGS}: ${COMMONPREF}$$@
	@for d in ${R4KD}; do \
		echo "=======\tcd $$d; ${MAKE} _lib"; \
		(cd $$d; exec ${MAKE} $@) \
	done

include ${DIAGDEPTH}/Makerules

