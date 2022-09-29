#!smake
#
# Makefile for build EventMon daemon
#
include $(ROOT)/usr/include/make/commondefs
include ../include/ssscommondefs

SRCDIR       = .
OBJECT_STYLE = N32
CFILES       = eventmond.c
SCRIPT       = eventmonitor
OFILES       = $(CFILES:.c=.o)
#LCDEFS	     = -I. 
LCINCS       = -nostdinc -I$(ROOT)/usr/include -I$(SRCDIR)/../include \
		-I$(SRCDIR)/../libsem/include
LIBS         = -lpthread
OPTIMIZER    = -g0 -O
TARGETS      = eventmond
INITPROGS    = $(TARGETS)
COMMONPREF   = sss_

LD_QUICKSTART_INFO=

all: default

$(COMMONPREF)install default: $(TARGETS)

include $(COMMONRULES)

clean clobber install:  $(COMMONPREF)$$@

default: eventmond_default

eventmond_default: $(TARGETS)

$(TARGETS):  $(OFILES)
	$(CCF) -woff 1048 -o $(TARGETS) $(OFILES) $(LCINCS) $(LIBS)

install: default
	$(INSTALL) -idb $(SSS_EOE) -F /usr/etc $(TARGETS)
	$(INSTALL) -idb $(SSS_EOE) -F /etc/init.d $(SCRIPT)	
