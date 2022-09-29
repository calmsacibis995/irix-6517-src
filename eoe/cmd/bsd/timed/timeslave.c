/* Measure the difference between our clock and some other,
 *
 *	using ICMP timestamp messages and the time service,
 *	or using serial I/O, and adjust our clock with the result.
 *
 *	We can talk to either a typical TCP machine, or to a Precision
 *	Time Standard WWV receiver, or a TrueTime Model 600 GPS	receiver.
 *
 * Given to 4.4BSD--vjs
 *
 */


#ident "$Revision: 1.39 $"

#include <stdio.h>
#include <math.h>
#include <values.h>
#include <errno.h>
#include <syslog.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/param.h>
#include <sys/types.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/times.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/fcntl.h>
#include <sys/termio.h>
#include <sys/schedctl.h>
#include <sys/syssgi.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <bstring.h>
#include <sys/clock.h>
#ifdef TIMED_IRIX_62
#define cap_settimeofday settimeofday
#define cap_adjtime	adjtime
#define cap_socket	socket
#define cap_schedctl	schedctl
#else
#include <cap_net.h>
#endif

#define min(a,b)	((a)>(b) ? (b) : (a))


#define MSECDAY (SECDAY*1000)		/* msec/day */

#define BU ((unsigned long)2208988800)	/* seconds from 1990 to UNIX epoch  */

#define	USEC_PER_SEC	1000000
#define F_USEC_PER_SEC	(USEC_PER_SEC*1.0)	/* reduce typos */
#define F_NSEC_PER_SEC	(F_USEC_PER_SEC*1000.0)


#define	MAXADJ 20			/* max adjtime(2) correction in sec */

#define MIN_ROUND (1.0/HZ)		/* best expected ICMP round usec */
double avg_round = SECDAY;		/* minimum round trip */
int clr_round = 0;

#define MAX_NET_DELAY 20		/* max one-way packet delay (sec) */
#define MAX_MAX_ICMP_TRIALS 200		/* try this hard to read the clock */
int icmp_max_sends;
int icmp_min_diffs;			/* require this many responses */
int icmp_wait = 1;			/* this many seconds/ICMP try */

#define MAX_TIME_TRIALS 3
int date_wait = 4;			/* wait for date (sec) */

#define VDAMP 32			/* damping for error variation */
#define DISCARD_MEAS 4			/* discard this many wild deltas */
#define MAX_CDAMP 32			/* damping for changes */

#define INIT_PASSES 11			/* this many initial, quick fixes */

#define DRIFT_DAMP 4			/* damp drift for these hours */
#define DRIFT_TICKS (DRIFT_DAMP*SECHR*HZ*1.0)
double drift_err = 0.0;			/* sec/tick of adjustments */
double drift_ticks = SECHR*HZ*1.0;	/* total HZ of good measurements
					 *  preload to avoid division by 0
					 *  and for initial stability
					 */
#define MAX_DRIFT 3000000.0		/* max drift in nsec/sec, 0.3% */
#define MAX_TRIM MAX_DRIFT		/* max trim in nsec/sec */
double max_drift;			/* max clock drift, sec/tick */
double neg_max_drift;

int chg_smooth;				/* spread changes out this far */

#define USEC_PER_C (USEC_PER_SEC/960)	/* usec/char for the WWV clock */

clock_t rep_rate;			/* check this often (in HZ) */
#define DEF_REP_RATE (210*HZ)
#define INIT_REP_RATE (90*HZ)		/* run fast at first */

enum {
    MODE_UNKNOWN,
    MODE_NOT_ICMP,
    MODE_ICMP,				/* network master */
    MODE_PST_WWV,			/* Precision Standard Time WWV */
    MODE_TT600				/* TrueTime model 600 */
} mode = MODE_UNKNOWN;			/* 0=have netmaster, 1=have clock */

struct sockaddr_in masaddr;		/* ICMP master's address */
char *masname;				/* master hostname or device name */

static int debug;
static int icmp_logged;			/* <>0 if master complaint logged */
static int udp_logged;
static int udp_year_logged;
static int clock_logged;
static int wwv_year_logged;
static int headings;
static time_t log_ticks;

unsigned char background = 1;		/* fork into the background */

unsigned char quiet = 0;		/* do not complain about dead server */

unsigned char measure_only;		/* <>0 to only measure, not adjust */

int set_year;				/* set to this year (since 1900) */
int set_year2;				/* or this year */

unsigned char cap_y;			/* guard against WWV switch changes,
					 * or their lack on the first of the
					 * year.
					 */

char *pgmname;				/* our name */


#define PROTO "udp"
#define PORT "time"

char *timetrim_fn;
FILE *timetrim_st;
char *timetrim_wpat = "long timetrim = %ld;\ndouble tot_adj = %.0f;\ndouble tot_ticks = %.0f;\n/* timeslave version 2 */\n";
char *timetrim_rpat = "long timetrim = %ld;\ndouble tot_adj = %lf;\ndouble tot_ticks = %lf;";
long timetrim;
double tot_adj, hr_adj;			/* totals in nsec */
double tot_ticks, hr_ticks;

double experr = 0.0;			/* expected error */
double varerr = 1.0;			/* variation in error in sec/tick */
double skip_varerr;
double chgs[MAX_CDAMP];			/* committed changes */
unsigned int cur_chg = 0;
unsigned int skip = 0;
unsigned int pass = 0;
double prev_ticks = 0;
double prev_drift_ticks = 0;
double prev_drift;


static void
log_debug(int priority, char *p, ...)
{
    va_list args;

    va_start(args, p);
    (void)fprintf(stderr, "%s: ", pgmname);
    (void)vfprintf(stderr, p, args);
    (void)putc('\n',stderr);
    (void)fflush(stderr);
    vsyslog(priority, p, args);
    va_end(args);
}


static void
log_once(int *flag, char *p, ...)
{
    va_list args;

    if (debug > 2 || !*flag) {
	va_start(args, p);
	(void)fprintf(stderr, "%s: ", pgmname);
	(void)vfprintf(stderr, p, args);
	(void)putc('\n',stderr);
	(void)fflush(stderr);
	vsyslog(LOG_ERR, p, args)
	va_end(args);
    }

    *flag = 1;
}


static void
morelogging()
{
    icmp_logged = 0;
    udp_logged = 0;
    udp_year_logged = 0;
    clock_logged = 0;
    wwv_year_logged = 0;
    headings = 0;
}


/* change debugging, probably for a signal
 */
static void
moredebug()
{
    if (!debug)
	morelogging();
    debug++;
    log_debug(LOG_DEBUG, "debugging increased to %d", debug);
}

static void
lessdebug()
{
    if (debug != 0)
	debug--;
    log_debug(LOG_DEBUG, "debugging decreased to %d", debug);
}


/* exit cleanly for profiling.
 */
/* ARGSUSED */
static void
done(int sig)
{
    exit(1);
}


/* 'fix' a time value
 */
static void
fix_time(struct timeval *tp)
{
    for (;;) {
	if (tp->tv_usec > USEC_PER_SEC) {
	    tp->tv_sec++;
	    tp->tv_usec -= USEC_PER_SEC;
	    continue;
	}
	if (tp->tv_usec < -USEC_PER_SEC
	    || (tp->tv_usec < 0 && tp->tv_sec > 0)) {
	    tp->tv_sec--;
	    tp->tv_usec += USEC_PER_SEC;
	    continue;
	}
	return;
    }
}


/* compute possible years in case the clock or remote host is confused
 */
static void
fix_set_year(time_t now)		/* UNIX time */
{
    int leap_day_sec;

    if (!cap_y && set_year == 0)
	return;

    /* compute the current year, adjusted for leap years */
    leap_day_sec = ((now/SECYR+2)/4)*SECDAY;
    set_year = (now - leap_day_sec)/SECYR + 70;

    /* Near the end of a year, be prepared for a jump to the next year.
     */
    if ((now-leap_day_sec)%SECYR < (365-7)*SECDAY) {
	set_year2 = set_year;
    } else {
	set_year2 = set_year+1;
    }
}



/* Checksum routine for Internet Protocol family headers
 */
