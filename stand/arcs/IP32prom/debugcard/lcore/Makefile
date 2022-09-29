#
# Makefile for Moosehead debugcard.
DEBUGCARD	=	..
include		$(DEBUGCARD)/mhdbcommondefs

#
# some target macros.
TARGETS= lcore
TARGETDIR=$(DEBUGCARD)/$(PRODUCT).O
OBJECTDIR=$(TARGETDIR)
VPATH=$(OBJECTDIR)
MHLIB=$(MHDEPTH)/$(PRODUCT).O

#
# Assembly sources.
ASFILES=\
	bev_trap_vector.s   \
	cache_init.s		\
    asm_support.s

#
# C sources.
#
CFILES=\
    st16c1451.c         \
    uartio.c            \
    utilities.c         \
	memory.c			\
	flash.c             \
    menu.c


#
# Here is the place we pickup any local flags for CFLAGS and ASFLAGS.
# SIMULATION will get defined by the Makefile in triton.sim directory.
DBLCCFLAGS=$(SIMULATION)
DBLASFLAGS=$(SIMULATION)

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
# Now heres something special just for the debugcard monitor.
lcore:	 $(OBJECTS)
	${CD}	$(OBJECTDIR);\
	${LD}	$(LDRFLAGS) $(OBJECTS) -o $@

#
# Include more common stuff from MH make tree.
include $(DEBUGCARD)/mhdbcommonrules
