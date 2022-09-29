#ifndef __SYS_TCPIPSTATS_H__
#define __SYS_TCPIPSTATS_H__
#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ident	"$Revision: 1.17 $"
/*
 * System wide TCP/IP network statistics.
 *
 * These include those for IP, UDP, TCP and IGMP Multicast.
 * The various structures were copied from there respective header files
 * to allow them to be easily incorporated into the system wide statistics
 * maintained for each cpu. This allows us to avoid having to acquire a
 * spin lock on MP machines just to update some statistic.
 */

/*
 * IPv4 specific statistics
 */
struct	ipstat {			/* IP statistics */
	__uint64_t	ips_total;	/* total packets received */
	__uint64_t	ips_badsum;	/* checksum bad */
	__uint64_t	ips_tooshort;	/* packet too short */
	__uint64_t	ips_toosmall;	/* not enough data */
	__uint64_t	ips_badhlen;	/* ip header length < data size */
	__uint64_t	ips_badlen;	/* ip length < ip header length */
	__uint64_t	ips_fragments;	/* fragments received */
	__uint64_t	ips_fragdropped; /*frags dropped (dups, out of space)*/
	__uint64_t	ips_fragtimeout; /* fragments timed out */
	__uint64_t	ips_forward;	/* packets forwarded */
	__uint64_t	ips_cantforward; /* packets rcvd for unreachable dst */
	__uint64_t	ips_redirectsent; /* packets forwarded on same net */
	__uint64_t	ips_noproto;	/* unknown or unsupported protocol */
	__uint64_t	ips_delivered;	/* datagrams delivered to upper level*/
	__uint64_t	ips_localout;	/* total ip packets generated here */
	__uint64_t	ips_odropped;	/* lost packets due to nobufs, etc. */
	__uint64_t	ips_reassembled; /* total packets reassembled ok */
	__uint64_t	ips_fragmented;	/* datagrams sucessfully fragmented */
	__uint64_t	ips_ofragments;	/* output fragments created */
	__uint64_t	ips_cantfrag;	/* don't fragment flag was set, etc. */
	__uint64_t	ips_badoptions;	/* error in option processing */
	__uint64_t	ips_noroute;	/* packtes discarded due to no route */
	__uint64_t	ips_badvers;	/* ip version != 4 */
	__uint64_t	ips_rawout;	/* total raw ip packets generated */
};

/*
 * IPv6 specific statistics
 */
struct	ip6stat {
	__uint64_t	ip6s_total;	/* total packets received */
	__uint64_t	ip6s_tooshort;	/* packet too short */
	__uint64_t	ip6s_toosmall;	/* not enough data */
	__uint64_t	ip6s_inomem;	/* no more memory on input */
	__uint64_t	ip6s_fragments;	/* fragments received */
	__uint64_t	ip6s_fragdropped;/* fragments dropped */
	__uint64_t	ip6s_fragtimeout;/* fragments timed out */
	__uint64_t	ip6s_forward;	/* packets forwarded */
	__uint64_t	ip6s_cantforward;/* packets rcvd for unreachable dst */
	__uint64_t	ip6s_badsource;	/* packets rcvd from bad sources */
	__uint64_t	ip6s_noproto;	/* unknown or unsupported protocol */
	__uint64_t	ip6s_delivered;	/* datagrams delivered to upper level*/
	__uint64_t	ip6s_inhist[256];/* input histogram */
	__uint64_t	ip6s_localout;	/* total ipv6 packets generated here */
	__uint64_t	ip6s_onomem;	/* no more memory on output */
	__uint64_t	ip6s_odropped;	/* lost packets due to nobufs, etc. */
	__uint64_t	ip6s_reassembled;/* total packets reassembled ok */
	__uint64_t	ip6s_fragmented;/* datagrams sucessfully fragmented */
	__uint64_t	ip6s_ofragments;/* output fragments created */
	__uint64_t	ip6s_noroute;	/* packets discarded due to no route */
	__uint64_t	ip6s_badvers;	/* ipv6 version != 6 */
	__uint64_t	ip6s_rawout;	/* total raw ipv6 packets generated */
	__uint64_t	ip6s_toobig;	/* not forwarded bnecause size > MTU */
};

/*
 * UDP statistics
 */
