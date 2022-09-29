#define _KMEMUSER
#include <sys/types.h>
#undef _KMEMUSER
#include <sys/reg.h>
#include <sys/prctl.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <assert.h>
#include <bstring.h>
#include <ulocks.h>
#include <stdlib.h>
#include <sys/syssgi.h>
#include <sys/kusharena.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/schedctl.h>

#ifdef NOTDEF
#include <dirent.h>
#include <dlfcn.h>
#endif

#include <errno.h>

#include "nanothread.h"

#define SPROC_BASED		/* It's the only version that works definitely. */
#ifdef UTHREAD_BASED
<<< BOMB >>>
#endif

#ifdef DEBUG
#define dprintf(x)	printf x;
#else
#define dprintf(x)
#endif

#if ((! defined(_COMPILER_VERSION)) || (_COMPILER_VERSION < 700))
extern uint_t __fetch_and_or_64 (uint64_t * a, uint64_t b);
extern uint_t __fetch_and_and_64 (uint64_t * a, uint64_t b);
extern uint_t __fetch_and_or_32 (uint_t * a, uint_t b);
extern uint_t __fetch_and_and_32 (uint_t * a, uint_t b);

#define __fetch_and_or __fetch_and_or_64
#define __fetch_and_and __fetch_and_and_64
#endif

#define INITNID (NULL_NID+2)
#define MASTERNID (NULL_NID+1)

extern int allocrsa (nid_t);

/* Shared Nanothread Data */
int nnid = INITNID;		/* next nid to assign */

/*
 * The states are seperated rather than forcing set_num_processors to
 * setup the upcall, so the setup may be passed parameters that depend
 * the kusharena address or anything else that is setup by
 * set_num_processors.
 */
static volatile uint32_t mp_state = 0;
#define NT_SETUP       0x1	/* safe to init upcall after here. */
#define NT_INIT_UPCALL 0x2	/* can start multiprocessing here. */

#ifdef SPROC_BASED
static pid_t ntab[1024];	/* XXX:to keep spids   */
#endif

#ifdef YIELDCOUNT
int yields[2048];		/* sucessful yield by nid */
int *fyields = &(yields[1024]);	/* failed yield by nid */
#endif

extern int save_rsa (rsa_t *);
typedef greg_t ((*fptr_t) (int));

/*
 * setting up rsa[NULL_RSA] as context to be resumed when execution
 * vehicle needs to execute scheduler code. (upcall)
 */
void
setup_upcall (upcall_handler_t upcall, greg_t arg4, greg_t arg5,
	      greg_t arg6, greg_t arg7)
{
	save_rsa (&(kusp->rsa[NULL_RSA].rsa));

	/* Need to save EPC address in t9 too, since it's PIC code */
	kusp->rsa[NULL_RSA].rsa_gregs[CTX_EPC] = (greg_t) upcall;
	kusp->rsa[NULL_RSA].rsa_gregs[CTX_T9] = (greg_t) upcall;

	/*
	 * Since this is only an indirect reference to the upcall function,
	 * I don't this will be sufficient to ensure that save registers
	 * are set up correctly.
	 */
	kusp->rsa[NULL_RSA].rsa_gregs[CTX_GP] = (greg_t)
		((fptr_t) upcall) (RSAupcall_return_gp);

	kusp->rsa[NULL_RSA].rsa_gregs[CTX_SP] = (greg_t) PRDA + getpagesize () - 16;

	/* Make back trace more sensible, I hope. */
	kusp->rsa[NULL_RSA].rsa_gregs[CTX_RA] = (greg_t) NULL;

	kusp->rsa[NULL_RSA].rsa_gregs[CTX_A4] = (greg_t) arg4;
	kusp->rsa[NULL_RSA].rsa_gregs[CTX_A5] = (greg_t) arg5;
	kusp->rsa[NULL_RSA].rsa_gregs[CTX_A6] = (greg_t) arg6;
	kusp->rsa[NULL_RSA].rsa_gregs[CTX_A7] = (greg_t) arg7;

	mp_state |= NT_INIT_UPCALL;
}

/****************************************************************************/

/*
 * Exported interfaces
 */
kusharena_t *kusp = NULL;

