#ifndef __NETDB_H__
#define __NETDB_H__
#ident "$Revision: 1.25 $"
/*
*
* Copyright 1995, Silicon Graphics, Inc.
* All Rights Reserved.
*
* This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
* the contents of this file may not be disclosed to third parties, copied or
* duplicated in any form, in whole or in part, without the prior written
* permission of Silicon Graphics, Inc.
*
* RESTRICTED RIGHTS LEGEND:
* Use, duplication or disclosure by the Government is subject to restrictions
* as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
* and Computer Software clause at DFARS 252.227-7013, and/or in similar or
* successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
* rights reserved under the Copyright Laws of the United States.
*/

/*-
 * Copyright (c) 1980, 1983, 1988 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that: (1) source distributions retain this entire copyright
 * notice and comment, and (2) distributions including binaries display
 * the following acknowledgement:  ``This product includes software
 * developed by the University of California, Berkeley and its contributors''
 * in the documentation or other materials provided with the distribution
 * and in all advertising materials mentioning features or use of this
 * software. Neither the name of the University nor the names of its
 * contributors may be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 *	@(#)netdb.h	5.11 (Berkeley) 5/21/90
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <standards.h>
#include <netinet/in.h>

#define	_PATH_HEQUIV	"/etc/hosts.equiv"
#define	_PATH_HOSTS	"/etc/hosts"
#define	_PATH_NETWORKS	"/etc/networks"
#define	_PATH_PROTOCOLS	"/etc/protocols"
#define	_PATH_SERVICES	"/etc/services"
#define	_PATH_RPC	"/etc/rpc"

/*
 * Structures returned by network data base library.  All addresses are
 * supplied in host order, and returned in network order (suitable for
 * use in system calls).
 */
struct	hostent {
	char	*h_name;	/* official name of host */
	char	**h_aliases;	/* alias list */
	int	h_addrtype;	/* host address type */
	int	h_length;	/* length of address */
	char	**h_addr_list;	/* list of addresses from name server */
#define	h_addr	h_addr_list[0]	/* address, for backward compatiblity */
};

/*
 * Assumption here is that a network number
 * fits in 32 bits -- probably a poor one.
 */
struct	netent {
	char		*n_name;	/* official name of net */
	char		**n_aliases;	/* alias list */
	int		n_addrtype;	/* net address type */
	in_addr_t	n_net;		/* network # */
};

struct	servent {
	char	*s_name;	/* official service name */
	char	**s_aliases;	/* alias list */
	int	s_port;		/* port # */
	char	*s_proto;	/* protocol to use */
};

struct	protoent {
	char	*p_name;	/* official protocol name */
	char	**p_aliases;	/* alias list */
	int	p_proto;	/* protocol # */
};

#if _SGIAPI
/* from 4.0 RPCSRC */
struct rpcent {
	char	*r_name;	/* name of server for this rpc program */
	char	**r_aliases;	/* alias list */
	int	r_number;	/* rpc program number */
};
#endif /* _SGIAPI */

#if !defined(_SGI_COMPILING_LIBC)
#if _NO_XOPEN4
extern struct hostent	*gethostbyaddr(const void *, int, int);
#else
/* XPG4 compatibility, len changed to a size_t */
extern struct hostent	*__xpg4_gethostbyaddr(const void *, size_t, int);

/*REFERENCED*/ static struct hostent *
gethostbyaddr(const void *__addr, size_t __len, int __type)
{
	return(__xpg4_gethostbyaddr(__addr, __len, __type));
}
#endif
#else
/* prototypes for libc */
extern struct hostent	*gethostbyaddr(const void *, int, int);
extern struct hostent	*__xpg4_gethostbyaddr(const void *, size_t, int);
#endif
extern struct hostent	*gethostbyname(const char *),
			*gethostent(void);
extern struct netent	*getnetbyname(const char *),
			*getnetbyaddr(in_addr_t, int),
			*getnetent(void);
extern struct servent	*getservbyname(const char *, const char *),
			*getservbyport(int, const char *),
			*getservent(void);