static int
in_cksum(u_short *addr, int len)
{
    u_short *w = addr;
    u_short answer;
    int sum = 0;

    while( len > 1 )  {
	sum += *w++;
	len -= 2;
    }

    /* mop up an odd byte, if necessary */
    if( len == 1 )
	sum += (*(u_char *)w) << 8;

    /*
     * add back carry outs from top 16 bits to low 16 bits
     */
    sum = (sum >> 16) + (sum & 0xffff);	/* add hi 16 to low 16 */
    sum += (sum >> 16);			/* add carry */
    answer = ~sum;			/* truncate to 16 bits */
    return (answer);
}


/* clear the input queue of a socket */
int					/* <0 on error */
clear_soc(int soc)			/* this socket */
{
    int i;
    struct timeval tout;
    fd_set ready;
    char buf[1024];

    for (;;) {
	FD_ZERO(&ready);
	FD_SET(soc, &ready);
	tout.tv_sec = tout.tv_usec = 0;
	i = select(soc+1, &ready, (fd_set *)0, (fd_set *)0, &tout);
	if (i < 0) {
	    if (errno == EINTR)
		continue;
	    log_debug(LOG_ERR, "select(ICMP clear): %s", strerror(errno));
	    return -1;
	}
	if (i == 0)
	    return 0;

	i = recv(soc, &buf[0], sizeof(buf), 0);
	if (i < 0) {
	    log_debug(LOG_ERR, "recv(clear_soc): %s", strerror(errno));
	    return -1;
	}
    }
}


#ifdef NOTDEF
double
float_time(struct timeval *tp)
{
    return (double)tp->tv_sec + tp->tv_usec/F_USEC_PER_SEC;
}
#else
#define float_time(tp) ((double)(tp)->tv_sec + (tp)->tv_usec/F_USEC_PER_SEC)
#endif


static void
set_tval(struct timeval *tp,		/* create time structure */
	 double f_sec)			/* from this number of seconds */
{
    f_sec = rint(f_sec * USEC_PER_SEC);

    tp->tv_sec = f_sec/USEC_PER_SEC;
    tp->tv_usec = f_sec - (tp->tv_sec * F_USEC_PER_SEC);
}


/* compute the current drift */
double					/* predicted seconds of drift */
drift(double delta)			/* for this many ticks */
{
    double ndrift;

    if (drift_ticks >= DRIFT_TICKS) {	/* age the old drift value */
	    drift_ticks -= drift_ticks/16;
	    drift_err -= drift_err/16;
    }

    ndrift = drift_err/drift_ticks;
    if (ndrift < neg_max_drift)		/* do not go crazy */
	ndrift = neg_max_drift;
    else if (ndrift > max_drift)
	ndrift = max_drift;

    return ndrift*delta;
}


/* guess at a measurement */
void
guess_adj(struct timeval *adjp)		/* put adjustment here */
{
    struct tms tm;
    double cur_ticks;
    double cur_drift;
    double tmp;

    cur_ticks = times(&tm);
    cur_drift = drift(cur_ticks-prev_drift_ticks);
    prev_drift_ticks = cur_ticks;

    prev_drift += cur_drift;
    tmp = chgs[cur_chg];
    experr -= tmp;
    tmp += cur_drift;
    set_tval(adjp, tmp);

    if (debug != 0)
	log_debug(LOG_DEBUG, "guess %.3f%+.3f=%.3f",
		  cur_drift*1000.0, chgs[cur_chg]*1000.0,
		  float_time(adjp)*1000.0);

    chgs[cur_chg] = 0;
    cur_chg = (cur_chg+1) % MAX_CDAMP;
    skip++;
}


long min_idelta, min_odelta, min_rdelta;    /* preferred differences */
long diffs, tot_idelta, tot_odelta, tot_rdelta;	/* for averages */

/* accumulate a difference
 */
static double				/* return sec of difference */
diffaccum(long rcvtime,			/* when it got back here */
	  u_long ttime,			/* received by other guy */
	  u_long rtime,			/* sent by the other guy */
	  u_long otime,			/* when it was sent by us */
	  u_short seq)			/* packet sequence # */
{
    long idelta, odelta, rdelta;
    double diff;

    rdelta = rcvtime-otime;		/* total round trip */
    idelta = rcvtime-ttime;		/* apparent return time */
    odelta = rtime-otime;		/* apparent tranmission time */

    /* do not be confused by midnight */
    if (idelta < -MSECDAY/2) idelta += MSECDAY;
    else if (idelta > MSECDAY/2) idelta -= MSECDAY;

    if (odelta < -MSECDAY/2) odelta += MSECDAY;
    else if (odelta > MSECDAY/2) odelta -= MSECDAY;

    if (rdelta > MSECDAY/2) rdelta -= MSECDAY;

    tot_idelta += idelta;
    tot_odelta += odelta;
    tot_rdelta += rdelta;

    diff = (odelta - idelta)/2000.0;

    if (idelta < min_idelta)
	min_idelta = idelta;
    if (odelta < min_odelta)
	min_odelta = odelta;
    if (rdelta < min_rdelta)
	min_rdelta = rdelta;

    /* Compute real the round trip delay to the server.
     * Shrink the computed value quickly.
     */
    if (rdelta < avg_round*(.75*1000.0)) {
	avg_round = rdelta/1000.0;
	clr_round = 0;
    }
    if (rdelta < avg_round*(2*1000.0)) {
	avg_round = (rdelta/1000.0 + avg_round*7)/8;
    }

    if (debug > 2)
	log_debug(LOG_DEBUG, "#%d rt%-+5d out%-+5d back%-+5d  %+.1f",
		  seq, rdelta, odelta, idelta, diff*1000.0);

    return ((tot_odelta-tot_idelta)/2000.0)/diffs;
}



/* Measure the difference between our clock and some other machine,
 *	using ICMP timestamp messages.
 */
