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
static int srpc(struct mbox *, tks_ch_t, void *, int, void *, int);

/*
 * protocol
 */
struct scheme {
	tks_ch_t handle;		/* client handle */
	struct mbox *return_addr;	/* return address for RPC */
};
#define ABG_LOOKUP	1	/* c->s RPC */
#define ABG_GETNEW	2	/* c->s RPC */
#define ABG_REVOKE	3	/* server -> client 1-way */
#define ABG_GETMORE	5	/* c->s RPC */

struct abg_req {
	struct scheme addr;		/* MUST be first!! */
	int op;
	int service_id;
	void *obj;			/* handle for server */
	union {
		struct getnew_req {
			tk_set_t obtain;
			tk_set_t toreturn;
		} getnew_req;
		struct revoke_req {
			tk_set_t torevoke;
			int which;
		} revoke_req;
		struct getmore_req {
			tk_set_t toreturn;
			tk_set_t eh;
			unsigned long giveback;
			unsigned long wanted;
			int which;
			int inuse;
		} getmore_req;
	} req_un;
};
struct abg_res {
	struct scheme addr;		/* MUST be first!! */
	int op;
	union {
		struct lookup_res {
			tk_set_t existance;
		} lookup_res;
		struct getnew_res {
			tk_set_t granted;
			tk_set_t already;
			int inuse;
		} getnew_res;
		struct getmore_res {
			int errno;
			unsigned long bg;
		} getmore_res;
	} res_un;
};

/*
 * Service info
 */
#define MAXSERVICES	1
struct service_info {
	int server_node;	/* node that will act as server */
	int service_id;
	int cmpl;		/* client multi-programming level */
	void (*wait)(struct abg_req *, int);
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
};

/*
 * client side
 *
 * State flags:
 * gettingmore - communicate between getmore() and (*tkc_revoke)()
 * recall - communicate between client_return and 'busy' holders
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
	char busy;
	char gettingmore;
	tk_set_t recall;	/* need a recall/release */
	unsigned long wanted;	/* how many units wanted */
	sv_t svsync;
	tk_set_t held;	
	int getmoreerr;		/* status from (*tkc_revoke)->getmore() */
	int inuse;		/* total inuse */
};
/*
 * busy states
 * If any busy bit is on, new getlocks() will wait.
 */
#define BUSY_CNT 	1
#define BUSY_BG		2
#define BUSY_REVOKE	4

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

