#ifndef lint
static char sccsid[] = 	"@(#)rup.c	1.2 88/05/13 4.0NFSSRC Copyr 1988 Sun Micro";
#endif

/* 
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 */

#include <stdio.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <rpcsvc/rstat.h>
#include <rpc/rpc.h>
#include <rpc/pmap_clnt.h>

#define MACHINELEN 20		/* length of machine name printed out */
#define AVENSIZE (3*sizeof(long))

/*
 * ******* THIS WAS TAKEN FROM H/PARAM.H ******************
 *
 * Scale factor for scaled integers used to count
 * %cpu time and load averages.
 *
 */
#define FSHIFT 8		/* bits to right of fixed binary point */
#define FSCALE (1<<FSHIFT)
 
int machinecmp();
int loadcmp();
int uptimecmp();
int collectnames();

#define INIT_HOSTS	200

struct entry {
	struct in_addr addr;
	char *machine;
	struct timeval boottime;
	time_t curtime;
	long avenrun[3];
} *entry;

void usage(), putline(), printnames();

int curhosts = INIT_HOSTS;

int curentry;
int vers;			/* which version did the broadcasting */
int lflag;			/* load: sort by load average */
int tflag;			/* time: sort by uptime average */
int hflag;			/* host: sort by machine name */
int dflag;			/* debug: list only first n machines */
int debug;

main(argc, argv)
	char **argv;
{
	struct statstime sw;
	int err;
	int single;
	enum clnt_stat clnt_stat;

	err = single = 0;
	while (argc > 1) {
		if (argv[1][0] != '-') {
			single++;
			if (0 > singlehost(argv[1]))
				err++;
		}
		else {
			switch(argv[1][1]) {
	
			case 'l':
				lflag++;
				break;
			case 't':
				tflag++;
				break;
			case 'h':
				hflag++;
				break;
			case 'd':
				dflag++;
				if (argc < 3)
					usage();
				debug = atoi(argv[2]);
				argc--;
				argv++;
				break;
			default:
				usage();
			}
		}
		argv++;
		argc--;
	}
	if (single > 0)
		exit( (err) ? 1 : 0 );
	if (hflag || tflag || lflag) {
		printf("collecting responses... ");
		fflush(stdout);
	}
	entry = (struct entry *)malloc(INIT_HOSTS * sizeof(*entry));
	if (entry == 0) {
		fprintf(stderr,"rup: initial malloc failed\n");
		exit(1);
	}
	vers = RSTATVERS_TIME;
	clnt_stat = clnt_broadcast_exp(RSTATPROG, RSTATVERS_TIME, RSTATPROC_STATS,
	    xdr_void, NULL, xdr_statstime,  &sw, collectnames, 5, 0);
#ifdef TESTING
	fprintf(stderr, "starting second round of broadcasting\n");
#endif
	vers = RSTATVERS_SWTCH;
	clnt_stat = clnt_broadcast_exp(RSTATPROG, RSTATVERS_SWTCH, RSTATPROC_STATS,
	    xdr_void, NULL, xdr_statsswtch,  &sw, collectnames, 5, 0);
	if (hflag || tflag || lflag)
		printnames();
}

int
singlehost(host)
	char *host;
{
	enum clnt_stat err;
	struct statstime sw;
	time_t now;
    
	err = (enum clnt_stat)callrpc(host, RSTATPROG, RSTATVERS_TIME,
	    RSTATPROC_STATS, xdr_void, 0, xdr_statstime, &sw);
	if (err == RPC_SUCCESS)
		now = sw.curtime.tv_sec;
	else if (err == RPC_PROGVERSMISMATCH) {
		if (err = (enum clnt_stat)callrpc(host, RSTATPROG,
		    RSTATVERS_SWTCH, RSTATPROC_STATS, xdr_void, 0,
		    xdr_statsswtch, &sw)) {
			fprintf(stderr,
			    "%*.*s: ", MACHINELEN, MACHINELEN, host);
			clnt_perrno(err);
			fprintf(stderr, "\n");
			return (-1);
		}
		time (&now);
	}
	else {
		fprintf(stderr, "%*.*s: ", MACHINELEN, MACHINELEN, host);
		clnt_perrno(err);
		fprintf(stderr, "\n");
		return (-1);
	}
	printf("%*.*s  ", MACHINELEN, MACHINELEN, host);
	putline(now, sw.boottime, sw.avenrun);
	return (0);
}

