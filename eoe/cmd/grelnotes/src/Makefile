#! smake
#**************************************************************************
#*									  *
#* 		 Copyright (C) 1991, Silicon Graphics, Inc.		  *
#* 			All Rights Reserved.				  *
#*									  *
#* This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.; *
#* the contents of this file may not be disclosed to third parties,	  *
#* copied or duplicated in any form, in whole or in part, without the	  *
#* prior written permission of Silicon Graphics, Inc.			  *
#*									  *
#* RESTRICTED RIGHTS LEGEND:						  *
#*	Use, duplication or disclosure by the Government is subject to    *
#*	restrictions as set forth in subdivision (c)(1)(ii) of the Rights *
#*	in Technical Data and Computer Software clause at DFARS 	  *
#*	252.227-7013, and/or in similar or successor clauses in the FAR,  *
#*	DOD or NASA FAR Supplement. Unpublished - rights reserved under   *
#*	the Copyright Laws of the United States.			  *
#**************************************************************************
#*
#* Makefile - grelnotes, graphical release notes browser
#*
#* $Revision: 1.3 $
#*
#*
#**************************************************************************/

DEPTH = ..
include $(DEPTH)/viewerdefs

IDB_TAG = "eoe1.sw.unix"

TARGETS = grelnotes
CFILES = main.c \
	 menus.c \
	 place.c \
	 buttons.c \
	 finder.c \
	 errors.c \
	 print.c \
	 search.c \
	 help.c

LIBRARIES = $(VIEWERLIB_DIR)/libvwr.a

LCINCS = -I$(VIEWERINC_DIR)
# Uncomment this to have product movement buttons
#LCDEFS = -DPRODBUTTONS
LCDEFS += -DFUNCPROTO -DNARROWPROTO -DNeedFunctionPrototypes -DFAST_QUARKS
LCOPTS = -fullwarn
LLDOPTS = -L$(VIEWERLIB_DIR)
LLDLIBS = -lvwr -lXm -lXt -lX11 -lXmu -lgen -lmalloc

LINTFLAGS = -u -z -y -woff 91

CVERSION = -xansi
OPTIMIZER = -O2

default all: $(TARGETS)

include $(COMMONRULES)

$(TARGETS): $(OBJECTS) $(LIBRARIES)
	$(CCF) $(OBJECTS) $(LDFLAGS) -o $@

install: all
	$(INSTALL) -m 755 -idb $(IDB_TAG) -F /usr/sbin $(TARGETS)
	$(INSTALL) -m 644 -idb $(IDB_TAG) \
					-F /usr/lib/X11/app-defaults Grelnotes