struct	udpstat {
	/* input statistics */
	__uint64_t	udps_ipackets;	/* total input packets */
	__uint64_t	udps_hdrops;	/* packet shorter than header */
	__uint64_t	udps_badsum;	/* checksum error */
	__uint64_t	udps_badlen;	/* data length larger than pkt */
	__uint64_t	udps_noport;	/* no socket on port */
	__uint64_t	udps_noportbcast; /* of above, arrived as bcast */
	__uint64_t	udps_fullsock;	/* not delivered, input sock full */

	/* output statistics */
	__uint64_t	udps_opackets;	/* total output packets */
};

/*
 * TCP statistics.
 * Many of these should be kept per connection, but it's
 * inconvenient at the moment.
 */
struct	tcpstat {			/* TCP statistics */
	__uint64_t	tcps_connattempt;	/* connections initiated */
	__uint64_t	tcps_accepts;		/* connections accepted */
	__uint64_t	tcps_connects;		/* connections established */
	__uint64_t	tcps_drops;		/* connections dropped */
	__uint64_t	tcps_conndrops;	  /* embryonic connections dropped */
	__uint64_t	tcps_closed;	  /* conn. closed (includes drops) */
	__uint64_t	tcps_segstimed;	  /* segs where we tried to get rtt */
	__uint64_t	tcps_rttupdated;	/* times we succeeded */
	__uint64_t	tcps_delack;		/* delayed acks sent */
	__uint64_t	tcps_timeoutdrop;	/* conn dropped in rxmt timeo*/
	__uint64_t	tcps_rexmttimeo;	/* retransmit timeouts */
	__uint64_t	tcps_persisttimeo;	/* persist timeouts */
	__uint64_t	tcps_keeptimeo;		/* keepalive timeouts */
	__uint64_t	tcps_keepprobe;		/* keepalive probes sent */
	__uint64_t	tcps_keepdrops;	/* connections dropped in keepalive */

	__uint64_t	tcps_sndtotal;		/* total packets sent */
	__uint64_t	tcps_sndpack;		/* data packets sent */
	__uint64_t	tcps_sndbyte;		/* data bytes sent */
	__uint64_t	tcps_sndrexmitpack;	/* data packets retransmitted*/
	__uint64_t	tcps_sndrexmitbyte;	/* data bytes retransmitted */
	__uint64_t	tcps_sndacks;		/* ack-only packets sent */
	__uint64_t	tcps_sndprobe;		/* window probes sent */
	__uint64_t	tcps_sndurg;		/* packets sent with URG only*/
	__uint64_t	tcps_sndwinup;	/* window update-only packets sent */
	__uint64_t	tcps_sndctrl;	/* control (SYN|FIN|RST) packets sent*/
	__uint64_t	tcps_sndrst;		/* packets with RST sent */

	__uint64_t	tcps_rcvtotal;		/* total packets received */
	__uint64_t	tcps_rcvpack;		/* packets rcvd in sequence*/
	__uint64_t	tcps_rcvbyte;		/* bytes rcvd in sequence */
	__uint64_t	tcps_rcvbadsum;		/* pkts rcvd with ccksum errs*/
	__uint64_t	tcps_rcvbadoff;		/* pkts rcvd with bad offset */
	__uint64_t	tcps_rcvshort;		/* packets received too short*/
	__uint64_t	tcps_rcvduppack; 	/* duplicate-only pkts rcvd */
	__uint64_t	tcps_rcvdupbyte; 	/* duplicate-only bytes rcvd */
	__uint64_t	tcps_rcvpartduppack;	/* packets with some dup data*/
	__uint64_t	tcps_rcvpartdupbyte;	/* dup bytes in part-dup pkts*/
	__uint64_t	tcps_rcvoopack;	 	/* out-of-order packets rcvd */
	__uint64_t	tcps_rcvoobyte;		/* out-of-order bytes rcvd */
	__uint64_t	tcps_rcvpackafterwin;	/* pkts with data after wind */
	__uint64_t	tcps_rcvbyteafterwin;	/* bytes rcvd after window */
	__uint64_t	tcps_rcvafterclose;	/* packets rcvd after "close"*/
	__uint64_t	tcps_rcvwinprobe;	/* rcvd window probe packets */
	__uint64_t	tcps_rcvdupack;		/* rcvd duplicate acks */
	__uint64_t	tcps_rcvacktoomuch;	/* rcvd acks for unsent data */
	__uint64_t	tcps_rcvackpack;	/* rcvd ack packets */
	__uint64_t	tcps_rcvackbyte;	/* bytes acked by rcvd acks */
	__uint64_t	tcps_rcvwinupd;		/* rcvd window update packets*/
	__uint64_t	tcps_predack;		/* # hdr predict ok for acks */
	__uint64_t	tcps_preddat;		/* # hdr predict ok for data */
	__uint64_t	tcps_pawsdrop;		/* segs dropped due to PAWS */
	__uint64_t	tcps_persistdrop;	/* conns drop due to persist */
	__uint64_t	tcps_listendrop;	/* congestion at port */
	__uint64_t	tcps_badsyn;		/* initial SYN-ACK */
	__uint64_t	tcps_synpurge;	/* half-opens killed by SYN overslow */
};

