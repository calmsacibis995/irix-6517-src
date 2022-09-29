#!smake
# $Revision: 1.2 $
#
include ${ROOT}/usr/include/make/commondefs

INCEFS= -I../lib
LCDEFS= -DEFS ${ECDEFS}
LCINCS= ${INCEFS} ${ECINCS}

CFILES= libmload.c

TARGETS= libmload.a

${TARGETS}: ${OBJECTS}
	${AR} crl $@ ${OBJECTS}

install:
	$(INSTALL) -F /usr/share/src/irix/examples/mload libmload.c
	$(INSTALL) -F /usr/share/src/irix/examples/mload Makefile
