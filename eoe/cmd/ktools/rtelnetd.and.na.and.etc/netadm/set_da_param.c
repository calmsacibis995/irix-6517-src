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
 *      %$(Description)$%
 *
 * Original Author: %$(author)$%        Created on: %$(created-on)$%
 *
 * Revision Control Information:
 *
 * $Header: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/netadm/RCS/set_da_param.c,v 1.3 1996/10/04 12:14:27 cwilson Exp $
 *
 * This file created by RCS from $Source: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/netadm/RCS/set_da_param.c,v $
 *
 * Revision History:
 *
 * $Log: set_da_param.c,v $
 * Revision 1.3  1996/10/04 12:14:27  cwilson
 * latest rev
 *
 * Revision 2.19  1994/09/13  13:07:18  geiser
 * Add support for MOP_PASSWD_P (forgot to port this from 7.1)
 *
 * Revision 2.18  1994/05/25  10:33:28  russ
 * Bigbird na/adm merge.
 *
 * Revision 2.17  1993/12/30  14:16:24  carlson
 * SPR 2084 -- made compatible with 64 bit machines.
 *
 * Revision 2.16  1993/02/18  21:44:41  raison
 * added DECserver Annex parameters
 *
 * Revision 2.15  93/02/05  18:20:20  wang
 * Support new eeprom encode type.
 * 
 * Revision 2.14  91/04/10  16:59:14  raison
 * fixed ptr warning
 * 
 * Revision 2.13  91/04/09  00:22:59  emond
 * Accommodate for generic API interface
 * 
 * Revision 2.12  91/01/23  21:52:49  raison
 * added lat_value na parameter and GROUP_CODE parameter type.
 * 
 * Revision 2.11  90/12/14  14:41:26  raison
 * added ADM_STRING type for sys_location (for LAT now, and snmp later).
 * 
 * Revision 2.10  90/12/04  12:21:14  sasson
 * The image_name was increased to 100 characters on annex3.
 * 
 * Revision 2.9  89/04/05  12:44:36  loverso
 * Changed copyright notice
 * 
 * Revision 2.8  88/05/24  18:40:55  parker
 * Changes for new install-annex script
 * 
 * Revision 2.7  88/05/04  23:28:35  harris
 * Use rpc() interface.
 * 
 * Revision 2.6  88/04/15  12:38:48  mattes
 * SL/IP integration
 * 
 * Revision 2.5  87/06/10  18:09:44  parker
 * Added support for Xenix/Excelan PC.
 * 
 * Revision 2.4  87/06/01  11:55:24  harris
 * Include files conditioned by #ifdefs for UMAX V (BERKNET) and XENIX (EXOS).
 * 
 * Revision 2.3  87/03/05  15:49:25  parker
 * fixed usage of set_long() and set_unspec_long().
 * 
 * Revision 2.2  86/12/08  12:07:56  parker
 * R2.1 NA parameter changes
 * 
 * Revision 2.1  86/05/07  11:21:51  goodmon
 * Changes for broadcast command.
 * `.
 * 
 * Revision 2.0  86/02/21  11:36:25  parker
 * First development revision for Release 2
 * 
 * Revision 1.1  85/11/01  17:52:02  palmer
 * Initial revision
 * 
 *
 * This file is currently under revision by:
 *
 * $Locker:  $
 *
 *
 ******************************************************************************/

#define RCSDATE $Date: 1996/10/04 12:14:27 $
#define RCSREV  $Revision: 1.3 $
#define RCSID   "$Header: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/netadm/RCS/set_da_param.c,v 1.3 1996/10/04 12:14:27 cwilson Exp $"
#ifndef lint
static char rcsid[] = RCSID;
#endif

/* Include Files */
#include "../inc/config.h"

#include "port/port.h"
#include <sys/types.h>
#include "../libannex/api_if.h"
#include <netinet/in.h>
#include <sys/uio.h>

#include "../inc/courier/courier.h"
#include "../inc/erpc/netadmp.h"
#include "netadm.h"
#include "netadm_err.h"

/* External Data Declarations */
extern UINT32 get_long(), get_unspec_long();

/* defines and Macros */

#define OUTGOING_COUNT  3

/* Structure Definitions */


/* Forward Routine Declarations */


/* Global Data Declarations */


/* Static Declarations */


set_dla_param(Pinet_addr, cat, number, type, Pdata)
    struct sockaddr_in *Pinet_addr;
    u_short         cat;
    u_short         number;
    u_short         type;
    char            *Pdata;

{
    struct iovec    outgoing[OUTGOING_COUNT + 1];

    u_char	    *fmptr, *toptr;
    int		    error;
    u_short         param_one,
                    param_two,
    	            string_length,
		    group;

    PARAM           param_three;
    u_char	    group_code[LAT_GROUP_SZ];

    /* Check *Pinet_addr address family. */

    if (Pinet_addr->sin_family != AF_INET)
        return NAE_ADDR;

    /* Set up outgoing iovecs.
    outgoing[0] is used only by erpc_callresp().
    outgoing[1] contains the catagory.
    outgoing[2] contains the dla param number.
    outgoing[3] contains the data. */

    param_one = htons(cat);
    outgoing[1].iov_base = (caddr_t)&param_one;
    outgoing[1].iov_len = sizeof(param_one);

    param_two = htons(number);
    outgoing[2].iov_base = (caddr_t)&param_two;
    outgoing[2].iov_len = sizeof(param_two);

    param_three.type = htons(type);
    switch (type)
        {
        case CARDINAL_P:
        case BOOLEAN_P:
            param_three.data.short_data = htons(*(u_short *)Pdata);
            outgoing[3].iov_len = 2 * sizeof(u_short);
            break;

        case LONG_CARDINAL_P:
            set_long(param_three.data.long_data,
				get_unspec_long((u_short *)Pdata));
            outgoing[3].iov_len = sizeof(u_short) + sizeof(UINT32);
            break;

        case LONG_UNSPEC_P:
            set_unspec_long(param_three.data.long_data,
				get_unspec_long((u_short *)Pdata));
            outgoing[3].iov_len = sizeof(u_short) + sizeof(UINT32);
            break;

        case ENET_ADDR_P:
            (void)bcopy(Pdata, param_three.data.raw_data, ENET_ADDR_SZ);
            outgoing[3].iov_len = sizeof(u_short) + ENET_ADDR_SZ;
            break;

        case MOP_PASSWD_P:
            (void)bcopy(Pdata, param_three.data.raw_data, MOP_PASSWD_SZ);
            outgoing[3].iov_len = sizeof(u_short) + MOP_PASSWD_SZ;
            break;

        case STRING_P:
        case STRING_P_100:
        case ADM_STRING_P:
        case RIP_ROUTERS_P:
	case KERB_HOST_P:
	case IPX_STRING_P:
            string_length = *(u_short *)Pdata;
            param_three.data.string.count = htons(string_length);
	    (void)bcopy(&Pdata[sizeof(u_short)], param_three.data.string.data,
	     (int)string_length);
            outgoing[3].iov_len = 2 * sizeof(u_short) + string_length;
            break;

        case LAT_GROUP_P:

	    /* in the case of this type, we are requested to turn on */
	    /* or off certain bits in a 32 byte field.  To do this, we */
	    /* must first get the current value, then manipulate it. */

            param_three.data.string.count = htons(LAT_GROUP_SZ);
	    if(error = get_dla_param(Pinet_addr, (u_short)cat,
				(u_short)number, (u_short)type, group_code)) {
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
	    (void)bcopy(group_code, param_three.data.raw_data,
	     					(int)LAT_GROUP_SZ);
            outgoing[3].iov_len = sizeof(u_short) + LAT_GROUP_SZ;
            break;

        default:
            return NAE_TYPE;
        }
    outgoing[3].iov_base = (caddr_t)&param_three;

    /* Call rpc() to communicate the request to the annex via erpc or srpc. */

    return rpc(Pinet_addr, RPROC_SET_DLA_PARAM, OUTGOING_COUNT, outgoing,
	       (char *)0, (u_short)0);

}       /* set_dla_param() */
