#!smake
#
# Make the RIP-routed deamon
#
# $Revision: 1.5 $

include	$(ROOT)/usr/include/make/commondefs

CFILES	=rtquery.c md5.c

TARGETS	=rtquery

LCOPTS	=-signed -fullwarn

default:$(TARGETS)

install:default
	$(INSTALL) -F /usr/etc "$(TARGETS)"

include $(ROOT)/usr/include/make/commonrules

rtquery:$(OBJECTS)
	$(CCF) $(OBJECTS) $(LDFLAGS) -o $@
