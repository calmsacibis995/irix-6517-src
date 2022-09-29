#include "assert.h"
#include "errno.h"
#include "tkm.h"
#include "unistd.h"
#include "memory.h"
#include "stdlib.h"
#include "stdio.h"
#include "strings.h"
#include "getopt.h"
#include "ulocks.h"
#include "mutex.h"
#include "time.h"
#include "signal.h"
#include "sys/types.h"
#include "sys/prctl.h"
#include "mbox.h"
#include "syncv.h"

#define STKSIZE	(1024*1024*1)
static void sig(int);
static void node(int, int);

#define ABG_LOOKUP	1	/* c->s RPC */
#define ABG_GETNEW	2	/* c->s RPC */
#define ABG_REVOKE	3	/* server -> client 1-way */
#define ABG_GETMORE	5	/* c->s RPC */

union abg_req {
	struct getnew_req {
		tk_set_t obtain;
		tk_set_t toreturn;
		tk_disp_t why;
		unsigned long inuse;
	} getnew_req;
	struct revoke_req {
		tk_set_t torevoke;
		tk_disp_t which;
	} revoke_req;
	struct getmore_req {
		tk_set_t toreturn;
		tk_set_t eh;
		unsigned long giveback;
		unsigned long wanted;
		tk_disp_t which;
		unsigned long inuse;
	} getmore_req;
};
union abg_res {
	struct lookup_res {
		tk_set_t existance;
	} lookup_res;
	struct getnew_res {
		tk_set_t granted;
		tk_set_t already;
		unsigned long inuse;
	} getnew_res;
	struct getmore_res {
		int errno;
		unsigned long bg;
	} getmore_res;
};

/*
 * Service info
 */
#define MAXSERVICES	1
struct service_info {
	int server_node;	/* node that will act as server */
	int service_id;
	int cmpl;		/* client multi-programming level */
	void (*wait)(mesg_t *, int);
	void (*start)(void *, size_t);
	void *sdata;		/* private server data */
	void **cdata;		/* private client data / node */
} services[MAXSERVICES];

#define S1_CP(n) (services[SERVICE1_ID].cdata[n])/* get private data for node */
#define S1_SP() (services[SERVICE1_ID].sdata)/* get server data */
#define SERVICE1_ID	0
/* are we the service1 server? */
#define S1S()		(services[SERVICE1_ID].server_node)
#define AM_S1S(n)	(n == services[SERVICE1_ID].server_node)

/*
 * tokens
 * To update a counter one must have both the COUNTERS_UPDATE and
 * BG_UPDATE tokens.
 * If someone needs to read the 'current' amount - they must grab
 * the COUNTERS_READ token - this will cause all clients to send
 * back what they have used.
 * To get another/additional grant,
 * all the BG_UPDATE tokens need to be revoked - 
 * We start with a central server - all grant requests must go to the
 * server.
 */
#define EXIST_CLASS		0
#define EXIST_READ		TK_MAKE(EXIST_CLASS, TK_READ)


#define COUNTERS_CLASS		1
#define COUNTERS_READ		TK_MAKE(COUNTERS_CLASS, TK_WRITE)
#define SCOUNTERS_READ		TK_MAKE_SINGLETON(COUNTERS_CLASS, TK_WRITE)
#define COUNTERS_UPDATE		TK_MAKE(COUNTERS_CLASS, TK_SWRITE)

#define BG_CLASS		2
#define BG_READ			TK_MAKE(BG_CLASS, TK_READ)
#define BG_UPDATE		TK_MAKE(BG_CLASS, TK_SWRITE)

#define NTOKENS	3
/*
 * server side
 * XXX to 'recover' must track # given out per client 
 */
struct server {
	TKS_DECL(ss, NTOKENS);
	int initted;
	usema_t *sync;		/* sync up threads */
	/* data for server side of ABG algorithm */
	usema_t *recallsync;	/* wait for recall */
	int nrecallsync;	/* # on recallsync */
	usema_t *recallwait;	/* wait for recaller */
	int inrecall;		/* someone is performing a recall */
	usema_t *upd;		/* update values */
	unsigned long max;	/* overall max */
	unsigned long remain;	/* # remaining */
	unsigned long total;	/* sum of all units 'allocated' - only really
				 * valid if have COUNTERS_READ for read
				 */
	ulock_t lckvalue;
};

/*
 * client side
 *
 * State flags:
 * gettingmore - communicate between getmore() and (*tkc_return)()
 * recall - communicate between tkc_recall and 'busy' holders
 */
struct client {
	TKC_DECL(cs, NTOKENS);
	tks_ch_t h;		/* handle (contains node we're on) */
	int node;		/* node we're on (for debug) */
	int initted;
	int ndone;		/* # threads done */
	usema_t *sync;		/* sync up threads */
	/* stuff for the ABG algorithm itself */
	ulock_t upd;
	unsigned long nl;	/* current # of 'nl''s in use */
	long remain;		/* granted block - remaining */
	char gmdone;
	char gettingmore;
	unsigned long wanted;	/* how many units wanted */
	sv_t svsync;
	int getmoreerr;		/* status from (*tkc_return)->getmore() */
	unsigned long inuse;	/* total inuse */
};
/* values for gmdone */
#define GM_INIT	1
#define GM_INPROGRESS 2
#define GM_DONE 3

