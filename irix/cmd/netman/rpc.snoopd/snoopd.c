/*
 * Copyright 1990 Silicon Graphics, Inc.  All rights reserved.
 *
 *	RPC-based remote snooping deamon
 *
 *	$Revision: 1.18 $
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */

#include <bstring.h>
#include <stdio.h>
#include <errno.h>
/* #include <getopt.h> */
#include <netdb.h>
#include <signal.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include "snoopd.h"
#include "timer.h"
#include "exception.h"
#include "expr.h"
#include "heap.h"
#include "index.h"
#include "macros.h"
#include "packetview.h"
#include "protocol.h"
#include "snooper.h"

/*
 * Table of interfaces available
 */
extern struct iftab *iftable;

/*
 * Service switch table
 */
struct svcsw {
	void		(*svc_init)(void);
	void		(*svc_subscribe)(SnoopClient *, u_int, u_int, u_int);
	void		(*svc_unsubscribe)(SnoopClient *);
	enum snoopstat	(*svc_add)(SnoopClient *, Expr *, ExprError *, int *);
	enum snoopstat	(*svc_delete)(SnoopClient *, int);
	void		(*svc_preprocess)(void);
	int		(*svc_process)(SnoopClient *, PacketBuf *, Packet *);
	void		(*svc_timer)(SnoopClient *);
	void		(*svc_setinterval)(SnoopClient *, u_int);
};

extern void ns_init(void),
	    ns_subscribe(SnoopClient *, u_int, u_int, u_int),
	    ns_unsubscribe(SnoopClient *),
	    ns_timer(SnoopClient *);
extern enum snoopstat
	    ns_add(SnoopClient *, Expr *, ExprError *, int *),
	    ns_delete(SnoopClient *, int);
extern int  ns_process(SnoopClient *, PacketBuf *, Packet *);

extern void nl_init(void),
	    nl_subscribe(SnoopClient *, u_int, u_int, u_int),
	    nl_unsubscribe(SnoopClient *),
	    nl_preprocess(void),
	    nl_timer(SnoopClient *);
extern enum snoopstat
	    nl_add(SnoopClient *, Expr *, ExprError *, int *),
	    nl_delete(SnoopClient *, int);
extern int  nl_process(SnoopClient *, PacketBuf *, Packet *);

extern void hist_init(void),
	    hist_subscribe(SnoopClient *, u_int, u_int, u_int),
	    hist_unsubscribe(SnoopClient *),
	    hist_timer(SnoopClient *),
	    hist_setinterval(SnoopClient *, u_int);
extern enum snoopstat
	    hist_add(SnoopClient *, Expr *, ExprError *, int *),
	    hist_delete(SnoopClient *, int);
extern int  hist_process(SnoopClient *, PacketBuf *, Packet *);

extern void al_init(void),
	    al_subscribe(SnoopClient *, u_int, u_int, u_int),
	    al_unsubscribe(SnoopClient *),
	    al_preprocess(void),
	    al_timer(SnoopClient *);
extern enum snoopstat
	    al_add(SnoopClient *, Expr *, ExprError *, int *),
	    al_delete(SnoopClient *, int);
extern int  al_process(SnoopClient *, PacketBuf *, Packet *);

static struct svcsw svcsw[] = {
	{ ns_init, ns_subscribe, ns_unsubscribe,
	  ns_add, ns_delete, 0, ns_process, ns_timer, 0 },
	{ nl_init, nl_subscribe, nl_unsubscribe,
	  nl_add, nl_delete, nl_preprocess, nl_process, nl_timer, 0 },
	{ hist_init, hist_subscribe, hist_unsubscribe,
	  hist_add, hist_delete, 0, hist_process, hist_timer, hist_setinterval},
	{ al_init, al_subscribe, al_unsubscribe,
	  al_add, al_delete, al_preprocess, al_process, al_timer, 0 },
};

/*
 * List of clients
 */
static SnoopClient *snoopClient = 0;
static SnoopClient *snoopClientTail = 0;
static Index *xprtindex;
static u_int xprthash(u_int *, int);
static int xprtcmp(u_int *, u_int *, int);
static struct indexops xprtops = { xprthash, xprtcmp, 0 };

