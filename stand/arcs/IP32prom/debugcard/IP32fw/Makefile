#
# Makefile for Moosehead debugcard.
DEBUGCARD=	..
include		$(DEBUGCARD)/mhdbcommondefs

#
# Some target macros.
TARGETS= IP32flash
TARGETDIR=$(DEBUGCARD)/$(PRODUCT).O
OBJECTDIR=$(TARGETDIR)
VPATH=$(OBJECTDIR)

#
# Sources and Objects.
IMAGES=		$(IP32FW)/flash.image
ASFILES=	IP32flash.s
CFILES=

#
# Some default setting common for all mh stuff.
default: $(TARGETS)
clean:   mhclean
	cd $(OBJECTDIR) ;\
	rm -f $(OBJECTS)
clobber: clean rmtargets
	rm -rf $(MKDEPFILE)
rmtargets: $(_FORCE)
	rm -rf $(TARGETS)

#
# Some local make targets.

$(TARGETS):	$(OBJECTS)
	${TOUCH}	$@

$(ASFILES):	$(IMAGES)
	${IMG2ASM}	$(IMAGES) > $@

$(IMAGES):
	${CD}	$(IP32FW) ;\
	${MAKE}	flash.hex

#
# Include more common stuff from MH make tree.
include		$(DEBUGCARD)/mhdbcommonrules
