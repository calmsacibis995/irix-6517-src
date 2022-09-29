/*
 *****************************************************************************
 *
 *        Copyright 1989, Xylogics, Inc.  ALL RIGHTS RESERVED.
 *
 * ALL RIGHTS RESERVED. Licensed Material - Property of Xylogics, Inc.
 * This software is made available solely pursuant to the terms of a
 * software license agreement which governs its use.
 * Unauthorized duplication, distribution or sale are strictly prohibited.
 *
 * Include file description:
 *	Interpretation of server byte 
 *	(condensed from dfe/parmdfe.h)
 *
 * Original Author: Paul Mattes		Created on: 19. January 1988
 *
 * Revision Control Information:
 *
 * $Header: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/inc/na/RCS/server.h,v 1.2 1996/10/04 12:04:56 cwilson Exp $
 *
 * This file created by RCS from:
 * $Source: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/inc/na/RCS/server.h,v $
 *
 * Revision History:
 *
 * $Log: server.h,v $
 * Revision 1.2  1996/10/04 12:04:56  cwilson
 * latest rev
 *
 * Revision 1.5  1992/02/04  14:47:04  wang
 * Changes to support new file parser
 *
 * Revision 1.4  92/01/27  18:58:40  barnes
 * add conditional flag to suppress errors caused by multiple includes
 * of this file
 * 
 * Revision 1.3  89/12/14  15:34:55  russ
 * added server  capability options SERVE_MOTD and SERVE_MACROS.
 * 
 * Revision 1.2  89/04/05  12:42:14  loverso
 * Changed copyright notice
 * 
 * Revision 1.1  88/01/19  09:57:18  mattes
 * Initial revision
 * 
 *
 * This file is currently under revision by:
 *
 * $Locker:  $
 *
 *  DATE:	$Date: 1996/10/04 12:04:56 $
 *  REVISION:	$Revision: 1.2 $
 *
 ****************************************************************************
 */

#ifndef SERVER_H
#define SERVER_H 0

/* Server bit definitions */

#define SERVE_IMAGE	0x01
#define SERVE_CONFIG	0x02
#define SERVE_MOTD	0x08
#define SERVE_ALL	0xff

#endif
