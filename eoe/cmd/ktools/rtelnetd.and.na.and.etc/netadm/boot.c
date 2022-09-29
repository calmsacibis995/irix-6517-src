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
 * $Header: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/netadm/RCS/boot.c,v 1.3 1996/10/04 12:11:24 cwilson Exp $
 *
 * This file created by RCS from $Source: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/netadm/RCS/boot.c,v $
 *
 * Revision History:
 *
 * $Log: boot.c,v $
 * Revision 1.3  1996/10/04 12:11:24  cwilson
 * latest rev
 *
 * Revision 2.11  1993/12/30  14:15:37  carlson
 * SPR 2084 -- made compatible with 64 bit machines.
 *
 * Revision 2.10  1991/11/05  10:40:33  carlson
 * SPR 396 -- respect bounds of strings.
 *
 * Revision 2.9  91/04/09  00:10:29  emond
 * Accommodate generic TLI interface
 * 
 * Revision 2.8  89/05/18  15:04:59  grant
 * Add shutdown RPC call for boot shutdown option.
 * 
 * Revision 2.7  89/04/05  12:44:12  loverso
 * Changed copyright notice
 * 
 * Revision 2.6  88/05/24  18:40:00  parker
 * Changes for new install-annex script
 * 
 * Revision 2.5  88/05/04  23:12:36  harris
 * Use rpc() interface.
 * 
 * Revision 2.4  88/04/15  12:35:36  mattes
 * SL/IP integration
 * 
 * Revision 2.3  87/06/10  18:09:21  parker
 * Added support for Xenix/Excelan PC.
 * 
 * Revision 2.2  86/12/03  16:43:44  harris
 * XENIX and SYSTEM V changes.
 * 
 * Revision 2.1  86/05/07  11:17:11  goodmon
 * Changes for broadcast command.
 * 
 * Revision 2.0  86/02/21  11:32:22  parker
 * First development revision for Release 2
 * 
 * Revision 1.1  85/11/01  17:46:14  palmer
 * Initial revision
 * 
 *
 * This file is currently under revision by:
 *
 * $Locker:  $
 *
 *
 ******************************************************************************/

#define RCSDATE $Date: 1996/10/04 12:11:24 $
#define RCSREV  $Revision: 1.3 $
#define RCSID   "$Header: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/netadm/RCS/boot.c,v 1.3 1996/10/04 12:11:24 cwilson Exp $"
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

/* Defines and Macros */

#define OUTGOING_COUNT  	6
#define BOOT_OUTGOING_COUNT 	1

/* Structure Definitions */


/* Forward Routine Declarations */


/* Global Data Declarations */


/* Static Declarations */


/*
 *****************************************************************************
 *
 * Function Name:
 *	boot()
 *
 * Functional Description:
 *	Makes an RPC arguments out of the parameters and sends them to the
 *      target annex.  If the shutdown flag is set then send a RPROC shutdown
 *      RPC.  Otherwise acording to the dump flag send an RPROC_BOOT or 
 *      RPROC_DUMPBOOT RPC.
 *
 * Parameters:
 *  	Pinet_addr - pointer to the target annex's inet address
 *  	filename   - image to boot name,
 *      warning    - warning message, shutdown only. 
 *      username   - username that initiated shutdown, shutdown only.
 *      hostname   - hostname where shutdown orifinated from, shutdown only.
 *      switches   - options used by shutdown, shutdown only.
 *   	boot_time  - offset in seconds until boot.
 *      dump       - flag signaling an RPROC_DUMPBOOT or RPROC_BOOT.
 *      shutdown   - flag signaling using a RPROC_SHUTDOWN. 
 *
 * Return Value:
 *	Error code from RPC.	
 *
 *****************************************************************************
 */

/*
 * Copy length-limited string from s into ts, and then fill in io vector
 * at index ind.  Assumes that recipient wants null-terminated strings.
 */

#define STRCOPY(ind,ts,s) \
	strncpy(ts,s,sizeof(ts)-1);		\
	(ts)[sizeof(ts)-1] = '\0';		\
	outgoing[ind].iov_base = (caddr_t)(ts);	\
	outgoing[ind].iov_len = strlen(ts) + 1;

boot(Pinet_addr, filename, warning, switches,
     boot_time, username, hostname, dump, shutdown)
    	struct sockaddr_in *Pinet_addr;
    	char        filename[], warning[], username[], hostname[];
    	u_short	    switches,
		    dump,
		    shutdown; 
    	time_t      boot_time;
{
    	u_short 
		param_one,
                string_length;

    	UINT32 
		param_two;

    	struct iovec 
		boot_outgoing[BOOT_OUTGOING_COUNT +1],
		outgoing[OUTGOING_COUNT + 1];

    	char         
		param_three[FILENAME_LENGTH],
    	    	param_four[WARNING_LENGTH],
	   	param_five[USERNAME_LENGTH],
		param_six[HOSTNAME_LENGTH];

    	char    
		boot_param_one[FILENAME_LENGTH + sizeof(u_short)];

    	if (Pinet_addr->sin_family != AF_INET)
        	return NAE_ADDR;

    /*
     * Set up outgoing iovecs.
     * outgoing[0] is only used by erpc_callresp().
     * outgoing[1] contains the switches.
     * outgoing[2] contains the boot_time.
     * outgoing[3] contains the filename.
     * outgoing[4] contains the warning message.
     * outgoing[5] contains the username who started boot.
     * outgoing[6] contains the hostname who started boot.
     *
     *  The last for vectors contain variable length strings.  These
     *  will be sent with out a length header, instead they rely on
     * 	being null terminated.
     */

	if(shutdown) {
	
		param_one = htons(switches);
		outgoing[1].iov_base = (caddr_t)&param_one;
		outgoing[1].iov_len = sizeof(param_one);

		param_two = htonl(boot_time);
		outgoing[2].iov_base = (caddr_t)&param_two;
		outgoing[2].iov_len = sizeof(param_two);

		STRCOPY(3,param_three,filename);
		STRCOPY(4,param_four,warning);
		STRCOPY(5,param_five,username);
		STRCOPY(6,param_six,hostname);

	    	return rpc(Pinet_addr, RPROC_SHUTDOWN, OUTGOING_COUNT, outgoing,
		         (char *)0, (u_short)0);
	}
	else {

	       /*
     		*  Set up outgoing iovecs.
     		*  outgoing[0] is only used by erpc_callresp().
     		*  outgoing[1] contains the filename. 
     		*/
	
		string_length = strlen(filename);

		*(u_short *)boot_param_one = htons(string_length);
		(void)strcpy(&boot_param_one[sizeof(u_short)], filename);
		boot_outgoing[1].iov_base = (caddr_t)&boot_param_one[0];
		boot_outgoing[1].iov_len=sizeof(u_short)+string_length;

	    	return rpc(Pinet_addr, 
			 dump?RPROC_DUMPBOOT:RPROC_BOOT, 
			 BOOT_OUTGOING_COUNT, boot_outgoing,
		         (char *)0, (u_short)0);
	}

}   /* boot() */