/*
 * ICMP - Internet Control Message Protocol statistics
 *
 * Also defined in ip_icmp.h but duplicated here to avoid massive header
 * file includes to get all the rest required symbols and constants.
 */
#define	ICMP_MAXTYPE		18

struct icmpstat {
	__uint64_t	icps_dropped;	/* packets deliberately dropped */

		/* ICMP statistics related to packets generated */
	__uint64_t	icps_error;	/* # of calls to icmp_error */
	__uint64_t	icps_oldshort;	/* no error 'cuz old ip too short */
	__uint64_t	icps_oldicmp;	/* no error 'cuz old was icmp */
	__uint64_t	icps_outhist[ICMP_MAXTYPE + 1];

		/* statistics related to input messages processed */
 	__uint64_t	icps_badcode;	/* icmp_code out of range */
	__uint64_t	icps_tooshort;	/* packet < ICMP_MINLEN */
	__uint64_t	icps_checksum;	/* bad checksum */
	__uint64_t	icps_badlen;	/* calculated bound mismatch */
	__uint64_t	icps_reflect;	/* number of responses */
	__uint64_t	icps_inhist[ICMP_MAXTYPE + 1];
};

/*
 * Statistics related to implementation of the IPv6 control message protocol
 * and IPV6 icmp packets generated
 */
struct	icmp6stat {
	__uint64_t	icp6s_error;		/* # of calls to icmp6_error */
	__uint64_t	icp6s_ratelim;		/* # beyond error rate limit */
	__uint64_t	icp6s_oldicmp;		/* no error 'cuz old was icmp*/
	__uint64_t	icp6s_snd_unreach;	/* # of sent unreachables */
	__uint64_t	icp6s_snd_pkttoobig;	/* # of sent packet too big */
	__uint64_t	icp6s_snd_timxceed;	/* # of sent time exceeded */
	__uint64_t	icp6s_snd_paramprob; /* # of sent parameter problems */
	__uint64_t	icp6s_snd_redirect;	/* # of sent redirects */

	__uint64_t	icp6s_snd_echoreq;	/* # of sent echo requests */
	__uint64_t	icp6s_snd_echorep;	/* # of sent echo replies */

	__uint64_t	icp6s_snd_grpqry;	/* # of sent group queries */
	__uint64_t	icp6s_snd_grprep;	/* # of sent group reports */
	__uint64_t	icp6s_snd_grpterm; /* # of sent group terminations */

	__uint64_t	icp6s_snd_rtsol; /* # of sent router solicitations */
	__uint64_t	icp6s_snd_rtadv; /* # of sent router advertisements */
	__uint64_t	icp6s_snd_ndsol; /* # of sent neighbor solicitations */
	__uint64_t	icp6s_snd_ndadv; /* # of sent neighbor advertisements*/

 	__uint64_t	icp6s_badcode;		/* icmp6_code out of range */
	__uint64_t	icp6s_tooshort;		/* packet < ICMP6_MINLEN */
	__uint64_t	icp6s_checksum;		/* bad checksum */
	__uint64_t	icp6s_badlen;		/* calculated bound mismatch */
	__uint64_t	icp6s_reflect;		/* number of responses */

	__uint64_t	icp6s_rcv_unreach;	/* # of rcvd unreachables */
	__uint64_t	icp6s_rcv_pkttoobig;	/* # of rcvd packet too big */
	__uint64_t	icp6s_rcv_timxceed;	/* # of rcvd time exceeded */
	__uint64_t	icp6s_rcv_paramprob;	/* # of rcvd parameter probs */

	__uint64_t	icp6s_rcv_echoreq;	/* # of rcvd echo requests */
	__uint64_t	icp6s_rcv_echorep;	/* # of rcvd echo replies */

