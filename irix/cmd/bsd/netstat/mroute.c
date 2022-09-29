/*
 * Print multicast forwarding structures and statistics.
 *
 * From Xerox
 */

#define _KMEMUSER
#include <stdio.h>
#include <sys/param.h>
#include <sys/mbuf.h>
#include <sys/sysmp.h>
#include <sys/tcpipstats.h>

#include <net/if.h>
#include <netinet/in.h>
#include <netinet/igmp.h>
#include <net/route.h>
#include <netinet/ip_mroute.h>
#include <unistd.h>
#include <net/soioctl.h>

#include "netstat.h"

static char *
pktscale(n)
	u_int n;
{
	static char buf[6];
	char t;
	int n2;

	if (n < 2000) {
		t = ' ';
	} else if (n < 1050000) {
		t = 'K';
		n += 500;
		n /= 1000;
	} else if (n < 9950000) {
		/*
		 * Special case 1.1M to 9.9M
		 */
		n += 50000;
		n2 = n / 1000000;
		n -= n2 * 1000000;
		n /= 100000;
		sprintf(buf, "%2u.%01uM", n2, n);
		return buf;
	} else {
		t = 'M';
		n += 500000;
		n /= 1000000;
	}

	sprintf(buf, "%4u%c", n, t);

	return buf;
}

static char mbdata[256];		/* big enough for all mbufs */

/*
 * Print interface "name" given address
 */
static char *
ifname(int32_t a)
{
	static struct ifreq *ifb, *ifend;
	struct ifconf ifc;
	struct ifreq *ifrp;
	int s;

	if (ifb == NULL) {
		/*
		 * Allocate a new interface list buffer on first use,
		 * or whenever we seem to "grow" a new interface.
		 */
		s = socket(AF_INET, SOCK_DGRAM, 0);
		if (s < 0) return ("?");
		ifb = (struct ifreq *)calloc(128, sizeof(struct ifreq));
		if (ifb == NULL) return ("?");
		ifc.ifc_buf = (caddr_t)ifb;
		ifc.ifc_len = 128 * sizeof(struct ifreq);
		(void) ioctl(s, SIOCGIFCONF, (char *)&ifc);
		ifend = (struct ifreq *)((char *)ifb + ifc.ifc_len);
		(void) close(s);
	}
	for (ifrp = ifb; ifrp < ifend; ifrp++) {
		if (ifrp->ifr_addr.sa_family != AF_INET)
		    continue;
		if (((struct sockaddr_in *)&ifrp->ifr_addr)->sin_addr.s_addr == a)
		    return (ifrp->ifr_name);
	}
	free(ifb);
	ifb = NULL;
	return ("?");
}

