#ident  "ide/godzilla/ecc/Makefile:  $Revision: 1.6 $"
#
# ecc diag makefile

DIAGDEPTH= ../..

include ${DIAGDEPTH}/Makedefs

ASFILES=  ecc_asm.s
CFILES=   ecc.c heart_ecc.c

include ${DIAGDEPTH}/Makerules

# place all godzilla-specific header files in godzilla/include
# LINCLUDES is used in stand/arcs/Makedefs
LINCLUDES = -I../include

