/*
 * Copyright 1989,1990,1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	$Revision: 1.13 $
 */

#include <stdio.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/signal.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <string.h>
#include <bstring.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <getopt.h>

#include <ctype.h>
#include <netdb.h>

#include <sm_types.h>
#include <sm_log.h>
#include <sm_mib.h>
#include <smtd.h>
#include <smt_subr.h>
#include <smt_snmp_clnt.h>
#include <smtd_fs.h>
#include <sm_map.h>
#include <sm_addr.h>
#include <smtd_parm.h>

typedef struct {
	int port;
	int timo;
	int xid;
	struct timeval tiss;
} SMT_PING;

/*
 * FRHDR ==> 16+20+4 = 40
 * DXHDR ==> 40+20   = 60
 */
#define FRHDR_SIZE (sizeof(struct lmac_hdr)+sizeof(smt_header)+sizeof(int))
#define DXHDR_SIZE (FRHDR_SIZE+sizeof(SMT_PING))

LFDDI_ADDR tma, mma;
SMTD smtd;
SMT_FS_INFO preg;

#define	MAXWAIT		10	/* max time to wait for response, sec. */
#define FLOOD_RATE	10	/* max msec between flood output */

#define VERBOSE		1	/* verbose flag */
#define QUIET		2	/* quiet flag */
#define FLOOD		4	/* floodping flag */
#define PING_FILLED     16      /* is buffer filled? */
#define	NUMERIC		32	/* don't do gethostbyaddr() calls */
#define	INTERVAL	64	/* did user specify interval? */
#define	DIRECTXMIT	128	/* xmit directly bypassing smtd */

/* MAX_DUP_CHK is the number of bits in received table, ie the */
/*      maximum number of received sequence numbers we can keep track of. */
/*      Change 128 to 8192 for complete accuracy... */

#define MAX_DUP_CHK     (8 * 128)
int     mx_dup_ck = MAX_DUP_CHK;
char    rcvd_tbl[ MAX_DUP_CHK / 8 ];
int     nrepeats = 0;

#define A(bit)          rcvd_tbl[bit>>3]    /* identify byte in array */
#define B(bit)          (1 << (bit & 0x07)) /* identify bit in byte */
#define SET(bit)        (A(bit) |= B(bit))
#define CLR(bit)        (A(bit) &= (~B(bit)))
#define TST(bit)        (A(bit) & B(bit))

int	i, pingflags = 0, options;

/* multi ring stuff */
int	Iflag = 0;
int	devnum = 0;
char	*devname = NULL;		/* Device name only */
char	interface[16];			/* Device name & unit # -- ipg0 */

int recvsock = -1;			/* Socket for recv frame */
int sendsock = -1;			/* Socket for send frame */

int datalen = 8;	/* How much data */
#define	MAXPACKET	(sizeof(outpack)-DXHDR_SIZE)	/* max packet size */

char usage[] = "Usage: \n\
smtping [-dfnqrvx] [-c count] [-s size] [-l preload] [-p pattern] [-i interval]\
	[-I interface] [-i interval] [-I ringnum] host\n";

char *hostname;
char hname[128];

SMT_FB outpack;
SMT_FB inpack;

int npackets=0;
int preload = 0;			/* number of packets to "preload" */
int ntransmitted = 0;			/* output sequence # = #sent */
int ident;
int ms_int;				/* interval between packets in msec */

int nreceived = 0;			/* # of packets we got back */
int tmin = 999999999;
int tmax = 0;
int tsum = 0;				/* sum of all times */

extern char *pr_addr();

static void prefinish(void), finish(void), catcher(void), pinger(void);
static void fill(char*,char*);
static long msec(struct timeval*, struct timeval*);
static void pr_pack(SMT_FB *, int);
static void setsmtd(void);
static void setoutbuf();

int bufspace = 60*1024;
char *patternp;

struct timeval last_tx, first_tx;
struct timeval last_rx, first_rx;
int lastrcvd = 1;			/* last ping sent has been received */

/*
 *			M A I N
 */
