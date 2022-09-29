#ident  "ide/godzilla/rad/Makefile:  $Revision: 1.3 $"
#
#  This makes the diagnostics for rad tests.
#  This Makefile was originally ported from fforward, then stripped of 
#  the other board options (IP20, IP22, IP26)
#

DIAGDEPTH= ../..

include ${DIAGDEPTH}/Makedefs

IP30_X=\#
$(CPUBOARD)_X=

$(IP30_X)EXTRA_C_FILES=
$(IP30_X)EXTRA_AS_FILES= 

HFILES = 
CFILES= RadCspaceTest.c RadStatusTest.c \
	RadUtils.c RadRamTest.c

ASFILES=

include ${DIAGDEPTH}/Makerules

# place all diag header files in ide/godzilla/include dir
# LINCLUDES is used in stand/arcs/ide/Makedefs
LINCLUDES = -I../include  
