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

#define TEST3_LOOKUP	1	/* c->s RPC */
#define TEST3_GETVALUE	2	/* c->s RPC */
#define TEST3_REVOKE	3	/* server -> client 1-way */
#define TEST3_RVALUE	5	/* c->s IPC/RPC */

union test3_req {
	struct lookup_req {
		unsigned long id;
	} lookup_req;
	struct getvalue_req {
		tk_set_t obtain;
		tk_set_t toreturn;
		tk_disp_t why;
		void *handle;
		unsigned long value;
	} getvalue_req;
	struct revoke_req {
		tk_set_t torevoke;
		tk_set_t eh;
		tk_disp_t which;
		void *handle;
		unsigned long id; /* used for debugging */
		unsigned long value;
	} revoke_req;
};
union test3_res {
	struct lookup_res {
		tk_set_t existance;
		void *handle;
	} lookup_res;
	struct getvalue_res {
		tk_set_t granted;
		tk_set_t already;
		unsigned long value;
		unsigned long id;
	} getvalue_res;
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
 */
#define EXIST_CLASS		0
#define EXIST_READ		TK_MAKE(EXIST_CLASS, TK_READ)


#define DATA_CLASS		1
#define DATA_READ		TK_MAKE(DATA_CLASS, TK_READ)
#define SDATA_READ		TK_MAKE_SINGLETON(DATA_CLASS, TK_READ)
#define DATA_UPDATE		TK_MAKE(DATA_CLASS, TK_WRITE)
#define SDATA_UPDATE		TK_MAKE_SINGLETON(DATA_CLASS, TK_WRITE)
#define DATA_SET		TK_ADD_SET(DATA_READ, DATA_UPDATE)

#define NTOKENS	2
/*
 * server side
 * data per node
 */
struct server {
	usema_t *upd;
	ulock_t lckvalue;
	struct sdata *data;	/* list of active sdata's */
};

/* server side distributed object */
struct sdata {
	TKS_DECL(ss, NTOKENS);
	unsigned long id;	/* object id */
	unsigned long value;	/* object value */
	struct sdata *next;
};

/*
 * client side
 * data per node
 */
struct client {
	tks_ch_t h;		/* handle (contains node we're on) */
	int node;		/* node we're on (for debug) */
	int ndone;		/* # threads done */
	ulock_t *upd;
	sv_t wait;		/* wait for in-progress objects */
	struct cdata *data;	/* list of active cdata's */
};

/* client side of distributed object */
struct cdata {
	TKC_DECL(cs, NTOKENS);
	unsigned long id;	/* object value */
	unsigned long value;	/* object value */
	tks_ch_t h;		/* client handle ... should be global */
	void *handle;		/* server handle */
	int flags;
	int ref;
	ulock_t *upd;
	struct cdata *next;
};
#define CL_INPROGRESS	1

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
int dumplog = 0;
int maxobjects = 100;

static void service1_wait(mesg_t *, int);
static void service1_start(void *, size_t);
static void service1_test(int node);

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

	while ((c = getopt(argc, argv, "o:dTN:vLm:n:")) != EOF) 
		switch (c) {
		case 'd':
			usedebuglocks = 1;
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
		case 'o':
			maxobjects = atoi(optarg);
			break;
		case 'N':
			nodes = atoi(optarg);
			break;
		case 'n':
			nloops = atoi(optarg);
			break;
		default:
			fprintf(stderr, "test3:illegal option %c\n", c);
			fprintf(stderr, "Usage:test3 [options]\n");
			fprintf(stderr, "\t-n #\t# loops\n");
			fprintf(stderr, "\t-N #\t# nodes\n");
			fprintf(stderr, "\t-o #\t# objects\n");
			fprintf(stderr, "\t-m #\tclient multi-programming level\n");
			fprintf(stderr, "\t-v\tverbose\n");
			exit(1);
		}

	printf("test3: nodes %d loops %d client-multiprogramming %d objects %d\n",
		nodes, nloops, cmpl, maxobjects);
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
	usconfig(CONF_INITSIZE, (32*1024) * nodes);

	sigset(SIGTERM, sig);
	sigset(SIGABRT, sig);
	sigset(SIGUSR1, sig);
	sigset(SIGUSR2, sig);
	sigset(SIGURG, sig);