int					/* <0 on trouble */
secdiff(int rawsoc,
	int sndsoc,
	double *diffp)			/* signed sec difference */
{
    static short seqno = 1;

    fd_set ready;
    int i;
    int sends, waits;
    double diff, rt_excess, excess_err;
    short seqno0;
    struct timeval sdelay, prevtv, tv;
    struct sockaddr_in from;
    int fromlen;
    struct {
	    struct ip ip;
	    struct icmp icmp;
    } opkt;
    time_t recvtime;
    union {
	char b[1024];
	struct ip ip;
    } ipkt;
    struct icmp *ipp;


    bzero(&opkt,sizeof(opkt));
    opkt.ip.ip_dst = masaddr.sin_addr;
    opkt.ip.ip_tos = IPTOS_LOWDELAY;
    opkt.ip.ip_p = IPPROTO_ICMP;
    opkt.ip.ip_len = sizeof(opkt);
    opkt.ip.ip_ttl = MAXTTL;
    opkt.icmp.icmp_type = ICMP_TSTAMP;	/* make an ICMP packet to send */
    opkt.icmp.icmp_id = getpid();
    if (seqno > 16000)			/* keep the sequence number */
	seqno = 1;			/*	from wrapping */
    seqno0 = seqno-1;

    *diffp = 0.0;

    if (clear_soc(rawsoc) < 0)		/* empty the input queue */
	return -1;

    sginap(1);				/* start at clock tick */

    min_idelta = MSECDAY;
    min_odelta = MSECDAY;
    min_rdelta = MSECDAY;
    tot_idelta = 0;
    tot_odelta = 0;
    tot_rdelta = 0;
    diffs = 0;
    sends = 0;
    while (sends < icmp_max_sends && diffs < icmp_min_diffs) {

	(void)gettimeofday(&tv, 0);

	if (sends < icmp_max_sends) {	/* limit total ICMPs sent */
	    sends++;
	    opkt.icmp.icmp_seq = seqno++;
	    opkt.icmp.icmp_otime = htonl((tv.tv_sec % SECDAY)*1000
					 + (tv.tv_usec+500)/1000);
	    opkt.icmp.icmp_cksum = 0;
	    opkt.icmp.icmp_cksum = in_cksum((u_short *)&opkt.icmp,
					    sizeof(opkt.icmp));
	    i = send(sndsoc, &opkt, opkt.ip.ip_len, 0);
	    if (i != opkt.ip.ip_len) {
		if (i < 0)
		    log_once(&icmp_logged, "send(ICMP): %s", strerror(errno));
		else
		    log_once(&icmp_logged, "send(ICMP): result=%d", i);
		return -1;
	    }
	}

	/* get ICMP packets until we get the right one */
	prevtv = tv;
	for (;;) {
	    sdelay.tv_sec = icmp_wait + prevtv.tv_sec - tv.tv_sec;
	    sdelay.tv_usec = prevtv.tv_usec - tv.tv_usec;
	    fix_time(&sdelay);
	    if (sdelay.tv_sec < 0) {
		sdelay.tv_usec = 0;
		sdelay.tv_sec = 0;
	    }
	    ipp = 0;
	    FD_ZERO(&ready);
	    FD_SET(rawsoc, &ready);
	    i = select(rawsoc+1, &ready, (fd_set *)0, (fd_set *)0, &sdelay);
	    (void)gettimeofday(&tv, 0);
	    if (i < 0) {
		if (errno == EINTR)
		    continue;
		log_once(&icmp_logged, "select(ICMP): %s", strerror(errno));
		return -1;
	    }
	    if (i == 0)			/* send after waiting a while */
		break;

	    fromlen = sizeof(from);
	    if (0 > recvfrom(rawsoc,(char*)&ipkt,sizeof(ipkt),
			     0, &from,&fromlen)) {
		log_once(&icmp_logged, "recvfrom(ICMP): %s", strerror(errno));
		return -1;
	    }

	    /* accept replies to any of the packets we have sent */
	    ipp = (struct icmp *)&ipkt.b[ipkt.ip.ip_hl<<2];
	    if ((ipp->icmp_type != ICMP_TSTAMPREPLY)
		|| ipp->icmp_id != opkt.icmp.icmp_id
		|| ipp->icmp_seq > seqno
		|| ipp->icmp_seq < seqno0)
		continue;

	    recvtime = ((tv.tv_sec % SECDAY)*1000
			+ (tv.tv_usec+500)/1000);

	    ipp->icmp_otime = ntohl(ipp->icmp_otime);
	    ipp->icmp_rtime = ntohl(ipp->icmp_rtime);
	    ipp->icmp_ttime = ntohl(ipp->icmp_ttime);
	    if (recvtime < ipp->icmp_otime)
		continue;		/* do not worry about midnight */

	    diffs++;
	    diff = diffaccum(recvtime, ipp->icmp_ttime,
			     ipp->icmp_rtime, ipp->icmp_otime,
			     ipp->icmp_seq - seqno0);

	    /* exchange timestamps as fast as the other side is willing.
	     */
	    if (ipp->icmp_seq == seqno-1)
		break;
	}
    }
    if (diffs < icmp_min_diffs) {
	if (!quiet || debug > 0) {
	    log_once(&icmp_logged, "%d responses in %d trials from %s",
		     diffs, sends, masname);
	}
	return -1;
    }

    /* if the round trip time for the measurement is more than 25% high,
     * and if the error is unexpectedly large by a similar amount,
     * then assume the path delay has become asymmetric because of
     * asymmetric loads, and adjust the computed difference by the inferred
     * asymmetry.
     *
     * The asymmetry biases the measurement by half of its value.
     *
     * If the current estimate of the minmum round trip time is persistently
     * exceeded, assume the route has changed, and start looking for a new
     * round trip time.
     */
    rt_excess = (tot_rdelta/1000.0)/diffs - avg_round;
    excess_err = diff - experr;
    if (rt_excess <= avg_round/4+MIN_ROUND) {
	clr_round = 0;

    } else if (abs(excess_err) < rt_excess/2) {
	clr_round = 0;
	if (excess_err > 0)
	    diff -= rt_excess/2;
	else
	    diff += rt_excess/2;
	if (debug > 0)
	    log_debug(LOG_DEBUG, "trim asymmetry of %+.1f to get %+.1f",
		      rt_excess*1000.0/2, diff*1000.0);

    } else if (++clr_round > 5) {
	avg_round = SECDAY;
	clr_round = 0;
	if (debug > 0)
	    log_debug(LOG_DEBUG, "resetting minimum round trip measure");
    }


    if (debug > 1)
	log_debug(LOG_DEBUG, "error=%+.1f  %3d values  %2d trials",
		  diff*1000.0, diffs, sends);

    if (icmp_logged) {			/* after a successful measurement, */
	icmp_logged = 0;		/*	complain again */
	syslog(LOG_ERR, "Time measurements from %s are working again",
	       masname);
    }
    *diffp = diff;
    return 0;
}



/* compute the difference between our date and the masters.
 */
double					/* return error in seconds */
daydiff(int udpsoc)
{
    int i;
    int trials;
    struct timeval tout;
    fd_set ready;
    struct sockaddr_in from;
    int fromlen;
    double now;
    u_long buf;
    int buf_year;

    if (clear_soc(udpsoc) < 0)		/* get rid of drek */
	return 0.0;

    tout.tv_sec = date_wait;
    tout.tv_usec = 0;
    for (trials = 0; trials < MAX_TIME_TRIALS; trials++) {
	/* ask for the time */
	buf = 0;
	if (send(udpsoc, (char *)&buf, sizeof(buf), 0) < 0) {
	    log_once(&udp_logged, "send(udpsoc): %s", strerror(errno));
	    return 0.0;
	}

	for (;;) {
	    FD_ZERO(&ready);
	    FD_SET(udpsoc, &ready);
	    i = select(udpsoc+1, &ready, (fd_set *)0, (fd_set *)0, &tout);
	    if (i < 0) {
		if (errno == EINTR)
		    continue;
		log_debug(LOG_ERR, "select(date read): %s", strerror(errno));
		return 0.0;
	    }
	    if (0 == i)
		break;

	    fromlen = sizeof(from);
	    errno = 0;
	    i = recvfrom(udpsoc,(char*)&buf,sizeof(buf),0, &from,&fromlen);
	    if (sizeof(buf) != i) {
		log_debug(LOG_ERR,
			  "recvfrom(date read)=%d: %s", i, strerror(errno));
		return 0.0;
	    }

	    /* See what year it says it is, after converting to the UNIX epoch,
	     * adjusted for leap years.
	     */
	    NTOHL(buf);
	    buf_year = (buf - ((buf/SECYR)/4)*SECDAY)/SECYR;

	    if (set_year != 0) {
		if (buf_year == set_year || buf_year == set_year2) {
		    udp_year_logged = 0;
		    /* after good data, let the set year roll forward */
		    fix_set_year(buf-BU);
		} else {
		    log_once(&udp_year_logged, "ignoring the year %u from %s",
			     buf_year+1900, masname);
		    buf -= (buf_year*SECYR + buf_year/4*SECDAY);
		    buf += (set_year*SECYR + set_year/4*SECDAY);
		}

	    } else {
		if (buf_year < 90 || buf_year > 200) {
		    log_once(&udp_year_logged,
			     "ignoring %s because it says the year is %u",
			     masname, buf_year+1900, buf);
		    return 0.0;
		}
		udp_year_logged = 0;
	    }

	    if (udp_logged) {
		udp_logged = 0;
		log_debug(LOG_ERR,
			  "Date measurements from %s are working again",
			  masname);
	    }
	    return now = (double)buf - (double)BU - (double)time(0);
	}
    }

    /* if we get here, we tried too many times */
    if (!quiet || debug > 0)
	log_once(&udp_logged, "%s will not tell us the date", masname);
    return 0.0;
}



/* Compute a new adjustment for our clock.  The new adjustment
 *  should be chosen so that the next time we check, the difference
 *  between our clock and the masters will be zero.  Unfortunately,
 *  the measurement of the differences between our clocks is affected
 *  by systematic errors.  These errors are produced by intervals when
 *  the network delays to the master and back are asymmetric.
 *
 * If the error is much larger than the maximum network delay, we
 *  should not try to damp the change.  The master might have reset its
 *  clock.
 *
 * We should not try to damp the change very much until we have a good
 *  baseline.
 *
 * Large but isolated measured errors should be ignored.
 *
 * We want to choose adjustments so that each measured delta is close
 *  to zero.  That is, we should predict the drift for the next period
 *  and adjust the clock for that as well as for the currently measured
 *  error.
 *
 * The adjustment we compute should completely fix any error attributed
 *  to the long term drift of the local clock.  We should correct only part
 *  of rest of the error.
 */


/* smooth a measurement
 */
