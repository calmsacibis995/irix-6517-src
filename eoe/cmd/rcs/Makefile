#!smake
#ident "$Revision: 1.30 $"
# $Id: Makefile,v 1.30 1997/03/31 21:26:28 msf Exp $

OBJECT_STYLE=N32
include	$(ROOT)/usr/include/make/commondefs

# Copyright 1982, 1988, 1989 Walter Tichy
# Copyright 1990, 1991, 1992, 1993, 1994, 1995 Paul Eggert
# Copyright 1997 Silicon Graphics, Inc.
#   Distributed under license by the Free Software Foundation, Inc.
#
# This file is part of a port or RCS version 5.7 to Silicon Graphics systems.
#
# RCS is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.
#
# RCS is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with RCS; see the file COPYING.
# If not, write to the Free Software Foundation,
# 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
#
# This Makefile has been radically altered by SGI from the one
# in the original 5.7 RCS release distributed by the Free
# Software Foundation, to better fit the Makefile tools
# supplied by SGI.  Those porting these RCS commands to
# non-SGI systems should examine the original RCS Makefile
# from FSF, as it contains instructions and options designed
# to facilitate ports to various systems.
#
# Those modifying and porting RCS for use on SGI systems
# should find this Makefile well suited for their needs.

# If the diff command supports the -a option for use
# with binary files (as does the GNU based diff that
# is included in this same IRIX release) then the
# rcs commands will automatically detect and use that,
# at runtime.
DIFF=/bin/diff
DIFF3=$(DIFF)3

TARGETDIR=/usr/sbin
SOURCEDIR=/usr/share/src/gnutools/rcs

#executable programs (commands)
RCSDEVCMDS=ci co merge rcs rcsclean rcsfreeze rcsdiff rcsmerge rlog
RCSEOECMDS=ident
RCSCOMMANDS=$(RCSDEVCMDS) $(RCSEOECMDS)

LIBOBJS = 

CIFILES = ci.o rcslex.o rcssyn.o rcsgen.o rcsedit.o rcskeys.o rcsmap.o \
	rcsrev.o rcsutil.o rcsfnms.o maketime.o partime.o rcstime.o rcskeep.o \
	rcsfcmp.o version.o $(LBOBJS)

COFILES = co.o rcslex.o rcssyn.o rcsgen.o rcsedit.o rcskeys.o rcsmap.o \
	rcsrev.o rcsutil.o rcsfnms.o maketime.o partime.o rcstime.o rcskeep.o \
	version.o $(LBOBJS)

IDENT = ident.o rcsmap.o version.o $(LIBOBJS)

MERGE = merge.o merger.o rcsedit.o rcsfnms.o rcsgen.o rcslex.o \
	rcsmap.o rcsrev.o rcssyn.o rcsutil.o \
	rcskeep.o rcskeys.o maketime.o partime.o rcstime.o version.o \
	$(LIBOBJS)

RLOG = rlog.o rcsedit.o rcsgen.o rcslex.o rcsmap.o rcssyn.o rcsrev.o rcsutil.o \
	maketime.o partime.o rcstime.o rcsfnms.o rcskeep.o rcskeys.o \
	version.o $(LIBOBJS)

RCS = rcs.o rcslex.o rcssyn.o rcsrev.o rcsutil.o rcsgen.o \
	rcsedit.o rcskeys.o rcsmap.o rcsfnms.o rcskeep.o \
	maketime.o partime.o rcstime.o version.o $(LIBOBJS)

RCSCLEAN = rcsclean.o rcsedit.o rcsfcmp.o rcsfnms.o rcsgen.o rcskeys.o \
	rcslex.o rcsmap.o rcsrev.o rcssyn.o rcsutil.o rcskeep.o \
	maketime.o partime.o rcstime.o version.o $(LIBOBJS)

RCSDIFF = rcsdiff.o rcsutil.o rcsedit.o rcsfnms.o \
	rcsgen.o rcsmap.o rcsrev.o rcssyn.o  \
	rcslex.o maketime.o partime.o rcstime.o rcskeep.o rcskeys.o \
	version.o $(LIBOBJS)

RCSMERGE = rcsmerge.o merger.o rcsutil.o rcsedit.o rcsfnms.o \
	rcsgen.o rcsmap.o rcsrev.o \
	rcssyn.o rcslex.o rcskeep.o rcskeys.o \
	maketime.o partime.o rcstime.o version.o $(LIBOBJS)



