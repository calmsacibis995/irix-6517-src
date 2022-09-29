#ident "$Revision: 1.6 $"

include $(ROOT)/usr/include/make/commondefs

TARGETS=prio_cp
LLDOPTS=-Wl,-I,$(ROOTRLDNAME)
LLDLIBS=-lprio

default:$(TARGETS)

include $(COMMONRULES)

install: default
	${INSTALL} -F /usr/share/src/irix/examples/prio/src -m 444 -src prio_cp.c prio_cp.c	
	${INSTALL} -F /usr/share/src/irix/examples/prio/src -m 444 -src Makefile.gifts Makefile
