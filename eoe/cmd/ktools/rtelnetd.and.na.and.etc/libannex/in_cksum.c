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
 *	Internet checksum calculator
 *
 * Original Author: Paul Mattes		Created on: 01/04/88
 *
 * Revision Control Information:
 *
 * $Header: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/libannex/RCS/in_cksum.c,v 1.3 1996/10/04 12:08:26 cwilson Exp $
 *
 * This file created by RCS from:
 * $Source: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/libannex/RCS/in_cksum.c,v $
 *
 * Revision History:
 *
 * $Log: in_cksum.c,v $
 * Revision 1.3  1996/10/04 12:08:26  cwilson
 * latest rev
 *
 * Revision 1.5  1993/12/30  13:13:48  carlson
 * SPR 2084 -- made compatible with 64 bit machines.
 *
 * Revision 1.4  1989/04/05  12:40:16  loverso
 * Changed copyright notice
 *
 * Revision 1.3  88/05/31  17:12:12  parker
 * fixes to get XENIX/SLIP to build again
 * 
 * Revision 1.2  88/05/24  18:35:40  parker
 * Changes for new install-annex script
 * 
 * Revision 1.1  88/04/15  11:57:14  mattes
 * Initial revision
 * 
 *
 * This file is currently under revision by:
 *
 * $Locker:  $
 *
 *****************************************************************************
 */

#define RCSDATE $Date: 1996/10/04 12:08:26 $
#define RCSREV	$Revision: 1.3 $
#define RCSID   "$Header: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/libannex/RCS/in_cksum.c,v 1.3 1996/10/04 12:08:26 cwilson Exp $"

#ifndef lint
static char rcsid[] = RCSID;
#endif

/*****************************************************************************
 *									     *
 * Include files							     *
 *									     *
 *****************************************************************************/

#include <sys/types.h>
#include "../inc/config.h"
#include "port/port.h"
#include "../inc/slip/slip_user.h"		/* useful data types */


/*****************************************************************************
 *									     *
 * Local defines and macros						     *
 *									     *
 *****************************************************************************/

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

/*
 * Slow but reliable Internet checksum calculator
 * Calculates a 16-bit unsigned checksum over a range of unsigned characters
 */

u_short _in_cksum(buffer, length)
u_char *buffer;
int length;	/* in bytes; hopefully, never odd */
{
    UINT32 result = 0L;
    u_short *p = (u_short *)buffer;
    int len = (length + 1) >> 1;

    while(len--) {
	result += *p++;
	if(result & 0x10000L)
	    result -= 0xffffL;
	}

    return (u_short)(~result & 0xffffL);
}