main(argc, argv)
char *argv[];
{
	char *cp;
	int c, k, on = 1, hostind = 0;
	char *p;

	while ((c = getopt(argc, argv, "c:xdfi:l:np:qrs:vRLT:I:")) != EOF) {
		switch(c) {
		case 'c':
			npackets = strtol(optarg,&p,0);
			if (*p != '\0' || npackets <= 0) {
				fprintf(stderr, usage);
				exit(1);
			}
			break;
		case 'd':
			options |= SO_DEBUG;
			break;
		case 'f':
			pingflags |= FLOOD;
			break;
		case 'x':
			pingflags |= DIRECTXMIT;
			break;
		case 'h':
			hostind = optind-1;
			break;
		case 'i':		/* wait between sending packets */
			ms_int = strtod(optarg,&p)*1000.0 + 0.5;
			if (*p != '\0'
			    || ms_int <= 0) {
				(void)fprintf(stderr,
					      "smtping: bad interval \"%s\".\n",
					      optarg);
				exit(1);
			}
			break;
		case 'l':
			preload = strtol(optarg,&p,0);
			if (*p != '\0'
			    || preload < 0) {
				(void)fprintf(stderr,
					    "smtping: bad preload value.\n");
				exit(1);
			}
			break;
		case 'n':
			pingflags |= NUMERIC;
			break;
		case 'p':		/* fill buffer with user pattern */
			pingflags |= PING_FILLED;
			patternp = optarg;
			break;
		case 'q':
			pingflags |= QUIET;
			break;
		case 's':		/* size of packet to send */
			datalen = strtol(optarg,&p,0);
			if (*p != '\0' || datalen <= 0) {
				(void)fprintf(stderr,
				      "smtping: illegal packet size.\n");
				exit(1);
			}
			if (datalen > MAXPACKET) {
				(void)fprintf(stderr,
				      "smtping: packet size too large.\n");
				exit(1);
			}
			break;
		case 'v':
			pingflags |= VERBOSE;
			break;
		case 'I':
			devname = optarg;
			cp = devname+strlen(devname)-1;
			if (*devname == '\0' || !isdigit(*cp)) {
				fprintf(stderr,
					"Invalid device name: \"%s\"\n",
					optarg);
				exit(1);
			}
			Iflag++;
			strcpy(interface, devname);
			while (isdigit(*cp))
				cp--;
			*cp = 0;
			break;
		default:
			fprintf(stderr, usage);
			exit(1);
		}
	}

	if (ms_int == 0)
		ms_int = (pingflags & FLOOD) ? FLOOD_RATE : 1000;

	if (npackets != 0)
		npackets += preload;


	if (hostind == 0) {
		if (optind != argc-1) {
			fprintf(stderr, usage);
			exit(1);
		} else hostind = optind;
	}
	hostname = argv[hostind];
	if (smt_getmid(hostname, &tma)) {
		fprintf(stderr, "smtping: can't find MAC address for %s\n",
			hostname);
		map_exit(1);
	}

	if (!(pingflags & DIRECTXMIT) && (options & SO_DEBUG)) {
		fprintf(stderr, "smtping: -d is meaningful only with -x\n");
		exit(1);
	}
	/* Now check permission so don't need to be root to validate args. */
	if ((pingflags & DIRECTXMIT) && (geteuid() != 0)) {
		fprintf(stderr, "Superuser permission required for -x\n");
		exit(1);
	}

	sm_openlog(SM_LOGON_STDERR, LOG_ERR, 0, 0, 0);
	/* set interface to smtd */
	setsmtd();
	setoutbuf();

	if (pingflags & DIRECTXMIT) {
		if (options & SO_DEBUG) {
			(void)setsockopt(recvsock, SOL_SOCKET, SO_DEBUG,
					&on, sizeof(on));
			(void)setsockopt(sendsock,SOL_SOCKET,SO_DEBUG,
					&on,sizeof(on));
		}
	}

	/*
	 * When pinging the broadcast address, you can get a lot
	 * of answers.  Doing something so evil is useful if you
	 * are trying to stress the ring.
	 */
	(void)setsockopt(recvsock, SOL_SOCKET, SO_RCVBUF,
			 &bufspace, sizeof(bufspace));
	/* make it possible to send giant probes */
	(void)setsockopt(sendsock, SOL_SOCKET, SO_SNDBUF,
			 &bufspace, sizeof(bufspace));

	printf("SMTPING %s (%s) -- %d data bytes\n",
	       hostname, fddi_ntoa(&tma, hname), datalen);

	signal(SIGINT, prefinish);

	/* fire off them quickies */
	for (i = 0; i < preload; i++)
		pinger();

	if (!(pingflags & FLOOD)) {	/* start things going */
		struct itimerval val, oval;

		val.it_value.tv_sec = 0;
		val.it_value.tv_usec = 1000;	/* something small */
		val.it_interval.tv_sec = ms_int / 1000;
		val.it_interval.tv_usec = (ms_int % 1000) * 1000;
		signal(SIGALRM, catcher);
		if (0 > setitimer(ITIMER_REAL, &val, &oval)) {
			perror("ping: setitimer");
			exit(1);
		}
	}

	for (;;) {
		int cc;
		int fromlen;

		if ((pingflags & FLOOD)
		    && (!npackets || ntransmitted <= npackets)) {
			struct timeval timeout, now;
			fd_set fdmask;

			/* Avoid sending bursts of packets when we get a
			 * delayed burst.  We want to send a new ping after
			 * the last packet in a burst of responses, if
			 * the other side has caught up.  We also want
			 * to transmit at least once every ms_int seconds.
			 *
			 * Rounding in the kernel can make the select() return
			 * early.  Imagine what can happen if we finish
			 * sending a packet just before the end of the tick.
			 * The select() will return immediately, and it will
			 * appear that it has been at least one tick since
			 * we sent anything.
			 */

			if (lastrcvd) {
				pinger();
				continue;
			}

			(void)gettimeofday(&now,0);
			if (msec(&now, &last_tx) >= ms_int) {
				timeout.tv_sec = 0;
				timeout.tv_usec = 0;
			} else {
				timeout.tv_sec = ms_int / 1000;
				timeout.tv_usec = (ms_int % 1000)*1000;
			}
			FD_ZERO(&fdmask);
			FD_SET(recvsock, &fdmask);
			cc = select(recvsock+1, &fdmask, 0, 0, &timeout);
			if (cc == 0) {
				if (timeout.tv_usec == 0)
					pinger();
				continue;
			} else if (cc < 0) {
				if (errno != EINTR) {
					fflush(stdout);
					perror("ping: select");
					fflush(stderr);
					exit(1);
				}
				continue;
			}
		}
		cc = recv(recvsock,&inpack,sizeof(inpack),0);
		if (cc < 0) {
			fflush(stdout);
			if (errno != EINTR)
				perror("ping: recvfrom");
			fflush(stderr);
			continue;
		}
		pr_pack(&inpack, cc);
		if (npackets && nreceived >= npackets)
			finish();
	}
	/*NOTREACHED*/
}

