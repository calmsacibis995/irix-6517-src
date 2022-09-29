#!smake
#ident "$Revision: 1.7 $"

OBJECT_STYLE=N32
include $(ROOT)/usr/include/make/commondefs

CFILES=swap.c
TARGETS=swap
LLDLIBS=-ldisk
LLDOPTS=-Wl,-I,$(ROOTRLDNAME_N32)
LCOPTS=-fullwarn

default:$(TARGETS)

include $(COMMONRULES)

install: default
	$(INSTALL) -F /sbin -idb "mr" $(TARGETS)
	$(INSTALL) -F /etc -idb "mr" -lns ../sbin/$(TARGETS) $(TARGETS)
