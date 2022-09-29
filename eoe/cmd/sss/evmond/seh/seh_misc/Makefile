#!smake

include $(ROOT)/usr/include/make/commondefs
include ../../semdefs

CFILES = seh_config.c seh_errors.c
TARGETS= $(OFILES)
OFILES = $(CFILES:.c=.o)
all: default

include $(COMMONRULES)

clean clobber:	$(COMMONPREF)$$@

install: $(COMMONPREF)$$@

$(COMMONPREF)install: $(TARGETS)
default: $(TARGETS)


