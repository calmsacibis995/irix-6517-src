#!smake
#ident "$Revision: 1.2 $"

DEPTH=../..
include $(DEPTH)/Makedefs

LLDLIBS=-ldisk 
KERN=../../../../kern
LCINCS=-I$(KERN) -I../../include

CFILES = dvhpart.c 

DEBUG = -g -DDEBUG
REGOPTS = -fullwarn -D_SGI_SOURCE 
LCOPTS = $(REGOPTS)

TARGETS = $(OBJECTS)

default: $(TARGETS)

include $(COMMONRULES)

install: default
	${INSTALL} -F /sbin $(TARGETS)
	${INSTALL} -F /etc -lns ../sbin/$(TARGETS) $(TARGETS)

debug:
	exec $(MAKE) 'LCDEFS=$(REGOPTS) $(DEBUG)'
