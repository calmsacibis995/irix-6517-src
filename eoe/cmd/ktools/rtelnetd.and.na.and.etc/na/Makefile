#
##############################################################################
#
#        Copyright 1989, Xylogics, Inc.  ALL RIGHTS RESERVED.
#
# ALL RIGHTS RESERVED. Licensed Material - Property of Xylogics, Inc.
# This software is made available solely pursuant to the terms of a
# software license agreement which governs its use.
# Unauthorized duplication, distribution or sale are strictly prohibited.
#
# Makefile description:
#
#	Builds network administration utilities - high level
#
# Original Author:  Jonathan Taylor		Created on: 85/01/01
# Revised 86/06/12: Dave Harris			Origin:	Makefile.vax 1.3.1.4
#
# Revision Control Information:
#
# $Header: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/na/RCS/Makefile,v 1.3 1996/10/04 12:09:49 cwilson Exp $
#
# This file created by RCS from:
# $Source: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/na/RCS/Makefile,v $
#
# This file is currently under revision by:
# $Locker:  $
#
##############################################################################
#
# DATE:		$Date: 1996/10/04 12:09:49 $
# REVISION:	$Revision: 1.3 $
#

# Use this define to force "na" to use a subdirectory as its home.  This
# is done to prevent system damage from the use of the "write" command
# by naive users.
# CHOME	= -DCHANGE_DIR=\"/bfs/nahome\"

# Comment the next line out to display one parameter per line with the
# show command.  Otherwise, two are shown if there's room.
SPLIT	= -DSPLIT_LINES

# Comment the next line out to avoid use of a pager process.
# (Adding -DHAVE_POPEN forces the use of popen(3) and pclose(3), which
#  are, sadly, not as nicely integrated as the default version.)
UPAGER	= -DUSE_PAGER

OPTS	= ${SPLIT} ${CHOME} ${UPAGER}
NACFLAGS = ${CFLAGS} -I../inc/na -DNA ${OPTS}

OFILES	= main.o cmd.o parse.o do.o sub.o conv.o conv2.o vers.o pager.o
CFILES	= main.c cmd.c parse.c do.c sub.c conv.c conv2.c vers.c pager.c
AFILES	= ../netadm/netadm.a ../libannex/libannex.a
EFILES	= na

all:
	make -f ../make.config -f Makefile na

clean:
	make -f ../make.config -f Makefile cleanup

remake:		
	make clean
	make all

.c.o:
	${CC} ${NACFLAGS} -c $<

${OFILES}:	../make.config Makefile

cleanup:
	${RM} -f ${OFILES} ${EFILES}

na:		${OFILES} ${AFILES}
	${CC} ${NACFLAGS} -o na ${OFILES} ${AFILES} ${LIBFLAG}

../libannex/libannex.a:
	cd ../libannex; make

../netadm/netadm.a:
	cd ../netadm; make

install:	na
	$(INSTALL) $(INSTALL_DIR) na

# DO NOT DELETE THIS LINE -- mkdep uses it.
# DO NOT PUT ANYTHING AFTER THIS LINE, IT WILL GO AWAY.

cmd.o: ../inc/config.h ../inc/erpc/netadmp.h ../inc/na/cmd.h ../inc/na/help.h \
  ../inc/na/na.h ../inc/na/names.h cmd.c 
conv.o: ../inc/config.h ../inc/erpc/netadmp.h ../inc/na/iftype.h \
  ../inc/na/na.h ../inc/na/names.h ../inc/na/server.h ../inc/syslog.h \
  ../libannex/api_if.h conv.c ../inc/port/port.h
conv2.o: ../inc/config.h ../inc/erpc/netadmp.h ../inc/na/iftype.h \
  ../inc/na/na.h ../inc/na/names.h ../inc/na/server.h ../inc/syslog.h \
  ../libannex/api_if.h conv2.c ../inc/port/port.h
do.o: ../inc/config.h ../inc/erpc/netadmp.h ../inc/na/na.h ../inc/na/names.h \
  ../netadm/netadm_err.h do.c ../inc/port/port.h
main.o: ../inc/config.h ../inc/na/na.h ../inc/na/names.h main.c
parse.o: ../inc/config.h ../inc/erpc/netadmp.h ../inc/na/na.h \
	../inc/na/displaytext.h ../inc/na/names.h ../libannex/api_if.h \
	../netadm/netadm_err.h parse.c ../inc/port/port.h
sub.o: ../inc/config.h ../inc/na/na.h ../inc/na/names.h sub.c
vers.o: vers.c
pager.o: ../inc/config.h ../inc/na/na.h pager.c

# IF YOU PUT ANYTHING HERE IT WILL GO AWAY