static void
smoothadj(struct timeval *adjp,		/* put adjustment here */
	  double err)			/* error in sec */
{
    struct tms tm;
    double cur_ticks;			/* current time */
    double chg;				/* change in error since last time */
    double abschg;			/* change in error in sec/tick */
    double old_experr;
    double delta_ticks;
    double cur_drift;
    double tmp;
    int i;
    double *fp;


    chg = err - experr;			/* get change in error */
    old_experr = experr;
    cur_ticks = times(&tm);
    if (++pass == 1)
	prev_drift_ticks = prev_ticks = cur_ticks-1;
    delta_ticks = cur_ticks - prev_ticks;
    abschg = fabs(chg)/delta_ticks;	/* get scaled absolute value */
    if (pass <= 2)
	varerr = abschg;

    cur_drift = drift(cur_ticks-prev_drift_ticks);
    prev_drift_ticks = cur_ticks;

    /* After we have some good data, discard any measurement that
     * would change the average error too much.  This is because
     * the measurements are aflicted by periods when the transmission
     * delays are asymmetric.  This can come from asymmetric loads
     * on a SLIP line, or from scheduling delays for this process.
     * The former can cause errors of either sign, but the latter
     * causes only negative errors.
     *
     * Notice the binary exponential hack.  One good reading counts
     * against lots of bad ones.
     */

    /* get limit on variation, always > 1 tick */
    tmp = varerr*1.5 + (1.0/HZ)/delta_ticks;

    if (++skip > DISCARD_MEAS
	&& abschg > tmp			/* enough wildness to open filter */
	&& !measure_only) {
	if (debug != 0)
	    log_debug(LOG_DEBUG, "opening filter for %+1.f", err*1000.0);
	varerr = skip_varerr;
	tmp = abschg;
	skip = 1;
    }


    if (abschg <= tmp
	|| measure_only
	|| pass <= INIT_PASSES) {
	if (abschg > (MAXADJ*1.0)/HZ) {	/* handle changes abruptly */
	    if (pass > INIT_PASSES) {
		pass = INIT_PASSES+1;
	    } else {
		pass--;
	    }
	    if (debug != 0)
		log_debug(LOG_DEBUG, "clearing smoother for change of %+1.f",
			  chg*1000.0);
	    bzero((char*)&chgs[0], sizeof(chgs));
	    experr = 0;
	    tmp = err;

	} else {			/* average ordinary errors */
	    if (pass > 2) {
		varerr += (abschg-varerr)/VDAMP;
		if (varerr <= abschg)
		    varerr = abschg;
		drift_ticks += delta_ticks;
		drift_err += chg+prev_drift;
		/* The "chg+" term cancels the bad coupling from the changing
		 * of the drift to the next change.  Otherwise, it would
		 * oscillate.
		 */
	    }
	    i = min(chg_smooth, pass);
	    tmp = measure_only ? 0 : (chg / i);
	    fp = &chgs[cur_chg];
	    do {
		*fp += tmp;
		if (++fp >= &chgs[MAX_CDAMP])
		    fp = &chgs[0];
	    } while (--i > 0);
	    tmp = chgs[cur_chg];
	    chgs[cur_chg] = 0;
	    cur_chg = (cur_chg+1) % MAX_CDAMP;
	    experr = err-tmp;
	}
	if (!measure_only)
		tmp += cur_drift;
	skip_varerr = varerr;
	prev_drift = cur_drift;
	prev_ticks = cur_ticks;
	skip /= 2;

    } else {
	if (debug != 0)
	    log_debug(LOG_DEBUG, "skip error %+.1f", err*1000.0);
	if (skip_varerr <= abschg) {
		skip_varerr = abschg;
	} else {
		skip_varerr += (abschg-skip_varerr)/VDAMP;
	}
	prev_drift += cur_drift;
	tmp = chgs[cur_chg];
	chgs[cur_chg] = 0;
	cur_chg = (cur_chg+1) % MAX_CDAMP;
	experr -= tmp;
	tmp += cur_drift;
    }
    set_tval(adjp, tmp);


    if (measure_only) {
	if (!headings)
	    log_debug(LOG_DEBUG, "%10s %7s %7s %6s",
		      "", "err", "exp-err", "drift");
	log_debug(LOG_DEBUG, "%10s %+7.1f %+7.1f %+6.3f",
		  masname, err*1000.0, old_experr*1000.0, drift(HZ*1000));
    } else if (debug != 0) {
	if (!headings)
	    log_debug(LOG_DEBUG, "%7s %7s %7s %6s %4s",
		      "err", "exp-err", "adj", "drift", "var");
	log_debug(LOG_DEBUG, (pass<=INIT_PASSES
			      ?	      "%+7.1f %+7.1f %+7.2f %+6.3f %4.2f %x"
			      :	      "%+7.1f %+7.1f %+7.2f %+6.3f %4.2f"),
		  err*1000.0,
		  old_experr*1000.0,
		  float_time(adjp)*1000.0,
		  drift(HZ*1000),
		  varerr*(HZ*1000.0),
		  pass);
    }
    if (++headings >= 20)
	headings = 0;
}


/* get a smoothed difference from the network
 */
static int				/* -1=failed */
get_netadj(int rawsoc,
	   int sndsoc,
	   int udpsoc,			/* use these sockets */
	   struct timeval *adjp)	/* put adjustment here */
{
    double secs;			/* error in seconds */


    /* Check the date with master.
     * If within 12 hours, use ICMP timestamps to compute the delta.
     * If not, just change the time.
     */
    secs = daydiff(udpsoc);
    if (secs < -SECDAY/2 || secs > SECDAY/2) {
	adjp->tv_sec = secs;
	adjp->tv_usec = 0;
	if (debug != 0)
	    log_debug(LOG_DEBUG, "   chg %+.3f sec", float_time(adjp));
	return -1;
    }

    if (secdiff(rawsoc,sndsoc,&secs) < 0) {
	guess_adj(adjp);
	return -1;
    }

    smoothadj(adjp,secs);		/* smooth the answer */
    return 0;
}


/* convert string to printable characters
 */
static char *
toascii(void *v, int len)
{
#   define NUM_BUFS 2
    static struct {
	char b[80*3+1];
    } bufs[NUM_BUFS];
    static int buf_num;
    char *p0, *p;
    u_char *s2, c;
    u_char *s = v;

    if (len < 0)
	len = strlen(v);

    p0 = bufs[buf_num].b;
    buf_num = (buf_num+1) % NUM_BUFS;
    for (p = p0; len != 0 && p < &p0[sizeof(bufs[0].b)-1]; len--) {
	c = *s++;
	if (c == '\0') {
	    for (s2 = s+1; s2 < &s[len]; s2++) {
		if (*s2 != '\0')
		    break;
	    }
	    if (s2 >= &s[len])
		goto exit;
	}

	if (c >= ' ' && c < 0x7f && c != '\\') {
	    *p++ = c;
	    continue;
	}
	*p++ = '\\';
	switch (c) {
	case '\\':
	    *p++ = '\\';
	    break;
	case '\n':
	    *p++= 'n';
	    break;
	case '\r':
	    *p++= 'r';
	    break;
	case '\t':
	    *p++ = 't';
	    break;
	case '\b':
	    *p++ = 'b';
	    break;
	default:
	    p += sprintf(p,"0%o",c);
	    break;
	}
    }
exit:
    *p = '\0';
    return p0;
#undef NUM_BUFS
}


/* Send a string to a clock and get a response.
 *	This code should not daudle,
 */
