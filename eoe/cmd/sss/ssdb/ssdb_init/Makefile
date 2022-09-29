#!smake
#
# Makefile for startup scripts and binaries of ssdb
#
include $(ROOT)/usr/include/make/commondefs
include ../../include/ssscommondefs
OBJECT_STYLE=N32
OPTIMIZER=-O
SRCDIR = .

SWSUBSYS = $(SSS_EOE)

LCDEFS  = -DOS_UNIX -DPTHEAD_THREAD

LCINCS = -nostdinc -I$(SRCDIR)/../../ssdbserver/include \
	 -I$(ROOT)/usr/include -I$(ROOT)/usr/include/sss

LLDLIBS = -lssdb
LD_QUICKSTART_INFO=
TARGETS = ssdbinit sssevents
INITPROGS = $(TARGETS) 

CFILES  = ssdbinit.c sssevents.c

OFILES  = $(CFILES:.c=.o)

SSDBINIT_OBJS = ssdbinit.o

COMMONPREF = SSS

$(COMMONPREF)install default: $(TARGETS)

include $(COMMONRULES)

clean clobber install:  $(COMMONPREF)$$@

default: $(TARGETS)

install: default
	$(INSTALL) -idb $(SWSUBSYS) -F /usr/sbin -m 755 -src ssdbinit espdbinit 
	$(INSTALL) -idb $(SWSUBSYS) -F /usr/sbin -m 755 -src sssevents espevents
	$(INSTALL) -idb $(SWSUBSYS) -F /var/esp/init/eventlist -m 644 base_events
	$(INSTALL) -idb $(SWSUBSYS) -F /var/esp/init/datafiles -m 644 -src ssdbschema espschema
	$(INSTALL) -idb $(SWSUBSYS) -F /var/esp/init/datafiles -m 644 toolconfig
	$(INSTALL) -idb $(SWSUBSYS) -F /etc/init.d -m 755 -src sss esp
	${INSTALL} -idb $(SWSUBSYS) -F /etc/rc2.d -lns /etc/init.d/esp S76esp
	${INSTALL} -idb $(SWSUBSYS) -F /etc/rc0.d -lns /etc/init.d/esp K24esp
