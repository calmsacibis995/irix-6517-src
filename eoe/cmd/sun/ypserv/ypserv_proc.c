#ifndef lint
static char sccsid[] = 	"@(#)ypserv_proc.c	1.1 88/03/07 4.0NFSSRC Copyr 1988 Sun Micro";
#endif

/*
 * This contains NIS server code which supplies the set of
 * functions requested using rpc.   The top level functions in this module
 * are those which have symbols of the form YPPROC_xxxx defined in
 * yp_prot.h, and symbols of the form YPOLDPROC_xxxx defined in ypsym.h.
 * The latter exist to provide compatibility to the old version of the NIS
 * protocol/server, and may emulate the behaviour of the previous software
 * by invoking some other program.
 *
 * This module also contains functions which are used by (and only by) the
 * top-level functions here.
 *
 */

#include <ndbm.h>
#include <setjmp.h>
#include <values.h>
#include <arpa/inet.h>
#include "ypsym.h"
#include "ypdefs.h"
USE_YP_PREFIX

#define vfork fork
extern char *environ;

#ifndef YPXFR_PROC
#define YPXFR_PROC "/usr/sbin/ypxfr"
#endif
static char ypxfr_proc[] = YPXFR_PROC;
#ifndef YPPUSH_PROC
#define YPPUSH_PROC "/usr/sbin/yppush"
#endif
static char yppush_proc[] = YPPUSH_PROC;
struct yppriv_sym {
	char *sym;
	unsigned len;
};
static char err_fork[] = "%s: %s fork failure: %s\n";
#define FORK_ERR(fun) logprintf( err_fork, progname, fun, strerror(errno))
static char err_execl[] = "%s: %s execl failure: %s\n";
#define EXEC_ERR(fun) logprintf( err_execl, progname, fun, strerror(errno))
static char err_respond[] = "%s: %s can't respond to rpc request.\n";
#define RESPOND_ERR(fun) logprintf( err_respond, progname, fun)
static char err_free[] = "%s: %s can't free args.\n";
#define FREE_ERR(fun) logprintf( err_free, progname, fun)
static char err_map[] = "%s/%s: '%s': no such map or access denied.\n";
#define MAP_ERR(fun, map) logprintf( err_map, progname, fun, map)

static void 
    ypfilter(datum *inkey, datum *outkey, datum *val, long unsigned *status);
static bool isypsym(datum *key);
static bool_t xdrypserv_ypall(XDR *xdrs, struct ypreq_nokey *req);


/*
 * This determines whether or not a passed domain is served by this server,
 * and returns a boolean.  Used by both old and new protocol versions.
 */
void
ypdomain(rqstp, transp, always_respond)
	struct svc_req *rqstp;
	SVCXPRT *transp;
	bool always_respond;
{
	char domain_name[YPMAXDOMAIN + 1];
	char *pdomain_name = domain_name;
	bool isserved;
	static char fun[] = "ypdomain";

	if (!svc_getargs(transp, xdr_ypdomain_wrap_string, &pdomain_name) ) {
		svcerr_decode(transp);
		return;
	}

	isserved = (bool) ypcheck_domain(domain_name);

	if (isserved || always_respond) {

		if (!svc_sendreply(transp, xdr_bool, &isserved) ) {
			RESPOND_ERR(fun);
		}
		if (!isserved)
			logprintf("Domain %s not supported\n",
				domain_name);

	} else {
		/*
		 * This case is the one in which the domain is not
		 * supported, and in which we are not to respond in the
		 * unsupported case.  We are going to make an error happen
		 * to allow the portmapper to end his wait without the
		 * normal udp timeout period.  The assumption here is that
		 * the only process in the world which is using the function
		 * in its no-answer-if-nack form is the portmapper, which is
		 * doing the krock for pseudo-broadcast.  If some poor fool
		 * calls this function as a single-cast message, the nack
		 * case will look like an incomprehensible error.  Sigh...
		 * (The traditional Unix disclaimer)
		 */

		svcerr_decode(transp);
		logprintf("Domain %s not supported (broadcast)\n",
				domain_name);
	}
}

/*
 * Inter-domain name support
 */
USE_YP_INTERDOMAIN

/*
 * The following variables limit the number of child processes that
 * do inter-domain binding. The limit reduces system load when the
 * name server is slow in responding to lookup requests.
 */
int fork_cnt;		/* # of children doing binding, ypxfr, et al. */
int max_fork_cnt = 20;	/* can be changed with a command-line option */

#include <netdb.h>


/*
 * When we don't find a match in our local domain, this function is
 * invoked to try the match using the inter-domain binding.
 * Returns one of the following three values to deftermine
 * if we forked or not, and if so, if we are in the child or
 * the parent process.  The child blocks while it queries the name
 * server trying to answer the request, while the parent immediately
 * returns to avoid any deadlock problems.
 */
#define NEITHER_PROC 0
#define CHILD_PROC 1
#define PARENT_PROC 2
#define	ALARMED_PROC 3

/*
 * Cache of in-progress or failed name server queries.
 */
#define	QUERYKEYSIZE	40
long qc_timeout = 10 * 60;

enum querystatus { FREE = 0, IN_PROGRESS, NOT_FOUND };

struct querycache {
	char			qc_key[QUERYKEYSIZE];
	short			qc_keysize;
	short			qc_byname;
	pid_t			qc_pid;
	enum querystatus	qc_status;
	long			qc_expire;
};