/*
 * htonode - convert a handle to a node number
 */
#define htonode(h)	(h)
#define nodetoh(n)	(n)

int nloops = 10;
pid_t ppid;
unsigned long ndoneclients;
unsigned long amlogging;
int verbose;
/*
 * the following arena is used for all locks and semaphores for all nodes
 * and all services ...
 */
usptr_t *usptr;
char *Cmd;
int nodes;
int resources = 400;	/* number of resources that can be 'used' */
int maxresources = 200;	/* max per client iteration */
int dumplog = 0;
int noreads;		/* don't do any counter reads */

static void service1_wait(mesg_t *, int);
static void service1_start(void *, size_t);
static void service1_test(int node);
static void play(struct client *cp);

int
main(int argc, char **argv)
{
	int c, i;
	int cmpl;
	unsigned long totclients;
	int nwaiters;
	int usedebuglocks = 0;

	setlinebuf(stdout);
	setlinebuf(stderr);
	Cmd = strdup(argv[0]);
	ppid = getpid();

	prctl(PR_COREPID, 0, 1);

	cmpl = 3;
	nodes = 4;

	while ((c = getopt(argc, argv, "dr:TN:vLm:n:E")) != EOF) 
		switch (c) {
		case 'E':
			noreads = 1;
			break;
		case 'd':
			usedebuglocks = 1;
			break;
		case 'r':
			resources = atoi(optarg);
			break;
		case 'T':
			__tk_tracenow = 1;
			break;
		case 'v':
			verbose++;
			break;
		case 'L':
			dumplog++;
			break;
		case 'm': /* node multi-programming level */
			cmpl = atoi(optarg);
			break;
		case 'N':
			nodes = atoi(optarg);
			break;
		case 'n':
			nloops = atoi(optarg);
			break;
		default:
			fprintf(stderr, "abg3:illegal option %c\n", c);
			fprintf(stderr, "Usage:abg3 [options]\n");
			fprintf(stderr, "\t-n #\t# loops\n");
			fprintf(stderr, "\t-N #\t# nodes\n");
			fprintf(stderr, "\t-m #\tclient multi-programming level\n");
			fprintf(stderr, "\t-v\tverbose\n");
			fprintf(stderr, "\t-r #\t# of resources\n");
			exit(1);
		}

	printf("abg3: nodes %d loops %d client-multiprogramming %d resources %d\n",
		nodes, nloops, cmpl, resources);
	/*
	 * setup services
	 */
	for (i = 0; i < MAXSERVICES; i++)
		services[i].service_id = -1;
	services[0].server_node = 0;
	services[0].service_id = SERVICE1_ID;
	services[0].cmpl = cmpl;
	services[0].wait = service1_wait;
	services[0].start = service1_start;
	services[0].cdata = (void **)malloc(nodes * sizeof(void *));

	/*
	 * XXX we really need more why??
	 * 1) each service has 'cmpl' client threads per node
	 * 2) Each service server needs a thread per potential client thread 
	 *	that is not on its node
	 * 3) each client not on the server needs a 'wait' thread.
	 * XX each client thread could be calling the server but
	 * the server could also send out some RPCs that must be answered.
	 */
	totclients = nodes * cmpl;
	nwaiters = (nodes+1) * cmpl;
	nwaiters *= 2;
	nwaiters += (nodes - 1);
	usconfig(CONF_INITUSERS, nwaiters + totclients + 1);

	sigset(SIGTERM, sig);
	sigset(SIGABRT, sig);
	sigset(SIGUSR1, sig);
	sigset(SIGUSR2, sig);
	sigset(SIGURG, sig);

	prctl(PR_SETEXITSIG, SIGTERM);
	/* initialize RPC */
	initmbox("/usr/tmp/abg3mbox");

	/* call tkc_init to get arenas started up in correct sequence */
	tkc_init();
	tks_init();

	/*
	 * alloc an area for locks - note that all nodes 'share' this
	 * but that should be OK
	 */
	if (usedebuglocks)
		usconfig(CONF_LOCKTYPE, US_DEBUGPLUS);
	usconfig(CONF_ARENATYPE, US_SHAREDONLY);
	if ((usptr = usinit("/usr/tmp/abg3")) == NULL) 
		abort();

	/* create 'nodes' */
	for (i = 0; i < nodes; i++)
		node(i, nodes);

	while (ndoneclients != totclients)
		sginap(100);

	if (dumplog)
		tk_printlog(stdout, 1000, dumplog > 2 ? TK_LOG_ALL :
			(dumplog > 1 ? TK_LOG_TS : 0), NULL);

	return 0;
}