	__uint64_t	icp6s_rcv_grpqry;	/* # of rcvd group queries */
	__uint64_t	icp6s_rcv_grprep;	/* # of rcvd group reports */
	__uint64_t	icp6s_rcv_grpterm;	/* # of rcvd group terminate */

	__uint64_t	icp6s_rcv_bad_grpqry;	/* # of rcvd bad grp queries */
	__uint64_t	icp6s_rcv_bad_grprep;	/* # of rcvd bad grp reports */
	__uint64_t	icp6s_rcv_our_grprep;	/* # of rcvd our grps reports*/
	__uint64_t	icp6s_rcv_bad_grpterm;	/* # of rcvd bad grp term */

	__uint64_t	icp6s_rcv_rtsol;	/* # of rcvd router Sol */
	__uint64_t	icp6s_rcv_rtadv;	/* # of rcvd router Adv */
	__uint64_t	icp6s_rcv_ndsol;	/* # of rcvd neighbor Sol */
	__uint64_t	icp6s_rcv_ndadv;	/* # of rcvd neighbor Adv */

	__uint64_t	icp6s_rcv_redirect;	/* # of rcvd redirects */
	__uint64_t	icp6s_rcv_badrtsol;	/* # of rcvd bad router Sol */
	__uint64_t	icp6s_rcv_badrtadv;	/* # of rcvd bad router Adv */
	__uint64_t	icp6s_rcv_badndsol;	/* # of rcvd bad neighbor Sol*/
	__uint64_t	icp6s_rcv_badndadv;	/* # of rcvd bad neighbor Adv*/
	__uint64_t	icp6s_rcv_badredirect;	/* # of rcvd bad redirects */
};

/*
 * Raw IP statistics.
 */
struct  rip6stat {
        __uint64_t      rip6_rcv_sckbuf;        /* # fails to add pkt to sock */
        __uint64_t      rip6_pkts_in;           /* # pkts received */
        __uint64_t      rip6_pkts_out;          /* # pkts sent */
        __uint64_t      rip6_pkts_tosock;       /* # pkts sent to sockets */
        __uint64_t      rip6_inp_slowpath;      /* # >16 pkt copies to socks */
        __uint64_t      rip6_badchecksum;       /* # input cksum failures */
};

/*
 * IGMP statistics
 */
struct igmpstat {
	__uint64_t	igps_rcv_total;		/* total IGMP messages rcvd */
	__uint64_t	igps_rcv_tooshort;	/* rcvd with too few bytes */
	__uint64_t	igps_rcv_badsum;	/* rcvd with bad checksum */
	__uint64_t	igps_rcv_queries;	/* rcvd membership queries */
	__uint64_t	igps_rcv_badqueries;	/* rcvd invalid queries */
	__uint64_t	igps_rcv_reports;	/* rcvd membership reports */
	__uint64_t	igps_rcv_badreports;	/* rcvd invalid reports */
	__uint64_t	igps_rcv_ourreports;	/* rcvd reports for our grp*/
	__uint64_t	igps_snd_reports;	/* sent membership reports */
};

#define MT_MAX		21	/* maximum number of mbuf types from mbuf.h */
/*
 * mbuf statistics
 */
struct mbstat {			/* MBUF related statistics */
	__int64_t	m_mbufs;	/* mbufs obtained from page pool */
	__int64_t	m_clusters;	/* clusters obtained from page pool */
	__int64_t	m_spare;	/* spare field */
	__int64_t	m_clfree;	/* free clusters */
	__int64_t	m_drops;	/* times failed to find space */
	__int64_t	m_wait;		/* times waited for space */
	__int64_t	m_drain;	/* times drained protocols for space */
	__int64_t	m_pcbtot;	/* total PCB */
	__int64_t	m_pcbbytes;	/* PCB bytes */
	__int64_t	m_mcbtot;	/* mcb_get() total */
	__int64_t	m_mcbbytes;	/* mcb_get() bytes */
	__int64_t	m_mcbfail;	/* mcb_get() (and other) failures */
	__int64_t	m_mtypes[MT_MAX]; /* type specific mbuf allocations */
};

/*
 * Per-Protocol inpcb statistics and kernel PCB array counts
 */
#define UDP_PCBSTAT		0
#define TCP_PCBSTAT		1
#define RP6_PCBSTAT		2
#define NUM_PCBSTAT		3

struct inpcbstat {			/* INPCB related statistics */
	__uint64_t	inpcb_notify;	/* Calls to "really" notify */
	__uint64_t	inpcb_lookup;	/* Calls to lookup in the inpcb table*/
	__uint64_t	inpcb_regular;	/* Lookup: regular lookup requested */
	__uint64_t	inpcb_wildcard;	/* Lookup: wildcard lookup requested */

