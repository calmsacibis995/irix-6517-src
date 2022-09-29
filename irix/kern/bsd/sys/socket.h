/*
 * Copyright (c) 1982, 1985, 1986, 1988, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#ifndef __SYS_TPI_SOCKET_H__
#ifndef _SYS_SOCKET_H_
#define	_SYS_SOCKET_H_
#ifdef __cplusplus
extern "C" {
#endif

#include <standards.h>
#include <sys/types.h>
#if _SGIAPI
#include <sys/bsd_types.h>
#endif /* _SGIAPI */

/*
 * Definitions related to sockets: types, address families, options.
 */

/*
 * Types
 */

#if _SGIAPI
#ifndef NC_TPI_CLTS
#define NC_TPI_CLTS	1		/* must agree with netconfig.h */
#define NC_TPI_COTS	2		/* must agree with netconfig.h */
#define NC_TPI_COTS_ORD	3		/* must agree with netconfig.h */
#define	NC_TPI_RAW	4		/* must agree with netconfig.h */
#endif /* !NC_TPI_CLTS */
#endif /* _SGIAPI  */

#define _NC_TPI_CLTS		1
#define _NC_TPI_COTS		2
#define _NC_TPI_COTS_ORD	3
#define _NC_TPI_RAW		4

#define	SOCK_RAW	_NC_TPI_RAW	/* raw-protocol interface */
#define	SOCK_RDM	5		/* reliably-delivered message */
#define	SOCK_DGRAM	_NC_TPI_CLTS	/* datagram socket */
#define	SOCK_STREAM	_NC_TPI_COTS	/* stream socket */
#define	SOCK_SEQPACKET	6		/* sequenced packet stream */

/*
 * Option flags per-socket.
 */
#define	SO_DEBUG	0x0001		/* turn on debugging info recording */
#define	SO_ACCEPTCONN	0x0002		/* socket has had listen() */
#define	SO_REUSEADDR	0x0004		/* allow local address reuse */
#define	SO_KEEPALIVE	0x0008		/* keep connections alive */
#define	SO_DONTROUTE	0x0010		/* just use interface addresses */
#define	SO_BROADCAST	0x0020		/* permit sending of broadcast msgs */
#define	SO_USELOOPBACK	0x0040		/* bypass hardware when possible */
#define	SO_LINGER	0x0080		/* linger on close if data present */
#define	SO_OOBINLINE	0x0100		/* leave received OOB data in line */
#define	SO_REUSEPORT	0x0200		/* allow local address,port reuse */
#define SO_ORDREL	0x0200		/* MIPS ABI - unimplemented */
#define SO_IMASOCKET	0x0400		/* use libsocket (not TLI) semantics */
#define	SO_CHAMELEON	0x1000		/* (cipso) set label to 1st req rcvd */
#define SO_PASSIFNAME	0x2000		/* Pass the Ifname in front of data */
#define SO_XOPEN	0x4000		/* XOPEN socket behavior */

#define SHUT_RD			0	/* disable further receive oper's   */
#define SHUT_WR			1	/* disable further send oper's      */
#define SHUT_RDWR		2	/* disable further send/recv oper's */

/*
 * Additional options, not kept in so_options.
 */
#define SO_SNDBUF	0x1001		/* send buffer size */
#define SO_RCVBUF	0x1002		/* receive buffer size */
#define SO_SNDLOWAT	0x1003		/* send low-water mark */
#define SO_RCVLOWAT	0x1004		/* receive low-water mark */
#define SO_SNDTIMEO	0x1005		/* send timeout */
#define SO_RCVTIMEO	0x1006		/* receive timeout */
#define	SO_ERROR	0x1007		/* get error status and clear */
#define	SO_TYPE		0x1008		/* get socket type */
#define SO_PROTOTYPE	0x1009		/* get protocol type (libsocket) */
#define	SO_PROTOCOL	0x100a		/* get protocol (cpr) */
#define	SO_BACKLOG	0x100b		/* get backlog (cpr) */


/*
 * Structure used for manipulating linger option.
 */
struct	linger {
	int	l_onoff;		/* option on/off */
	int	l_linger;		/* linger time */
};

/*
 * Level number for (get/set)sockopt() to apply to socket itself.
 */
#define	SOL_SOCKET	0xffff		/* options for socket level */

/*
 * Address families.
 */
