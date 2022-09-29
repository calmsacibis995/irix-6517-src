#!smake

include $(ROOT)/usr/include/make/commondefs
include ../../../include/ssscommondefs
include ../../semdefs

TARGETS= dsm_wrap 
DSM_WRAP_OFILES = $(DSM_WRAP_CFILES:.c=.o)
DSM_WRAP_CFILES = dsm_wrap.c 
OFILES = $(DSM_WRAP_OFILES)

OPTIMIZER=-g

SUBDIRS=rules_check
SWDEST=/usr/etc
SWSUBSYS=$(SSS_EOE)
all: default

include $(COMMONRULES)

clean clobber: $(COMMONPREF)$$@
	$(SUBDIRS_MAKERULE)	
install: $(COMMONPREF)$$@ default
	$(INSTALL) -idb $(SWSUBSYS) -F $(SWDEST) $(TARGETS)

$(COMMONPREF)install: $(TARGETS)
default: $(OFILES)
	$(SUBDIRS_MAKERULE)

$(SUBDIRS):$(_FORCE)
	cd $@; $(MAKE)