void
putline(now, boottime, avenrun)
	time_t now;
	struct timeval boottime;
	long avenrun[];
{
	int uptime, days, hrs, mins, i;
	
	uptime = now - boottime.tv_sec;
	uptime += 30;
	if (uptime < 0)		/* unsynchronized clocks */
		uptime = 0;
	days = uptime / (60*60*24);
	uptime %= (60*60*24);
	hrs = uptime / (60*60);
	uptime %= (60*60);
	mins = uptime / 60;

	printf("  up");
	if (days > 0)
		printf(" %2d day%s", days, days>1?"s,":", ");
	else
		printf("         ");
	if (hrs > 0)
		printf(" %2d:%02d,  ", hrs, mins);
	else
		printf(" %2d min%s", mins, mins>1?"s,":", ");

	/*
	 * Print 1, 5, and 15 minute load averages.
	 * (Found by looking in kernel for avenrun).
	 */
	printf("  load average:");
	for (i = 0; i < (AVENSIZE/sizeof(avenrun[0])); i++) {
		if (i > 0)
			printf(",");
		printf(" %.2f", (double)avenrun[i]/FSCALE);
	}
	printf("\n");
}

collectnames(resultsp, raddrp)
	char *resultsp;
	struct sockaddr_in *raddrp;
{
	struct hostent *hp;
	static int debugcnt;
	int i, cnt;
	register int addr;
	register struct entry *entryp, *lim;
	struct statstime *sw;
	time_t now;

	/* 
	 * weed out duplicates
	 */
	addr = raddrp->sin_addr.s_addr;
	lim = entry + curentry;
	for (entryp = entry; entryp < lim; entryp++)
		if (addr == entryp->addr.s_addr)
			return (0);
	sw = (struct statstime *)resultsp;
	debugcnt++;
	entry[curentry].addr.s_addr = addr;

	/*
	 * if raw, print this entry out immediately
	 * otherwise store for later sorting
	 */
	if (!hflag && !lflag && !tflag) {
		hp = gethostbyaddr(&raddrp->sin_addr.s_addr,
		    sizeof(int),AF_INET);
		if (hp == NULL)
			printf("%*.*s  ", MACHINELEN, MACHINELEN, 
				inet_ntoa(raddrp->sin_addr));
		else
			printf("%*.*s  ", MACHINELEN, MACHINELEN, hp->h_name);
		if (vers == RSTATVERS_TIME)
			now = sw->curtime.tv_sec;
		else
			time (&now);
		putline(now, sw->boottime, sw->avenrun);
	}
	else {
		entry[curentry].boottime = sw->boottime;
		if (vers == RSTATVERS_TIME)
			entry[curentry].curtime = sw->curtime.tv_sec;
		else
			time(&entry[curentry].curtime);
		bcopy(sw->avenrun, entry[curentry].avenrun, AVENSIZE);
	}
	curentry++;
	if (curentry == curhosts) {
		struct entry *newentry;
		int newhosts = curhosts * 2;
		newentry = malloc(newhosts * sizeof(*entry));
		if (newentry == 0) {
			fprintf(stderr, "rup: malloc failed\n");
			exit(1);
		}
		memcpy(newentry, entry, sizeof(*entry) * curhosts);
		free(entry);
		entry = newentry;
		curhosts = newhosts;
	}
	if (dflag && debugcnt >= debug)
		return (1);
	return(0);
}

void
printnames()
{
	char buf[MACHINELEN+1];
	struct hostent *hp;
	int i, j;

	for (i = 0; i < curentry; i++) {
		hp = gethostbyaddr(&entry[i].addr,sizeof(int),AF_INET);
		if (hp == NULL)
			sprintf(buf, "%.*s", MACHINELEN, 
				inet_ntoa(entry[i].addr));
		else
			sprintf(buf, "%.*s", MACHINELEN, hp->h_name);
		entry[i].machine = (char *)malloc(MACHINELEN+1);
		strcpy(entry[i].machine, buf);
	}
	if (hflag)
		qsort(entry, curentry, sizeof(struct entry), machinecmp);
	else if (lflag)
		qsort(entry, curentry, sizeof(struct entry), loadcmp);
	else
		qsort(entry, curentry, sizeof(struct entry), uptimecmp);
	printf("\n");
	for (i = 0; i < curentry; i++) {
		printf("%*.*s  ", MACHINELEN, MACHINELEN, entry[i].machine);
		putline(entry[i].curtime, entry[i].boottime, entry[i].avenrun);
	}
}

machinecmp(a,b)
	struct entry *a, *b;
{
	return (strcmp(a->machine, b->machine));
}

uptimecmp(a,b)
	struct entry *a, *b;
{
	int tmp;
	
	return (a->boottime.tv_sec - b->boottime.tv_sec);
}

loadcmp(a,b)
	struct entry *a, *b;
{
	return (a->avenrun[0] - b->avenrun[0]);
}

void
usage()
{
	fprintf(stderr, "Usage: rup [-h] [-l] [-t] [host ...]\n");
	exit(1);
}