	prctl(PR_SETEXITSIG, SIGTERM);
	/* initialize RPC */
	initmbox("/usr/tmp/test3mbox");

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
	if ((usptr = usinit("/usr/tmp/test3")) == NULL) 
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
		f = fopen("test3.LOG", "w");

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

		/*
		 * start up 'threads' that do testing
		 */
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
static void server_idle(void *, tk_set_t);

tks_ifstate_t svriface = {
	server_recall,
	server_recalled,
	server_idle
};
tkc_ifstate_t iface = {
	client_obtain,
	client_return
};

/*
 * stubs and server implementation modules
 */
static int invoke_lookup(tks_ch_t, unsigned long, tk_set_t *, void **);
static int invoke_getvalue(tks_ch_t, tk_set_t, tk_set_t, tk_disp_t,
	tk_set_t *, tk_set_t *, void *, unsigned long *, unsigned long *);
static int server_lookup(tks_ch_t h, unsigned long, tk_set_t *, void **);
static int server_getvalue(tks_ch_t, tk_set_t, tk_set_t, tk_disp_t,
	tk_set_t *, tk_set_t *, void *, unsigned long *, unsigned long *);
static void server_return(tks_ch_t, unsigned long, void *, tk_set_t, tk_set_t, tk_disp_t);

static int getlock(struct cdata *, int);
static void putlock(struct cdata *, int);
static struct cdata *getid(struct client *cp, unsigned long id);
static void releaseid(struct client *, struct cdata *);

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
	cd.upd = usnewlock(usptr);
	sv_create(&cd.wait);
	cd.data = NULL;
	cd.ndone = 0;
	/* place local data in 'fake' private memory */
	S1_CP(mynode) = &cd;