CFILES=	ci.c co.c ident.c maketime.c merge.c merger.c partime.c rcs.c \
	rcsclean.c rcsdiff.c \
	rcsedit.c rcsfcmp.c rcsfnms.c rcsgen.c \
	rcskeep.c rcskeys.c rcslex.c rcsmap.c rcsmerge.c rcsrev.c \
	rcssyn.c rcstime.c rcsutil.c \
	rlog.c version.c 

RCSSOURCE = COPYING Makefile Makefile.in README rcstest $(CFILES) \
	    conf.sh conf.h maketime.h partime.h rcsbase.h \
	    rcsfreeze.sh
		
default:	$(RCSCOMMANDS)

TARGETS = $(RCSCOMMANDS)

include $(COMMONRULES)

install:	default $(RCSSOURCE)
		$(INSTALL) -F $(TARGETDIR) "$(RCSDEVCMDS)"
		$(INSTALL) -F $(TARGETDIR) "$(RCSEOECMDS)"
		$(INSTALL) -m 444 -F $(SOURCEDIR) "$(RCSSOURCE)"

lint:
	${LINT} ${CDEFS} -DRCS_lint=1 ${CIFILES:.o=.c} ${LDLIBS}
	${LINT} ${CDEFS} -DRCS_lint=1 ${COFILES:.o=.c} ${LDLIBS}
	${LINT} ${CDEFS} -DRCS_lint=1 ${RLOG:.o=.c} ${LDLIBS}
	${LINT} ${CDEFS} -DRCS_lint=1 ${RCS:.o=.c} ${LDLIBS}
	${LINT} ${CDEFS} -DRCS_lint=1 ${RCSDIFF:.o=.c} ${LDLIBS}
	${LINT} ${CDEFS} -DRCS_lint=1 ${RCSMERGE:.o=.c} ${LDLIBS}
	${LINT} ${CDEFS} -DRCS_lint=1 ${RCSCLEAN:.o=.c} ${LDLIBS}
	${LINT} ${CDEFS} -DRCS_lint=1 ${IDENT:.o=.c} ${LDLIBS}

ci:		$(CIFILES) 
		$(CCF) -o $@ $(CIFILES) $(LDFLAGS)

co:		$(COFILES)
		$(CCF) -o $@ $(COFILES) $(LDFLAGS)

rlog:		$(RLOG)
		$(CCF) -o $@ $(RLOG) $(LDFLAGS)

rcs:		$(RCS)
		$(CCF) -o $@ $(RCS) $(LDFLAGS)

rcsdiff:	$(RCSDIFF) 
		$(CCF) -o $@ $(RCSDIFF) $(LDFLAGS)

rcsmerge:	$(RCSMERGE) 
		$(CCF) -o $@ $(RCSMERGE) $(LDFLAGS)

merge:		$(MERGE) 
		$(CCF) -o $@ $(MERGE) $(LDFLAGS)

rcsclean:	$(RCSCLEAN) 
		$(CCF) -o $@ $(RCSCLEAN) $(LDFLAGS)

ident:		$(IDENT) 
		$(CCF) -o $@ $(IDENT) $(LDFLAGS)

		

# The following target "make runtest" uses a script called "rcstest"
# that is included in version 5.7 of RCS.  This scipt executes
# the major options of all the commands, and is useful as sanity
# check after changes or rebuilds.  If successful, this target
# creates a temporary subdirectory RCSTEST.tmp.junk, changes to that directory,
# runs the rcstest script, then deletes the RCSTEST.tmp.junk directory.
#
# The script creates and manipulates a number of files all
# with names of the pattern "a.*".  If successful, the script
# outputs nothing and exits with zero for success.
#
# If any problem is detected by the script, it leaves the "a.*"
# files in the RCSTEST.tmp.junk subdirectory, and exits with failure.
# This causes the make to also exit with immediate failure.
#
# Paul Jackson, Silicon Graphics, 11 Mar 92.

runtest:		$(RCSCOMMANDS)
		rm -fr RCSTEST.tmp.junk
		mkdir RCSTEST.tmp.junk
		mkdir RCSTEST.tmp.junk/RCS
		cd RCSTEST.tmp.junk; PATH=..:${PATH} ../rcstest -v
		rm -fr RCSTEST.tmp.junk