static int				/* <0 if failed */
ask_clock(int dev_fd,
	  char *qstring,
	  u_char *ans,			/* buffer */
	  int alen,			/* desired length=length-1 of buffer */
	  struct timeval *now,
	  int slowness)			/* ms of extra delay */
{
    int qlen, i, j, k;
    time_t total_usec;
    struct timeval start, delta, tout;
    fd_set ready;


    qlen = strlen(qstring);
    total_usec = (qlen+alen-1)*USEC_PER_C + slowness*1000;

    for (k = 0; k < 10; k++) {
	FD_ZERO(&ready);

	/* send the message to the clock */
	if (0 > ioctl(dev_fd, TCFLSH, 2)) {
	    log_debug(LOG_ERR, "ioctl(%s, TCFLSH): %s",
		      masname, strerror(errno));
	    return -1;
	}
	i = write(dev_fd, qstring, qlen);
	(void)gettimeofday(&start, 0);

	if (i != qlen) {
	    if (i < 0)
		log_debug(LOG_ERR, "write(%s): %s", masname, strerror(errno));
	    else
		log_debug(LOG_ERR, "write(%s)==%d", masname, i);
	    return -1;
	}

	/* read the answer
	 *	Take several tries as necessary, so that we get all we want
	 *	and do not wait forever.
	 *
	 *	We must use select(2) because VMIN>0 says to wait one
	 *	for at least one character, and we don't want to die if
	 *	a character gets lost.
	 */
	i = 0;
	do {
	    FD_SET(dev_fd, &ready);
	    tout.tv_sec = 0;
	    tout.tv_usec = total_usec;
	    j = select(dev_fd+1, &ready, 0,0, &tout);
	    if (j < 0) {
		if (errno == EINTR)
		    continue;
		log_debug(LOG_ERR, "select(%s): %s", masname, strerror(errno));
		return -1;
	    }

	    if (j == 0) {
		(void)gettimeofday(&delta, 0);
		delta.tv_sec -= start.tv_sec;
		delta.tv_usec -= start.tv_usec;
		fix_time(&delta);
		/* wait some more if it has not been too long yet */
		if (delta.tv_sec == 0 && delta.tv_usec <= total_usec)
		    continue;
		break;
	    }

	    j = read(dev_fd, &ans[i], alen-i);
	    if (j < 0) {
		log_debug(LOG_ERR, "read(%s): %s", masname, strerror(errno));
		return -1;
	    }
	    i += j;
	    ans[i] = '\0';
	} while (i < alen);
	(void)gettimeofday(&delta, 0);
	/* the preceding was time-critical.  From now on we deal with
	 *	deltas, and so do not have to worry.
	 */

	if (i == 0) {
	    log_once(&clock_logged, "%s did not respond to \"%s\"",
		     masname, toascii(qstring,qlen));
	    continue;
	}

	/* Require that we finished soon after the clock finished.
	 */
	if (now)
		*now = delta;
	delta.tv_sec -= start.tv_sec;
	delta.tv_usec -= start.tv_usec;
	delta.tv_usec -= (qlen+alen-1)*USEC_PER_C;
	fix_time(&delta);
	if (i != alen) {
	    log_once(&clock_logged, "instead of %d, read %d bytes=\"%s\"",
		     alen, i, toascii(ans, i));
	    continue;
	}

	if (delta.tv_usec > total_usec) {
	    log_once(&clock_logged,
		     "%s took %.1f msec to respond to \"%s\"",
		     masname,
		     float_time(&delta)*1000.0, toascii(qstring,qlen));
	    continue;
	}

	if (debug > 2) {
	    log_debug(LOG_DEBUG, "%s responded to \"%s\" in %.1f msec",
		      masname,
		      toascii(qstring, qlen), float_time(&delta)*1000.0);
	}
	return 0;
    }

    return -1;
}


/* Get a smoothed difference from WWV using the Precision Time Standard
 *	(Traconix) WWV receiver.
 */
static int				/* -1=failed */
get_wwvadj(int dev_fd,
	   struct timeval *adjp)	/* put adjustment here */
{
    int i;
    struct timeval now;
    int msec, yy;
    time_t tclock;
    struct {				/* bytes from the WWV receiver */
	u_char ok;			/*	for a 'QA' command */
	u_char pm;
	u_char ms_hi;
	u_char ms_lo;
	u_char ss;
	u_char mm;
	u_char hh;
	u_char dd_hi;
	u_char dd_lo;
	u_char yy;
	u_char resvd[2];
	u_char cr;
	u_char pad;
    } ans;

    /* ask for the time and date
     * Start the measurement at a clock tick when our notion of the time
     *	is most accurately represented by the values in the kernel.
     */
    sginap(1);
    i = ask_clock(dev_fd, "QA0000", (u_char *)&ans, sizeof(ans)-1, &now,20);
    if (i < 0) {
	guess_adj(adjp);
	return -1;
    }

    /* validate the answer */
    if ((ans.ok & 0xf8) != 0x08
	|| ans.cr != '\r'
	|| ans.pm != 0) {
	log_debug(LOG_ERR, "read %d nonsense bytes=\"%s\"",
		  i, toascii(&ans,i));
	guess_adj(adjp);
	return -1;
    }

    /* Decode the answer.
     * The manual does not say, but people at Precision Time Standards
     * have said that the time reported by the clock is correct at the
     * last bit of the message.
     */
    msec = ((ans.ms_hi/2) << 6) + ans.ms_lo/2;
    yy = ans.yy/2 + 86-70;
    if (set_year != 0) {
	if (yy == set_year-70 || yy == set_year2-70) {
	    wwv_year_logged = 0;
	} else {
	    log_once(&wwv_year_logged, "year in the WWV receiver is %d",
		     yy+1970);
	    yy = set_year-70;
	}
    }
    tclock = yy*SECYR-SECDAY;		/* count seconds since the epoch */
    tclock += ((yy+1)/4)*SECDAY;	/*	until first of the year */

    tclock += (((ans.dd_hi/2) << 6) + ans.dd_lo/2)*SECDAY;
    tclock += (ans.hh/2)*SECHR;
    tclock += (ans.mm/2)*60;
    tclock += ans.ss/2;

    if (!wwv_year_logged)
	fix_set_year(tclock);

    /* compute the error */
    adjp->tv_sec = tclock - now.tv_sec;
    adjp->tv_usec = msec*1000 - now.tv_usec;
    fix_time(adjp);
    if (debug > 1)
	log_debug(LOG_DEBUG, "delta=%+1.f msec",
		  adjp->tv_sec*1000.0 + adjp->tv_usec/1000.0);

    /* If we are off by a multiple of years, and it is near the first of the
     *	    year, assume the clock is wrong.  Assume someone forget to
     *	    change the year jumpers in the clock or changed the jumpers
     *	    too soon.
     */
    if (set_year == 0) {
	if (adjp->tv_sec <= -(SECYR-SECDAY)
	    && (-adjp->tv_sec) % SECYR < SECDAY
	    && (now.tv_sec % SECYR < 14*SECDAY
		|| now.tv_sec % SECYR > 360*SECDAY)) {
	    log_once(&wwv_year_logged, "year in the WWV receiver is %d",
		     yy+1970);
	    adjp->tv_sec %= SECDAY;
	} else {
	    wwv_year_logged = 0;
	}
    }

    /* if too much to smooth, simply set the time */
    if (adjp->tv_sec <= -MAXADJ
	|| adjp->tv_sec >= MAXADJ) {
	if (debug != 0 || measure_only)
	    log_debug(LOG_DEBUG, "   chg %+.3f", float_time(adjp));
	return 0;
    }

    if (clock_logged) {			/* after a successful measurement, */
	clock_logged = 0;		/*	resume complaining */
	log_debug(LOG_ERR, "The clock is working again");
    }

    smoothadj(adjp,			/* smooth the answer */
	      adjp->tv_sec*1.0 + adjp->tv_usec/F_USEC_PER_SEC);
	 return 0;
}



static int
cmd_tt600(int dev_fd, char *cmd, char *expect)
{
#   define OK "OK\r\n"
    u_char ans[100];
    int i = strlen(expect);

    if (0 > ask_clock(dev_fd, cmd, ans, MIN(sizeof(ans)-1, i), 0,50))
	return -1;

    if (strcmp((char *)ans, expect)) {
	log_once(&clock_logged, "GPS clock said \"%s\" instead of \"%s\"",
		 toascii(ans,-1), toascii(expect,-1));
	return -1;
    }

    return 0;
#undef OK
}


/* Get a smoothed difference from TrueTime Model 600 GPS receiver.
 */
static int				/* -1=failed */
get_tt600_adj(int dev_fd,
	      struct timeval *adjp)	/* put adjustment here */
{
    int i;
    struct timeval now;
    u_long year, day, hh, mm, ss, msec;
    time_t tclock;
#   define RESYNC "ERROR 05 NO SUCH FUNCTION\r\n"
    u_char year_str[sizeof("F68 1997\r\n")];
#   define GPS_FMT "\01DDD:HH:MM:SS.MMMQ\r\n"
    u_char date[sizeof(GPS_FMT)];

    /* First turn off any previous mode, select UTC, 24-hour mode,
     * and clear the format string.
     * You cannot send more than one command at a time.  You must
     * let it say "OK".
     */
    (void)write(dev_fd, "\03", 1);
    sginap(2);
    if (0 > cmd_tt600(dev_fd, "\03\r", RESYNC)) {
	guess_adj(adjp);
	return -1;
    }
    /* Do not write the NVRAM on the clock more often than necessary
     */
    if ((cmd_tt600(dev_fd, "f69\r", "F69 UTC     \r\n")
	 && cmd_tt600(dev_fd, "f69utc\r", "OK\r\n"))
	|| (0 > cmd_tt600(dev_fd, "f02\r", "F02 24\r\n")
	    && 0 > cmd_tt600(dev_fd, "f0224\r", "OK\r\n"))
	|| (0 > cmd_tt600(dev_fd, "f11\r", "F11 \r\n")
	    && 0 > cmd_tt600(dev_fd, "f11 \r", "OK\r\n"))) {
	guess_adj(adjp);
	return -1;
    }

