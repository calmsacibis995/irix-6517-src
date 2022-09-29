#ident  "ide/godzilla/heart/Makefile:  $Revision: 1.10 $"
#
# heart diag makefile
# ported from IP26

DIAGDEPTH= ../..
include ${DIAGDEPTH}/Makedefs

ASFILES= hr_blast.s
CFILES=	hr_regs.c 	\
	hr_piu_acc.c	\
	hr_misc.c	\
	hr_intr.c	\
	hr_util.c	\
	hr_pio_wcr.c	\
	hr_byte.c	\
	hr_alias.c

include ${DIAGDEPTH}/Makerules
LINCLUDES = -I../include
