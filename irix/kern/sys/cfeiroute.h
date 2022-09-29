#ifndef __SYS_CFEIROUTE_H__
#define __SYS_CFEIROUTE_H__

/*
 ******************************************************************************
 *	The software described herein is public domain and may be used,  
 *	copied, or modified by anyone without restriction.
 ******************************************************************************
 */
#ident "$Revision: 1.3 $"

#define	CFEIROUTE

/*
 * HYPERchannel header word control bits
 */
#ifdef  vax
#define H_XTRUNKS       0x00F0  /* transmit trunks */
#define H_RTRUNKS       0x000F  /* remote trunks to transmit on for loopback */
#define H_ASSOC         0x0100  /* has associated data */
#define H_LOOPBK        0x00FF  /* loopback command */
#define H_RLOOPBK       0x008F  /* A710 remote loopback command */
#define H_PADLPBK       0x80ff  /* HYM Param for Adapter Loopback */
#else
/*
 * All other machines have reasonable byte ordering.
 */
#define H_XTRUNKS       0xF000  /* transmit trunks */
#define H_RTRUNKS       0x0F00  /* remote trunks to transmit on for loopback */
#define H_ASSOC         0x0001  /* has associated data */
#define H_LOOPBK        0xFF00  /* loopback command */
#define H_RLOOPBK       0x8F00  /* A710 remote loopback command */
#define H_PADLPBK       0xff80  /* HYM Param for Adapter Loopback */
#endif

#ifdef  CFEIROUTE
/*
 * Routing database
 */
#define HYRSIZE  7		/* max number of adapters in routing tables */

struct hy_route {
	time_t hyr_lasttime;            /* last update time */
	struct hyr_hash {
		u_long  hyr_key;        /* desired address */
		u_short hyr_flags;      /* status flags - see below */
		union {
			/*
			 * direct entry (can get there directly)
			 */
			struct {
                                u_short hyru_mtu;       /* mtu for this host */
				u_short hyru_dst;       /* adapter number & port */
				u_short hyru_ctl;       /* trunks to try */
				u_short hyru_access;    /* access code (mostly unused) */
			} hyr_d;
#define hyr_mtu         hyr_u.hyr_d.hyru_mtu
#define hyr_dst         hyr_u.hyr_d.hyru_dst
#define hyr_ctl         hyr_u.hyr_d.hyru_ctl
#define hyr_access      hyr_u.hyr_d.hyru_access
			/*
			 * indirect entry (one or more hops required)
			 */
			struct {
				u_char hyru_pgate;      /* 1st gateway slot */
				u_char hyru_egate;      /* # gateways */
				u_char hyru_nextgate;   /* gateway to use next */
			} hyr_i;
#define hyr_pgate       hyr_u.hyr_i.hyru_pgate
#define hyr_egate       hyr_u.hyr_i.hyru_egate
#define hyr_nextgate    hyr_u.hyr_i.hyru_nextgate
		} hyr_u;
	} hyr_hash[HYRSIZE];
	u_char hyr_gateway[256];
};

/*
 * routing table set/get structure
 *
 * used to just pass the entire routing table through, but 4.2 ioctls
 * limit the data part of an ioctl to 128 bytes or so and use the
 * interface name to get things sent the right place.
 * see ../net/if.h for additional details.
 */
struct hyrsetget {
	char    hyrsg_name[IFNAMSIZ];   /* if name, e.g. "hy0" */
	struct hy_route *hyrsg_ptr;     /* pointer to routing table */
	unsigned        hyrsg_len;      /* size of routing table provided */
};

#define HYR_INUSE       0x01    /* entry in use */
#define HYR_DIR         0x02    /* direct entry */
#define HYR_GATE        0x04    /* gateway entry */
#define HYR_LOOP        0x08    /* hardware loopback entry */
#define HYR_RLOOP       0x10    /* remote adapter hardware loopback entry */

#define HYRHASH(x) (((x) + ((x) >> 16)) % HYRSIZE)

#define HYSETROUTE      _IOW('i', 0x80, struct hyrsetget)
#define HYGETROUTE      _IOW('i', 0x81, struct hyrsetget)
#endif  /* CFEIROUTE */

#endif /* __SYS_CFEIROUTE_H__ */
