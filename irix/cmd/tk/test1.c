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

static void client_obtain(void *, tk_set_t, tk_set_t, tk_disp_t, tk_set_t *);
static void client_return(tkc_state_t, void *, tk_set_t, tk_set_t, tk_disp_t);
static void client(int), client_wait(void *), client2(void *);
static void client_launch(void *a);
static void server(void *);
static void sig(int);

#define MAXTOKENS	10

/*
 * htonode - convert a handle to a node number
 */
#define htonode(h)	(h)
#define nodetoh(n)	(n)

#define TEST1_OBTAIN 1		/* client -> server RPC */
#define TEST1_REVOKE 2		/* server -> client 1-way */
#define TEST1_RETURN 3		/* client -> server 1-way */
union test1_req {
	/* obtain request message */
	struct obtain_req {
		tk_set_t obtain;
		tk_set_t toreturn;
		tk_disp_t why;
		int data[MAXTOKENS];
	} obtain_req;
	struct revoke_req {
		tk_set_t torevoke;
		tk_disp_t disp;
	} revoke_req;
	struct return_req {
		tk_set_t toreturn;
		tk_set_t eh;
		tk_disp_t disp;
		int data[MAXTOKENS];
	} return_req;
};
union  test1_res {
	/* obtain response message */
	struct obtain_res {
		tk_set_t granted;
		tk_set_t already;
		int data[MAXTOKENS];
	} obtain_res;
};

struct server_info {
	int strace;
	struct mbox *server_id;
	int nserverthreads;
	FILE *server_log;
};

int nloops = 300;
int cdebug;
int cmpl = 3;	/* client multi-programming level */
struct mbox *server_id;
int dumplog;
pid_t ppid;
unsigned long ndoneclients;
unsigned long amlogging;
int nclasses = 5;
int verbose;

