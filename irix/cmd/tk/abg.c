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

#define STKSIZE	(1024*1024*1)
static void sig(int);
static void node(int, int);

#define ABG_LOOKUP	1	/* c->s RPC */
#define ABG_GETNEW	2	/* c->s RPC */
#define ABG_REVOKE	3	/* server -> client 1-way */
#define ABG_GETMORE	5	/* c->s RPC */
#define ABG_RETURN	6	/* c->s 1-way */

union abg_req {
	struct getnew_req {
		tk_set_t obtain;
	} getnew_req;
	struct getmore_req {
		unsigned long giveback;
		unsigned long wanted;
		tk_set_t ok;
	} getmore_req;
	struct revoke_req {
		tk_set_t torevoke;
		tk_disp_t which;
	} revoke_req;
	struct retrecall_req {
		tk_set_t toreturn;
		tk_set_t eh;
		unsigned long giveback;
		tk_disp_t which;
	} retrecall_req;
};
union abg_res {
	struct lookup_res {
		tk_set_t existance;
	} lookup_res;
	struct getnew_res {
		tk_set_t granted;
		tk_set_t already;
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

/* helper for test_and_set .. */
#define TADD(a,b)	((long)test_then_add((unsigned long *)&(a), \
				(unsigned long)(b)))
#define TSUB(a,b)	((long)test_then_add((unsigned long *)&(a), \
				(unsigned long)-(b)))

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
#define COUNTERS_READ		TK_MAKE(COUNTERS_CLASS, TK_READ)
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
				 * valid if have UPDATE_COUNTERS for read
				 */
};

/*
 * client side
 */