static void
sig(int s)
{
	static FILE *f = NULL;
	if (s != SIGTERM && f == NULL)
		f = fopen("abg3.LOG", "w");

	if (s == SIGABRT) {
		/* make sure only one process logs */
		sighold(SIGTERM);
		if (f == NULL)
			f = stderr;
		fprintf(f, "\n\n======ABORT=============\n\n");
		if (test_and_set(&amlogging, 1) == 0)
			tk_printlog(f, -1, TK_LOG_ALL, NULL);
		fflush(f);
		sigset(SIGABRT, SIG_DFL);
		abort();
	} else if (s == SIGUSR1 || s == SIGUSR2) {
		if (f == NULL)
			f = stdout;
		if (amlogging)
			return;
		fprintf(f, "\n\n===SIGUSR===============\n\n");
		tk_printlog(f, -1, s == SIGUSR2 ? TK_LOG_ALL : 0, NULL);
		fflush(f);
		return;
	} else if (s == SIGURG) {
		char criteria[128];

		fprintf(stdout, "Criteria?:");
		fflush(stdout);

		if (fgets(criteria, sizeof(criteria), stdin) == NULL)
			criteria[0] = '\0';

		if (f == NULL)
			f = stdout;
		fprintf(f, "\n\n===SIGURG===============\n\n");
		tk_printlog(f, -1, TK_LOG_ALL, criteria);
		fflush(f);
		return;
	}
	sighold(SIGTERM);
	exit(0);
}

/*
 * Node
 */
/*
 * do all waits for ootb messages - both client and server
 */
/* ARGSUSED */
static void
node_wait(void *a, size_t sz)
{
	int node = (int)(ptrdiff_t)a;
	struct mbox *mbox;
	auto mesg_t *m;

	mbox = ntombox(node);
	for (;;) {
		readmbox(mbox, &m);
		(services[m->service_id].wait)(m, node);
		if (m->flags & MESG_RPC)
			replymbox(mbox, m);
		else
			freemesg(m);
	}
}

static void
node(int mynode, int nodes)
{
	int i, j;
	struct mbox *mb;
	pid_t spid;

	mb = allocmbox();
	setmbox(mynode, mb);

	for (i = 0; i < MAXSERVICES; i++) {
		if (services[i].service_id < 0)
			continue;

		/* start up wait threads -
		 * if we are the server then we need 'cmpl' server threads
		 * 	per client node AND client wait threads.
		 * if we are a client we need 'cmpl' client wait threads
		 *	but no server wait threads ..
		 */
		if (services[i].server_node == mynode) {
			int ns;
			ns = (nodes+1) * services[SERVICE1_ID].cmpl;
			for (j = 0; j < ns; j++) {
				if ((spid = sprocsp(node_wait, PR_SALL,
					    (void *)(ptrdiff_t)mynode,
					    NULL, STKSIZE)) < 0) {
					perror("sproc");
					exit(1);
				}
				if (verbose)
					printf("%s:started up server wait thread on node %d pid %d\n",
						Cmd, mynode, spid);
			}
		} else {
			int ns;
			ns = services[SERVICE1_ID].cmpl;
			for (j = 0; j < ns; j++) {
				if ((spid = sprocsp(node_wait, PR_SALL,
					   (void *)(ptrdiff_t)mynode,
					   NULL, STKSIZE)) < 0) {
					perror("sproc");
					exit(1);
				}
				if (verbose)
					printf("%s:started up client wait thread on node %d pid %d\n",
						Cmd, mynode, spid);
			}
		}

		if ((spid = sprocsp(services[i].start, PR_SALL,
					(void *)(ptrdiff_t)mynode,
					NULL, STKSIZE)) < 0) {
			perror("sproc");
			exit(1);
		}
		if (verbose)
			printf("%s:started up service %d on node %d pid %d\n",
					Cmd, i, mynode, spid);
	}
}

/*
 * token callout modules
 */
static void client_obtain(void *, tk_set_t, tk_set_t, tk_disp_t, tk_set_t *);
static void client_return(tkc_state_t, void *, tk_set_t, tk_set_t, tk_disp_t);
static void server_recall(void *o, tks_ch_t h, tk_set_t r, tk_disp_t);
static void server_recalled(void *, tk_set_t, tk_set_t);

tks_ifstate_t svriface = {
	server_recall,
	server_recalled,
	NULL
};
tkc_ifstate_t iface = {
	client_obtain,
	client_return
};

/*
 * stubs and server implementation modules
 */
static int invoke_lookup(tks_ch_t, tk_set_t *);
static int invoke_getnew(tks_ch_t, tk_set_t, tk_set_t, tk_disp_t, tk_set_t *, tk_set_t *, unsigned long *);
static void server_lookup(tks_ch_t h, tk_set_t *granted);
static int server_getnew(tks_ch_t, tk_set_t, tk_set_t, tk_disp_t, tk_set_t *, tk_set_t *, unsigned long *);
static int sgetmore(tks_ch_t, unsigned long *);
static int server_return(tks_ch_t, unsigned long, unsigned long *, unsigned long, tk_set_t, tk_set_t, tk_disp_t);
/*
 * valued for getlock
 */
#define CNTREAD 1
#define CNTUPDATE 2
static void getlock(struct client *cp, int);
static void putlock(struct client *cp, int);
static int getmore(struct client *cp, int wanted);

