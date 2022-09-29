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
include $(DEPTH)/libdefs

# List of 'C' files
#
CFILES	= fillmemw.c filltagw.c

#
# Assembly files
#
ASFILES=  ainatest.s \
	kh_test.s \
	march_test.s \
	mats_test.s \
	mem_aina.s \
	mem_err.s \
	mem_kh.s \
	mem_march.s \
	mem_mats.s \
	mem_util.s


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