/* start waiting for the final bell
 */
static void
startfin()
{
	struct itimerval val, oval;
	long waittime;

	npackets = MAX(ntransmitted-1,1);

	if (nreceived || (pingflags & FLOOD)) {
		waittime = 2 * tmax;
		if (waittime == 0)
			waittime = 1000;
	} else {
		waittime = MAXWAIT*1000;
	}
	val.it_value.tv_sec = waittime / 1000;
	val.it_value.tv_usec = (waittime % 1000)*1000;
	val.it_interval.tv_sec = 0;
	val.it_interval.tv_usec = 0;
	signal(SIGALRM, finish);
	if (0 > setitimer(ITIMER_REAL, &val, &oval)) {
		perror("ping: setitimer");
		exit(1);
	}
}


/*
 *			C A T C H E R
 *
 * This routine causes another PING to be transmitted, and then
 * schedules another SIGALRM for 1 second from now.
 *
 * Bug -
 *	Our sense of time will slowly skew (ie, packets will not be launched
 *	exactly at 1-second intervals).  This does not affect the quality
 *	of the delay and loss statistics.
 */
static void
catcher()
{
	signal(SIGALRM, catcher);
	pinger();
}

/* jiggle the cursor for flood-ping
 */
static void
jiggle(char c)
{
	static struct timeval lastflush;
	static char lastjiggle;

	if (pingflags & QUIET)
		return;

	putchar(c);

	/* flush the FLOOD dots when things are quiet
	 * or occassionally at odd times to make
	 * the cursor jiggle.
	 */
	if (lastflush.tv_sec == 0
	    || msec(&last_tx, &lastflush) >= 200) {
		if (c == lastjiggle) {
			lastjiggle = '\0';
		} else {
			fflush(stdout);
			gettimeofday(&lastflush,0);
			lastjiggle = c;
		}
	}
}


