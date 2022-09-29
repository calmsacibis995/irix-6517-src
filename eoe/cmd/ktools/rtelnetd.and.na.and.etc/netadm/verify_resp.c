/******************************************************************************
 *
 *        Copyright 1989, Xylogics, Inc.  ALL RIGHTS RESERVED.
 *
 * ALL RIGHTS RESERVED. Licensed Material - Property of Xylogics, Inc.
 * This software is made available solely pursuant to the terms of a
 * software license agreement which governs its use.
 * Unauthorized duplication, distribution or sale are strictly prohibited.
 *
 * Module Function:
 *  %$(Description)$%
 *
 * Original Author: %$(author)$%    Created on: %$(created-on)$%
 *
 * Revision Control Information:
 *
 * $Header: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/netadm/RCS/verify_resp.c,v 1.3 1996/10/04 12:15:09 cwilson Exp $
 *
 * This file created by RCS from $Source: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/netadm/RCS/verify_resp.c,v $
 *
 * Revision History:
 *
 * $Log: verify_resp.c,v $
 * Revision 1.3  1996/10/04 12:15:09  cwilson
 * latest rev
 *
 * Revision 2.7  1991/04/09  00:23:43  emond
 * Accommodate for generic API interface
 *
 * Revision 2.6  89/04/05  12:44:39  loverso
 * Changed copyright notice
 * 
 * Revision 2.5  88/05/24  18:41:10  parker
 * Changes for new install-annex script
 * 
 * Revision 2.4  88/04/15  12:39:19  mattes
 * SL/IP integration
 * 
 * Revision 2.3  87/06/10  18:09:51  parker
 * Added support for Xenix/Excelan PC.
 * 
 * Revision 2.2  86/12/03  16:40:28  harris
 * XENIX and SYSTEM V changes.
 * 
 * Revision 2.1  86/05/07  11:23:06  goodmon
 * Changes for broadcast command.
 * 
 * Revision 2.0  86/02/21  11:36:47  parker
 * First development revision for Release 2
 * 
 * Revision 1.1  85/11/01  17:52:34  palmer
 * Initial revision
 * 
 *
 * This file is currently under revision by:
 *
 * $Locker:  $
 *
 *
 ******************************************************************************/

#define RCSDATE $Date: 1996/10/04 12:15:09 $
#define RCSREV  $Revision: 1.3 $
#define RCSID   "$Header: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/netadm/RCS/verify_resp.c,v 1.3 1996/10/04 12:15:09 cwilson Exp $"
#ifndef lint
static char rcsid[] = RCSID;
#endif

/* Include Files */
#include "../inc/config.h"

#include <sys/types.h>
#include <netinet/in.h>
#include <sys/uio.h>
#include "../libannex/api_if.h"

#include "../inc/courier/courier.h"
#include "../inc/erpc/netadmp.h"
#include "netadm.h"
#include "netadm_err.h"

/* External Data Declarations */


/* Defines and Macros */


/* Structure Definitions */


/* Forward Routine Declarations */


/* Global Data Declarations */


/* Static Declarations */


verify_response(response, length)
    char            response[];
    int             length;

{

    COUR_HDR    *Pheader;

    /* Make sure that the response isn't a rejection or an abortion. */

    Pheader = (COUR_HDR *)&response[0];

    if (length == -1)
        return NAE_TIME;

    if (length < sizeof(COUR_MSG))
        return NAE_SRES;

    switch (ntohs(Pheader->msg.cm_type))
	{
	case C_REJECT:

	    if (length >= sizeof(CMREJECT) &&
	     ntohs(Pheader->rej.cmj_det) > 0 &&
	     ntohs(Pheader->rej.cmj_det) <= MAX_DETAIL)
		 return details[ntohs(Pheader->rej.cmj_det)];

	    return NAE_REJ;

	case C_ABORT:

	    if (length >= sizeof(CMABORT) &&
	     ntohs(Pheader->ab.cma_err) > 0 &&
	     ntohs(Pheader->ab.cma_err) <= MAX_ERROR)
		 return errors[ntohs(Pheader->ab.cma_err)];

	    return NAE_ABT;

	case C_RETURN:
	    break;

	default:
            return NAE_CTYP;
	}

    return NAE_SUCC;

}   /* verify_response() */