void
mroutepr(ns_off_t mrpaddr, ns_off_t mrtaddr, ns_off_t vifaddr)
{
	u_int mrtproto;
	struct mbuf *mfctable[MFCTBLSIZ];
	struct vif viftable[MAXVIFS];
	struct mfc *mfc;
	struct mbuf *mp;
	register struct vif *v;
	register vifi_t vifi;
	struct in_addr *grp;
	int i;
	int banner_printed;
	int saved_nflag;

	if (mrpaddr == 0) {
		printf("ip_mrtproto: symbol not in namelist\n");
		return;
	}

	(void) klseek(kmem, mrpaddr, 0);
	(void) read(kmem, (char *)&mrtproto, sizeof(mrtproto));
	switch (mrtproto) {
	    case 0:
		printf("multicast routing daemon not running or system not configured for mcast routing\n");
		return;

	    case IGMP_DVMRP:
		break;

	    default:
		printf("multicast routing protocol %u, unknown\n", mrtproto);
		return;
	}

	if (mrtaddr == 0) {
		printf("mfctable: symbol not in namelist\n");
		return;
	}
	if (vifaddr == 0) {
		printf("viftable: symbol not in namelist\n");
		return;
	}

	saved_nflag = nflag;
	nflag = 1;

	klseek(kmem, vifaddr, 0);
	read(kmem, (char *)viftable, sizeof(viftable));
	banner_printed = 0;
	for (vifi = 0, v = viftable; vifi < MAXVIFS; ++vifi, ++v) {

		if (v->v_lcl_addr.s_addr == 0) continue;

		if (!banner_printed) {
			printf("\nVirtual Interface Table\n%s%s%s",
			       " Vif Thresh  Rate     ",
				   "Local-Address   ",
			       "Remote-Address    Pkts in   Pkts out\n");
			banner_printed = 1;
		}


		printf(" %2u  %3u  %6u     %-15.15s",
			   vifi, v->v_threshold,
			   v->v_rate_limit,
			   routename(v->v_lcl_addr.s_addr,15));
		printf(" %-15.15s  %8s",
			(v->v_flags & VIFF_TUNNEL) ?
				routename(v->v_rmt_addr.s_addr,15) : " ",
		       pktscale(v->v_pkt_in));
		printf(" %8s\n", pktscale(v->v_pkt_out));
	}
	if (!banner_printed) printf("\nVirtual Interface Table is empty\n");

	klseek(kmem, mrtaddr, 0);
	read(kmem, (char *)mfctable, sizeof(mfctable));
	banner_printed = 0;
	for (i = 0; i < MFCTBLSIZ; ++i) {
	    for (mp = mfctable[i]; mp != NULL; mp = ((struct mbuf *)mbdata)->m_next) {
		if (!banner_printed) {
			printf("\nMulticast Forwarding Cache\n%s%s",
			       " Hash Origin	      Group  ",
			       "       Packets In-Vif  Out-Vifs\n");
			banner_printed = 1;
		}

		klseek(kmem, (ns_off_t)mp, 0);
		read(kmem, mbdata, mbufconst.m_msize);
		mfc = mtod((struct mbuf *)mbdata, struct mfc *);
		printf(" %3u  %-15.15s",
			i,
			routename(mfc->mfc_origin.s_addr,15));
		printf(" %-15.15s %5s   %2u  ",
			routename(mfc->mfc_mcastgrp.s_addr,15),
			pktscale(mfc->mfc_pkt_cnt), mfc->mfc_parent);
		for (vifi = 0; vifi < MAXVIFS; ++vifi) {
			if (viftable[vifi].v_lcl_addr.s_addr == 0)
			    continue;
			if (mfc->mfc_ttls[vifi])
				printf(" %u", vifi);
		}
		printf("\n");
	    }
	}
	if (!banner_printed) printf("\nMulticast Forwarding Cache is empty\n");

	printf("\n");
	nflag = saved_nflag;
}

void
mrt_stats(mrpaddr)
	ns_off_t mrpaddr;
{
	u_int mrtproto;
	struct kna kna;

	if (mrpaddr == 0) {
		printf("ip_mrtproto: symbol not in namelist\n");
		return;
	}

	klseek(kmem, mrpaddr, 0);
	read(kmem, (char *)&mrtproto, sizeof(mrtproto));
	switch (mrtproto) {
	    case 0:
		printf("multicast routing daemon not running or system not configured for mcast routing\n");
		return;

	    case IGMP_DVMRP:
		break;

	    default:
		printf("multicast routing protocol %u, unknown\n", mrtproto);
		return;
	}

	if (sysmp(MP_SAGET, MPSA_TCPIPSTATS, &kna, sizeof(struct kna)) < 0) {
		fprintf(stderr, "mrt_stats: failed sysmp MP_SAGET\n");
		return;
	}

	printf("multicast routing:\n");
	printf(" %10llu multicast forwarding lookup%s\n",
	  kna.mrtstat.mrts_mfc_lookups, plural(kna.mrtstat.mrts_mfc_lookups));

	printf(" %10llu multicast forwarding cache miss%s\n",
	  kna.mrtstat.mrts_mfc_misses, plurales(kna.mrtstat.mrts_mfc_misses));
	printf(" %10llu datagram%s with no route for origin\n",
	  kna.mrtstat.mrts_no_route, plural(kna.mrtstat.mrts_no_route));
	printf(" %10llu upcall%s made to mrouted\n",
	  kna.mrtstat.mrts_upcalls, plural(kna.mrtstat.mrts_upcalls));
	printf(" %10llu datagram%s with malformed tunnel options\n",
	  kna.mrtstat.mrts_bad_tunnel, plural(kna.mrtstat.mrts_bad_tunnel));
	printf(" %10llu datagram%s with no room for tunnel options\n",
	  kna.mrtstat.mrts_cant_tunnel, plural(kna.mrtstat.mrts_cant_tunnel));
	printf(" %10llu datagram%s that arrived on the wrong interface\n",
	  kna.mrtstat.mrts_wrong_if, plural(kna.mrtstat.mrts_wrong_if));

	printf(" %10llu datagram%s dropped due to upcall Q overflow\n",
	  kna.mrtstat.mrts_upq_ovflw, plural(kna.mrtstat.mrts_upq_ovflw));
	printf(" %10llu datagram%s dropped due to upcall socket overflow\n",
	  kna.mrtstat.mrts_upq_sockfull,
	  plural(kna.mrtstat.mrts_upq_sockfull));
	printf(" %10llu datagram%s cleaned up by the cache\n",
	  kna.mrtstat.mrts_cache_cleanups,
	  plural(kna.mrtstat.mrts_cache_cleanups));
	printf(" %10llu datagram%s dropped selectively by ratelimiter\n",
	  kna.mrtstat.mrts_drop_sel, plural(kna.mrtstat.mrts_drop_sel));
	printf(" %10llu datagram%s dropped - bucket Q overflow\n",
	  kna.mrtstat.mrts_q_overflow, plural(kna.mrtstat.mrts_q_overflow));
	printf(" %10llu datagram%s dropped - larger than bkt size\n",
	  kna.mrtstat.mrts_pkt2large, plural(kna.mrtstat.mrts_pkt2large));

	return;
}