/*
 * transmit a SMT ECHO REQUEST frame.
 */
static void
pinger()
{
	int i, cc;
	SMT_PING *r;
	char *info = outpack.smt_info;

	if (npackets == 0 || ntransmitted < npackets) {

		if (pingflags&DIRECTXMIT) {
			r = (SMT_PING*)&outpack.smt_info[2*sizeof(short)];
			r->xid = htonl((unsigned short)(ntransmitted));
			ntransmitted++;
			CLR(r->xid%mx_dup_ck);

			cc = datalen + DXHDR_SIZE;

			gettimeofday(&last_tx,0);
			if (first_tx.tv_sec == 0)
				first_tx = last_tx;
			bcopy(&last_tx,&r->tiss,sizeof(last_tx));

			i = send(sendsock, &outpack, cc, MSG_DONTROUTE);
			if (i != cc)  {
				fflush(stdout);
				if (i < 0)
					perror("smtping: send");
				fflush(stderr);
				printf("smtping: wrote %s %d chars, ret=%d\n",
				       hostname, cc, i);
				fflush(stdout);
			}
		} else {
			r = (SMT_PING*)&outpack.smt_info[sizeof(SMT_FS_INFO)];
			r->xid = htonl((unsigned short)(ntransmitted));
			ntransmitted++;
			CLR(r->xid%mx_dup_ck);

			cc = sizeof(SMT_FS_INFO)+sizeof(SMT_PING)+datalen;

			gettimeofday(&last_tx,0);
			if (first_tx.tv_sec == 0)
				first_tx = last_tx;
			bcopy(&last_tx,&r->tiss,sizeof(last_tx));

			if (map_smt(interface, FDDI_SNMP_SENDFRAME,
				    info, cc, 0)
			    != STAT_SUCCESS) {
				printf("smtping: wrote %s %d chars\n",
				       hostname, cc);
				fflush(stdout);
			}
		}

		/* be conservative about timing our tranmissions to avoid
		 * sending too faster.
		 */
		lastrcvd = 0;

		if (pingflags & FLOOD)
			jiggle('.');
	}

	if (npackets == ntransmitted)
		startfin();
}

/*
 * Print out the packet, if it came from us.  This logic is necessary
 * because ALL readers of the socket get a copy of ALL SMT packets
 * which arrive ('tis only fair).  This permits multiple copies of this
 * program to be run without having intermingled output (or statistics!).
 */
