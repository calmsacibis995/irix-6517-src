
include	$(ROOT)/usr/include/make/commondefs

TARGETS=tr
CFILES=mtr.c str.c tr.c
LLDLIBS=-lw

default: $(TARGETS)

include $(COMMONRULES)

tr: $(OBJECTS)
	$(CCF) $(OBJECTS) $(LDFLAGS) -o $@

test: tr
	cd testsuite; $(MAKE) clean; $(MAKE) TR=../tr

install: default
	$(INSTALL) -F /usr/bin $(TARGETS)
