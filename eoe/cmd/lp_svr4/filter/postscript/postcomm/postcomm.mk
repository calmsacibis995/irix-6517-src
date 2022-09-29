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
# $Header: /proj/irix6.5.7m/isms/eoe/cmd/lp_svr4/filter/postscript/postcomm/RCS/postcomm.mk,v 1.1 1992/12/14 13:17:17 suresh Exp $
#	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T
#	  All Rights Reserved

#	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
#	The copyright notice above does not evidence any
#	actual or intended publication of such source code.

#ident	"@(#)lp:filter/postscript/postcomm/postcomm.mk	1.2"

#
# makefile for the program that sends files to PostScript printers.
#

MAKEFILE=postcomm.mk
ARGS=all

#
# Common source and header files have been moved to $(COMMONDIR).
#

COMMONDIR=../common

#
# postcomm doesn't use floating point arithmetic, so the -f flag isn't needed.
#

SHELL   =/bin/sh
ETCDIR  =/etc

DESTROOT=

DEBUG	=-O2
MAXOPT	=$(DEBUG)
INCLUDE =-DSYSV -I$(COMMONDIR)
STDC	=
CFLAGS  =-systype svr4 -D_RISCOS $(STDC) $(MAXOPT) $(INCLUDE) $(ECFLAGS)
LFLAGS  =$(ELFLAGS)
YFLAGS  =$(EYFLAGS)
AR	= ar
ARFLAGS =crs

INSTALL =$(ETCDIR)/mipsinstall

#CFILES=postcomm.c ifdef.c
CFILES=postcomm.c

HFILES=postcomm.h

POSTIO=$(CFILES:.c=.o)

ALLFILES=README $(MAKEFILE) $(HFILES) $(CFILES)


all : postcomm

install : postcomm
	@if [ ! -d "$(BINDIR)" ]; then \
	    mkdir $(BINDIR); \
	    $(CH)chmod 775 $(BINDIR); \
	    $(CH)chgrp $(GROUP) $(BINDIR); \
	    $(CH)chown $(OWNER) $(BINDIR); \
	fi
	$(INSTALL) -c -s -m 775 -o $(OWNER) -g $(GROUP) postcomm $(BINDIR)
#	cp postcomm $(BINDIR)
#	chmod 775 $(BINDIR)/postcomm
#	chgrp $(GROUP) $(BINDIR)/postcomm
#	chown $(OWNER) $(BINDIR)/postcomm

postcomm : $(POSTIO)
	if [ -d "$(DKHOSTDIR)" ]; \
	    then $(CC) $(CFLAGS) -o postcomm $(POSTIO) -Wl,-L$(DKHOSTDIR)/lib -ldk; \
	    else $(CC) $(CFLAGS) -o postcomm $(POSTIO); \
	fi

postcomm.o : $(HFILES)

clean :
	rm -f *.o

clobber : clean
	rm -f postcomm

list :
	pr -n $(ALLFILES) | $(LIST)