extern struct protoent	*getprotobyname(const char *),
			*getprotobynumber(int),
			*getprotoent(void);
extern void 		sethostent(int),  endhostent(void),
	    		setnetent(int),   endnetent(void),
	    		setservent(int),  endservent(void),
	    		setprotoent(int), endprotoent(void);

#if _SGIAPI
extern struct rpcent	*getrpcbyname(const char *), 
			*getrpcbynumber(int), 
			*getrpcent(void);
extern void 		setrpcent(int),  
			endrpcent(void);
#endif /* _SGIAPI */

#if _SGIAPI
extern int rcmd(char **, unsigned short, char *, char *, char *, int *);
extern int rexec(char **, int, char *, char *, char *, int *);
extern int rresvport(int *);
extern int ruserok(char *, int, char *, char *);
#endif

/*
 * Error return codes from gethostbyname() and gethostbyaddr()
 * (left in extern int h_errno).
 */
extern int h_errno;
#if _SGIAPI
extern void herror(const char *);
extern char *hstrerror(int);

#define	NETDB_INTERNAL	-1	/* see errno */
#define	NETDB_SUCCESS	0	/* no problem */
#endif /* _SGIAPI */
#define	HOST_NOT_FOUND	1 /* Authoritative Answer Host not found */
#define	TRY_AGAIN	2 /* Non-Authoritive Host not found, or SERVERFAIL */
#define	NO_RECOVERY	3 /* Non recoverable errors, FORMERR, REFUSED, NOTIMP */
#define	NO_DATA		4 /* Valid name, no data record of requested type */
#define	NO_ADDRESS	NO_DATA		/* no address, look for MX record */

#define __HOST_SVC_NOT_AVAIL 99		/* libc internal use only */

#if _SGIAPI
/*
 * NIS network group
 */
extern int	innetgr(char *, char *, char *, char *);
extern int	getnetgrent(char **, char **, char **);
extern void	setnetgrent(char *);
extern void	endnetgrent(void);

/*
 * SGI specific
 */
#include <stdio.h>
extern void		sethostfile(char *);
extern int		sethostresorder(const char *); /* depricated */
extern struct rpcent	*getrpcbyname_r(const char *, struct rpcent *, char *,
			    int),
			*getrpcbynumber_r(int, struct rpcent *, char *, int),
			*getrpcent_r(struct rpcent *, char *, int),
			*fgetrpcent(FILE *),
			*fgetrpcent_r(FILE *, struct rpcent *, char *, int);
extern struct protoent	*getprotobyname_r(const char *, struct protoent *,
			    char *, int),
			*getprotobynumber_r(int, struct protoent *, char *,
			    int),
			*getprotoent_r(struct protoent *, char *, int),
			*fgetprotoent(FILE *),
			*fgetprotoent_r(FILE *, struct protoent *, char *, int);
extern struct servent	*getservbyname_r(const char *, const char *,
			    struct servent *, char *, int),
			*getservbyport_r(int, const char *, struct servent *,
			    char *, int),
			*getservent_r(struct servent *, char *, int),
			*fgetservent(FILE *),
			*fgetservent_r(FILE *, struct servent *, char *, int);
extern struct netent	*getnetbyname_r(const char *, struct netent *, char *,
			    int),
			*getnetbyaddr_r(in_addr_t, int, struct netent *, char *,
			    int),
			*getnetent_r(struct netent *, char *, int),
			*fgetnetent(FILE *),
			*fgetnetent_r(FILE *, struct netent *, char *, int);
extern struct hostent	*gethostbyname_r(const char *, struct hostent *, char *,
			    int, int *),
			*gethostbyaddr_r(const void *, size_t, int,
			    struct hostent *, char *, int, int *),
			*gethostent_r(struct hostent *, char *, int),
			*fgethostent(FILE *),
			*fgethostent_r(FILE *, struct hostent *, char *, int);
#endif /* _SGIAPI */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* !__NETDB_H__ */