    /* Then ask for the year, and then day and time */
    if (0 > ask_clock(dev_fd, "f68\r", year_str, sizeof(year_str)-1, 0,50)) {
	guess_adj(adjp);
	return -1;
    }
    if (1 != sscanf((char*)year_str, "F68 %4u\r", &year) || year < 1997) {
	log_once(&clock_logged, "GPS clock says \"%s\" instead of"
		 " \"F68 year\"", toascii(year_str,-1));
	guess_adj(adjp);
	return -1;
    }
    year -= 1970;

    /* The clock is slow to respond to F09, and in fact says nothing.
     * So ask for the time first without caring about how quickly
     * it responds and then a second time for real.
     */
    if (0 > ask_clock(dev_fd, "f09\rT", date, sizeof(date)-1, 0,50)) {
	guess_adj(adjp);
	return -1;
    }
    /*  Start the measurement at a clock tick when our notion of the time
     *	is most accurately represented by the values in the kernel.
     */
    sginap(1);
    (void)gettimeofday(&now, 0);
    if (0 > ask_clock(dev_fd, "T", date, sizeof(date)-1, 0,50)) {
	guess_adj(adjp);
	return -1;
    }

    /* decode it */
    if (5 != sscanf((char *)date, "\01%3u:%2u:%2u:%2u.%3u[ .*#?][\r][\n]",
		    &day, &hh, &mm, &ss, &msec)
	|| day > 366 || day == 0
	|| hh >= 24
	|| mm >= 60
	|| ss >= 60
	|| msec >= 1000) {
	log_once(&clock_logged, "GPS clock says \"%s\" instead of \"%s\"",
		 toascii(date,-1), toascii(GPS_FMT,-1));
	guess_adj(adjp);
	return -1;
    }

    /* try again if close to midnight to avoid confusion with the
     * end of the year or leap seconds.
     */
    if (hh == 0 && mm == 0 && ss < 2) {
	if (debug)
	    log_debug(LOG_DEBUG, "delay until after away from midnight");
	guess_adj(adjp);
	return -1;
    }

    tclock = ((day*24 + hh)*60 + mm)*60 + ss;
    tclock += year*SECYR-SECDAY;
    tclock += ((year+1)/4)*SECDAY;

    fix_set_year(tclock);

    /* Compute the error.
     * The manual says the time is captured when the "T" is received.
     */
    adjp->tv_sec = tclock - now.tv_sec;
    adjp->tv_usec = msec*1000 - now.tv_usec + USEC_PER_C;
    fix_time(adjp);
    if (debug > 1)
	log_debug(LOG_DEBUG, "delta=%+1.f msec",
		  adjp->tv_sec*1000.0 + adjp->tv_usec/1000.0);

    /* if too much to smooth, simply set the time */
    if (adjp->tv_sec <= -MAXADJ
	|| adjp->tv_sec >= MAXADJ) {
	if (debug != 0 || measure_only)
	    log_debug(LOG_DEBUG, "   chg %+.3f", float_time(adjp));
	return 0;
    }

    if (clock_logged) {			/* after a successful measurement, */
	clock_logged = 0;		/*	resume complaining */
	log_debug(LOG_ERR, "The clock is working again");
    }

    smoothadj(adjp,			/* smooth the answer */
	      adjp->tv_sec*1.0 + adjp->tv_usec/F_USEC_PER_SEC);
    return 0;

#undef QUERY
}



/* adjust the clock
 */
int					/* delay this long afterwards */
adjclock(int rawsoc,			/* use these sockets */
	 int sndsoc,
	 int udpsoc,
	 int dev_fd)			/* or this file descriptor */
{
    static int max_fuzz;
    static int passes = 0;
    struct timeval now, adj;
    int meas_ok;
    clock_t rep;
    double delta;
    struct tms tm;
    double cur_ticks;


    /* restore complains about sick server once a day */
    (void)gettimeofday(&now, 0);
    if (now.tv_sec % SECDAY < SECHR/4
	&& log_ticks - times(&tm) > SECDAY*HZ/2) {
	log_ticks = times(&tm);
	morelogging();
    }

    /* choose a random time to try again later
     * to try to keep a gaggle of clients from asking simultaneously
     */
    if (passes < INIT_PASSES) {
	if (passes < 1) {
	    srandom((unsigned)(now.tv_sec + now.tv_usec));
	    max_fuzz = rep_rate/10;
	    if (max_fuzz < 2)
		max_fuzz = 2;
	}
	/* Choose the speed at which to correct the clock
	 * Always get at least 25% of the error when listening to the radio.
	 * Get it all done within 60 minutes.
	 */
	chg_smooth = (SECHR*HZ)/rep_rate;
	if (chg_smooth < 1)
	    chg_smooth = 1;
	if (mode != MODE_ICMP && chg_smooth > 4)
	    chg_smooth = 4;
	if (chg_smooth > MAX_CDAMP)
	    chg_smooth = MAX_CDAMP;
	rep = (rep_rate > INIT_REP_RATE) ? INIT_REP_RATE : rep_rate;
    } else {
	rep = rep_rate + (random() % max_fuzz);
    }

    switch (mode) {			/* get an adjustment */
    case MODE_ICMP:
	meas_ok = get_netadj(rawsoc,sndsoc,udpsoc, &adj);
	break;
    case MODE_PST_WWV:
	meas_ok = get_wwvadj(dev_fd, &adj);
	break;
    case MODE_TT600:
	meas_ok = get_tt600_adj(dev_fd, &adj);
	break;
    default:
	abort();
    }
    if (meas_ok >= 0)
	passes++;

    if (!measure_only) {
	delta = adj.tv_sec*F_USEC_PER_SEC + adj.tv_usec;

	/* If our time is far from the masters, smash it.
	 */
	if (adj.tv_sec <= -MAXADJ || adj.tv_sec >= MAXADJ
	    || (adj.tv_sec != 0 && passes < INIT_PASSES)) {
	    char olddate[32];
	    (void)gettimeofday(&now, 0);
	    (void)cftime(olddate, "%D %T", &now.tv_sec);
	    now.tv_sec += adj.tv_sec;
	    now.tv_usec += adj.tv_usec;
	    fix_time(&now);
	    if (cap_settimeofday(&now,0) < 0) {
		log_debug(LOG_ERR, "settimeofday(): %s", strerror(errno));
		exit(1);
	    }
	    log_debug(LOG_NOTICE, "date changed from %s", olddate);

	} else {
	    /* always adjust the clock to keep the TOD chip out of it
	     */
	    if (cap_adjtime(&adj, 0) < 0) {
		log_debug(LOG_ERR, "adjtime(): %s", strerror(errno));
		exit(1);
	    }
	}

	cur_ticks = times(&tm);
	if (passes <= INIT_PASSES) {
		/* just mark time until time is stable */
		tot_ticks += cur_ticks-hr_ticks;
		hr_ticks = cur_ticks;
	} else {
	    static double nag_tick;
	    double hr_delta_ticks, tot_delta_ticks;
	    double tru_tot_adj, tru_hr_adj; /* nsecs of adjustment */
	    double tot_trim, hr_trim;	/* nsec/sec */

	    tot_delta_ticks = cur_ticks-tot_ticks;
	    tot_adj += delta*1000.0;
	    hr_adj += delta*1000.0;

	    if (tot_delta_ticks >= 16*SECDAY*HZ) {
		tot_adj -= rint(tot_adj/16);
		tot_ticks += rint(tot_delta_ticks/16);
		tot_delta_ticks = cur_ticks-tot_ticks;
	    }
	    hr_delta_ticks = cur_ticks-hr_ticks;

	    tru_hr_adj = hr_adj + timetrim*rint(hr_delta_ticks/HZ);
	    tru_tot_adj = tot_adj + timetrim*rint(tot_delta_ticks/HZ);

	    if (meas_ok >= 0
		&& (hr_delta_ticks >= SECDAY*HZ
		    || (tot_delta_ticks < 4*SECDAY*HZ
			&& hr_delta_ticks >= SECHR*HZ)
		    || (debug > 2
			&& hr_delta_ticks >= (SECHR/10)*HZ))) {

		hr_trim = rint(tru_hr_adj*HZ/hr_delta_ticks);
		tot_trim = rint(tru_tot_adj*HZ/tot_delta_ticks);

		if (debug != 0
		    || (abs(timetrim - hr_trim) > 100000.0
			&& 0 == timetrim_fn
			&& (cur_ticks - nag_tick) >= 24*SECDAY*HZ)) {
		    nag_tick = cur_ticks;
		    log_debug(LOG_INFO, "%+.3f/%.2f or %+.3f/%.2f sec/hr;"
			      " timetrim=%+.0f or %+.0f",
			      tru_tot_adj/F_NSEC_PER_SEC,
			      tot_delta_ticks/(SECHR*HZ*1.0),
			      tru_hr_adj/F_NSEC_PER_SEC,
			      hr_delta_ticks/(SECHR*HZ*1.0),
			      tot_trim,
			      hr_trim);
		}

		if (tot_trim < -MAX_TRIM || tot_trim > MAX_TRIM) {
		    tot_ticks = hr_ticks;
		    tot_adj = hr_adj;
		    if (debug)
			log_debug(LOG_DEBUG,
				  "trimming tot_adj=%d, tot_ticks=%d",
				  tot_adj, tot_ticks);
#ifdef TIMED_IRIX_62
		} else if (0 > syssgi(SGI_SETTIMETRIM, (long)tot_trim)) {
		    log_debug(LOG_ERR,"SETTIMETRIM(%d): %s",
			      (long)tot_trim, strerror(errno));
#else
		} else if (0 > cap_settimetrim((long)tot_trim)) {
		    log_debug(LOG_ERR,"SETTIMETRIM(%d): %s",
			      (long)tot_trim, strerror(errno));
#endif
		} else {
		    if (0 != timetrim_fn) {
			timetrim_st = fopen(timetrim_fn, "w");
			if (0 == timetrim_st) {
			    log_debug(LOG_ERR, "fopen(%s): %s",
				      timetrim_fn, strerror(errno));
			} else {
			    if (0 > fprintf(timetrim_st,
					    timetrim_wpat,
					    (long)tot_trim,
					    tru_tot_adj,
					    tot_delta_ticks)) {
				log_debug(LOG_ERR, "fprintf(%s): %s",
					  timetrim_fn, strerror(errno));
			    }
			    (void)fclose(timetrim_st);

			    fix_set_year(now.tv_sec);
			}
		    }

		    /* The drift is being changed by the change in
		     * timetrim.  Convert nsec/sec of trim to ticks/tick.
		     */
		    drift_err -= ((tot_trim - (double)timetrim)
				    * drift_ticks
				    / (F_NSEC_PER_SEC*HZ));

		    /* The total adjustment is being moved to timetrim
		     */
		    tot_adj -= ((tot_trim - timetrim)
				* rint(tot_delta_ticks / HZ));
		    timetrim = tot_trim;
		}

		hr_ticks = cur_ticks;
		hr_adj = 0;
	    }
	}
    }

    return rep;
}



