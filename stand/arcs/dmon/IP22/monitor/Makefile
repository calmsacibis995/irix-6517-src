####
#
#
# Makefile for monitor directory
#
#	"$Revision: 1.1 $"

#
# Bring in the generic definitions used at SGI and some of
# our own.
#
DEPTH= ..
include	$(ROOT)/usr/include/make/commondefs
include $(PRODUCTDEFS)
include ../dmondefs

# List of 'C' files
#
CFILES	= table_cmd.c \
	table_test.c

# Assembly files
#
ASFILES= csu.monitor.s \
	dcommands.s \
	nv_commands.s \
	handler.s \
	monitor.s \
	msgs.s \
	parser.s \
	print_menu.s \
	runtests.s \
	lmem_config.s 


# Define the target
#
TARGETS	=libmonitor.a

# uncomment the following line if there is just a single file being compiled
# LMKDEPFLAGS= $(NULLSUFFIX_MKDEPFLAG)

# Define the default rule.
default:$(TARGETS)

include $(COMMONRULES)

install:default
	echo "installing nothing"

# Only build the library if the source is newer than the objs.
# OBJECTS is defined in the commondefs file= CFILES and ASFILES .o's
# BLDLIB creates the library for this directory and deposit it in the 
# OBJDIR directory.
#
$(TARGETS):	$(OBJECTS)
		$(BLDLIB)
