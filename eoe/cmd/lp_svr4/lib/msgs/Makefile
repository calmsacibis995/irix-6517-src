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
#* Makefile - lib/msgs 	The msgs library.
#*
#* $Revision: 1.2 $
#*
#*
#**************************************************************************

DEPTH = ../..


LPDEFS = $(DEPTH)/lpdefs

include $(LPDEFS)

LIBRARY = liblpmsg.a

LCINCS = -I$(LPINC) -I$(INC)
LCOPTS = -fullwarn 

ARFLAGS = crls
LINTFLAGS = -u -z -y 



TARGETS = $(LIBRARY)


CFILES = \
	_getmessage.c \
	_putmessage.c \
	getmessage.c \
	hslconv.c \
	putmessage.c \
	msgfmts.c \
	mclose.c \
	mconnect.c \
	mdisconnect.c \
	mgetputm.c \
	mlisten.c \
	mcreate.c \
	mdestroy.c \
	mneeds.c \
	mopen.c \
	mread.c \
	mrecv.c \
	msend.c \
	mwrite.c \
	streamio.c \
	fifo_buffs.c \
	read_fifo.c \
	write_fifo.c


default all: $(TARGETS)


include $(COMMONRULES)


$(LIBRARY): $(OBJECTS)
	$(AR) $(ARFLAGS) $@ $(OBJECTS)

