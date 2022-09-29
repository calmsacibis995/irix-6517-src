#
# Makefile to create IDE libraries.
#
#ident  "arcs/ide/EVEREST/IP19/Makefile $Revision: 1.9 $"
#
DIAGDEPTH=../..
include ${DIAGDEPTH}/Makedefs

IP19D= r4k mp

COMMONPREF=IP19

CFILES= Catalog_IP19.c local_regtest.c cfig_regtest.c \
	counter.c intr_level0.c intr_level3.c intr_timer.c intr_group.c\
	wr_gatherer.c

LLCINCS= -I -I${DIAGDEPTH}/EVEREST/include
LDEFINES=

LDIRT=

_lib ${COMMONTARGS}: ${COMMONPREF}$$@
	@for d in ${IP19D}; do \
  		echo "=======\tcd $$d; ${MAKE} $@"; \
  		(cd $$d; exec ${MAKE} $@) \
	done


include ${DIAGDEPTH}/Makerules
