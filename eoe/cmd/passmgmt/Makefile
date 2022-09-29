#
# Makefile for passmgmt(1M).
#
#ident "$Revision: 1.7 $"
include	${ROOT}/usr/include/make/commondefs

CFILES= passmgmt.c
DATEFILE= datemsk
TARGETS= passmgmt 

LCDEFS= -ignore_unresolved -D_SHAREII -DDO_PWEXPIRE

#LCOPTS= -Olimit 800
LLDLIBS =
LMKDEPFLAGS= ${NULLSUFFIX_MKDEPFLAG}

default: passmgmt

include ${COMMONRULES}

install: default 
	${INSTALL} -F /usr/sbin -m 4511 -u root -g sys passmgmt
	${INSTALL} -F /etc -m 0444 -u root -g sys $(DATEFILE)