struct client {
	TKC_DECL(cs, NTOKENS);
	tks_ch_t h;		/* handle (contains node we're on) */
	int node;		/* node we're on (for debug) */
	int initted;
	int ndone;		/* # threads done */
	usema_t *sync;		/* sync up threads */
	ulock_t upd;
	/* stuff for the ABG algorithm itself */
	unsigned long nl;	/* current # of 'nl''s in use */
	long remain;		/* granted block - remaining */
	int havebgupdate;	/* flag - do we have BG_UPDATE token?? */
};

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

	cmpl = 1;
	nodes = 1;

	while ((c = getopt(argc, argv, "dr:TN:vLm:n:")) != EOF) 
		switch (c) {
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
	 */
	totclients = nodes * cmpl;
	nwaiters = (nodes - 1) * cmpl;
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
	initmbox("/usr/tmp/abgmbox");

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
		f = fopen("abg.LOG", "w");

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
		 * 	per client node but no client wait threads.
		 * if we are a client we need 'cmpl' client wait threads
		 *	but no server wait threads ..
		 */
		if (services[i].server_node == mynode) {
			int ns;
			ns = (nodes - 1) * services[SERVICE1_ID].cmpl;
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
static void invoke_lookup(tks_ch_t, tk_set_t *);
static void invoke_getnew(tks_ch_t, tk_set_t, tk_set_t *, tk_set_t *);
static int invoke_getmore(tks_ch_t, unsigned long, unsigned long *, tk_set_t);
static void server_lookup(tks_ch_t h, tk_set_t *granted);
static void server_getnew(tks_ch_t, tk_set_t, tk_set_t *, tk_set_t *);
static int server_getmore(tks_ch_t, unsigned long, unsigned long *, tk_set_t);
static void server_return(tks_ch_t, unsigned long, tk_set_t, tk_set_t, tk_disp_t);

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
	cd.havebgupdate = 0;
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
		tkc_create("client", cp->cs, (void *)cp, &iface, NTOKENS,
					granted, 0);
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
 * The BG_UPDATE token is ones right to add or delete resources from the
 * pool - one MUST have it.
 * The cp->upd lock is used to keep local state so that in the fast path
 * one doesn't have to acquire tokens - we shadow the state of the BG_UPDATE
 * token in cp->havebgupdate so we can check it quickly.
 * As long as we have the upd lock, and havebgupdate is true, we know
 * we have the token and no-one can take it away (even though we don't have it
 * held).
 */

static void
play(struct client *cp)
{
	int didacquire, rv, j, wanted;
	unsigned long remain;
	unsigned long bg;
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
		ussetlock(cp->upd);
		while (cp->remain < wanted) {
			auto tk_set_t ok;
			/* REFERENCED */
			auto tk_disp_t dofret;

			neededmore = 1;
			usunsetlock(cp->upd);

			/* single thread getting more */
			uspsema(cp->sync);
			if (cp->remain >= wanted) {
				/* something wonderful happened! */
				usvsema(cp->sync);
				ussetlock(cp->upd);
				continue;
			}

			for (;;) {
				/*
				 * its safe to call tkc_returning even if we
				 * don't have the token - it will tell us.
				 */
				tkc_returning(cp->cs, BG_UPDATE, &ok,
						&dofret, TK_WAIT);

				/*
				 * if in fact we had the BG_UPDATE token then
				 * its now in the 'returning' state which is
				 * fine - we will either complete the return
				 * (if the server DENIES our request) or cancel
				 * the return (if the server honors our request)
				 * If however, we didn't have the BG_UPDATE
				 * token then we need to set it in the
				 * 'obtaining' state since we know the server
				 * will (Potentially) send it to us.
				 * Of course, while we wait for tkc_obtaining
				 * to complete - another thread might have
				 * gotten the token for us. In that case we
				 * must loop and try to return it again.
				 */
				if (ok != BG_UPDATE) {
					auto tk_set_t toobtain, toret,
						refused, later;
					auto tk_disp_t dofret;

					tkc_obtaining(cp->cs, BG_UPDATE,
						&toobtain, &toret, &dofret,
						&refused, &later);
					assert(refused == TK_NULLSET);
					assert(later == TK_NULLSET);
					assert(toret == TK_NULLSET);
					if (toobtain != BG_UPDATE) {
						/* we got it! thats not
						 * what we wanted!
						 */
						tkc_release(cp->cs, BG_UPDATE);
						continue;
					}
					remain = 0;
				} else {
					/* we had it - so send back our remain */
					ussetlock(cp->upd);
					remain = cp->remain;
					cp->remain = 0;
					cp->havebgupdate = 0;
					usunsetlock(cp->upd);
				}
				break;
			}

			bg = wanted;
			if (verbose > 1)
				printf("client %d node %d getting %d more resources remain %d\n",
					get_pid(), cp->node, wanted, remain);
			if (AM_S1S(cp->node))
				rv = server_getmore(cp->h, remain, &bg, ok);
			else
				rv = invoke_getmore(cp->h, remain, &bg, ok);
			if (rv == 0 || rv == EALREADY) {
				/* we got more */
				ngetmore++;
				/*
				 * Since we got more rations - we
				 * really got a token also
				 * Note that no matter what state the token is
				 * in (either 'obtaining' or 'returning') it
				 * can't be taken away - this is the time
				 * to update the 'remain' counter.
				 * ASSERTION - we don't need a lock to
				 * bump the count since NO other callout
				 * can happen while the tokens are in this
				 * state.
				 * The code below that returns used
				 * resources is locked in tkc_acquire until
				 * we call one of the routines below.
				 * Note that as soon as we set havebgupdate
				 * to 1, other threads can start to
				 * return things.
				 */
				cp->remain += bg;
				cp->havebgupdate = 1;

				if (ok != BG_UPDATE) {
					/*
					 * we didn't have it 
					 * Some other thread might have gotten
					 * it however so we might have
					 * got an 'ALREADY' indication
					 * from the server.
					 */
					assert(rv != EALREADY);
#if OLD
					if (rv == EALREADY)
						tkc_obtained(cp->cs, TK_NULLSET,
							BG_UPDATE, TK_NULLSET,
							TK_NULLSET);
					else
#endif
						tkc_obtained(cp->cs, BG_UPDATE,
							TK_NULLSET, TK_NULLSET);
					tkc_release(cp->cs, BG_UPDATE);
				} else {
					/* cancel return */
					tkc_returned(cp->cs, TK_NULLSET, ok);
				}

				/* grab lock before releasing other waiters
				 * just for some fairness
				 */
				ussetlock(cp->upd);
				usvsema(cp->sync);
			} else {
				/* no more from server */
				if (ok != BG_UPDATE) {
					/* we didn't have it */
					tkc_obtained(cp->cs, TK_NULLSET,
						BG_UPDATE, TK_NULLSET);
				} else {
					tkc_returned(cp->cs, ok, TK_NULLSET);
				}
				usvsema(cp->sync);
				if (verbose > 1)
					printf("client %d node %d - no more resources\n",
						getpid(), cp->node);
				nenospc++;
				ts.tv_sec = 0;
				ts.tv_nsec = (rand() % 4) * 1000 * 1000;
				nanosleep(&ts, NULL);
				ussetlock(cp->upd);
				continue;
			}
		}

		cp->remain -= wanted;
		cp->nl += wanted;
		usunsetlock(cp->upd);

		if (!neededmore)
			ngotimmed++;

		/* wait a bit */
		ts.tv_sec = 0;
		ts.tv_nsec = (rand() % 4) * 1000 * 1000;
		nanosleep(&ts, NULL);

		/* give it back */
		ussetlock(cp->upd);
		didacquire = 0;
		if (!cp->havebgupdate) {
			tk_set_t got;

			usunsetlock(cp->upd);

			do {
				tkc_acquire(cp->cs, BG_UPDATE, &got);
			} while (got != BG_UPDATE);
			didacquire = 1;
			ussetlock(cp->upd);
			assert(cp->havebgupdate);
		}
		cp->remain += wanted;
		cp->nl -= wanted;
		if (verbose > 1)
			printf("client %d node %d finished w/ %d resources remain %d inuse %d\n",
				get_pid(), cp->node, wanted, cp->remain,
				cp->nl);
		usunsetlock(cp->upd);
		if (didacquire)
			tkc_release(cp->cs, BG_UPDATE);
	}
	printf("client %d node %d - ENOSPC %d, getmore %d, immediate %d times\n",
		getpid(), cp->node, nenospc, ngetmore, ngotimmed);
}

/*===================================================================*/

/*
 * client message ops - these correspong 1-1 with server routines
 * that are called from _wait
 */
static void
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
	assert(error == 0);
	if (error == 0) {
		res = (union abg_res *)&m->response;
		*granted = res->lookup_res.existance;
	}
	freemesg(m);
}

