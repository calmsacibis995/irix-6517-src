#!smake
#	Makefile for the recover commmand used by the restore shell script
#   (installed in miniroot only for system recovery; initstates=3)

include $(ROOT)/usr/include/make/commondefs

# we override close and exec for tracing, shut up complaint
LCFLAGS=-Wl,-woff,85

TARGETS	= recover

default:	$(TARGETS)

include $(ROOT)/usr/include/make/commonrules

install: default
	$(INSTALL) -idb "mr" -F /sbin $(TARGETS)
