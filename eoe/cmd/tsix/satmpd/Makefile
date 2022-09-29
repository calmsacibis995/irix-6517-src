#
# Makefile for satmpd(1).
#
#ident "$Revision: 1.6 $"

include	$(ROOT)/usr/include/make/commondefs

CFILES= atomic.c client.c debug.c generic_list.c list.c lrep.c main.c \
	match.c name.c ns.c parse.c ref.c request.c sem.c server.c thread.c \
	timer.c
TARGETS= satmpd

CONFIG_FILES= ATTRIDS REQATTR WEIGHTS localmap remotemap

#LCDEFS=-D_SGI_MP_SOURCE -DPTHREADS -fullwarn
LCDEFS=-D_SGI_MP_SOURCE -fullwarn
LLDLIBS=-lsatmp

all default: $(TARGETS)

include $(COMMONRULES)

satmpd: $(OBJECTS)
	$(CCF) $(OBJECTS) $(LDFLAGS) -o $@

install: all
	$(INSTALL) -F /usr/etc $(TARGETS)
	$(INSTALL) -dir /etc/satmpd
	$(INSTALL) -F /etc/satmpd $(CONFIG_FILES)