struct querycache	*qc_lookup(char *, int, int);
struct querycache	*qc_enter(char *, int, int, int, enum querystatus);
void			qc_log(char *, int, datum, char *,
			       struct sockaddr_in *);

#define	QC_ENTRIES	256	/* XXX - is this reasonable? */

static struct querycache querycache[QC_ENTRIES];

static jmp_buf alrmbuf;

void
setForkCnt(int n)
{
	/*
	 * I don't think it is a good idea to have more outstanding than
	 * we have query cache entries.
	 */
	if (n >= QC_ENTRIES)
		n = QC_ENTRIES;
	max_fork_cnt = n;
}

static void
onalrm()
{
	longjmp(alrmbuf, 1);
}

int
TryInterDomain(map, keydat, valdatp, statusp, caller)
	char *map;		/* map name */
	datum keydat;		/* key to match (e.g. host name) */
	datum *valdatp;		/* returned value if found */
	long unsigned *statusp;	/* returns the status */
	struct sockaddr_in *caller;
{
	struct hostent *h;
	static char buf[1024];
	datum idkey, idval;
	int pid, byname, i;
	struct querycache *qc;
	extern bool InterDomains;
	int	retries;

	byname = !strcmp(map, "hosts.byname");
	if (!byname && strcmp(map, "hosts.byaddr")) {
		*statusp = YP_NOKEY;
		return(NEITHER_PROC);
	}
	if (!InterDomains) {
		/*
		 * See whether the map supports inter-domain attempts.
		 */
		idkey.dptr = yp_interdomain;
		idkey.dsize = yp_interdomain_sz;
		idval = fetch(idkey);
		if (idval.dptr == 0
		    || keydat.dsize == 0 || *(char *)keydat.dptr == '\0') {
			*statusp = YP_NOKEY;
			return(NEITHER_PROC);
		}
	}

	qc = qc_lookup(keydat.dptr, keydat.dsize, byname);
	if (qc) {
		if (qc->qc_status == IN_PROGRESS) {
			if (kill(qc->qc_pid, 0) == -1) {
				qc->qc_status = FREE;
				goto dofork;
			}
			/* make the client try again */
			return(PARENT_PROC);
		}
		if (log_request & LOG_QUERYCACHE) {
			qc_log("lookup", qc->qc_pid, keydat, "NOT_FOUND",
				caller);
		}
		/* cache status is NOT_FOUND */
		*statusp = YP_NOKEY;
		return(NEITHER_PROC);
	}

dofork:
	if (fork_cnt >= max_fork_cnt) {
		/*
		 * If the # of children limit is reached, return PARENT_PROC
		 * to drop the request and cause the client to try later.
		 */
		return(PARENT_PROC);
	}
	fork_cnt++;
	pid = fork();
	if (pid < 0) {
		*statusp  = YP_YPERR;
		return(NEITHER_PROC);
	}
	if (pid > 0) {
		if (log_request & LOG_QUERYCACHE) {
			qc_log("enter", pid, keydat, "IN_PROGRESS", caller);
		}
		(void) qc_enter(keydat.dptr, keydat.dsize, byname, pid,
				IN_PROGRESS);
		return(PARENT_PROC);
	}

	/*
	 * If this guy fails, no retry.
	 */
	(void) signal(SIGILL, SIG_DFL);
	(void) signal(SIGTRAP, SIG_DFL);
	(void) signal(SIGEMT, SIG_DFL);
	(void) signal(SIGFPE, SIG_DFL);
	(void) signal(SIGBUS, SIG_DFL);
	(void) signal(SIGSEGV, SIG_DFL);

	/*
	 * The malloc'ed buffer for keydat.dptr doesn't include space for
	 * the terminal null byte.  Copy the key to a larger buffer so we
	 * can properly terminate the string.
	 */
	strncpy(buf, keydat.dptr, keydat.dsize);
	buf[keydat.dsize] = '\0';
	if (log_request & LOG_INTERDOMAIN) {
		logprintf("interdomain: caller %s/%d, key '%s' (%d) %s\n",
			  inet_ntoa(caller->sin_addr), caller->sin_port,
			  buf, keydat.dsize, byname ? "byname": "byaddr");
	}

	if (setjmp(alrmbuf)) {
		*statusp = YP_NOKEY;
		return(ALARMED_PROC);
	}
	signal(SIGALRM, onalrm);
	/*
	 * Allow enough time for several resolver retransmissions.  From 
	 * res_send.c and res_init.c, resolver requests are resent after
	 * 5,10,20,40 seconds. Wait for 3 tries.
	 */
	alarm(50);
	retries = 0;
	do {
		h_errno = 0;
		if (byname) {
			extern struct hostent *_gethostbyname_named();

			h = _gethostbyname_named(buf);
		} else {
			u_long addr;
			extern struct hostent *_gethostbyaddr_named();

			if (!inet_isaddr(buf, &addr)) {
				h = NULL;
			} else {
				h = _gethostbyaddr_named(&addr, sizeof(addr),
							 AF_INET);
			}
		}
	} while (h == NULL && h_errno == TRY_AGAIN && ++retries <= 5);
	alarm(0);
	signal(SIGALRM, SIG_DFL);
	if (h == NULL) {
		/*
		 * NB: h_errno is either HOST_NOT_FOUND, NO_RECOVERY, or
		 * NO_DATA (same as NO_ADDRESS).  NO_ADDRESS means the name
		 * is valid but there's no address, so return YP_NOKEY.
		 *
		 * LMXXX - can be try again.
		 */
		*statusp = YP_NOKEY;
		return(ALARMED_PROC);
	}
	sprintf(buf,"%s\t%s", 
		inet_ntoa(*(struct in_addr *)(h->h_addr_list[0])),
		h->h_name);
	for (i = 0; h->h_aliases[i]; i++) {
		strcat(buf, " ");
		strcat(buf, h->h_aliases[i]);
	}
	valdatp->dptr = buf;
	valdatp->dsize = strlen(buf);
	*statusp = YP_TRUE;
	return(CHILD_PROC);
}