	__uint64_t	inpcb_found_cache; /* Lookup fund in head cache */
	__uint64_t	inpcb_found_chain; /* Lookup found on hash chain */
	__uint64_t	inpcb_searched_all; /* Lookup searched all buckets */
	__uint64_t	inpcb_success;		/* Lookup success */
	__uint64_t	inpcb_failed;	/* Lookup failed to find a match */

	__uint64_t	inpcb_bind;		/* # of bind calls */
	__uint64_t	inpcb_listen;		/* # of listen lookups */
	__uint64_t	inpcb_connect;		/* # of connect calls */
	__uint64_t	inpcb_connectbyaddr;	/* # of connectbyaddr calls */
	__uint64_t	inpcb_setaddr;		/* # of setaddr */
	__uint64_t	inpcb_setaddrx;		/* # of setaddrx */
	__uint64_t	inpcb_curpcbs;		/* current # of pcbs */
	__uint64_t	inpcb_maxpcbs;		/* max # of pcbs */
	__uint64_t	inpcb_maxbucklen;	/* maximum bucket length */
};

#define TCPSTAT_MAX_SOCKTYPES 6	 /* Max socket type value; socket.h + 1 */
#define TCPSTAT_TPISOCKET 7	 /* Number Stream sockets used by XTI/TPI */
#define TCPSTAT_MAX_TCPSTATES 11 /* Max num tcp states, tcp_fsm.h */

#define TCPSTAT_NUM_SOCKTYPES 8	 /* Num slots for various socket types */
#define TCPSTAT_NUM_TCPSTATES 11 /* Num slots for tcp state stats; tcp_fsm.h */

/*
 * General socket statistics; Retrieved via special sysmp syscall
 */
struct sockstat {
    __uint64_t	open_sock[TCPSTAT_NUM_SOCKTYPES]; /* num open sock each type */
    __uint64_t	tcp_sock[TCPSTAT_NUM_TCPSTATES]; /* num tcp soc each state*/
};

/*
 * The kernel's multicast routing statistics.
 */
struct mrtstat {
	__uint64_t	mrts_mfc_lookups; /* # forw cache hash table hits */
	__uint64_t	mrts_mfc_misses;  /* # forw cache hash table miss */
	__uint64_t	mrts_upcalls;	  /* # calls to mrouted */
	__uint64_t	mrts_no_route;	  /* no route for packet's origin */
	__uint64_t	mrts_bad_tunnel;  /* malformed tunnel options */
	__uint64_t	mrts_cant_tunnel; /* no room for tunnel options */
	__uint64_t	mrts_wrong_if;	  /* arrived on wrong interface */
	__uint64_t	mrts_upq_ovflw;	  /* upcall Q overflow */
	__uint64_t	mrts_cache_cleanups; /* # entries with no upcalls */
	__uint64_t 	mrts_drop_sel;     /* pkts dropped selectively */
	__uint64_t 	mrts_q_overflow;    /* pkts dropped - Q overflow */
	__uint64_t 	mrts_pkt2large;     /* pkts dropped;size > BKTSIZE */
	__uint64_t	mrts_upq_sockfull;  /* upcalls dropped; Socket full*/
};

/*
 * Scheduled Transfers (ST) Statistics
 */
struct ststat {
	__uint64_t	stps_connattempt;	/* Conn attempts ie connect()*/
	__uint64_t	stps_accepts;		/* Accepts ie accept() */
	__uint64_t	stps_connects;		/* Connection Established */
	__uint64_t	stps_drops; /* Est conn drops link fail/keepalive to */
	__uint64_t	stps_connfails;		/* No Established Connection */
	__uint64_t	stps_closed;		/* Connection closed */

	__uint64_t	stps_txtotal;		/* num packets xmitted */
	__uint64_t	stps_datatxtotal;	/* num data bytes xmitted */
	__uint64_t	stps_rxtotal;		/* num packets received */
	__uint64_t	stps_datarxtotal;	/* num data bytes received */
	__uint64_t	stps_cksumbad;		/* bad cksum */

	__uint64_t	stps_oototal;		/* out of order stus */
	__uint64_t	stps_keyrejects;	/* key rejections */
	__uint64_t	stps_txrejects;		/* rejections sent */
	__uint64_t	stps_rxrejects;		/* rejections transmitted */
	__uint64_t	stps_slotdrops;		/* pkts dropped => no slots */

