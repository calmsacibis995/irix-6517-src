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
 * $Header: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/netadm/RCS/return_param.c,v 1.3 1996/10/04 12:14:11 cwilson Exp $
 *
 * This file created by RCS from $Source: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/netadm/RCS/return_param.c,v $
 *
 * Revision History:
 *
 * $Log: return_param.c,v $
 * Revision 1.3  1996/10/04 12:14:11  cwilson
 * latest rev
 *
 * Revision 2.22  1994/06/14  10:45:32  bullock
 * spr.2802 - Added case MOP_PASSWD_P
 *
 * Revision 2.21  1994/05/25  10:33:49  russ
 * Bigbird na/adm merge.
 *
 * Revision 2.20  1993/12/30  14:16:04  carlson
 * SPR 2084 -- made compatible with 64 bit machines.
 *
 * Revision 2.19  1993/11/16  11:03:26  carlson
 * Fixes from 15NOV93 visit to HP Porting Center -- Pdata might not
 * be aligned properly for casting and dereferencing.  Use bcopy.
 *
 * Revision 2.18  93/02/18  21:44:32  raison
 * added DECserver Annex parameters
 * 
 * Revision 2.17  93/01/22  18:20:47  wang
 * New parameters support for Rel8.0
 * 
 * Revision 2.16  91/04/09  00:22:25  emond
 * Accommodate for generic API interface
 * 
 * Revision 2.15  91/01/23  21:52:40  raison
 * added lat_value na parameter and GROUP_CODE parameter type.
 * 
 * Revision 2.14  90/12/14  14:39:45  raison
 * added ADM_STRING type for sys_location (for LAT now, and snmp later).
 * 
 * Revision 2.13  90/12/04  12:13:54  sasson
 * The image_name was increased to 100 characters on annex3.
 * 
 * Revision 2.12  89/04/05  12:44:32  loverso
 * Changed copyright notice
 * 
 * Revision 2.11  88/11/29  14:50:59  harris
 * Long values fetched with get_unspec_long and stored with set_[unspec_]long.
 * This is because both Pdata and long_data may be "misaligned".
 * 
 * Revision 2.10  88/06/22  10:34:01  mattes
 * Fixed RAW_BLOCK
 * 
 * Revision 2.9  88/05/31  17:17:27  parker
 * Bug fix.
 * 
 * Revision 2.8  88/05/24  18:40:51  parker
 * Changes for new install-annex script
 * 
 * Revision 2.7  88/05/04  23:24:13  harris
 * Added new "typeless" type - RAW_BLOCK_P - no type field - len , data.
 * Removed courier header.  Address of message is actual procedure data.
 * 
 * Revision 2.6  88/04/15  12:38:40  mattes
 * SL/IP integration
 * 
 * Revision 2.5  87/06/10  18:09:42  parker
 * Added support for Xenix/Excelan PC.
 * 
 * Revision 2.4  87/03/05  15:40:52  parker
 * changed set_{unspec_}long() to get_{unspec_}long().
 * 
 * Revision 2.3  87/01/05  16:41:29  parker
 * changed to use set_long() and set_unspec_l() for portablity to Pyramid.
 * 
 * Revision 2.2  86/12/03  16:39:03  harris
 * XENIX and SYSTEM V changes.
 * 
 * Revision 2.1  86/05/07  11:21:26  goodmon
 * Changes for broadcast command.
 * 
 * Revision 2.0  86/02/21  11:36:17  parker
 * First development revision for Release 2
 * 
 * Revision 1.1  85/11/01  17:51:54  palmer
 * Initial revision
 * 
 *
 * This file is currently under revision by:
 *
 * $Locker:  $
 *
 *
 ******************************************************************************/

#define RCSDATE $Date: 1996/10/04 12:14:11 $
#define RCSREV  $Revision: 1.3 $
#define RCSID   "$Header: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/netadm/RCS/return_param.c,v 1.3 1996/10/04 12:14:11 cwilson Exp $"
#ifndef lint
static char rcsid[] = RCSID;
#endif

/* Include Files */
#include "../inc/config.h"

#include "port/port.h"
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/uio.h>
#include "../libannex/api_if.h"

