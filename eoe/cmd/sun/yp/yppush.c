#ifndef lint
static char sccsid[] = 	"@(#)yppush.c	1.1 88/03/07 4.0NFSSRC Copyr 1988 Sun Micro";
#endif

#include <stdio.h>
#include <sys/param.h>
#include <errno.h>
#include <sys/dir.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <ctype.h>
#include <netdb.h>
#include <rpc/rpc.h>
#include <sys/socket.h>
#include <rpc/pmap_clnt.h>
#include <rpcsvc/ypclnt.h>
#include <rpcsvc/yp_prot.h>
#include <rpcsvc/ypv1_prot.h>
#include <syslog.h>
#include "ypdefs.h"
#ifdef sgi
#include "ns_api.h"
#endif

#define INTER_TRY 12			/* Seconds between tries */
#define TIMEOUT INTER_TRY*4		/* Total time for timeout */
#define GRACE_PERIOD 180		/* time for last ypxfr */

#define DEBUG(x) /* printf x debug macro */;

USE_YPDBPATH
char *domain = NULL;
char default_domain_name[YPMAXDOMAIN];
char *map = NULL;
bool verbose = FALSE;
bool callback_timeout = FALSE;	/* set when a callback times out */
char ypmapname[MAXPATHLEN];	/* Used to check for the map's existence */
int  parallelpush = 5;		/* # of pushes to do in parallel, default */
int  Semaphore;		        /* Semaphore to enforce parallelism */

struct timeval udp_intertry = {
	INTER_TRY,			/* Seconds */
	0				/* Microseconds */
};
struct timeval udp_timeout = {
	TIMEOUT,			/* Seconds */
	0				/* Microseconds */
};
SVCXPRT *transport;
struct server {
	struct server *pnext;
	char name[YPMAXPEER];
	struct dom_binding domb;
	unsigned long xactid;
	unsigned short state;
	unsigned long status;
	bool oldvers;
};
struct server *server_list = (struct server *) NULL;

/*  State values for server.state field */

#define SSTAT_INIT 0
#define SSTAT_CALLED 1
#define SSTAT_RESPONDED 2
#define SSTAT_PROGNOTREG 3
#define SSTAT_RPC 4
#define SSTAT_RSCRC 5
#define SSTAT_SYSTEM 6

char err_usage[] =
"Usage:\n\
	yppush [-d <domainname>] [-v] [-p <parallelism>] map\n";
char err_bad_args[] =
	"The %s argument is bad.\n";
char err_null_kname[] =
	"The %s hasn't been set on this machine.\n";
char err_bad_domainname[] = "domainname";
char err_cant_bind[] =
	"Can't find NIS server for domain %s.  Reason:  %s.\n";
char err_cant_build_serverlist[] =
	"Can't build server list from map \"ypservers\".  Reason:  %s.\n";
/*
 * State_duple table.  All messages should take 1 arg - the node name.
 */
struct state_duple {
	int state;
	char *state_msg;
};
struct state_duple state_duples[] = {
	{SSTAT_INIT, "Internal error with %s"},
	{SSTAT_CALLED, "%s called"},
	{SSTAT_RESPONDED, "%s (v1 ypserv) sent an old-style request"},
	{SSTAT_PROGNOTREG, "NIS server not registered at %s"},
	{SSTAT_RPC, "RPC error to %s:  "},
	{SSTAT_RSCRC, "Local resource allocation failure for %s"},
	{SSTAT_SYSTEM, "System error with %s: "},
	{0, (char *) NULL}
};
/*
 * Status_duple table.  No messages should require any args.
 */
