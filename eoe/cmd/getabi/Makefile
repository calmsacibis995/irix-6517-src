# $Revision: 1.1 $

include $(ROOT)/usr/include/make/commondefs

CFILES=getabi.c
TARGETS=getabi

default:$(TARGETS)

include $(COMMONRULES)

install: default
	${INSTALL} -F /usr/sbin getabi
