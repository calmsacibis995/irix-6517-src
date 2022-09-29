#ident  "ide/godzilla/scsi/Makefile:  $Revision: 1.3 $"
#
# scsi diag makefile

DIAGDEPTH= ../..
include ${DIAGDEPTH}/Makedefs

ASFILES=
CFILES=	ql_diag.c scsi_regs.c

include ${DIAGDEPTH}/Makerules

# place all diag header files in ide/godzilla/include dir
# LINCLUDES is used in stand/arcs/ide/Makedefs
LINCLUDES = -I../include
