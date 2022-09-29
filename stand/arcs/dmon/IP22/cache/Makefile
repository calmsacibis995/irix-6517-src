#ident	"$Id: Makefile,v 1.1 1994/07/20 23:44:34 davidl Exp $"
#
#
# Makefile for cache directory
#

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
CFILES	= cache_table.c \
	sd_aina.c \
	sd_tagkh.c \
	sd_parram.c \
	sd_data.c \
	poncache.c \
	cutil.c 


# Assembly files
#
ASFILES	= ponsubr.s\
	junk.s


# Define the target
#
TARGETS	=libcache.a

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
