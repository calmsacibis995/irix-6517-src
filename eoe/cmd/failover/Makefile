#ident "$Revision: 1.2 $

include $(ROOT)/usr/include/make/commondefs

LLDLIBS=-lds -Wl,-woff,84

LCOPTS=-Wl,-I,$(ROOTRLDNAME)

CFILES=foconfig.c
SBIN_TARGETS= foconfig

default:$(SBIN_TARGETS)

include $(COMMONRULES)

install: default
	${INSTALL} -F /sbin $(SBIN_TARGETS)
