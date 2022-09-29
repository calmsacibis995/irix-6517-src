####
#
#
# Makefile for mem directory
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
CFILES	= mem_table.c

#
# Assembly files
#
ASFILES= simx_test.s \
	badsim.s


# Define the target
#
TARGETS	=libmem.a

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