#define	AF_UNSPEC	0		/* unspecified */
#define	AF_LOCAL	1		/* local to host (pipes, portals) */
#define	AF_UNIX		AF_LOCAL	/* backward compatibility */
#define	AF_INET		2		/* internetwork: UDP, TCP, etc. */
#define	AF_IMPLINK	3		/* arpanet imp addresses */
#define	AF_PUP		4		/* pup protocols: e.g. BSP */
#define	AF_CHAOS	5		/* mit CHAOS protocols */
#define	AF_NS		6		/* XEROX NS protocols */
#define	AF_ISO		7		/* ISO protocols */
#define	AF_ECMA		8		/* european computer manufacturers */
#define	AF_DATAKIT	9		/* datakit protocols */
#define	AF_CCITT	10		/* CCITT protocols, X.25 etc */
#define	AF_SNA		11		/* IBM SNA */
#define AF_DECnet	12		/* DECnet */
#define AF_DLI		13		/* DEC Direct data link interface */
#define AF_LAT		14		/* LAT */
#define	AF_HYLINK	15		/* NSC Hyperchannel */
#define	AF_APPLETALK	16		/* Apple Talk */
#define	AF_ROUTE	17		/* Internal Routing Protocol */
#ifdef __sgi
#define	AF_RAW		18		/* Raw link layer interface */
#else
#define	AF_LINK		18		/* Link layer interface */
#endif
#if _SGIAPI
#define	pseudo_AF_XTP	19		/* eXpress Transfer Protocol (no AF) */
#endif /* _SGIAPI */

/* MIPS ABI VALUES - unimplemented - notice these overlap real values */
#define AF_NIT		17		/* Network Interface Tap */
#define AF_802		18		/* IEEE 802.2, also ISO 8802 */
#define AF_OSI		19		/* umbrella for all families used */
#define AF_X25		20		/* CCITT X.25 in particular */
#define AF_OSINET	21		/* AFI = 47, IDI = 4 */
#define AF_GOSIP	22		/* U.S. Government OSI */

#define AF_SDL		23		/* SGI Data Link for DLPI */
#define AF_INET6	24		/* Internet Protocol version 6 */
#define	AF_LINK		25		/* Link layer interface */
#define	AF_ATMARP	26		/* (Cray) ATM signaling daemons */
#define	AF_STP		27		/* Scheduled Transfer protocol */

#define AF_MAX		(AF_STP+1)

/*
 * integral type mandated by X/Open to be both here and netinet/in.h
 */

#if !defined(_SA_FAMILY_T)
#define _SA_FAMILY_T
typedef unsigned short sa_family_t;
#endif

/*
 * Structure used by kernel to store most
 * addresses.
 * NOTE: For Raganarok compilers we MUST have a union for sockaddr which
 * specifies integer data, otherwise the compiler will only enforce
 * half-word alignment and modules using alternate defines (sockaddr_in)
 * will get Address Error exceptions on word accesses.
 */

struct sockaddr {
  union {
    struct {
#undef _HAVE_SA_LEN			/* marker for some places to change */
#ifdef _HAVE_SA_LEN			/* when we have sin_len */
	u_char	sa_len2;		/* total length */
	u_char	sa_family2;		/* address family */
#else
	sa_family_t sa_family2;		/* address family */
#endif
	char	sa_data2[14];		/* up to 14 bytes of direct address */
      } sa_generic;
    int sa_align;
  } sa_union;
};
#define sa_len		sa_union.sa_generic.sa_len2
#define sa_family	sa_union.sa_generic.sa_family2
#define	sa_data		sa_union.sa_generic.sa_data2

#if _SGIAPI
#ifndef _HAVE_SIN_LEN
struct sockaddr_new {
  union {
    struct {
	u_char	sa_len2;		/* total length */
	u_char	sa_family2;		/* address family */
	char	sa_data2[14];		/* up to 14 bytes of direct address */
      } sa_generic;
    int sa_align;
  } sa_union;
};

#define _SIN_ADDR_SIZE		8		/* size of sockaddr_in data */
#define _SIN_SA_DATA_SIZE	14		/* size of sa_data array */

#define _MAX_SA_LEN	20		/* largest sockaddr for now (_dl) */
/* Fake .sa_len for sources or destinations.
 */