/* ARGSUSED */
static void
service1_launch(void *a, size_t sz)
{
	service1_test((int)(ptrdiff_t)a);
	/* NOTREACHED */
}

/*
 * called once per node - sets up minimal state needed
 */
/* ARGSUSED */
void
service1_start(void *a, size_t sz)
{
	int mynode = (int)(ptrdiff_t)a;
	int i;
	pid_t spid;
	struct server sd;
	struct client cd;	/* this is what we're manipulating */

	/* init a client data object */
	cd.h = nodetoh(mynode);	/* handle is just node number */
	cd.node = mynode;
	cd.sync = usnewsema(usptr, 1);
	cd.upd = usnewlock(usptr);
	cd.initted = 0;
	cd.gettingmore = 0;
	cd.gmdone = GM_DONE;
	sv_create(&cd.svsync);
	/* place local data in 'fake' private memory */
	S1_CP(mynode) = &cd;

	if (AM_S1S(mynode)) {
		/* init server side */
		sd.sync = usnewsema(usptr, 0);
		sd.recallsync = usnewsema(usptr, 0);
		sd.nrecallsync = 0;
		sd.inrecall = 0;
		sd.total = 0;
		sd.recallwait = usnewsema(usptr, 0);
		sd.upd = usnewsema(usptr, 1);
		sd.lckvalue = usnewlock(usptr);
		sd.initted = 0;
		/* place local data in 'fake' private memory */
		S1_SP() = &sd;
	}

	/*
	 * each client / server is multi-threaded
	 */
	for (i = 1; i < services[SERVICE1_ID].cmpl; i++) {
		if ((spid = sprocsp(service1_launch, PR_SALL,
				(void *)(ptrdiff_t)mynode,
				NULL, STKSIZE)) < 0) {
			perror("sproc");
			exit(1);
		}
		if (verbose)
			printf("%s:started up client on node %d pid %d\n",
				Cmd, mynode, spid);
	}
	service1_test(mynode);
	/* NOTREACHED */
}

/*
 * called by all threads - both client & server - so far nothing
 * much has been set up!
 */
static void
service1_test(int node)
{
	struct client *cp;
	tk_set_t granted;

	cp = S1_CP(node);
	uspsema(cp->sync);
	while (!cp->initted) {
		/*
		 * lookup object
		 */
		if (AM_S1S(node)) {
			server_lookup(cp->h, &granted);
		} else {
			invoke_lookup(cp->h, &granted);
		}
		if (granted == TK_NULLSET) {
			/* noone home ... wait a bit */
			sginap(4);
			continue;
		}

		/*
		 * we now have existance token
		 */
		tkc_create("client", cp->cs, (void *)cp, &iface, NTOKENS,
					granted, NULL);
		cp->initted = 1;
		cp->nl = 0;
		cp->ndone = 0;
		cp->remain = 0;
	}
	usvsema(cp->sync);
	play(cp);

	uspsema(cp->sync);
	cp->ndone++;
	if (cp->ndone >= services[SERVICE1_ID].cmpl) {
		tkc_print(cp->cs, stdout, "Node %d ", node);
		if (AM_S1S(node))
			tks_print(((struct server *)S1_SP())->ss, stdout,
					"Server Node %d ", node);
	}
	usvsema(cp->sync);

	test_then_add(&ndoneclients, 1);
	for (;;)
		pause();
}

/*
 * ABG algorithm - 
 *
 * The BG_UPDATE token is the right to have tokens in a local pool.
 * The COUNTERS_UPDATE token is one's right to add or delete resources from
 *	the pool.
 */

static void
play(struct client *cp)
{
	int err, j, wanted;
	struct timespec ts;
	int nenospc = 0, ngotimmed = 0, ngetmore = 0, neededmore;

	for (j = 0; j < nloops; j++) {
		neededmore = 0;
		/* compute resources to get */
		do {
			wanted = (rand() % (2*maxresources)) - maxresources;
		} while (wanted <= 0);

		if (verbose > 1)
			printf("client %d node %d wants %d resources\n",
					get_pid(), cp->node, wanted);
		getlock(cp, CNTUPDATE);
		while (cp->remain < wanted) {
			neededmore = 1;	/* metering */
			err = getmore(cp, wanted);
			if (err) {
				/* ran out */
				if (verbose > 1)
					printf("client %d node %d - no more resources\n",
						getpid(), cp->node);
				nenospc++;
				ts.tv_sec = 0;
				ts.tv_nsec = (rand() % 4) * 1000 * 1000;
				nanosleep(&ts, NULL);
				getlock(cp, CNTUPDATE);
			}
		}

		cp->remain -= wanted;
		cp->nl += wanted;
		putlock(cp, CNTUPDATE);

		if (!neededmore)
			ngotimmed++;

		/* wait a bit */
		ts.tv_sec = 0;
		ts.tv_nsec = (rand() % 4) * 1000 * 1000;
		nanosleep(&ts, NULL);

		if (!noreads) {
			/*
			 * every once in a while - grab total in-use count
			 */
			if ((j % 10) == 0) {
				getlock(cp, CNTREAD);
				printf("client %d node %d total %d\n",
					get_pid(), cp->node, cp->inuse);
				putlock(cp, CNTREAD);
			}
		}

		/* give it back */
		getlock(cp, CNTUPDATE);

		cp->remain += wanted;
		cp->nl -= wanted;
		if (verbose > 1)
			printf("client %d node %d finished w/ %d resources remain %d inuse %d\n",
				get_pid(), cp->node, wanted, cp->remain,
				cp->nl);
		putlock(cp, CNTUPDATE);
	}
	printf("client %d node %d - ENOSPC %d, getmore %d, immediate %d times\n",
		getpid(), cp->node, nenospc, ngetmore, ngotimmed);
}