struct status_duple {
	long status;
	char *status_msg;
};
struct status_duple status_duples[] = {
	{YPPUSH_SUCC, "success"},
	{YPPUSH_AGE,  "version up to date"},
	{YPPUSH_NOMAP, "Failed - ypxfr can't find master"},
	{YPPUSH_NODOM, "Failed - domain not supported"},
	{YPPUSH_RSRC, "Failed - local resource failure."},
	{YPPUSH_RPC, "Failed - ypxfr RPC failure"},
	{YPPUSH_MADDR, "Failed - ypxfr couldn't get master's address"},
	{YPPUSH_YPERR, "Failed - NIS server or map format error"},
	{YPPUSH_BADARGS, "Failed - bad arguments to ypxfr"},
	{YPPUSH_DBM, "Failed - dbm operation on map failed"},
	{YPPUSH_FILE, "Failed - file I/O operation on map failed"},
	{YPPUSH_SKEW, "Failed - map version skew during transfer"},
	{YPPUSH_CLEAR, "Map transferred, but \"Clear map\" problem"},
	{YPPUSH_FORCE, "Failed - no order number in map; use ypxfr -f"},
	{YPPUSH_XFRERR, "Failed - ypxfr internal error"},
	{YPPUSH_REFUSED, "Failed - transfer request refused"},
	/*
	 * An interal error code only used here (never should be printed)
	 */
#define YPPUSH_INTERNAL 999
	{YPPUSH_INTERNAL, "Failed - never got to the host"},
	{0, (char *) NULL}
};
/*
 * rpcerr_duple table
 */
struct rpcerr_duple {
	enum clnt_stat rpc_stat;
	char *rpc_msg;
};
struct rpcerr_duple rpcerr_duples[] = {
	{RPC_SUCCESS, "RPC success"},
	{RPC_CANTENCODEARGS, "RPC Can't encode args"},
	{RPC_CANTDECODERES, "RPC Can't decode results"},
	{RPC_CANTSEND, "RPC Can't send"},
	{RPC_CANTRECV, "RPC Can't recv"},
	{RPC_TIMEDOUT, "NIS server registered, but does not respond"},
	{RPC_VERSMISMATCH, "RPC version mismatch"},
	{RPC_AUTHERROR, "RPC auth error"},
	{RPC_PROGUNAVAIL, "RPC remote program unavailable"},
	{RPC_PROGVERSMISMATCH, "RPC program mismatch"},
	{RPC_PROCUNAVAIL, "RPC unknown procedure"},
	{RPC_CANTDECODEARGS, "RPC Can't decode args"},
	{RPC_UNKNOWNHOST, "unknown host"},
	{RPC_PMAPFAILURE, "portmap failure (host down?)"},
	{RPC_PROGNOTREGISTERED, "NIS server not registered"},
	{RPC_SYSTEMERROR, "RPC system error"},
	{RPC_SUCCESS, (char *) NULL}		/* Duplicate rpc_stat unused
					         *  in list-end entry */
};

/* number of ypservers */
int  no_of_ypservers = 0;

void get_default_domain_name();
void get_command_line_args();
unsigned short send_message();
void make_server_list();
void add_server();
void generate_callback();
void xactid_seed();
void main_loop();
void listener_exit();
void listener_dispatch();
void print_state_msg();
void print_callback_msg();
char *rpcerr_msg();
void get_xfr_response();
void set_time_up();

extern unsigned long inet_addr();
extern struct rpc_createerr rpc_createerr;
extern void yp_make_dbname();

void
cleanup()
{
  semDestroy(Semaphore);
  exit(1);
}

void
main (argc, argv)
	int argc;
	char **argv;

{
	unsigned long program;
	unsigned short port;
	struct	stat	sbuf;
	char dbname[MAXNAMLEN];

	openlog("yppush", LOG_PID | LOG_ODELAY | LOG_PERROR, LOG_USER);

	get_command_line_args(argc, argv);

	if (!domain) {
		get_default_domain_name();
	}

	/* check to see if the map exists in this domain */
#ifdef sgi
	(void) sprintf(ypmapname,"%s%s/%s.m",NS_DOMAINS_DIR,domain,map);
#else
	yp_make_dbname(map, dbname, sizeof (dbname));
	(void) sprintf(ypmapname,"%s/%s/%s.dir",ypdbdir,domain,dbname);
#endif
	if (stat(ypmapname,&sbuf) < 0) {
		syslog(LOG_CRIT, "Map does not exist: %s\n", ypmapname);
		exit(1);
	}

	make_server_list();

	/*
	 * All process exits after the call to generate_callback should be
	 * through listener_exit(program, status), not exit(status), so the
	 * transient server can get unregistered with the portmapper.
	 */

	generate_callback(&program, &port, &transport);

	main_loop(program, port);

	listener_exit(program, 0);
	/*NOTREACHED*/
}