/* open a TTY connected to a WWV or GPS clock.
 */
static int				/* <0 on error, else fd */
get_tty(void)
{
    int dev_fd;
    struct termio ttyb;

    dev_fd = open(masname, O_RDWR, 0);
    if (0 > dev_fd) {
	log_debug(LOG_ERR, "open(%s): %s", masname, strerror(errno));
	return -1;
    }

    if (0 > ioctl(dev_fd, TCGETA, &ttyb)) {
	log_debug(LOG_ERR, "ioctl(%s,TCGETA): %s", masname, strerror(errno));
	return -1;
    }

    ttyb.c_iflag = 0;
    ttyb.c_oflag = 0;
#ifdef TIMED_IRIX_62
    ttyb.c_cflag = (ttyb.c_cflag & ~CBAUD) | B9600 | HUPCL | CLOCAL;
#else
    ttyb.c_cflag |= HUPCL | CLOCAL;
    ttyb.c_ospeed = ttyb.c_ispeed = B9600;
#endif
    ttyb.c_lflag = 0;
    ttyb.c_cc[VMIN] = 0;
    ttyb.c_cc[VTIME] = 1;

    if (0 > ioctl(dev_fd, TCSETA, &ttyb)) {
	log_debug(LOG_ERR, "ioctl(%s,TCSETA): %s", masname, strerror(errno));
	return -1;
    }

    return dev_fd;
}



/* get sockets connected to the master
 */
int					/* <0 on error */
get_masname(char *port,
	    int *rawsocp,
	    int *sndsocp,
	    int *udpsocp)
{
    struct protoent *proto;
    char *p;
    int on;


    bzero((char *)&masaddr, sizeof(masaddr));
    masaddr.sin_family = AF_INET;
    masaddr.sin_addr.s_addr = inet_addr(masname);
    if (masaddr.sin_addr.s_addr == -1) {
	struct hostent *hp;
	hp = gethostbyname(masname);
	if (hp) {
	    masaddr.sin_family = hp->h_addrtype;
	    bcopy(hp->h_addr, (caddr_t)&masaddr.sin_addr,
		  hp->h_length);
	} else {
	    log_debug(LOG_ERR, "unknown host name %s", masname);

	}
    }

    if ((*rawsocp = cap_socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)) < 0) {
	log_debug(LOG_ERR, "socket(RAW): %s", strerror(errno));
	return -1;
    }
    if (connect(*rawsocp, (struct sockaddr*)&masaddr, sizeof(masaddr)) < 0) {
	log_debug(LOG_ERR, "connect(RAW): %s", strerror(errno));
	return -1;
    }
    if ((*sndsocp = cap_socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) < 0) {
	log_debug(LOG_ERR, "socket(RAW): %s", strerror(errno));
	return -1;
    }
    if (connect(*sndsocp, (struct sockaddr*)&masaddr, sizeof(masaddr)) < 0) {
	log_debug(LOG_ERR, "connect(RAW): %s", strerror(errno));
	return -1;
    }

    masaddr.sin_port = strtol(port,&p,0);
    if (*p != '\0' || masaddr.sin_port == 0) {
	struct servent *sp;
	sp = getservbyname(PORT, "udp");
	if (!sp) {
	    log_debug(LOG_ERR, "%s/%s is an unknown service", PORT, PROTO);
	    return -1;
	}
	masaddr.sin_port = sp->s_port;
    }
    if ((proto = getprotobyname(PROTO)) == NULL) {
	log_debug(LOG_ERR, "%s is an unknown protocol", PROTO);
	return -1;
    }
    if ((*udpsocp = socket(AF_INET, SOCK_DGRAM, proto->p_proto)) < 0) {
	log_debug(LOG_ERR, "socket(UDP): %s", strerror(errno));
	return -1;
    }
    on = IPTOS_LOWDELAY;
    if (setsockopt(*udpsocp,IPPROTO_IP,IP_TOS,&on,sizeof(on)) < 0)
	    perror("setsockopt TOS");
    if (connect(*udpsocp, (struct sockaddr*)&masaddr, sizeof(masaddr)) < 0) {
	log_debug(LOG_ERR, "connect(UDP): %s", strerror(errno));
	return -1;
    }

    return 0;
}


static void
usage(void)
{
    fprintf(stderr,
	    "%s: timeslave [-md[d]BqY] [-T max-trials] [-t min-trials]\n"
	    "   [-w ICMP-wait] [-W date-wait] [-r rate] [-p date-port]\n"
	    "   [-D max-drift] [-P parm-file] [-y year] [-c clock_type]\n"
	    " -H net-master|-C clock-device\n",
	    pgmname);
    exit(1);
}


