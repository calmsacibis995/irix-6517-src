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
 *	 4.3BSD Protocol Definitions for SL/IP
 *
 * Original Author:  Paul Mattes		Created on: 01/04/88
 *
 * Revision Control Information:
 *
 * $Header: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/inc/slip/RCS/BSDslip.h,v 1.2 1996/10/04 12:06:44 cwilson Exp $
 *
 * This file created by RCS from:
 * $Source: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/inc/slip/RCS/BSDslip.h,v $
 *
 * Revision History:
 *
 * $Log: BSDslip.h,v $
 * Revision 1.2  1996/10/04 12:06:44  cwilson
 * latest rev
 *
 * Revision 1.3  89/04/05  14:48:18  root
 * Changed copyright notice
 * 
 * Revision 1.2  88/05/31  17:08:25  parker
 * Changes for new install-annex script
 * 
 * Revision 1.1  88/04/15  12:18:02  mattes
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
 * Include files							     *
 *									     *
 *****************************************************************************/

/*****************************************************************************
 *									     *
 * Local defines and macros						     *
 *									     *
 *****************************************************************************/

#define	SLMTU	1006				/* Maximum transmission unit */

#define FRAME_END	 	0300		/* Frame End */
#define FRAME_ESCAPE		0333		/* Frame Esc */
#define TRANS_FRAME_END	 	0334		/* transposed frame end */
#define TRANS_FRAME_ESCAPE 	0335		/* transposed frame esc */


/*****************************************************************************
 *									     *
 * Structure and union definitions					     *
 *									     *
 *****************************************************************************/

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
