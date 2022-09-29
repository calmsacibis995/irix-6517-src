#
# Makefile to create IDE libraries.
#
#ident  "arcs/ide/EVEREST/io4/Makefile "
#$Revision: 1.4 $"
#
DIAGDEPTH=../..
include ${DIAGDEPTH}/Makedefs

IO4D= vme scsi enet graphics pbus

COMMONPREF=IO4

CFILES= check_iocfg.c check_hinv.c io4_regtest.c io4_pioerr.c  \
	mapram.c Catalog_io4.c
HFILES=

LINCLUDES= -I -I${DIAGDEPTH}/EVEREST/include
LDEFINES=

LDIRT=

_lib ${COMMONTARGS}: ${COMMONPREF}$$@
	@for d in ${IO4D}; do \
  		echo "=======\tcd $$d; ${MAKE} $@"; \
  		(cd $$d; exec ${MAKE} $@) \
	done


include ${DIAGDEPTH}/Makerules
