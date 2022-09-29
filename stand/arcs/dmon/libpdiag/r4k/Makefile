#ident	"$Id: Makefile,v 1.1 1994/07/21 01:21:22 davidl Exp $"
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
include $(DEPTH)/libdefs

# List of 'C' files
#
CFILES	= cache_table.c 

# Assembly files
#
ASFILES	= r4kcache_util.s \
	  d_indxstortag.s \
	  d_indxwbinv.s \
	  i_indxstortag.s \
	  noscache.s \
	  d_cacheinit.s \
	  i_cacheinit.s \
	  sd_cacheinit.s \
	  cache_err.s \
	  dcache_aina.s \
	  dcache_kh.s \
	  dcache_march.s \
	  dcache_mats.s \
	  dkseg01_func.s \
	  dtag_mats.s \
	  dtag_parity.s \
	  icache_mats.s \
	  itag_mats.s \
	  itag_parity.s	\
	  sd_indxstortag.s \
	  scache_loop.s


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
