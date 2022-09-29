#!smake
#ident "$Revision: 1.6 $"

include $(ROOT)/usr/include/make/commondefs

CFILES=find.c
TARGETS=find
LLDLIBS=
LLDOPTS=-Wl,-I,$(ROOTRLDNAME)

default:$(TARGETS)

include $(COMMONRULES)

install: default
	${INSTALL} -F /sbin $(TARGETS)
	${INSTALL} -F /usr/bin -lns ../../sbin/$(TARGETS) $(TARGETS)
