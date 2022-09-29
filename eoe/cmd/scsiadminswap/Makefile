#ident "$Revision: 1.2 $"

include $(ROOT)/usr/include/make/commondefs

LLDLIBS = -lds -Wl,-woff,84
LCFLAGS = -fullwarn

CFILES=scsiadminswap.c
TARGETS= scsiadminswap

default:$(TARGETS)

include $(COMMONRULES)

install: default
	${INSTALL} -F /usr/sbin $(TARGETS)