static struct querycache *
qc_lookup(key, keysize, byname)
	char *key;
	int keysize;
	int byname;
{
	long now;
	struct querycache *qc = querycache;
	int	i;

	now = time(0);
	for (i = 0; i < QC_ENTRIES; ++i, ++qc) {
		if (qc->qc_status == FREE) {
			continue;
		}
		if (qc->qc_expire < now) {
			qc->qc_pid = 0;
			qc->qc_status = FREE;
			continue;
		}
		if (qc->qc_byname == byname
		    && qc->qc_keysize == keysize
		    && !bcmp(qc->qc_key, key, keysize)) {
			return (qc);
		}
	}
	return (0);
}

static struct querycache *
qc_getslot()
{
	long now;
	struct querycache *qc = querycache, *oldest = 0;
	int i;

	now = time(0);
	for (i = 0; i < QC_ENTRIES; ++i, ++qc) {
		if (qc->qc_status == FREE) {
			return (qc);
		}
		if (qc->qc_expire < now) {
			qc->qc_pid = 0;
			qc->qc_status = FREE;
			return (qc);
		}
		if (!oldest || oldest->qc_expire > qc->qc_expire) {
			oldest = qc;
		}
	}
	oldest->qc_pid = 0;
	oldest->qc_status = FREE;
	return (oldest);
}

static struct querycache *
qc_enter(key, keysize, byname, pid, status)
	char *key;
	int keysize;
	int byname;
	int pid;
	enum querystatus status;
{
	struct querycache *qc;

	if (keysize > QUERYKEYSIZE) {
		return NULL;
	}
	qc = qc_getslot();
	bcopy(key, qc->qc_key, keysize);
	qc->qc_keysize = keysize;
	qc->qc_byname = byname;
	qc->qc_pid = pid;
	qc->qc_status = status;
	qc->qc_expire = time(0) + qc_timeout;
	return qc;
}

void
qc_flush(void)
{
	bzero(querycache, sizeof(querycache));
}

void
reapchild()
{
	long now;
	struct querycache *qc;
	int i;
	pid_t	pid;
	int	status;

	while ((pid = wait3(&status, WNOHANG, 0)) > 0) {
		--fork_cnt;
		for (i = 0, qc = querycache; i < QC_ENTRIES; ++i, ++qc) {
			if (qc->qc_pid == pid) {
				if (log_request & LOG_QUERYCACHE) {
					logprintf(
					    "reapchild: pid %d, %.*s (%d) exit status %#x\n",
					    qc->qc_pid, qc->qc_keysize,
					    qc->qc_key, qc->qc_keysize,
					    status);
				}
				if (status) {
					qc->qc_status = NOT_FOUND;
					qc->qc_expire = time(0) + qc_timeout;
				} else {
					qc->qc_pid = 0;
					qc->qc_status = FREE;
				}
				break;
			}
		}
#ifdef	DEBUG_QC
	{	int inprogress = 0;

		for (i = 0, qc = querycache; i < QC_ENTRIES; ++i, ++qc) {
			if (qc->qc_status == IN_PROGRESS) {
				inprogress++;
			}
		}
		if (inprogress || fork_cnt) {
			printf("-------- inprogress %d, forked %d ------\n",
			    inprogress, fork_cnt);
		}
	}
#endif
	}
}

static void
qc_log(operation, pid, keydat, status, caller)
	char *operation;
	int pid;
	datum keydat;
	char *status;
	struct sockaddr_in *caller;
{
	logprintf("qc_%s: pid %d, caller %s/%d, %.*s (%d) status %s\n",
		  operation, pid, inet_ntoa(caller->sin_addr),
		  caller->sin_port, keydat.dsize, keydat.dptr, keydat.dsize,
		  status);
}

/*
 * The following procedures are used only in the new protocol.
 */

/*
 * This implements the NIS "match" function.
 */
void
ypmatch(rqstp, transp)
	struct svc_req *rqstp;
	SVCXPRT *transp;
{
	struct ypreq_key req;
	struct ypresp_val resp;
	static char fun[] = "ypmatch";
	int didfork = NEITHER_PROC;

	req.domain = req.map = NULL;
	req.keydat.dptr = NULL;
	resp.valdat.dptr = NULL;
	resp.valdat.dsize = 0;

	if (!svc_getargs(transp, xdr_ypreq_key, &req) ) {
		svcerr_decode(transp);
		return;
	}

	if (ypset_current_map(req.map, req.domain, &resp.status) &&
	    yp_map_access(rqstp, transp, &resp.status) ) {

		resp.valdat = fetch(req.keydat);

		if (resp.valdat.dptr != NULL) {
			resp.status = YP_TRUE;
		} else {
			didfork = TryInterDomain(req.map, req.keydat, 
						 &resp.valdat, &resp.status, 
						 svc_getcaller(transp));
		}
	}

        if (didfork != PARENT_PROC
	    && !svc_sendreply(transp, xdr_ypresp_val, &resp)) {
		RESPOND_ERR(fun);
	}

	if (!svc_freeargs(transp, xdr_ypreq_key, &req) ) {
		FREE_ERR(fun);
	}
	if (didfork == CHILD_PROC) {
		exit(0);
	} else if (didfork == ALARMED_PROC) {
		exit(SIGALRM);
	}
}