static void service1_wait(struct abg_req *, int);
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

	cmpl = 1;
	nodes = 1;

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
			fprintf(stderr, "illegal option %c\n", c);
			exit(1);
		}

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
	initmbox("/usr/tmp/abg2mbox");

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
	if ((usptr = usinit("/usr/tmp/abg")) == NULL) 
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
		f = fopen("abg2.LOG", "w");

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
	auto struct abg_req *req;
	auto int dyn;

	mbox = ntombox(node);
	for (;;) {
		readmbox(mbox, (void *)&req, &dyn);
		(services[req->service_id].wait)(req, node);
		if (dyn)
			freemboxmsg(req);
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
static void client_obtain(void *, tk_set_t, tk_set_t, tk_set_t *, tk_set_t *);
static void client_revoke(void *, tk_set_t, tk_set_t, tk_set_t, int);
static void server_revoke(void *o, tks_ch_t h, tk_set_t r, int);
static void server_recalled(void *, tk_set_t, tk_set_t);

tks_ifstate_t svriface = {
	server_revoke,
	server_recalled
};
tkc_ifstate_t iface = {
	client_obtain,
	client_revoke
};

/*
 * stubs and server implementation modules
 */
static void invoke_lookup(tks_ch_t, tk_set_t *);
static void invoke_getnew(tks_ch_t, tk_set_t, tk_set_t, tk_set_t *, tk_set_t *, int *);
static void server_lookup(tks_ch_t h, tk_set_t *granted);
static void server_getnew(tks_ch_t, tk_set_t, tk_set_t, tk_set_t *, tk_set_t *, int *);
static int sgetmore(tks_ch_t, unsigned long *);
static int server_return(tks_ch_t, unsigned long, unsigned long *, int, tk_set_t, tk_set_t, int);
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
	cd.held = TK_NULLSET;
	cd.recall = 0;
	cd.gettingmore = 0;
	cd.busy = 0;
	sv_create(&cd.svsync);
	/* place local data in 'fake' private memory */
	S1_CP(mynode) = &cd;

	if (AM_S1S(mynode)) {
		/* init server side */
		sd.sync = usnewsema(usptr, 0);
		sd.recallsync = usnewsema(usptr, 0);
		sd.nrecallsync = 0;
		sd.recallwait = usnewsema(usptr, 0);
		sd.upd = usnewsema(usptr, 1);
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
		tkc_create("client", cp->cs, (void *)cp, &iface, NTOKENS, granted);
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
 * The cp->upd lock is used to keep local state so that in the fast path
 * one doesn't have to acquire tokens - we shadow the state of the BG_UPDATE
 * token in cp->held so we can check it quickly.
 * As long as we have the upd lock, and held is true, we know
 * we have the token and no-one can take it away (even though we don't have it
 * held).
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
 * grab local thread lock and be sure that our node has BG_UPDATE token held
 * exactly once.
 * Can get the lock for either read or update - for read we need the
 * COUNTERS_READ token, for update we need BG_UPDATE & COUNTERS_UPDATE.
 */
static void
getlock(struct client *cp, int type)
{
	tk_set_t want, need, torelease;
	int wb;

	ussetlock(cp->upd);
	/* grab bgupdate token first if we need it */
	if (type == CNTREAD)
		want = COUNTERS_READ;
	else
		want = TK_ADD_SET(COUNTERS_UPDATE, BG_UPDATE);
	while (TK_IS_IN_SET(cp->held, want) != want) {
		if (cp->busy) {
			/* wait */
			sv_wait(&cp->svsync, cp->upd);
			ussetlock(cp->upd);
		} else {
			/* we're the lucky ones to get token */
			torelease = TK_NULLSET;
			need = TK_SUB_SET(want, cp->held);
			if (TK_IS_IN_SET(need, TK_ADD_SET(COUNTERS_READ, COUNTERS_UPDATE)))
				wb = BUSY_CNT;
			else
				wb = BUSY_BG;
			assert((cp->busy & wb) == 0);
			cp->busy |= wb;
			/*
			 * shadow what the acquire will do with
			 * conflicting tokens
			 */
			if (TK_IS_IN_SET(need, COUNTERS_READ))
				torelease = TK_INTERSECT_SET(cp->held, COUNTERS_UPDATE);
			else if (TK_IS_IN_SET(need, COUNTERS_UPDATE))
				torelease = TK_INTERSECT_SET(cp->held, COUNTERS_READ);
			TK_DIFF_SET(cp->held, torelease);
			usunsetlock(cp->upd);

			if (torelease)
				tkc_release(cp->cs, torelease);

			tkc_acquire(cp->cs, need);
			ussetlock(cp->upd);
			TK_COMBINE_SET(cp->held, need);
			if (cp->recall != TK_NULLSET) {
				/*
				 * bummer - someone wants it back already
				 * Note that we have to call tkc_release
				 * before turning off busy so that
				 * getmore() can't come in and start a
				 * revoke cycle.
				 */
				torelease = cp->recall;
				cp->recall = TK_NULLSET;
				TK_DIFF_SET(cp->held, torelease);
				usunsetlock(cp->upd);
				tkc_release(cp->cs, torelease);

				ussetlock(cp->upd);
				assert(cp->busy & wb);
				cp->busy &= ~wb;
				sv_broadcast(&cp->svsync);
			} else {
				assert(cp->busy & wb);
				cp->busy &= ~wb;
				sv_broadcast(&cp->svsync);
			}
		}
	}
#if OLD
	ussetlock(cp->upd);
	if (type == CNTREAD)
		want = COUNTERS_READ;
	else
		want = TK_ADD_SET(COUNTERS_UPDATE, BG_UPDATE);
	while (TK_IS_IN_SET(cp->held, want) != want) {
		if (cp->busy) {
			/* wait */
			sv_wait(&cp->svsync, cp->upd);
			ussetlock(cp->upd);
		} else {
			/* we're the lucky ones to get token */
			torelease = TK_NULLSET;
			need = TK_SUB_SET(want, cp->held);
			if (TK_IS_IN_SET(need, TK_ADD_SET(COUNTERS_READ, COUNTERS_UPDATE)))
				wb = BUSY_CNT;
			else
				wb = BUSY_BG;
			assert((cp->busy & wb) == 0);
			cp->busy |= wb;
			/*
			 * shadow what the acquire will do with
			 * conflicting tokens
			 */
			if (TK_IS_IN_SET(need, COUNTERS_READ))
				torelease = TK_INTERSECT_SET(cp->held, COUNTERS_UPDATE);
			else if (TK_IS_IN_SET(need, COUNTERS_UPDATE))
				torelease = TK_INTERSECT_SET(cp->held, COUNTERS_READ);
			TK_DIFF_SET(cp->held, torelease);
			usunsetlock(cp->upd);

			if (torelease)
				tkc_release(cp->cs, torelease);

			tkc_acquire(cp->cs, need);
			ussetlock(cp->upd);
			TK_COMBINE_SET(cp->held, need);
			if (cp->recall != TK_NULLSET) {
				/*
				 * bummer - someone wants it back already
				 * Note that we have to call tkc_release
				 * before turning off busy so that
				 * getmore() can't come in and start a
				 * revoke cycle.
				 */
				torelease = cp->recall;
				cp->recall = TK_NULLSET;
				TK_DIFF_SET(cp->held, torelease);
				usunsetlock(cp->upd);
				tkc_release(cp->cs, torelease);

				ussetlock(cp->upd);
				assert(cp->busy & wb);
				cp->busy &= ~wb;
				sv_broadcast(&cp->svsync);
			} else {
				assert(cp->busy & wb);
				cp->busy &= ~wb;
				sv_broadcast(&cp->svsync);
			}
		}
	}
#endif
}

static void
putlock(struct client *cp, int type)
{
	if (type == CNTUPDATE)
		assert(!(cp->busy & BUSY_BG));
	usunsetlock(cp->upd);
}

/*
 * getmore - the guts of the client side
 * We know that here we have the BG_UPDATE token held exactly once
 * We also have the 'lock' from getlock. Thus this routine is inherently
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

	assert(!(cp->busy & BUSY_BG));

	cp->wanted = wanted;
	/*
	 * revoke token - since we have it held, no callouts
	 * will occur
	 */
	tkc_revoke(cp->cs, BG_UPDATE, TK_CLIENT_INITIATED);

	/*
	 * say that we are 'busy' the BGUPDATE token so others will wait
	 * in getlock and not in tkc_acquire
	 * also set gettingmore to communicate with client_revoke()
	 */
	cp->busy |= BUSY_BG;
	cp->gettingmore = 1;
	TK_DIFF_SET(cp->held, BG_UPDATE);
	usunsetlock(cp->upd);

	/*
	 * though we have unlocked the lock - no one can touch 'remain'
	 * or 'nl' since they can't get the token.
	 *
	 * As soon as release finishes, our revoke callout will get called
	 * We won't return until the revoke callout completes which may
	 * well have given us back the token. Since 'busy' is still
	 * set, we know that noone else can get in - this lets us use
	 * cp->remain to shuffle the return value from the revoke.
	 */
	tkc_release(cp->cs, BG_UPDATE);

	/*
	 * The only reason we wouldn't have the token is because the server
	 * couldn't get us anymore
	 */
	cp->gettingmore = 0;
	assert(cp->busy & BUSY_BG);
	ussetlock(cp->upd);
	if (cp->getmoreerr) {
		/* error */
		rv = ENOSPC;
		cp->busy &= ~BUSY_BG;
		cp->recall = TK_NULLSET;
		sv_broadcast(&cp->svsync);
		usunsetlock(cp->upd);
		tkc_return(cp->cs, BG_UPDATE, TK_NULLSET);
	} else if (cp->recall != TK_NULLSET) {
		/*
		 * server wants it!
		 * we need to refuse the return, but not hold the token
		 */
		assert(cp->recall == BG_UPDATE);
		tkc_return(cp->cs, TK_NULLSET, BG_UPDATE);
		cp->busy &= ~BUSY_BG;
		cp->recall = TK_NULLSET;
		sv_broadcast(&cp->svsync);
		usunsetlock(cp->upd);
		getlock(cp, CNTUPDATE);
	} else {
		/*
		 * the only way the hold could fail is if a revoke
		 * came in between the return and hold. We can't put a lock
		 * around the revoke since it might go ahead and send
		 * messages.
		 * Doing a tkc_acquire means that we need to replicate all
		 * the recall code in getlock. We do a quick hold check -
		 * then fall back to calling getlock.
		 */
		tkc_return(cp->cs, TK_NULLSET, BG_UPDATE);
		if (tkc_hold(cp->cs, BG_UPDATE) == BG_UPDATE) {
			TK_COMBINE_SET(cp->held, BG_UPDATE);
			cp->busy &= ~BUSY_BG;
			sv_broadcast(&cp->svsync);
		} else {
			cp->busy &= ~BUSY_BG;
			sv_broadcast(&cp->svsync);
			usunsetlock(cp->upd);
			getlock(cp, CNTUPDATE);
		}
	}

	return rv;
}

/*===================================================================*/

/*
 * client message ops - these correspond 1-1 with server routines
 * that are called from _wait
 */
static void
invoke_lookup(tks_ch_t h, tk_set_t *granted)
{
	struct abg_req req;
	struct abg_res res;

	req.op = ABG_LOOKUP;
	req.service_id = SERVICE1_ID;
	srpc(ntombox(S1S()), h, &req, sizeof(req), &res, sizeof(res));
	assert(req.op == res.op);
	*granted = res.res_un.lookup_res.existance;
}

static void
invoke_getnew(tks_ch_t h,
	tk_set_t obtain,
	tk_set_t toreturn,
	tk_set_t *granted,
	tk_set_t *already,
	int *inuse)
{
	struct abg_req req;
	struct abg_res res;

	req.op = ABG_GETNEW;
	req.service_id = SERVICE1_ID;
	req.req_un.getnew_req.obtain = obtain;
	req.req_un.getnew_req.toreturn = toreturn;

	srpc(ntombox(S1S()), h, &req, sizeof(req), &res, sizeof(res));
	assert(req.op == res.op);
	*granted = res.res_un.getnew_res.granted;
	*already = res.res_un.getnew_res.already;
	*inuse = res.res_un.getnew_res.inuse;
	return;
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
		tks_create("server", sp->ss, (void *)sp, &svriface, NTOKENS);
		sp->initted = 1;
		sp->max = resources;
		sp->remain = resources;
	}
	usvsema(sp->upd);

	/* all set - respond with existance token */
	tks_obtain(sp->ss, h, EXIST_READ, granted, &already);
	assert(already == TK_NULLSET);
	assert(*granted == EXIST_READ);
}