static void
pr_pack(SMT_FB *fb, int cc)
{
	register int i;
	register char *cp,*dp;
	int triptime, dupflag;

	register struct lmac_hdr *mh = &fb->mac_hdr;
	register smt_header *fp = &fb->smt_hdr;
	register char *info = &fb->smt_info[0];
	register u_short *usp = (u_short *)info;
	register SMT_PING *r = (SMT_PING *)(usp+2);

	/* Check the frame size */
	if (cc < (datalen+DXHDR_SIZE)) {
		if (pingflags & VERBOSE)
			printf("packet too short (%d bytes)\n", cc);
		fflush(stdout);
		return;
	}
	if (pingflags&DIRECTXMIT) {
		if (cc != datalen + DXHDR_SIZE)
			return;
	} else {
		if (cc != sizeof(SMT_FS_INFO)+sizeof(SMT_PING)+datalen)
			return;
	}

	if ((fp->fc != SMT_FC_ECF) || (fp->type != SMT_FT_RESPONSE) ||
	    (r->port != preg.fsi_to.sin_port) || (r->timo != preg.timo) ||
	    (cc != (datalen+DXHDR_SIZE))) {
		/* We got something other than an ECHOREPLY */
		if (pingflags & VERBOSE) {
			printf("%d bytes from %s: ",
			       cc, fddi_ntoa(&mh->mac_sa, hname));
			printf("datalen=%d DXHDR = %d\n", datalen,DXHDR_SIZE);
			printf("port=%d r_port = %d\n",
			       r->port, preg.fsi_to.sin_port);
			printf("timo=%d r_timo = %d\n", r->timo, preg.timo);

		}
		return;			/* ignore bad packets */
	}

	if (r->xid == ntransmitted-1)
		lastrcvd = 1;
	gettimeofday(&last_rx, 0);
	if (first_rx.tv_sec == 0)
		first_rx = last_rx;
	nreceived++;
	triptime = msec(&last_rx, &r->tiss);
	tsum += triptime;
	if (triptime < tmin)
		tmin = triptime;
	if (triptime > tmax)
		tmax = triptime;

	if (TST(r->xid%mx_dup_ck)) {
		nrepeats++, nreceived--;
		dupflag = 1;
	} else {
		SET(r->xid%mx_dup_ck);
		dupflag = 0;
	}

	if (pingflags & QUIET)
		return;

	if (pingflags & FLOOD) {
		jiggle('\b');
		return;
	}

	printf("%d bytes from %s: xid=%d", cc,
	       fddi_ntoa(&mh->mac_sa, hname), r->xid);
	if (dupflag)
		printf(" DUP!");
	printf(" time=%d ms", triptime);

	if (pingflags & VERBOSE) {

		/* check the data including timestamp */
		if (pingflags&DIRECTXMIT) {
			cp = fb->smt_info;
			cc -= FRHDR_SIZE;
			dp = outpack.smt_info;
		} else {
			cp = (char*)r;
			cc -= (FRHDR_SIZE + 2*sizeof(short));
			dp = &outpack.smt_info[sizeof(SMT_FS_INFO)];
		}
		for (i = 0; i < cc; i++, cp++, dp++) {
			if (*cp == *dp)
				continue;

			printf("\nwrong data byte #%d should be 0x%x but was 0x%x",
			       i, *dp, *cp);
			printf("\nPacket sent:\n");
			dp = &outpack.smt_info[sizeof(SMT_PING)];
			for (i = 0; i < cc; i++, dp++) {
				if ((i%32) == 0) {
					printf("\n\t");
				}
				printf("%x ", *dp);
			}

			printf("\nPacket returned:\n");
			fflush(stdout);
			cp = (char*)(r+1);
			for (i = 0; i < cc; i++, cp++) {
				if ((i%32) == 0) {
					fflush(stdout);
				}
				printf("%x ", *cp);
			}
			break;
		}
	}
	putchar('\n');
	fflush(stdout);
}


/*
 * compute the difference of two timevals in msec.
 */
static long
msec(struct timeval *now, struct timeval *then)
{
	double val;

	val = (now->tv_sec - then->tv_sec);
	if (val < 1000000.0 && val > -1000000.0)
		val *= 1000.0;

	val += (now->tv_usec - then->tv_usec)/1000.0;
	return rint(val);
}


/*
 * On the first SIGINT, allow any outstanding packets to dribble in
 */
static void
prefinish()
{
	if (lastrcvd			/* quit now if caught up */
	    || nreceived == 0)		/* or if remote is dead */
		finish();

	signal(SIGINT, finish);		/* do this only the 1st time */

	if (npackets > ntransmitted	/* let the normal limit work */
	    || npackets == 0)
		startfin();
}


/*
 * Print out statistics, and give up.
 * Heavily buffered STDIO is used here, so that all the statistics
 * will be written with 1 sys-write call.  This is nice when more
 * than one copy of the program is running on a terminal;  it prevents
 * the statistics output from becomming intermingled.
 */