/*
 * This implements the NIS "get first" function.
 */
void
ypfirst(rqstp, transp)
	struct svc_req *rqstp;
	SVCXPRT *transp;
{
	struct ypreq_nokey req;
	struct ypresp_key_val resp;
	static char fun[] = "ypfirst";

	req.domain = req.map = NULL;
	resp.keydat.dptr = resp.valdat.dptr = NULL;
	resp.keydat.dsize = resp.valdat.dsize = 0;

	if (!svc_getargs(transp, xdr_ypreq_nokey, &req) ) {
		svcerr_decode(transp);
		return;
	}

	if (ypset_current_map(req.map, req.domain, &resp.status) &&
	    yp_map_access(rqstp, transp, &resp.status) ) {
		ypfilter(NULL, &resp.keydat, &resp.valdat, &resp.status);
	}

	if (!svc_sendreply(transp, xdr_ypresp_key_val, &resp) ) {
		RESPOND_ERR(fun);
	}

	if (!svc_freeargs(transp, xdr_ypreq_nokey, &req) ) {
		FREE_ERR(fun);
	}
}

/*
 * This implements the NIS "get next" function.
 */
void
ypnext(rqstp, transp)
	struct svc_req *rqstp;
	SVCXPRT *transp;
{
	struct ypreq_key req;
	struct ypresp_key_val resp;
	static char fun[] = "ypnext";

	req.domain = req.map = req.keydat.dptr = NULL;
	req.keydat.dsize = 0;
	resp.keydat.dptr = resp.valdat.dptr = NULL;
	resp.keydat.dsize = resp.valdat.dsize = 0;

	if (!svc_getargs(transp, xdr_ypreq_key, &req) ) {
		svcerr_decode(transp);
		return;
	}

	if (ypset_current_map(req.map, req.domain, &resp.status) &&
	    yp_map_access(rqstp, transp, &resp.status) ) {
		ypfilter(&req.keydat, &resp.keydat, &resp.valdat, &resp.status);

	}

	if (!svc_sendreply(transp, xdr_ypresp_key_val, &resp) ) {
		RESPOND_ERR(fun);
	}

	if (!svc_freeargs(transp, xdr_ypreq_key, &req) ) {
		FREE_ERR(fun);
	}

}

static void
send_xfrstatus(reqp, ipaddr, status)
	struct ypreq_xfr *reqp;
	struct in_addr ipaddr;
	unsigned long status;
{
#define CALLINTER_TRY 10		/* Seconds between callback tries */
#define CALLTIMEOUT CALLINTER_TRY*6	/* Total timeout for callback */
	struct yppushresp_xfr resp;
	struct dom_binding domb;
	struct timeval udp_timeout, udp_intertry;

	resp.transid = reqp->transid;
	resp.status =  status;

	domb.dom_server_addr.sin_addr = ipaddr;
	domb.dom_server_addr.sin_family = AF_INET;
	domb.dom_server_addr.sin_port = (unsigned short) htons(reqp->port);
	domb.dom_server_port = domb.dom_server_addr.sin_port;
	domb.dom_socket = RPC_ANYSOCK;

	udp_intertry.tv_sec = CALLINTER_TRY;
	udp_timeout.tv_sec = CALLTIMEOUT;
	udp_intertry.tv_usec = udp_timeout.tv_usec = 0;

	if ((domb.dom_client = clntudp_create(&(domb.dom_server_addr),
	    (unsigned long) htons(reqp->proto), YPPUSHVERS, 
	    udp_intertry, &(domb.dom_socket) ) ) == NULL) {
		return;
	}	
	
	if ((enum clnt_stat) clnt_call(domb.dom_client,
	    YPPUSHPROC_XFRRESP, xdr_yppushresp_xfr, &resp, xdr_void, 0, 
	    udp_timeout) != RPC_SUCCESS) {
		return;
	} 
	clnt_destroy(domb.dom_client);
	(void) close(domb.dom_socket);
}

/*
 * This implements the  "transfer map" function.  It takes the domain and
 * map names and the callback information provided by the requester (yppush
 * on some node), and execs a ypxfr process to do the actual transfer.
 */
