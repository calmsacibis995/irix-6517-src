#!smake
#
# Makefile for libdm.
#
#ident  "$Revision: 1.8 $"

DEPTH	= .

DEBUG = -g -DDEBUG
LINTOPTS = $(DEBUG) -wlint -woff 813,852
REGOPTS = $(LIBRARY_COPTS) $(VERSION_COPTS) $(SUBDIR_COPTS)

CFILES = dm_attr.c dm_bulkattr.c dm_config.c dm_dmattr.c dm_event.c \
	 dm_handle.c dm_handle2path.c dm_hole.c dm_mountinfo.c dm_rdwr.c \
	 dm_region.c dm_right.c dm_session.c handle.c

include $(ROOT)/usr/include/make/libleafdefs
include $(LIBLEAFRULES)

debug:
	exec $(MAKE) LCOPTS='$(REGOPTS) $(DEBUG)'

lint:
	exec $(MAKE) LCOPTS='$(REGOPTS) $(LINTOPTS)'
