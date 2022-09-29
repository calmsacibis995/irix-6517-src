#ifndef IP_H
#define IP_H
/*
 * Copyright 1990 Silicon Graphics, Inc.  All rights reserved.
 *
 * Internet Protocol utility routines and data.
 */
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>	/* prerequisite of <netinet/ip.h> */
#undef	IP_MSS			/* kill conditional define in tcp.h */
#include <netinet/ip.h>
#include "protostack.h"

struct datastream;

#define	IP_HDRGRAIN		4
#define	IP_HDRLEN(grains)	((grains) * IP_HDRGRAIN)
#define	IP_MINHDRLEN		IP_HDRLEN(5)
#define	IP_MAXHDRLEN		IP_HDRLEN(15)
#define	IP_MAXOPTLEN		(IP_MAXHDRLEN - IP_MINHDRLEN)

/*
 * Selected identifier codes for UDP/TCP and IP address/port fields.
 * Don't change them without changing the layout of descriptor tables
 * in lib/protocols/{ip,ip_tcp,ip_udp}.c.
 */
#define	IPFID_SPORT	0
#define	IPFID_DPORT	1
#define	IPFID_SRC	9
#define	IPFID_DST	10

/*
 * IP protocol options
 */
enum ip_propt {
	IP_PROPT_ETHERUPDATE, IP_PROPT_HOSTBYNAME, IP_PROPT_HOSTRESORDER
};

/*
 * UDP protocol options
 */
enum udp_propt { UDP_PROPT_SETPORT, UDP_PROPT_BCKSUM };

/*
 * TCP protocol options
 */
enum tcp_propt { TCP_PROPT_SEQCOMMA, TCP_PROPT_SETPORT, TCP_PROPT_ZEROSEQ };

/*
 * Key associated with protocol encapsulating an IP fragment.
 */
struct ipfragkey {
	u_short		ipk_id;		/* identification */
	u_short		ipk_prototype;	/* IP protocol number */
	struct in_addr	ipk_src;	/* originating host */
	struct in_addr	ipk_dst;	/* destination host */
};

/*
 * IP protocol stack frame data.
 */
struct ipframe {
	ProtoStackFrame		ipf_frame;	/* base class state */
	u_int			ipf_hdrlen;	/* IP header length in bytes */
	u_int			ipf_len;	/* length of IP fragment */
	u_int			ipf_fragoffset;	/* fragment byte offset */
	u_int			ipf_morefrags;	/* more fragments flag */
	struct ipfragkey	ipf_fragkey;	/* fragment buffer key */
	struct in_addr		ipf_rdst;	/* route destination */
	struct protocol		*ipf_proto;	/* nested protocol interface */
	u_short			ipf_sport;	/* source port */
	u_short			ipf_dport;	/* destination port */
};

#define	ipf_id		ipf_fragkey.ipk_id
#define	ipf_prototype	ipf_fragkey.ipk_prototype
#define	ipf_src		ipf_fragkey.ipk_src
#define	ipf_dst		ipf_fragkey.ipk_dst

/*
 * IP directed and recorded routing option handling uses this struct as
 * the type of a decoded copy of the route option.  It is not a template
 * for the data structure on the wire.
 */
struct ip_route {
	struct ip_opt {			/* common header for long options */
		u_char	ipo_opt;	/* type code value */
		u_char	ipo_len;	/* length in bytes */
		u_char	ipo_off;	/* byte number of next free addr */
		u_char	ipo_cnt;	/* number of addrs (NOT DECODED) */
	} ipr_hdr;
	struct in_addr	ipr_addr[1];	/* recorded route addresses */
};
#define	ipr_opt		ipr_hdr.ipo_opt
#define	ipr_len		ipr_hdr.ipo_len
#define	ipr_off		ipr_hdr.ipo_off
#define	ipr_cnt		ipr_hdr.ipo_cnt