void
ypxfr(rqstp, transp)
	struct svc_req *rqstp;
	SVCXPRT *transp;
{
	struct ypreq_xfr req;
	struct ypresp_val resp;  /* not returned to the caller */
	char transid[10];
	char proto[15];
	char port[10];
	char *ipaddr;
	int pid;
	static char fun[] = "ypxfr";
	unsigned long status = YPPUSH_SUCC;

	req.ypxfr_domain = req.ypxfr_map = req.ypxfr_owner = NULL;
	req.ypxfr_ordernum = 0;
	pid = -1;

	if (!svc_getargs(transp, xdr_ypreq_xfr, &req) ) {
		svcerr_decode(transp);
		return;
	}

	(void) sprintf(transid, "%d", req.transid);
	(void) sprintf(proto, "%d", req.proto);
	(void) sprintf(port, "%d", req.port);
	ipaddr = inet_ntoa(svc_getcaller(transp)->sin_addr);

	/* Check that the map exists and is accessable */
	if (ypset_current_map(req.ypxfr_map, req.ypxfr_domain, &resp.status) &&
	    yp_map_access(rqstp, transp, &resp.status) ) {
		fork_cnt++;
		pid = vfork();
		if (pid == -1) {
			FORK_ERR(fun);
			status = YPPUSH_RSRC;
		} else if (pid == 0) {
			if (execl(ypxfr_proc, "ypxfr", "-d", req.ypxfr_domain,
			    		"-C", transid, proto, ipaddr, port,
					req.ypxfr_map, NULL)) 
				EXEC_ERR(fun);
			send_xfrstatus(&req, svc_getcaller(transp)->sin_addr,
				YPPUSH_RSRC);
			_exit(1);
		}

	} else {
		MAP_ERR(fun, req.ypxfr_map);
		status = YPPUSH_YPERR;
	}
	if (log_request & LOG_DISPATCH) {
	    logprintf("xfr: %s/%s: tid %s proto %s ipaddr %s/%s; pid %d\n",
		    req.ypxfr_domain, req.ypxfr_map,
		    transid, proto, ipaddr, port, pid);
	}

	if (!svc_sendreply(transp, xdr_void, 0) ) {
		RESPOND_ERR(fun);
	}

	/* 
	 * If ypxfr wasn't exec'd, then send a failure message to the
	 * the requesting yppush.
	 */
	if (status != YPPUSH_SUCC) {
		send_xfrstatus(&req, svc_getcaller(transp)->sin_addr, status);
	}

	if (!svc_freeargs(transp, xdr_ypreq_xfr, &req) ) {
		FREE_ERR(fun);
	}
}


/*
 * This implements the "get all" function.
 */
void
ypall(rqstp, transp)
	struct svc_req *rqstp;
	SVCXPRT *transp;
{
	struct ypreq_nokey req;
	struct ypresp_val resp;  /* not returned to the caller */
	int pid;
	static char fun[] = "ypall";

	req.domain = req.map = NULL;

	if (!svc_getargs(transp, xdr_ypreq_nokey, &req) ) {
		svcerr_decode(transp);
		return;
	}

	fork_cnt++;
	pid = fork();

	if (pid) {

		if (pid == -1) {
			FORK_ERR(fun);
		}

		if (!svc_freeargs(transp, xdr_ypreq_nokey, &req) ) {
			FREE_ERR(fun);
		}

		return;
	}

	/*
	 * access control hack:  If denied then invalidate the map name.
	 */
	ypclr_current_map();
	if (ypset_current_map(req.map, req.domain, &resp.status) &&
	    ! yp_map_access(rqstp, transp, &resp.status) ) {
		req.map[0] = '-';
	}
	/*
	 * This is the child process.  The work gets done by xdrypserv_ypall/
	 * we must clear the "current map" first so that we do not
	 * share a seek pointer with the parent server.
	 */

	if (!svc_sendreply(transp, xdrypserv_ypall, &req) ) {
		RESPOND_ERR(fun);
	}

	if (!svc_freeargs(transp, xdr_ypreq_nokey, &req) ) {
		FREE_ERR(fun);
	}

	exit(0);
}

/*
 * This implements the "get master name" function.
 */
void
ypmaster(rqstp, transp)
	struct svc_req *rqstp;
	SVCXPRT *transp;
{
	struct ypreq_nokey req;
	struct ypresp_master resp;
	char *nullstring = "";
	static char fun[] = "ypmaster";

	req.domain = req.map = NULL;
	resp.master = nullstring;
	resp.status  = YP_TRUE;

	if (!svc_getargs(transp, xdr_ypreq_nokey, &req) ) {
		svcerr_decode(transp);
		return;
	}

	if (ypset_current_map(req.map, req.domain, &resp.status)) {

		if (!ypget_map_master(req.map, req.domain, &resp.master) ) {
			resp.status = YP_BADDB;
		}
	}

	if (!svc_sendreply(transp, xdr_ypresp_master, &resp) ) {
		RESPOND_ERR(fun);
	}

	if (!svc_freeargs(transp, xdr_ypreq_nokey, &req) ) {
		FREE_ERR(fun);
	}
}

/*
 * This implements the "get order number" function.
 */
void
yporder(rqstp, transp)
	struct svc_req *rqstp;
	SVCXPRT *transp;
{
	struct ypreq_nokey req;
	struct ypresp_order resp;
	static char fun[] = "yporder";

	req.domain = req.map = NULL;
	resp.status  = YP_TRUE;
	resp.ordernum  = 0;

	if (!svc_getargs(transp, xdr_ypreq_nokey, &req) ) {
		svcerr_decode(transp);
		return;
	}

	resp.ordernum = 0;

	if (ypset_current_map(req.map, req.domain, &resp.status)) {

		if (!ypget_map_order(req.map, req.domain, &resp.ordernum) ) {
			resp.status = YP_BADDB;
		}
	}

	if (!svc_sendreply(transp, xdr_ypresp_order, &resp) ) {
		RESPOND_ERR(fun);
	}

	if (!svc_freeargs(transp, xdr_ypreq_nokey, &req) ) {
		FREE_ERR(fun);
	}
}

