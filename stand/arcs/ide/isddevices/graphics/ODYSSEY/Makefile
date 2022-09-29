# Mgras makefile
#
DIAGDEPTH= ../../..
include ${DIAGDEPTH}/Makedefs


IP30_X=\#
$(CPUBOARD)_X=
$(CPUBOARD)_$(COMPILATION_MODEL)X=

#include for extra msg_printf's
#LDEFINES=-DDEBUG
LDEFINES=$(HQNUM) -DLDEFINES -Xcpluscomm
LINCLUDES=-I../../include

64_Z=\#
$(COMPILATION_MODEL)_Z=

BASE_LINCLUDES=-I../../include -I../${DIAGDEPTH}/lib/libsk/graphics/ODYSSEY
$(64_Z)LINCLUDES= $(BASE_LINCLUDES)

CFILES= odsy_pon.c

COMMONPREF = ODYSSEY

_lib ${COMMONTARGS}: ${COMMONPREF}$$@

_plib: ${COMMONPREF}$$@

${COMMONTARGS}: ${COMMONPREF}$$@

default: _lib

# override common POBJECTS so only necessary files compile for PROM
PCFILES= odsy_pon.c 
POBJECTS= ${PCFILES:.c=.o}

include ${DIAGDEPTH}/Makerules