/*
 * Can get the lock for either read or update - for read we need the
 * COUNTERS_READ token, for update we need BG_UPDATE & COUNTERS_UPDATE.
 */
static void
getlock(struct client *cp, int type)
{
	tk_set_t gotten;
	if (type == CNTREAD)
		tkc_acquire1(cp->cs, SCOUNTERS_READ);
	else
		tkc_acquire(cp->cs, TK_ADD_SET(COUNTERS_UPDATE, BG_UPDATE), &gotten);
	ussetlock(cp->upd);
}

static void
putlock(struct client *cp, int type)
{
	usunsetlock(cp->upd);
	if (type == CNTREAD)
		tkc_release1(cp->cs, SCOUNTERS_READ);
	else
		tkc_release(cp->cs, TK_ADD_SET(COUNTERS_UPDATE, BG_UPDATE));
}

/*
 * getmore - the guts of the client side
 * We have the 'lock' from getlock. Thus this routine is inherently
 * single threaded.
 *
 * Returns: 0 if can get the requested resource. returns with the lock/token
 * 	set
 *	    >0 if some error; no lock/token.
 */
static int
getmore(struct client *cp, int wanted)
{
	int rv = 0;

	while (cp->gettingmore) {
		/*
		 * someone already getting more - wait
		 * can't hold lock across release since revoke might be called
		 * We don't want to hold either token since that might
		 * deadlock
		 */
		putlock(cp, CNTUPDATE);
		getlock(cp, CNTUPDATE);
		if (cp->remain >= wanted)
			return 0;
	}

	cp->gettingmore = 1;
	cp->wanted = wanted;/* communicate with (*tkc_return)() */

	/*
	 * revoke token - since we have it held, no callouts
	 * will occur
	 */
	tkc_recall(cp->cs, BG_UPDATE, TK_ALL_CLIENT);
	assert(cp->gmdone == GM_DONE);
	cp->gmdone = GM_INIT;
	/* release BOTH tokens - otherwise we violate acquire ordering */
	putlock(cp, CNTUPDATE);
	/* releases lock */

	ussetlock(cp->upd);
	while (cp->gmdone != GM_DONE) {
		/* XXX ok to use same sync sema?? */
		sv_wait(&cp->svsync, cp->upd);
		ussetlock(cp->upd);
	}
	assert(cp->gettingmore);

	/*
	 * We could get an error either cause the server wouldn't give
	 * us anymore or because we got an OOB error.
	 */
	if (cp->getmoreerr) {
		/* error */
		rv = cp->getmoreerr;
		cp->gettingmore = 0;
		sv_broadcast(&cp->svsync);
		usunsetlock(cp->upd);
	} else {
		/*
		 * the only way the hold could fail is if a revoke
		 * came in between the return and hold. We can't put a lock
		 * around the revoke since it might go ahead and send
		 * messages.
		 */
		cp->gettingmore = 0;
		sv_broadcast(&cp->svsync);
		usunsetlock(cp->upd);
		getlock(cp, CNTUPDATE);
	}

	return rv;
}

/*===================================================================*/

/*
 * client message ops - these correspond 1-1 with server routines
 * that are called from _wait
 */
static int
invoke_lookup(tks_ch_t h, tk_set_t *granted)
{
	mesg_t *m;
	union abg_res *res;
	int error;

	m = getmesg();
	m->op = ABG_LOOKUP;
	m->service_id = SERVICE1_ID;
	m->handle = h;
	error = callmbox(ntombox(S1S()), m);
	if (error == 0) {
		res = (union abg_res *)&m->response;
		*granted = res->lookup_res.existance;
	}
	freemesg(m);
	return error;
}

static int
invoke_getnew(tks_ch_t h,
	tk_set_t obtain,
	tk_set_t toreturn,
	tk_disp_t why,
	tk_set_t *granted,
	tk_set_t *already,
	unsigned long *inuse)
{
	mesg_t *m;
	union abg_req *req;
	union abg_res *res;
	int error;

	m = getmesg();
	m->op = ABG_GETNEW;
	m->service_id = SERVICE1_ID;
	m->handle = h;
	req = (union abg_req *)&m->request;
	req->getnew_req.obtain = obtain;
	req->getnew_req.toreturn = toreturn;
	req->getnew_req.why = why;
	req->getnew_req.inuse = *inuse;

	error = callmbox(ntombox(S1S()), m);
	if (error == 0) {
		res = (union abg_res *)&m->response;
		*granted = res->getnew_res.granted;
		*already = res->getnew_res.already;
		*inuse = res->getnew_res.inuse;
	}
	freemesg(m);
	return error;
}

