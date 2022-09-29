/*
 * Copyright 1989,1990,1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	$Revision: 1.16 $
 */

#include <stdio.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/signal.h>
#include <sys/types.h>

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/file.h>

#include <sys/un.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/ip_var.h>
#include <ctype.h>
#include <netdb.h>
#include <assert.h>
#include <string.h>
#include <bstring.h>

#include <sm_types.h>
#include <sm_log.h>
#include <sm_mib.h>
#include <smtd_parm.h>
#include <smtd.h>
#include <smt_subr.h>
#include <smt_snmp_clnt.h>
#include <smtd_fs.h>
#include <sm_map.h>
#include <sm_addr.h>
#include <tlv_sm.h>

#define DBG_PRINT(a)	printf a

static void setsmtd(void);
static void buildring(char *, int);
static void pr_ring(void);
static void finish(void);
static void xmit(int, char *, char *);
static void sortring(void);

#define	NUMERIC		0x1	/* don't do gethostbyaddr() calls */
#define	ACTIVE		0x2	/* ACTIVEly collect dtaa */
#define	NOSWAPBIT	0x4	/* print addr in FDDI bit order */
int ringflags = ACTIVE;

/* multi ring stuff */
int	Iflag = 0;
int	devnum = 0;
int	unit = 0;
char	*devname = NULL;/* Device name only */
char	interface[16];	/* Device name & unit # -- ipg0 */

LFDDI_ADDR mma;
SMTD smtd;
SMT_FS_INFO preg;
int ringtimo = ((T_NOTIFY*2)+1);

int s;			/* Socket file descriptor for recv frame */

char *hostname = 0;
char *macname = "ff:ff:ff:ff:ff:ff";

#define	MAXPACKET	(4500)	/* max fddi frame size */
static char packet[MAXPACKET];

unsigned interval = 0;		/* interval between screen dumps */
int nreceived = 1;		/* # of nodes on the ring */
static int cursort = 0;

int bufspace = 60*1024;
int smtdebug = LOG_ERR;
char usage[] = "Usage:\n\
smtring [-dpnb] [-i interval] [-I interface] [-t timeout] [address|name]\n";


main(int argc, char **argv)
{
	int c, cc;
	extern int optind;
	extern char *optarg;

	while ((c = getopt(argc, argv, "dpi:nbcot:I:")) != EOF) {
		switch(c) {
		case 'd':
			smtdebug++;
			break;
		case 'i':       /* wait between sending packets */
			interval = atoi(optarg);
			if (interval <= 0) {
				(void)fprintf(stderr,
				    "smtring: invalid interval: %d\n",
				    interval);
				exit(1);
			}
			break;
		case 't':
			ringtimo = atoi(optarg);
			if (ringtimo <= 0) {
				(void)fprintf(stderr,
				    "smtring: invalid timeout: %d\n",
				    ringtimo);
				exit(1);
			}
			break;
		case 'p':
			ringflags &= ~ACTIVE;
			break;
		case 'b':
			ringflags |= NOSWAPBIT;
			break;
		case 'n':
			ringflags |= NUMERIC;
			break;
		case 'I':
			devname = optarg;
			if (devname) {
				char *cp;

				Iflag++;
				strcpy(interface, devname);
				for (cp = devname; isalpha(*cp); cp++)
					;
				unit = atoi(cp);
				*cp-- = 0;
			}
			break;
		default:
			fprintf(stderr, usage);
			exit(1);
		}
	}

	sm_openlog(SM_LOGON_STDERR, smtdebug, 0, 0, 0);

	/* get target mac addr iff any */
	if (optind == argc-1)
		hostname = argv[optind];

	preg.fc = SMT_FC_NIF;
	preg.ft = 0;

	/* set interface to smtd */
	setsmtd();
	(void)setsockopt(s, SOL_SOCKET,SO_RCVBUF, &bufspace,sizeof(bufspace));

	sigset(SIGINT, finish);
	if (interval) {
		sigset(SIGALRM, pr_ring);
		alarm(interval);
	} else {
		sigset(SIGALRM, finish);
		alarm(ringtimo);
	}
	printf("Collecting responses...\n");
	fflush(stdout);

	if (ringflags & ACTIVE)
		xmit(preg.fc, devname ? interface : 0, macname);

	for (;;) {
		if ((cc = recv(s, packet, sizeof(packet), 0)) < 0) {
			if (errno != EINTR)
				perror("smtring: recv");
			continue;
		}
		buildring(packet, cc);
	}
	/*NOTREACHED*/
}

