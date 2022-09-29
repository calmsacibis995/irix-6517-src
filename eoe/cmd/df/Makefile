#!smake
#ident "$Revision: 1.13 $"

BASEVERSION=n32bit
include $(ROOT)/usr/include/make/cmdcommondefs

COMMANDS=df
LLDLIBS=-ldisk

default:$(TARGETS)

include $(CMDCOMMONRULES)

n32bitinstall: default
	${INSTALL} -m 4755 -F /sbin ${COMMANDS}
	${INSTALL} -F /usr/sbin -lns ../../sbin/${COMMANDS} ${COMMANDS}
	${INSTALL} -F /usr/bin -lns ../../sbin/${COMMANDS} devnm
	${INSTALL} -F /etc -ln ../sbin/${COMMANDS} devnm
	${INSTALL} -F /sbin -lns ${COMMANDS} devnm
	${INSTALL} -F /usr/bin -lns ../../sbin/${COMMANDS} ${COMMANDS}
