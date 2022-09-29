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
 * $Header: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/netadm/RCS/set_ln_param.c,v 1.3 1996/10/04 12:14:46 cwilson Exp $
 *
 * This file created by RCS from $Source: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/netadm/RCS/set_ln_param.c,v $
 *
 * Revision History:
 *
 * $Log: set_ln_param.c,v $
 * Revision 1.3  1996/10/04 12:14:46  cwilson
 * latest rev
 *
 * Revision 2.13  1993/12/30  14:16:48  carlson
 * SPR 2084 -- made compatible with 64 bit machines.
 *
 * Revision 2.12  1993/03/01  17:38:21  wang
 * Fixed "set port phone_number" not working problem in NA.
 *
 * Revision 2.11  92/02/18  17:34:22  sasson
 * Added support for the LAT parameter authorized_group (one per port).
 * 
 * Revision 2.10  91/04/09  00:23:28  emond
 * Accommodate for generic API interface
 * 
 * Revision 2.9  89/04/05  12:44:38  loverso
 * Changed copyright notice
 * 
 * Revision 2.8  88/05/24  18:41:03  parker
 * Changes for new install-annex script
 * 
 * Revision 2.7  88/05/04  23:31:19  harris
 * Use rpc() interface.
 * 
 * Revision 2.6  88/04/15  12:39:06  mattes
 * SL/IP integration
 * 
 * Revision 2.5  87/06/10  18:09:48  parker
 * Added support for Xenix/Excelan PC.
 * 
 * Revision 2.4  87/06/01  11:57:44  harris
 * #ifdefs around include files for UMAX V (BERKNET) and XENIX (EXOS).
 * 
 * Revision 2.3  87/03/05  15:51:06  parker
 * fixed usage of set_long() and set_unspec_long().
 * 
 * Revision 2.2  86/12/08  12:09:29  parker
 * R2.1 NA parameter changes
 * 
 * Revision 2.1  86/05/07  11:22:35  goodmon
 * Changes for broadcast command.
 * 
 * Revision 2.0  86/02/21  11:36:37  parker
 * First development revision for Release 2
 * 
 * Revision 1.1  85/11/01  17:52:19  palmer
 * Initial revision
 * 
 *
 * This file is currently under revision by:
 *
 * $Locker:  $
 *
 *
 ******************************************************************************/

#define RCSDATE $Date: 1996/10/04 12:14:46 $
#define RCSREV  $Revision: 1.3 $
#define RCSID   "$Header: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/netadm/RCS/set_ln_param.c,v 1.3 1996/10/04 12:14:46 cwilson Exp $"
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
extern UINT32 get_long(), get_unspec_long();


/* Defines and Macros */

#define OUTGOING_COUNT  5

/* Structure Definitions */


/* Forward Routine Declarations */


/* Global Data Declarations */


/* Static Declarations */


set_ln_param(Pinet_addr, device_type, line_number, cat, number, type, Pdata)
    struct sockaddr_in *Pinet_addr;
    u_short         device_type;
    u_short         line_number;
    u_short         cat;
    u_short         number;
    u_short         type;
    char            *Pdata;

{
    struct iovec    outgoing[OUTGOING_COUNT + 1];

    u_short         param_one,
                    param_two,
                    param_three,
                    param_four,
                    group;

    u_short         string_length;

    PARAM           param_five;
    u_char	    group_code[LAT_GROUP_SZ];
    u_char	    *fmptr, *toptr;
    int		    error;

    /* Check *Pinet_addr address family. */

    if (Pinet_addr->sin_family != AF_INET)
        return NAE_ADDR;

    /* Set up outgoing iovecs.
       outgoing[0] is only used by erpc_callresp().
       outgoing[1] contains the device_type.
       outgoing[2] contains the line_number.
       outgoing[3] contains the catagory.
       outgoing[4] contains the line param number.
       outgoing[5] contains the line param. */

    param_one = htons(device_type);
    outgoing[1].iov_base = (caddr_t)&param_one;
    outgoing[1].iov_len = sizeof(param_one);

    param_two = htons(line_number);
    outgoing[2].iov_base = (caddr_t)&param_two;
    outgoing[2].iov_len = sizeof(param_two);

    param_three = htons(cat);
    outgoing[3].iov_base = (caddr_t)&param_three;
    outgoing[3].iov_len = sizeof(param_three);

    param_four = htons(number);
    outgoing[4].iov_base = (caddr_t)&param_four;
    outgoing[4].iov_len = sizeof(param_four);

    param_five.type = htons(type);
    switch (type)
        {
        case CARDINAL_P:
        case BOOLEAN_P:
            param_five.data.short_data = htons(*(u_short *)Pdata);
	    outgoing[5].iov_len = 2 * sizeof(u_short);
            break;

        case LONG_CARDINAL_P:
            set_long(param_five.data.long_data,
				get_unspec_long((u_short *)Pdata));
	    outgoing[5].iov_len = sizeof(u_short) + sizeof(UINT32);
            break;

        case LONG_UNSPEC_P:
            set_unspec_long(param_five.data.long_data,
				get_unspec_long((u_short *)Pdata));
	    outgoing[5].iov_len = sizeof(u_short) + sizeof(UINT32);
            break;

	case STRING_P:
	case ADM_STRING_P:
	    string_length = *(u_short *)Pdata;
	    param_five.data.string.count = htons(string_length);
	    (void)bcopy(&Pdata[sizeof(u_short)], param_five.data.string.data,
	     (int)string_length);
	    outgoing[5].iov_len = 2 * sizeof(u_short) + string_length;
	    break;

        case LAT_GROUP_P:

	    /* in the case of this type, we are requested to turn on */
	    /* or off certain bits in a 32 byte field.  To do this, we */
	    /* must first get the current value, then manipulate it. */

            param_five.data.string.count = htons(LAT_GROUP_SZ);
	    if(error = get_ln_param(Pinet_addr, device_type, line_number,
				(u_short)cat, (u_short)number,
				(u_short)type, group_code)) {
			return(error);
	    }
	    fmptr = (u_char *) Pdata;
	    toptr = group_code;
	    if (Pdata[LAT_GROUP_SZ]) {		/* if enable */
		for (group = 0; group < LAT_GROUP_SZ; group++) {
		    *toptr++ |= *fmptr++;
		}
	    } else {					/* else disable */
		for (group = 0; group < LAT_GROUP_SZ; group++) {
		    *toptr++ &= ~(*fmptr++);
		}
	    }
	    (void)bcopy(group_code, param_five.data.raw_data,
	     					(int)LAT_GROUP_SZ);
            outgoing[5].iov_len = sizeof(u_short) + LAT_GROUP_SZ;
            break;

        default:
            return NAE_TYPE;
        }
    outgoing[5].iov_base = (caddr_t)&param_five;

    /* Call rpc() to communicate the request to the annex via erpc or srpc. */

    return rpc(Pinet_addr, RPROC_SET_LINE_PARAM, OUTGOING_COUNT, outgoing,
	       (char *)0, (u_short)0);

}   /* set_ln_param() */
