/*
 *****************************************************************************
 *
 *        Copyright 1989, Xylogics, Inc.  ALL RIGHTS RESERVED.
 *
 * ALL RIGHTS RESERVED. Licensed Material - Property of Xylogics, Inc.
 * This software is made available solely pursuant to the terms of a
 * software license agreement which governs its use. 
 * Unauthorized duplication, distribution or sale are strictly prohibited.
 *
 * Module Description::
 *
 * 	Header definitions for libannex compatibility routines
 *
 * Original Author: %$(author)$%	Created on: %$(created-on)$%
 *
 * Module Reviewers:
 *	%$(reviewers)$%
 *
 * Revision Control Information:
 * $Header: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/inc/port/RCS/libannex.h,v 1.2 1996/10/04 12:06:11 cwilson Exp $
 *
 * This file created by RCS from
 * $Source: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/inc/port/RCS/libannex.h,v $
 *
 * Revision History:
 * $Log: libannex.h,v $
 * Revision 1.2  1996/10/04 12:06:11  cwilson
 * latest rev
 *
 * Revision 1.11  1994/08/29  14:51:54  carlson
 * SPR 1321 -- added support for compilers without "void".
 * Also added possible fix for more 64-bit problems.
 *
 * Revision 1.10  1993/06/24  21:52:12  reeve
 * Resolved sigmask for any platform that doesn't have it.
 *
 * Revision 1.9  1992/01/24  15:18:42  carlson
 * SPR 513 -- don't include socket stuff if have_sockdefs is defined.
 *
 * Revision 1.8  91/04/08  14:37:08  emond
 * Added option of defining some socket defines (i.e. sockadd struct) when
 * used with TLI.
 * 
 * Revision 1.7  89/10/16  17:17:35  loverso
 * Merge in portability changes
 * 
 * Revision 1.6  89/09/25  17:51:01  loverso
 * Don't include struct msghdr if have_msghdr is defined
 * 
 * Revision 1.5  89/04/14  13:42:11  loverso
 * only do things as "need"ed
 * 
 * Revision 1.4  89/04/13  22:55:40  loverso
 * u_long wrong again
 * 
 * Revision 1.3  89/04/13  16:20:10  loverso
 * do the typedefs before you use them!
 * 
 * Revision 1.2  89/04/12  15:10:14  loverso
 * additional mappings, as needed
 * 
 * Revision 1.1  89/04/10  23:38:26  loverso
 * Initial revision
 * 
 * This file is currently under revision by: $Locker:  $
 *
 *****************************************************************************
 */


/*
 * Replace missing network typedefs as needed
 */
#ifdef need_u_char
typedef unsigned char u_char;
#endif
#ifdef need_u_short
typedef unsigned short u_short;
#endif
#ifdef need_u_long
#ifdef USE_64
typedef unsigned int u_long;
#else
typedef unsigned long u_long;
#endif
#endif
#ifdef need_void
typedef char void;
#endif

/*
 * "libannex" functions
 */
#ifdef need_htonl
u_long htonl();
#endif

#ifdef need_ntohl
u_long ntohl();
#endif

#ifdef need_htons
u_short htons();
#endif

#ifdef need_ntohs
u_short ntohs();
#endif

#ifdef need_inet_ntoa
char *inet_ntoa();
#endif

#ifdef need_bcopy
char *bcopy();
#endif

#ifdef need_bzero
char *bzero();
#endif

#ifdef need_sigmask
#undef sigmask
#define sigmask(signum)
#endif

#ifdef need_hostent
/*
 * Substitute for missing <netdb.h>
 *
 * Structures returned by network
 * data base library.  All addresses
 * are supplied in host order, and
 * returned in network order (suitable
 * for use in system calls).
 */
struct	hostent {
	char	*h_name;	/* official name of host */
	char	**h_aliases;	/* alias list */
	int	h_addrtype;	/* host address type */
	int	h_length;	/* length of address */
	char	*h_addr;	/* address */
};
#endif

#if defined(need_gethostbyname)
struct hostent	*gethostbyname();
#endif

#ifdef need_servent
/*
 * Substitute for missing <netdb.h>
 *
 * Structures for Internet port service database
 */
struct servent {
	char	*s_name;	/* official service name */
	char	**s_aliases;	/* alias list (unused) */
	int	s_port;		/* port # */
	char	*s_proto;	/* protocol to use */
};
#endif

#if defined(need_getservbyname)
struct servent	*getservbyname();
#endif

#if (defined(need_sendmsg) || defined(need_recvmsg) || defined(need_sockdefs))      && !defined(have_msghdr)
/*
 * Substitute for missing or munged <sys/socket.h>
 *
 * message header structure used by system calls sendmsg(), recvmsg()
 */

struct msghdr {
	char		*msg_name;
	int		msg_namelen;
	struct iovec	*msg_iov;
	int		msg_iovlen;
	char		*msg_accrights;
	int		msg_accrightslen;
};
#endif

#if defined(need_sockdefs) && !defined(have_sockdefs)
/* socket.h */

#define	SOCK_DGRAM	2
#define	AF_INET		2

struct sockaddr {
    unsigned short sa_family;
    char sa_data[14];
    };

#define	PF_INET		AF_INET

#endif /* need_sockdefs */

/*
 * other function mappings as needed
 */
#ifdef need_index
#define index(x,y) strchr(x,y)
#else
extern char *index();
#endif
#ifdef need_rindex
#define rindex(x,y) strrchr(x,y)
#else
extern char *rindex();
#endif