/*
 * Curses mode additions by sgi
 */

#include "curses.h"
#include "cdisplay.h"

struct vif viftable[MAXVIFS];
struct vif zviftable[MAXVIFS];

#define MAXROUTES 10000 /* should be dynamic */
int maxroutes;		/* max ever really seen */
struct mfc *tmfc, *zmfc; /* huge malloced tables */

struct for_sorting {
	u_int bytes;	/* sort key */
	u_int packets;
	u_int index;	/* into above arrays */
} *temp;

/*
 * Huge buffers are malloced the first time needed
 */
static int
getbuf(void)
{
	if (tmfc == NULL) {
		tmfc = (struct mfc *) calloc(MAXROUTES,
						 sizeof (struct mfc));
		if (tmfc == NULL)
		    return 1;
	}
	if (zmfc == NULL) {
		zmfc = (struct mfc *)calloc(MAXROUTES,
						 sizeof (struct mfc));
		if (zmfc == NULL)
		    return 1;
	}
	if (temp == NULL) {
		temp = (struct for_sorting *)calloc(MAXROUTES,
						 sizeof (struct for_sorting));
		if (temp == NULL)
		    return 1;
	}
	return 0;
}

void
initmulti(void)
{
	int i;
	if (getbuf()) {
		/* do something */
		sleep(1);
		return;
	}
	bzero(zviftable, sizeof(zviftable));
	for (i = 0; i < maxroutes; i++) {
		tmfc[i].mfc_pkt_cnt = 0;
		tmfc[i].mfc_byte_cnt = 0;
		zmfc[i].mfc_pkt_cnt = 0;
		zmfc[i].mfc_byte_cnt = 0;
	}
}

void
zeromulti(void)
{
	if (getbuf()) {
		/* do something */
		sleep(1);
		return;
	}
	bcopy(viftable, zviftable, sizeof(viftable));
	if (maxroutes) {
		bcopy(tmfc, zmfc, sizeof(struct mfc) * maxroutes);
	}
}

/*
 * handle a single routing entry
 * Skip the garbage entries for objectserver, etc.
 */
void
doroute(struct mfc *mfc, struct mfc *omfc)
{
	int i;

	if (mfc->mfc_pkt_cnt < 10)
	    return;

	for (i = 0; i < maxroutes; i++) {
		if (tmfc[i].mfc_mcastgrp.s_addr ==
		    mfc->mfc_mcastgrp.s_addr &&
		    tmfc[i].mfc_origin.s_addr ==
		    mfc->mfc_origin.s_addr) {
			break;
		}
	}
	if (i == maxroutes) {
		/*
		 * make a new entry in the table
		 */
		if (maxroutes >= MAXROUTES)
		    return;
		maxroutes++;
	}
	temp[i].index = i;
	temp[i].bytes = mfc->mfc_byte_cnt - omfc[i].mfc_byte_cnt;
	temp[i].packets = mfc->mfc_pkt_cnt - omfc[i].mfc_pkt_cnt;
	tmfc[i] = *mfc;
}

/* qsort comparison function */
static int
compare(const void *a, const void *b)
{
	struct for_sorting *aa = (struct for_sorting *)a;
	struct for_sorting *bb = (struct for_sorting *)b;

	return (bb->bytes -  aa->bytes);
}

