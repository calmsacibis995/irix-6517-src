#!smake

BASEVERSION=n32bit

include ${ROOT}/usr/include/make/cmdcommondefs

LLDLIBS=-lgen

COMMANDS=tlink

default: ${TARGETS}

include ${CMDCOMMONRULES}

n32bitinstall: default
	${INSTALL} -F /usr/sbin $(COMMANDS)
