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
#* Makefile - lpsched , a lp print scheduler command
#*
#*
#*
#**************************************************************************
DEPTH = ../../..

include $(DEPTH)/lpdefs

CFILES =  \
		dial.c \
		alerts.c \
		cancel.c \
		checkchild.c \
		daisyforms.c \
		disena.c \
		disp1.c \
		disp2.c \
		disp3.c \
		disp4.c \
		disp5.c \
		disptab.c \
		dowait.c \
		exec.c \
		faults.c \
		files.c \
		flt.c \
		fncs.c \
		getkey.c \
		getstatus.c \
		init.c \
		log.c \
		lpfsck.c \
		lpsched.c \
		msgs.c \
		notify.c \
		pickfilter.c \
		ports.c \
		putjob.c \
		rexec.c \
		requeue.c \
		rstatus.c \
		schedule.c \
		status.c \
		terminate.c \
		validate.c

TARGETS = lpsched

IDB_TAG = eoe2.sw.lp 

LCINCS = -I$(LPINC) -I$(INC)
LCDEFS = -DDEBUG -D_RISCOS -g -O0
LLDLIBS = $(LIBMSG) $(LIBFRM) $(LIBREQ) $(LIBPRT) $(LIBCLS) $(LIBACC) \
	$(LIBFLT) $(LIBUSR) $(LIBLP) $(LIBOAM) $(LIBSEC) $(LIBSYS) 

default: $(TARGETS)

include $(COMMONRULES)

$(TARGETS): $(OBJECTS) 
	$(CCF) $(OBJECTS) $(LDFLAGS) -lgen  -lcurses -o $@


install: default
	$(INSTALL) -idb $(IDB_TAG) -u lp -g lp -m 755 \
			-F /usr/lib/lp $(TARGETS)