#include "../inc/courier/courier.h"
#include "../inc/erpc/netadmp.h"
#include "netadm.h"
#include "netadm_err.h"

/* External Data Declarations */
extern UINT32 get_unspec_long();

/* Defines and Macros */


/* Structure Definitions */


/* Forward Routine Declarations */


/* Global Data Declarations */


/* Static Declarations */


return_param(Pdata, type, response, rmsgsize)

    char    *Pdata;		/* place to put final result */
    u_short type;		/* type of parameter expected */
    char    response[];		/* return message offset, no courier hdr */
    int     rmsgsize;		/* size of return message w/o courier */
{
    u_short string_length;
    u_short return_type;
    UINT32  longval;		/* properly aligned long for conversion */

    PARAM   *Pparam;

    /* If no Pdata, no conversion is performed! */

    if(!Pdata)
	return NAE_SUCC;

    Pparam = (PARAM *)(&response[0]);

    /* Check the type of the returned param, unless a RAW_BLOCK_P. */
    /* If requested type is STRING_P_100 and we got type STRING_P, */
    /* we are probably dealing with and old annex, this is not an error. */

    return_type = ntohs(Pparam->type);

    if ((return_type != type)  &&
	(type != RAW_BLOCK_P)  &&
	!( (type == STRING_P_100) && (return_type == STRING_P) ))
      return NAE_TYPE;
    /* Pass the param back to the caller. */

    switch (type)
        {
        case CARDINAL_P:
	case BOOLEAN_P:
	    if (rmsgsize < 2 * sizeof(u_short))
                return NAE_SRES;

            string_length = ntohs(Pparam->data.short_data);
	    (void)bcopy(&string_length,Pdata,sizeof(u_short));

            break;

        case LONG_CARDINAL_P:
	    if (rmsgsize < sizeof(u_short) + sizeof(UINT32))
                return NAE_SRES;

	    longval = get_unspec_long(Pparam->data.long_data);
	    set_long((u_short *)Pdata, longval);

            break;

        case LONG_UNSPEC_P:
	    if (rmsgsize < sizeof(u_short) + sizeof(UINT32))
                return NAE_SRES;

	    longval = get_unspec_long(Pparam->data.long_data);
	    set_unspec_long((u_short *)Pdata, longval);

            break;

        case ENET_ADDR_P:

            if (rmsgsize < sizeof(u_short) + ENET_ADDR_SZ)
                return NAE_SRES;

            (void)bcopy(Pparam->data.raw_data, Pdata, ENET_ADDR_SZ);

            break;

        case STRING_P:
        case ADM_STRING_P:
        case STRING_P_100:
        case RIP_ROUTERS_P:
	case KERB_HOST_P:
	case IPX_STRING_P:
	case MOP_PASSWD_P:
	    if (rmsgsize < 2 * sizeof(u_short))
                return NAE_SRES;

	    string_length = ntohs(Pparam->data.string.count);

	    if (rmsgsize < 2 * sizeof(u_short) + string_length)
	        return NAE_SRES;

	    (void)bcopy(&string_length,Pdata,sizeof(u_short));
	    (void)bcopy(Pparam->data.string.data,Pdata+sizeof(u_short),
		string_length);

            break;

        case RAW_BLOCK_P:
	    if (rmsgsize < sizeof(u_short))
                return NAE_SRES;

	    (void)bcopy(response,&string_length,sizeof(u_short));
	    string_length = ntohs(string_length);

	    if (rmsgsize < sizeof(u_short) + string_length)
	        return NAE_SRES;

	    (void)bcopy(&string_length,Pdata,sizeof(u_short));
	    (void)bcopy(response+sizeof(u_short),Pdata+sizeof(u_short),
		string_length);

            break;
	case LAT_GROUP_P:
	    if (rmsgsize < sizeof(u_short) + LAT_GROUP_SZ)
	        return NAE_SRES;

             (void)bcopy(Pparam->data.raw_data, Pdata, LAT_GROUP_SZ);

            break;

        default:
            return NAE_RTYP;

        } /* switch (type) */

    return NAE_SUCC;

}   /* return_param() */