	__uint64_t	stps_pad[16];		/* reserved for later */
};

/*
 * Kernel network activity statistics for the TCP/IP protocol family
 */
struct kna {
	struct ipstat		ipstat;
	struct udpstat		udpstat;
	struct tcpstat		tcpstat;
	struct icmpstat		icmpstat;

	struct igmpstat		igmpstat;

	struct mbstat		mbstat;
	struct inpcbstat	pcbstat[NUM_PCBSTAT];
	struct mrtstat		mrtstat;

	struct ip6stat          ip6stat;
	struct icmp6stat        icmp6stat;
	struct rip6stat         rip6stat;
	struct ststat		ststat;
};

#ifdef _KERNEL

#include <sys/pda.h>	/* for private macro */
#include <sys/immu.h>	/* for PDAPAGE, inside private macro */

#define IPSTAT(x)		((struct kna *)(private.knaptr))->ipstat.x++
#define IPSTAT_DEC(x, c)	((struct kna *)(private.knaptr))->ipstat.x -=c

#define IP6STAT(x)		((struct kna *)(private.knaptr))->ip6stat.x++
#define IP6STAT_DEC(x, c)	((struct kna *)(private.knaptr))->ip6stat.x -=c

#define UDPSTAT(x)		((struct kna *)(private.knaptr))->udpstat.x++

#define TCPSTAT(x)		((struct kna *)(private.knaptr))->tcpstat.x++
#define TCPSTAT_ADD(x, c)	((struct kna *)(private.knaptr))->tcpstat.x +=c

#define ICMPSTAT(x)		((struct kna *)(private.knaptr))->icmpstat.x++
#define ICMPSTAT_INHIST(x)	((struct kna *)(private.knaptr))->icmpstat.icps_inhist[x]++
#define ICMPSTAT_OUTHIST(x)	((struct kna *)(private.knaptr))->icmpstat.icps_outhist[x]++

#define ICMP6STAT(x)		((struct kna *)(private.knaptr))->icmp6stat.x++
#define RIP6STAT(x)             ((struct kna *)(private.knaptr))->rip6stat.x++

#define IGMPSTAT(x)		((struct kna *)(private.knaptr))->igmpstat.x++

#define IP_MROUTE_STAT(x)	((struct kna *)(private.knaptr))->mrtstat.x++

#define STSTAT(x)		((struct kna *)(private.knaptr))->ststat.x++
#define STSTAT_ADD(x, c)	((struct kna *)(private.knaptr))->ststat.x +=c

#define PCBSTAT(x,y)	   ((struct kna *)(private.knaptr))->pcbstat[y].x++
#define PCBSTAT_SET(x,y,c) ((struct kna *)(private.knaptr))->pcbstat[y].x = c
#define PCBSTAT_DEC(x,y)   ((struct kna *)(private.knaptr))->pcbstat[y].x--
#define PCBSTAT_ADD(x,y,c) ((struct kna *)(private.knaptr))->pcbstat[y].x += c

#endif /* _KERNEL */

#define _KA_TCB			 0
#define _KA_UDB			 1
#define _KA_RAWCB		 2
#define _KA_KPTBL		 3
#define	_KA_IFNET		 4
#define	_KA_RTREE		 5
#define	_KA_RTSTAT		 6
#define	_KA_UNPCB		 7
#define	_KA_UNIXSW		 8
#define	_KA_KERNEL_MAGIC	 9
#define	_KA_END			10
#define	_KA_V			11
#define	_KA_STR_CURPAGES	12
#define	_KA_STR_MINPAGES	13
#define	_KA_STR_MAXPAGES	14
#define	_KA_STRINFO		15
#define	_KA_MRTPROTO		16
#define _KA_MFCTABLE		17
#define _KA_VIFTABLE		18
#define _KA_PAGECONST		19
#define _KA_MBUFCONST		20
#define _KA_HASHINFO_INADDR	21
#define _KA_HASHTABLE_INADDR	22
#define _KA_MEMBASE		23
#define _KA_PTECONST		24

#define _KA_HASHINFO_IN6ADDR    25
#define _KA_HASHTABLE_IN6ADDR   26
#define	_KA_MAX			27

struct bsd_kernaddrs {
	caddr_t	bk_addr[_KA_MAX];
};

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* __SYS_TCPIPSTATS_H__ */
