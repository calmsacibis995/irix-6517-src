/*****************************************************************************
 *
 *        Copyright 1989, Xylogics, Inc.  ALL RIGHTS RESERVED.
 *
 * ALL RIGHTS RESERVED. Licensed Material - Property of Xylogics, Inc.
 * This software is made available solely pursuant to the terms of a
 * software license agreement which governs its use.
 * Unauthorized duplication, distribution or sale are strictly prohibited.
 *
 * Module Function:
 *
 *	Where things reside in the Xenix UDP SL/IP world
 *
 * Original Author:  Paul Mattes		Created on: 01/04/88
 *
 * Revision Control Information:
 *
 * $Header: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/inc/slip/RCS/slip_user.h,v 1.2 1996/10/04 12:06:58 cwilson Exp $
 *
 * This file created by RCS from:
 * $Source: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/inc/slip/RCS/slip_user.h,v $
 *
 * Revision History:
 *
 * $Log: slip_user.h,v $
 * Revision 1.2  1996/10/04 12:06:58  cwilson
 * latest rev
 *
 * Revision 1.3  89/04/05  14:48:20  root
 * Changed copyright notice
 * 
 * Revision 1.2  88/05/31  17:08:33  parker
 * Changes for new install-annex script
 * 
 * Revision 1.1  88/04/15  12:18:50  mattes
 * Initial revision
 * 
 *
 * This file is currently under revision by:
 *
 * $Locker:  $
 *
 *****************************************************************************/


/*****************************************************************************
 *									     *
 * Local defines and macros						     *
 *									     *
 *****************************************************************************/

#define CFGFILE		INSTALL_DIR/slipcfg"
#define SLIPDATA	"/usr/spool/slipd/D.%d"
#define OUTPIPE		"/usr/spool/slipd/outpipe"
#define PORTLOCK	"/usr/spool/slipd/portlock"


/*****************************************************************************
 *									     *
 * Structure and union definitions					     *
 *									     *
 *****************************************************************************/

struct sockiobuf {
    char *sb_base;	/* base address */
    char *sb_curr;	/* current address */
    int sb_len;		/* useful length */
    };


/*****************************************************************************
 *									     *
 * External data							     *
 *									     *
 *****************************************************************************/

/*****************************************************************************
 *									     *
 * Global data								     *
 *									     *
 *****************************************************************************/

/*****************************************************************************
 *									     *
 * Static data								     *
 *									     *
 *****************************************************************************/

/*****************************************************************************
 *									     *
 * Forward definitions							     *
 *									     *
 *****************************************************************************/
