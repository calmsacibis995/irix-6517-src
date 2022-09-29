#!smake -f Makefile.sgi
#	(Above applies to IRIX only - this makefile is for the PC environment)
#
# Makefile for VxWorks kernel code. In general, the modules in this tree
# should change infrequently.
#
# The following environment variables are expected to be defined. See the
# top-level makefile for definitions:
#
#	WIND_HOST_TYPE, WIND_BASE, VX_SRC, VX_LIB, SGI_SRC
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
ifndef LOCALDEFS
	LOCALDEFS	= ..\..\..\localdefs.h
	WARNING		= warning
	WARNVARS	+= LOCALDEFS
endif
ifndef OUTPUT
	OUTPUT		= kernel.o
	WARNING		= warning
	WARNVARS	+= OUTPUT
endif
ifndef RAM_LOW_ADRS
	RAM_LOW_ADRS	= 00108000
	WARNING		= warning
	WARNVARS	+= RAM_LOW_ADRS
endif
ifndef RAM_HIGH_ADRS
	RAM_HIGH_ADRS	= 00008000
	WARNING		= warning
	WARNVARS	+= RAM_HIGH_ADRS
endif
ifndef ROM_BASE_ADRS
	ROM_BASE_ADRS	= fff20000
	WARNING		= warning
	WARNVARS	+= ROM_BASE_ADRS
endif
ifndef ROM_SIZE
	ROM_SIZE	= 0007fe00
	WARNING		= warning
	WARNVARS	+= ROM_SIZE
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

#
# Compute the build target and related parameters based on incoming vars
#
ifdef BOOTROM

	TARGET			= bootrom
	BIDEFS			= -DUNCOMPRESS -DBOOTROM
	SOURCES			= rominit.s bootinit.c bootconfig.c \
				  sysalib.s syslib.c

else	# !BOOTROM

	ifdef STANDALONE

		ifdef NETBOOT
			EXTRA_DEFINE += -DNETBOOT
		endif

		TARGET		= standalone
		SOURCES		= sysalib.s syslib.c usrconfig.c
		UCDEFS		= -DSTANDALONE
		EXTRA_DEFINE	+= -DFFSC_STANDALONE

	else 	# !STANDALONE

		TARGET		= normal
		SOURCES		= sysalib.s syslib.c usrconfig.c

	endif
endif

ifdef PRODUCTION
	LCOPTS = -O2 -DPRODUCTION
else
	ifdef BOOTROM
		LCOPTS = -O2
	else
#		LCOPTS = -g
		LCOPTS = -O2
	endif
endif

CSRCS	= $(filter %.c,$(SOURCES))
OBJ1	= $(patsubst %.c,%.o,$(SOURCES))
OBJECTS = $(patsubst %.s,%.o,$(OBJ1))
PADOBJ  = datasegpad.o
DEPENDS = depend.vxworks

EXTRA_INCLUDE   = -I$(SGI_SRC)\include -I$(SRC_DIR)
ADDED_CFLAGS	= -include $(LOCALDEFS) $(LCOPTS) $(VCOPTS)
ADDED_CASFLAGS  = -include $(LOCALDEFS) $(VASOPTS)

VPATH = $(SRC_DIR)
.PHONY: normal standalone bootrom


#
# Local targets
#

include $(VX_SRC)/target/h/make/rules.basic

default: $(TARGET)


normal standalone: $(WARNING) $(OUTPUT) $(PADOBJ)

bootrom: $(WARNING) $(OUTPUT)


$(OUTPUT): $(OBJECTS)
	$(LD) -o $@ $(LD_PARTIAL_FLAGS) $(OBJECTS)


#
# Unusual objects (mostly files whose source resides elsewhere)
#
bootinit.o: bootinit.c
	- @ $(RM) $@
	$(CC) -c $(CFLAGS) $(BIDEFS) $<

usrconfig.o: usrconfig.c
	- @ $(RM) $@
	$(CC) -c $(CFLAGS) $(UCDEFS) $<


#
# Miscellaneous targets
#
depend:
	$(CC)  -M $(CFLAGS) $(addprefix $(SRC_DIR)\,$(CSRCS)) > $(DEPENDS)
	$(CPP) -M $(CASFLAGS) $(SRC_DIR)\rominit.s >> $(DEPENDS)
	$(CPP) -M $(CASFLAGS) $(SRC_DIR)\sysalib.s >> $(DEPENDS)

clean:
	- $(RM) $(OBJECTS) 
	- $(RM) $(PADOBJ)
	- $(RM) $(OUTPUT)

clobber: clean
	- $(RM) $(DEPENDS)

srcclean:		# Removes cruft from source directories
	- $(RM) $(SRC_DIR)\*~
	- $(RM) $(addprefix $(SRC_DIR)\,$(OBJECTS) $(PADOBJ))
	- $(RM) $(SRC_DIR)\$(DEPENDS)

warning:
	@echo WARNING - default values used for these variables:
	@echo $(WARNVARS)


#
# Include dependencies if available
#
-include $(DEPENDS)