static void
finish()
{
	double t,r;

	/*
	 * Disable alarms to avoid a race getting here in the -c case,
	 * since startfin gets called on the last packet and starts an
	 * itimer for the twice the maximum round trip time.  This alarm
	 * may fire before we get to exit, resulting in two copies of
	 * the statistics in that case.
	 */
	signal(SIGALRM, SIG_IGN);

	putchar('\n');
	fflush(stdout);
	printf("\n----%s PING Statistics----\n", hostname);
	printf("%d packets transmitted, ", ntransmitted);
	printf("%d packets received, ", nreceived);
	if (nrepeats) printf("+%d duplicates, ", nrepeats);
	if (ntransmitted)
		if (nreceived > ntransmitted)
			printf("-- somebody's printing up packets!");
		else
			printf("%d%% packet loss",
			  (int) (((ntransmitted-nreceived)*100) /
			  ntransmitted));
	printf("\n");
	if (nreceived) {
		r = msec(&last_rx,&first_rx)/1000.0;
		if (r == 0)
			r = 0.0001;
		t = msec(&last_tx,&first_tx)/1000.0;
		if (t == 0)
			t = 0.0001;
		printf("round-trip min/avg/max = %d/%d/%d ms",
			tmin, tsum/(nreceived+nrepeats), tmax);
		if (pingflags & FLOOD)
			printf("  %.0f,%.0f avg packets/sec sent,received",
				ntransmitted/t, nreceived/r);
		putchar('\n');
	}
	fflush(stdout);
	if (map_smt(0, FDDI_SNMP_UNREGFS, &preg, sizeof(preg), devnum)
		!= STAT_SUCCESS) {
		printf("smtping unregister failed\n");
		map_exit(2);
	}
	map_exit(0);
}


static void
fill(char *bp, char *patp)
{
	register int ii,jj,kk;
	char *cp;
	int pat[16];

	if (patp == NULL) {
		for (ii = 0; ii < datalen; ii++)
			*bp++ = ii;
		return;
	}

	for (cp = patp; *cp; cp++)
		if (!isxdigit(*cp)) {
			fflush(stdout);
			fprintf(stderr, "\"-p %s\" ???: ", patp);
			fprintf(stderr,
				"patterns must be specified as hex digits\n");
			map_exit(1);
		}

	ii = sscanf(patp,
		"%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x",
		&pat[0], &pat[1], &pat[2], &pat[3],
		&pat[4], &pat[5], &pat[6], &pat[7],
		&pat[8], &pat[9], &pat[10], &pat[11],
		&pat[12], &pat[13], &pat[14], &pat[15]);

	if (ii > 0)
		for (kk=0; kk<=MAXPACKET-(8+ii); kk+=ii)
		for (jj=0; jj<ii; jj++)
			bp[jj+kk] = pat[jj];

	if (!(pingflags & QUIET)) {
		printf("PATTERN: 0x");
		for (jj=0; jj<ii; jj++)
			printf("%02x", bp[jj]&0xFF);
		printf("\n");
	}

}

/*
 * Compose a SMT ECHO REQUEST frame.
 */
static void
setoutbuf()
{
	SMT_PING *r;

	if (!(pingflags&DIRECTXMIT)) {
		SMT_FS_INFO *pp;

		pp = (SMT_FS_INFO *)&outpack.smt_info[0];
		pp->fc = SMT_FC_ECF;
		pp->fsi_da = tma;

		r = (SMT_PING*)&outpack.smt_info[sizeof(SMT_FS_INFO)];
		r->port = preg.fsi_to.sin_port;
		r->timo = preg.timo;
	} else {
		struct lmac_hdr *mh = &outpack.mac_hdr;
		smt_header *fp = &outpack.smt_hdr;
		u_short *usp = (u_short*)&outpack.smt_info[0];

		/* setup mac header */
		mh->mac_fc = FC_SMTINFO;
		mh->mac_da = tma;
		mh->mac_sa = mma;

		/* setup smt header */
		fp->fc = SMT_FC_ECF;
		fp->type = SMT_FT_REQUEST;
		fp->vers = htons(smtd.vers_op);
		fp->sid = smtd.sid;
		fp->pad = 0;
		fp->len = htons(4+sizeof(SMT_PING)+datalen);

		/* setup tlv */
		*usp = PTYPE_ECHO; usp++;
		*usp = datalen+sizeof(SMT_PING); usp++;

		/* setup ping hdr */
		r = (SMT_PING *)usp;
		r->port = preg.fsi_to.sin_port;
		r->timo = preg.timo;
	}

	fill((char*)(r+1), (pingflags&PING_FILLED)?patternp:NULL);
}



