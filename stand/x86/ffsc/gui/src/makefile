#!smake -f Makefile.sgi
#	(Above applies to IRIX only - this makefile is for the PC environment)
#
# Top-level makefile for GUI-specific FFSC code
#
# The following environment variables are expected to be defined. See the
# top-level makefile for definitions:
#
#	WIND_HOST_TYPE, WIND_BASE, VX_SRC, SGI_SRC, CDI_SRC
#

#
# The following variables are expected to be defined by the caller on the
# command line under normal circumstances (i.e. being invoked by the top
# level makefile). However, set default values if they are not.
#

WARNING  =
WARNVARS =
ifndef CPU
	CPU		= i80486
	WARNING		= warning
	WARNVARS	+= CPU
endif
ifndef OUTPUT
	OUTPUT		= gui.o
	WARNING		= warning
	WARNVARS	+= OUTPUT
endif
ifndef SRC_DIR
	SRC_DIR		= .
	WARNING		= warning
	WARNVARS	+= SRC_DIR
endif
ifndef TOOL
	TOOL		= gnu
	WARNING		= warning
	WARNVARS	+= TOOL
endif


#
# include standard definitions
#
include $(VX_SRC)/target/h/make/defs.bsp
include $(VX_SRC)/target/h/make/make.$(CPU)$(TOOL)
include $(VX_SRC)/target/h/make/defs.$(WIND_HOST_TYPE)


#
# Local definitions
# WARNING: definitions made above this point may be overridden by the
#	   include'd files above.
#

OBJECTS	= \
		guidlg.o	\
		guiinit.o	\
		guimain.o	\
		guimotf.o	\
		guiutil.o	\
		guiview.o	\
		system96.o	\
		ffscgui.o	\
		bargrf.o	\
		dispreq.o	\
		backimg.o	\
		swevt.o

SOURCES = $(addprefix $(SRC_DIR)\,$(subst .o,.c,$(OBJECTS)))
DEPENDS = depend.guisrc

#
# This makefile for some reason does not get the -DPRODUCTION flag passed to 
# it.  Need to ask Rob for Help
#
#ifdef PRODUCTION
#LCOPTS = -O2 -DPRODUCTION
#else
#	LCOPTS = -g
#endif
LCOPTS = -O2 -DPRODUCTION

ADDED_CFLAGS   = $(LCOPTS) $(VCOPTS) -DVXWORKS
EXTRA_INCLUDE += -I$(VX_SRC)\target\config	\
		 -I$(CDI_SRC)			\
		 -I$(SGI_SRC)\include		\
		 -I$(GUI_SRC)\include		\
		 -I$(MW_HDR)
GUILIB = $(MW_LIB)\metawndo.o

VPATH = $(SRC_DIR)
.PHONY: default depend clean clobber



#
# Local targets
#

include $(VX_SRC)/target/h/make/rules.basic


default: $(OUTPUT)


$(OUTPUT): $(WARNING) $(OBJECTS) $(GUILIB)
	$(LD) -o $(OUTPUT) $(LD_PARTIAL_FLAGS) $(OBJECTS) $(GUILIB)


#
# Miscellaneous targets
#
depend:
	$(CC) -M $(CFLAGS) $(SOURCES) > $(DEPENDS)

clean:
	- $(RM) $(OBJECTS)
	- $(RM) $(OUTPUT)

clobber: clean
	- $(RM) $(DEPENDS)

srcclean:
	- $(RM) $(SRC_DIR)\*~
	- $(RM) $(addprefix $(SRC_DIR)\,$(OBJECTS))
	- $(RM) $(SRC_DIR)\$(DEPENDS)

warning:
	@echo WARNING - default values used for these variables:
	@echo $(WARNVARS)


#
# Include dependencies if available
#
-include $(DEPENDS)
