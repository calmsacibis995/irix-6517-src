#ident "$Revision: 1.2 $"

include $(ROOT)/usr/include/make/commondefs

CFILES=script.c
TARGETS=script
LCOPTS=-D_BSD_COMPAT
LLDLIBS=-lbsd

default:$(TARGETS)

include $(COMMONRULES)

install: default
	$(INSTALL) -F /usr/bin $(TARGETS)
