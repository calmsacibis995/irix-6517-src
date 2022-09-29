# Makefile for EVEREST IDE user interface routines
#
#ident "arcs/ide/EVEREST/uif/Makefile: $Revision: 1.5 $"
#
DIAGDEPTH= ../..
include ${DIAGDEPTH}/Makedefs

CFILES= msg_printf.c busy.c status.c

# override common POBJECTS so only necessary files compile for PROM
PCFILES= msg_printf.c
POBJECTS= ${PCFILES:.c=.o}

include ${DIAGDEPTH}/Makerules
