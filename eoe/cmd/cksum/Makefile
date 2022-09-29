#
# Makefile for cksum(1).
#
#ident "$Revision: 1.3 $"

include	$(ROOT)/usr/include/make/commondefs

CFILES= cksum.c crc.c print.c sum1.c sum2.c
TARGETS= cksum

default: $(TARGETS)

include $(COMMONRULES)

cksum: $(OBJECTS)
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@

install: default
	$(INSTALL) -F /usr/bin cksum
