include	$(ROOT)/usr/include/make/commondefs

TARGETS = sed
CFILES = sed0.c sed1.c

default: $(TARGETS)

include $(COMMONRULES)

install: default
	$(INSTALL) -F /sbin sed
	$(INSTALL) -F /usr/bin -lns ../../sbin/sed sed

sed:	sed0.o sed1.o
	$(CCF) sed0.o sed1.o -o $@ $(LDFLAGS)

