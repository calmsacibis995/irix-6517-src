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
# $Header: /proj/irix6.5.7m/isms/eoe/cmd/lp_svr4/lib/RCS/libcommon.mk,v 1.1 1992/12/14 13:27:33 suresh Exp $
#	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T
#	  All Rights Reserved

#	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
#	The copyright notice above does not evidence any
#	actual or intended publication of such source code.

#ident	"@(#)lp:lib/libcommon.mk	1.2"
#
# Common makefile definitions and targets used by library makefiles
#


#####
#
# Each library makefile must define $(LIBNAME) before including
# this file, so that the object and lint library names can be
# defined. Also, the separate makefiles must define
#
#	SRCS	list of source files
#	OBJS	list of objects created from the source files
#	LINTS	list of other lint libraries needed to lint the library
#
# Also, each library makefile should either include the main common
# definition file (common.mk in the top level directory) or must take
# other steps to define common things like $(CFLAGS), $(LINT), etc.
#####

LIBRARY	=	lib$(LIBNAME).a
LINTSRC	=	llib-l$(LIBNAME).c
LINTLIB	=	llib-l$(LIBNAME).ln


all:		$(LIBRARY)

install:	all

clean::	
	$(RM) $(OBJS)

clobber:	clean
	$(RM) $(LIBRARY) $(LINTLIB)

strip:
	$(STRIP) $(LIBRARY)

library:	$(LIBRARY)

$(LIBRARY):	$(OBJS)
	$(AR) $(ARFLAGS) $(LIBRARY) $?

lint:
	$(LINT) -ux $(CFLAGS) $(SRCS) $(LINTS)

lintsrc:	$(LINTSRC)

$(LINTSRC):	
	if [ -n "$(FUNCDCL)" ]; \
	then \
		$(FUNCDCL) $(SRCS) >$(LINTSRC); \
	fi

lintlib:	$(LINTLIB)

$(LINTLIB):	$(LINTSRC)
	$(LINT) -o $(LIBNAME) -x -v $(CFLAGS) $(LINTFLAGS) $(LINTSRC)
