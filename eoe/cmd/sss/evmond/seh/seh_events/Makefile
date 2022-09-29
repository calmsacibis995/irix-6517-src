#!smake

include $(ROOT)/usr/include/make/commondefs
include ../../semdefs

TARGETS= $(OFILES)
OFILES = $(CFILES:.c=.o)
CFILES = seh_init.c seh_main.c seh_events.c seh_archive.c

all: default

include $(COMMONRULES)

clean clobber: $(COMMONPREF)$$@

install: $(COMMONPREF)$$@

$(COMMONPREF)install: $(TARGETS)
default: $(TARGETS)


