/*
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 * 
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 * 
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 * 
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 *
 * $Id: client.h,v 2.10 1997/09/05 00:58:03 markgw Exp $
 */

#ifndef _CLIENT_H
#define _CLIENT_H

#include <unistd.h>
#if defined(sgi)
#include <bstring.h>
#endif
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>

/* The table of clients, used by pmcd */
typedef struct {
    int			fd;		/* Socket descriptor */
    struct sockaddr_in	addr;		/* Address of client */
    struct {				/* Status of connection to client */
	unsigned int	connected : 1;	/* Client connected */
	unsigned int	changes : 3;	/* PMCD_* bits for changes since last fetch */
    } status;
    /* There is an array of profiles, as there is a profile associated
     * with each client context.  The array is not guaranteed to be dense.
     * The context number sent with each profile/fetch is used to index it.
     */
    __pmProfile		**profile;	/* Client context profile pointers */
    int			szProfile;	/* Size of array */
    unsigned int	denyOps;	/* Disallowed operations for client */
    __pmPDUInfo		pduInfo;
} ClientInfo;

extern ClientInfo	*client;		/* Array of clients */
extern int		nClients;		/* Number of entries in array */
extern int		maxClientFd;		/* largest fd for a client */
extern fd_set		clientFds;		/* for client select() */

/* prototypes */
extern ClientInfo *AcceptNewClient(int);
extern int NewClient(void);
extern void DeleteClient(ClientInfo *);
extern void ShowClients(FILE *m);
#ifdef PCP_DEBUG
extern char *nameclient(int);
#endif

#endif /* _CLIENT_H */
