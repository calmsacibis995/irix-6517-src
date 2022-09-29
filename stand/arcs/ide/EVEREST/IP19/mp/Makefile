#
# Makefile to create IDE libraries.
#
#ident  "arcs/ide/EVEREST/IP19/mp/Makefile $Revision: 1.6 $"
#
DIAGDEPTH=../../..
include ${DIAGDEPTH}/Makedefs

CFILES=Catalog_mp.c mpchk_cache.c mpclear_ip.c mpmem_rd.c mpchk_cstate.c \
	mpinval.c mpmem_wr.c
ASFILES=../bus_tags.s
HFILES=

LLCINCS=-I -I${DIAGDEPTH}/EVEREST/include -I${DIAGDEPTH}/EVEREST/IP19/r4k/cache
LLCOPTS=

include ${DIAGDEPTH}/Makerules