#define _FAKE_SA_LEN_SRC(sa) (((struct sockaddr_new*)(sa))->sa_len	\
			      ? ((struct sockaddr_new*)(sa))->sa_len	\
			      : ((sa)->sa_family == AF_INET ? _SIN_ADDR_SIZE \
				 : ((sa)->sa_family == AF_LINK ? _MAX_SA_LEN \
				    : sizeof(struct sockaddr))))
#define _FAKE_SA_LEN_DST(sa) (((struct sockaddr_new*)(sa))->sa_len	\
			      ? ((struct sockaddr_new*)(sa))->sa_len	\
			      : ((sa)->sa_family == AF_LINK ? _MAX_SA_LEN \
				 : sizeof(struct sockaddr)))
#define _FAKE_SA_FAMILY(sa) (((struct sockaddr_new*)(sa))->sa_family)
#endif  /* _HAVE_SIN_LEN  */

#if defined (_KERNEL) /* Trusted IRIX */
struct mac_label;

/*
 * CIPSO: structure passed in MT_SONAME mbuf from sbappendaddr() to recvit()
 */
struct sockaddrlbl {
	struct sockaddr  sal_addr;	/* datagram source address */
	struct mac_label *sal_lbl;	/* label of datagram */
	uid_t sal_uid;			/* uid of datagram */
};
#endif  /*  _KERNEL  */

struct recvluinfo {			/* argument to recvlumsg */
	struct mac_label *rlu_label;
	uid_t		 *rlu_uidp;
};

/*
 * Structure used by kernel to pass protocol
 * information in raw sockets.
 */
struct sockproto {
	u_short	sp_family;		/* address family */
	u_short	sp_protocol;		/* protocol */
};

/*
 * An option specification consists of an opthdr, followed by the value of
 * the option.  An options buffer contains one or more options.  The len
 * field of opthdr specifies the length of the option value in bytes.  This
 * length must be a multiple of sizeof(int) (use OPTLEN macro).
 */

struct opthdr {
	int	       level;  /* protocol level affected */
	int	       name;   /* option to modify */
	int	       len;    /* length of option value */
};

#define OPTLEN(x) ((((x) + sizeof(int) - 1) / sizeof(int)) * sizeof(int))
#define OPTVAL(opt) ((char *)(opt + 1))

/*
 * the optdefault structure is used for internal tables of option default
 * values.
 */
struct optdefault {
	int	optname;	/* the option */
	char	*val;		/* ptr to default value */
	int	len;		/* length of value */
};

struct tpisocket;
struct T_optmgmt_req;
struct msgb;

/*
 * the opproc structure is used to build tables of options processing
 * functions for dooptions().
 */
struct opproc {
	int	level;		/* options level this function handles */
	int	(*func)(struct tpisocket *, struct T_optmgmt_req *,
			struct opthdr *, struct msgb *);
				/* the function */
};

/*
 * This structure is used to encode pseudo system calls
 */
struct socksysreq {
	int		args[7];
};

/*
 * This structure is used for adding new protocols to the list supported by
 * sockets.
 */

struct socknewproto {
	int		family;	/* address family (AF_INET, etc.) */
	int		type;	/* protocol type (SOCK_STREAM, etc.) */
	int		proto;	/* per family proto number */
	dev_t		dev;	/* major/minor to use (must be a clone) */
	int		flags;	/* protosw flags */
};
#endif /*  _SGIAPI  */

/*
 * Protocol families, same as address families for now.
 */
#define	PF_UNSPEC	AF_UNSPEC
#define	PF_LOCAL	AF_LOCAL
#define	PF_UNIX		PF_LOCAL	/* backward compatibility */
#define	PF_INET		AF_INET
#define	PF_IMPLINK	AF_IMPLINK
#define	PF_PUP		AF_PUP
#define	PF_CHAOS	AF_CHAOS
#define	PF_NS		AF_NS
#define	PF_ISO		AF_ISO
#define	PF_OSI		AF_ISO
#define	PF_ECMA		AF_ECMA
#define	PF_DATAKIT	AF_DATAKIT
#define	PF_CCITT	AF_CCITT
#define	PF_SNA		AF_SNA
#define PF_DECnet	AF_DECnet
#define PF_DLI		AF_DLI
#define PF_LAT		AF_LAT
#define	PF_HYLINK	AF_HYLINK
#define	PF_APPLETALK	AF_APPLETALK
#define	PF_ROUTE	AF_ROUTE
#define	PF_LINK		AF_LINK
#define	PF_XTP		pseudo_AF_XTP	/* really just proto family, no AF */
#define	PF_RAW		AF_RAW
#define	PF_STP		AF_STP