static void
invoke_getnew(tks_ch_t h, tk_set_t obtain, tk_set_t *granted, tk_set_t *already)
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

	error = callmbox(ntombox(S1S()), m);
	assert(error == 0);
	if (error == 0) {
		res = (union abg_res *)&m->response;
		*granted = res->getnew_res.granted;
		*already = res->getnew_res.already;
	}
	freemesg(m);
	return;
}

static int
invoke_getmore(tks_ch_t h, unsigned long giveback, unsigned long *bg, tk_set_t ok)
{
	mesg_t *m;
	union abg_req *req;
	union abg_res *res;
	int error;

	m = getmesg();
	m->op = ABG_GETMORE;
	m->service_id = SERVICE1_ID;
	m->handle = h;
	req = (union abg_req *)&m->request;
	req->getmore_req.wanted = *bg;
	req->getmore_req.giveback = giveback;
	req->getmore_req.ok = ok;

	error = callmbox(ntombox(S1S()), m);
	assert(error == 0);
	if (error == 0) {
		res = (union abg_res *)&m->response;
		*bg = res->getmore_res.bg;
		error = res->getmore_res.errno;
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
static void
server_getnew(tks_ch_t h, tk_set_t obtain, tk_set_t *granted, tk_set_t *already)
{
	struct server *sp = S1_SP();

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
	tks_obtain(sp->ss, h, obtain, granted, NULL, already);
	usvsema(sp->upd);
	return;
}

/*
 * server function for GETMORE request from client
 */
static int
server_getmore(tks_ch_t h,
	unsigned long giveback,
	unsigned long *bg,
	tk_set_t ok)
{
	struct server *sp = S1_SP();
	auto tk_set_t granted, already;
	unsigned long given, wanted;
	int rv = 0;

	/* they usually are 'returning' the BG_UPDATE token */
	if (TK_IS_IN_SET(ok, BG_UPDATE)) {
		tks_return(sp->ss, h, ok, TK_NULLSET, TK_NULLSET,
			TK_MAKE(BG_CLASS, TK_CLIENT_INITIATED));
		uspsema(sp->upd);
		sp->remain += giveback;
	} else
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
static void
server_return(tks_ch_t h, unsigned long giveback, tk_set_t ok, tk_set_t eh, tk_disp_t which)
{
	struct server *sp = S1_SP();

	if (TK_IS_IN_SET(ok, BG_UPDATE)) {

		uspsema(sp->upd);

		sp->remain += giveback;
		assert(sp->remain <= resources);

		usvsema(sp->upd);
	}
	/* this could trigger the tkc_recalled callback */
	tks_return(sp->ss, h, ok, TK_NULLSET, eh, which);
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

	assert(toreturn == TK_NULLSET);
	if (AM_S1S(cp->node)) {
		server_getnew(cp->h, obtain, &granted, &already);
	} else {
		invoke_getnew(cp->h, obtain, &granted, &already);
	}
	*refused = TK_SUB_SET(obtain, TK_ADD_SET(granted, already));
	assert(*refused == TK_NULLSET);
	if (TK_IS_IN_SET(granted, BG_UPDATE)) {
		assert(cp->havebgupdate == 0);
		cp->havebgupdate = 1;
	}
	assert(already == TK_NULLSET);
}

/*
 * client_return - callout from token client module
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
	struct client *cp = (struct client *)o;
	unsigned long giveback;
	int error;

	if (TK_IS_IN_SET(revoke, BG_UPDATE)) {
		ussetlock(cp->upd);
		giveback = cp->remain;
		cp->remain = 0;
		assert(cp->havebgupdate);
		cp->havebgupdate = 0;
		if (verbose > 1)
			printf("client %d node %d returning %d to server upon revoke; in use %d \n",
				get_pid(), cp->node, giveback, cp->nl);
		usunsetlock(cp->upd);
	}

	if (AM_S1S(cp->node)) {
		/* we are server */
		server_return(cp->h, giveback, revoke, eh, which);
	} else {
		/* send return message and data */
		m = getmesg();
		m->op = ABG_RETURN;
		m->service_id = SERVICE1_ID;
		m->handle = cp->h;
		req = (union abg_req *)&m->request;
		req->retrecall_req.toreturn = revoke;
		req->retrecall_req.eh = eh;
		req->retrecall_req.giveback = giveback;
		req->retrecall_req.which = which;

		if (TK_IS_ANY_CLIENT(which)) {
			error = callmbox(ntombox(S1S()), m);
			assert(error == 0);
			freemesg(m);
		} else {
			writembox(ntombox(S1S()), m);
		}
	}
	/* inform client token module that all is done */
	tkc_returned(cp->cs, revoke, TK_NULLSET);
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
}

/*
 * this thread handles any out-of-the-blue messages
 * Both client and server.
 * 'node' is which node we are.
 */
static void
service1_wait(mesg_t *m, int node)
{
	union abg_req *req;
	union abg_res *res;
	struct client *cp;	/* client side data */

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
	case ABG_RETURN:
		assert(AM_S1S(node));
		server_return(m->handle,
			req->retrecall_req.giveback,
			req->retrecall_req.toreturn,
			req->retrecall_req.eh,
			req->retrecall_req.which);
		break;
	case ABG_GETNEW:
		{
		assert(AM_S1S(node));
		server_getnew(m->handle,
				req->getnew_req.obtain,
				&res->getnew_res.granted,
				&res->getnew_res.already);
		break;
		}
	case ABG_LOOKUP:
		assert(AM_S1S(node));
		server_lookup(m->handle,
				&res->lookup_res.existance);
		break;
	case ABG_GETMORE:
		{
		unsigned long bg;
		assert(AM_S1S(node));
		bg = req->getmore_req.wanted;
		res->getmore_res.errno =
				server_getmore(m->handle,
					req->getmore_req.giveback,
					&bg,
					req->getmore_req.ok);
		res->getmore_res.bg = bg;
		break;
		}
	default:
		fprintf(stderr, "service1-wait:Unknown msg op:%d\n", m->op);
		abort();
		break;
	}
	return;
}