struct tcp_opt {			/* tcp option template */
	u_char	to_opt;			/* EOL, NOP, or MAXSEG */
	u_char	to_len;			/* len (4) if MAXSEG */
	union {
		u_short	tou_maxseg;	/* maximum segment size */
		u_char	tou_winscale;	/* window scale */
		struct {
			u_char	tout_tsval[4];	/* timestamp value */
			u_char	tout_tsecr[4];	/* timestamp echo reply */
		} tou_tout;
	} to_tou;
};
#define to_maxseg	to_tou.tou_maxseg
#define to_winscale	to_tou.tou_winscale
#define to_tsval	to_tou.tou_tout.tout_tsval
#define to_tsecr	to_tou.tou_tout.tout_tsecr

/*
 * Get or put an IP header given a datastream.
 */
int	ds_ip(struct datastream *, struct ip *);

/*
 * Get or put an IP option.  The ip_opt pointer should point to at least
 * ipo_len bytes of store (up to IP_MAXOPTLEN).
 */
int	ds_ip_opt(struct datastream *, struct ip_opt *);

/*
 * Datastream filter macros for various typedefs.
 */
#define	ds_in_addr(ds, addr)	ds_u_long(ds, &(addr)->s_addr)
#define	ds_n_long(ds, nl)	ds_u_long(ds, nl)
#define	ds_n_time(ds, nt)	ds_u_long(ds, nt)
#define	ds_tcp_seq(ds, seq)	ds_u_long(ds, seq)

/*
 * Compute an IP checksum given arbitrary data or a complete IP frame.
 * Return false if the char pointer is not even, or if the length (int)
 * argument is not equal to the number of byte in the IP fragment that
 * ipframe describes.  Return true otherwise, storing the checksum via
 * the final argument.
 *
 * NB: We assume packet data is zero-padded to an even byte boundary.
 */
int	ip_checksum(char *, int, u_short *);
int	ip_checksum_frame(struct ipframe *, char *, int, u_short *);
int	ip_checksum_pseudohdr(struct ipframe *, char *, int, u_short *);

/*
 * Given a 32-bit accumulator containing the sum of several 16-bit words'
 * worth of data, compute the IP checksum.
 */
#define	IP_FOLD_CHECKSUM_CARRY(ac) \
	(ac = (ac & 0xffff) + (ac >> 16), (u_short) ~(ac + (ac >> 16)))

/*
 * Call ip_hostname to translate an IP address in host or net byte order
 * into a hostname or dot-notation string representation.  Call ip_hostaddr
 * to go from a hostname or dot-string to IP address.
 */
enum iporder { IP_NET, IP_HOST };

char	*ip_hostname(struct in_addr, enum iporder);
int	ip_hostaddr(char *, enum iporder, struct in_addr *);

/*
 * This function adds a new host/address association to the cache of IP
 * hostnames and addresses.  It returns a pointer to the named symbol in
 * the IP hostname symbol table.
 */
struct symbol	*ip_addhost(char *, struct in_addr);

/*
 * Map an IP port number and protocol name to a string in static store.
 * The string contains the port's numeric representation and possibly the
 * parenthesized name of the port's service.
 *
 * Notes:
 *	Port must be in host order.
 *	Returns a pointer to static store.
 */
char	*ip_service(u_short, char *, struct protocol *);

/*
 * Find a name associated with an IP protocol number and return a pointer
 * to its static store.  Otherwise return a numeric string.
 */
char	*ip_protoname(int);

/*
 * Translate a dot-notation string into an unsigned long IP address in
 * host order.  Return updated string pointer or 0 on error.
 */
char	*ip_addr(char *, u_long *);

/*
 * Return a string representation of the given IP timestamp.
 */
char	*ip_timestamp(n_long);

/*
 * Index operations for hashing and comparing IP addresses.
 */
u_int	ip_hashaddr(char *);
int	ip_cmpaddrs(char *, char *);

/*
 * Utility used by TCP and UDP to crack their "setport" option's value
 * string and associate the named protocol with a port, possibly only on
 * a certain host.
 */
int	ip_setport(char *, void (*)(struct protocol *, long, long));

/*
 * IP family protocol names ("ip", "tcp", and "udp").
 */
extern char	ipname[];
extern char	tcpname[];
extern char	udpname[];

#endif
