#!smake
# Makefile for Moosehead debugcard.
DEBUGCARD	=	..
include		$(DEBUGCARD)/mhdbcommondefs

#
# some target macros.
TARGETS= DPROM.s3 DPROM
TARGETDIR=$(DEBUGCARD)/$(PRODUCT).O
OBJECTDIR=$(TARGETDIR)
VPATH=$(OBJECTDIR)

#
# binaries.
OBJECTS=\
	$(OBJECTDIR)/lcore\
	$(OBJECTDIR)/diags\
	$(OBJECTDIR)/IP32flash.o

#
# Some default setting common for all mh stuff.
default: $(TARGETS)
clean:   mhclean
rmtargets: $(_FORCE)
	rm -rf $(TARGETS)
clobber: clean rmtargets
	rm -rf $(MKDEPFILE)

#
# Now heres something special just for the debugcard monitor.

DPROM.s3:		DPROM
	${RM}		$@
	${CONVERT}	DPROM > $@

DPROM:		$(OBJECTS)
	${LD}	$(LDOBJFLAGS) $(OBJECTS) -o $@

debug:
	echo "The objects = $(OBJECTS)"

#
# Include more common stuff from MH make tree.
include		$(DEBUGCARD)/mhdbcommonrules