void
sprmulti(int y, ns_off_t vifaddr, ns_off_t mfcaddr)
{
	struct vif *v, *ov;
	struct mfc *mf, *omf;
	struct mbuf *mfchash[MFCTBLSIZ];
	register vifi_t vifi;
	struct vif tviftable[MAXVIFS];
	struct mfc *mfc;
	struct mbuf *mp;
	int i;

	if (mfcaddr == 0 || vifaddr == 0) {
		move(y+2,0);
		printw(" No multicast support in this kernel");
		return;
	}

	if (getbuf()) {
		/* do something */
		sleep(1);
		return;
	}
	klseek(kmem, mfcaddr, 0);
	read(kmem, (char *)&mfchash, sizeof(mfchash));
	klseek(kmem, vifaddr, 0);
	read(kmem, (char *)tviftable, sizeof(tviftable));

	if (cmode == DELTA) {
		ov = viftable;
		omf = tmfc;
	} else {
		ov = zviftable;
		omf = zmfc;
	}

	move(y,0);
	printw("    Virtual Interface Table	     %s",
	       "      ---- In ----      ---- Out ----");
	y++;
	move(y,0);
	printw("Vif   Local-Address    Remote or ifname %s",
	       "   Bytes Packets     Bytes Packets");
	y++;
	for (vifi = 0, v = tviftable; vifi < MAXVIFS; ++vifi, ++v, ++ov) {

		if (v->v_lcl_addr.s_addr == 0) continue;

		move(y,0);
		printw("%2u %-18.18s",
			   vifi,
			   routename(v->v_lcl_addr.s_addr,15));
		if (v->v_flags & VIFF_TUNNEL) {
			printw(" %-16.16s ",
				routename(v->v_rmt_addr.s_addr,15));
		} else {
			printw("   %-14.14s ", ifname(v->v_lcl_addr.s_addr));
		}
		printw(" %8s", pktscale(v->v_bytes_in - ov->v_bytes_in));
		printw("%8s ", pktscale(v->v_pkt_in - ov->v_pkt_in));
		printw(" %8s", pktscale(v->v_bytes_out - ov->v_bytes_out));
		printw(" %8s", pktscale(v->v_pkt_out - ov->v_pkt_out));
		viftable[vifi] = *v;
		clrtoeol();
		y++;
	}
	if (maxroutes)
	    bzero(temp, sizeof(*temp) * maxroutes);
	for (i = 0; i < MFCTBLSIZ; ++i) {
		for (mp = mfchash[i]; mp != NULL; mp = ((struct mbuf *)mbdata)->m_next) {
			if (klseek(kmem, (ns_off_t)mp, 0) == -1) {
			    break;
			}
			if (read(kmem, mbdata, mbufconst.m_msize) !=
			    mbufconst.m_msize) {
				break;
			}
			if (((struct mbuf *)mbdata)->m_off < 0 ||
			    ((struct mbuf *)mbdata)->m_off > 150) {
				break;
			}
			mfc = mtod(((struct mbuf *)mbdata), struct mfc *);
                       	if (mfc == NULL || (u_long)mfc % 4 ||
                                        mfc->mfc_mcastgrp.s_addr == NULL ||
                                        mfc->mfc_origin.s_addr == NULL)
                                break;

			doroute(mfc, omf);
		}
	}
	qsort(temp, maxroutes, sizeof (struct for_sorting), compare);

	move(y,0);
	clrtoeol();
	y++;
	move(y,0);
	printw("    Source	     Group	  Bytes  Packets In-Vif  Out-Vifs");
	clrtoeol();
	for (i = 0; i < maxroutes; i++) {
		y++;
		if (y >= BOS)
		    break;
		move(y,0);
		mfc = tmfc + temp[i].index;
		printw(" %-15.15s ", inet_ntoa(mfc->mfc_origin));
		printw(" %-15.15s %5s  ",
		       inet_ntoa(mfc->mfc_mcastgrp),
		       pktscale(temp[i].bytes));
		printw(" %5s     ", pktscale(temp[i].packets));
		if ( mfc->mfc_parent > MAXVIFS) {
			printw("[incomplete]");
		} else {
			printw("%2u  ", mfc->mfc_parent);
			for (vifi = 0; vifi < MAXVIFS; ++vifi) {
				if (viftable[vifi].v_lcl_addr.s_addr == 0)
				    continue;
				if (mfc->mfc_ttls[vifi])
				    printw(" %u", vifi);
			}
		}
		clrtoeol();
	}
	while (y < BOS) {
		clrtoeol();
		y++;
	}
}