/* MIPS ABI VALUES - unimplemented */
#define PF_NIT		AF_NIT		/* Network Interface Tap */
#define PF_802		AF_802		/* IEEE 802.2, also ISO 8802 */
#define PF_X25		AF_X25		/* CCITT X.25 in particular */
#define PF_OSINET	AF_OSINET	/* AFI = 47, IDI = 4 */
#define PF_GOSIP	AF_GOSIP	/* U.S. Government OSI */
#define	PF_INET6	AF_INET6

#define	PF_MAX		AF_MAX

#if _SGIAPI

/*
 * Definitions for network related sysctl, CTL_NET.
 *
 * Second level is protocol family.
 * Third level is protocol number.
 */

/*
 * PF_ROUTE - Routing table
 *
 * Three additional levels are defined:
 *	Fourth: address family, 0 is wildcard
 *	Fifth: type of info, defined below
 *	Sixth: flag(s) to mask with for NET_RT_FLAGS
 */
#define NET_RT_DUMP	1		/* dump; may limit to a.f. */
#define NET_RT_FLAGS	2		/* by flags, e.g. RESOLVING */
#define NET_RT_IFLIST	3		/* survey interface list */

/*
 * Fourth level sysctl() variables for IPPROTO_IP
 */
#define	IPCTL_FORWARDING	1	/* act as router */
#define	IPCTL_SENDREDIRECTS	2	/* may send redirects when forwarding */

/*
 * Fourth level sysctl() variables for IPPROTO_UDP
 */
#define	UDPCTL_CHECKSUM		1	/* checksum UDP packets */

#endif /*  _SGIAPI  */

/*
 * Maximum queue length specifiable by listen.
 * Applications can always reduce this in the listen call.
 */
#define	SOMAXCONN	1000

#if !defined (_IOVEC_T)
#define _IOVEC_T
typedef struct iovec {
	void *iov_base;
	size_t  iov_len;
} iovec_t;
#endif  /* _IOVEC_T */

/*
 * Message header for recvmsg and sendmsg calls.
 * Used value-result for recvmsg, value only for sendmsg.
 */
#if _SGIAPI
struct msghdr {
	caddr_t	msg_name;		/* optional address */
	int	msg_namelen;		/* size of address */
	struct	iovec *msg_iov;		/* scatter/gather array */
	int	msg_iovlen;		/* # elements in msg_iov */
	caddr_t	msg_accrights;		/* access rights sent/received */
	int	msg_accrightslen;
};

/* XXX This will become msghdr in the next release */
struct nmsghdr {
	caddr_t	msg_name;		/* optional address */
	int	msg_namelen;		/* size of address */
	struct	iovec *msg_iov;		/* scatter/gather array */
	int	msg_iovlen;		/* # elements in msg_iov */
	caddr_t	msg_control;		/* ancillary data, see below */
	int	msg_controllen;		/* ancillary data buffer len */
	int	msg_flags;		/* flags on received message */
};

/*
 * XPG4 message header
 */
struct xpg4_msghdr {
	void	*msg_name;		/* optional address */
	size_t	msg_namelen;		/* size of address */
	struct  iovec *msg_iov;		/* scatter/gather array */
	int     msg_iovlen;		/* # elements in msg_iov */
	void	*msg_ctrl;		/* ancillary data */
	size_t	msg_ctrllen;		/* ancillary data buffer len */
	int	msg_flags;		/* flags on rec'd message */
};

/* This goes away in the next release XXX */
#define	msg_control	msg_accrights
#define	msg_controllen	msg_accrightslen

#else
struct msghdr {
	void	*msg_name;		/* optional address */
	size_t	msg_namelen;		/* size of address */
	struct  iovec *msg_iov;		/* scatter/gather array */
	int     msg_iovlen;		/* # elements in msg_iov */
	void	*msg_control;		/* ancillary data */
	size_t	msg_controllen;		/* ancillary data buffer len */
	int	msg_flags;		/* flags on rec'd message */
};
#endif  /* _SGIAPI  */

#if defined(_KERNEL)
/*
 * socket interface version specifiers: xpg4 or BSD 4.3
 */
#define XPG4_SOCKETS    1
#define BSD43_SOCKETS   2

