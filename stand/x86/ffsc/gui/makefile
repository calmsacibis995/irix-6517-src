#!smake -f Makefile.sgi
#	(Above applies to IRIX only - this makefile is for the PC environment)
#
# Top-level makefile for GUI-specific FFSC code
#
# The real action is in the src subdirectory, so we just transfer control
# there in most cases
#
# The following environment variables are expected to be defined. See the
# top-level makefile for definitions:
#
#	WIND_HOST_TYPE, WIND_BASE, VX_SRC, SGI_SRC
#


#
# include standard definitions
#
include $(VX_SRC)/target/h/make/defs.bsp
include $(VX_SRC)/target/h/make/make.$(CPU)$(TOOL)
include $(VX_SRC)/target/h/make/defs.$(WIND_HOST_TYPE)


#
# Local definitions
#
SRCDIR	= $(SRC_DIR)\src
INCDIR	= $(SRC_DIR)\include

WARNING  =
WARNVARS =
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


#
# Unfortunately, GNUmake does not propagate variables to recursive makes
# under MS-DOS, so we must handle this manually
#
MAKEVARS = \
		CPU=$(CPU)			\
		MAKEFLAGS=$(MAKEFLAGS)		\
		OUTPUT=$(OUTPUT)		\
		TOOL=$(TOOL)

VPATH = $(SRC_DIR)
.PHONY: default clean clobber warning $(OUTPUT)


#
# Local targets
#
default: $(OUTPUT)

clean:
	$(MAKE) -f $(SRCDIR)\makefile $(MAKEVARS) SRC_DIR=$(SRCDIR) clean
	$(MAKE) -f $(INCDIR)\makefile $(MAKEVARS) SRC_DIR=$(INCDIR) clean

clobber: $(WARNING)
	$(MAKE) -f $(SRCDIR)\makefile $(MAKEVARS) SRC_DIR=$(SRCDIR) clobber
	$(MAKE) -f $(INCDIR)\makefile $(MAKEVARS) SRC_DIR=$(INCDIR) clobber

srcclean:
	$(MAKE) -f $(SRCDIR)\makefile $(MAKEVARS) SRC_DIR=$(SRCDIR) srcclean
	$(MAKE) -f $(INCDIR)\makefile $(MAKEVARS) SRC_DIR=$(INCDIR) srcclean

depend:
	$(MAKE) -f $(SRCDIR)\makefile $(MAKEVARS) SRC_DIR=$(SRCDIR) depend

$(OUTPUT): $(WARNING)
	$(MAKE) -f $(SRCDIR)\makefile $(MAKEVARS) SRC_DIR=$(SRCDIR) $(OUTPUT)

warning:
	@echo WARNING - default values used for these variables:
	@echo $(WARNVARS)
