#
# Makefile for the IDE libraries.
#
#ident  "arcs/ide/EVEREST/io4/scsi/Makefile $Revision: 1.10 $"
#
DIAGDEPTH=../../..
include ${DIAGDEPTH}/Makedefs

CFILES= scsilib.c s1_regtest.c regs_95a.c scsi_intr.c scsi_dmaintr.c \
	scsi_dmaxfer.c scsi_self.c Catalog_scsi.c 
HFILES= scsidefs.h ${DIAGDEPTH}/EVEREST/include/io4_tdefs.h

LINCLUDES= -I -I${DIAGDEPTH}/EVEREST/include
LDEFINES=

include ${DIAGDEPTH}/Makerules