/*
 *
 */
static void
xmit(int fc, char *ifname, char *name)
{
	SMT_FS_INFO preg;
	LFDDI_ADDR targ;

	if (!fddi_aton(name, &targ)) {
		fprintf(stderr,
			"smtring: can't find MAC address for %s\n",
			name);
		map_exit(1);
	}

	preg.fc = fc;
	preg.fsi_da = targ;
	if (map_smt(ifname, FDDI_SNMP_SENDFRAME, &preg, sizeof(preg), 0)
		!= STAT_SUCCESS) {
		fprintf(stderr, "smtring: xmit failed\n");
		map_exit(2);
	}
}

typedef struct ring {
	struct ring *next;		/* links as data was received */
	struct ring *prev;
	struct ring *un;		/* links for ring map */
	struct ring *dn;
	int dn_ok;
	int un_ok;
	LFDDI_ADDR sa;
	LFDDI_ADDR una;
	SMT_FB fb;
} RING;

RING ringhdr;
RING *ring = &ringhdr;

/* quick comparison of MAC addresses, since calling bcmp() is expensive */
#define CMP_MAC(a1,a2) ((a1).b[5]==(a2).b[5] && !bcmp(&(a1),&(a2),sizeof(a)))

/*
 * buf has whole frame.
 * just update it.
 */
static void
buildring(char *buf, int cc)
{
	SMT_FB *fb = (SMT_FB *)buf;
	struct lmac_hdr *mh, *nmh;
	smt_header *fp;
	RING *rp;
	u_long infolen;
	char *info;
	LFDDI_ADDR una;

	fp = &fb->smt_hdr;
	if (fp->fc != SMT_FC_NIF)
		return;

	/* if exists already then just update it */
	rp = ring;
	do {
		mh = &rp->fb.mac_hdr;
		nmh = &fb->mac_hdr;
		if (!bcmp(&mh->mac_sa, &nmh->mac_sa, sizeof(mh->mac_sa))) {
			bcopy(nmh, &rp->fb, MIN(cc,sizeof(rp->fb)));
			return;
		}
		rp = rp->next;
	} while (rp != ring);

	/* alloc enough buf for NIF */
	rp = (RING *)Malloc(MAX(256,sizeof(RING)));
	bcopy(buf, &rp->fb, MIN(cc,sizeof(rp->fb)));

	info = &rp->fb.smt_info[0];
	infolen = 0x28;
	if (tlv_parse_una(&info, &infolen, &una) != RC_SUCCESS) {
		sm_log(LOG_ERR, 0, "bad una purged");
		free(rp);
		return;
	}
	rp->sa = rp->fb.mac_hdr.mac_sa;
	rp->una = una;
	nreceived++;

	/* insert into the ring into both chains */
	rp->dn_ok = cursort;
	rp->un_ok = cursort;

	rp->un = ring;
	rp->dn = ring->dn;
	ring->dn->un = rp;
	ring->dn = rp;

	rp->prev = ring;
	rp->next = ring->next;
	ring->next->prev = rp;
	ring->next = rp;
}


/*
 * print the sorted ring map, and start asking for more data
 */
static void
pr_ring(void)
{
	RING *rp;

	alarm(0);			/* do not be confused by timer */

	if ((ringflags & ACTIVE)	/* start asking for next time */
	    && interval) {
		xmit(preg.fc, devname ? interface : 0, macname);
	}

	sortring();

	/* Print title in reverse video (^[[7m) */
	printf("\n\033[7mLogical FDDI Ring Dump(%d node%s)\033[0m\n\n",
	       nreceived, nreceived != 1 ? "s" : "");
	printf("%-23s %-25.25s    %-25.25s\n",
	       "Station ID", "Upstream Nbr", "MAC Address");

	rp = ring;
	do {
		u_long infolen;
		char *info;
		LFDDI_ADDR una;
		char sida[128], unaa[128], saa[128];
		register struct lmac_hdr *mh;
		smt_header      *fp;

		fp = FBTOFP(&rp->fb);
		if (ringflags&NOSWAPBIT)
			strcpy(sida, sidtoa(&fp->sid));
		else
			strcpy(sida, fddi_sidtoa(&fp->sid,unaa));

		mh = &rp->fb.mac_hdr;
		info = &rp->fb.smt_info[0];
		infolen = 0x28;
		if (tlv_parse_una(&info, &infolen, &una) != RC_SUCCESS) {
			sm_log(LOG_ERR, 0, "bad una");
			map_exit(4);
		}

		if (ringflags&NOSWAPBIT)
			strcpy(unaa, midtoa(&una));
		else if ((ringflags&NUMERIC) || fddi_ntohost(&una, unaa))
			(void)fddi_ntoa(&una, unaa);

		if (ringflags&NOSWAPBIT) {
			strcpy(saa, midtoa(&mh->mac_sa));
		} else if ((ringflags&NUMERIC)
			   || fddi_ntohost(&mh->mac_sa, saa)) {
			(void)fddi_ntoa(&mh->mac_sa, saa);
		}

		printf("%-23s %-25.25s -> %-25.25s\n", sida, unaa, saa);
	} while ((rp = rp->dn) != ring);

	if (interval)
		alarm(interval);
}