static void
setsmtd(void)
{
	SMT_MAC mac;
	int i, namelen;
	int vers;
	struct sockaddr_in sin;
	struct timeval tp;
	struct hostent *hp;		/* Pointer to host info */
	struct sockaddr_raw sr;

	sm_openlog(SM_LOGON_STDERR, 0, 0, 0, LOG_USER);
	vers = -1;
	if (map_smt(0, FDDI_SNMP_VERSION, &vers, sizeof(vers), devnum)
	    != STAT_SUCCESS) {
		printf("Can't get smtd version\n");
		map_exit(1);
	}
	if (vers != SMTD_VERS) {
		printf("smtping version %d doesn't match daemon's (%d)\n",
		       SMTD_VERS, vers);
		map_exit(1);
	}
	sm_openlog(SM_LOGON_STDERR, LOG_ERR, 0, 0, LOG_USER);

	if (devname) {
		int i, j;
		IPHASE_INFO ifi;

		for (j = 0; j < NSMTD; j++) {
			if (map_smt(0,FDDI_SNMP_INFO, &ifi,sizeof(ifi),j)
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
		printf("Invalid device name: \"%s\"\n", interface);
		map_exit(1);
	}
devfound:

	if (map_smt(0,FDDI_SNMP_DUMP,&smtd,sizeof(smtd),devnum)
	    != STAT_SUCCESS) {
		printf("smtd dump\n");
		map_exit(2);
	}

	if (!devname) {
		i = strlen(smtd.trunk);
		bcopy(smtd.trunk, interface, i);
		interface[i] = '0';
		interface[i+1] = 0;
	}

	if (map_smt(interface,FDDI_SNMP_DUMP,&mac,sizeof(mac),devnum)
	    != STAT_SUCCESS) {
		printf("mac dump for %s failed\n", interface);
		map_exit(2);
	}
	mma = mac.addr;

	/* open direct xmit socket */
	if (pingflags & DIRECTXMIT) {
		sendsock = socket(PF_RAW, SOCK_RAW, RAWPROTO_DRAIN);
		if (sendsock < 0) {
			perror("smtping: send socket");
			map_exit(5);
		}
		bzero(&sr, sizeof(sr));
		sr.sr_family = AF_RAW;
		sr.sr_port = 55;	/* something illegal */
		strncpy(sr.sr_ifname, interface, strlen(interface));
		if (bind(sendsock, &sr, sizeof(sr)) < 0) {
			sm_log(LOG_ERR,SM_ISSYSERR,"bind port(%s)",interface);
			map_exit(5);
		}
	}

	/* open recv socket */
	if ((recvsock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
		perror("smtping: recv socket");
		map_exit(5);
	}

	/* bind recv socket */
	hp = gethostbyname("localhost");
	if (!hp) {
		hp = gethostbyname("127.1");
		if (!hp) {
			sm_log(LOG_ERR, SM_ISSYSERR, "get localhost failed");
			map_exit(3);
		}
	}
	bzero(&sin, sizeof(sin));
	sin.sin_port = 0;
	sin.sin_family = AF_INET;
	bcopy(hp->h_addr, (caddr_t)&sin.sin_addr, sizeof(sin.sin_addr));
	namelen = sizeof(sin);

	if (bind(recvsock, (struct sockaddr *)&sin, namelen) < 0) {
		sm_log(LOG_ERR, SM_ISSYSERR, "bind");
		map_exit(3);
	}


	/*
	 * register with SMTD to receive responses for a week
	 */
	namelen = sizeof(sin);
	if (getsockname(recvsock, (struct sockaddr *)&sin, &namelen) < 0) {
		printf("getsockname failed(%d)\n", errno);
		map_exit(2);
	}
	gettimeofday(&tp, NULL);
	preg.timo = tp.tv_sec + 60*60*24*7;
	preg.fc = SMT_FC_ECF;
	preg.ft = SMT_FT_RESPONSE;
	preg.fsi_to = sin;
	if (map_smt(0, FDDI_SNMP_REGFS, &preg, sizeof(preg), devnum)
		!= STAT_SUCCESS) {
		printf("smtping: register failed\n");
		map_exit(2);
	}
}
