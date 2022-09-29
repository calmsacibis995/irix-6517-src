#
#
#**************************************************************************
#*
#* 		  Copyright (c) 1992 Silicon Graphics, Inc.
#*			All Rights Reserved
#*
#*	   THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF SGI
#*
#* The copyright notice above does not evidence any actual of intended
#* publication of such source code, and is an unpublished work by Silicon
#* Graphics, Inc. This material contains CONFIDENTIAL INFORMATION that is
#* the property of Silicon Graphics, Inc. Any use, duplication or
#* disclosure not specifically authorized by Silicon Graphics is strictly
#* prohibited.
#*
#* RESTRICTED RIGHTS LEGEND:
#*
#* Use, duplication or disclosure by the Government is subject to
#* restrictions as set forth in subdivision (c)(1)(ii) of the Rights in
#* Technical Data and Computer Software clause at DFARS 52.227-7013,
#* and/or in similar or successor clauses in the FAR, DOD or NASA FAR
#* Supplement. Unpublished - rights reserved under the Copyright Laws of
#* the United States. Contractor is SILICON GRAPHICS, INC., 2011 N.
#* Shoreline Blvd., Mountain View, CA 94039-7311
#**************************************************************************
#*
#* Makefile - lpNet parent library
#*
#* $Revision: 1.6 $
#*
#*
#**************************************************************************

DEPTH = ../../../..


LPDEFS = $(DEPTH)/lpdefs

include $(LPDEFS)

LIBRARY =  parent.a
LPSYSTEMS = Systems

LCINCS = -I../include -I$(LPINC) -I$(INC)
LCOPTS = -DDEBUG

IDB_TAG = eoe2.sw.lp

ARFLAGS = crls
LINTFLAGS = -u -z -y 
LPNETLIBS = parent.a \
	    ../svChild/svChild.a \
	    ../bsdChild/bsdChild.a 
SYSLIBS = -lgen -lnsl -lnls -lcurses 

LIBS = $(LPNETLIBS) \
       $(LIBNET) \
       $(LIBBSD) \
       $(LIBREQ) \
       $(LIBMSG) \
       $(LIBSEC) \
       $(LIBPRT) \
       $(LIBSYS) \
       $(LIBLP) \
       $(LIBOAM) 

LPNET = lpNet

TARGETS = $(LIBRARY) $(LPNET)

CFILES = \
	main.c       \
	init.c       \
	parent.c     


default all: $(TARGETS)

include $(COMMONRULES)


$(LPNET): $(LIBRARY)
	$(CC) $(CFLAGS) $(LIBS) $(SYSLIBS) -o lpNet

$(LIBRARY): $(OBJECTS)
	$(AR) $(ARFLAGS) $@ $(OBJECTS)

install: default
	$(INSTALL) -idb $(IDB_TAG) -u lp -g lp -m 755 -F /usr/lib/lp $(LPNET)
	$(INSTALL) -idb $(IDB_TAG) -u lp -g lp -m 644 -F /etc/lp $(LPSYSTEMS)
