#!smake

include $(ROOT)/usr/include/make/commondefs
include ../../semdefs

TARGETS= $(OFILES)
OFILES = $(CFILES:.c=.o)
CFILES = dsm_children.c dsm_config.c dsm_dso.c exec_dso.c

all: default

include $(COMMONRULES)

clean clobber: $(COMMONPREF)$$@

install: $(COMMONPREF)$$@


$(COMMONPREF)install: $(TARGETS)
default: $(TARGETS)


