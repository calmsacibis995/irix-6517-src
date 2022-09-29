#!smake

include $(ROOT)/usr/include/make/commondefs
include ../../semdefs

TARGETS= $(OFILES)
OFILES = $(CFILES:.c=.o)
CFILES = seh_dsm.c seh_ssdb.c seh_sysid.c seh_license.c
all: default

include $(COMMONRULES)

clean clobber: $(COMMONPREF)$$@

install: $(COMMONPREF)$$@


$(COMMONPREF)install: $(TARGETS)
default: $(TARGETS)