#ifdef NOTDEF
__inline uint8_t
log2_32 (uint32_t num)
{
	/* 0 is never passed as an argument */
	unsigned char placeholder = 16, delta = 8;

	for (; delta != 0; delta >>= 1) {
		if ((num >> placeholder) != 0) {
			placeholder += delta;
		}
		else {
			placeholder -= delta;
		}
	}
	if ((num >> placeholder) == 0) {
		placeholder--;
	}
	return placeholder;
}
#endif

__inline uint8_t
log2_64 (uint64_t num)
{
	/* 0 is never passed as an argument */
	unsigned char placeholder = 32, delta = 16;

	for (; delta != 0; delta >>= 1) {
		if ((num >> placeholder) != 0) {
			placeholder += delta;
		}
		else {
			placeholder -= delta;
		}
	}
	if ((num >> placeholder) == 0) {
		placeholder--;
	}
	return placeholder;
}

int
set_num_processors (nid_t n, int static_rsa_alloc)
{
	/* This is the first routine the user will invoke when using
	 * nanothreads.  Initialize all nanothread related data structures
	 * here.
	 */

#ifdef NOTDEF
	/* arch. independent pipe-dream */
	extern int test_clean_llbit (void);
	char extralib[MAXNAMLEN];

	if (n < 1)
		return -1;

	sprintf (extralib, "libnano");
	if (static_rsa_alloc) {
		strcat (extralib, ".static");
	}
	else {
		strcat (extralib, ".dynamic");
	}
	if (test_clean_llbit ()) {
		strcat (extralib, ".fastresume");
	}
	else {
		strcat (extralib, ".slowresume");
	}
	if (sgidladd (extralib, RTLD_NOW) == NULL) {
		perror ("failed to add user-switch via sgidladd");
		return -1;
	}
#endif

	if (kusp == NULL) {
		int fd, len, minsize;

		if (usconfig (CONF_INITUSERS, n + 4) == -1) {
			perror ("usconfig initusers");
			exit (-1);
		}
		if (usconfig (CONF_ARENATYPE, US_SHAREDONLY) == -1) {
			perror ("usconfig arenatype");
			exit (-1);
		}
		/* Map kusharena in user address space */
		minsize = sizeof (kusharena_t) - NT_MAXRSAS * sizeof (padded_rsa_t);
		len = minsize + (n + 1) * (int)sizeof (padded_rsa_t);
		if ((fd = open ("/hw/sharena", O_RDWR)) < 0) {
			perror ("open:/hw/sharena");
			exit (-1);
		}
		if ((kusp = (kusharena_t *)
		     mmap (NULL, len, PROT_READ | PROT_WRITE,
			MAP_SHARED, fd, 0)) == (kusharena_t *) MAP_FAILED) {
			kusp = NULL;
			perror ("mmap:/hw/sharena");
			exit (-1);
		}
		dprintf (("Allocated SHARENA @ 0x%llx\n", kusp));

		/* Initialize kusharena */
		/* Initialize yield accounting array */
#ifdef YIELDCOUNT
		kusp->yieldp = yields;
		for (len = 0; len < 2048; len++) {
			yields[len] = 0;
		}
#endif

		for (len = 0; len < n; len++) {
			kusp->rsa[len].rsa.rsa_context.gregs[CTX_SP] =
				(greg_t) 0;
		}

		kusp->nrequested = n;
		if (static_rsa_alloc) {
			for (len = MASTERNID; len < n + MASTERNID; len++) {
				kusp->nid_to_rsaid[len] = len;
			}
		}
		printf ("set_num_processors: allocated %d\n", n);
		mp_state |= NT_SETUP;
		return 0;
	}
	else {
		printf ("set_num_processors: changing nrequested not implemented\n");
		return -1;
	}
}

void
startNthd (nid_t newnid)
{
	register short rsaid;

	if (schedctl (SETHINTS) == -1L) {
		printf ("[%d], error in SETHITS, acquiring prda\n", getpid ());
		perror ("schedctl:SETHITS");
		exit (-1);
	}
	__fetch_and_add (&kusp->nallocated, 1);
	rsaid = kusp->nid_to_rsaid[newnid];
	assert (rsaid != NULL_RSA);
	kusp->rsa[rsaid].rsa.rsa_context.gregs[CTX_AT] = newnid;
	kusp->rsa[rsaid].rsa.rsa_context.gregs[CTX_A0] = newnid;
	PRDA->t_sys.t_affinity_nid = newnid;
	/* printf("[%d], executing rsa %d\n", newnid, rsaid); */
	run_nid (&resume_nid, kusp, newnid);

	/* Slaves probably get sigterm from setexitsig or exit on their own. */
#ifndef NDEBUG
	abort ();
#else
	_exit (0);
#endif
}