#endif /* _KERNEL */

#define	MSG_OOB		0x1		/* process out-of-band data */
#define	MSG_PEEK	0x2		/* peek at incoming message */
#define	MSG_DONTROUTE	0x4		/* send without using routing tables */
#define	MSG_EOR		0x8		/* data completes record */
#define	MSG_TRUNC	0x10		/* data discarded before delivery */
#define	MSG_CTRUNC	0x20		/* control data lost before delivery */
#define	MSG_WAITALL	0x40		/* wait for full request or error */
#define	MSG_DONTWAIT	0x80		/* this message should be nonblocking */
#ifdef XTP
#define	MSG_BTAG	0x40		/* XTP packet with BTAG field */
#define	MSG_ETAG	0x80		/* XTP packet with ETAG field */
#endif

#if _SGIAPI
/* IRIX-specific MSG_ flags */

#define	_MSG_NOINTR	0x80000000	/* ignore interrupts when blocking */
#endif

#define	MSG_MAXIOVLEN	16

/*
 * Header for ancillary data objects in msg_control buffer.
 * Used for additional information with/about a datagram
 * not expressible by flags.  The format is a sequence
 * of message elements headed by cmsghdr structures.
 */
struct cmsghdr {
	size_t	cmsg_len;	/* data byte count, including hdr */
	int	cmsg_level;	/* originating protocol */
	int	cmsg_type;	/* protocol-specific type */
/* followed by	u_char  cmsg_data[]; */
};

#define	_ALIGNBYTES	7
#define	_ALIGN(p)	(((uint_t)(p) + _ALIGNBYTES) & ~_ALIGNBYTES)

/* given pointer to struct cmsghdr, return pointer to data */
#define	CMSG_DATA(cmsg)		((uchar_t *)((cmsg) + 1))

/* given pointer to struct cmsghdr, return pointer to next cmsghdr */
#define	CMSG_NXTHDR(mhdr, cmsg)	\
	(((caddr_t)(cmsg) + (cmsg)->cmsg_len + sizeof(struct cmsghdr) > \
	    (caddr_t)(mhdr)->msg_control + (mhdr)->msg_controllen) ? \
	    (struct cmsghdr *)NULL : \
	    (struct cmsghdr *)((caddr_t)(cmsg) + _ALIGN((cmsg)->cmsg_len)))

#define	CMSG_FIRSTHDR(mhdr)	((struct cmsghdr *)(mhdr)->msg_control)

#ifdef INET6
/* IPv6 API macros (no alignment constraints) */

#define CMSG_SPACE(length)      (sizeof(struct cmsghdr) + (length))

#define CMSG_LEN(length)	(sizeof(struct cmsghdr) + (length))
#endif

#ifdef _XOPEN_SOURCE
/* "Socket"-level control message types: */
#define	SCM_RIGHTS	0x01		/* access rights (array of int) */
#endif /* _XOPEN_SOURCE */

#if !defined(_KERNEL)
extern int listen(int, int);
extern int shutdown(int, int);
extern int socketpair(int, int, int, int *);
#if _NO_XOPEN4
/* Std BSD4.3/SGI */
extern int recvmsg(int, struct msghdr *, int);
extern int sendmsg(int, const struct msghdr *, int);
extern int accept(int, void *, int *);
extern int bind(int, const void *, int);
extern int connect(int, const void *, int);
extern int getpeername(int, void *, int *);
extern int getsockname(int, void *, int *);
extern int getsockopt(int, int, int, void *, int *);
extern int recv(int, void *, int, int);
extern int recvfrom(int, void *, int, int, void *, int *);
extern int send(int, const void *, int, int);
extern int sendto(int, const void *, int, int, const void *, int);
extern int setsockopt(int, int, int, const void *, int);
extern int socket(int, int, int);
#else
#if _ABIAPI
/* BB3.0 is XPG compliant but can't use hidden names */
extern ssize_t recvmsg(int, struct msghdr *, int);
extern ssize_t sendmsg(int, const struct msghdr *, int);
extern int accept(int, struct sockaddr *, size_t *);
extern int bind(int, const struct sockaddr *, size_t);
extern int connect(int, const struct sockaddr *, size_t);
extern int getpeername(int, struct sockaddr *, size_t *);
extern int getsockname(int, struct sockaddr *, size_t *);
extern int getsockopt(int, int, int, void *, size_t *);
extern ssize_t recv(int, void *, size_t, int);
extern ssize_t recvfrom(int, void *, size_t, int, struct sockaddr *,
	size_t*);