	if (AM_S1S(mynode)) {
		/* init server side */
		sd.upd = usnewsema(usptr, 1);
		sd.data = NULL;
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
 * called by all threads - they all simulate accessing a variety of
 * like-objects
 * each thread selects an object and a mode (read/write), gets access
 * to it, plays with it a while and releases it. Upon last us
 * the object is flushed back to the server
 */
static void
service1_test(int node)
{
	struct client *cp;
	struct cdata *cd;
	int i, j, rw;
	unsigned long id, nvalue;
	auto unsigned int seed;

	seed = getpid();
	cp = S1_CP(node);
	for (j = 0; j < nloops; j++) {
		id = (unsigned long)(rand_r(&seed) % maxobjects);
		rw = (rand_r(&seed) % 2) == 0? TK_READ : TK_WRITE;

		if ((cd = getid(cp, id)) == NULL) {
			/* some form of error */
			if (verbose)
				printf("test3:client %d failed to get object %lu\n",
					node, id);
			sginap(4);
			continue;
		}

		if (verbose > 1)
			printf("test3:client %d has object %d\n",
				node, id);
		/* play a bit */
		for (i = 0; i < 10; i++) {
			rw = (rand_r(&seed) % 2) == 0? TK_READ : TK_WRITE;
			if (getlock(cd, rw)) {
				/* error - server dead */
				assert(0);
			}
			assert(cd->value);
			if (rw == TK_WRITE) {
				do {
					nvalue = rand_r(&seed);
				} while (nvalue == cd->value || nvalue == 0);
				cd->value = nvalue;
			}
			sginap(rand_r(&seed) % 5);
			putlock(cd, rw);
		}

		/* release object */
		releaseid(cp, cd);
	}

	ussetlock(cp->upd);
	cp->ndone++;
	usunsetlock(cp->upd);

	test_then_add(&ndoneclients, 1);
	for (;;)
		pause();
}

static struct cdata *
getid(struct client *cp, unsigned long id)
{
	struct cdata *cd, **pcd;
	tk_set_t granted;
	int rv;

	/* grab lock protecting client chain */
retry:
	ussetlock(cp->upd);
	for (cd = cp->data; cd; cd = cd->next) {
		if (cd->id == id) {
			/* have one already */
			ussetlock(cd->upd);
			if (cd->flags & CL_INPROGRESS) {
				usunsetlock(cd->upd);
				sv_wait(&cp->wait, cp->upd);
				goto retry;
			}
			assert(cd->ref > 0);
			cd->ref++;
			usunsetlock(cd->upd);
			usunsetlock(cp->upd);
			return cd;
		}
	}

	/* none there - create one, put onlist and mark INPROGRESS */
	cd = malloc(sizeof(*cd));
	cd->handle = NULL;
	cd->id = id;
	cd->flags = CL_INPROGRESS;
	cd->ref = 1;
	cd->h = cp->h;
	cd->upd = usnewlock(usptr);
	cd->next = cp->data;
	cp->data = cd;
	usunsetlock(cp->upd);

	/* look up on server */
	if (AM_S1S(cp->node)) {
		rv = server_lookup(cp->h, id, &granted, &cd->handle);
	} else {
		rv = invoke_lookup(cp->h, id, &granted, &cd->handle);
	}

	assert(rv == 0);
	/*
	 * if rv != 0 then the server id dead
	 * if granted is TK_NULLSET then server didn't have what we wanted
	 */
	if (rv == 0 && granted != TK_NULLSET) {
		/* we now have existance token */
		tkc_create("client", cd->cs, (void *)cd, &iface, NTOKENS,
							granted, (void *)id);
		ussetlock(cd->upd);
		assert(cd->flags & CL_INPROGRESS);
		cd->flags &= ~CL_INPROGRESS;
		usunsetlock(cd->upd);

		ussetlock(cp->upd);
		sv_broadcast(&cp->wait);
		usunsetlock(cp->upd);
	} else {
		/* some form of error */
		ussetlock(cp->upd);
		for (pcd = &cp->data; *pcd; pcd = &(*pcd)->next) {
			if (*pcd == cd) {
				*pcd = cd->next;
				break;
			}
		}
		usfreelock(cd->upd, usptr);
		free(cd);
		sv_broadcast(&cp->wait);
		usunsetlock(cp->upd);
		cd = NULL;
	}
	return cd;
}

/*
 * releaseid - client all done with object 
 * called either from client app code or client_recall
 */
static void
releaseid(struct client *cp, struct cdata *cd)
{
	tk_set_t toreturn;
	tk_disp_t dofret;
	struct cdata **pcd;

	ussetlock(cd->upd);

	assert(cd->ref > 0);

	if (--cd->ref > 0) {
		usunsetlock(cd->upd);
		return;
	}

	/*
	 * ref count 0 - mark as INPROGRESS, return to server, then
	 * delete from client
	 */
	cd->flags |= CL_INPROGRESS;
	usunsetlock(cd->upd);
	tkc_release(cd->cs, EXIST_READ);

	/*
	 * Must pass TK_WAIT in since otherwise we can get the
	 * following race:
	 * thread gets WRITE token and then releases it and it's ref on
	 *	the object (the WRITE token gets CACHED).
	 * a revoke comes in from the server, the WRITE token goes into
	 *	the RETURNING state.
	 * another client thread drops its ref count, the ref count goes
	 *	to 0 and it calls tkc_returning. If it doesn't wait
	 *	for the WRITE token to finish returning, this client
	 *	could come back from tkc_returning, send an RPC to the
	 *	server. get the response and call tkc_destroy - all before
	 *	the thread that is in client_revoke() gets back and runs
	 *	and calls tkc_return ...
	 */
	tkc_returning(cd->cs, TK_ADD_SET(EXIST_READ, DATA_SET), &toreturn,
					&dofret, TK_WAIT);
	assert(TK_SUB_SET(toreturn, TK_ADD_SET(EXIST_READ, DATA_SET)) == TK_NULLSET);
	/*
	 * send back tokens and data
	 * Turns out that the revoke callout is just the thing we need
	 * It also calls tkc_return();
	 * Note that the token server will not return to us until any
	 * outstanding revokes it has (that would come into client_recall())
	 * have completed. Thus once we return there can be no more outstanding
	 * messages for this object.
	 */
	client_return(NULL, cd, toreturn, TK_NULLSET, dofret);

	/* delete from client list */
	ussetlock(cp->upd);
	for (pcd = &cp->data; *pcd; pcd = &(*pcd)->next) {
		if (*pcd == cd) {
			*pcd = cd->next;
			break;
		}
	}
	sv_broadcast(&cp->wait);
	usunsetlock(cp->upd);

	usfreelock(cd->upd, usptr);
	tkc_destroy(cd->cs);
	free(cd);
}

/*
 * Can get the lock for either read or update
 */
static int
getlock(struct cdata *cd, int type)
{
	int rv;
	if (type == TK_READ)
		rv = tkc_acquire1(cd->cs, SDATA_READ);
	else
		rv = tkc_acquire1(cd->cs, SDATA_UPDATE);
	if (rv == 0)
		ussetlock(cd->upd);
	return rv;
}

static void
putlock(struct cdata *cd, int type)
{
	usunsetlock(cd->upd);
	if (type == TK_READ)
		tkc_release1(cd->cs, SDATA_READ);
	else
		tkc_release1(cd->cs, SDATA_UPDATE);
}

/*===================================================================*/

/*
 * client message ops - these correspond 1-1 with server routines
 * that are called from _wait
 */
static int
invoke_lookup(tks_ch_t h, unsigned long id, tk_set_t *granted, void **handle)
{
	mesg_t *m;
	union test3_req *req;
	union test3_res *res;
	int error;

	m = getmesg();
	m->op = TEST3_LOOKUP;
	m->service_id = SERVICE1_ID;
	m->handle = h;
	req = (union test3_req *)&m->request;
	req->lookup_req.id = id;
	error = callmbox(ntombox(S1S()), m);
	if (error == 0) {
		res = (union test3_res *)&m->response;
		*granted = res->lookup_res.existance;
		*handle = res->lookup_res.handle;
	}
	freemesg(m);
	return error;
}

static int
invoke_getvalue(tks_ch_t h,
	tk_set_t obtain,
	tk_set_t toreturn,
	tk_disp_t why,
	tk_set_t *granted,
	tk_set_t *already,
	void *handle,
	unsigned long *value,
	unsigned long *id)
{
	mesg_t *m;
	union test3_req *req;
	union test3_res *res;
	int error;

	m = getmesg();
	m->op = TEST3_GETVALUE;
	m->service_id = SERVICE1_ID;
	m->handle = h;
	req = (union test3_req *)&m->request;
	req->getvalue_req.obtain = obtain;
	req->getvalue_req.toreturn = toreturn;
	req->getvalue_req.why = why;
	req->getvalue_req.handle = handle;
	req->getvalue_req.value = *value;

	error = callmbox(ntombox(S1S()), m);
	if (error == 0) {
		res = (union test3_res *)&m->response;
		*granted = res->getvalue_res.granted;
		*already = res->getvalue_res.already;
		*value = res->getvalue_res.value;
		*id = res->getvalue_res.id;
	}
	freemesg(m);
	return error;
}

/*=================================================================*/
/*
 * server routines - invoked from _wait and from local clients
 */
static int
server_lookup(tks_ch_t h, unsigned long id, tk_set_t *granted, void **handle)
{
	auto tk_set_t already;
	struct server *sp;
	struct sdata *sd;

	assert(id < maxobjects);
	sp = (struct server *)S1_SP();
	if (sp == NULL) {
		/* race .. return equiv of ENOENT */
		*granted = TK_NULLSET;
		return 0;
	}

	uspsema(sp->upd);
	for (sd = sp->data; sd; sd = sd->next) {
		if (sd->id == id)
			break;
	}
	if (sd) {
		/* got one already */
		tks_obtain(sd->ss, h, EXIST_READ, granted, NULL, &already);
	} else {
		/* create a new one */
		sd = malloc(sizeof(*sd));
		sd->id = id;
		sd->value = 1;
		sd->next = sp->data;
		sp->data = sd;
		tks_create("server", sd->ss, (void *)sd, &svriface, NTOKENS, 
				(void *)id);
		/* all set - respond with existance token */
		tks_obtain(sd->ss, h, EXIST_READ, granted, NULL, &already);
	}
	*handle = sd;
	usvsema(sp->upd);
	tks_notify_idle(sd->ss, EXIST_READ);

	assert(already == TK_NULLSET);
	assert(*granted == EXIST_READ);
	return 0;
}

/*
 * respond to GETVALUE request from client
 */
static int
server_getvalue(tks_ch_t h,
	tk_set_t obtain,
	tk_set_t toreturn,
	tk_disp_t why,
	tk_set_t *granted,
	tk_set_t *already,
	void *handle,
	unsigned long *value,
	unsigned long *id)
{
	struct sdata *sd = (struct sdata *)handle;

	if (toreturn != TK_NULLSET) {
		if (TK_IS_IN_SET(toreturn, DATA_UPDATE)) {
			/* shouldn't need a lock since we have WRITE token */
			sd->value = *value;
		}
		assert(!TK_IS_IN_SET(toreturn, EXIST_READ));
		tks_return(sd->ss, h, toreturn, TK_NULLSET, TK_NULLSET,
			why);
	}
	tks_obtain(sd->ss, h, obtain, granted, NULL, already);

	/* always give back value */
	*value = sd->value;
	*id = sd->id;

	return 0;
}

/*
 * server function to handle a return from client - either via a recall
 * or a revoke or a client-initiated return.
 */
static void
server_return(tks_ch_t h,
	unsigned long value,
	void *handle,
	tk_set_t ok,
	tk_set_t eh,
	tk_disp_t which)
{
	struct sdata *sd = (struct sdata *)handle;

	if (TK_IS_IN_SET(ok, DATA_UPDATE)) {
		/* shouldn't need lock - no one else can touch */
		sd->value = value;
	}

	/* this could trigger the tks_idle callback */
	tks_return(sd->ss, h, ok, TK_NULLSET, eh, which);

	return;
}

/*
 * callout function - called when a token has been recalled and can now be
 * reclaimed/destroyed
 */
/* ARGSUSED */
static void
server_recalled(void *o, tk_set_t r, tk_set_t refused)
{
	assert(0);
}

static void
server_idle(void *o, tk_set_t idle)
{
	struct sdata *sd = (struct sdata *)o;
	struct sdata **psd;
	struct server *sp;
	auto tk_set_t st;
	int found;

	sp = (struct server *)S1_SP();
	assert(idle == EXIST_READ);
	uspsema(sp->upd);
	tks_getstate(sd->ss, &st);

	if (TK_GET_LEVEL(st, EXIST_CLASS)) {
		/* someone grabbed it again */
		usvsema(sp->upd);
		return;
	}
	assert(st == TK_NULLSET);

	/* remove from list on server */
	for (psd = &sp->data, found = 0; *psd; psd = &(*psd)->next) {
		if (*psd == sd) {
			*psd = sd->next;
			found = 1;
			break;
		}
	}
	assert(found);
	usvsema(sp->upd);

	tks_destroy(sd->ss);
	free(sd);
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
	struct cdata *cd = (struct cdata *)o;
	tk_set_t granted, already;
	auto unsigned long value, id;
	int error;

	if (AM_S1S(cd->h)) {
		error = server_getvalue(cd->h, obtain, toreturn, why,
				&granted, &already, cd->handle, &value, &id);
	} else {
		error = invoke_getvalue(cd->h, obtain, toreturn, why,
				&granted, &already, cd->handle, &value, &id);
	}
	assert(error == 0);
	assert(id == cd->id);

	*refused = TK_SUB_SET(obtain, TK_ADD_SET(granted, already));
	assert(*refused == TK_NULLSET);
	assert(already == TK_NULLSET);
	if (TK_IS_IN_SET(granted, DATA_SET))
		cd->value = value;
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
	union test3_req *req;
	struct cdata *cd = (struct cdata *)o;
	/* REFERENCED */
	int error = 0;

	if (AM_S1S(cd->h)) {
		/* we are server */
		server_return(cd->h, cd->value, cd->handle, revoke, eh, which);
	} else {
		m = getmesg();
		m->op = TEST3_RVALUE;
		m->service_id = SERVICE1_ID;
		m->handle = cd->h;
		req = (union test3_req *)&m->request;
		req->revoke_req.torevoke = revoke;
		req->revoke_req.eh = eh;
		req->revoke_req.which = which;
		req->revoke_req.value = cd->value;
		req->revoke_req.handle = cd->handle;

		if (TK_IS_ANY_CLIENT(which)) {
			error = callmbox(ntombox(S1S()), m);
			freemesg(m);
		} else {
			error = writembox(ntombox(S1S()), m);
		}
	}
	/* inform client token module that all is done */
	tkc_returned(cd->cs, revoke, TK_NULLSET);
}

/*
 * client_recall - called for a server invoked REVOKE or RECALL
 * Note that because of the token server protocol, it won't respond
 * to a client_initiated return until all server initiated revokes are
 * completed, we know that the handle presented here is valid.
 */
static void
client_recall(
	struct client *cp,
	void *handle,
	unsigned long id,
	tk_set_t toreturn,
	tk_disp_t which)
{
	struct cdata *cd;

	/* assert that handle is still valid */
	ussetlock(cp->upd);
	for (cd = cp->data; cd; cd = cd->next) {
		if (cd->handle == handle)
			break;
	}
	assert(cd);
	assert(cd->id == id);
	usunsetlock(cp->upd);

	assert(!TK_IS_IN_SET(toreturn, EXIST_READ));
	tkc_recall(cd->cs, toreturn, which);
#if OLD
	ussetlock(cd->upd);
	usunsetlock(cp->upd);
	/*
	 * as soon as we release this lock another client thread could
	 * release this object. We don't want it to go away until
	 * we're done with it
	 */
	assert(cd->ref > 0);
	cd->ref++;
	usunsetlock(cd->upd);
	assert(!TK_IS_IN_SET(toreturn, EXIST_READ));
	(void) tkc_recall(cd->cs, toreturn, which);

	assert(cd->ref > 0);
	releaseid(cp, cd);
#endif
}

/*
 * get client handle from within token module - used for TRACING only
 */
tks_ch_t
getch_t(void *o)
{
	struct cdata *cd = (struct cdata *)o;

	assert(cd);
	return(cd->h);
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
	union test3_req *req;
	struct sdata *sd = (struct sdata *)o;

	if (AM_S1S(htonode(h))) {
		struct client *cp;
		cp = (struct client *)S1_CP(htonode(h));
		client_recall(cp, sd, sd->id, r, which);
		return;
	}

	m = getmesg();
	m->op = TEST3_REVOKE;
	m->service_id = SERVICE1_ID;
	m->handle = h;
	req = (union test3_req *)&m->request;
	req->revoke_req.torevoke = r;
	req->revoke_req.which = which;
	req->revoke_req.handle = sd;
	req->revoke_req.id = sd->id;

	(void)writembox(ntombox(htonode(h)), m);
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
	union test3_req *req;
	union test3_res *res;

	req = (union test3_req *)&m->request;
	res = (union test3_res *)&m->response;

	switch (m->op) {
	/* client messages */
	case TEST3_REVOKE:
		assert(!AM_S1S(node));
		cp = (struct client *)S1_CP(node);
		client_recall(cp, req->revoke_req.handle,
			req->revoke_req.id,
			req->revoke_req.torevoke,
			req->revoke_req.which);
		break;
	/*
	 * server messages
	 */
	case TEST3_RVALUE:
		{
		assert(AM_S1S(node));

		server_return(m->handle,
			req->revoke_req.value,
			req->revoke_req.handle,
			req->revoke_req.torevoke,
			req->revoke_req.eh,
			req->revoke_req.which);
		break;
		}
	case TEST3_GETVALUE:
		{
		auto unsigned long value;
		assert(AM_S1S(node));
		value = req->getvalue_req.value;
		server_getvalue(m->handle,
				req->getvalue_req.obtain,
				req->getvalue_req.toreturn,
				req->getvalue_req.why,
				&res->getvalue_res.granted,
				&res->getvalue_res.already,
				req->getvalue_req.handle,
				&value,
				&res->getvalue_res.id);
		res->getvalue_res.value = value;
		break;
		}
	case TEST3_LOOKUP:
		assert(AM_S1S(node));
		server_lookup(m->handle, req->lookup_req.id,
				&res->lookup_res.existance,
				&res->lookup_res.handle);
		break;
	default:
		fprintf(stderr, "service1-wait:Unknown msg op:%d\n", m->op);
		abort();
		break;
	}
	return;
}
