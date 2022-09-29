/*
 *****************************************************************************
 *
 *        Copyright 1991, Xylogics, Inc.  ALL RIGHTS RESERVED.
 *
 * ALL RIGHTS RESERVED. Licensed Material - Property of Xylogics, Inc.
 * This software is made available solely pursuant to the terms of a
 * software license agreement which governs its use. 
 * Unauthorized duplication, distribution or sale are strictly prohibited.
 *
 * Include file description:
 *	Define appropriate API I/F according to which is selected
 *	(TLI, or SOCKETS).
 *
 * Original Author: D Emond	Created on: New Year's Eve
 *
 * Revision Control Information:
 * $Id: api_if.h,v 1.2 1996/10/04 12:07:30 cwilson Exp $
 *
 * This file created by RCS from
 * $Source: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/libannex/RCS/api_if.h,v $
 *
 * Revision History:
 * $Log: api_if.h,v $
 * Revision 1.2  1996/10/04 12:07:30  cwilson
 * latest rev
 *
 * Revision 1.3  1992/08/21  14:39:20  carlson
 * Removed ugly hacks that put TLI code into the upper level programs.
 *
 * Revision 1.2  91/06/27  14:47:46  emond
 * Move fcntl.h include from api_if.h to api_if.c
 * 
 * Revision 1.1  91/04/07  23:49:32  emond
 * Initial revision
 * 
 * This file is currently under revision by: $Locker:  $
 *
 ****************************************************************************
 */


#ifdef TLI

#include <stdio.h>
#include <sys/stream.h>
#include <sys/tiuser.h>
#include <sys/tihdr.h>

#else

#include <sys/socket.h>

#endif /* TLI */



#ifndef TRUE
#define	TRUE	1
#endif
#ifndef FALSE
#define	FALSE	0
#endif

/* Generic Transport Option flags. */
/* DON'T CHANGE THESE WITHOUT ALSO CHANGING t_opt[] in api_if.c */

#define	API_TO_ALIVE		0	/* Transport layer Keep alive	*/
#define	API_TO_DEBUG		1	/* Transport I/F Debugging 	*/
#define MAX_TO_OPTS	 	2	/* Max. number of options 	*/

/* Generic transport send flags. */
/* WARNING: KEEP THIS CONSISTENT WITH tp_flags in api_if.c */

#define	API_NOFLAGS	0	/* No flags set				*/
#define	API_PEEK	1	/* Sockets - MSG_PEEK; TLI - NULL 	*/
#define	API_DONTROUTE	2	/* Sockets - MSG_DONTROUTE; TLI - NULL	*/
#define	API_OOB		3	/* Sockets - MSG_OOB; TLI - T_EXPEDITED */
#define	MAX_FLGS	4	/* Max. number of flags			*/

/* Just dummy these things out. */
#define TLI_PTR(type,var,val)	void *var = (void *)NULL;
#define TLI_ALLOC(var,type,sock,ttype,ftype,app,todo)
#define TLI_MSG_RCPT(from)	from
