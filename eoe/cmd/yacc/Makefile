#!smake
#
# Makefile for yacc(1).
#
#ident "$Revision: 1.13 $"

include	$(ROOT)/usr/include/make/commondefs

CFILES= y1.c y2.c y3.c y4.c
TARGETS= yacc

default: $(TARGETS)

include $(COMMONRULES)

yacc: $(OBJECTS)
	$(CCF) $(OBJECTS) $(LDFLAGS) -o $@

install: default
	$(INSTALL) -F /usr/bin yacc