/*
 *
 */
static void
finish(void)
{
	sm_log(LOG_DEBUG, 0, "finish: unregister NIF");
	if (map_smt(0, FDDI_SNMP_UNREGFS, &preg, sizeof(preg), devnum)
		!= STAT_SUCCESS) {
		printf("smtring unregister failed\n");
	}
	interval = 0;
	pr_ring();
	sm_log(LOG_DEBUG, 0, "finish: all done");
	map_exit(0);
}


/*
 *
 */
static void
setsmtd(void)
{
	char name[10];
	SMT_MAC mac;
	int i, namelen;
	int vers;
	struct sockaddr_in sin, localsin;
	struct timeval tp;
	struct hostent *hp;	/* Pointer to host info */

	if (hostname) {
		char hname[128];

		map_setpeer(hostname);
		if (gethostname(hname, sizeof(hname)) != 0) {
			sm_log(LOG_EMERG, SM_ISSYSERR, "couldn't get hostname");
			map_exit(1);
		}
		hp = gethostbyname(hname);
	} else {
		hp = gethostbyname("localhost");
	}
	if (!hp) {
		sm_log(LOG_ERR, SM_ISSYSERR, "gethostbyname");
		map_exit(3);
	}
	bzero(&localsin, sizeof(localsin));
	localsin.sin_port = 0;
	localsin.sin_family = hp->h_addrtype;
	bcopy(hp->h_addr, &localsin.sin_addr, hp->h_length);

	sm_openlog(SM_LOGON_STDERR, LOG_ERR, 0, 0, LOG_USER);
	vers = -1;
	if (map_smt(0, FDDI_SNMP_VERSION, &vers, sizeof(vers), devnum)
		!= STAT_SUCCESS) {
		printf("Can't get smtd version\n");
		map_exit(1);
	}
	if (vers != SMTD_VERS) {
		printf("smtring version %d doesn't match daemon's (%d)\n",
			SMTD_VERS, vers);
		map_exit(1);
	}

	if (devname) {
		int i, j;
		IPHASE_INFO ifi;

		for (j = 0; j < NSMTD; j++) {
			if (map_smt(0,FDDI_SNMP_INFO,&ifi,sizeof(ifi),j)
				!= STAT_SUCCESS) {
				break;
			}
			devnum = j;
			for (i = 0; i < ifi.mac_ct; i++) {
				if (!strcmp(ifi.names[i], interface)) {
					goto devfound;
				}
			}
		}
		printf("Invalid device name: %s\n", interface);
		map_exit(1);
	}

devfound:
	sm_openlog(SM_LOGON_STDERR, LOG_ERR, 0, 0, LOG_USER);
	if (map_smt(0, FDDI_SNMP_DUMP, &smtd, sizeof(smtd), devnum)
		!= STAT_SUCCESS) {
		printf("smt status failed\n");
		map_exit(3);
	}
	sm_log(LOG_DEBUG, 0, "setsmtd: dump done, trunk=%s", smtd.trunk);

	i = strlen(smtd.trunk);
	bcopy(smtd.trunk, name, i);
	name[i] = '0';
	name[i+1] = 0;
	if (map_smt(name, FDDI_SNMP_DUMP, &mac, sizeof(mac), devnum)
		!= STAT_SUCCESS) {
		printf("mac dump for %s failed\n", name);
		map_exit(3);
	}
	mma = mac.addr;

	if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
		perror("smtring: recv socket");
		map_exit(3);
	}

	/* bind recv socket */
	bzero(&sin, sizeof(sin));
	sin.sin_port = 0;
	sin.sin_family = localsin.sin_family;
	sin.sin_addr = localsin.sin_addr;
	namelen = sizeof(sin);
	if (((struct sockaddr *)&sin)->sa_family == AF_INET) {
		SNMPDEBUG((LOG_DBGSNMP, 0, "binding to %s:%d",
			inet_ntoa(sin.sin_addr), (int)sin.sin_port));
	} else {
		SNMPDEBUG((LOG_DBGSNMP, 0, "binding to %s",
			((struct sockaddr_un *)&sin)->sun_path));
	}
	if (bind(s, (struct sockaddr *)&sin, namelen) < 0) {
		sm_log(LOG_ERR, SM_ISSYSERR, "smtring bind");
		map_exit(3);
	}

	/* register ring */
	namelen = sizeof(sin);
	if (getsockname(s, (struct sockaddr *)&sin, &namelen) < 0) {
		printf("getsockname failed(%d)\n", errno);
		map_exit(3);
	}
	sm_log(LOG_DEBUG, 0, "setsmtd: udp port=%d", (int)sin.sin_port);
	gettimeofday(&tp, NULL);
	preg.timo = tp.tv_sec + ringtimo;
	preg.fsi_to = sin;
	if (map_smt(0, FDDI_SNMP_REGFS, &preg, sizeof(preg), devnum)
		!= STAT_SUCCESS) {
		printf("smtring register failed\n");
		map_exit(3);
	}
	sm_log(LOG_DEBUG, 0, "smtring: ring registered");

	/* init ring */
	ring->next = ring;
	ring->prev = ring;
	ring->dn = ring;
	ring->un = ring;
	ring->fb.mac_hdr.mac_sa = mma;
	ring->sa = mma;
	FBTOFP(&ring->fb)->sid = smtd.sid;
	{
		u_long infolen = 0x28;
		char *info = &ring->fb.smt_info[0];

		if (tlv_build_una(&info, &infolen, &mac.una) != RC_SUCCESS) {
			sm_log(LOG_ERR, 0, "build una failed");
			map_exit(3);
		}
		ring->una = mac.una;
	}
}


