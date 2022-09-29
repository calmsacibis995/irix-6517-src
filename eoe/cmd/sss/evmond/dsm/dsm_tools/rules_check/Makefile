#!make
#
# Makefile for IRIX crash
#
#
# If you change the OBJECT_STYLE here ... then you should look 
# below for additional OBJECT_STYLE's.
#
include $(ROOT)/usr/include/make/commondefs
include ../../../../include/ssscommondefs

TARGETS = rules_check

#
# General definitions.
#

OBJECT_STYLE=N32
GCDEFS    = -DPARSE_DEBUG=0
LCDEFS    = -I. -g 
LDLIBS    = 	-nostdlib -L$(ROOT)/usr/lib32/mips3 \
		-L$(ROOT)/root/usr/lib32                 \
		-L$(ROOT)/usr/lib32/internal

CFILES    = parse.c scan.c main.c 
OFILES    = $(CFILES:.c=.o)
YFILES    = parse.y

SWSUBSYS  = $(SSS_EOE)
SWDEST    = /usr/etc

#
# override standard rule
#
default: $(TARGETS) y.tab.h

install: $(TARGETS) y.tab.h 
	$(INSTALL) -idb $(SWSUBSYS) -F $(SWDEST) $(TARGETS)

include $(COMMONRULES)
LDIRT = *.tab.* *.output $(TARGETS) parse.c

y.tab.h: $(YFILES)
	$(YACC) -v -d parse.y

$(CFILES): y.tab.h

parse.c :        $(YFILES) y.tab.h
	mv -f y.tab.c parse.c

rules_check: $(YFILES) $(OFILES)
	$(CCF) -o $@ $(OFILES) $(LDLIBS)


