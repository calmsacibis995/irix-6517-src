#
# Makefile for hinv, the hardware inventory command.
#
# $Revision: 1.11 $
#
include $(ROOT)/usr/include/make/commondefs

CFILES=hinv.c
TARGETS=hinv
LLDOPTS=-Wl,-I,$(ROOTRLDNAME)

LMKDEPFLAGS= $(NULLSUFFIX_MKDEPFLAG)

default: $(TARGETS)

include $(COMMONRULES)

install: default
	$(INSTALL) -idb "std.sw.unix mr" -F /sbin hinv
	$(INSTALL) -idb "std.sw.unix mr" -F /usr/bin -lns ../../sbin/hinv hinv