static void client_service(struct svc_req *, SVCXPRT *);
static void client_delete(SnoopClient *);
static void client_destroy(SVCXPRT *);
static SnoopClient *getclient(SVCXPRT *);
static enum snoopstat subscribe(SVCXPRT *, struct authunix_parms *,
				enum svctype, char *, u_int, u_int,
				u_int, int *);
static enum snoopstat unsubscribe(SVCXPRT *);
static void trim_snooplen(SnoopClient *, struct iftab *);
static int process_packet(PacketBuf *, Packet *, void *);

/*
 * Standard RPC destroy procedure that we rewrite (yuch!)
 */
static void (*std_destroy)(SVCXPRT *);

/*
 * Send a non-blocking RPC reply.
 */
static int nb_sendreply(SVCXPRT *, xdrproc_t, void *, u_int);

/*
 * Timing and timer callback function
 */
#define UPPERTIMEBOUND	70000
#define AVGTIME		50000
#define LOWERTIMEBOUND	30000

static struct timer timer;
static void do_timer(void);

/*
 * Snoopd options
 */
struct options opts;

/*
 * libsun flag to turn off NIS
 */
#ifdef __sgi
extern int _yp_disabled;
#endif

/*
 * Set up socket and wait for requests
 */
main(int argc, char **argv)
{
	SVCXPRT *xprt;
	fd_set readfds;
	struct iftab *ift;
	int n, rc;
	u_int readcount, avgtime;

	/*
	 * Set default settings
	 */
	opts.opt_queuelimit = 60000;
	opts.opt_usenameservice = 0;
	opts.opt_packetbufsize = 128;
	opts.opt_rawsequence = 0;
	opts.opt_loglevel = LOG_ERR;
	opts.opt_deadtime.tv_sec = 30;
	opts.opt_deadtime.tv_usec = 0;

	/*
	 * Setup exception handling.
	 */
	exc_progname = "snoopd";
	exc_autofail = 1;
	exc_level = opts.opt_loglevel;
#ifndef DEBUG
	exc_openlog(exc_progname, LOG_PID, LOG_DAEMON);
#endif
	signal(SIGPIPE, SIG_IGN);

	opterr = 0;
	while ((n = getopt(argc, argv, "dl:p:q:t:y")) != -1) {
		switch (n) {
		    case 'd':
			exc_level = ++opts.opt_loglevel;
			break;

		    case 'p':
			opts.opt_packetbufsize = atoi(optarg);
			break;

		    case 'q':
			opts.opt_queuelimit = atoi(optarg);
			break;

		    case 't':
			opts.opt_deadtime.tv_sec = atoi(optarg);
			break;

		    case 'y':
			opts.opt_usenameservice = 1;
			break;

		    default:
			exc_errlog(LOG_ERR, 0, "unknown option -%c", optopt);
		}
	}

	exc_errlog(LOG_NOTICE, 0, "starting");
	if (geteuid() != 0) {
		exc_errlog(LOG_ERR, EPERM, "not superuser");
		exit(EPERM);
	}

	/*
	 * Initialize protocols
	 *
	 * XXX We must disable NIS temporarily, as protocol init routines
	 * XXX tend to call getservbyname, which is horrendously slow thanks
	 * XXX to Sun's quadratic-growth lookup botch.
	 */
#ifdef __sgi
	_yp_disabled = 1;
#endif
	initprotocols();
#ifdef __sgi
	_yp_disabled = 0;
#endif

	/* Find out about the interfaces */
	iftable_init();
	for (n = 0, ift = iftable; ift != 0; ift = ift->if_next)
		n++;
	exc_errlog(LOG_DEBUG, 0, "%d interface%s available",
		   n, (n == 1) ? "" : "s");

	/* Initialize authorization list */
	authorize_init();

	/* Initialize services */
	for (n = 0; n < lengthof(svcsw); n++)
		(*svcsw[n].svc_init)();

	/* Set up RPC */
#ifdef DEBUG
	{
		struct sockaddr_in addr;
		int sock, len = sizeof addr;

		bzero(&addr, len);
		if ((sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
			exc_errlog(LOG_ERR, errno, "socket");
			exit(0);
		}
		if (bind(sock, &addr, len) < 0) {
			exc_errlog(LOG_ERR, errno, "bind");
			exit(0);
		}
		if (getsockname(sock, &addr, &len) != 0) {
			exc_errlog(LOG_ERR, errno, "getsockname");
			exit(0);
		}
		pmap_unset(SNOOPPROG, SNOOPVERS);
		pmap_set(SNOOPPROG, SNOOPVERS, IPPROTO_TCP,
						ntohs(addr.sin_port));
		if (dup2(sock, 0) < 0) {
			exc_errlog(LOG_ERR, errno, "dup2");
			exit(0);
		}
	}
#endif
	if ((xprt = svctcp_create(0, 8192, 8192)) == 0) {
		exc_errlog(LOG_ERR, errno, "svctcp_create");
		exit(0);
	}
	if (!svc_register(xprt, SNOOPPROG, SNOOPVERS, client_service, 0)) {
		exc_errlog(LOG_ERR, errno, "svc_register");
		exit(0);
	}

	/* Set up timer */
	readcount = opts.opt_packetbufsize / 2;
	avgtime = AVGTIME;
	timer_set(&timer, 0, TICKTIME, do_timer, 0);

	/* Set up hash table for clients */
	xprtindex = index(32, sizeof(SVCXPRT *), &xprtops);

	/* Set nameservice based on options */
#ifdef __sgi
	if (!opts.opt_usenameservice) {
		_yp_disabled = 1;
		sethostresorder("local");
	}
#endif

	/* Start snooping */
	for ( ; ; ) {
		readfds = svc_fdset;
		for (ift = iftable; ift != 0; ift = ift->if_next) {
			if (ift->if_snooper != 0)
				FD_SET(ift->if_snooper->sn_file, &readfds);
		}
		if (timer_isrunning(&timer))
			n = select(FD_SETSIZE, &readfds, 0, 0,
							&timer.t_remaining);
		else {
			n = select(FD_SETSIZE, &readfds, 0, 0,
							&opts.opt_deadtime);
			if (n == 0) {
#ifdef DEBUG
				continue;
#else
				exc_errlog(LOG_NOTICE, 0, "expired");
				exit(0);
#endif
			}
		}
		if (n <= 0) {
			rc = 0;
			if (n < 0)
				exc_errlog(LOG_ERR, errno, "select");
		} else {
			/* Do the snoop sockets */
			for (ift = iftable; ift != 0; ift = ift->if_next) {
				if (ift->if_snooper == 0 ||
				    !FD_ISSET(ift->if_snooper->sn_file,
					      &readfds))
					continue;

				rc = snoop_read(ift, readcount);
				if (rc < 0) {
					exc_errlog(LOG_ERR, errno,"snoop read");
					break;
				}
				pb_scan(&ift->if_pb, process_packet, ift);
				pb_flush(&ift->if_pb);
				if (--n == 0)
					break;
			}

			/* Do the RPC requests */
			if (n != 0)
				svc_getreqset(&readfds);
		}

		timer_tick(&timer);

		/* Calibrate buffer size */
		avgtime = avgtime * 0.95 + (timer.t_ticked.tv_sec * 1000000
					    + timer.t_ticked.tv_usec) * 0.05;
		if (avgtime > UPPERTIMEBOUND) {
			readcount -= (readcount >> 2);
			avgtime = AVGTIME;
			exc_errlog(LOG_DEBUG, 0,
					"recalibrated down to %d", readcount);
		} else if (rc == readcount &&
				avgtime < LOWERTIMEBOUND &&
				readcount < opts.opt_packetbufsize) {
			readcount += (readcount >> 2) + 1;
			if (readcount > opts.opt_packetbufsize)
				readcount = opts.opt_packetbufsize;
			avgtime = AVGTIME;
			exc_errlog(LOG_DEBUG, 0,
					"recalibrated up to %d", readcount);
		}
	}
}

/*
 * Service an RPC request
 */
static void
client_service(struct svc_req *reqp, SVCXPRT *xprt)
{
	SnoopClient *sc;
	struct authunix_parms *cred;
	int error;
	enum snoopstat status;

	exc_errlog(LOG_DEBUG, 0, "%X(%X)", xprt, reqp->rq_proc);

	/*
	 * As is customary, don't authenticate the NULL procedure
	 */
	if (reqp->rq_proc == SNOOPPROC_NULL) {
		(void) svc_getargs(xprt, xdr_void, 0);

		if (!svc_sendreply(xprt, xdr_void, 0))
			exc_errlog(LOG_WARNING, errno, "null: send reply");
		return;
	}

	switch (reqp->rq_cred.oa_flavor) {
	    case AUTH_UNIX:
		cred = (struct authunix_parms *) reqp->rq_clntcred;
		if (cred != 0)
			break;
		/* Fall through if cred == 0 */

	    case AUTH_NULL:
	    default:
		svcerr_weakauth(xprt);
		exc_errlog(LOG_WARNING, 0, "authentication failed");
		return;
	}

	switch (reqp->rq_proc) {
	    case SNOOPPROC_SUBSCRIBE:
	    {
		struct subreq sr;
		struct intres ir;

		if (!svc_getargs(xprt, xdr_subreq, &sr)) {
			exc_errlog(LOG_WARNING, errno,
						"subscribe: get argument");
			svcerr_decode(xprt);
			break;
		}

		ir.ir_status = subscribe(xprt, cred, sr.sr_service,
					 sr.sr_interface, sr.sr_buffer,
					 sr.sr_count, sr.sr_interval,
					 &ir.ir_value);
		if (sr.sr_interface != 0)
			delete(sr.sr_interface);

		error = nb_sendreply(xprt, xdr_intres, &ir, sizeof ir);
		if (error)
			exc_errlog(LOG_WARNING, error, "subscribe: send reply");
		break;
	    }

	    case SNOOPPROC_UNSUBSCRIBE:
	    {
		(void) svc_getargs(xprt, xdr_void, 0);

		status = unsubscribe(xprt);

		error = nb_sendreply(xprt, xdr_enum, &status, sizeof status);
		if (error)
			exc_errlog(LOG_WARNING, error,
				   "unsubscribe: send reply");
		sc = getclient(xprt);
		if (sc != 0 && sc->sc_blkproc == 0)
			client_delete(sc);
		break;
	    }

	    case SNOOPPROC_SETSNOOPLEN:
	    {
		u_int snooplen;

		if (!svc_getargs(xprt, xdr_u_int, &snooplen)) {
			exc_errlog(LOG_WARNING, errno,
				   "setsnooplen: get argument");
			svcerr_decode(xprt);
			break;
		}

		sc = getclient(xprt);
		if (sc == 0) {
			status = SNOOPERR_INVAL;
		} else {
			sc->sc_snooplen = snooplen;
			status = SNOOP_OK;
		}

		error = nb_sendreply(xprt, xdr_enum, &status, sizeof status);
		if (error)
			exc_errlog(LOG_WARNING, error,
				   "setsnooplen: send reply");
		break;
	    }

	    case SNOOPPROC_SETERRFLAGS:
	    {
		u_int flags;

		if (!svc_getargs(xprt, xdr_u_int, &flags)) {
			exc_errlog(LOG_WARNING, errno,
				   "seterrflags: get argument");
			svcerr_decode(xprt);
			break;
		}

		sc = getclient(xprt);
		if (sc == 0)
			status = SNOOPERR_INVAL;
		else {
			if (sc->sc_errflags != 0)
				snoop_seterrflags(sc->sc_if, 0);
			snoop_seterrflags(sc->sc_if, flags);
			sc->sc_errflags = flags;
			status = SNOOP_OK;
		}

		error = nb_sendreply(xprt, xdr_enum, &status, sizeof status);
		if (error)
			exc_errlog(LOG_WARNING, error,
				   "seterrflags: send reply");
		break;
	    }

	    case SNOOPPROC_ADDFILTER:
	    {
		struct expression exp;
		ExprError experr;
		struct intres ir;

		sc = getclient(xprt);
		if (sc == 0)
			ir.ir_status = SNOOPERR_INVAL;
		else {
			exp.exp_expr = 0;
			exp.exp_proto = sc->sc_if->if_snooper->sn_rawproto;

			if (!svc_getargs(sc->sc_xprt, xdr_expression, &exp)) {
				exc_errlog(LOG_WARNING, errno,
					   "add: get argument");
				svcerr_decode(sc->sc_xprt);
				break;
			}
			ir.ir_status = (*svcsw[sc->sc_service].svc_add)(sc,
					exp.exp_expr, &experr, &ir.ir_value);
			if (ir.ir_status != SNOOP_OK) {
				ex_destroy(exp.exp_expr);
				if (ir.ir_status == SNOOPERR_BADF)
					ir.ir_message = experr.err_message;
			}
		}

		error = nb_sendreply(xprt, xdr_intres, &ir, sizeof ir);
		if (error)
			exc_errlog(LOG_WARNING, error, "add: send reply");
		break;
	    }

	    case SNOOPPROC_DELETEFILTER:
	    {
		int bin;

		if (!svc_getargs(xprt, xdr_int, &bin)) {
			exc_errlog(LOG_WARNING, errno, "remove: get argument");
			svcerr_decode(xprt);
			break;
		}

		sc = getclient(xprt);
		if (sc == 0)
			status = SNOOPERR_INVAL;
		else
			status = (*svcsw[sc->sc_service].svc_delete)(sc, bin);

		error = nb_sendreply(xprt, xdr_enum, &status, sizeof status);
		if (error)
			exc_errlog(LOG_WARNING, error, "remove: send reply");
		break;
	    }

	    case SNOOPPROC_STARTCAPTURE:
	    {
		struct snoopstats ss;

		(void) svc_getargs(xprt, xdr_void, 0);

		sc = getclient(xprt);
		if (sc == 0 || sc->sc_state != STOPPED)
			status = SNOOPERR_INVAL;
		else {
			struct iftab *ift = sc->sc_if;

			if (sc->sc_snooplen == 0)
				sc->sc_snooplen = ift->if_mtu;

			if (ift->if_snooplen < sc->sc_snooplen
			    || ift->if_clients == 1) {
				snoop_setsnooplen(ift, sc->sc_snooplen);
				exc_errlog(LOG_DEBUG, 0,
					   "set snooplen to %d",
					   ift->if_snooplen);
			}
			sc->sc_state = STARTED;
			sc->sc_starttime = curtime;
			sn_getstats(ift->if_snooper, &ss);
			sc->sc_ifdrops = ss.ss_ifdrops;
			sc->sc_sbdrops = ss.ss_sbdrops;
			snoop_start(ift);
			status = SNOOP_OK;
		}

		error = nb_sendreply(xprt, xdr_enum, &status, sizeof status);
		if (error)
			exc_errlog(LOG_WARNING, error, "start: send reply");
		break;
	    }

	    case SNOOPPROC_STOPCAPTURE:
	    {
		struct snoopstats ss;

		(void) svc_getargs(xprt, xdr_void, 0);

		sc = getclient(xprt);
		if (sc == 0 || sc->sc_state == STOPPED)
			status = SNOOPERR_INVAL;
		else {
			struct iftab *ift = sc->sc_if;

			if (sc->sc_snooplen >= ift->if_snooplen)
				trim_snooplen(sc, ift);
			sc->sc_state = STOPPED;
			sn_getstats(sc->sc_if->if_snooper, &ss);
			sc->sc_ifdrops = ss.ss_ifdrops - sc->sc_ifdrops;
			sc->sc_sbdrops = ss.ss_sbdrops - sc->sc_sbdrops;
			snoop_stop(sc->sc_if);
			status = SNOOP_OK;
		}

		error = nb_sendreply(xprt, xdr_enum, &status, sizeof status);
		if (error)
			exc_errlog(LOG_WARNING, error, "stop: send reply");
		break;
	    }

	    case SNOOPPROC_GETSTATS:
	    {
		struct snoopstats ss;

		(void) svc_getargs(xprt, xdr_void, 0);

		sc = getclient(xprt);
		if (sc == 0)
			bzero(&ss, sizeof ss);
		else
			ss = sc->sc_stats;

		error = nb_sendreply(xprt, xdr_snoopstats, &ss, sizeof ss);
		if (error)
			exc_errlog(LOG_WARNING, error, "getstat: send reply");
		break;
	    }

	    case SNOOPPROC_GETADDR:
	    {
		struct getaddrarg gaa;
		struct getaddrres gar;
		int cmd;

		if (!svc_getargs(xprt, xdr_getaddrarg, &gaa)) {
			exc_errlog(LOG_WARNING, errno, "getaddr: get argument");
			svcerr_decode(xprt);
			break;
		}

		switch (gaa.gaa_cmd) {
		    case GET_IFADDR:
			cmd = SIOCGIFADDR;
			break;
		    case GET_IFDSTADDR:
			cmd = SIOCGIFDSTADDR;
			break;
		    case GET_IFBRDADDR:
			cmd = SIOCGIFBRDADDR;
			break;
		    case GET_IFNETMASK:
			cmd = SIOCGIFNETMASK;
			break;
		    default:
			cmd = 0;
		}

		if (cmd == 0)
			gar.gar_status = SNOOPERR_INVAL;
		else {
			sc = getclient(xprt);
			if (sc == 0) {
				gar.gar_status = SNOOPERR_INVAL;
			} else if (!sn_getaddr(sc->sc_if->if_snooper, cmd,
					       &gaa.gaa_addr)) {
				gar.gar_status = snoop_status(sc->sc_if);
			} else {
				gar.gar_status = SNOOP_OK;
				gar.gar_addr = gaa.gaa_addr;
			}
		}

		error = nb_sendreply(xprt, xdr_getaddrres, &gar, sizeof gar);
		if (error)
			exc_errlog(LOG_WARNING, error, "getaddr: send reply");
	    }

	    case SNOOPPROC_SETINTERVAL:
	    {
		unsigned int interval;

		if (!svc_getargs(xprt, xdr_u_int, &interval)) {
			exc_errlog(LOG_WARNING, errno, "setinterval: get argument");
			svcerr_decode(xprt);
			break;
		}

		sc = getclient(xprt);
		if (sc == 0
		    || sc->sc_state != STOPPED
		    || svcsw[sc->sc_service].svc_setinterval == 0)
			status = SNOOPERR_INVAL;
		else {
			(*svcsw[sc->sc_service].svc_setinterval)(sc, interval);
			status = SNOOP_OK;
		}

		error = nb_sendreply(xprt, xdr_enum, &status, sizeof status);
		if (error)
			exc_errlog(LOG_WARNING, error, "setinterval: send reply");
		break;
	    }

	    default: 
		svcerr_noproc(xprt);
	}
}

/*
 * Common subscribe function.
 */
static enum snoopstat
subscribe(SVCXPRT *xprt, struct authunix_parms *cred, enum svctype service,
	  char *interface, u_int buffer, u_int count, u_int interval,
	  int *protoid)
{
	struct sockaddr_in *sin;
	struct iftab *ift;
	SnoopClient *sc;
	struct svcops *s;
	int error, on = 1;

	/*
	 * Don't allow multiple concurrent subscriptions
	 */
	(void) unsubscribe(xprt);

	/*
	 * XXX - Here is a nasty one!  Rewrite the service transport
	 * function table to call our destroy function rather than the
	 * standard one.
	 */
	if (std_destroy == 0) {
		std_destroy = xprt->xp_ops->xp_destroy;
		xprt->xp_ops->xp_destroy = client_destroy;
	}

	/*
	 * Check authorization
	 */
	sin = svc_getcaller(xprt);
	if (authorize(sin->sin_addr, cred, service) <= 0) {
		exc_errlog(LOG_INFO, 0, "authorization failure: %s:%d.%d/%d",
				inet_ntoa(sin->sin_addr),
				cred->aup_uid, cred->aup_gid, service);
		return SNOOPERR_PERM;
	}

	exc_errlog(LOG_INFO, 0, "authorization success: %s:%d.%d/%d",
				inet_ntoa(sin->sin_addr),
				cred->aup_uid, cred->aup_gid, service);

	/*
	 * Start a snooper running on the interface if not one already
	 */
	ift = iftable_match(interface);
	if (ift == 0)
		return SNOOPERR_NOIF;
	error = snoop_open(ift);
	if (error)
		return errno_to_snoopstat(error);

	/* Start the timer if this is our first client */
	if (snoopClient == 0)
		timer_start(&timer);

	if (service < SS_NETSNOOP || service > SS_ADDRLIST)
		return SNOOPERR_NOSVC;

	/* Set up the client descriptor */
	sc = new(SnoopClient);
	sc->sc_next = 0;
	sc->sc_prev = snoopClientTail;
	sc->sc_service = (int)service - 1;
	sc->sc_xprt = xprt;
	sc->sc_state = STOPPED;
	sc->sc_if = ift;
	sc->sc_blkproc = 0;
	sc->sc_blkarg = 0;
	sc->sc_snooplen = ift->if_mtu;
	sc->sc_errflags = 0;
	sc->sc_seq = 0;
	sc->sc_sddrops = 0;
	sc->sc_ifdrops = 0;
	sc->sc_sbdrops = 0;
	(*svcsw[sc->sc_service].svc_subscribe)(sc, buffer, count, interval);

	if (snoopClientTail != 0) {
		snoopClientTail->sc_next = sc;
		snoopClientTail = sc;
	} else
		snoopClient = snoopClientTail = sc;

	/* Set non-blocking I/O on the socket */
	(void) ioctl(xprt->xp_sock, FIONBIO, &on);

	/* Enter into hash table */
	in_enter(xprtindex, xprt, sc);

	*protoid = ift->if_snooper->sn_rawproto->pr_id;
	return SNOOP_OK;
}

/*
 * Common unsubscribe from a service
 */
static enum snoopstat
unsubscribe(SVCXPRT *xprt)
{
	SnoopClient *sc;
	struct iftab *ift;

	sc = getclient(xprt);
	if (sc == 0)
		return SNOOPERR_INVAL;
	ift = sc->sc_if;
	if (sc->sc_state != STOPPED) {
		sc->sc_state = STOPPED;
		snoop_stop(ift);
	}

	/* Destroy snooper if last client for the interface */
	snoop_close(ift);
	if (ift->if_clients != 0 && sc->sc_snooplen >= ift->if_snooplen)
		trim_snooplen(sc, ift);

	if (sc->sc_errflags != 0)
		snoop_seterrflags(sc->sc_if, 0);
	(*svcsw[sc->sc_service].svc_unsubscribe)(sc);
	sc->sc_state = ZOMBIE;
	return SNOOP_OK;
}

static void
trim_snooplen(SnoopClient *sc, struct iftab *ift)
{
	SnoopClient *tsc;
	u_int maxsnooplen = 0;

	/*
	 * See if we can turn down the snooplen
	 */
	for (tsc = snoopClient; tsc != 0; tsc = tsc->sc_next) {
		if (tsc != sc && tsc->sc_if == ift &&
		    tsc->sc_snooplen > maxsnooplen) {
			maxsnooplen = tsc->sc_snooplen;
			if (maxsnooplen == ift->if_snooplen)
				break;
		}
	}
	if (maxsnooplen < ift->if_snooplen) {
		snoop_setsnooplen(ift, maxsnooplen);
		exc_errlog(LOG_DEBUG, 0, "reset snooplen down to %d",
			maxsnooplen);
	}
}

/*
 * Index hashing and comparing functions
 */
/* ARGSUSED */
static u_int
xprthash(u_int *key, int size)
{
	return *key >> 5;
}

/* ARGSUSED */
static int
xprtcmp(u_int *key1, u_int *key2, int size)
{
	return *key1 != *key2;
}

/*
 * Return a pointer to the snoopclient structure given its transport
 */
static SnoopClient *
getclient(SVCXPRT *xprt)
{
	return in_match(xprtindex, xprt);
}

/*
 * Process snooped packets
 */
/* ARGSUSED */
static int
process_packet(PacketBuf *pb, Packet *p, void *v)
{
	int n, error;
	SnoopClient *sc;
	struct iftab *ift = v;

	for (n = 0; n < lengthof(svcsw); n++) {
		if (svcsw[n].svc_preprocess)
			(*svcsw[n].svc_preprocess)();
	}
	for (sc = snoopClient; sc != 0; sc = sc->sc_next) {
		if (sc->sc_if != ift
		    || sc->sc_state == STOPPED
		    || (ift->if_file == 0
			&& timercmp(&p->p_sp.sp_hdr.snoop_timestamp,
				    &sc->sc_starttime, <))) {
			continue;
		}
		if (sc->sc_state == BLOCKED) {
			sc->sc_sddrops++;
			continue;
		}

		error = (*svcsw[sc->sc_service].svc_process)(sc, pb, p);
		if (error) {
			exc_errlog(LOG_WARNING, error,
				   "error processing packet");
		}
	}

	return 0;
}

static void
client_delete(SnoopClient *sc)
{
	/* Stop timer if this is the last client */
	if (sc == snoopClient && sc->sc_next == 0)
		timer_stop(&timer);

	/* Pull off lists */
	if (sc == snoopClient)
		snoopClient = sc->sc_next;
	else
		sc->sc_prev->sc_next = sc->sc_next;
	if (sc == snoopClientTail)
		snoopClientTail = sc->sc_prev;
	else
		sc->sc_next->sc_prev = sc->sc_prev;

	/* Delete */
	in_remove(xprtindex, sc->sc_xprt);
	delete(sc);
}

static void
client_destroy(SVCXPRT *xprt)
{
	SnoopClient *sc = getclient(xprt);

	if (sc != 0) {
		if (sc->sc_state != ZOMBIE)
			(void) unsubscribe(xprt);
		client_delete(sc);
	}
	(*std_destroy)(xprt);
	exc_errlog(LOG_DEBUG, 0, "destroyed %X", xprt);
}

/*
 * Timer callback function to send unsolicted replies to clients
 */
static void
do_timer(void)
{
	SnoopClient *sc;
	void (*callout)(SnoopClient *);

	for (sc = snoopClient; sc != 0; sc = sc->sc_next) {
		if (sc->sc_blkproc != 0) {
			if (nb_sendreply(sc->sc_xprt, sc->sc_blkproc,
					 sc->sc_blkarg, 0) != 0)
				continue;
			if (sc->sc_blkproc == 0)
				exc_errlog(LOG_INFO, 0,
					"do_timer: sent blocked reply");
		}

		if (sc->sc_state == STOPPED || sc->sc_state == ZOMBIE)
			continue;
		callout = svcsw[sc->sc_service].svc_timer;
		if (callout)
			(*callout)(sc);
	}
}

/*
 * Send a non-blocking RPC reply.
 *
 * If the call succeeds:
 *	sc->sc_blkproc will be set to 0,
 *	sc->sc_blkarg will be freed if it was not 0,
 *	the return value will be 0.
 *
 * If the send is blocked:
 *	the proc and arg will be saved for later delivery,
 *	the return value will be 0.
 *
 * If an error other than EWOULDBLOCK or EINTR occurs:
 *	sc->sc_blkproc will be set to 0,
 *	sc->sc_blkarg will be freed if it was not 0,
 *	the return value will be an errno.
 *
 * NB: This does not work for batching clients.
 */
static int
nb_sendreply(SVCXPRT *xprt, xdrproc_t proc, void *arg, u_int size)
{
	SnoopClient *sc = getclient(xprt);	/* XXX caller already did */ 
	int error;

	if (svc_sendreply(xprt, proc, arg)) {
		/* Successful */
		if (sc != 0 && sc->sc_blkproc != 0) {
			if (sc->sc_blkarg != 0) {
				delete(sc->sc_blkarg);
				sc->sc_blkarg = 0;
			}
			sc->sc_blkproc = 0;
		}
		return 0;
	}
	if (sc == 0)
		return EWOULDBLOCK;

	error = errno;
	switch (error) {
	    case EINTR:
	    case EWOULDBLOCK:
		sc->sc_blkproc = proc;
		if (size != 0) {
			if (sc->sc_blkarg != 0)
				delete(sc->sc_blkarg);
			sc->sc_blkarg = tnew(void, size);
			bcopy(arg, sc->sc_blkarg, size);
		}
		exc_errlog(LOG_NOTICE, error,
				"nb_sendreply: blocked reply");
		if (sc->sc_state == STARTED)
			sc->sc_state = BLOCKED;
		return 0;

	    default:
		sc->sc_blkproc = 0;
		if (sc->sc_blkarg != 0) {
			delete(sc->sc_blkarg);
			sc->sc_blkarg = 0;
		}
		return error;
	}
}
