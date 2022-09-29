#!smake
#
# Makefile for ConfigMon
#
#ident "$Header: "

OBJECT_STYLE=n32
include $(ROOT)/usr/include/make/commondefs

LD_QUICKSTART_INFO=

VERSION   	= 1.0.a
GCDEFS    	= -DALLOC_DEBUG 
LCDEFS    	= -I. -I -g -DVERSION=\"$(VERSION)\"
LIBS      	= -lkern -lhwconfig -lswconfig -lsym -lmem -lklib -lutil -lalloc \
			  -lconfigmon -lds -ldwarf -lelf -lssdb -leventmon -lexc
TARGETS		= configmon
CFILES    	= configmon.c sysinfo.c configmon_event.c configmon_print.c
OFILES    	= $(CFILES:.c=.o)

COMMONPREF  = configmon_

all:default 

default: $(COMMONPREF)default $(TARGETS)

include $(COMMONRULES)

clean clobber install: $(COMMONPREF)$$@

$(COMMONTARGS): $(_FORCE)

configmon: $(OFILES) $(_FORCE)
	$(CCF) -o $@ $(OFILES) $(LDFLAGS) $(LIBS) 

$(COMMONPREF)default: $(_FORCE)

$(COMMONPREF)install: default
	${INSTALL} -F /usr/sbin configmon
