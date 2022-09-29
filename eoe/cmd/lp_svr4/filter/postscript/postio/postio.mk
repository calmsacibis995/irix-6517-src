#
# |-----------------------------------------------------------|
# | Copyright (c) 1991, 1990 MIPS Computer Systems, Inc.      |
# | All Rights Reserved                                       |
# |-----------------------------------------------------------|
# |          Restricted Rights Legend                         |
# | Use, duplication, or disclosure by the Government is      |
# | subject to restrictions as set forth in                   |
# | subparagraph (c)(1)(ii) of the Rights in Technical        |
# | Data and Computer Software Clause of DFARS 252.227-7013.  |
# |         MIPS Computer Systems, Inc.                       |
# |         950 DeGuigne Avenue                               |
# |         Sunnyvale, California 94088-3650, USA             |
# |-----------------------------------------------------------|
#
# $Header: /proj/irix6.5.7m/isms/eoe/cmd/lp_svr4/filter/postscript/postio/RCS/postio.mk,v 1.1 1992/12/14 13:18:29 suresh Exp $
#	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T
#	  All Rights Reserved

#	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
#	The copyright notice above does not evidence any
#	actual or intended publication of such source code.

#ident	"@(#)lp:filter/postscript/postio/postio.mk	1.6"
#
# makefile for the RS-232 serial interface program for PostScript printers.
#
include $(ROOT)/usr/include/make/commondefs

MAKEFILE=postio.mk
ARGS=all

#
# Common source and header files have been moved to $(COMMONDIR).
#

COMMONDIR=../common

#
# postio doesn't use floating point arithmetic, so the -f flag isn't needed.
#

SYSTEM=SYSV
SHELL   =/bin/sh
ETCDIR  =/etc

DESTROOT=

DEBUG	=-O2
MAXOPT	=$(DEBUG)
#INCLUDE =-DSYSV -I$(COMMONDIR)
INCLUDE =-DSYSV -I$(COMMONDIR) -I$(LPINC) -I$(INC)
STDC	=
#CFLAGS  =-systype svr4 -D_RISCOS $(STDC) $(MAXOPT) $(INCLUDE) $(ECFLAGS)
LCFLAGS  =-D_RISCOS $(STDC) $(MAXOPT) $(INCLUDE) $(ECFLAGS)
LFLAGS  =$(ELFLAGS)
YFLAGS  =$(EYFLAGS)
AR	= ar
ARFLAGS =crs

INSTALL =$(ETCDIR)/mipsinstall

CFILES=postio.c ifdef.c slowsend.c

HFILES=postio.h\
       ifdef.h\
       $(COMMONDIR)/gen.h

POSTIO=postio.o ifdef.o slowsend.o

ALLFILES=README $(MAKEFILE) $(HFILES) $(CFILES)


all : postio

install : postio
	@if [ ! -d "$(BINDIR)" ]; then \
	    mkdir $(BINDIR); \
	    $(CH)chmod 775 $(BINDIR); \
	    $(CH)chgrp $(GROUP) $(BINDIR); \
	    $(CH)chown $(OWNER) $(BINDIR); \
	fi
	$(INSTALL) -c -s -m 775 -o $(OWNER) -g $(GROUP) postio $(BINDIR) 
#	cp postio $(BINDIR)
#	$(CH)chmod 775 $(BINDIR)/postio
#	$(CH)chgrp $(GROUP) $(BINDIR)/postio
#	$(CH)chown $(OWNER) $(BINDIR)/postio

postio : $(POSTIO)
	@if [ "$(SYSTEM)" = "SYSV" -a -d "$(DKHOSTDIR)" ]; then \
	    EXTRA="-Wl,-L$(DKHOSTDIR)/lib -ldk"; \
	fi; \
	if [ "$(SYSTEM)" = "V9" ]; then \
	    EXTRA="-lipc"; \
	fi; \
	echo "	$(CC) $(CFLAGS) -o postio $(POSTIO) $$EXTRA"; \
	$(CC) $(CFLAGS) -o postio $(POSTIO) $$EXTRA

postio.o : $(HFILES)
slowsend.o : postio.h $(COMMONDIR)/gen.h
ifdef.o : ifdef.h $(COMMONDIR)/gen.h

clean :
	rm -f $(POSTIO)

clobber : clean
	rm -f postio

list :
	pr -n $(ALLFILES) | $(LIST)

