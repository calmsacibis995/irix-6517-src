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
#* Makefile - cmds, all the lp commands
#*
#*
#*
#**************************************************************************
DEPTH = ../..

include $(DEPTH)/lpdefs

TARGETS = accept cancel disable enable lp lpfilter lpforms lpmove  \
		lpshut lpsystem lpusers reject

CFILES = cancel.c comb.c dumpolp.c lp.c lpfilter.c lpforms.c lpmove.c \
	lpshut.c lpsystem.c lpusers.c

IDB_TAG = eoe2.sw.lp 

LCINCS = -I$(LPINC) -I$(INC)
LLDLIBS = $(LIBACC) $(LIBCLS) $(LIBMSG) $(LIBPRT) $(LIBFRM) $(LIBLP) $(LIBOAM) 


default: $(TARGETS)

include $(COMMONRULES)

accept: comb.o
	$(CCF) $(CFLAGS) -o $@ comb.o $(LIBMSG) $(LIBACC) $(LIBOAM) $(LIBLP)

disable: comb.o
	$(CCF) $(CFLAGS) -o $@ comb.o $(LIBMSG) $(LIBACC) $(LIBOAM) $(LIBLP)

enable: comb.o
	$(CCF) $(CFLAGS) -o $@ comb.o $(LIBMSG) $(LIBACC) $(LIBOAM) $(LIBLP)

reject: comb.o
	$(CCF) $(CFLAGS) -o $@ comb.o $(LIBMSG) $(LIBACC) $(LIBOAM) $(LIBLP)

cancel: cancel.o
	$(CCF) $(CFLAGS) -o $@ $@.o $(LIBREQ) $(LIBMSG) $(LIBOAM) $(LIBLP)

lp: lp.o
	$(CCF) $(CFLAGS) -o $@ $@.o $(LIBPRT) $(LIBREQ) $(LIBMSG) $(LIBOAM) $(LIBLP)

lpfilter: lpfilter.o
	$(CCF) $(CFLAGS) -o $@ $@.o $(LIBFLT) $(LIBMSG) $(LIBACC) $(LIBOAM) $(LIBLP) -lgen

lpforms: lpforms.o
	$(CCF) $(CFLAGS) -o $@ $@.o $(LIBFRM) $(LIBMSG) $(LIBREQ) $(LIBACC) $(LIBLP) $(LIBOAM)

lpmove: lpmove.o
	$(CCF) $(CFLAGS) -o $@ $@.o $(LIBMSG) $(LIBACC) $(LIBLP) $(LIBOAM) 
	 
lpshut: lpshut.o
	$(CCF) $(CFLAGS) -o $@ $@.o $(LIBMSG) $(LIBLP) $(LIBOAM) 

lpsystem: lpsystem.o
	$(CCF) $(CFLAGS) -o $@ $@.o $(LIBSYS) $(LIBMSG) $(LIBLP) $(LIBOAM) -lnsl $(LDFLAGS)

lpusers: lpusers.o
	$(CCF) $(CFLAGS) -o $@ $@.o $(LIBMSG) $(LIBACC) $(LIBUSR) $(LIBLP) $(LIBOAM) 

dumpolp: dumpolp.o
	$(CCF) $(CFLAGS) -o $@ $@.o $(LIBLP) 

# Install line needs more tweaking, need to understand where each of
# the command supposed to go.
install: default
	$(INSTALL) -idb $(IDB_TAG) -u lp -g lp -m 755 \
			-F /usr/sbin $(TARGETS)

