/*
 * Copyright (c) 1984 by Sun Microsystems, Inc.
 */

#include <rpc/rpc.h>
#include <utmpx.h>
#include <rpcsvc/rusers.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syslog.h>

#define	DIV60(t)	((t+30)/60)    /* x/60 rounded */
#define MAXINT 0x7fffffff
#define min(a,b) ((a) < (b) ? (a) : (b))

struct utmparr utmparr;
struct utmpidlearr utmpidlearr;
int utmpptr = 0;
struct rusers_utmp utmprec[MAXUSERS];
struct rusers_utmp *utmpp[MAXUSERS];
int idleptr = 0;
struct utmpidle	idlerec[MAXUSERS];
struct utmpidle *idlerecp[MAXUSERS];
int cnt;
void rusers_service();

/*
 * This is a utmp entry that does not correspond to a genuine user
 */
#define nonuser(ut) \
	((ut).ut_host[0] == 0 \
	 && ((ut).ut_line[0] == 0 \
	     || strncmp((ut).ut_line, "tty", 3) \
	     && strncmp((ut).ut_line, "console", 7)))

#define NLNTH 8			/* sizeof ut_name */

main(argc, argv)
int	argc;
char	**argv;
{
	register SVCXPRT * transp;
	struct sockaddr_in addr;
	int len = sizeof(struct sockaddr_in);

	openlog(argv[0], LOG_PID | LOG_ODELAY, LOG_DAEMON);

#ifdef DEBUG
	{
		int s;
		struct sockaddr_in addr;
		int len = sizeof(struct sockaddr_in);

		if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
			syslog(LOG_ERR, "inet: socket");
			return - 1;
		}
		if (bind(s, &addr, sizeof(addr)) < 0) {
			syslog(LOG_ERR, "bind");
			return - 1;
		}
		if (getsockname(s, &addr, &len) != 0) {
			syslog(LOG_ERR, "inet: getsockname");
			(void)close(s);
			return - 1;
		}
		pmap_unset(RUSERSPROG, RUSERSVERS_ORIG);
		pmap_set(RUSERSPROG, RUSERSVERS_ORIG, IPPROTO_UDP,
		    ntohs(addr.sin_port));
		pmap_unset(RUSERSPROG, RUSERSVERS_IDLE);
		pmap_set(RUSERSPROG, RUSERSVERS_IDLE, IPPROTO_UDP,
		    ntohs(addr.sin_port));
		if (dup2(s, 0) < 0) {
			syslog(LOG_ERR, "dup2");
			exit(1);
		}
	}
#endif	

	if (getsockname(0, &addr, &len) != 0) {
		syslog(LOG_ERR, "rstat: getsockname");
		exit(1);
	}
	if ((transp = svcudp_create(0)) == NULL) {
		syslog(LOG_ERR, "svc_rpc_udp_create: error\n");
		exit(1);
	}
	if (!svc_register(transp, RUSERSPROG, RUSERSVERS_ORIG,
	    rusers_service,0)) {
		syslog(LOG_ERR, "svc_rpc_register: error\n");
		exit(1);
	}
	if (!svc_register(transp, RUSERSPROG, RUSERSVERS_IDLE,
	    rusers_service,0)) {
		syslog(LOG_ERR, "svc_rpc_register: error\n");
		exit(1);
	}
	svc_run();		/* never returns */
	syslog(LOG_ERR, "run_svc_rpc should never return\n");
}

