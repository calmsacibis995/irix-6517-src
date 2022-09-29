#
#**************************************************************************
#*                                                                        *
#*            Copyright (c) 1991 Silicon Graphics, Inc.                   *
#*                      All Rights Reserved                               *
#*                                                                        *
#*         THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF SGI             *
#*                                                                        *
#* The copyright notice above does not evidence any actual of intended    *
#* publication of such source code, and is an unpublished work by Silicon *
#* Graphics, Inc. This material contains CONFIDENTIAL INFORMATION that is *
#* the property of Silicon Graphics, Inc. Any use, duplication or         *
#* disclosure not specifically authorized by Silicon Graphics is strictly *
#* prohibited.                                                            *
#*                                                                        *
#* RESTRICTED RIGHTS LEGEND:                                              *
#*                                                                        *
#* Use, duplication or disclosure by the Government is subject to         *
#* restrictions as set forth in subdivision (c)(1)(ii) of the Rights in   *
#* Technical Data and Computer Software clause at DFARS 52.227-7013,      *
#* and/or in similar or successor clauses in the FAR, DOD or NASA FAR     *
#* Supplement. Unpublished - rights reserved under the Copyright Laws of  *
#* the United States. Contractor is SILICON GRAPHICS, INC., 2011 N.       *
#* Shoreline Blvd., Mountain View, CA 94039-7311                          *
#**************************************************************************
#*
#* Makefile - lpstat , a lp print status command
#*
#*
#*
#**************************************************************************
DEPTH = ../..

include $(DEPTH)/lpdefs

CFILES =  \
	accept.c \
	add_mounted.c \
	charset.c \
	class.c \
	device.c \
	done.c \
	form.c \
	lpstat.c \
	output.c \
	parse.c \
	printer.c \
	request.c \
	send_message.c


TARGETS = lpstat

IDB_TAG = eoe2.sw.lp 

LCINCS = -I$(LPINC) -I$(INC)
LLDLIBS = $(LIBACC) $(LIBCLS) $(LIBMSG) $(LIBPRT) $(LIBFRM) $(LIBLP) $(LIBOAM) 


default: $(TARGETS)

include $(COMMONRULES)

$(TARGETS): $(OBJECTS) 
	$(CCF) $(OBJECTS) $(LDFLAGS) -lgen  -lcurses -o $@


install: default
	$(INSTALL) -idb $(IDB_TAG) -u lp -g lp -m 755 \
			-F /usr/sbin $(TARGETS)

