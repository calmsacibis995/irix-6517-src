#!smake
include $(ROOT)/usr/include/make/commondefs
include ../../semdefs

TARGETS= $(OFILES)
OFILES = $(CFILES:.c=.o)
CFILES = dsm_parse.c dsm_execute.c dsm_rules.c

all: default

include $(COMMONRULES)

clean clobber: $(COMMONPREF)$$@

install: $(COMMONPREF)$$@


$(COMMONPREF)install: $(TARGETS)
default: $(TARGETS)


