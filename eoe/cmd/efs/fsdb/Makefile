#!smake
#ident "$Revision: 1.12 $"
include	../commondefs

CFILES = fsdb.c
TARGETS = fsdb

LCOPTS += -OPT:Olimit=2593
LLDLIBS = -ldisk

include ../commonrules

install: all
	$(INSTALL) -F /usr/bin $(TARGETS)