/*
 * This does the command line parsing.
 */
void
get_command_line_args(argc, argv)
	int argc;
	char **argv;

{
	argv++;

	if (argc < 2) {
		(void) fprintf(stderr, err_usage);
		exit(1);
	}

	while (--argc) {

		if ( (*argv)[0] == '-') {

			switch ((*argv)[1]) {

			case 'v':
				verbose = TRUE;
				argv++;
				break;

			case 'd': {

				if (argc > 1) {
					argv++;
					argc--;
					domain = *argv;
					argv++;

					if (strlen(domain) > YPMAXDOMAIN) {
						(void) fprintf(stderr,
						    err_bad_args,
						    err_bad_domainname);
						exit(1);
					}

				} else {
					(void) fprintf(stderr, err_usage);
					exit(1);
				}

				break;
			}

			case 'p': {
			  if (argc > 1) {
			    argv++;
			    argc--;
			    parallelpush = atoi(*argv);
			    argv++;
			    
			    if (parallelpush > 32 || parallelpush < 1) {
			      (void) fprintf(stderr,
		     "Parallelism must be between 1 and 32\n");
			      exit(1);
			    }
			  }
			  break;
			}

			default: {
				(void) fprintf(stderr, err_usage);
				exit(1);
			}

			}

		} else {

			if (!map) {
				map = *argv;
			} else {
				(void) fprintf(stderr, err_usage);
				exit(1);
			}

			argv++;

		}
	}

	if (!map) {
		(void) fprintf(stderr, err_usage);
		exit(1);
	}
}

/*
 *  This gets the local kernel domainname, and sets the global domain to it.
 */
void
get_default_domain_name()
{
	(void) getdomainname(default_domain_name, YPMAXDOMAIN);
	domain = default_domain_name;
	if (strlen(domain) == 0) {
		syslog(LOG_CRIT, err_null_kname, err_bad_domainname);
		exit(1);
	}
}

/*
 * This uses NIS operations to retrieve each server name in the map
 *  "ypservers".  add_server is called for each one to add it to the list of
 *  servers.
 */
void
make_server_list()
{
	char *key;
	int keylen;
	char *outkey;
	int outkeylen;
	char *val;
	int vallen;
	int err;
	char *ypservers = "ypservers";
	int count;

	if (verbose) {
	    (void) printf("Finding NIS servers:");
	    (void) fflush(stdout); 
	    count = 4;
	}
	if (err = yp_bind(domain) ) {
	        syslog(LOG_CRIT, err_cant_bind, domain,
		    yperr_string(err) ); 
		exit(1);
	}

	if (err = yp_first(domain, ypservers, &outkey, &outkeylen,
	    &val, &vallen) ) {
		(void) syslog(LOG_CRIT, err_cant_build_serverlist,
		     yperr_string(err) );
		exit(1);
	}

	while (TRUE) {
		add_server(outkey, outkeylen);
		if (verbose) {
		    (void) printf(" %s", outkey);
		    (void) fflush(stdout);
		    if (count++ == 8) {
		    	(void) printf("\n");
		        count = 0;
		    }
		}
		no_of_ypservers++;
		free(val);
		key = outkey;
		keylen = outkeylen;

		if (err = yp_next(domain, ypservers, key, keylen,
		    &outkey, &outkeylen, &val, &vallen) ) {

			if (err == YPERR_NOMORE) {
				break;
			} else {
			  	(void) syslog(LOG_CRIT,
				    err_cant_build_serverlist,
				    yperr_string(err) );
				exit(1);
			}
		}

		free(key);
	}
	if (count != 0) {
	    (void) printf("\n");
	    (void) fflush(stdout);
	}
}

/*
 *  This adds a single server to the server list.  The servers name is
 *  translated to an IP address by calling gethostbyname(3n), which will
 *  probably make use of NIS services.
 */