/*=================================================================*/
/*
 * server routines - invoked from _wait and from local clients
 */
static void
server_lookup(tks_ch_t h, tk_set_t *granted)
{
	auto tk_set_t already;
	struct server *sp;

	sp = (struct server *)S1_SP();
	if (sp == NULL) {
		/* race .. return equiv of ENOENT */
		*granted = TK_NULLSET;
		return;
	}

	uspsema(sp->upd);
	if (!sp->initted) {
		tks_create("server", sp->ss, (void *)sp, &svriface, NTOKENS, 0);
		sp->initted = 1;
		sp->max = resources;
		sp->remain = resources;
	}
	usvsema(sp->upd);

	/* all set - respond with existance token */
	tks_obtain(sp->ss, h, EXIST_READ, granted, NULL, &already);
	assert(already == TK_NULLSET);
	assert(*granted == EXIST_READ);
}

/*
 * respond to GETNEW request from client
 */
static int
server_getnew(tks_ch_t h,
	tk_set_t obtain,
	tk_set_t toreturn,
	tk_disp_t why,
	tk_set_t *granted,
	tk_set_t *already,
	unsigned long *inuse)
{
	struct server *sp = S1_SP();
	tk_set_t got = TK_NULLSET, got2 = TK_NULLSET;

	if (toreturn != TK_NULLSET) {
		if (TK_IS_IN_SET(toreturn, COUNTERS_UPDATE)) {
			ussetlock(sp->lckvalue);
			sp->total += *inuse;
			assert(sp->total <= resources);
			usunsetlock(sp->lckvalue);
		}
		tks_return(sp->ss, h, toreturn, TK_NULLSET, TK_NULLSET,
			why);
	}
	if (TK_SUB_SET(obtain, BG_UPDATE) != TK_NULLSET) {
		tks_obtain(sp->ss, h, TK_SUB_SET(obtain, BG_UPDATE), &got, NULL,
				already);
	}
	if (TK_IS_IN_SET(obtain, BG_UPDATE)) {
		/*
		 * we can't do an tks_obtain while in recall state
		 */
		uspsema(sp->upd);
		while (sp->inrecall) {
			sp->nrecallsync++;
			usvsema(sp->upd);
			uspsema(sp->recallsync);
			uspsema(sp->upd);
			continue;
		}
		tks_obtain(sp->ss, h, BG_UPDATE, &got2, NULL, already);
		assert(*already == TK_NULLSET);
		usvsema(sp->upd);
	}
	TK_COMBINE_SET(got, got2);
	*granted = got;

	if (TK_IS_IN_SET(got, COUNTERS_READ))
		*inuse = sp->total;
	/*
	 * we don't want to remember each nodes count - just the total
	 * so we need to subtract out their amount when we give them
	 * an update token. They send us the number
	 */
	if (TK_IS_IN_SET(got, COUNTERS_UPDATE)) {
		ussetlock(sp->lckvalue);
		assert(sp->total >= *inuse);
		sp->total -= *inuse;
		usunsetlock(sp->lckvalue);
	}

	return 0;
}

/*
 * Server side of getmore - the main algorithm
 */
static int
sgetmore(tks_ch_t h, unsigned long *bg)
{
	struct server *sp = S1_SP();
	auto tk_set_t granted, already;
	unsigned long given, wanted;
	int rv = 0;

	uspsema(sp->upd);

	assert(sp->remain <= resources);
	wanted = *bg;
	/* do a sanity check */
	/*
	 * we can't do an tks_obtain while in recall state - and it seems
	 * fairer to not let one client have some when another requested
	 * more first
	 */
	while ((sp->remain < wanted) || sp->inrecall) {
		/*
		 * no hope! - time to bring back all unused stuff and see
		 * if some client has some we could use 
		 */
		if (sp->inrecall) {
			sp->nrecallsync++;
			usvsema(sp->upd);
			uspsema(sp->recallsync);
			uspsema(sp->upd);
			continue;
		}
		sp->inrecall = 1;
		usvsema(sp->upd);

		tks_recall(sp->ss, BG_UPDATE);
		uspsema(sp->recallwait); /* wait for recall to complete */
		assert(sp->inrecall);

		uspsema(sp->upd);
		if (verbose > 1)
			printf("server after recall remain %d want %d\n",
				sp->remain, wanted);
		sp->inrecall = 0;

		while (sp->nrecallsync > 0) {
			usvsema(sp->recallsync);
			sp->nrecallsync--;
		}

		if (sp->remain < wanted) {
			usvsema(sp->upd);
			return ENOSPC;
		}

	}

	/* give them the max of 1/2 of whats left and what they want */
	given = sp->remain / 2;
	if (given < wanted)
		given = wanted;
	if (given == 0)
		given = 1;
	assert(given <= sp->remain);
	sp->remain -= given;
	*bg = given;
	if (verbose > 1)
		printf("server %d giving %d more resources to node %d\n",
			get_pid(), given, htonode(h));

	/* get the token again */
	tks_obtain(sp->ss, h, BG_UPDATE, &granted, NULL, &already);
	if (already == BG_UPDATE)
		rv = EALREADY;

	/* must hold the sema across the tkc_obtain so that a recall doesn't
	 * get started
	 */
	usvsema(sp->upd);
	return rv;
}

