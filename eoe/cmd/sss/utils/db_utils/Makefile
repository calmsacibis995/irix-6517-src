#!smake
#
# Makefile for db utilities
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
TARGETS = dbquery

INITPROGS = $(TARGETS) 

CFILES  = dbquery.c

OFILES  = $(CFILES:.c=.o)

SSDBINIT_OBJS = dbquery.o

COMMONPREF = SSS

$(COMMONPREF)install default: $(TARGETS)

include $(COMMONRULES)

clean clobber install:  $(COMMONPREF)$$@

default: $(TARGETS)

install: default
	$(INSTALL) -idb $(SWSUBSYS) -F /usr/sbin -m 755  -src dbquery espquery
	$(INSTALL) -idb $(SWSUBSYS) -F /usr/sbin -m 755  esparchive
