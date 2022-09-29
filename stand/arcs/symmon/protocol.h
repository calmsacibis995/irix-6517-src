/*
 * Copyright 1985 by MIPS Computer Systems, Inc.
 */

/*
 * protocol.h -- definitions for serial line protocol
 */

/*
 * Max/min packet size parameters
 */
#define	MAXPACKET	1023
#define	MINPACKET	32
#define	MAXPKTOVERHEAD	10	/* max bytes in pkt header/trailer */

/*
 * Parameters for line quality adaptive transmission routine (proto_pktsize)
 */
#define	MIN_XMIT_PKTS	10	/* min # of pkts before calc new pkt size */
#define	MAX_ACK_THRESH	95	/* % good pkts to trigger size increase */
#define	MIN_ACK_THRESH	85	/* % good pkts to trigger size decrease */

/*
 * Ascii characters that are special to protocol
 */
#define	SYN		0x16
#define	DLE		0x10

/*
 * Packet protocol retransmit time
 * NOTE: if these are too short, you'll see frequent "bad seq" messages
 * do to premature retransmissions.
 */
#ifdef DEBUG
#define	REXMIT_TIME	100000
#else
#define	REXMIT_TIME	3
#endif

/*
 * Packet type definitions
 */
#define	MASK_PKTTYPE	0x20

#define	DATA_PKTTYPE	0
#define	ACK_PKTTYPE	0x20
#define	ANY_PKTTYPE	0x100

extern void proto_enable(ULONG);
extern void init_proto(ULONG);
extern int getpkt(ULONG, char *, unsigned, int *);
extern void putpkt(ULONG fd, char *buf, int cnt, int pkt_type);
extern void proto_disable(ULONG);

#ifdef NETDBX
extern void netinit_proto(ULONG);
extern void netputpkt(ULONG fd, char *buf, int cnt, int pkt_type);
extern int  netgetpkt(ULONG, char *, ULONG, int *);
#endif /* NETDBX */
