#$Modified: Tue Oct  1 12:16:38 1996 by cwilson $
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
#	Builds rtelnet
#
# Revision Control Information:
#
# $Header: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/rtelnetd/RCS/Makefile,v 1.2 1996/10/04 12:15:18 cwilson Exp $
#
# This file created by RCS from:
# $Source: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/rtelnetd/RCS/Makefile,v $
#
# This file is currently under revision by:
# $Locker:  $
#
##############################################################################
#
# DATE:		$Date: 1996/10/04 12:15:18 $
# REVISION:	$Revision: 1.2 $
#

OFILES		= rtelnetd.o rtelnet.o tcp.o
CFILES		= rtelnetd.c rtelnet.c tcp.c

LIBFLAG		= 
CFLAGS	=   -DINETD -DSGI -DSYS_V  -I../inc

all:	rtelnetd

remake:
		$(MAKE) clean
		$(MAKE) all

clean:
		${RM} -f ${OFILES}
		${RM} -f rtelnetd

rtelnetd:	${OFILES} ../netadm/netadm.a ../libannex/libannex.a
		${CC} ${CFLAGS} -g -o rtelnetd ${OFILES} \
			../netadm/netadm.a ../libannex/libannex.a ${LIBFLAG}

../libannex/libannex.a:	../make.config
		cd ../libannex; $(MAKE) -f ../make.config -f Makefile libannex.a

../netadm/netadm.a:	../make.config
		cd ../netadm; $(MAKE) -f ../make.config -f Makefile netadm.a

${OFILES}:	../make.config
		${CC} ${CFLAGS}  -c $*.c

install:	all
		$(INSTALL) $(INSTALL_DIR) rtelnetd

# DO NOT DELETE THIS LINE -- mkdep uses it.
# DO NOT PUT ANYTHING AFTER THIS LINE, IT WILL GO AWAY.

rtelnetd.o:	rtelnetd.c rtelnetd.h ../inc/config.h
rtelnet.o:	rtelnet.c rtelnetd.h ../inc/config.h
tcp.o:		tcp.c rtelnetd.h ../inc/config.h

# IF YOU PUT ANYTHING HERE IT WILL GO AWAY
