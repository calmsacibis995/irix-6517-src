#!smake
include $(ROOT)/usr/include/make/commondefs
include ../../semdefs
TARGETS= $(OFILES)
OFILES = $(CFILES:.c=.o)
CFILES = dsm_events.c dsm_init.c dsm_main.c

OPTIMIZER =-g -D_POSIX_C_SOURCE=199506L -D_BSD_TYPES


all: default

include $(COMMONRULES)

clean clobber: $(COMMONPREF)$$@

install: $(COMMONPREF)$$@


$(COMMONPREF)install: $(TARGETS)
default: $(TARGETS)


