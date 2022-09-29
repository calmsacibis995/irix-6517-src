#!smake
#
# Makefile for mrouted, a multicast router, and its auxiliary programs,
# mtrace, mapper and mrinfo.
#
# $Revision: 1.17 $

include $(ROOT)/usr/include/make/commondefs

IGMP_FILES=	igmp.c inet.c kern.c
IGMP_OBJS=	$(IGMP_FILES:.c=.o)
ROUTER_FILES=	callout.c config.c main.c prune.c route.c vif.c cfparse.c rsrr.c version.c
ROUTER_OBJS=	$(ROUTER_FILES:.c=.o)
CFILES=		$(IGMP_FILES) $(ROUTER_FILES)

TARGETS	=mrouted mtrace

LCOPTS	=
LCDEFS	=-DRSRR -DUSE_CAP_NET -DALLOW_BLACK_HOLES=1

default:$(TARGETS)

include $(COMMONRULES)

install:default
	$(INSTALL) -F /usr/etc "$(TARGETS)"
	$(INSTALL) -idb "config(suggest)" -F /etc \
		-m 644 -u root -g sys  mrouted.conf
	$(INSTALL) -F /usr/etc -lns ../../etc/mrouted.conf mrouted.conf

mrouted: $(ROUTER_OBJS) $(IGMP_OBJS)
	$(CCF) $(ROUTER_OBJS) $(IGMP_OBJS) $(LDFLAGS) -o $@

#
# Note: can't ship mrinfo and mapper due to restrictive Xerox copyright
LDIRT	= mrinfo mapper

mrinfo mapper: $$@.o $(IGMP_OBJS) 
	$(CCF) $(IGMP_OBJS)  $@.o $(LDFLAGS) -o $@