int
main(int argc, char **argv)
{
	int c;
	char *Cmd;
	int serverdebug = 0;
	int nservers = 2;
	int nclients = 2;
	struct server_info svr_info;
	FILE *server_log = stdout;

	setlinebuf(stdout);
	setlinebuf(stderr);
	Cmd = strdup(argv[0]);
	ppid = getpid();

	prctl(PR_COREPID, 0, 1);
	while ((c = getopt(argc, argv, "vLm:S:c:s:n:t:")) != EOF) 
		switch (c) {
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
		case 'c':
			cdebug = atoi(optarg);
			break;
		case 's':
			serverdebug = atoi(optarg);
			break;
		case 'n':
			nloops = atoi(optarg);
			break;
		case 'S':
			if ((server_log = fopen(optarg, "w+")) == NULL) {
				fprintf(stderr, "%s:Cannot open %s\n",
					Cmd, optarg);
				exit(1);
			}
			/*setlinebuf(server_log);*/
			break;
		default:
			fprintf(stderr, "illegal option %c\n", c);
			exit(1);
		}

	printf("test1:loops %d client-multiprogramming %d classes %d\n",
		nloops, cmpl, nclasses);
	/*
	 * we basically need a server per client thread
	 * XXX we really need more why??
	 */
	nservers = nclients * cmpl + 2;
	usconfig(CONF_INITUSERS, (nclients * cmpl) + 2 + nservers);
	sigset(SIGTERM, sig);
	sigset(SIGABRT, sig);
	sigset(SIGUSR1, sig);
	sigset(SIGUSR2, sig);
	sigset(SIGURG, sig);

	prctl(PR_SETEXITSIG, SIGTERM);
	/*
	 * initialize RPC scheme & invent server handle
	 */
	initmbox("/usr/tmp/test1mbox");

	server_id = allocmbox();
	setmbox(0, server_id);
	svr_info.server_id = server_id;
	svr_info.strace = serverdebug;
	svr_info.server_log = server_log;
	svr_info.nserverthreads = nservers;

	/* call tkc_init to get arenas started up in correct sequence */
	tkc_init();

	/* spawn server */
	if (sproc(server, PR_SALL, &svr_info) < 0) {
		perror("sproc");
		exit(1);
	}

	client(nclients);
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
		f = fopen("test1.LOG", "w");

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
 * Client
 */
struct cdata {
	struct mbox *ootb;
	TKC_DECL(cs, MAXTOKENS);
	int nclasses;
	int target[MAXTOKENS];
	usptr_t *cus;
	ulock_t lock;
	tks_ch_t h;		/* handle (contains node) */
};
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

void
client_init(struct cdata *cp)
{
	static tkc_ifstate_t iface;

	/* init client side */
	iface.tkc_obtain = client_obtain;
	iface.tkc_return = client_return;
	/* get a mail box/endpoint for out-of-the-blue messages */
	cp->ootb = allocmbox();
	/* register endpoint */
	setmbox(htonode(cp->h), cp->ootb);

	cp->nclasses = nclasses;
	/*
	 * create client_wait thread
	 * this thread waits for all revoke, etc messages (non-RPC)
	 */
	if (sproc(client_wait, PR_SALL, (void *)cp) < 0) {
		perror("client:sproc");
		exit(1);
	}

	/*
	 * create token set
	 */
	tkc_create("test1", cp->cs, (void *)cp, &iface, cp->nclasses,
			TK_NULLSET, NULL);

	/* alloc an area for locks - note that all nodes 'share' this
	 * but that should be OK
	 */
	usconfig(CONF_ARENATYPE, US_SHAREDONLY);
	if ((cp->cus = usinit("/usr/tmp/tktest1")) == NULL) 
		abort();
	cp->lock = usnewlock(cp->cus);
}

void
client_test(struct cdata *cd)
{
	int i, lev, class;
	struct timespec ts;
	tk_set_t gotten;

	for (i = 0; i < nloops; i++) {
		if (verbose)
			printf("client %d loop %d\n", get_pid(), i);
		/* get which token */
		lev = 1 << (rand() % 3);
		class = rand() % cd->nclasses;
		if (cdebug > 0) {
			printf("Client:%d:0x%x acquiring class %d for %s\n",
				get_pid(), cd->h, class, levels[lev]);
		}
		tkc_acquire(cd->cs, TK_MAKE(class, lev), &gotten);
		if (cdebug > 1)
			tkc_print(cd->cs, stdout, "Client:%d:0x%x:acquired ",
				get_pid(), cd->h);
		ussetlock(cd->lock);
		if (lev == TK_WRITE) {
			/*
			 * note that 2 threads in a client 'node'
			 * can both have the TK_WRITE level - the token
			 * module makes no claims within a node
			 */
			cd->target[class] = rand();
			shadowtarget[class] = cd->target[class];
			if (cdebug > 0)
				printf("Client:%d:0x%x set data to %d\n",
					get_pid(),
					cd->h,
					cd->target[class]);
		} else if (cd->target[class] != shadowtarget[class]) {
			fprintf(stderr, "ERROR:client:%d:0x%x class:%d cd.target %d shadowtarget %d\n",
				get_pid(),
				cd->h,
				class,
				cd->target[class], shadowtarget[class]);
			abort();
		}
		usunsetlock(cd->lock);
		ts.tv_sec = 0;
		ts.tv_nsec = (rand() % 4) * 1000 * 1000;
		nanosleep(&ts, NULL);
		tkc_release(cd->cs, TK_MAKE(class, lev));
	}
}

void
client(int nclients)
{
	struct cdata cd;	/* this is what we're manipulating */
	int i;
	pid_t cpids;
	tk_set_t gotten;

	/* init client side */
	cd.h = nodetoh(1);
	client_init(&cd);

	/* get a read right and the data */
	tkc_acquire(cd.cs, TK_MAKE(0, TK_READ), &gotten);

	if (cdebug > 0)
		tkc_print(cd.cs, stdout, "got read token data:%d\ntokens:",
					cd.target[0]);

	tkc_release(cd.cs, TK_MAKE(0, TK_READ));
	if (cdebug > 0)
		tkc_print(cd.cs, stdout, "after release of read token\n");

	/* get for write */
	if (cdebug > 0)
		printf("\nAcquire for WRITE\n");
	tkc_acquire(cd.cs, TK_MAKE(0, TK_WRITE), &gotten);

	if (cdebug > 0)
		tkc_print(cd.cs, stdout, "got write token data:%d\ntokens:",
					cd.target[0]);
	/* change value of data */
	cd.target[0] = 99;

	tkc_release(cd.cs, TK_MAKE(0, TK_WRITE));
	if (cdebug > 0)
		tkc_print(cd.cs, stdout, NULL);

	/*
	 * spawn another thread
	 */
	if (sproc(client2, PR_SALL, (void *)(ptrdiff_t)get_pid()) < 0) {
		perror("client2:sproc");
		exit(1);
	}
	if (cdebug > 0)
		printf("Client1 pausing...\n");
	blockproc(get_pid());

	/* get read token */
	if (cdebug > 0)
		printf("Client1:Acquire for READ\n");
	tkc_acquire(cd.cs, TK_MAKE(0, TK_READ), &gotten);
	if (cdebug > 0)
		printf("Client1:got read token data:%d\n", cd.target[0]);
	tkc_release(cd.cs, TK_MAKE(0, TK_READ));

	blockproc(get_pid());

	/* a more stressful test */
	for (i = 1; i < cmpl; i++) {
		if ((cpids = sproc(client_launch, PR_SALL, (void *)&cd)) < 0) {
			perror("client_launch:sproc");
			exit(1);
		}
		printf("Client1:Launched pid %d\n", cpids);
	}
	client_test(&cd);
	test_then_add(&ndoneclients, 1);

	while (ndoneclients != (nclients * cmpl))
		sginap(100);

	return;
}

/*
 * Second client - this is effectivly a second 'node'
 */
void
client2(void *a)
{
	struct cdata cd;	/* this is what we're manipulating */
	pid_t c1pid = (pid_t)(ptrdiff_t)a;
	pid_t cpids;
	int i;
	tk_set_t gotten;

	/* init client side */
	cd.h = (tks_ch_t)2L;
	client_init(&cd);

	/* get for write */
	if (cdebug > 0)
		printf("\nClient2:Acquire for WRITE\n");
	tkc_acquire(cd.cs, TK_MAKE(0, TK_WRITE), &gotten);

	if (cdebug > 0)
		tkc_print(cd.cs, stdout, "Client2:got write token data:%d\ntokens:",
			cd.target[0]);

	tkc_release(cd.cs, TK_MAKE(0, TK_WRITE));
	if (cdebug > 0)
		tkc_print(cd.cs, stdout, NULL);

	/* test:
	 * client 2 has token for write
	 * client 1 tries to get read token
	 * client 2 releases write token
	 * client 2 gets read token
	 */
	if (cdebug > 0)
		printf("\nClient2:Acquire for WRITE\n");
	tkc_acquire(cd.cs, TK_MAKE(0, TK_WRITE), &gotten);

	cd.target[0] = 37;
	unblockproc(c1pid);
	sleep(2);
	tkc_release(cd.cs, TK_MAKE(0, TK_WRITE));
	if (cdebug > 0)
		tkc_print(cd.cs, stdout, "Client2:after release of WRITE\n");

	if (cdebug > 0)
		printf("Client2:Acquire for READ\n");
	tkc_acquire(cd.cs, TK_MAKE(0, TK_READ), &gotten);
	if (cdebug > 0)
		printf("Client2:got read token data:%d\n", cd.target[0]);
	tkc_release(cd.cs, TK_MAKE(0, TK_READ));

	/*
	 * now loop, randomly grabbing the token in one of the 3 states
	 */
	printf("Client2:Start loop test\n");
	for (i = 0; i < cd.nclasses; i++) {
		tkc_acquire(cd.cs, TK_MAKE(i, TK_WRITE), &gotten);
		cd.target[i] = 44;
		shadowtarget[i] = cd.target[i];
		tkc_release(cd.cs, TK_MAKE(i, TK_WRITE));
	}
	for (i = 1; i < cmpl; i++) {
		if ((cpids = sproc(client_launch, PR_SALL, (void *)&cd)) < 0) {
			perror("client_launch:sproc");
			exit(1);
		}
		printf("Client2:Launched pid %d\n", cpids);
	}
	unblockproc(c1pid);

	/* a more stressful test */
	client_test(&cd);
	test_then_add(&ndoneclients, 1);
	pause();
}

static void
client_launch(void *a)
{
	client_test((struct cdata *)a);
	test_then_add(&ndoneclients, 1);
	pause();
}

/* ARGSUSED */
static void
client_obtain(void *o,
	tk_set_t obtain,
	tk_set_t toreturn,
	tk_disp_t why,
	tk_set_t *refused)
{
	mesg_t *m;
	union test1_req *req;
	union test1_res *res;
	struct cdata *cp = (struct cdata *)o;
	int i, error;

	if (cdebug > 0) {
		tk_print_tk_set(obtain, stdout,
			"client:%d:0x%x:obtain called on set ",
				get_pid(), cp->h);
		tk_print_tk_set(toreturn, stdout,
			"client:%d:0x%x:obtain returned set ",
				get_pid(), cp->h);
	}

	/* send out obtain request message */
	m = getmesg();
	m->op = TEST1_OBTAIN;
	m->handle = cp->h;
	req = (union test1_req *)&m->request;
	req->obtain_req.obtain = obtain;
	req->obtain_req.toreturn = toreturn;
	req->obtain_req.why = why;
	for (i = 0; i < cp->nclasses; i++) {
		if (TK_IS_IN_SET(toreturn, TK_MAKE(i, TK_WRITE))) {
			/* returning write token - return data also */
			req->obtain_req.data[i] = cp->target[i];
		}
	}

	error = callmbox(server_id, m);
	if (error == 0) {
		res = (union test1_res *)&m->response;

		for (i = 0; i < cp->nclasses; i++) {
			if (TK_GET_CLASS(res->obtain_res.granted, i) != TK_NULLSET) {
				cp->target[i] = res->obtain_res.data[i];
			}
		}
		assert(res->obtain_res.already == TK_NULLSET);
	}
	freemesg(m);
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
	tk_disp_t disp)
{
	mesg_t *m;
	union test1_req *req;
	struct cdata *cp = (struct cdata *)o;
	int i;
	int error = 0;

	if (cdebug > 0) {
		tk_print_tk_set(revoke, stdout,
			"client:%d:0x%x:revoke called on set ",
			get_pid(), ((struct cdata *)o)->h);
	}

	/* send return message and data */
	m = getmesg();
	m->op = TEST1_RETURN;
	m->handle = cp->h;
	req = (union test1_req *)&m->request;
	req->return_req.toreturn = revoke;
	req->return_req.eh = eh;
	req->return_req.disp = disp;
	for (i = 0; i < cp->nclasses; i++) {
		if (TK_GET_CLASS(revoke, i) != TK_NULLSET) {
			req->return_req.data[i] = cp->target[i];
		}
	}

#if !_TK_RETURNIPC
	if (TK_IS_ANY_CLIENT(disp)) {
		error = callmbox(server_id, m);
		assert(error == 0);
		freemesg(m);
	} else {
		/* 1-way msg ok */
		error = writembox(server_id, m);
		assert(error == 0);
	}
#else
	/* 1-way msg ok */
	writembox(server_id, m);
#endif /* _TK_RETURNIPC */

	/* inform client token module that all is done */
	tkc_returned(cp->cs, revoke, TK_NULLSET);
}

/*
 * this thread waits for any out-of-the-blue messages
 */
void
client_wait(void *a)
{
	struct cdata *cp = (struct cdata *)a;
	struct mbox *mbox = cp->ootb;
	mesg_t *m;
	union test1_req *req;

	for (;;) {
		readmbox(mbox, &m);
		req = (union test1_req *)&m->request;
		switch (m->op) {
		case TEST1_REVOKE:
			if (cdebug > 0)
				tk_print_tk_set(req->revoke_req.torevoke,
					stdout,
					"client:%d:0x%x:got REVOKE message on set:",
					get_pid(), mbox);
			tkc_recall(cp->cs, req->revoke_req.torevoke,
				req->revoke_req.disp);
			break;
		default:
			fprintf(stderr, "client-wait:0x%x:Unknown msg op:%d\n",
					mbox, m->op);
			abort();
			break;
		}
		if (m->flags & MESG_RPC)
			replymbox(mbox, m);
		else
			freemesg(m);
	}
}

/*
 * HACK to get client handle from within token module
 */
tks_ch_t
getch_t(void *o)
{
	struct cdata *cp = (struct cdata *)o;

	return(cp->h);
}

/*
 * server process
 */
struct remoteobj {
	TKS_DECL(ss, MAXTOKENS);
	int starget[MAXTOKENS];
	int nclasses;
};
static void server_recall(void *o, tks_ch_t h, tk_set_t r, tk_disp_t);
static void server_return(struct remoteobj *ro, mesg_t *);
static struct server_info *si;

/*
 * to handle revokes, one needs at least 2 server threads since a thread
 * has to pause in revokeall() waiting for all the grants to come in
 * XXX actually probably need n+1 threads where 'n' is the number of clients
 */
static void
server_wait(void *a)
{
	struct remoteobj *ro = (struct remoteobj *)a;
	mesg_t *m;
	union test1_req *req;
	union test1_res *res;
	tk_set_t toreturn;
	tk_disp_t why;
	int i;

	if (si->strace)
		fprintf(si->server_log, "Server:pid:%d\n", getpid());
	for (;;) {
		readmbox(si->server_id, &m);
		req = (union test1_req *)&m->request;
		res = (union test1_res *)&m->response;

		switch (m->op) {
		case TEST1_OBTAIN:
			toreturn = req->obtain_req.toreturn;
			why = req->obtain_req.why;
			if (toreturn != TK_NULLSET) {
				if (si->strace > 0) {
					tk_print_tk_set(req->obtain_req.obtain,
						si->server_log,
						"Server:%d:Rcv OBTAIN from client 0x%x:",
						get_pid(),
						m->handle);
					tk_print_tk_set(toreturn,
						si->server_log,
						"\tserver:%d:OBTAIN returned:",
						get_pid());
				}
				if (si->strace > 1) {
					tks_print(ro->ss, si->server_log,
						"Server:%d:state before tks_return:\n\t",
						get_pid());
				}
				/*
				 * if returning write token - update data
				 */
				for (i = 0; i < ro->nclasses; i++) {
					if (TK_IS_IN_SET(toreturn, TK_MAKE(i, TK_WRITE)))
						ro->starget[i] = req->obtain_req.data[i];
				}
				tks_return(ro->ss, m->handle, toreturn,
					TK_NULLSET, TK_NULLSET, why);
			}

			if (si->strace > 1) {
				tks_print(ro->ss, si->server_log,
					"Server:%d:state before tks_obtain:\n\t",
						get_pid());
			}
			tks_obtain(ro->ss, m->handle,
					req->obtain_req.obtain,
					&res->obtain_res.granted,
					NULL,
					&res->obtain_res.already);
			if (si->strace > 1) {
				tks_print(ro->ss, si->server_log,
					"Server:%d:state after tks_obtain:\n\t",
						get_pid());
			}
			for (i = 0; i < ro->nclasses; i++) {
				if (TK_GET_CLASS(res->obtain_res.granted, i) != TK_NULLSET)
					res->obtain_res.data[i] =
								ro->starget[i];
			}
			replymbox(si->server_id, m);
			break;
		case TEST1_RETURN:
			/*
			 * 1-way message in response to a revoke
			 * Or an RPC in response to a CLIENT_INITIATED revoke
			 */
			server_return(ro, m);
#if !_TK_RETURNIPC
			if (TK_IS_ANY_CLIENT(req->return_req.disp)) {
				replymbox(si->server_id, m);
			} else
				freemesg(m);
#else
			freemesg(m);
#endif
			break;
		default:
			fprintf(stderr, "server:Unknown msg op:%d\n", m->op);
			abort();
			break;
		}
	}
}

static void
server(void *a)
{
	struct remoteobj ro;
	tks_ifstate_t svriface;
	int i;

	si = (struct server_info *)a;

	/* init server */
	tks_init();
	svriface.tks_recall = server_recall;
	tks_create("Stest1", ro.ss, (void *)&ro, &svriface, nclasses, NULL);

	ro.nclasses = nclasses;
	/* set initial data */
	ro.starget[0] = 29;

	/*
	 * spawn additional servers
	 */
	for (i = 1; i < si->nserverthreads; i++) {
		if (sproc(server_wait, PR_SALL, (void *)&ro) < 0) {
			perror("server:sproc");
			exit(1);
		}
	}
	server_wait(&ro);
}

/*
 * callout function that is called from the token server module when a
 * token (set) needs to be revoked. It sends a message to the appropriate
 * client. This is not an RPC
 */
/* ARGSUSED */
static void
server_recall(void *o, tks_ch_t h, tk_set_t r, tk_disp_t disp)
{
	mesg_t *m;
	union test1_req *req;

	if (si->strace > 0) {
		tk_print_tk_set(r, si->server_log,
			"Server:%d:Send REVOKE to client:0x%x:",
				get_pid(), h);
	}

	m = getmesg();
	m->op = TEST1_REVOKE;
	m->handle = h;
	req = (union test1_req *)&m->request;
	req->revoke_req.torevoke = r;
	req->revoke_req.disp = disp;

	callmbox(ntombox(htonode(h)), m);
	freemesg(m);
}

/*
 * server_return - called by the server message handler when a TEST1_RETURN
 * message is received. The data is updated and the token(s) returned.
 */
static void
server_return(struct remoteobj *ro, mesg_t *m)
{
	tk_set_t r;
	int i;
	union test1_req *req = (union test1_req *)&m->request;

	r = req->return_req.toreturn;

	if (si->strace > 0) {
		tk_print_tk_set(r, si->server_log,
			"Server:%d:Rcv RETURN from client 0x%x:",
			get_pid(), m->handle);
	}

	/* 
	 * update any modified data
	 */
	for (i = 0; i < ro->nclasses; i++) {
		if (TK_IS_IN_SET(r, TK_MAKE(i, TK_WRITE))) {
			ro->starget[i] = req->return_req.data[i];
		} else if (TK_IS_IN_SET(r, TK_MAKE(i, TK_READ))) {
			if (ro->starget[i] != req->return_req.data[i]) {
				printf("server revoked a read token and got back different data.\n");
				printf("\tserver data:%d client data %d\n",
					ro->starget[i],
					req->return_req.data[i]);
				abort();
			}
		}
	}

	/* return set of tokens */
	if (si->strace > 1) {
		tks_print(ro->ss, si->server_log,
			"Server:%d:state before tks_return:\n\t",
				get_pid());
	}
	tks_return(ro->ss, m->handle, r, TK_NULLSET,
		req->return_req.eh, req->return_req.disp);
}