main(int argc,
     char *argv[])
{
    int i;
    char *port = "time";
    char *p;
    int udpsoc = -1;
    int rawsoc = -1;
    int sndsoc = -1;
    int dev_fd = -1;
    struct tms tm;
    struct stat sbuf;


    pgmname = argv[0];
    if (0 > syssgi(SGI_GETTIMETRIM, &timetrim)) {
	log_debug(LOG_ERR, "syssgi(GETTIMETRIM): %s", strerror(errno));
	timetrim = 0;
    }
    tot_ticks = hr_ticks = times(&tm);

    while ((i = getopt(argc, argv, "mdBqT:t:W:w:r:p:i:D:y:YH:C:c:P:")) != EOF)
	switch (i) {
	case 'm':
	    measure_only = 1;
	    break;

	case 'd':
	    debug++;
	    break;

	case 'B':
	    background = 0;
	    break;

	case 'q':
	    quiet = 1;
	    break;

	case 'T':
	    i = strtol(optarg,&p,0);
	    if (*p != '\0'
		|| i < icmp_min_diffs
		|| i > MAX_MAX_ICMP_TRIALS) {
		log_debug(LOG_ERR, "invalid max # of ICMP measurements: %s",
			  optarg);
	    } else {
		icmp_max_sends = i;
	    }
	    break;

	case 't':
	    i = strtol(optarg,&p,0);
	    if (*p != '\0'
		|| i < 1
		|| (icmp_max_sends > 0 && i > icmp_max_sends)) {
		log_debug(LOG_ERR, "invalid min # of ICMP measurements: %s",
			  optarg);
	    } else {
		icmp_min_diffs = i;
	    }
	    break;

	case 'W':
	    i = strtol(optarg,&p,0);
	    if (*p != '\0'
		|| i < 1) {
		log_debug(LOG_ERR, "invalid date/time wait-time: %s",
			  optarg);
	    } else {
		date_wait = i;
	    }
	    break;

	case 'w':
	    i = strtol(optarg,&p,0);
	    if (*p != '\0'
		|| i < 1 || i > MAX_NET_DELAY*2) {
		log_debug(LOG_ERR, "%s: invalid ICMP wait-time: %s",
			      optarg, optarg);
	    } else {
		icmp_wait = i;
	    }
	    break;

	case 'r':
	    i = strtol(optarg,&p,0)*HZ;
	    if (*p != '\0'
		|| i < 5*HZ) {
		log_debug(LOG_ERR, "repetition rate %s bad",
			  optarg);
	    } else {
		rep_rate = i;
	    }
	    break;

	case 'p':
	    port = optarg;
	    break;

	case 'i':			/* initial nsec/sec of drift */
	    i = strtol(optarg,&p,0);
	    if (*p != '\0'
		|| i > 1000000		/* at most 0.1% or 1 msec/sec */
		|| i < -1000000) {	/* or 1 million nsec/sec */
		log_debug(LOG_ERR,
			  "initial drift %s is not between 1 & 1000000 nsec",
			  optarg);
	    } else {
		drift_err = (i/(F_NSEC_PER_SEC))*(DRIFT_TICKS/HZ);
	    }
	    break;

	case 'D':			/* get usec/sec max drift */
	    i = strtol(optarg,&p,0);
	    if (*p != '\0'
		|| i < 1 || i > MAX_DRIFT) {	/* <= 0.3% or 3 msec/sec */
		log_debug(LOG_ERR,
			  "max-drift %s is not between 1 & 3000 usec",
			  optarg);
	    } else {
		max_drift = i/(F_NSEC_PER_SEC*HZ);  /* convert to sec/HZ */
	    }
	    break;

	case 'y':
	    i = strtol(optarg,&p,0);
	    if (*p != '\0'
		|| (i > 100 && i < 1997)
		|| i > 2100) {
		log_debug(LOG_ERR, "%s is not a good year", optarg);
	    } else if (cap_y) {
		log_debug(LOG_ERR, "-y and -Y cannot be used together");
	    } else {
		if (i <= 37)
		    i += 2000;	
		set_year = set_year2 = (i>1900) ? (i-1900) : i;
	    }
	    break;

	case 'Y':
	    if (set_year != 0) {
		log_debug(LOG_ERR, "-y and -Y cannot be used together");
	    } else {
		cap_y = 1;
	    }
	    break;

	case 'H':
	    if (mode != MODE_UNKNOWN)
		usage();
	    mode = MODE_ICMP;
	    masname = optarg;
	    break;

	case 'C':
	    if (mode == MODE_ICMP)
		usage();
	    if (mode == MODE_UNKNOWN)
		mode = MODE_NOT_ICMP;
	    masname = optarg;
	    break;

	case 'c':
	    if (mode == MODE_NOT_ICMP)
		mode = MODE_UNKNOWN;
	    if (!strcasecmp(optarg, "PST_WWV")
		|| !strcasecmp(optarg, "Traconix_WWV")) {
		if (mode != MODE_UNKNOWN)
		    usage();
		mode = MODE_PST_WWV;
	    } else if (!strcasecmp(optarg, "TT600")) {
		if (mode != MODE_UNKNOWN)
		    usage();
		mode = MODE_TT600;
	    } else {
		usage();
	    }
	    break;

	case 'P':
	    timetrim_fn = optarg;
	    break;

	default:
	    usage();
    }
    if (argc != optind)
	usage();
    if (cap_y && !measure_only) {
	if (timetrim_fn == 0) {
	    log_debug(LOG_ERR, "-Y requires -P");
	    cap_y = 0;
	} else if (0 <= stat(timetrim_fn, &sbuf)) {
	    fix_set_year(sbuf.st_mtim.tv_sec);
	}
    }

    switch (mode) {
    case MODE_ICMP:
	if (get_masname(port,&rawsoc,&sndsoc,&udpsoc) < 0)
	    exit(1);
	break;
    case MODE_NOT_ICMP:
	mode = MODE_PST_WWV;
	/* fall through */
    case MODE_PST_WWV:
    case MODE_TT600:
	if ((dev_fd = get_tty()) < 0)
	    exit(1);
	break;
    default:
	usage();
    }

    if (0 == rep_rate)
	rep_rate = DEF_REP_RATE;

    if (0 == icmp_max_sends)
	icmp_max_sends = (icmp_min_diffs > 0 ? icmp_min_diffs*4 : 16);
    if (0 == icmp_min_diffs)
	icmp_min_diffs = (icmp_max_sends > 16 ? icmp_max_sends/4 : 4);

    if (0 == max_drift) {		/* assume a max. drift of 1% */
	max_drift = 0.01/HZ;
    }
    neg_max_drift = -max_drift;


    (void)_daemonize(!background ? (_DF_NOFORK|_DF_NOCHDIR|_DF_NOCLOSE) : 0,
		     udpsoc, rawsoc, sndsoc >= 0 ? sndsoc : dev_fd);

    (void)signal(SIGUSR1, moredebug);
    (void)signal(SIGUSR2, lessdebug);
    (void)signal(SIGINT, done);
    (void)signal(SIGTERM, done);

    openlog("timeslave", LOG_CONS|LOG_PID, LOG_DAEMON);

    if (!measure_only && getuid())
	log_debug(LOG_ERR, "not running with UID=0");

    if (timetrim_fn != 0 && !measure_only) {
	timetrim_st = fopen(timetrim_fn, "r+");
	if (0 == timetrim_st) {
	    if (errno != ENOENT) {
		log_debug(LOG_ERR, "%s: %s: %s", timetrim_fn, strerror(errno));
		timetrim_fn = 0;
	    }
	} else {
	    long trim;
	    double adj, ticks;

	    i = fscanf(timetrim_st, timetrim_rpat,
		       &trim, &adj, &ticks);
	    if (i < 1
		|| trim > MAX_TRIM
		|| trim < -MAX_TRIM
		|| ticks < 0
		|| i == 2
		|| (i == 3
		    && trim != rint(adj*HZ/ticks))) {
		if (i != EOF)
		    log_debug(LOG_ERR, "%s: unrecognized contents",
			       timetrim_fn);
		if (debug > 0)
		    log_debug(LOG_DEBUG, "ignoring %s", timetrim_fn);
	    } else {
		if (0 > syssgi(SGI_SETTIMETRIM, trim)) {
		    log_debug(LOG_ERR, "syssgi(SETTIMETRIM): %s",
			      strerror(errno));
		} else {
		    timetrim = trim;
		}
		if (i == 3) {
		    tot_ticks -= ticks;
		    if (debug > 0)
			log_debug(LOG_DEBUG, "start tot_ticks=%.0f", ticks);
		}
	    }
	    (void)fclose(timetrim_st);
	}
    }

    (void)cap_schedctl(NDPRI,0,NDPHIMAX);	/* run fast to get good time */

    for (;;) {
	i = adjclock(rawsoc,sndsoc,udpsoc,dev_fd);  /* set the date */
	if (debug > 1)
	    log_debug(LOG_DEBUG, "delaying for %.2f sec", ((float)i)/HZ);
	sginap(i);
    }
}