/*
 * respond to GETNEW request from client
 */
static void
server_getnew(tks_ch_t h,
	tk_set_t obtain,
	tk_set_t toreturn,
	tk_set_t *granted,
	tk_set_t *already,
	int *inuse)
{
	struct server *sp = S1_SP();

	if (toreturn != TK_NULLSET)
		tks_return(sp->ss, h, toreturn, TK_NULLSET, TK_NULLSET,
			TK_CLIENT_INITIATED);
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
		tks_obtain(sp->ss, h, obtain, granted, already);
		usvsema(sp->upd);
	} else {
		tks_obtain(sp->ss, h, obtain, granted, already);
	}
	if (TK_IS_IN_SET(*granted, COUNTERS_READ))
		*inuse = sp->total;
	return;
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
	 * fairer to not let one client have some when a nother requested
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
	tks_obtain(sp->ss, h, BG_UPDATE, &granted, &already);
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
server_return(tks_ch_t h, unsigned long giveback, unsigned long *bg, int inuse,
			tk_set_t ok, tk_set_t eh, int which)
{
	struct server *sp = S1_SP();

	if (TK_IS_IN_SET(ok, BG_UPDATE)) {
		uspsema(sp->upd);
		sp->remain += giveback;
		assert(sp->remain <= resources);
		usvsema(sp->upd);
	}
	if (TK_IS_IN_SET(ok, COUNTERS_UPDATE)) {
		uspsema(sp->upd);
		sp->total += inuse;
		assert(sp->total <= resources);
		usvsema(sp->upd);
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
	tk_set_t *already,
	tk_set_t *refused)
{
	struct client *cp = (struct client *)o;
	tk_set_t granted;
	auto int inuse;

	if (AM_S1S(cp->node)) {
		server_getnew(cp->h, obtain, toreturn, &granted, already, &inuse);
	} else {
		invoke_getnew(cp->h, obtain, toreturn, &granted, already, &inuse);
	}
	*refused = TK_SUB_SET(obtain, TK_ADD_SET(granted, *already));
	assert(*refused == TK_NULLSET);
	assert(*already == TK_NULLSET);
	if (TK_IS_IN_SET(granted, COUNTERS_READ))
		cp->inuse = inuse;
}

/*
 * client_revoke - callout from token client module
 *
 * BG_UPDATE can get be a part of the revoke set when 'which' is either
 *	CLIENT_INITIATED or SERVER_RECALL - since the token module
 *	'upgrades' CLIENT_INITIATED to RECALL messages to save messages.
 *	It can also be SERVER_REVOKE.
 * Locking - not really a problem since we are in this revoke callout
 *	and thus noone can have the token so can't really look at
 *	cp->gettingmore, etc..
 *	Since gettingmore is single threaded, we don't need to protect it
 *	and we all understand that it can't change during this function.
 */
static void
client_revoke(void *o, tk_set_t revoke, tk_set_t refuse, tk_set_t eh, int which)
{
	struct abg_req req;
	struct abg_res res;
	struct client *cp = (struct client *)o;
	unsigned long giveback, bg;
	int err, cgetmore = 0;

	assert(refuse == TK_NULLSET);
	bg = 0;
	giveback = 0;
	if (TK_IS_IN_SET(revoke, BG_UPDATE)) {
		if (cp->gettingmore) {
			bg = cp->wanted;
			cgetmore = 1;
			assert(bg > 0);
		} else {
			assert(which != TK_CLIENT_INITIATED);
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
		req.req_un.getmore_req.toreturn = revoke;
		req.req_un.getmore_req.eh = eh;
		req.req_un.getmore_req.giveback = giveback;
		req.req_un.getmore_req.which = which;
		req.req_un.getmore_req.wanted = bg;
		req.req_un.getmore_req.inuse = cp->nl;
		req.op = ABG_GETMORE;
		req.service_id = SERVICE1_ID;

		req.addr.handle = cp->h;
		/*
		 * if we are getting more - do an RPC
		 * This also handle the requirement that if its a
		 * CLIENT_INITIATED revoke we MUST do an RPC
		 */
		if (cgetmore) {
			assert(bg > 0);
			srpc(ntombox(S1S()), cp->h, &req, sizeof(req), &res, sizeof(res));

			if (res.op != ABG_GETMORE) {
				fprintf(stderr, "client_revoke: srpc return message op wrong!\n");
				exit(1);
			}
			bg = res.res_un.getmore_res.bg;
			err = res.res_un.getmore_res.errno;
		} else {
			assert(bg == 0);
			assert(which != TK_CLIENT_INITIATED);
			writembox_copy(ntombox(S1S()), &req, sizeof(req));
		}
	}
	if (cgetmore) {
		assert(cp->busy & BUSY_BG);
		if (err == 0) {
			cp->remain = (long)bg;
		}
		cp->getmoreerr = err;
	} else {
		/* inform client token module that all is done */
		tkc_return(cp->cs, revoke, TK_NULLSET);
	}
}

/*
 * client_return - called for a server invoked REVOKE or RECALL
 * Since we by default hold onto the BGUPDATE token, when a REVOKE
 * or RECALL comes in we need to release it.
 * The tricky part is to be sure that we only wait for 'busy' states
 * when we are in situations where we really must release the token -
 * otherwise we may well hang since if we don't do a revoke, any
 * pending recalls the server has going will not complete.
 */
#define SPEC	TK_ADD_SET(BG_UPDATE, TK_ADD_SET(COUNTERS_READ, COUNTERS_UPDATE))

static void
client_return(struct client *cp, tk_set_t toreturn, int which)
{
	tk_set_t held;

	assert(which != TK_CLIENT_INITIATED);
	/*
	 * we really want to call tkc_revoke before setting the recall bit
	 * since otherwise the upper layer could see that the recall bit
	 * was on and (in some instances) simply not 'hold' the token.
	 * But another thread could then hold the token, but the recall
	 * bit would be off and we hang. By calling revoke first we
	 * know that if we ever get to a hold count of 0, the token
	 * will be sent back. The only downside is that in some cases
	 * the revoke will actually work immediately (thereby calling
	 * (*tkc_revoke), and if the busy bit happens to be set we will
	 * also set the recall bit. This will cause the upper layer to think
	 * it shouldn't 'hold' the token, when in fact its ok to do so.
	 */
	tkc_revoke(cp->cs, toreturn, which);
	if (TK_IS_IN_SET(toreturn, SPEC)) {
		/*
		 * seems always safe to 'revoke' the token. If its held
		 * it simply will go into the REVOKING state.
		 *
		 * The hard part now is to know whether to release the
		 * token or not
		 */
		ussetlock(cp->upd);
		held = TK_INTERSECT_SET(toreturn, cp->held);
		if (held != TK_NULLSET) {
			assert(cp->busy == 0 || \
				(cp->busy & BUSY_REVOKE) || \
				(held == BG_UPDATE && ((cp->busy & BUSY_BG) == 0)) || \
				(held != BG_UPDATE && ((cp->busy & BUSY_CNT) == 0)));
			cp->busy |= BUSY_REVOKE;
			TK_DIFF_SET(cp->held, held);
			/*assert(cp->gettingmore == 0);*/
			usunsetlock(cp->upd);

			tkc_release(cp->cs, held);

			ussetlock(cp->upd);
			cp->busy &= ~BUSY_REVOKE;
			sv_broadcast(&cp->svsync);
			usunsetlock(cp->upd);
		} else {
			if (cp->busy)
				/* tell them to be sure not hold/release it */
				cp->recall = toreturn;
			usunsetlock(cp->upd);
		}
	}
#if OLD
	if (TK_SUB_SET(toreturn, SPEC) != TK_NULLSET) {
		tkc_revoke(cp->cs, TK_SUB_SET(toreturn, SPEC), which);
	}
#endif
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
server_revoke(void *o, tks_ch_t h, tk_set_t r, int which)
{
	struct abg_req req;

	if (TK_IS_IN_SET(r, COUNTERS_UPDATE)) {
		struct server *sp = S1_SP();
		sp->total = 0;
	}

	if (AM_S1S(htonode(h))) {
		client_return((struct client *)S1_CP(htonode(h)), r, which);
		return;
	}

	req.req_un.revoke_req.torevoke = r;
	req.req_un.revoke_req.which = which;
	req.op = ABG_REVOKE;
	req.service_id = SERVICE1_ID;

	writembox_copy(ntombox(htonode(h)), &req, sizeof(req));
}

/*
 * this thread handles any out-of-the-blue messages
 * Both client and server.
 * 'node' is which node we are.
 */
static void
service1_wait(struct abg_req *req, int node)
{
	struct client *cp;	/* client side data */
	struct abg_res res;

	switch (req->op) {
	/* client messages */
	case ABG_REVOKE:
		cp = (struct client *)S1_CP(node);
		assert(!AM_S1S(node));
		client_return(cp, req->req_un.revoke_req.torevoke,
			req->req_un.revoke_req.which);
		break;
	/*
	 * server messages
	 */
	case ABG_GETMORE:
		{
		unsigned long bg;
		assert(AM_S1S(node));

		bg = req->req_un.getmore_req.wanted;
		res.res_un.getmore_res.errno =
			server_return(req->addr.handle,
				req->req_un.getmore_req.giveback,
				&bg,
				req->req_un.getmore_req.inuse,
				req->req_un.getmore_req.toreturn,
				req->req_un.getmore_req.eh,
				req->req_un.getmore_req.which);
		if (req->req_un.getmore_req.wanted > 0) {
			/* we use this to 'mark' whether we need
			 * a reply msg or not
			 */
			res.res_un.getmore_res.bg = bg;
			res.op = ABG_GETMORE;
			writembox_copy((struct mbox *)req->addr.return_addr,
						&res, sizeof(res));
		}
		break;
		}
	case ABG_GETNEW:
		{
		assert(AM_S1S(node));
		server_getnew(req->addr.handle,
				req->req_un.getnew_req.obtain,
				req->req_un.getnew_req.toreturn,
				&res.res_un.getnew_res.granted,
				&res.res_un.getnew_res.already,
				&res.res_un.getnew_res.inuse);
		res.op = ABG_GETNEW;
		writembox_copy((struct mbox *)req->addr.return_addr,
						&res, sizeof(res));
		break;
		}
	case ABG_LOOKUP:
		assert(AM_S1S(node));
		server_lookup(req->addr.handle,
				&res.res_un.lookup_res.existance);
		res.op = ABG_LOOKUP;
		writembox_copy((struct mbox *)req->addr.return_addr,
						&res, sizeof(res));
		break;
	default:
		fprintf(stderr, "service1-wait:Unknown msg op:%d\n", req->op);
		abort();
		break;
	}
}

/* 
 * rpc - 
 */
static int
srpc(struct mbox *to, tks_ch_t h, void *req, int reqlen, void *res, int reslen)
{
	struct mbox *mb;

	mb = allocmbox();

	((struct scheme *)req)->handle = h;	/* our handle */
	((struct scheme *)req)->return_addr = mb;/* where to return message to */
	writembox(to, req, reqlen);

	/* wait for response */
	readmbox_copy(mb, res, reslen);
	freembox(mb);

	return 0;
}
