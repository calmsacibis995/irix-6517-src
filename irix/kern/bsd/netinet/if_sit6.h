/*
 * IPv6 over IPv4 tunnel (SIT & CTI) defines
 */

#ifndef _NETINET_IF_SIT6_H_
#define _NETINET_IF_SIT6_H_

/* Maximum packet size */
#define SITMTU		(1500-20)

struct	llinfo_sit {
	int	sit_flags;			/* cache valid ? */
	struct	rtentry *sit_rt;		/* back pointer */
	struct	ip sit_ip;			/* cached IPv4 header */
	struct	route sit_route;		/* cached IPv4 route */
};

struct	sit_softc {
	struct	ifnet sit_if;
	struct {
		struct sit_softc *stqe_next;
	} sit_list;
	struct	llinfo_sit sit_llinfo;
	struct	in6_addr sit_llip6;
#define	sit_src		sit_llinfo.sit_ip.ip_src
#define	sit_dst		sit_llinfo.sit_ip.ip_dst
#define	SIT_NBMA	IFF_LINK0
};

#ifdef _KERNEL

struct sit_head {
        struct sit_softc *stqh_first;/* first element */
	struct sit_softc **stqh_last;/* addr of last next element */
};
extern	struct sit_head sitif;
extern	struct sit_softc *last_sitif;
extern	u_int sitcnt, cticnt;

extern	void sitadd __P((u_int));
extern	void ctiadd __P((u_int));
#endif

#endif