void
ypmaplist(rqstp, transp)
	struct svc_req *rqstp;
	SVCXPRT *transp;
{
	char domain_name[YPMAXDOMAIN + 1];
	char *pdomain_name = domain_name;
	static char fun[] = "ypmaplist";
	struct ypresp_maplist maplist;
	struct ypmaplist *tmp;

	maplist.list = (struct ypmaplist *) NULL;

	if (!svc_getargs(transp, xdr_ypdomain_wrap_string, &pdomain_name) ) {
		svcerr_decode(transp);
		return;
	}

	maplist.status = yplist_maps(domain_name, &maplist.list);

	if (!svc_sendreply(transp, xdr_ypresp_maplist, &maplist) ) {
		RESPOND_ERR(fun);
	}

	while (maplist.list) {
		tmp = maplist.list->ypml_next;
		(void) free(maplist.list);
		maplist.list = tmp;
	}
}
/*
 * The following procedures are used only to support the old protocol.
 */

/*
 * This implements the NIS "match" function.
 */
void
ypoldmatch(rqstp, transp)
	struct svc_req *rqstp;
	SVCXPRT *transp;
{
	bool dbmop_ok = TRUE;
	struct yprequest req;
	struct ypresponse resp;
	static char fun[] = "ypoldmatch";
	int didfork = NEITHER_PROC;

	req.ypmatch_req_domain = req.ypmatch_req_map = NULL;
	req.ypmatch_req_keyptr = NULL;
	resp.ypmatch_resp_valptr = NULL;
	resp.ypmatch_resp_valsize = 0;

	if (!svc_getargs(transp, _xdr_yprequest, &req) ) {
		svcerr_decode(transp);
		return;
	}

	if (req.yp_reqtype != YPMATCH_REQTYPE) {
		resp.ypmatch_resp_status = YP_BADARGS;
		dbmop_ok = FALSE;
	};

	if (dbmop_ok && ypset_current_map(req.ypmatch_req_map,
	    req.ypmatch_req_domain, &resp.ypmatch_resp_status) &&
	    yp_map_access(rqstp, transp, &resp.ypmatch_resp_status) ) {

		resp.ypmatch_resp_valdat = fetch(req.ypmatch_req_keydat);

		if (resp.ypmatch_resp_valptr != NULL) {
			resp.ypmatch_resp_status = YP_TRUE;
		} else {
			didfork = TryInterDomain(req.ypmatch_req_map,
						 req.ypmatch_req_keydat,
						 &resp.ypmatch_resp_valdat,
						 &resp.ypmatch_resp_status,
						 svc_getcaller(transp));
		}

	}

	resp.yp_resptype = YPMATCH_RESPTYPE;

        if (didfork != PARENT_PROC
	    && !svc_sendreply(transp, _xdr_ypresponse, &resp)) {
		RESPOND_ERR(fun);
	}

	if (!svc_freeargs(transp, _xdr_yprequest, &req) ) {
		FREE_ERR(fun);
	}
	if (didfork == CHILD_PROC) {
		exit(0);
	} else if (didfork == ALARMED_PROC) {
		exit(SIGALRM);
	}
}

/*
 * This implements the NIS "get first" function.
 */
void
ypoldfirst(rqstp, transp)
	struct svc_req *rqstp;
	SVCXPRT *transp;
{
	bool dbmop_ok = TRUE;
	struct yprequest req;
	struct ypresponse resp;
	static char fun[] = "ypoldfirst";

	req.ypfirst_req_domain = req.ypfirst_req_map = NULL;
	resp.ypfirst_resp_keyptr = resp.ypfirst_resp_valptr = NULL;
	resp.ypfirst_resp_keysize = resp.ypfirst_resp_valsize = 0;

	if (!svc_getargs(transp, _xdr_yprequest, &req) ) {
		svcerr_decode(transp);
		return;
	}

	if (req.yp_reqtype != YPFIRST_REQTYPE) {
		resp.ypfirst_resp_status = YP_BADARGS;
		dbmop_ok = FALSE;
	};

	if (dbmop_ok && ypset_current_map(req.ypfirst_req_map,
	    req.ypfirst_req_domain, &resp.ypfirst_resp_status) &&
	    yp_map_access(rqstp, transp, &resp.ypfirst_resp_status) ) {

		resp.ypfirst_resp_keydat = firstkey();

		if (resp.ypfirst_resp_keyptr != NULL) {
			resp.ypfirst_resp_valdat =
			    fetch(resp.ypfirst_resp_keydat);

			if (resp.ypfirst_resp_valptr != NULL) {
				resp.ypfirst_resp_status = YP_TRUE;
			} else {
				resp.ypfirst_resp_status = YP_BADDB;
			}

		} else {
			resp.ypfirst_resp_status = YP_NOKEY;
		}
	}

	resp.yp_resptype = YPFIRST_RESPTYPE;

	if (!svc_sendreply(transp, _xdr_ypresponse, &resp) ) {
		RESPOND_ERR(fun);
	}

	if (!svc_freeargs(transp, _xdr_yprequest, &req) ) {
		FREE_ERR(fun);
	}
}

/*
 * This implements the NIS "get next" function.
 */
