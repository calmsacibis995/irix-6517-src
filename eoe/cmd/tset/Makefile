#ident "$Revision: 1.1 $"

include $(ROOT)/usr/include/make/commondefs

CFILES= tset.c
TARGETS= tset
LCOPTS=-signed
LLDLIBS=-ltermcap

default:$(TARGETS)

include $(COMMONRULES)

install: default
	$(INSTALL) -F /usr/bin $(TARGETS)
