#include "assert.h"
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

static void sig(int);
static void node(int, int);

static int srpc(struct mbox *, tks_ch_t, void *, int, void *, int);
#define MAXTOKENS	10
#define MAXSERVICES	10
#define SERVICE1_ID	0
#define SERVICE2_ID	1
/* are we the service1 server? */
#define S1S()		(services[SERVICE1_ID].server_node)
#define AM_S1S(n)	(n == services[SERVICE1_ID].server_node)
#define S2S()		(services[SERVICE2_ID].server_node)
#define AM_S2S(n)	(n == services[SERVICE2_ID].server_node)

/*
 * protocol
 */
struct scheme {
	tks_ch_t handle;		/* client handle */
	struct mbox *return_addr;	/* return address for RPC */
};
/*
 * htonode - convert a handle to a node number
 */
#define htonode(h)	(h)
#define nodetoh(n)	(n)

#define TEST2_OBTAIN 1		/* client -> server RPC */
#define TEST2_REVOKE 2		/* server -> client 1-way */
#define TEST2_RETURN 3		/* client -> server 1-way */
#define TEST2_RECALL 4		/* server -> client 1-way */
#define TEST2_RETURN_RECALL 5	/* client -> server 1-way */
#define TEST2_GEN 6		/* client -> server RPC */
struct test2_req {
	struct scheme addr;		/* MUST be first!! */
	int op;
	int service_id;
	union {
		/* obtain request message */
		struct obtain_req {
			tk_set_t obtain;
			tk_set_t toreturn;
			int data[MAXTOKENS];
		} obtain_req;
		struct revoke_req {
			tk_set_t torevoke;
		} revoke_req;
		struct recall_req {
			tk_set_t torecall;
			int flag;
		} recall_req;
		struct return_req {
			tk_set_t toreturn;
			int data[MAXTOKENS];
			tk_set_t refuse;/* for TEST2_RETURN_RECALL only */
			tk_set_t eh;/* for TEST2_RETURN_RECALL only */
		} return_req;
		struct gen_req {
			int gen;
		} gen_req;
	} req_un;
};
struct test2_res {
	struct scheme addr;		/* MUST be first!! */
	int op;
	union {
		/* obtain response message */
		struct obtain_res {
			tk_set_t granted;
			tk_set_t already;
			int data[MAXTOKENS];
		} obtain_res;
		struct gen_res {
			tk_set_t existance;
		} gen_res;
	} res_un;
};

/*
 * XXX currently all services are really pretty much the same - they
 * share the same basic algorithms..
 */
struct service_info {
	int server_node;	/* node that will act as server */
	int service_id;
	int cmpl;		/* client multi-programming level */
	int nclasses;		/* # classes in token
				 * we use nclasses+1 as the existance token
				 */
	void (*wait)(struct test2_req *, int);
	void (*start)(void *);
	void **nsdata;		/* private data */
	tk_set_t alltk;		/* set of all tokens */
} services[MAXSERVICES];

#define S1_P(n) (services[SERVICE1_ID].nsdata[n])/* get private data for node */
#define S1_NCLASS()  (services[SERVICE1_ID].nclasses) /* get nclasses */
#define S1_ALLTK()  (services[SERVICE1_ID].alltk)

int nloops = 10;
pid_t ppid;
unsigned long ndoneclients;
int verbose;
/*
 * the following arena is used for all locks and semaphores for all nodes
 * and all services ...
 */
usptr_t *usptr;
char *Cmd;
int nodes;
int dumplog = 0;

static void service1_wait(struct test2_req *, int);
static void service1_start(void *);

