#!smake -f Makefile.sgi
#	(Above applies to IRIX only - this makefile is for the PC environment)
#
# Top-level makefile for SGI-specific FFSC code
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
	OUTPUT		= sgi.o
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

#
# We dare not let OBJECTS get too big or else "make depend" will die
# (presumably a bug in GCC?)
#
OBJECTS1 = \
		addrd.o		\
		buf.o		\
		cmd.o		\
		cmdadmin.o	\
		cmdcfg.o	\
		cmdctrl.o	\
		cmdlog.o	\
		cmdmode.o	\
		cmdsrvc.o

OBJECTS2 = \
		consenter.o	\
		consleave.o	\
		consmisc.o	\
		console.o	\
		consroute.o	\
		conssys.o	\
		consuser.o	\
		debug.o		\
		dest.o

OBJECTS3 = \
		elsc.o		\
		getflash.o	\
		identity.o	\
		init.o		\
		log.o		\
		misc.o		\
		msg.o		\
		nvram.o		\
		oobmsg.o

OBJECTS4 = \
		remote.o	\
		route.o		\
		switches.o	\
		timeout.o	\
		tty.o		\
		user.o		\
		sgiversion.o    \
                partition.o     

OBJECTS = $(OBJECTS1) $(OBJECTS2) $(OBJECTS3) $(OBJECTS4)

SOURCES1 = $(addprefix $(SRC_DIR)\,$(subst .o,.c,$(OBJECTS1)))
SOURCES2 = $(addprefix $(SRC_DIR)\,$(subst .o,.c,$(OBJECTS2)))
SOURCES3 = $(addprefix $(SRC_DIR)\,$(subst .o,.c,$(OBJECTS3)))
SOURCES4 = $(addprefix $(SRC_DIR)\,$(subst .o,.c,$(OBJECTS4)))

DEPENDS = depend.sgisrc

ifdef PRODUCTION
	LCOPTS = -O2 -DPRODUCTION
else
	LCOPTS = -g
endif

ADDED_CFLAGS   = $(LCOPTS) $(VCOPTS)
EXTRA_INCLUDE += -I$(VX_SRC)\target\config	\
		 -I$(SGI_SRC)\include		\
		 -I$(CDI_SRC)

VPATH = $(SRC_DIR)
.PHONY: default depend clean clobber


#
# Local targets
#

include $(VX_SRC)/target/h/make/rules.basic


default: $(OUTPUT)


$(OUTPUT): $(WARNING) $(OBJECTS)
	$(LD) -o $(OUTPUT) $(LD_PARTIAL_FLAGS) $(OBJECTS)


#
# Miscellaneous targets
#
depend:
	$(CC) -M $(CFLAGS) $(SOURCES1) > $(DEPENDS)
	$(CC) -M $(CFLAGS) $(SOURCES2) >> $(DEPENDS)
	$(CC) -M $(CFLAGS) $(SOURCES3) >> $(DEPENDS)
	$(CC) -M $(CFLAGS) $(SOURCES4) >> $(DEPENDS)

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