void
add_server(name, namelen)
	char *name;
	int namelen;
{
	struct server *ps;
	struct hostent *h;
	static unsigned long seq;
	static unsigned long xactid = 0;

	if (xactid == 0) {
		xactid_seed(&xactid);
	}

	ps = (struct server *) malloc( (unsigned) sizeof (struct server));

	if (ps == (struct server *) NULL) {
		(void) syslog(LOG_CRIT, "yppush malloc failure");
		exit(1);
	}

	name[namelen] = '\0';
	(void) strcpy(ps->name, name);
	ps->state = SSTAT_INIT;
	ps->status = 0;
	ps->oldvers = FALSE;

	if (h = (struct hostent *) gethostbyname(name) ) {
		ps->domb.dom_server_addr.sin_addr =
		    *((struct in_addr *) h->h_addr);
		ps->domb.dom_server_addr.sin_family = AF_INET;
		ps->domb.dom_server_addr.sin_port = 0;
		ps->domb.dom_server_port = 0;
		ps->domb.dom_socket = RPC_ANYSOCK;
		ps->xactid = xactid + seq++;
		ps->pnext = server_list;
		server_list = ps;
	} else {
		(void) syslog(LOG_CRIT, "Can't get an address for server %s.\n",
		    name);
		free(ps);
	}
}

/*
 * This sets the base range for the transaction ids used in speaking the the
 *  server ypxfr processes.
 */
void
xactid_seed(xactid)
unsigned long *xactid;
{
	struct timeval t;

	if (gettimeofday(&t, (struct timezone *) NULL) == -1) {
		perror("yppush gettimeofday failure");
		*xactid = 1234567;
	} else {
		*xactid = t.tv_sec;
	}
}

/*
 *  This generates the udp channel which will be used as the listener process'
 *  service rendezvous point, and comes up with a transient program number
 *  for the use of the RPC messages from the ypxfr processes.
 */
void
generate_callback(program, port, transport)
	unsigned long *program;
	unsigned short *port;
	SVCXPRT **transport;
{
	long unsigned prognum;
	SVCXPRT *xport;

	if ((xport = svcudp_create(RPC_ANYSOCK) ) == (SVCXPRT *) NULL) {
		(void) syslog(LOG_CRIT, "Can't set up as a udp server.\n");
		exit(1);
	}

	*port = xport->xp_port;
	*transport = xport;
	prognum = 0x40000000;

	while (!pmap_set(prognum++, YPPUSHVERS, IPPROTO_UDP, xport->xp_port) ) {
		;
	}

	*program = --prognum;
}


/*
 * This is the main loop. Send messages to each server,
 * and then wait for a response.
 */
void
main_loop(program, port)
	unsigned long program;
	unsigned short port;
{
  	pid_t child_pid;

	fd_set readfds;
	register struct server *ps;
	long error;

	/* create semaphore to control children */
	Semaphore = semCreate(parallelpush);

	if (!svc_register(transport, program, YPPUSHVERS,
	    listener_dispatch, 0) ) {
		(void) syslog(LOG_CRIT,
		    "Can't set up transient callback server.\n");
	}

	(void) signal(SIGINT, cleanup);
	(void) signal(SIGTERM, cleanup);
	(void) signal(SIGHUP, cleanup);
	(void) signal(SIGALRM, set_time_up);

	callback_timeout = FALSE;
	child_pid = fork();
	if (child_pid == 0) {
	    int servers_not_done = 0;
	    /*
	     * This process just becomes a server and waits for all the
	     * call-backs to happen.
	     */

	    (void) alarm(GRACE_PERIOD);
	    do {
	      readfds = svc_fdset;
	      errno = 0;
	      switch ( (int) select(FD_SETSIZE, &readfds, NULL, NULL, NULL) ) {

	      case -1:		
		if (errno != EINTR) {
		  (void) perror("main loop select");
		  callback_timeout = TRUE;
		  semUp(Semaphore);
		  error = semVal(Semaphore);
		  DEBUG(("value for semaUp = %d\n", error));
		}
		break;

	      case 0:
		(void) syslog(LOG_CRIT,
				"Invalid timeout in main loop select.\n");
		break;

	      default:
		(void) alarm(GRACE_PERIOD);
		svc_getreqset(&readfds);
		break;
	      } /* switch */

	      servers_not_done = 0;
	      for (ps = server_list; ps; ps = ps->pnext) {
		if (ps->state != SSTAT_RESPONDED) 
		    servers_not_done++;
	      }

	    } while ( callback_timeout == FALSE && servers_not_done != 0);

  	    (void) alarm(0);

	    for (ps = server_list; ps; ps = ps->pnext) {
		if (ps->state != SSTAT_RESPONDED) 
	            (void) syslog(LOG_ERR,
			     "No response from ypxfr on %s\n", ps->name);
	    }

	    exit(0);
	}


	for (ps = server_list; ps; ps = ps->pnext) {

	    semDown(Semaphore);
	    error = semVal(Semaphore);
	    DEBUG(("after semDown value = %d\n", error));

	    child_pid = fork();
	    if (child_pid == 0) {
		    ps->state = send_message(ps, program, port, &error);
		    print_state_msg(ps, error);
		    exit(0);
  	    }

	    /*
	     * Do a polling wait here to collect any zombies
	     */
	    do {
		    child_pid = waitpid((pid_t) -1, (int *)NULL, WNOHANG);
		    if (child_pid > 0)
		      DEBUG(("collected %d\n", child_pid));
		  } while (child_pid > 0);
	} /* for each server */
	/*
	 * Do a hanging wait to collect any others
	 */
	do {
	  child_pid = waitpid((pid_t) -1, (int *)NULL, 0);
	  if (child_pid > 0)
	    DEBUG(("collected %d\n", child_pid));
	} while (child_pid > 0);
	semDestroy(Semaphore);
}

