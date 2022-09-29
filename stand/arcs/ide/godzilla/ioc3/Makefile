#ident  "ide/godzilla/ioc3/Makefile:  $Revision: 1.7 $"
#
#  This makes the diagnostics for ioc3 tests.
#  This Makefile was originally ported from fforward, then stripped of 
#  the other board options (IP20, IP22, IP26)
#

DIAGDEPTH= ../..

include ${DIAGDEPTH}/Makedefs

HFILES = 
CFILES= ioc3_regs.c ioc3_sram.c duart_regs.c rtc_regs.c
ASFILES=

PCFILES=pon_pckm.c
POBJECTS=${PCFILES:.c=.o}

include ${DIAGDEPTH}/Makerules

# place all diag header files in ide/godzilla/include dir
# LINCLUDES is used in stand/arcs/ide/Makedefs
LINCLUDES = -I../include  