void
ypoldnext(rqstp, transp)
	struct svc_req *rqstp;
	SVCXPRT *transp;
{
	bool dbmop_ok = TRUE;
	struct yprequest req;
	struct ypresponse resp;
	static char fun[] = "ypoldnext";

	req.ypnext_req_domain = req.ypnext_req_map = NULL;
	req.ypnext_req_keyptr = NULL;
	resp.ypnext_resp_keyptr = resp.ypnext_resp_valptr = NULL;
	resp.ypnext_resp_keysize = resp.ypnext_resp_valsize = 0;

	if (!svc_getargs(transp, _xdr_yprequest, &req) ) {
		svcerr_decode(transp);
		return;
	}

	if (req.yp_reqtype != YPNEXT_REQTYPE) {
		resp.ypnext_resp_status = YP_BADARGS;
		dbmop_ok = FALSE;
	};

	if (dbmop_ok && ypset_current_map(req.ypnext_req_map,
	    req.ypnext_req_domain, &resp.ypnext_resp_status)  &&
	    yp_map_access(rqstp, transp, &resp.ypnext_resp_status) ) {
		resp.ypnext_resp_keydat = nextkey(req.ypnext_req_keydat);

		if (resp.ypnext_resp_keyptr != NULL) {
			resp.ypnext_resp_valdat =
			    fetch(resp.ypnext_resp_keydat);

			if (resp.ypnext_resp_valptr != NULL) {
				resp.ypnext_resp_status = YP_TRUE;
			} else {
				resp.ypnext_resp_status = YP_BADDB;
			}

		} else {
			resp.ypnext_resp_status = YP_NOMORE;
		}

	}

	resp.yp_resptype = YPNEXT_RESPTYPE;

	if (!svc_sendreply(transp, _xdr_ypresponse, &resp) ) {
		RESPOND_ERR(fun);
	}

	if (!svc_freeargs(transp, _xdr_yprequest, &req) ) {
		FREE_ERR(fun);
	}

}

/*
 * This retrieves the order number and master peer name from the map.
 * The conditions for the various message fields are:
 * 	domain is filled in iff the domain exists.
 *	map is filled in iff the map exists.
 * 	order number is filled in iff it's in the map.
 * 	owner is filled in iff the master peer is in the map.
 */
void
ypoldpoll(rqstp, transp)
	struct svc_req *rqstp;
	SVCXPRT *transp;
{
	struct yprequest req;
	struct ypresponse resp;
	char *map = "";
	char *domain = "";
	char *owner = "";
	unsigned error;
	static char fun[] = "ypoldpoll";

	req.yppoll_req_domain = req.yppoll_req_map = NULL;

	if (!svc_getargs(transp, _xdr_yprequest, &req) ) {
		svcerr_decode(transp);
		return;
	}

	resp.yppoll_resp_ordernum = 0;

	if (req.yp_reqtype == YPPOLL_REQTYPE) {
		if (strcmp(req.yppoll_req_domain,"yp_private")==0 ||
		    strcmp(req.yppoll_req_map,"ypdomains")==0 ||
		    strcmp(req.yppoll_req_map,"ypmaps")==0 ) {
		  /*
		   * backward compatibility for 2.0 NIS servers
		   */
			domain = req.yppoll_req_domain;
			map = req.yppoll_req_map;
			resp.yppoll_resp_ordernum = 0;
		}

		else if (ypset_current_map(req.yppoll_req_map,
		    req.yppoll_req_domain, &error)) {
			domain = req.yppoll_req_domain;
			map = req.yppoll_req_map;
			(void) ypget_map_order(map, domain,
			    &resp.yppoll_resp_ordernum);
			(void) ypget_map_master(map, domain,
				    &owner);
		} else {

			switch (error) {

			case YP_BADDB:
				map = req.yppoll_req_map;
				/* Fall through to set the domain, too. */

			case YP_NOMAP:
				domain = req.yppoll_req_domain;
				break;
			}
		}
	}

	resp.yp_resptype = YPPOLL_RESPTYPE;
	resp.yppoll_resp_domain = domain;
	resp.yppoll_resp_map = map;
	resp.yppoll_resp_owner = owner;

	if (!svc_sendreply(transp, _xdr_ypresponse, &resp) ) {
		RESPOND_ERR(fun);
	}

	if (!svc_freeargs(transp, _xdr_yprequest, &req) ) {
		FREE_ERR(fun);
	}
}

/*
 * yppush
 */
void
yppush(rqstp, transp)
	struct svc_req *rqstp;
	SVCXPRT *transp;
{
	struct yprequest req;
	struct ypresp_val resp;  /* not returned to the caller */
	int pid = -1;
	static char fun[] = "yppush";

	req.yppush_req_domain = req.yppush_req_map = NULL;

	if (!svc_getargs(transp, _xdr_yprequest, &req) ) {
		svcerr_decode(transp);
		return;
	}

	if (!ypset_current_map(req.yppush_req_map, req.yppush_req_domain,
			       &resp.status)
	    || !yp_map_access(rqstp, transp, &resp.status)) {
		MAP_ERR(fun, req.yppush_req_map);
		pid = -2;	/* avoid FORK_ERR and yppush below */
	} else {
		fork_cnt++;
		pid = vfork();
	}

	if (pid == -1) {
		FORK_ERR(fun);
	} else if (pid == 0) {

		ypclr_current_map();

		if (execl(yppush_proc, "yppush", "-d", req.yppush_req_domain,
		    req.yppush_req_map, NULL) ) {
			EXEC_ERR(fun);
		}

		_exit(1);
	}

	if (!svc_sendreply(transp, xdr_void, 0) ) {
		RESPOND_ERR(fun);
	}

	if (!svc_freeargs(transp, _xdr_yprequest, &req) ) {
		FREE_ERR(fun);
	}
}

/*
 * This clears the current map, vforks, and execs the ypxfr process to get
 * the map referred to in the request.
 */
