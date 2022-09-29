#
# Makefile to create IDE libraries.
#
#ident  "arcs/ide/EVEREST/io4/vme/Makefile $Revision: 1.4 $"
#
DIAGDEPTH=../../..
include ${DIAGDEPTH}/Makedefs

CFILES=vme_lpbk.c cdsio_data.c cdsio_intr.c vme_uts.c vme_buserr.c vme_intr.c\
	vme_xintr.c  vme_dmaxfer.c vme_dmaeng.c Catalog_vme.c vme_reg.c \
	fchip_reg.c

HFILES=vmedefs.h cdsio.h

ASFLAGS= -Wa,-r4000 -mips3 -DLOCORE -G 0 -DLANGUAGE_ASSEMBLY

#LLCINCS= -I ${DIAGDEPTH}

include ${DIAGDEPTH}/Makerules