/*
 * This does the listener process cleanup and process exit.
 */
void
listener_exit(program, stat)
	unsigned long program;
	int stat;
{
	(void) pmap_unset(program, YPPUSHVERS);
	exit(stat);
}

/*
 * This is the listener process' RPC service dispatcher.
 */
void
listener_dispatch(rqstp, transp)
	struct svc_req *rqstp;
	SVCXPRT *transp;
{
	switch (rqstp->rq_proc) {

	case YPPUSHPROC_NULL:
		if (!svc_sendreply(transp, xdr_void, 0) ) {
			(void) syslog(LOG_CRIT,
			    "Can't reply to rpc call.\n");
		}

		break;

	case YPPUSHPROC_XFRRESP:
		get_xfr_response(rqstp, transp);
		break;

	default:
		svcerr_noproc(transp);
		break;
	}
}


/*
 *  This dumps a server state message to stdout.  It is called in cases where
 *  we have no expectation of receiving a callback from the remote ypxfr.
 */
void
print_state_msg(s, e)
	struct server *s;
	long e;
{
	struct state_duple *sd;

	if (s->state == SSTAT_SYSTEM)
		return;			/* already printed */
	if (!verbose && ( s->state == SSTAT_RESPONDED ||
			  s->state == SSTAT_CALLED) )
		return;

	for (sd = state_duples; sd->state_msg; sd++) {
		if (sd->state == s->state) {
  			(void) printf("yppush[%d]: ", getpid());
  			(void) printf(sd->state_msg, s->name);
			
			if (s->state == SSTAT_RPC) {
				(void) printf(rpcerr_msg((enum clnt_stat) e));
			}

			(void) printf("\n");
			(void) fflush(stdout);
			return;
		}
	}

	(void) syslog(LOG_CRIT,
	  "yppush: Bad server state value %u.\n", s->state);
}

/*
 *  This logs a transfer status message.  It is called in 
 *  response to a received RPC message from the called ypxfr.
 */
void
print_callback_msg(s)
	struct server *s;
{
	register struct status_duple *sd;

	if (!verbose && (s->status==YPPUSH_AGE) || (s->status==YPPUSH_SUCC))
		return;
	if (s->status == YPPUSH_INTERNAL)
		return;
	for (sd = status_duples; sd->status_msg; sd++) {

		if (sd->status == s->status) {
			if ((s->status == YPPUSH_AGE) ||
			    (s->status==YPPUSH_SUCC)) {
			  (void) printf("yppush[%d]: From %s about %s: %s\n",
					getpid(),
					s->name, map, sd->status_msg);
			  (void) fflush(stdout);
			} else {
			  (void) syslog(LOG_CRIT,
			    "From %s about %s: %s",
			    s->name, map, sd->status_msg);
			}
			return;
		}
	}

	(void) syslog(LOG_CRIT,
	"yppush listener: Unknown transaction status (value %u) from ypxfr on %s about %s.\n",
	    s->status, s->name, map);
}