#ifdef SPROC_BASED
void
wait_threads (void)
{
	int i;

	for (i = 1; i < kusp->nrequested; i++) {
		if (waitpid (ntab[i], NULL, 0) == -1) {
			fprintf (stderr, "problem with wait.\n");
			kill_threads (SIGTERM);
		}
	}
}

void
kill_threads (int signal)
{
	register int i;
	register nid_t myid = PRDA->t_sys.t_nid;
	i = __fetch_and_or (&mp_state, NANO_signaling);
	if ((i & NANO_signaling) != 0) {
		return;
	}
	for (i = 0; i < myid; i++) {
		kill (ntab[i], signal);
	}
	for (i++; i < kusp->nrequested; i++) {
		kill (ntab[i], signal);
	}
	raise (signal);
	i = __fetch_and_and (&mp_state, ~NANO_signaling);
	assert ((i & NANO_signaling) != 0);
}
#endif

/*
 * init_upcall will go away as soon as uthread allocator is complete.
 */
#define FIRST_STACK (char *)0x10000000000LL
#define STACK_OFFSET 0x4000000LL
#define STACK_SIZE 0x200000LL

int
start_threads (nanostartf_t init_upcall)
{
	register pid_t cpid;
	register int i, rsaid;
	register nid_t currnid;

#ifdef MMAPSTACKS
	caddr_t stacks[NT_MAXNIDS];
	caddr_t location;
	int fd;
#endif
	volatile char bigstack[STACK_SIZE];

	if (kusp == NULL)
		return -1;

	if (((mp_state & NT_SETUP) == 0) ||
	    ((mp_state & NT_INIT_UPCALL) == 0)) {
		return -1;
	}
	bigstack[STACK_SIZE - 1] = 1;
	bigstack[STACK_SIZE - 1] = bigstack[STACK_SIZE - 1] - 1;	/* warning bait */

#ifdef MMAPSTACKS

	/* XXX: premap stacks to ensure that threads can read one
	 * anothers stacks. */

	if ((fd = open ("/dev/zero", O_RDWR)) < 0) {
		perror ("open:/dev/zero");
		exit (-1);
	}
	for (i = INITNID; i < kusp->nrequested; i++) {
		location = FIRST_STACK - STACK_OFFSET * i - STACK_SIZE;
		stacks[i] = mmap (location, STACK_SIZE, PROT_READ |
				  PROT_WRITE, MAP_SHARED, fd, 0);
	}
#endif
	for (i = INITNID; i < (kusp->nrequested + INITNID - 1); i++) {
		currnid = __fetch_and_add (&nnid, 1);
		rsaid = kusp->nid_to_rsaid[currnid];
		if (rsaid == NULL_RSA) {
			rsaid = allocrsa (currnid);
		}
		bcopy (&kusp->rsa[0], &kusp->rsa[rsaid], sizeof (padded_rsa_t));
		kusp->rsa[rsaid].rsa.rsa_context.gregs[CTX_EPC] =
			kusp->rsa[rsaid].rsa.rsa_context.gregs[CTX_T9] = (greg_t) init_upcall;

		if (kusp->nid_to_rsaid[currnid] == NULL_RSA) {
			perror ("allocrsa allocated rsa[NULL_RSA]");
			abort ();
		}

		/* Stack allocation should move into the kernel. */
#ifdef MMAPSTACKS
		cpid = sprocsp (startNthd, PR_SALL, (void *)currnid,
				stacks[currnid], STACK_SIZE);
#else
		cpid = sprocsp ((void (*)())startNthd, PR_SALL,
				(void *)(NULL + currnid), NULL, STACK_SIZE);
#endif
		switch (cpid) {
		case 0:
			perror ("sproc:child");
			exit (-1);
		case -1:
			perror ("sproc");
			break;
		default:
			ntab[i] = cpid;
			break;
		}
	}
	currnid = MASTERNID;
	__fetch_and_add (&kusp->nallocated, 1);
	if (kusp->nid_to_rsaid[MASTERNID] == NULL_RSA) {
		allocrsa (currnid);
	}

	if (kusp->nid_to_rsaid[currnid] == NULL_RSA) {
		perror ("allocrsa allocated rsa[NULL_RSA]");
		abort ();
	}

	PRDA->t_sys.t_nid = MASTERNID;
	return 0;
}

