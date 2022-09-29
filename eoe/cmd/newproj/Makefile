#ident "$Revision: 1.2 $"

include $(ROOT)/usr/include/make/commondefs

CFILES=newproj.c
TARGETS=newproj
LCOPTS=-fullwarn
LLDLIBS=

default:$(TARGETS)

include $(COMMONRULES)

install: default
	$(INSTALL) -u root -g sys -m 4755 -F /usr/bin $(TARGETS)
