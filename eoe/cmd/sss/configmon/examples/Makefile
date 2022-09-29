#!smake
#
# Makefile for ConfigMon examples
#
#ident "$Header: "

OBJECT_STYLE=n32
include $(ROOT)/usr/include/make/commondefs

LD_QUICKSTART_INFO=

DEPTH       = ..
VERSION   	= 1.0.a
GCDEFS    	= -DCM_DEBUG -DALLOC_DEBUG
LCDEFS    	= -I. -I -g -DVERSION=\"$(VERSION)\"
LIBS      	= -lconfigmon -lhwconfig -lswconfig -lklib -lutil -lalloc -lssdb
TARGETS		= apitest hwconfig swconfig hwalk swalk select_test

COMMONPREF  = configmon_

all:default 

include $(COMMONRULES)

clean clobber install: $(COMMONPREF)$$@

apitest: apitest.o 
	$(CCF) -o $@ apitest.o ../configmon_print.o $(LDFLAGS) $(LIBS)

select_test: select_test.o 
	$(CCF) -o $@ select_test.o ../configmon_print.o $(LDFLAGS) $(LIBS)

hwconfig: hwconfig.o 
	$(CCF) -o $@ hwconfig.o ../configmon_print.o $(LDFLAGS) $(LIBS)

swconfig: swconfig.o 
	$(CCF) -o $@ swconfig.o ../configmon_print.o $(LDFLAGS) $(LIBS)

hwalk: hwalk.o 
	$(CCF) -o $@ hwalk.o ../configmon_print.o $(LDFLAGS) $(LIBS)

swalk: swalk.o 
	$(CCF) -o $@ swalk.o ../configmon_print.o $(LDFLAGS) $(LIBS)

default: $(TARGETS) $(_FORCE)

$(COMMONPREF)install: default
	${INSTALL} -F /usr/bin ConfigMon

