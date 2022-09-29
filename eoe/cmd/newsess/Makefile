#ident "$Revision: 1.4 $"

include $(ROOT)/usr/include/make/commondefs

TARGETS=newsess
CFILES=newsess.c
LCOPTS=
LLDLIBS=

default:$(TARGETS)

include $(COMMONRULES)

install: default
	$(INSTALL) -u root -g sys -m 755 -F /usr/bin $(TARGETS)