/* pthread version of nanothreads is a little broken */
#ifdef NOTDEF
int _restart_nid (nid_t, kusharena_t *);

/* ARGSUSED */
int
resume_nid (nid_t myid, kusharena_t * kusp, nid_t resume)
{

	if (kusp->nid_to_rsaid[myid]) {

		/* RSA sets are preallocated by library */

		return (_resume_nid (myid, kusp, resume));

	}
	else {
		/* allocate RSA on demand */

		uint64_t oldrbits, mask;

		/* start by clearing t_nid in case we get pre-empted here,
		 * since we don't want the kernel saving our state into
		 * an RSA.
		 */
		PRDA->t_sys.t_nid = NULL_NID;

		/* attempt to reserve new nid */

		mask = (1LL << (resume % 64));
		oldrbits = __fetch_and_and (&kusp->rbits[resume / 64], ~mask);
		if (oldrbits & mask) {
			register short rsaid;

			/* have reserved new nid */
			rsaid = allocrsa (myid);
			if (!save_rsa (kusp->rsa[rsaid].rsa)) {

				__fetch_and_or (&kusp->rbits[myid / 64],
						1LL << (myid % 64));

				/* our nid (myid)  is now "runable " */

				/* _restart_nid will:
				 *    load register values from RSA
				 *      set fbits for resume's RSA
				 *      set correct nid value (resume)
				 *      clear nid_to_rsaid[resume]
				 */

#ifdef YIELDCOUNT
				yields[myid]++;
#endif /* YIELDCOUNT */
				_restart_nid (resume, kusp);

				/* UNREACHABLE */

			}
			else {
				/* save_rsa() returned non-zero which means
				 * we've been resumed after a susccesful
				 * yield.
				 */
				return (1);	/* yield worked */
			}

		}
		else {
			/* could not obtain new nid, continue old nid */

#ifdef YIELDCOUNT
			fyields[myid]++;
#endif /* YIELDCOUNT */
			PRDA->t_sys.t_nid = myid;
			return (0);	/* yield failed */
		}
	}
	/* UNREACHABLE */
	return (0);
}

#endif /* NOTDEF */

/* ARGSUSED */
int
heavy_resume_nid (nid_t myid, kusharena_t * kusp, nid_t resume)
{
	int result;

	result = (int)syssgi (SGI_DYIELD, resume);
#ifdef YIELDCOUNT
	if (result)
		yields[myid]++;
	else
		fyields[myid]++;
#endif
	return (result);
}

nid_t
getnid ()
{
	return ((nid_t) PRDA->t_sys.t_nid);
}
#pragma weak getnid_ = getnid

/* This is probaby going to get erased soon. */
#ifdef NOTDEF
void
block_nid (nid_t nid)
{
	blockproc (ntab[nid]);
}

void
unblock_nid (nid_t nid)
{
	unblockproc (ntab[nid]);
}

#endif

/*----------------------------------------------------------------------*/
/*                      RSA MAINTENANCE                                 */
/* --------------------------------------------------------------------- */
int
allocrsa (nid_t nid)
{
	int rsaid;
	__int64_t mask, oldfbits;

	if (kusp == NULL)
		return NULL_RSA;
	assert (nid < NT_MAXNIDS);
	for (;;) {
		for (rsaid = NULL_RSA + 1;
		     rsaid < NT_MAXRSAS; rsaid++) {
			mask = (1LL << (rsaid % 64));
			oldfbits = __fetch_and_and (&kusp->fbits[rsaid / 64], ~mask);
			if (oldfbits & mask) {
				kusp->nid_to_rsaid[nid] = rsaid;
				dprintf (("[%d], allocated rsa %d to nid %d\n",
					  getpid (), rsaid, nid));
				return (rsaid);
			}
		}
		/* for now, if no rsa is avaliable, just spin waiting for one */
	}
}

int
resume_any_nid (nid_t oldnid, kusharena_t * kusp)
{
	register volatile uint64_t *rbitp = kusp->rbits;
	register uint64_t bitv;
	register int32_t nrequested = kusp->nrequested;
	register nid_t newnid;
	for (newnid = MASTERNID; newnid < nrequested; newnid += 64) {
		bitv = *rbitp;
		if (bitv) {
			return (resume_nid (oldnid, kusp, log2_64 (bitv) + newnid));
		}
	}
	return (0);
}
