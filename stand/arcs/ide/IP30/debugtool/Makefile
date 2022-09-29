#ident  "ide/godzilla/debugtool/Makefile:  $Revision: 1.2 $"
#
#  This makes the diagnostics for ioc3 tests.
#  This Makefile was originally ported from fforward, then stripped of 
#  the other board options (IP20, IP22, IP26)
#

DIAGDEPTH= ../..

include ${DIAGDEPTH}/Makedefs

HFILES =
CFILES= regs.c heart_regs.c bridge_regs.c xbow_regs.c ioc3_regs_ep.c duart_regs_ep.c scsi_regs_ep.c rad_regs.c phy_regs.c
ASFILES=

include ${DIAGDEPTH}/Makerules

# place all diag header files in ide/godzilla/include dir
# LINCLUDES is used in stand/arcs/ide/Makedefs
LINCLUDES = -I../../godzilla/include  