/*
 * server function to handle a return from client - either via a recall
 * or a revoke
 */
static int
server_return(tks_ch_t h,
	unsigned long giveback,
	unsigned long *bg,
	unsigned long inuse,
	tk_set_t ok,
	tk_set_t eh,
	tk_disp_t which)
{
	struct server *sp = S1_SP();

	if (TK_IS_IN_SET(ok, BG_UPDATE)) {
		uspsema(sp->upd);
		sp->remain += giveback;
		assert(sp->remain <= resources);
		usvsema(sp->upd);
	}
	if (TK_IS_IN_SET(ok, COUNTERS_UPDATE)) {
		ussetlock(sp->lckvalue);
		sp->total += inuse;
		assert(sp->total <= resources);
		usunsetlock(sp->lckvalue);
	}
	/* this could trigger the tkc_recalled callback */
	tks_return(sp->ss, h, ok, TK_NULLSET, eh, which);

	if (bg && *bg > 0) {
		/* they want more! */
		return sgetmore(h, bg);
	}
	return 0;
}

/*
 * callout function - called when a token has been recalled and can now be
 * reclaimed/destroyed
 */
/* ARGSUSED */
static void
server_recalled(void *o, tk_set_t r, tk_set_t refused)
{
	struct server *sp = (struct server *)o;

	assert(refused == TK_NULLSET);
	assert(sp->inrecall);
	if (verbose > 1)
		printf("server recalled max %d remain %d\n",
			sp->max, sp->remain);
	usvsema(sp->recallwait);
}

/*
 * token callout for obtaining tokens
 */
/* ARGSUSED */
static void
client_obtain(void *o,
	tk_set_t obtain,
	tk_set_t toreturn,
	tk_disp_t why,
	tk_set_t *refused)
{
	struct client *cp = (struct client *)o;
	tk_set_t granted, already;
	auto unsigned long inuse;
	int error;

	inuse = cp->nl;
	if (AM_S1S(cp->node)) {
		error = server_getnew(cp->h, obtain, toreturn, why,
					&granted, &already, &inuse);
	} else {
		error = invoke_getnew(cp->h, obtain, toreturn, why,
					&granted, &already, &inuse);
	}
	assert(error == 0);

	*refused = TK_SUB_SET(obtain, TK_ADD_SET(granted, already));
	assert(*refused == TK_NULLSET);
	assert(already == TK_NULLSET);
	if (TK_IS_IN_SET(granted, COUNTERS_READ))
		cp->inuse = inuse;
}

/*
 * client_return - callout from token client module
 *
 * BG_UPDATE can get be a part of the revoke set when 'which' is either
 *	CLIENT_INITIATED or SERVER_RECALL - since the token module
 *	'upgrades' CLIENT_INITIATED to RECALL messages to save messages.
 *	It can also be SERVER_REVOKE.
 * This means that we have to make very sure whether getmore() will be
 * doing the tkc_return or not - otherwise nooone will and we'll hang.
 * Locking - not really a problem since we are in this revoke callout
 *	and thus noone can have the token so can't really look at
 *	cp->gettingmore, etc..
 *	Since gettingmore is single threaded, we don't need to protect it
 *	and we all understand that it can't change during this function.
 */
