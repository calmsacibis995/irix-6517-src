#!smake
include $(ROOT)/usr/include/make/commondefs

LCDEFS= -I. -I../..include -I$(ROOT)/usr/include -I$(ROOT)/usr/include/sss 
OPTIMIZER=-g -D_POSIX_C_SOURCE=199506L
COMMONPREF=sss_

TARGETS= $(OFILES)
OFILES = $(CFILES:.c=.o)
CFILES = semapisrv.c
LCDEFS += -D_BSD_TYPES
OPTIMIZER=-g 
COMMONPREF=sss_
exports all: default

include $(COMMONRULES)

clean clobber install: $(COMMONPREF)$$@

$(COMMONPREF)install: $(TARGETS)
default: $(TARGETS)


