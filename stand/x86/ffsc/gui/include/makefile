#!smake -f Makefile.sgi
#	(Above applies to IRIX only - this makefile is for the PC environment)
#
# Makefile for GUI-specific FFSC include files
#
# The following environment variables are expected to be defined. See the
# top-level makefile for definitions:
#
#	WIND_HOST_TYPE, WIND_BASE, VX_SRC
#


.PHONY: default clean clobber


#
# Include standard definitions
#
include $(VX_SRC)/target/h/make/defs.bsp
include $(VX_SRC)/target/h/make/make.$(CPU)$(TOOL)
include $(VX_SRC)/target/h/make/defs.$(WIND_HOST_TYPE)


#
# Local targets
#
default:
	@rem nothing to do

srcclean:
	- $(RM) $(SRC_DIR)\*~

%:
	@rem nothing to do
