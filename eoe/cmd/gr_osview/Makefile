#!smake
#
# 		 Copyright (C) 1989, Silicon Graphics, Inc.
#
#  These coded instructions, statements, and computer programs  contain 
#  unpublished  proprietary  information of Silicon Graphics, Inc., and
#  are protected by Federal copyright law.  They  may  not be disclosed
#  to  third  parties  or copied or duplicated in any form, in whole or
#  in part, without the prior written consent of Silicon Graphics, Inc.
#

#
#	Makefile for system workload performance display
#

#
#	Define FIXGFX for old style kernels in which the graphics
#	wait time is counted incorrectly.  This causes gr_osview to twist
#	itself around and displays the information appropriately.
#
#	Define _KMEMUSER to get at some kernel type definitions.
#

include	$(ROOT)/usr/include/make/commondefs

LCOPTS	= -fullwarn
LWOFF   = ,1164,1506
LCDEFS	= -DFIXGFX -D_KMEMUSER
CFILES	= main.c setup.c initbar.c drawbar.c input.c getinfo.c rtbar.c \
	  remote.c readstruct.c
LLDLIBS	= -lgl -lfm -lm -lelf
IDB_TAG	= -idb "std.sw.unix"
GROSVIEW = gr_osview
TARGETS = $(GROSVIEW)

default:	$(TARGETS)

include	$(COMMONRULES)

$(GROSVIEW):	$(OBJECTS)
	$(CCF) -o $(GROSVIEW) $(OBJECTS) $(LDFLAGS)

install:	default
	$(INSTALL) $(IDB_TAG) -F /usr/sbin -u root -g sys \
		-m 4755 $(GROSVIEW)