extern ssize_t send(int, const void *, size_t, int);
extern ssize_t sendto(int, const void *, size_t, int,
	const struct sockaddr *, size_t);
extern int setsockopt(int, int, int, const void *, size_t );
extern int socket(int, int, int);
#else
/* this is normal XPG */
extern ssize_t __xpg4_recvmsg(int, struct msghdr *, int);
extern ssize_t __xpg4_sendmsg(int, const struct msghdr *, int);
extern int __xpg4_accept(int, struct sockaddr *, size_t *);
extern int __xpg4_bind(int, const struct sockaddr *, size_t);
extern int __xpg4_connect(int, const struct sockaddr *, size_t);
extern int __xpg4_getpeername(int, struct sockaddr *, size_t *);
extern int __xpg4_getsockname(int, struct sockaddr *, size_t *);
extern int __xpg4_getsockopt(int, int, int, void *, size_t *);
extern ssize_t __xpg4_recv(int, void *, size_t, int);
extern ssize_t __xpg4_recvfrom(int, void *, size_t, int, struct sockaddr *,
	size_t*);
extern ssize_t __xpg4_send(int, const void *, size_t, int);
extern ssize_t __xpg4_sendto(int, const void *, size_t, int,
	const struct sockaddr *, size_t);
extern int __xpg4_setsockopt(int, int, int, const void *, size_t );
extern int __xpg4_socket(int, int, int);

/*REFERENCED*/ static ssize_t
recvmsg(int s, struct msghdr *msg, int flags)
{
	return(__xpg4_recvmsg(s, msg, flags));
}

/*REFERENCED*/ static ssize_t
sendmsg(int s, const struct msghdr *msg, int flags)
{
	return(__xpg4_sendmsg(s, msg, flags));
}

/*REFERENCED*/ static int
accept(int s, struct sockaddr *sap, size_t *lenp)
{
	return(__xpg4_accept(s, sap, lenp));
}

/*REFERENCED*/ static int
bind(int s, const struct sockaddr *name, size_t namelen)
{
	return(__xpg4_bind(s, name, namelen));
}

/*REFERENCED*/ static int
connect(int s, const struct sockaddr *name, size_t namelen)
{
	return(__xpg4_connect(s, name, namelen));
}

/*REFERENCED*/ static ssize_t
recv(int s, void *buf, size_t len, int flags)
{
	return(__xpg4_recv(s, buf, len, flags));
}

/*REFERENCED*/ static int
recvfrom(int s, void *buf, size_t len, int flags, struct sockaddr *addr,
	size_t *addrlen)
{
	return(__xpg4_recvfrom(s, buf, len, flags, addr, addrlen));
}

/*REFERENCED*/ static int
getpeername(int s, struct sockaddr *addr, size_t *len)
{
	return(__xpg4_getpeername(s, addr, len));
}

/*REFERENCED*/ static int
getsockname(int s, struct sockaddr *addr, size_t *len)
{
	return(__xpg4_getsockname(s, addr, len));
}

/*REFERENCED*/ static int
getsockopt(int s, int level, int optname, void *optval, size_t *optlen)
{
	return(__xpg4_getsockopt(s, level, optname, optval, optlen));
}

/*REFERENCED*/ static ssize_t
send(int s, const void *buf, size_t len, int flags)
{
	return(__xpg4_send(s, buf, len, flags));
}

/*REFERENCED*/ static ssize_t
sendto(int s, const void *buf, size_t len, int flags,
	const struct sockaddr *addr, size_t addrlen)
{
	return(__xpg4_sendto(s, buf, len, flags, addr, addrlen));
}

/*REFERENCED*/ static int
setsockopt(int s, int level, int name, const void *optval, size_t optlen)
{
	return(__xpg4_setsockopt(s, level, name, optval, optlen));
}

/*REFERENCED*/ static int
socket(int domain, int type, int protocol)
{
	return(__xpg4_socket(domain, type, protocol));
}

#endif /* _ABIAPI */
#endif /* __NO_XOPEN4 */
#endif  /* _KERNEL  */

#ifdef __cplusplus
}
#endif
#endif /* !__SYS_SOCKET_H__ */
#endif /* !__SYS_TPI_SOCKET_H__ */