void
ypget(rqstp, transp)
	struct svc_req *rqstp;
	SVCXPRT *transp;
{
	struct yprequest req;
	struct ypresp_val resp;  /* not returned to the caller */
	int pid = -1;
	static char fun[] = "ypget";

	req.ypget_req_domain = req.ypget_req_map = req.ypget_req_owner = NULL;
	req.ypget_req_ordernum = 0;

	if (!svc_getargs(transp, _xdr_yprequest, &req) ) {
		svcerr_decode(transp);
		return;
	}

	if (!svc_sendreply(transp, xdr_void, 0) ) {
		RESPOND_ERR(fun);
	}

	if (req.yp_reqtype == YPGET_REQTYPE) {

		if (ypset_current_map(req.ypget_req_map, req.ypget_req_domain,
				      &resp.status)
		    && !yp_map_access(rqstp, transp, &resp.status)) {
			MAP_ERR(fun, req.ypget_req_map);
			pid = -2;	/* avoid FORK_ERR and ypxfr below */
		} else {
			fork_cnt++;
			pid = vfork();
		}

		if (pid == -1) {
			FORK_ERR(fun);
		} else if (pid == 0) {

			ypclr_current_map();

			if (execl(ypxfr_proc, "ypxfr", "-d",
			    req.ypget_req_domain, "-h", req.ypget_req_owner,
			    req.ypget_req_map, NULL) ) {
				EXEC_ERR(fun);
			}

			_exit(1);
		}
	}

	if (!svc_freeargs(transp, _xdr_yprequest, &req) ) {
		RESPOND_ERR(fun);
	}
}

/*
 * This clears the current map, vforks, and execs the ypxfr process to get
 * the map referred to in the request.
 */
void
yppull(rqstp, transp)
	struct svc_req *rqstp;
	SVCXPRT *transp;
{
	struct yprequest req;
	struct ypresp_val resp;  /* not returned to the caller */
	int pid = -1;
	static char fun[] = "yppull";

	req.yppull_req_domain = req.yppull_req_map = NULL;

	if (!svc_getargs(transp, _xdr_yprequest, &req) ) {
		svcerr_decode(transp);
		return;
	}

	if (!svc_sendreply(transp, xdr_void, 0) ) {
		RESPOND_ERR(fun);
	}

	if (req.yp_reqtype == YPPULL_REQTYPE) {

		if (ypset_current_map(req.yppull_req_map,req.yppull_req_domain,
				      &resp.status)
		    && !yp_map_access(rqstp, transp, &resp.status)) {
			MAP_ERR(fun, req.yppull_req_map);
			pid = -2;	/* avoid FORK_ERR and ypxfr below */
		} else {
			fork_cnt++;
			pid = vfork();
		}

		if (pid == -1) {
			FORK_ERR(fun);
		} else if (pid == 0) {

			ypclr_current_map();

			if (execl(ypxfr_proc, "ypxfr", "-d",
			    req.yppull_req_domain, req.yppull_req_map, NULL) ) {
				EXEC_ERR(fun);
			}

			_exit(1);
		}
	}

	if (!svc_freeargs(transp, _xdr_yprequest, &req) ) {
		FREE_ERR(fun);
	}
}

/*
 * Ancillary functions used by the top-level functions within this module
 */

/*
 * This returns TRUE if a given key is a NIS-private symbol, otherwise FALSE
 */
static bool
isypsym(datum *key)
{

	if ((key->dptr == NULL) || (key->dsize < yp_prefix_sz) ||
	    bcmp(key->dptr, yp_prefix, yp_prefix_sz) ) {
		return (FALSE);
	}
	return (TRUE);
}

/*
 * This provides private-symbol filtration for the enumeration functions.
 */
static void
ypfilter(datum *inkey, datum *outkey, datum *val, long unsigned *status)
{
	datum k;

	if (inkey) {

		if (isypsym(inkey) ) {
			*status = YP_BADARGS;
			return;
		}

		k = nextkey(*inkey);
	} else {
		k = firstkey();
	}

	while (k.dptr && isypsym(&k)) {
		k = nextkey(k);
	}

	if (k.dptr == NULL) {
		*status = YP_NOMORE;
		return;
	}

	*outkey = k;
	*val = fetch(k);

	if (val->dptr != NULL) {
		*status = YP_TRUE;
	} else {
		*status = YP_BADDB;
	}
}

/*
 * Serializes a stream of struct ypresp_key_val's.  This is used
 * only by the ypserv side of the transaction.
 */
static bool_t
xdrypserv_ypall(XDR *xdrs, struct ypreq_nokey *req)
{
	bool_t more = TRUE;
	struct ypresp_key_val resp;

	resp.keydat.dptr = resp.valdat.dptr = (char *) NULL;
	resp.keydat.dsize = resp.valdat.dsize = 0;

	if (ypset_current_map(req->map, req->domain, &resp.status)) {
		ypfilter(NULL, &resp.keydat, &resp.valdat, &resp.status);

		while (resp.status == YP_TRUE) {
			if (!xdr_bool(xdrs, &more) ) {
				return (FALSE);
			}

			if (!xdr_ypresp_key_val(xdrs, &resp) ) {
				return (FALSE);
			}

			ypfilter(&resp.keydat, &resp.keydat, &resp.valdat,
			    &resp.status);
		}

	}

	if (!xdr_bool(xdrs, &more) ) {
		return (FALSE);
	}

	if (!xdr_ypresp_key_val(xdrs, &resp) ) {
		return (FALSE);
	}

	more = FALSE;

	if (!xdr_bool(xdrs, &more) ) {
		return (FALSE);
	}

	return (TRUE);
}