/* ARGSUSED */
static void
client_return(
	tkc_state_t ci,
	void *o,
	tk_set_t revoke,
	tk_set_t eh,
	tk_disp_t which)
{
	mesg_t *m;
	union abg_req *req;
	union abg_res *res;
	struct client *cp = (struct client *)o;
	tk_set_t toret, toref;
	unsigned long giveback, bg;
	int err, cgetmore = 0;
	int oobe = 0;

	bg = 0;
	giveback = 0;
	if (TK_IS_IN_SET(revoke, BG_UPDATE)) {
		if (cp->gmdone == GM_INIT) {
			bg = cp->wanted;
			cgetmore = 1;
			cp->gmdone = GM_INPROGRESS;
			assert(bg > 0);
		} else {
			assert(TK_GET_CLASS(which, BG_CLASS) != TK_CLIENT_INITIATED);
		}

		ussetlock(cp->upd);
		giveback = cp->remain;
		cp->remain = 0;
		usunsetlock(cp->upd);
	}

	if (AM_S1S(cp->node)) {
		/* we are server */
		err = server_return(cp->h, giveback, &bg, cp->nl, revoke,
				eh, which);
	} else {
		/* send return message and data */
		m = getmesg();
		m->op = ABG_GETMORE;
		m->service_id = SERVICE1_ID;
		m->handle = cp->h;
		req = (union abg_req *)&m->request;
		req->getmore_req.toreturn = revoke;
		req->getmore_req.eh = eh;
		req->getmore_req.giveback = giveback;
		req->getmore_req.which = which;
		req->getmore_req.wanted = bg;
		req->getmore_req.inuse = cp->nl;

		/*
		 * if we are getting more - do an RPC
		 * This also handles the requirement that if its a
		 * CLIENT_INITIATED revoke we MUST do an RPC
		 */
		if (cgetmore) {
			assert(bg > 0);
			oobe = callmbox(ntombox(S1S()), m);

			if (oobe == 0) {
				res = (union abg_res *)&m->response;
				bg = res->getmore_res.bg;
				err = res->getmore_res.errno;
			}
			freemesg(m);
#if !_TK_RETURNIPC
		} else if (TK_IS_ANY_CLIENT(which)) {
			assert(bg == 0);
			oobe = callmbox(ntombox(S1S()), m);
			freemesg(m);
#endif /* _TK_RETURNIPC */
		} else {
			assert(bg == 0);
			oobe = writembox(ntombox(S1S()), m);
		}
		/*
		 * if get an OOB error ENOSEND, then really didn't return token
		 * so we really didn't give up the resources 
		 */
		if (oobe == ENOSEND && TK_IS_IN_SET(revoke, BG_UPDATE)) {
			ussetlock(cp->upd);
			cp->remain = (long)giveback;
			usunsetlock(cp->upd);
		}
		if (oobe)
			err = oobe;
	}
	toret = revoke;
	toref = TK_NULLSET;
	if (cgetmore) {
		if (err == 0) {
			/*
			 * If no error then we got the BG_UPDATE token back
			 * so we logically 'refuse' to return it.
			 */
			assert(oobe == 0);
			toret = TK_SUB_SET(toret, BG_UPDATE);
			toref = BG_UPDATE;
			cp->remain = (long)bg;
		}
		cp->getmoreerr = err;
		ussetlock(cp->upd);
		assert(cp->gmdone == GM_INPROGRESS);
		cp->gmdone = GM_DONE;
		sv_broadcast(&cp->svsync);
		usunsetlock(cp->upd);
	}
	/* inform client token module that all is done */
	tkc_returned(cp->cs, toret, toref);
}

/*
 * get client handle from within token module - used for TRACING only
 */
tks_ch_t
getch_t(void *o)
{
	struct client *cp = (struct client *)o;

	assert(cp);
	return(cp->h);
}


/*
 * callout function that is called from the token server module when a
 * token (set) needs to be revoked. It sends a message to the appropriate
 * client. This sends a 1 way message
 */
/* ARGSUSED */
static void
server_recall(void *o, tks_ch_t h, tk_set_t r, tk_disp_t which)
{
	mesg_t *m;
	union abg_req *req;

	if (AM_S1S(htonode(h))) {
		tkc_recall(((struct client *)S1_CP(htonode(h)))->cs, r, which);
		return;
	}

	m = getmesg();
	m->op = ABG_REVOKE;
	m->service_id = SERVICE1_ID;
	m->handle = h;
	req = (union abg_req *)&m->request;
	req->revoke_req.torevoke = r;
	req->revoke_req.which = which;

	writembox(ntombox(htonode(h)), m);
	return;
}

/*
 * this thread handles any out-of-the-blue messages
 * Both client and server.
 * 'node' is which node we are.
 */
static void
service1_wait(mesg_t *m, int node)
{
	struct client *cp;	/* client side data */
	union abg_req *req;
	union abg_res *res;

	req = (union abg_req *)&m->request;
	res = (union abg_res *)&m->response;

	switch (m->op) {
	/* client messages */
	case ABG_REVOKE:
		cp = (struct client *)S1_CP(node);
		assert(!AM_S1S(node));
		tkc_recall(cp->cs, req->revoke_req.torevoke,
			req->revoke_req.which);
		break;
	/*
	 * server messages
	 */
	case ABG_GETMORE:
		{
		unsigned long bg;
		assert(AM_S1S(node));

		bg = req->getmore_req.wanted;
		res->getmore_res.errno =
			server_return(m->handle,
				req->getmore_req.giveback,
				&bg,
				req->getmore_req.inuse,
				req->getmore_req.toreturn,
				req->getmore_req.eh,
				req->getmore_req.which);
#if !_TK_RETURNIPC
		if (req->getmore_req.wanted > 0 ||
			 TK_IS_ANY_CLIENT(req->getmore_req.which)) {
#else
		if (req->getmore_req.wanted > 0) {
#endif
			/* we use this to 'mark' whether we need
			 * a reply msg or not
			 */
			res->getmore_res.bg = bg;
		}
		break;
		}
	case ABG_GETNEW:
		{
		auto unsigned long inuse;
		assert(AM_S1S(node));
		inuse = req->getnew_req.inuse;
		server_getnew(m->handle,
				req->getnew_req.obtain,
				req->getnew_req.toreturn,
				req->getnew_req.why,
				&res->getnew_res.granted,
				&res->getnew_res.already,
				&inuse);
		res->getnew_res.inuse = inuse;
		break;
		}
	case ABG_LOOKUP:
		assert(AM_S1S(node));
		server_lookup(m->handle, &res->lookup_res.existance);
		break;
	default:
		fprintf(stderr, "service1-wait:Unknown msg op:%d\n", m->op);
		abort();
		break;
	}
	return;
}
