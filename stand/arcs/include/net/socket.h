#ident	"include/net/socket.h:  $Revision: 1.4 $"
#ifndef _NET_SOCKET_H
#define _NET_SOCKET_H

/*
 * socket.h -- definitions for prom sockets
 */

#define	NSO_TABLE	5

struct so_table {
	int st_count;			/* reference count */
	u_short st_udpport;		/* port socket is bound to */
	struct mbuf *st_mbuf;		/* packets recv on this port */
};

extern struct so_table *_get_socket(u_short);
extern struct so_table *_find_socket(u_short);
extern struct so_table _so_table[];

/*
 * Structure used by kernel to store most
 * addresses.
 */
struct sockaddr {
	u_short	sa_family;		/* address family */
	char	sa_data[14];		/* up to 14 bytes of direct address */
};

/*
 * Address families.
 */
#define	AF_UNSPEC	0		/* unspecified */
#define	AF_UNIX		1		/* local to host (pipes, portals) */
#define	AF_INET		2		/* internetwork: UDP, TCP, etc. */
#define	AF_IMPLINK	3		/* arpanet imp addresses */
#define	AF_PUP		4		/* pup protocols: e.g. BSP */
#define	AF_CHAOS	5		/* mit CHAOS protocols */
#define	AF_NS		6		/* XEROX NS protocols */
#define	AF_NBS		7		/* nbs protocols */
#define	AF_ECMA		8		/* european computer manufacturers */
#define	AF_DATAKIT	9		/* datakit protocols */
#define	AF_CCITT	10		/* CCITT protocols, X.25 etc */
#define	AF_SNA		11		/* IBM SNA */

#define	AF_MAX		12
#endif /* _NET_SOCKET_H */