lgetutmp(all, idle)
	int all;		/* give all listings? */
	int idle;		/* get idle time? */
{
	struct utmp *bufp;
	struct rusers_utmp buf, **p;
	struct utmpidle **q, *console=0;
	int minidle;
	FILE *fp;
	char *file;
	char name[NLNTH];
	
	cnt = 0;
	p = utmparr.uta_arr;
	q = utmpidlearr.uia_arr;
	while (bufp = getutent()) {
		if (bufp->ut_type != USER_PROCESS || 
		    bufp->ut_line[0] == 0 || bufp->ut_name[0] == 0)
			continue;
		sysv_to_rusers(bufp, &buf);
		/* 
		 * if not tty and not remote, then skip it
		 */
		if (!all && nonuser(buf))
			continue;
		if (idle) {
			if (idleptr >= MAXUSERS) {
				syslog(LOG_ERR, "too many users");
				break;
			}
			*q = &idlerec[idleptr++];
			if (strncmp(buf.ut_line, "console",
			    sizeof("console")-1) == 0) {
				console = *q;
				strncpy(name, buf.ut_name, NLNTH);
			    }
			bcopy(&buf, &((*q)->ui_utmp), sizeof(buf));
			(*q)->ui_idle = findidle(buf.ut_line,
			    sizeof(buf.ut_line));
#ifdef DEBUG
			printf("%-10s %-10s %-18s %s; idle %d",
			    buf.ut_line, buf.ut_name,
			    buf.ut_host, ctime(&buf.ut_time),
			    (*q)->ui_idle);
#endif
			q++;
		}
		else {
			if (utmpptr >= MAXUSERS) {
				syslog(LOG_ERR, "too many users");
				break;
			}
			*p = &utmprec[utmpptr++];
			bcopy(&buf, *p, sizeof(buf));
#ifdef DEBUG
			printf("%-10s %-10s %-18s %s",
			    buf.ut_line, buf.ut_name,
			    buf.ut_host, ctime(&buf.ut_time));
#endif
			p++;
		}
		cnt++;
	}
	/* 
	 * If the console and the window pty's are owned by the same
	 * user, take the min of the idle times and associate
	 * it with the console.
	 */
	if (idle && console) {
		minidle = MAXINT;
		setutent();
		while (bufp = getutent()) {
			if (bufp->ut_type != USER_PROCESS)
				continue;
			sysv_to_rusers(bufp, &buf);
			if (buf.ut_host[0] == 0
			    && strncmp(buf.ut_line, "ttyq", 4) == 0
			    && strncmp(buf.ut_name, name, NLNTH) == 0)
				minidle = min(findidle(&buf), minidle);
		}
		console->ui_idle = min(minidle, console->ui_idle);
	}
}

void
rusers_service(rqstp, transp)
	register struct svc_req *rqstp;
	register SVCXPRT *transp;
{
	switch (rqstp->rq_proc) {
	case 0:
		if (svc_sendreply(transp, xdr_void, 0)  == FALSE) {
			syslog(LOG_ERR, "err: rusersd");
			exit(1);
		    }
		exit(0);
	case RUSERSPROC_NUM:
		utmparr.uta_arr = &utmpp[0];
		lgetutmp(0, 0);
		if (!svc_sendreply(transp, xdr_u_long, &cnt))
			syslog(LOG_ERR,"svc_rpc_send_results");
		exit(0);
	case RUSERSPROC_NAMES:
	case RUSERSPROC_ALLNAMES:
		if (rqstp->rq_vers == RUSERSVERS_ORIG) {
			utmparr.uta_arr = &utmpp[0];
			lgetutmp(rqstp->rq_proc == RUSERSPROC_ALLNAMES, 0);
			utmparr.uta_cnt = cnt;
			if (!svc_sendreply(transp, xdr_utmparr, &utmparr))
				syslog(LOG_ERR,"svc_rpc_send_results");
			exit(0);
		}
		else {
			utmpidlearr.uia_arr = &idlerecp[0];
			lgetutmp(rqstp->rq_proc == RUSERSPROC_ALLNAMES, 1);
			utmpidlearr.uia_cnt = cnt;
			if (!svc_sendreply(transp, xdr_utmpidlearr,
			    &utmpidlearr))
				syslog(LOG_ERR,"svc_rpc_send_results");
			exit(0);
		}
	default: 
		svcerr_noproc(transp);
		exit(0);
	}
}

/* find & return number of minutes current tty has been idle */
findidle(name, ln)
	char *name;
{
	time_t	now;
	struct stat stbuf;
	long lastaction, diff;
	char ttyname[20];

	strcpy(ttyname, "/dev/");
	strncat(ttyname, name, ln);
	if (stat(ttyname, &stbuf) < 0)
		return(MAXINT);
	time(&now);
	lastaction = stbuf.st_atime;
	diff = now - lastaction;
	diff = DIV60(diff);
	if (diff < 0) diff = 0;
	return(diff);
}


sysv_to_rusers(u, r)
    register struct utmp *u;
    register struct rusers_utmp *r;
{
    struct utmpx tempx;  /* make utmpx version of u here */
    register struct utmpx *x;	  /* this will pt to getutxid's fetched entry */

    /* convert utmp entry u to the utmpx format required by getutxid */ 
    getutmpx(u, &tempx);
    setutxent();
    if (((x = getutxid(&tempx)) != NULL) &&
	  u->ut_pid == x->ut_pid && u->ut_time == x->ut_tv.tv_sec) {
	/* ftpd puts hostname in ut_line -- use proper id from utx_line  */
	strncpy(r->ut_line, x->ut_line, sizeof(r->ut_line));
	strncpy(r->ut_host, x->ut_host, sizeof(r->ut_host));
    } else {
	strncpy(r->ut_line, u->ut_line, sizeof(r->ut_line));
	r->ut_host[0] = '\0';
    }
    strncpy(r->ut_name, u->ut_user, sizeof(r->ut_name));
    r->ut_time = u->ut_time;
}
