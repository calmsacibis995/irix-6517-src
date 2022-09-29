#
# Makefile for Moosehead firmware
#
MHDEPTH=.
include $(MHDEPTH)/mhcommondefs

#SUBDIRS=	tools lib sloader post fw images debugcard
SUBDIRS=	images

default $(COMMONTARGS): $(COMMONPREF)$$@
	$(SUBDIRS_MAKERULE)

mhdefault: $(OBJECTDIR)

install: 
	cd images; make install

include $(MHCOMMONRULES)