/*
 * Sort the reports into a ring map.
 *	Because there are two chains of entries, and one chain
 *	does not change, we can be sure of checking every entry
 *	with a single pass around the constant chain.
 */
static void
sortring(void)
{
	RING *rp;			/* current entry */
	RING *un;
	RING *last;			/* end of connected arc */

	cursort++;

	/* Quit now if the map is trivial, consisting of 1 or 2 entries.
	 *	This saves tests for degenerate cases in the loops
	 */
	if (ring->next->next == ring)
		return;

	/* Mark all good existing links.
	 *	This keeps the ring stable.
	 */
	rp = ring;
	do {
		un = rp->un;
		if (!bcmp(&rp->una, &un->sa, sizeof(rp->una))) {
			rp->un_ok = cursort;
			un->dn_ok = cursort;
		}
	} while ((rp = un) != ring);

	/* look for and try to fix remaining problems */
	for (rp = ring->next; rp != ring; rp = rp->next) {

		/* worry only about those without a good upstream neighbor */
		if (rp->un_ok == cursort)
			continue;

		/* Look for the upstream neighbor.  If we find an upstream
		 * neighbor, and it does not already have a downstream
		 * neighbor, then move this one and the rest of the arc.
		 *
		 * Look for the upstream neighbor on the constant chain
		 * because we know the predecessor in the neighbor chain
		 * is the wrong one.  Looking on the constant chain
		 * also keeps the map stable.
		 *
		 * Only consider those possible upstream neighbors
		 * that do not already have a good downstream neithbor.
		 */
		for (un = rp->prev; un != rp; un = un->prev) {
			if (un->dn_ok != cursort
			    && !bcmp(&rp->una, &un->sa, sizeof(rp->una))) {

				/* find the arc to move */
				last = rp;
				while (last->dn_ok == cursort)
					last = last->dn;

				/* If we have lost ghosts, then leave them
				 * alone.  In other words, if we would be
				 * only trying to connect the ends of
				 * this arc to itself, then skip this one.
				 */
				if (last == un)
					break;

				/* remove arc from neighbor list */
				rp->un->dn = last->dn;
				last->dn->un = rp->un;

				/* insert arc after the upstream neighbor
				 */
				last->dn = un->dn;
				rp->un = un;
				un->dn->un = last;
				un->dn = rp;

				/* do not move it again */
				rp->un_ok = cursort;
				un->dn_ok = cursort;

				/* Notice if that fixed the other end.
				 */
				if (!bcmp(&last->sa, &last->dn->una,
					  sizeof(last->sa))) {
					last->dn_ok = cursort;
					last->dn->un_ok = cursort;
				}
				break;
			}
		}
	}
}