int
main(int argc, char **argv)
{
	int c, i;
	int cmpl, nservices;
	int nclasses;
	unsigned long totclients;
	int nwaiters;

	setlinebuf(stdout);
	setlinebuf(stderr);
	Cmd = strdup(argv[0]);
	ppid = getpid();

	prctl(PR_COREPID, 0, 1);

	cmpl = 1;
	nodes = 0;
	nclasses = 1;

	while ((c = getopt(argc, argv, "TN:vLm:n:t:")) != EOF) 
		switch (c) {
		case 'T':
			__tk_tracenow = 1;
			break;
		case 'v':
			verbose = 1;
			break;
		case 't':
			nclasses = atoi(optarg);
			break;
		case 'L':
			dumplog++;
			break;
		case 'm':
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
	nservices = 1;
	services[0].server_node = 0;
	services[0].service_id = SERVICE1_ID;
	services[0].cmpl = cmpl;
	services[0].nclasses = nclasses;
	services[0].wait = service1_wait;
	services[0].start = service1_start;
	services[0].nsdata = (void **)malloc(nodes * sizeof(void *));
	services[0].alltk = TK_NULLSET;
	/* alltk includes existance token */
	for (i = 0; i < S1_NCLASS()+1; i++)
		TK_COMBINE_SET(services[0].alltk, TK_MAKE(i, TK_READ|TK_WRITE|TK_SWRITE));

#ifdef LATER
	nservices++;
	services[1].server_node = 1;
	services[1].service_id = SERVICE2_ID;
	services[1].cmpl = cmpl;
	services[1].nclasses = nclasses;
#endif
	nodes = nodes < nservices ? nservices : nodes;

	/*
	 * XXX we really need more why??
	 * 1) each service has 'cmpl' client threads per node
	 * 2) Each service server needs a thread per potential client thread 
	 *	that is not on its node
	 * 3) each client not on the server needs a 'wait' thread.
	 * 4) Each server needs a thread to handle the GEN message from each
	 *	node.
	 */
	totclients = nodes * nservices * cmpl;
	nwaiters = nservices * ((nodes - 1) * cmpl);
	nwaiters *= 2;
	nwaiters += (nodes - 1);
	usconfig(CONF_INITUSERS, nwaiters + totclients + 1);

	sigset(SIGTERM, sig);
	sigset(SIGABRT, sig);
	sigset(SIGUSR1, sig);

	prctl(PR_SETEXITSIG, SIGTERM);
	/* initialize RPC */
	initmbox();

	/* call tkc_init to get arenas started up in correct sequence */
	tkc_init();
	tks_init();

	/*
	 * alloc an area for locks - note that all nodes 'share' this
	 * but that should be OK
	 */
	usconfig(CONF_ARENATYPE, US_SHAREDONLY);
	if ((usptr = usinit("/usr/tmp/tktest2")) == NULL) 
		abort();

	/* create 'nodes' */
	for (i = 0; i < nodes; i++)
		node(i, nodes);

	while (ndoneclients != totclients)
		sginap(100);

	if (dumplog)
		tk_printlog(stdout, 1000, dumplog > 2 ? TK_LOG_ALL :
			(dumplog > 1 ? TK_LOG_TS : 0));

	return 0;
}

static void
sig(int s)
{
	if (s == SIGABRT) {
		tk_printlog(stdout, 1000, dumplog > 2 ? TK_LOG_ALL :
			(dumplog > 1 ? TK_LOG_TS : 0));
		sigset(SIGABRT, SIG_DFL);
		abort();
	} else if (s == SIGUSR1) {
		tk_printlog(stdout, -1, dumplog > 2 ? TK_LOG_ALL :
			(dumplog > 1 ? TK_LOG_TS : 0));
		return;
	}
	exit(0);
}

/*
 * Node
 */
/*
 * do all waits for ootb messages - both client and server
 */
static void
node_wait(void *a)
{
	int node = (int)(ptrdiff_t)a;
	struct mbox *mbox;
	auto struct test2_req *req;
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
		 * 	per client node but no client wait threads.
		 * if we are a client we need 'cmpl' client wait threads
		 *	but no server wait threads ..
		 */
		if (services[i].server_node == mynode) {
			int ns;
			ns = (nodes - 1) * services[SERVICE1_ID].cmpl;
			/* one per node for GEN message */
			ns += (nodes - 1);
			for (j = 0; j < ns; j++) {
				if ((spid = sproc(node_wait, PR_SALL,
					    (void *)(ptrdiff_t)mynode)) < 0) {
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
				if ((spid = sproc(node_wait, PR_SALL,
					   (void *)(ptrdiff_t)mynode)) < 0) {
					perror("sproc");
					exit(1);
				}
				if (verbose)
					printf("%s:started up client wait thread on node %d pid %d\n",
						Cmd, mynode, spid);
			}
		}

		if ((spid = sproc(services[i].start, PR_SALL, (void *)(ptrdiff_t)mynode)) < 0) {
			perror("sproc");
			exit(1);
		}
		if (verbose)
			printf("%s:started up service %d on node %d pid %d\n",
					Cmd, i, mynode, spid);
	}
}

/* remote data for clients */
struct cdata {
	TKC_DECL(cs, MAXTOKENS);
	int target[MAXTOKENS];
	ulock_t lock;		/* protect mp client threads */
	tks_ch_t h;		/* handle (contains node we're on) */
	int initted;		/* are tokens created? */
	int node;		/* node we're running on */
	usema_t *sync;		/* sync up threads */
	usema_t *sync2;		/* sync up threads */
	int ref;		/* ref count */
};

/* local data for server */
struct sdata {
	TKS_DECL(ss, MAXTOKENS);
	ulock_t lock;		/* protect remote and local threads */
	struct cdata *cp;	/* server's client data */
	usema_t *sync;		/* for recall operation */
	int gen;		/* which generation we're accepting requests
				 * for
				 */
	usema_t *gensync;	/* wait for generation */
	int nwaitgen;
	int ncl;		/* track # clients that have initialized */
};

#define INOT	1		/* not initialized */
#define IIS	2		/* in progress of initializing */
#define IAM	3		/* initialized */

int shadowtarget[MAXTOKENS];	/* to track 'target' */
char *levels[] = {
	"UNK0",
	"READ",
	"WRITE",
	"UNK3",
	"SWRITE",
	"UNK5",
	"UNK6",
	"UNK7"
};
static void client_obtain(void *, tk_set_t, tk_set_t, tk_set_t *, tk_set_t *);
static void client_revoke(void *, tk_set_t);
static void client_recall(void *, tk_set_t, tk_set_t, tk_set_t);
static void server_revoke(void *o, tks_ch_t h, tk_set_t r);
static void server_recall(void *o, tks_ch_t h, tk_set_t r, int);
static void service1_recalled(void *o, tk_set_t r, tk_set_t);
static void server_return(struct sdata *, struct test2_req *);

static void
service1_play(struct cdata *cp)
{
	int i, lev, class;
	struct timespec ts;
	int loops = 10;

	for (i = 0; i < loops; i++) {
		if (verbose)
			printf("client %d node %d loop %d\n",
					get_pid(), cp->node, i);
		/* get which token */
		lev = 1 << (rand() % 3);
		class = rand() % S1_NCLASS();
		tkc_acquire(cp->cs, TK_MAKE(class, lev));
		ussetlock(cp->lock);
		if (lev == TK_WRITE) {
			/*
			 * note that 2 threads in a client 'node'
			 * can both have the TK_WRITE level - the token
			 * module makes no claims within a node
			 */
			cp->target[class] = rand();
			shadowtarget[class] = cp->target[class];
		} else if (cp->target[class] != shadowtarget[class]) {
			fprintf(stderr, "ERROR:client:%d:0x%x class:%d cd.target %d shadowtarget %d\n",
				get_pid(),
				cp->h,
				class,
				cp->target[class], shadowtarget[class]);
			abort();
		}
		usunsetlock(cp->lock);
		ts.tv_sec = 0;
		ts.tv_nsec = (rand() % 4) * 1000 * 1000;
		nanosleep(&ts, NULL);
		tkc_release(cp->cs, TK_MAKE(class, lev));
	}
}

/*
 * service1_test - the real test code
 * This runs on each node and is multi-threaded
 */
static void
service1_test(int node)
{
	int i, j;
	tkc_ifstate_t iface;
	tks_ifstate_t svriface;
	struct sdata *sp;
	struct cdata *cp;
	tk_set_t existance;
	struct test2_req req;
	struct test2_res res;
	char cname[TK_NAME_SZ], sname[TK_NAME_SZ];

	iface.tkc_obtain = client_obtain;
	iface.tkc_revoke = client_revoke;
	iface.tkc_recall = client_recall;
	svriface.tks_revoke = server_revoke;
	svriface.tks_recall = server_recall;
	svriface.tks_recalled = service1_recalled;

	existance = TK_MAKE(S1_NCLASS(), TK_READ);

	for (i = 0; i < nloops; i++) {
		/*
		 * for server create both server and client token sets
		 */
		sprintf(cname, "Ctest2%d", i);
		sprintf(sname, "Stest2%d", i);
		if (AM_S1S(node)) {
			sp = S1_P(node);
			cp = sp->cp;
			ussetlock(cp->lock);
			if (cp->initted == INOT) {
				cp->initted = IIS;
				usunsetlock(cp->lock);

				/* create token set */
				tks_create(sname, sp->ss, (void *)sp, &svriface,
					S1_NCLASS() + 1);
				tkc_create(cname, cp->cs, (void *)cp, &iface,
					S1_NCLASS() + 1,
					TK_NULLSET);
				cp->initted = IAM;
				/*
				 * immediately give ourselves an existance token
				 */
				tkc_acquire(cp->cs, existance);
				/*
				 * we are now ready to handle generation 'i'
				 */
				ussetlock(cp->lock);
				sp->gen = i;
				sp->ncl = 0;
				for (j = 0; j < sp->nwaitgen; j++)
					usvsema(sp->gensync);
				sp->nwaitgen = 0;
				usunsetlock(cp->lock);
				/*
				 * now wait for all clients to 'register'
				 */
				while (sp->ncl < (nodes - 1)) {
					sginap(10);
				}
				
				/* wake up other server/client threads on our
				 * node
				 */
				for (j = 1; j < services[SERVICE1_ID].cmpl; j++)
					usvsema(cp->sync2);
			} else {
				usunsetlock(cp->lock);
				uspsema(cp->sync2);
			}
		} else {
			cp = S1_P(node);
			ussetlock(cp->lock);
			if (cp->initted == INOT) {
				cp->initted = IAM;
				usunsetlock(cp->lock);

				/*
				 * send message to server requesting permission
				 * to play in generation 'gen'
				 */
				req.req_un.gen_req.gen = i;
				req.op = TEST2_GEN;
				req.service_id = SERVICE1_ID;
				srpc(ntombox(S1S()), cp->h, &req, sizeof(req),
							&res, sizeof(res));
				/*
				 * since we now have existance token
				 * the previous generation must be done so
				 * we can initialize a new one
				 */
				tkc_create(cname, cp->cs, (void *)cp, &iface,
					S1_NCLASS() + 1,
					res.res_un.gen_res.existance);
				/* wake all other threads (on our node) up */
				for (j = 1; j < services[SERVICE1_ID].cmpl; j++)
					usvsema(cp->sync2);
			} else {
				usunsetlock(cp->lock);
				uspsema(cp->sync2);
			}
		}
		/*
		 * At this point all clients have an existance token
		 * so that no matter who finishes first, the server won't
		 * be able to complete the recall until everyone is
		 * at least out of 'play'
		 */
		service1_play(cp);

		/*
		 * all threads but the last one will wait
		 */
		ussetlock(cp->lock);
		assert(cp->ref > 0);
		if (--cp->ref != 0) {
			usunsetlock(cp->lock);
			uspsema(cp->sync);
			continue;
		}
		usunsetlock(cp->lock);

		/* now that we are done - release our 'existance' token */ 
		tkc_release(cp->cs, existance);
		
		/*
		 * we are last thread in our group
		 * The server thread starts a recall
		 */
		if (AM_S1S(node)) {
			/* recall and destroy token */
			tks_recall(sp->ss, existance);
			/* service1_recalled will be called when all done */
			uspsema(sp->sync);
			/* test that everything got sent back */
			for (j = 0; j < S1_NCLASS(); j++) {
				if (cp->target[j] != shadowtarget[j]) {
					fprintf(stderr, "ERROR:server:%d:class:%d cd.target %d shadowtarget %d\n",
						get_pid(),
						j,
						cp->target[j], shadowtarget[j]);
					abort();
				}
			}
			tks_destroy(sp->ss);
		}

		/* reset the ref count and go again */
		cp->ref = services[SERVICE1_ID].cmpl;
		cp->initted = INOT;
		/* wake all other threads (on our node) up */
		for (j = 1; j < services[SERVICE1_ID].cmpl; j++)
			usvsema(cp->sync);
	}
	test_then_add(&ndoneclients, 1);
	pause();

}

static void
service1_launch(void *a)
{
	service1_test((int)(ptrdiff_t)a);
	/* NOTREACHED */
}

void
service1_start(void *a)
{
	int mynode = (int)(ptrdiff_t)a;
	int i;
	struct sdata sd;
	struct cdata cd;	/* this is what we're manipulating */

	/* init a client data object */
	cd.h = nodetoh(mynode);	/* handle is just node number */
	cd.initted = INOT;
	cd.lock = usnewlock(usptr);
	cd.sync = usnewsema(usptr, 0);
	cd.sync2 = usnewsema(usptr, 0);
	cd.node = mynode;
	cd.ref = services[SERVICE1_ID].cmpl;

	if (AM_S1S(mynode)) {
		/* init server side */
		sd.lock = usnewlock(usptr);
		sd.sync = usnewsema(usptr, 0);
		sd.cp = &cd;
		/*
		 * to provide syncronization between loops, clients
		 * send a request to the server requesting to 'play' in a
		 * particular generation - that request will sleep
		 * until the server is ready
		 */
		sd.gensync = usnewsema(usptr, 0);
		sd.gen = 0;	/* "now serving" */
		sd.nwaitgen = 0;
		/* place local data in 'fake' private memory */
		S1_P(mynode) = &sd;
	} else {
		/* place local data in 'fake' private memory */
		S1_P(mynode) = &cd;
	}

	/*
	 * each client / server is multi-threaded
	 */
	for (i = 1; i < services[SERVICE1_ID].cmpl; i++) {
		if (sproc(service1_launch, PR_SALL,
				(void *)(ptrdiff_t)mynode) < 0) {
			perror("sproc");
			exit(1);
		}
	}
	service1_test(mynode);
	/* NOTREACHED */
}

/* ARGSUSED */
static void
client_obtain(void *o,
	tk_set_t obtain,
	tk_set_t toreturn,
	tk_set_t *already,
	tk_set_t *refused)
{
	struct test2_req req;
	struct test2_res res;
	struct cdata *cp = (struct cdata *)o;
	int i;

	if (AM_S1S(cp->node)) {
		/* we are server */
		struct sdata *sp = S1_P(cp->node);
		auto tk_set_t granted;

		if (toreturn != TK_NULLSET)
			tks_return(sp->ss, cp->h, toreturn);
		tks_obtain(sp->ss, cp->h, obtain, &granted, already);
		assert(obtain == granted);
		return;
	}

	/* remote client */
	/* send out obtain request message */
	req.req_un.obtain_req.obtain = obtain;
	req.req_un.obtain_req.toreturn = toreturn;
	req.op = TEST2_OBTAIN;
	req.service_id = SERVICE1_ID;
	for (i = 0; i < S1_NCLASS(); i++) {
		if (TK_IS_IN_SET(toreturn, TK_MAKE(i, TK_WRITE))) {
			/* returning write token - return data also */
			req.req_un.obtain_req.data[i] = cp->target[i];
		}
	}

	srpc(ntombox(S1S()), cp->h, &req, sizeof(req), &res, sizeof(res));

	if (req.op != res.op) {
		fprintf(stderr, "client_obtain: srpc return message op wrong!\n");
		exit(1);
	}

	for (i = 0; i < S1_NCLASS(); i++) {
		if (TK_GET_CLASS(res.res_un.obtain_res.granted, i) != TK_NULLSET) {
			cp->target[i] = res.res_un.obtain_res.data[i];
		}
	}
	*already = res.res_un.obtain_res.already;
}

/*
 * client_revoke - callout from token client module
 */
static void
client_revoke(void *o, tk_set_t revoke)
{
	struct test2_req req;
	struct cdata *cp = (struct cdata *)o;
	int i;
	auto tk_set_t tret;
	struct sdata *sp;

	if (AM_S1S(cp->node))
		sp = S1_P(cp->node);

	/*
	 * if our existance token was recalled, then shut down entire object
	 * This includes sending back all tokens we might have cached
	 */
	if (TK_GET_CLASS(revoke, S1_NCLASS()) != TK_NULLSET) {
		tkc_returning(cp->cs, S1_ALLTK(), &tret, 0);
		revoke |= tret;
	}

	if (AM_S1S(cp->node)) {
		/* we are server */
		tks_return(sp->ss, cp->h, revoke);
	} else {
		/* send return message and data */
		req.req_un.return_req.toreturn = revoke;
		req.op = TEST2_RETURN;
		req.service_id = SERVICE1_ID;
		for (i = 0; i < S1_NCLASS(); i++) {
			if (TK_GET_CLASS(revoke, i) != TK_NULLSET) {
				req.req_un.return_req.data[i] = cp->target[i];
			}
		}

		req.addr.handle = cp->h;
		writembox_copy(ntombox(S1S()), &req, sizeof(req));
	}

	/* inform client token module that all is done */
	tkc_return(cp->cs, revoke, TK_NULLSET);

	if (TK_GET_CLASS(revoke, S1_NCLASS()) != TK_NULLSET)
		tkc_destroy(cp->cs);
}

/*
 * callout when client receives a recall request
 * For service1 a recall means to shut down usage of the object
 */
static void
client_recall(void *o, tk_set_t toreturn, tk_set_t refuse, tk_set_t eh)
{
	struct test2_req req;
	struct cdata *cp = (struct cdata *)o;
	int i;
	auto tk_set_t tret;
	struct sdata *sp;

	if (AM_S1S(cp->node))
		sp = S1_P(cp->node);

	/*
	 * if our existance token was recalled, then shut down entire object
	 * This includes sending back all tokens we might have cached
	 */
	if (TK_GET_CLASS(toreturn, S1_NCLASS()) != TK_NULLSET) {
		tkc_returning(cp->cs, S1_ALLTK(), &tret, 0);
		toreturn |= tret;
	}

	if (AM_S1S(cp->node)) {
		/* we are server */
		tks_return_recall(sp->ss, cp->h, toreturn, refuse, eh);
	} else {
		/* send return message and data */
		req.req_un.return_req.toreturn = toreturn;
		req.req_un.return_req.refuse = refuse;
		req.req_un.return_req.eh = eh;
		req.op = TEST2_RETURN_RECALL;
		req.service_id = SERVICE1_ID;
		for (i = 0; i < S1_NCLASS(); i++) {
			if (TK_GET_CLASS(toreturn, i) != TK_NULLSET) {
				req.req_un.return_req.data[i] = cp->target[i];
			}
		}

		req.addr.handle = cp->h;
		writembox_copy(ntombox(S1S()), &req, sizeof(req));
	}

	/* inform client token module that all is done */
	tkc_return(cp->cs, toreturn, TK_NULLSET);

	if (TK_GET_CLASS(toreturn, S1_NCLASS()) != TK_NULLSET)
		tkc_destroy(cp->cs);
}

/*
 * get client handle from within token module - used for TRACING only
 */
tks_ch_t
getch_t(void *o)
{
	struct cdata *cp = (struct cdata *)o;

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
server_revoke(void *o, tks_ch_t h, tk_set_t r)
{
	struct test2_req req;
	struct sdata *sp = (struct sdata *)o;

	if (AM_S1S(htonode(h))) {
		tkc_revoke(sp->cp->cs, r);
		return;
	}

	req.req_un.revoke_req.torevoke = r;
	req.op = TEST2_REVOKE;
	req.service_id = SERVICE1_ID;

	writembox_copy(ntombox(htonode(h)), &req, sizeof(req));
}

/*
 * callout function to start recall process - called as the result of
 * a tks_recall operation
 */
static void
server_recall(void *o, tks_ch_t h, tk_set_t r, int flag)
{
	struct test2_req req;
	struct sdata *sp = (struct sdata *)o;

	if (AM_S1S(htonode(h))) {
		tkc_recall(sp->cp->cs, r, flag);
		return;
	}

	req.req_un.recall_req.torecall = r;
	req.req_un.recall_req.flag = flag;
	req.op = TEST2_RECALL;
	req.service_id = SERVICE1_ID;

	writembox_copy(ntombox(htonode(h)), &req, sizeof(req));
}

/*
 * callout function - called when a token has been recalled and can now be
 * reclaimed/destroyed
 */
/* ARGSUSED */
static void
service1_recalled(void *o, tk_set_t r, tk_set_t refused)
{
	struct sdata *sp = (struct sdata *)o;

	assert(AM_S1S(sp->cp->node));
	assert(refused == TK_NULLSET);
	usvsema(sp->sync);
}

/*
 * server_return - called by the server message handler when a TEST2_RETURN or
 * TEST2_RETURN_RECALL
 * message is received. The data is updated and the token(s) returned.
 */
static void
server_return(struct sdata *sp, struct test2_req *req)
{
	tk_set_t r;
	int i;

	r = req->req_un.return_req.toreturn;

	/* 
	 * update any modified data
	 * We shouldn't need any locks since the caller still has the
	 * token for WR or RD, noone should be able to change it
	 */
	for (i = 0; i < S1_NCLASS(); i++) {
		if (TK_IS_IN_SET(r, TK_MAKE(i, TK_WRITE))) {
			sp->cp->target[i] = req->req_un.return_req.data[i];
		} else if (TK_IS_IN_SET(r, TK_MAKE(i, TK_READ))) {
			if (sp->cp->target[i] != req->req_un.return_req.data[i]) {
				printf("server revoked a read token and got back different data.\n");
				printf("\tserver data:%d client data %d\n",
					sp->cp->target[i],
					req->req_un.return_req.data[i]);
				abort();
			}
		}
	}

	/* return set of tokens */
	if (req->op == TEST2_RETURN)
		tks_return(sp->ss, req->addr.handle, r);
	else
		tks_return_recall(sp->ss, req->addr.handle, r,
			req->req_un.return_req.refuse,
			req->req_un.return_req.eh);
}

/*
 * this thread waits for any out-of-the-blue messages
 * Both client and server.
 * 'node' is which node we are.
 */
static void
service1_wait(struct test2_req *req, int node)
{
	struct cdata *cp;	/* client side data */
	struct sdata *sp;	/* server side data */
	struct test2_res res;
	tk_set_t toreturn;
	int i;

	switch (req->op) {
	/* client messages */
	case TEST2_REVOKE:
		cp = (struct cdata *)S1_P(node);
		assert(!AM_S1S(node));
		tkc_revoke(cp->cs, req->req_un.revoke_req.torevoke);
		break;
	case TEST2_RECALL:
		cp = (struct cdata *)S1_P(node);
		assert(!AM_S1S(node));
		tkc_recall(cp->cs, req->req_un.recall_req.torecall,
			req->req_un.recall_req.flag);
		break;
	/* server messages */
	case TEST2_OBTAIN:
		sp = (struct sdata *)S1_P(node);
		assert(AM_S1S(node));
		toreturn = req->req_un.obtain_req.toreturn;
		if (toreturn != TK_NULLSET) {
			/*
			 * if returning write token - update data
			 */
			for (i = 0; i < S1_NCLASS(); i++) {
				if (TK_IS_IN_SET(toreturn, TK_MAKE(i, TK_WRITE)))
					sp->cp->target[i] = req->req_un.obtain_req.data[i];
			}
			tks_return(sp->ss, req->addr.handle, toreturn);
		}

		tks_obtain(sp->ss, req->addr.handle,
				req->req_un.obtain_req.obtain,
				&res.res_un.obtain_res.granted,
				&res.res_un.obtain_res.already);
		for (i = 0; i < S1_NCLASS(); i++) {
			if (TK_GET_CLASS(res.res_un.obtain_res.granted, i) != TK_NULLSET)
				res.res_un.obtain_res.data[i] =
							sp->cp->target[i];
		}
		res.op = TEST2_OBTAIN;
		writembox_copy((struct mbox *)req->addr.return_addr,
						&res, sizeof(res));
		break;
	case TEST2_RETURN_RECALL:
	case TEST2_RETURN:
		sp = (struct sdata *)S1_P(node);
		assert(AM_S1S(node));
		/* 1-way message in response to a revoke/recall */
		server_return(sp, req);
		break;
	case TEST2_GEN:
		{
		auto tk_set_t granted, already;

		/* client wants to play at 'gen' - wait till server is ready */
		sp = (struct sdata *)S1_P(node);
		assert(AM_S1S(node));
		ussetlock(sp->cp->lock);
		if (sp->gen > req->req_un.gen_req.gen) {
			abort();
		} else if (sp->gen < req->req_un.gen_req.gen) {
			/* wait */
			assert(sp->nwaitgen >= 0);
			sp->nwaitgen++;
			usunsetlock(sp->cp->lock);
			uspsema(sp->gensync);
		} else
			usunsetlock(sp->cp->lock);
		/* hack - increment # clients */
		ussetlock(sp->cp->lock);
		sp->ncl++;
		usunsetlock(sp->cp->lock);

		/* all set - respond with existance token */
		tks_obtain(sp->ss, req->addr.handle,
				TK_MAKE(S1_NCLASS(), TK_READ),
				&granted,
				&already);
		assert(already == TK_NULLSET);
		assert(granted == TK_MAKE(S1_NCLASS(), TK_READ));
		res.op = TEST2_GEN;
		res.res_un.gen_res.existance = granted;
		writembox_copy((struct mbox *)req->addr.return_addr,
						&res, sizeof(res));
		break;
		}
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
