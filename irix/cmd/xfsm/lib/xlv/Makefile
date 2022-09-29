#!smake
#ident "$Revision: 1.5 $"

DEPTH=../..
include $(DEPTH)/Makedefs

KERN=../../../../kern
LCINCS=-I$(KERN) -I../../include -I../../../xlv/include

CFILES = xlv_cmd.c xlv_funcs.c xlv_int.c table.c

OBJECTS = $(CFILES:.c=.o)

DEBUG = -g -DDEBUG
REGOPTS = -D_SGI_SOURCE
LCOPTS = $(REGOPTS)

TARGETS = $(OBJECTS)

default: $(TARGETS)

include $(COMMONRULES)

debug:
	exec $(MAKE) 'LCDEFS=$(REGOPTS) $(DEBUG)'