/*
 *  This dumps an RPC error message to stdout.  This is basically a rewrite
 *  of clnt_perrno, but writes to stdout instead of stderr.
 */
char *
rpcerr_msg(e)
	enum clnt_stat e;
{
	struct rpcerr_duple *rd;

	for (rd = rpcerr_duples; rd->rpc_msg; rd++) {

		if (rd->rpc_stat == e) {
			return(rd->rpc_msg);
		}
	}

	return("Bad error code passed to rpcerr_msg");
}

/*
 * This picks up the response from the ypxfr process which has been started
 * up on the remote node.  The response status must be non-zero, otherwise
 * the status will be set to "ypxfr error".
 */
/*ARGSUSED*/
void
get_xfr_response(rqstp, transp)
	struct svc_req *rqstp;
	SVCXPRT *transp;
{
	struct yppushresp_xfr resp;
	register struct server *s;
	long error;

	if (!svc_getargs(transp, xdr_yppushresp_xfr, &resp) ) {
		svcerr_decode(transp);
		return;
	}

	if (!svc_sendreply(transp, xdr_void, 0) ) {
		(void) syslog(LOG_CRIT, "Can't reply to rpc call.\n");
	}

	for (s = server_list; s; s = s->pnext) {

		if (s->xactid == resp.transid) {
			s->status  = resp.status ? resp.status: YPPUSH_XFRERR;
			print_callback_msg(s);
			s->state = SSTAT_RESPONDED;
			semUp(Semaphore);
			error = semVal(Semaphore);
			DEBUG(("value for semaUp = %d\n", error));
			return;
		}
	}

}

/*
 * This is a UNIX signal handler which is called when the
 * timer expires waiting for a callback.
 */
void
set_time_up()
{
	callback_timeout = TRUE;
	(void) signal(SIGALRM, set_time_up);
}


/*
 * This sends a message to a single ypserv process.  The return value is
 * a state value.  
 */
unsigned short
send_message(ps, program, port, err)
	struct server *ps;
	unsigned long program;
	unsigned short port;
	long *err;
{
	struct ypreq_xfr req;
	struct yprequest oldreq;
	enum clnt_stat s;
	char my_name[YPMAXPEER +1];
	struct rpc_err rpcerr;
	struct yppushresp_xfr resp;

	if (gethostname(my_name, sizeof (my_name) ) == -1) {
		return(SSTAT_RSCRC);
	}
	if ((ps->domb.dom_client = clntudp_create(&(ps->domb.dom_server_addr),
	    YPPROG, YPVERS, udp_intertry, &(ps->domb.dom_socket)))  == NULL) {

		(void) syslog(LOG_ERR, "Error with %s: %s", ps->name,
				      rpcerr_msg(rpc_createerr.cf_stat));
		/*
		 * We now send a message to our call-back to avoid waiting
		 * (the server will NEVER respond, so why wait?)
		 */
		resp.status = YPPUSH_INTERNAL;
		resp.transid = ps->xactid;
		(void) callrpc(my_name, program, YPPUSHVERS,
			       YPPUSHPROC_XFRRESP, xdr_yppushresp_xfr, &resp,
			       xdr_void, NULL);
		return(SSTAT_SYSTEM);
	}

	req.ypxfr_domain = domain;
	req.ypxfr_map = map;
	req.ypxfr_ordernum = 0;
	req.ypxfr_owner = my_name;
	req.transid = ps->xactid;
	req.proto = program;
	req.port = port;

	s = (enum clnt_stat) clnt_call(ps->domb.dom_client, YPPROC_XFR,
	    xdr_ypreq_xfr, &req, xdr_void, 0, udp_timeout);
	clnt_geterr(ps->domb.dom_client, &rpcerr);
	clnt_destroy(ps->domb.dom_client);
	(void) close(ps->domb.dom_socket);

	if (s == RPC_SUCCESS) {
		return (SSTAT_CALLED);
	} else {

		if (s == RPC_SUCCESS) {
			return (SSTAT_RESPONDED);
		} else {
			*err = (long) rpcerr.re_status;
			return (SSTAT_RPC);
		}
	}
}
