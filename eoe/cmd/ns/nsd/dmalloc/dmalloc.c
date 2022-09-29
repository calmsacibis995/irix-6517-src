/*
 * Copyright 1996, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that (i) the above copyright notices and this
 * permission notice appear in all copies of the software and related
 * documentation, and (ii) the name of Silicon Graphics may not be
 * used in any advertising or publicity relating to the software
 * without the specific, prior written permission of Silicon Graphics.
 *
 * THE SOFTWARE IS PROVIDED "AS-IS" AND WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS, IMPLIED OR OTHERWISE, INCLUDING WITHOUT LIMITATION, ANY
 * WARRANTY OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
 *
 * IN NO EVENT SHALL SILICON GRAPHICS BE LIABLE FOR ANY SPECIAL,
 * INCIDENTAL, INDIRECT OR CONSEQUENTIAL DAMAGES OF ANY KIND, OR ANY
 * DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
 * WHETHER OR NOT ADVISED OF THE POSSIBILITY OF DAMAGE, AND ON ANY
 * THEORY OF LIABILITY, ARISING OUT OF OR IN CONNECTION WITH THE USE
 * OR PERFORMANCE OF THIS SOFTWARE.
 *
 */

/* 
 * This source has been customized for NSD static linking!
 *
 *  MALLOC_BAD_ASSET -  hardwired message (always assert)
 *  MALLOC_CHECK_ATEXEC - defaults to 0
 *
 */


/*
    dmalloc.c

    Front-end to malloc that does counting and stuff.
    Expects there to be a "real" malloc and free and realloc.
*/

#undef NDEBUG /* don't even think about it!! */

#include "dmalloc.h"
#include "stacktrace.h"
#include <sys/types.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <bstring.h>
#include <sys/param.h>
#include <assert.h>
#include <unistd.h>
#include <stdarg.h>
#include <fcntl.h>
#include <ctype.h>

/*
    sproc creates three spinlocks: __malloclock, __monlock, and __randlock.
    We can't use __malloclock because we call the real malloc which uses it,
    and that would cause deadlock.
*/
static pid_t lock_owner;/* XXXassume shared--this may not be a good assumption*/
static int lock_depth; /* number of times this process holds it */
#define THELOCK __randlock
extern ulock_t THELOCK;
#ifdef LOCK_DEBUG
#define lock_debug getenv("MALLOC_LOCK_DEBUG")
#else
#define lock_debug 0
#endif
static void
LOCK_MALLOC(int inc)
{
    if (lock_debug)
	printf("%d(%d): LOCKING %d+%d (owner=%d)\n", get_pid(), getpid(), lock_depth,inc,lock_owner);
    if (!THELOCK) {
	lock_depth += inc;
	if (lock_debug)
	    printf("%d(%d): locked %d\n", get_pid(), getpid(), lock_depth);
	return;			      /* no sprocs occured yet */
    }

    if (lock_owner != get_pid()) {    /* someone else has it, or no one does */
	ussetlock(THELOCK);
	lock_owner = get_pid();
	assert(lock_depth == 0);
    } else {			      /* this process already has it */
	assert(lock_depth > 0);
    }

    lock_depth += inc;		     /* we have the lock */
    if (lock_debug)
	printf("%d(%d): LOCKED %d\n", get_pid(), getpid(), lock_depth);
}
static void
UNLOCK_MALLOC(int inc)
{
    if (lock_debug)
	printf("%d(%d): UNLOCKING %d-%d (owner=%d)\n", get_pid(), getpid(), lock_depth,inc,lock_owner);
    if (!THELOCK) {
	lock_depth -= inc;
	if (lock_debug)
	    printf("%d(%d): unlocked %d\n", get_pid(), getpid(), lock_depth);
	return;			     /* no sprocs occured yet */
    }

    assert(lock_owner == get_pid());
    assert(lock_depth > 0);

    if ((lock_depth -= inc) == 0) {
	lock_owner = 0;
	usunsetlock(THELOCK);
    }
    if (lock_debug)
	printf("%d(%d): UNLOCKED %d\n", get_pid(), getpid(), lock_depth);
}

#define CREATE_MALLOC_LOCK() /* not needed, since we use monlock */


#undef MIN
#define MIN(a,b) ((a)<(b)?(a):(b))
#undef MAX
#define MAX(a,b) ((a)>(b)?(a):(b))
#define INRANGE(foo,bar,baz)	((foo(bar))&&((bar)baz))
#define streq(s,t) (strcmp(s, t) == 0)

#if 0 /* screw it, just use the strong symbols */
#ifndef real_malloc
#define real_malloc mAlLoC
#endif
#ifndef real_free
#define real_free fReE
#endif
#ifndef real_realloc
#define real_realloc rEaLlOc
#endif
#ifndef real__execve
#define real__execve _eXeCvE
#endif

#ifndef real_amalloc
#define real_amalloc aMaLlOc
#endif
#ifndef real_afree
#define real_afree aFrEe
#endif
#ifndef real_arealloc
#define real_arealloc aReAlLoC
#endif
#endif /* 0 */

#ifndef real_malloc
#define real_malloc _malloc
#endif
#ifndef real_free
#define real_free _free
#endif
#ifndef real_realloc
#define real_realloc _realloc
#endif
#ifndef real__execve
#define real__execve _execve
#endif
#ifndef real_amalloc
#define real_amalloc _amalloc
#endif
#ifndef real_afree
#define real_afree _afree
#endif
#ifndef real_arealloc
#define real_arealloc _arealloc
#endif
#ifndef real_acreate
#define real_acreate _acreate
#endif
#ifndef real_adelete
#define real_adelete _adelete
#endif


#   ifdef __cplusplus
    extern "C" {
#   endif
extern void *real_malloc(size_t);
extern void real_free(void *);
extern void *real_realloc(void *, size_t);
extern int real__execve(const char *file, char*const*argv, char*const*envp);
extern void *real_amalloc(size_t, void *);
extern void real_afree(void *, void *);
extern void *real_arealloc(void *, size_t, void *);
extern void *real_acreate(void *, size_t, int, void *, void *(*)(size_t, void *));
extern void real_adelete(void *);
#   ifdef __cplusplus
    };
#   endif

#define type_MALLOC ((void *)0)
#define type_FREE ((void *)1)

#ifndef MAX_MALLOCS
#define MAX_MALLOCS 10000	/* max number of occurrances of malloc&free */
#endif
#ifndef MAX_STACKTRACE_DEPTH
#define MAX_STACKTRACE_DEPTH 10
#endif
#define MAX_SUM_OF_STACKTRACES (MAX_MALLOCS * MAX_STACKTRACE_DEPTH) /* XXX for now, until we do better bounds checking */
#define SYMBUFSIZ 1024

struct hist {
    void *type;				/* type_MALLOC or type_FREE */

    int ncalls, nbytes;
    int ncalls_freed, nbytes_freed;	/* if it's a malloc */

    int stacktrace_depth;
    void **stacktrace;			/* pointer into stacktraces buffer */
};

/*
 * Stuff that gets prepended and appended to each malloc block.
 * Wow, that's a lot of stuff.
 */
static struct header {
    int magic0, magic1;
    int n;
    struct hist *hist;
    struct tailer *tail;
#define LINKED_LIST
#ifdef LINKED_LIST
    struct header *prev, *next;		/* linked list */
#endif /* LINKED_LIST */
    void *ap; /* arena pointer */
    int magic2, magic3;
} aheader = { 0xdeadbeef, 0xfedcba98, 0,0,0,
#ifdef LINKED_LIST
					     0,0, 
#endif /* LINKED_LIST */
						  NULL, 0x3456789a, 0xabbaabba};
static struct tailer {
    int magic4, magic5;
    struct header *head;
    void *real_mem; /* same as head unless memaligned */
    int magic6, magic7;
} atailer = { 0xcacacaca, 0x89898989, NULL, NULL, 0xface6969, 0x81818181 };

#ifdef LINKED_LIST
/* XXX relies on the assumption that the first small block allocated from
   an arena is always ap + 0x130 */
#define FIRSTHEADER_ADDR(ap) ((ap) ? (struct header **)(((char *)(ap))+0x130) : &firstheader)
#define THE_ARENA_LOCK(ap) (*(ulock_t *)(((char *)(ap))+39*sizeof(void*)))
#define LOCK_ARENA(ap) ((ap) && THE_ARENA_LOCK(ap) ? ussetlock(THE_ARENA_LOCK(ap)) : 0)
#define UNLOCK_ARENA(ap) ((ap) && THE_ARENA_LOCK(ap) ? usunsetlock(THE_ARENA_LOCK(ap)) : 0)

static struct header *firstheader = NULL;

static void
link_header(struct header *hdr, void *ap)
{
    struct header **firstheader_addr = FIRSTHEADER_ADDR(ap);
    LOCK_ARENA(ap);
    hdr->next = *firstheader_addr;
    if (*firstheader_addr)
	(*firstheader_addr)->prev = hdr;
    hdr->prev = NULL;
    *firstheader_addr = hdr;
    UNLOCK_ARENA(ap);
}
static void
unlink_header(struct header *hdr, void *ap)
{
    LOCK_ARENA(ap);
    if (hdr->next)
	hdr->next->prev = hdr->prev;
    if (hdr->prev)
	hdr->prev->next = hdr->next;
    else
	*FIRSTHEADER_ADDR(ap) = hdr->next;
    UNLOCK_ARENA(ap);
}

#define MAXARENAS 100
static void *arenas[MAXARENAS];
static int narenas = 0;
#endif /* LINKED_LIST */

static struct header *lowest_head_ever = NULL, *highest_head_ever = NULL;

/* big static variables */
static struct hist hists[MAX_MALLOCS];
static int nhists = 0;
static void *hists_stacktrace_buffer[MAX_SUM_OF_STACKTRACES];
static int hists_stacktrace_buffer_size = 0;

static int ncalls_malloc = 0;
static int ncalls_free = 0;
static int nbytes_malloced = 0;
static int nbytes_freed = 0;

static int ncalls_malloc_really = 0;	/* Not touched by reset */
static int ncalls_free_really = 0;	/* Not touched by reset */
static int nbytes_malloced_really = 0;	/* Not touched by reset */
static int nbytes_freed_really   = 0;	/* Not touched by reset */
static int max_diff_bytes	= 0;	/* Not touched by reset */

static void *malloc_block_of_interest = NULL;
static int malloc_call_of_interest = -1;
static int depth_of_trace_of_interest = -1;
static void *trace_of_interest[MAX_STACKTRACE_DEPTH];
static int malloc_size_of_interest = -1;
static int malloc_trace = 0;		/* If set, print every operation */

#define MALLOC_FILL '\001'
#define FREE_FILL   '\002'

static void
fdprintf(int fd, char *fmt, ...)
{
    char buf[MAXPATHLEN*2];
    va_list args;

    va_start(args, fmt);

    vsprintf(buf, fmt, args);
    write(fd, buf, strlen(buf));

    va_end(args);
}


/* external parameters user can mess with -- maybe should be mallopt option */
int malloc_stacktrace_get_depth = 0;	/* default is don't trace */
int malloc_fillarea = 1;		/* fill area by default */
extern void malloc_init_function();

static void
_atexit()
{
    int do_info = 0, do_free = 0, do_check = 1;	     /* note default values */
    char *p;

    if (p = getenv("MALLOC_INFO_ATEXIT"))
	do_info = (*p ? atoi(p) : 1);
    if (p = getenv("FREE_ATEXIT"))
	do_free = (*p ? atoi(p) : 1);
    if (p = getenv("MALLOC_CHECK_ATEXIT"))
	do_check = (*p ? atoi(p) : 1);

    if (do_info)
	malloc_info(0, -1);

#ifdef LINKED_LIST
    if (do_free) {
	if (do_free >= 2) {
	    fdprintf(2, "%s(%d): Freeing everything...",
			stacktrace_get_argv0() ? stacktrace_get_argv0() : "",
			getpid());
	}

	LOCK_MALLOC(1);
	while (firstheader)
	    free((void *)(firstheader+1));
	UNLOCK_MALLOC(1);

	if (do_free >= 2)
	    fdprintf(2, "done.\n");
    }

    if (do_check) {
	if (do_check >= 2) {
	    fdprintf(2, "%s(%d): Checking malloc chain (+%d amalloc arena%s) at exit...",
			stacktrace_get_argv0() ? stacktrace_get_argv0() : "",
			getpid(),
			narenas, narenas==1 ? "" : "s");
	}

	malloc_check_during("exit");

	if (do_check >= 2)
	    fdprintf(2, "done.\n");
    }
#endif /* LINKED_LIST */
}


/*
    From looking at the libc source:
	execl is weak symbol for _execl which calls _execv
	execle is weak symbol for _execle calls _execve
	execv is weak for _execv which calls _execve
	execve is weak for _execve

    So it looks like the simplest way to trap all of them
    is to redefine _execve.
    Also, to keep other modules from randomly
    latching on to the weak symbol execve in libc,
    we also define execve as a strong symbol here.
*/
#include <sys.s>   /* NOT syscall.h-- it gives wrong value for SYS_execve! */
extern int
_execve(const char *file, char *const*argv, char *const*envp)
{
    int do_check = 0;
    char *p;

    if (p = getenv("MALLOC_CHECK_ATEXEC"))
	do_check = (*p ? atoi(p) : 0);
#ifdef LINKED_LIST
    if (do_check) {

	if (do_check >= 2) {
	    /*
	     * Various levels of verbosity
	     * depending on the value of MALLOC_CHECK_ATEXEC...
	     *	>= 1 (default): do the check
	     *  >= 2: also print a message with the name of the executable
	     *  >= 3: print the args too
	     *  >= 4: quote the args
	     */
	    fdprintf(2, "%s(%d): Checking malloc chain (+%d amalloc arena%s) on execve(\"%s\"",
			stacktrace_get_argv0() ? stacktrace_get_argv0() : "",
			getpid(),
			narenas, narenas==1 ? "" : "s",
			file);
	    if (do_check >= 3) {
		int i;
		fdprintf(2, ", {");
		for (i = 0; argv[i] != NULL; ++i) {
		    fdprintf(2, do_check >= 4 ? "%s\"%s\"" : "%s%s",
			    i==0 ? "" : " ",
			    argv[i]);
		}
		fdprintf(2, "}");
	    }
	    fdprintf(2, ")...");
	}

	malloc_check_during("execve");

	if (do_check >= 2)
	    fdprintf(2, "done.\n");
    }
#endif /* LINKED_LIST */
    /* return real__execve(file, argv, envp); */
    /* argh... having trouble getting the real one
       because they don't supply libc.a any more...
       so fake it by calling syscall instead */
    return syscall(SYS_execve, file, argv, envp);
}

extern int
execve(const char *file, char *const*argv, char *const*envp)
{
    return _execve(file, argv, envp);
}


extern void *
acreate(void *addr, size_t len, int flags, void *ushdr, void *(*grow)(size_t, void *))
{
    void *ap = real_acreate(addr, len, flags, ushdr, grow);
    struct header **firstheader_addr =
	real_amalloc(sizeof(struct header *), ap);
    ulock_t arena_lock = THE_ARENA_LOCK(ap);
    assert(firstheader_addr == FIRSTHEADER_ADDR(ap));
    *firstheader_addr = NULL;
    arenas[narenas++] = ap;

    if (getenv("MALLOC_VERBOSE"))	/* XXX overhead */
	fdprintf(2, "%s(%d): acreate(addr=%#x, len=%#x, flags=%d, ushdr=%#x, grow=%#x) called, returning %#x, firstheader_addr = %#x, arena_lock = %#x\n",
	   stacktrace_get_argv0() ? stacktrace_get_argv0() : "", getpid(),
	   addr, len, flags, ushdr, grow, ap, firstheader_addr, arena_lock);
    return ap;
}

extern void
adelete(void *ap)
{
    fdprintf(2, "%s(%d): adelete(%#x) not implemented\n",
	   stacktrace_get_argv0() ? stacktrace_get_argv0() : "", getpid(),
	   ap);
}


#ifdef LINKED_LIST
extern int
amalloc_check_during(void *ap, char *during)
{
    int is_bad = 0;
    struct header *p;

    LOCK_MALLOC(1);
    LOCK_ARENA(ap);
    for (p = *FIRSTHEADER_ADDR(ap); p; p = p->next)
	if (!amalloc_isgoodblock_during(p+1, ap, during))
	    is_bad = 1;
    UNLOCK_ARENA(ap);
    UNLOCK_MALLOC(1);

    return is_bad ? -1 : 0;
}

extern int
malloc_check_during(char *during)
{
    int i;
    int return_value = amalloc_check_during(NULL, during);
    for (i = 0; i < narenas; ++i) {
	return_value |= amalloc_check_during(arenas[i], during);
    }
    return return_value;
}

extern int
malloc_check()
{
    return malloc_check_during("malloc_check");
}
#endif /* LINKED_LIST */

static int malloc_reset_signal = 0;
static int malloc_info_signal = 0;
static int malloc_check_signal = 0;
static int malloc_trace_signal = 0;

static void catch(int sig)
{
    if (lock_depth > 0) {
	fdprintf(2, "%s(%d): In a malloc function already; try again in a moment.\n",
		 stacktrace_get_argv0() ? stacktrace_get_argv0() : "",
		 getpid());
	return;
    }

    if (sig == malloc_check_signal) {
	fdprintf(2, "%s(%d): Checking malloc chain (+%d amalloc arena%s)...",
			stacktrace_get_argv0() ? stacktrace_get_argv0() : "",
			getpid(),
			narenas, narenas==1 ? "" : "s");

	malloc_check();
	fdprintf(2, "done.\n");
    }

    if (sig == malloc_info_signal) {
	malloc_check();
	malloc_info(1, -1); /* XXX for now, always print nonleaks too */
    }
    if (sig == malloc_reset_signal) {
	malloc_check();
	malloc_reset();
    }
    if (sig == malloc_trace_signal) {
	malloc_check();
	malloc_trace = !malloc_trace;
	fdprintf(2, "%s(%d): Setting malloc_trace to %d\n",
		 stacktrace_get_argv0() ? stacktrace_get_argv0() : "", getpid(),
		 malloc_trace);
    }
}

/* to be called only from _malloc_init() */
static void
__malloc_init()
{
    static int called_already = 0;
    if (!called_already) {
	char *p;

	/* try to snarf argv[0] as soon as possible, in case main clobbers it */
	(void)stacktrace_get_argv0();

	if (p = getenv("MALLOC_FILLAREA"))
	    malloc_fillarea = (*p ? atoi(p) : 1);
	if (p = getenv("MALLOC_STACKTRACE_GET_DEPTH"))
	    malloc_stacktrace_get_depth = (*p ? atoi(p) : 0);

	if (p = getenv("MALLOC_RESET_SIGNAL"))
	    malloc_reset_signal = (*p ? atoi(p) : SIGUSR1);
	if (p = getenv("MALLOC_INFO_SIGNAL"))
	    malloc_info_signal = (*p ? atoi(p) : SIGUSR2);
	if (p = getenv("MALLOC_CHECK_SIGNAL"))
	    malloc_check_signal = (*p ? atoi(p) : SIGUSR2);
	if (p = getenv("MALLOC_TRACE_SIGNAL"))
	    malloc_trace_signal = (*p ? atoi(p) : SIGUSR1);
	if (malloc_reset_signal != 0)
	    sigset(malloc_reset_signal, catch);
	if (malloc_info_signal != 0)
	    sigset(malloc_info_signal, catch);
	if (malloc_check_signal != 0)
	    sigset(malloc_check_signal, catch);
	if (malloc_trace_signal != 0)
	    sigset(malloc_trace_signal, catch);

	/* XXX it might be useful to make a nice callback interface... */
	/* XXX and definitely want a gui */
	if (p = getenv("MALLOC_STACKTRACE_OF_INTEREST")) {
	    depth_of_trace_of_interest =
		sscanf(p, "%x %x %x %x %x %x %x %x %x %x",
			&trace_of_interest[0],
			&trace_of_interest[1],
			&trace_of_interest[2],
			&trace_of_interest[3],
			&trace_of_interest[4],
			&trace_of_interest[5],
			&trace_of_interest[6],
			&trace_of_interest[7],
			&trace_of_interest[8],
			&trace_of_interest[9]);
	    if (depth_of_trace_of_interest == -1)
		depth_of_trace_of_interest = 0;	/* dumbass sscanf */
	}
	/*
	 * Note: we must have some code in this file that
	 * sets the value of malloc_call_of_interest,
	 * otherwise we will not be able to set it properly in the debugger
	 * (due to compiler optimization or something).
	 */
	/* XXX want a gui for this */
	if (p = getenv("MALLOC_CALL_OF_INTEREST"))
	    malloc_call_of_interest = (int) strtoul(p, (char **)NULL, 0);
	if (p = getenv("MALLOC_BLOCK_OF_INTEREST"))
	    malloc_block_of_interest = (void *)strtoul(p, (char **)NULL, 0);
	if (p = getenv("MALLOC_SIZE_OF_INTEREST"))
	    malloc_size_of_interest = (int)strtoul(p, (char **)NULL, 0);
	if (p = getenv("MALLOC_TRACE"))
	    malloc_trace = (*p ? (int) strtoul(p, (char **)NULL, 0) : 1);

	if (p = getenv("MALLOC_PROMPT_ON_STARTUP")) {
	    if (!*p || streq(p, stacktrace_get_argv0())) {
		int tty = open("/dev/tty", 2);
		char c;
		assert(tty >= 0);
		fdprintf(tty, "%s(%d): hit return to continue",
				  stacktrace_get_argv0(), getpid());
		read(tty, &c, 1);
		close(tty);
	    }
	}

	malloc_init_function();		/* call application-defined function */

	atexit(_atexit);

	called_already = 1;
	/*
	called_already must be true at this point,
	so that this block won't get re-entered
	if the us_ routines call malloc.
	*/
	CREATE_MALLOC_LOCK();
    }
}

/* this should be static, but we want to make it visible
   to the linker so it can be made an init function... */
extern void
_malloc_init()
{
    static int called_already = 0;
    if (!called_already) {
	char *p;
	if (p = getenv("MALLOC_STACKTRASH")) {
	    int length = 4096;
	    char val = '\003';
	    extern void *_stacktrace_get_sp();
	    char *sp = _stacktrace_get_sp();
	    if (*p)
		length = (int) strtoul(p, (char **)NULL, 0);
	    if (p = strchr(p, ','))
		val = (char) strtoul(p+1, (char **)NULL, 0);
	    while (length-- >= 0)
		*--sp = val;
	}
	/*
	 * Could just put the body of __malloc_init here,
	 * but we want to make this function as simple as possible
	 * so that sp will be as high as possible...
	 */
	called_already = 1;
	__malloc_init();
    }
}


#define MALLOC_STACKTRACE_GET_DEPTH (malloc_stacktrace_get_depth == -1 ? MAX_STACKTRACE_DEPTH : MIN(malloc_stacktrace_get_depth, MAX_STACKTRACE_DEPTH))

/* only compares up to the min of the two sizes */
static int
stacktracecmp(int siz0, void *trace0[], int siz1, void *trace1[])
{
	int i;
	for (i = 0; i < siz0 && i < siz1; ++i)
	    if (((unsigned long *)trace0)[i] != ((unsigned long *)trace1)[i]) {
		if (((unsigned long *)trace0)[i] < ((unsigned long *)trace1)[i])
		    return -1;
		else
		    return 1;
	    }
	return siz0 - siz1;
}

static int
stacktracencmp(int siz0, void *trace0[], int siz1, void *trace1[], int n)
{
	int i;
	for (i = 0; i < siz0 && i < siz1 && i < n; ++i)
	    if (((unsigned long *)trace0)[i] != ((unsigned long *)trace1)[i]) {
		if (((unsigned long *)trace0)[i] < ((unsigned long *)trace1)[i])
		    return -1;
		else
		    return 1;
	    }
	if (i == n)
	    return 0;
	return siz0 - siz1;
}

/* simple prime testing-- we only need it to initialize hashsiz */
static int 
isprime(int n)
{
    int i;
    if (n % 2 == 0)
	return 0;
    for (i = 3; i*i <= n; ++i)
	if (n % i == 0)
	    return 0;
    return 1;
}

static int
hashfun(int depth, void *trace[], int hashsiz)
{
    int i; /* not unsigned! or (i < depth) will be true when i==0 and depth<0 */
    unsigned long sum = 0;
    for (i = 0; i < depth; ++i)
	sum += (((unsigned long)trace[i])/4);
    return sum % (hashsiz-1) + 1;	/* in range 1..hashsiz-1 */
}

#ifdef HASH_STATS
    static int n_collisions = 0;
    static int n_distinct_collisions = 0;
    static int n_pileups;
    static int n_distinct_pileups;
    static int max_pileup = 0;
    extern void
    _malloc_history_print_stats()
    {
	printf("%d max pileup\n", max_pileup);
	printf("%d distinct pileups\n", n_distinct_pileups);
	printf("%d total pileups\n", n_pileups);
	printf("%d distinct collisions\n", n_distinct_collisions);
	printf("%d total collisions\n", n_collisions);
    }
#endif

static struct hist *
find_existing_hist(void *type, int stacktrace_depth, void *stacktrace[])
{
#ifndef HASHSIZ
#define HASHSIZ (MAX_MALLOCS * 3/2)
#endif
    static struct hist *hashtable[HASHSIZ];
    static int hashsiz = 0;
    int i, hash;
#ifdef HASH_STATS
    int pileup = 0;
#endif

    /*
     * Make the size of the hash table prime so that
     * for every hash, 0 < hash < hashsiz, the sequence
     *		hash, 2*hash, 3*hash, ... (mod hashsiz)
     * is a unique path through 1..hashsize-1 (this allows a simple
     * rehashing mechanism that avoids pileups).
     * Note that 0 is not allowed-- make sure hashfun() can not return 0!
     * (hashtable[0] is not used, but subtracting 1 from everything is
     * not worth the trouble).
     */
    if (!hashsiz)	/* then hashsiz hasn't been initialized yet */
	for (hashsiz = HASHSIZ; !isprime(hashsiz); hashsiz--)
	    ;

    hash = hashfun(stacktrace_depth, stacktrace, hashsiz);
    for (i = hash; hashtable[i];
#ifdef HASH_STUPID
				 i++
#else
				 i = (i+hash) % hashsiz
#endif
						       ) {
	if (type == hashtable[i]->type
	 && !stacktracecmp(stacktrace_depth, stacktrace,
			   hashtable[i]->stacktrace_depth,
			   hashtable[i]->stacktrace)) {
	    return hashtable[i];
	}
#ifdef HASH_STATS
	pileup++;
	n_collisions++;
	n_pileups += (pileup == 1);
#endif
    }
    if (nhists == MAX_MALLOCS)
	return NULL;

#ifdef HASH_STATS
    n_distinct_collisions += pileup;
    n_distinct_pileups += (pileup > 0);
    max_pileup = MAX(pileup, max_pileup);
#endif

    hashtable[i] = &hists[nhists];
    return hashtable[i];
}

/* just places for the debugger to stop... */
extern void
malloc_of_interest()
{
    printf("");		/* make sure this function doesn't get optimized out */
}

static struct hist *
findhist(void *type, int stacktrace_depth, void *stacktrace[])
{
    int i;
    struct hist *h;

    h = find_existing_hist(type, stacktrace_depth, stacktrace);

    if (h == NULL) {
	static int already_complained = 0;
	if (!already_complained) {
	    fdprintf(2, "Turning off malloc tracing: more than %d distinct mallocs & frees\n", MAX_MALLOCS);
	    already_complained = 1;
	}
	return NULL;
    }

    if (depth_of_trace_of_interest >= 0
     && !stacktracecmp(stacktrace_depth, stacktrace,
		depth_of_trace_of_interest, trace_of_interest)) {
	malloc_of_interest();
	/* make each one separate XXXthis should go with other bounds checking*/
	if (nhists < MAX_MALLOCS) {
	    h = hists+nhists;
	}
    }

    if (h-hists == nhists) {	/* then it wasn't really existing */
	nhists++;

	h->type   = type;
	h->ncalls  = 0;
	h->nbytes  = 0;
	h->ncalls_freed = 0;
	h->nbytes_freed = 0;

	/* XXX need to do some bounds checking here */
	h->stacktrace_depth = MIN(stacktrace_depth, MAX_STACKTRACE_DEPTH);
	h->stacktrace = hists_stacktrace_buffer + hists_stacktrace_buffer_size;
	hists_stacktrace_buffer_size += h->stacktrace_depth;
	for (i = 0; i < h->stacktrace_depth; ++i)
	    h->stacktrace[i] = stacktrace[i];
    }

    return h;
}

/*
 * Stuff to do on each malloc or realloc
 */
static void
malloc_do_stuff(size_t n, struct header *head, void *ap)
{
    struct hist *h;
    int stacktrace_depth;
    void *stacktrace[MAX_STACKTRACE_DEPTH];

    if (malloc_block_of_interest == (void *)(head+1))
	malloc_of_interest();
    if (malloc_call_of_interest == ncalls_malloc)
	malloc_of_interest();
    if (malloc_size_of_interest == n)
	malloc_of_interest();

    if (!lowest_head_ever || head < lowest_head_ever)
	lowest_head_ever = head;
    if (!highest_head_ever || head > highest_head_ever)
	highest_head_ever = head;

    ncalls_malloc++;
    ncalls_malloc_really++;
    nbytes_malloced += n;
    nbytes_malloced_really += n;

    if (nbytes_malloced_really - nbytes_freed_really > max_diff_bytes)
	max_diff_bytes = nbytes_malloced_really - nbytes_freed_really;

    /* don't get any traces in recursive mallocs */
    if (lock_depth >= 200)
	stacktrace_depth = 0;
    else
	stacktrace_depth = stacktrace_get(2, MALLOC_STACKTRACE_GET_DEPTH,
					     stacktrace);

    h = findhist(type_MALLOC, stacktrace_depth, stacktrace);
    if (h) {
	h->ncalls++;
	h->nbytes += n;
    }

    head->n = n;
    head->hist = h;
    head->ap = ap;

#ifdef LINKED_LIST
    link_header(head, ap);
#endif
}

static int
strcontains(char *S, char *s)	/* XXX remove this when no longer used */
{
    if (!S || !s)
	return 0;
    for (; *S; S++)
	if (!strncmp(S, s, strlen(s)))
	    return 1;
    return 0;
}

/*
    Are we suppressing messages about this block
    because of an environment variable?
*/
static int
suppressing(void *block)
{
    char *p = getenv("MALLOC_SUPPRESS");
    if (!p)
	return 0;

    while (p && *p) {
	char argv0_to_suppress[4096];
	long block_to_suppress = 0;
	while (*p && isspace(*p))
	    p++;
	if (sscanf(p, "%[^:]:%lx", argv0_to_suppress, &block_to_suppress) != 2){
	    argv0_to_suppress[0] = '\0';
	    if (sscanf(p, "%lx", &block_to_suppress) != 1)
		break;
	}

	if (streq(argv0_to_suppress, stacktrace_get_argv0())
	 && block_to_suppress == (long)block)
	    return 1;	/* found it; suppress it */
	while (*p && !isspace(*p))
	    p++;
	while (*p && isspace(*p))
	    p++;
    }
    return 0;	/* not found in suppress list; don't suppress */
}

extern void	/* can stop here for breakpoint debugging */
malloc_bad(void *block)
{
    assert(!"nsd bad malloc/free assert.  Notify mende@piecomputer.engr.sgi.com");
    assert(!getenv("MALLOC_BAD_ASSERT"));
}

extern int
amalloc_isgoodblock_during(void *block, void *ap, char *during)/* XXX think of a better name */
{
    int its_bad = 0;
    struct header *head = ((struct header *)block) - 1;
    struct tailer *tail;
    int nwrong_in_head, nwrong_in_tail;
    char *p;

    /*
     * Sanity check so we don't buserror ourself
     */
    if (((long)head & 3L) ||
	(ap == NULL && (head < lowest_head_ever || head > highest_head_ever))) {
	if (!suppressing(head+1))
	    fdprintf(2, "%s(%d): ERROR: %#x is not a valid malloced block,\n\tdetected during %s\n",
		    stacktrace_get_argv0() ? stacktrace_get_argv0() : "",
		    getpid(),
		    head+1, during);
	malloc_bad(block);
	return 0; /* failure */
    }

    /*
     * Heavy duty bounds checking
     */
    nwrong_in_head = (head->magic0 != aheader.magic0)
		   + (head->magic1 != aheader.magic1)
		   + (head->magic2 != aheader.magic2)
		   + (head->magic3 != aheader.magic3);
    if (nwrong_in_head > 1) {
	if (!suppressing(head+1))
	    fdprintf(2, "%s(%d): ERROR: %#x is not a valid malloced block (or more than 4 bytes of underflow corruption),\n\tdetected during %s\n",
		    stacktrace_get_argv0() ? stacktrace_get_argv0() : "",
		    getpid(),
		    head+1, during);
	malloc_bad(block);
	return 0; /* failure */
    }

    if (nwrong_in_head > 0) {
	if (!suppressing(head+1))
	    fdprintf(2,
		"%s(%d): ERROR: underflow corruption detected during %s\n\tat malloc block %#x\n",
		    stacktrace_get_argv0() ? stacktrace_get_argv0() : "",
		    getpid(),
		    during,
		    head+1);
	its_bad = 1;
    }
    tail = head->tail;
    nwrong_in_tail = (tail->magic4 != atailer.magic4)
		   + (tail->magic5 != atailer.magic5)
		   + (tail->head != head)
		   + (tail->magic6 != atailer.magic6)
		   + (tail->magic7 != atailer.magic7);
    /* check that the < 4 bytes of extra alignment space are
       still filled with MALLOC_FILL */
    for (p = (char *)(head+1) + head->n; p < (char *)tail; ++p)
	if (*p != MALLOC_FILL)
	    nwrong_in_tail++;

    if (nwrong_in_tail > 0) {
	if (!suppressing(head+1))
	    fdprintf(2, "%s(%d): ERROR: overflow corruption detected during %s\n\tat malloc block %#x (%s%d byte%s)\n",
		    stacktrace_get_argv0() ? stacktrace_get_argv0() : "",
		    getpid(),
		    during,
		    head+1,
		    nwrong_in_head ? "maybe " : "",
		    head->n,
		    head->n==1 ? "" : "s");
	its_bad = 1;
    }

    if (nwrong_in_head == 0 && head->ap != ap) {
	if (!suppressing(head+1))
	    fdprintf(2,
		"%s(%d): ERROR: malloc block %#x (%d byte%s) is from arena %#x, not %#x,\n\tdetected during %s\n",
			stacktrace_get_argv0() ? stacktrace_get_argv0() : "",
			getpid(),
			head+1,
			head->n,
			head->n==1 ? "" : "s",
			head->ap, ap,
			during);
	its_bad = 1;
    }

    if (!nwrong_in_head && !nwrong_in_tail && ((long)head->hist & 1)) {
    /*
     * When we free the block, we set the loworder bit of hist.
     * Of course the real free and malloc may clobber it,
     * but if that happened, the above never would have passed.
     */
	if (!suppressing(head+1))
	    fdprintf(2, "%s(%d): ERROR: %#x (%d byte%s) has been freed already,\n\tdetected during %s\n",
			stacktrace_get_argv0() ? stacktrace_get_argv0() : "",
			getpid(),
			head+1,
			head->n,
			head->n==1 ? "" : "s",
			during);
	its_bad = 1;
    }

    if (its_bad) {
	char buf[100];
	/* snarf the stacktrace away; since the block has been freed
	   already, our printing may clobber it */
	int stacktrace_depth;
	void *stacktrace[MAX_STACKTRACE_DEPTH];
	stacktrace_depth = ((struct hist *)((long)head->hist&~1))->stacktrace_depth;
	bcopy(((struct hist *)((long)head->hist&~1))->stacktrace, stacktrace,
		stacktrace_depth * sizeof(*stacktrace));

	malloc_bad(block);

	/*
	    XXX Quick semi-accurate attempt to see whether we are in a DSO--,
	    since we will probably core dump if we are and the main
	    program is stripped and we try to get a stack trace.
	*/
	if (!suppressing(head+1) &&
	   (strcontains(getenv("_RLD_LIST"), "libdmalloc")
		     && getenv("_MALLOC_TRY_TO_PRINT_STACKTRACES")
	    ||!strcontains(getenv("_RLD_LIST"), "libdmalloc")
		     && !getenv("_MALLOC_DONT_TRY_TO_PRINT_STACKTRACES"))) {

	    simple_stacktrace_print(/*fd*/2, NULL, /*skip*/3, 100);
	}

	if (!suppressing(head+1) &&
	   (strcontains(getenv("_RLD_LIST"), "libdmalloc")
		     && getenv("_MALLOC_TRY_TO_PRINT_STACKTRACES")
	    ||!strcontains(getenv("_RLD_LIST"), "libdmalloc")
		     && !getenv("_MALLOC_DONT_TRY_TO_PRINT_STACKTRACES"))) {
	    fdprintf(2, "This block may have been allocated here:\n");
	    simple_stacktrace_write(/*fd*/2, /*fmt*/NULL, /*filename*/NULL,
			stacktrace_depth, stacktrace);
	}

	return 0;	/* failure */
    }

    return 1;	/* success */
}

extern int
malloc_isgoodblock_during(void *block, char *during)/* XXX think of a better name */
{
    return amalloc_isgoodblock_during(block, NULL, during);
}

extern int
amalloc_isgoodblock(void *block, void *ap) /* XXX think of a better name */
{
    return amalloc_isgoodblock_during(block, ap, "amalloc_isgoodblock");
}

extern int
malloc_isgoodblock(void *block) /* XXX think of a better name */
{
    return malloc_isgoodblock_during(block, "malloc_isgoodblock");
}


/*
 * Stuff to do on each free or realloc.
 * Return 1 on success, 0 if corruption was detected.
 */
static int
free_do_stuff(struct header *head, void *ap, char *during)
{
    struct hist *h;
    int stacktrace_depth;
    void *stacktrace[MAX_STACKTRACE_DEPTH];

    if (!amalloc_isgoodblock_during(head+1, ap, during))
	return 0;	/* failure */

    ncalls_free++;
    ncalls_free_really++;
    nbytes_freed       += head->n;
    nbytes_freed_really += head->n;

    if (lock_depth >= 200)
	stacktrace_depth = 0;
    else
	stacktrace_depth = stacktrace_get(2, MALLOC_STACKTRACE_GET_DEPTH,
					     stacktrace);

    h = findhist(type_FREE, stacktrace_depth, stacktrace);
    if (h) {
	h->ncalls++;
	h->nbytes += head->n;
    }

    if (head->hist) {
	head->hist->ncalls_freed++;
	head->hist->nbytes_freed += head->n;
    }

#ifdef LINKED_LIST
    unlink_header(head, ap);
#endif

    return 1;	/* success */
}

/*
   Alignment necessary for our head or tail structure is 4.
   (This is not the same as the alignment we have to
   return from malloc).
*/
#define ALIGNUP(n) (((n)+3)&~3)

extern void
malloc_failed()
{
    /* this function exists for breakpoint debugging. */
    printf("");		/* make sure this function doesn't get optimized out */
    /* XXX maybe should allow a callback here? */
}

/* common code for malloc and amalloc... */
static void *
a_malloc(size_t n, void *ap, size_t alignment)
{
    void *real_mem;
    struct header *head;

    assert((alignment&(alignment-1))==0);
    alignment = MAX(alignment, 8);

    _malloc_init();

    LOCK_MALLOC(100);

    head = 0;	/* XXX suppress stupid compiler warning */
    if (ap != NULL)
	real_mem = (struct header *) real_amalloc(ALIGNUP(n + sizeof(*head))
					    + sizeof(struct tailer)
					    + alignment-8,
					    ap);
    else
	real_mem = (struct header *) real_malloc(ALIGNUP(n + sizeof(*head))
					    + sizeof(struct tailer)
					    + alignment-8);

    if (!real_mem) {
	/* XXX wrong message when it's only malloc! */
	fdprintf(2, "%s(%d): amalloc(%d, 0x%p) returning NULL\n",
		 stacktrace_get_argv0() ? stacktrace_get_argv0() : "", getpid(),
		 (int)n, ap);
	malloc_failed();
	malloc_check(); /* maybe it was corruption that caused it */
	UNLOCK_MALLOC(100);
	return NULL;
    }
    head = (struct header *)((char *)((unsigned long)(((char *)real_mem+sizeof(*head)) + alignment-1) &~ (unsigned long)(alignment-1))
	   - sizeof(*head));

    *head = aheader;
    head->tail = (struct tailer *)(((char *)head) + ALIGNUP(n + sizeof(*head)));
    *head->tail = atailer;
    head->tail->head = head;
    head->tail->real_mem = real_mem;

    malloc_do_stuff(n, head, ap);

    if (malloc_fillarea)
	memset((void *)(head+1), MALLOC_FILL, n);

    /* always fill any space after the buffer and before the tail,
       for overflow detection */
    memset((void *)((char *)(head+1)+n),
	   MALLOC_FILL,
	   (char *)head->tail - ((char *)(head+1)+n));

    UNLOCK_MALLOC(100);
    return (void *)(head+1);
}

/*
 * Front end to the real amalloc
 */
extern void *
amalloc(size_t n, void *ap)
{
    void *ret = a_malloc(n, ap, 1);
    if (malloc_trace)
	fdprintf(2, "%s(%d): amalloc(%d, 0x%p) returning 0x%p\n",
		 stacktrace_get_argv0() ? stacktrace_get_argv0() : "", getpid(),
		 n, ap, ret);
    return ret;
}

/*
 * Front end to the real malloc
 */
extern void *
malloc(size_t n)
{
    void *ret = a_malloc(n, NULL, 1);
    if (malloc_trace)
	fdprintf(2, "%s(%d): malloc(%d) returning 0x%p\n",
		 stacktrace_get_argv0() ? stacktrace_get_argv0() : "", getpid(),
		 n, ret);
    return ret;
}

/* common code for free and afree... */
static void
a_free(void *p, void *ap)
{
    struct header *vom;

    _malloc_init();

    LOCK_MALLOC(100);

    if (!p) {
	UNLOCK_MALLOC(100);
	return;		/* XXX do we want to save this information? */
    }
    vom = ((struct header *)p) - 1;

    if (!free_do_stuff(vom, ap, ap ? "afree" : "free")) {
	UNLOCK_MALLOC(100);
	return;			/* free_do_stuff prints its own error message */
    }

    if (malloc_fillarea)
	memset((void *)(vom+1), FREE_FILL, vom->n);

    vom->hist = (struct hist *)((long)vom->hist | 1);

    if (ap != NULL)
	real_afree(vom->tail->real_mem, ap);
    else
	real_free(vom->tail->real_mem);

    UNLOCK_MALLOC(100);
}

/*
 * Front end to the real afree
 */
extern void
afree(void *p, void *ap)
{
    if (malloc_trace)
	fdprintf(2, "%s(%d): afree(0x%p, 0x%p) (%d bytes)\n",
		 stacktrace_get_argv0() ? stacktrace_get_argv0() : "", getpid(),
		 p, ap, amallocblksize(p, ap));
    a_free(p, ap);
}

/*
 * Front end to the real free
 */
extern void
free(void *p)
{
    if (malloc_trace)
	fdprintf(2, "%s(%d): free(0x%p) (%d bytes)\n",
		 stacktrace_get_argv0() ? stacktrace_get_argv0() : "", getpid(),
		 p, mallocblksize(p));
    a_free(p, NULL);
}

/* common code for realloc and arealloc... */
static void *
a_realloc(void *p, size_t n, void *ap)
{
    struct header *head;
    void *old_real_mem, *real_mem;
    int old_n;

    if (!p)
	return a_malloc(n, ap, 1);	/* this is what the sgi man page says, anyway */

    _malloc_init();

    LOCK_MALLOC(100);

    head = ((struct header *)p) - 1;

    if (!free_do_stuff(head, ap, ap ? "arealloc" : "realloc")) {
	UNLOCK_MALLOC(100);
	return NULL;		/* free_do_stuff prints its own error message */
    }

    old_n = head->n;
    old_real_mem = head->tail->real_mem;

    if (malloc_fillarea && n < old_n)
	memset((void *)((char *)(head+1) + n), FREE_FILL, old_n - n);

    if ((void *)head != old_real_mem) {
	fdprintf(2, "%s(%d): WARNING: (a)realloc(0x%p, %d, 0x%p) (was %d bytes) but original memory was memaligned!  Alignment unknown, assuming 8\n",
		 stacktrace_get_argv0() ? stacktrace_get_argv0() : "", getpid(),
		 p, (int)n, ap, old_n);
    }

    /* XXX will not preserve alignment if original was memaligned, */
    /* XXX but will waste as much space... */
    if (ap != NULL)
	real_mem = (struct header *) real_arealloc(old_real_mem,
			    ALIGNUP(n + sizeof(*head)) + sizeof(struct tailer)
			    + ((char *)head - (char *)old_real_mem),
			    ap);
    else
	real_mem = (struct header *) real_realloc(old_real_mem,
			    ALIGNUP(n + sizeof(*head)) + sizeof(struct tailer)
			    + ((char *)head - (char *)old_real_mem));

    if (!real_mem) {
	/* XXX wrong message when it's only realloc! */
	fdprintf(2, "%s(%d): arealloc(0x%p, %d, 0x%p) (was %d bytes) returning returning NULL\n",
		 stacktrace_get_argv0() ? stacktrace_get_argv0() : "", getpid(),
		 p, (int)n, ap, old_n);
	malloc_failed();
	malloc_check(); /* maybe it was corruption that caused it */
	UNLOCK_MALLOC(100);
	return NULL;		/* XXX do we want to save this information? */
    }

    head = (struct header *)((char *)real_mem + ((char *)head - (char *)old_real_mem));

    head->tail = (struct tailer *)(((char *)head) + ALIGNUP(n+sizeof(*head)));
    *head->tail = atailer;
    head->tail->head = head;
    head->tail->real_mem = head;

    malloc_do_stuff(n, head, ap);

    if (malloc_fillarea && n > old_n)
	memset((void *)((char *)(head+1) + old_n), MALLOC_FILL, n - old_n);

    /* always fill any space after the buffer and before the tail,
       for overflow detection */
    memset((void *)((char *)(head+1)+n),
	   MALLOC_FILL,
	   (char *)head->tail - ((char *)(head+1)+n));

    UNLOCK_MALLOC(100);
    return (void *)(head + 1);
}

/*
 * Front end to the real arealloc
 */
extern void *
arealloc(void *p, size_t n, void *ap)
{
    int oldsiz = p ? amallocblksize(p, ap) : 0;
    void *ret = a_realloc(p, n, ap);
    if (malloc_trace)
	fdprintf(2, "%s(%d): arealloc(0x%p, %d, 0x%p) (was %d bytes) returning 0x%p\n",
		 stacktrace_get_argv0() ? stacktrace_get_argv0() : "", getpid(),
		 p, (int)n, ap, oldsiz, ret);
    return ret;
}

/*
 * Front end to the real realloc
 */
extern void *
realloc(void *p, size_t n)
{
    int oldsiz = p ? mallocblksize(p) : 0;
    void *ret = a_realloc(p, n, NULL);
    if (malloc_trace)
	fdprintf(2, "%s(%d): realloc(0x%p, %d) (was %d bytes) returning 0x%p\n",
		 stacktrace_get_argv0() ? stacktrace_get_argv0() : "", getpid(),
		 p, (int)n, oldsiz, ret);
    return ret;
}


/*
 * Sorting utilities
 */
static void
multiqsort(char *base, int n, int elsiz, int (*cmps[])(const void *, const void *))
/* cmps is a null-terminated array of comparison functions */
{
    int i, (*cmp)(const void *, const void *);

    if (!(cmp = cmps[0]))
	return;

    qsort(base, n, elsiz, cmp);

    if (cmps[1])
	for (; n > 0; n -= i, base += i*elsiz) {
	    for (i = 0; i < n; ++i)
		if ((*cmp)(base, base + i*elsiz))
		    break;
	    if (i > 1)
		multiqsort(base, i, elsiz, cmps+1);
	}
}

static int cmp_leaks(struct hist **a, struct hist **b)
{
    return ((*b)->nbytes - (*b)->nbytes_freed)
	 - ((*a)->nbytes - (*a)->nbytes_freed);		/* highest first */
}

static int cmp_ncalls(struct hist **a, struct hist **b)
{
    return (*b)->ncalls - (*a)->ncalls;			/* highest first */
}

static int cmp_stacktrace(struct hist **a, struct hist **b)
{
    return stacktracecmp((*a)->stacktrace_depth, (*a)->stacktrace, (*b)->stacktrace_depth, (*b)->stacktrace);
}

#if 0	/* maybe someday we'll do this by filename */
static int cmp_lineno(struct hist *a, struct hist *b)
{
    return (*a)->lineno - (*b)->lineno;
}

static int cmp_filename(struct hist **a, struct hist **b)
{
    if (!(*a)->file || !(*b)->file)
	return !(*a)->file - !(*b)->file;	/* with filename comes before without filename */
    return strcmp((*a)->file, (*b)->file);
}

static int cmp_diffbytes(struct hist **a, struct hist **b)
{
    return ((*a)->bytes - (*a)->fbytes) - ((*b)->bytes - (*b)->fbytes);
}
#endif /* 0 */

static int cmp_malloc_then_free(struct hist **a, struct hist **b)
{
    return ((*a)->type == type_FREE) - ((*b)->type == type_FREE);
}

int (*oldcmps[])(const void *, const void *) = {
    /* cmp_filename, */
    /* cmp_lineno, */
    (int (*)(const void *, const void *))cmp_malloc_then_free,
    (int (*)(const void *, const void *))cmp_stacktrace,
    NULL
};
int (*cmps[])(const void *, const void *) = {
    /* cmp_filename, */
    /* cmp_lineno, */
    (int (*)(const void *, const void *))cmp_malloc_then_free,
    (int (*)(const void *, const void *))cmp_leaks,
    (int (*)(const void *, const void *))cmp_ncalls,
    (int (*)(const void *, const void *))cmp_stacktrace,
    NULL
};

extern void
malloc_reset()
{
    int i;
    /* can't set nhists to 0, since existing header points to them */
    for (i = 0; i < nhists; ++i) {
	hists[i].ncalls = 0;
	hists[i].nbytes = 0;
	hists[i].ncalls_freed = 0;
	hists[i].nbytes_freed = 0;
    }
    ncalls_malloc = 0;
    ncalls_free = 0;
    nbytes_malloced = 0;
    nbytes_freed = 0;
}

extern void
malloc_info_cleanup()
{
    stacktrace_cleanup();
}

/*
    verbose = 0: just totals
    verbose = 1: leaks with traces
    verbose = 2: all counts with traces
*/
extern void
malloc_info(int nonleaks_too,
	    int stacktrace_print_depth)
{
	int orig_nhists;
	int i;
	struct hist *ptrs[MAX_MALLOCS], *h;
	char thisfile[SYMBUFSIZ];
	char thisfunc[SYMBUFSIZ];
	char avgbuf[30];
	int thisline;

	_malloc_init();

	LOCK_MALLOC(1);

	printf("%s(%d):\n",
	       stacktrace_get_argv0() ? stacktrace_get_argv0() : "", getpid());
	printf("\n");
	printf("%6s %10s %10s %10s %10s\n",
		" ",
		"Mallocs",
		"Frees",
		"Diff",
		"Max Diff");
	printf("-------------------------------------------------------------\n");
	printf("%6s %10d %10d %10d %10d\n",
		"Bytes:",
		nbytes_malloced_really,
		nbytes_freed_really,
		nbytes_malloced_really-nbytes_freed_really,
		max_diff_bytes);
	printf("%6s %10d %10d %10d\n",
		"Calls:",
		ncalls_malloc_really,
		ncalls_free_really,
		ncalls_malloc_really-ncalls_free_really);
	printf("Since last reset:\n");
	printf("%6s %10d %10d %10d\n",
		"Bytes:",
		nbytes_malloced,
		nbytes_freed,
		nbytes_malloced-nbytes_freed);
	printf("%6s %10d %10d %10d\n",
		"Calls:",
		ncalls_malloc,
		ncalls_free,
		ncalls_malloc-ncalls_free);

	/*
	if (!verbose) {
		UNLOCK_MALLOC(1);
		return;
	}
	*/
	printf("%d out of %d traces used\n", nhists, MAX_MALLOCS);
#ifdef HASH_STATS
	_malloc_history_print_stats();
#endif

	printf("\n");

	/* printf("%15s[%4s]: %3s %6s %9s %7s %9s %7s %9s\n", */
	printf("%15s[%4s]: %3s %6s %7s %7s%6s %7s %5s %9s\n",
		"Filename",
		"Line",
		"Wha",
		"Calls",
		"Bytes",
		"Avg ",
		"FCalls",
		"FBytes",
		"Diff",
		"DiffBytes");
	printf("-------------------------------------------------------------------------------\n");

	orig_nhists = nhists;	/* printing and the first stacktracing call malloc */
	for (i = 0; i < orig_nhists; ++i)
	    ptrs[i] = hists+i;
	if (getenv("_MALLOC_DONT_SORT_BY_NCALLS"))
	    multiqsort((char *)ptrs, orig_nhists,sizeof(struct hist *),oldcmps);
	else
	    multiqsort((char *)ptrs, orig_nhists, sizeof(struct hist *), cmps);

	for (i=0; i<orig_nhists; i++) {

		h = ptrs[i];

		if (h->ncalls == 0 &&
		    (h->type==type_FREE || h->ncalls_freed==0))
			continue;

		if (!nonleaks_too) {
		    if (h->type == type_FREE)
			continue;
		    if (h->ncalls == h->ncalls_freed
		      && h->nbytes == h->nbytes_freed)
			continue;
		}

		thisfunc[0] = 0;
		thisfile[0] = 0;
		thisline = -1;
		if (h->stacktrace_depth >= 1) {
		    stacktrace_get_ffl(h->stacktrace[0],
			thisfunc, thisfile, &thisline,
			SYMBUFSIZ-2, SYMBUFSIZ-2);
		}

		/* XXX should keep track of whether all sizes are the same */
		if (h->ncalls == 0)
		    sprintf(avgbuf, " ");
		else if (h->nbytes % h->ncalls != 0)
		    sprintf(avgbuf, "%d.", h->nbytes / h->ncalls);
		else
		    sprintf(avgbuf, "%d ", h->nbytes / h->ncalls);

		if (h->type == type_MALLOC)

			/* printf("%15s[%4d]: %3s %6d %9d %7d %9d %7d %9d\n", */
			printf("%15s[%4d]: %3s %6d %7d %7s%6d %7d %5d %9d\n",
				thisfile,
				thisline,
				"mal",
				h->ncalls,
				h->nbytes,
				avgbuf,
				h->ncalls_freed,
				h->nbytes_freed,
				h->ncalls-h->ncalls_freed,
				h->nbytes-h->nbytes_freed);
		else
			printf("%15s[%4d]: %3s %6d %7d %7s\n",
				thisfile,
				thisline,
				"fre",
				h->ncalls,
				h->nbytes,
				avgbuf);
		/* if (verbose >= 2) */
		{
			int j;
			for (j = 0; j < h->stacktrace_depth && 
				   (j < stacktrace_print_depth
				    || stacktrace_print_depth < 0); j++) {
			    if (!h->stacktrace[j]) {
				printf("(lost it)\n");
				break;
			    }
			    stacktrace_get_ffl(h->stacktrace[j],
				    thisfunc, thisfile, &thisline,
				    SYMBUFSIZ-2, SYMBUFSIZ);

			    /* I don't wanna see the args, so there! */
			    {
				char *p = strchr(thisfunc, '(');
				if (p) *p = '\0';
			    }
			    if (thisfunc[strlen(thisfunc)-1] != ')')
				strcat(thisfunc, "()");
			    printf("%-20s %s:%d (%#x)\n",
				thisfunc,
				thisfile,
				thisline,
				h->stacktrace[j]);
			}
		}
	}

	printf("-------------------------------------------------------------------------------\n");

	UNLOCK_MALLOC(1);
}

/*
   Gag me-- mpc's calloc doesn't call malloc, but its free is free...
*/
static void *
a_calloc(size_t n, size_t siz, void *ap)
{
    void *p;
    int old_fillarea;

    _malloc_init();

    LOCK_MALLOC(1);

    old_fillarea = malloc_fillarea;
    malloc_fillarea = 0;
    p = amalloc(n*siz, ap);
    malloc_fillarea = old_fillarea;
    if (p)
	bzero(p, n*siz);

    UNLOCK_MALLOC(1);
    return p;
}

extern void *
acalloc(size_t n, size_t siz, void *ap)
{
    return a_calloc(n, siz, ap);
}

extern void *
calloc(size_t n, size_t siz)
{
    return a_calloc(n, siz, NULL);
}



extern void
cfree(void *p)
{
    free(p);
}

/*
Smart:
    580 out of 600 traces used
    13 max pileup
    144 distinct pileups
    309 total pileups
    290 distinct collisions
    536 total collisions
Stupid:
    581 out of 600 traces used
    10 max pileup
    150 distinct pileups
    357 total pileups
    281 distinct collisions
    626 total collisions

With crowded hash table:
Smart:
    581 out of 600 traces used
    17 max pileup
    229 distinct pileups
    652 total pileups
    692 distinct collisions
    1596 total collisions
Stupid:
    580 out of 600 traces used
    60 max pileup
    227 distinct pileups
    542 total pileups
    1494 distinct collisions
    3273 total collisions
*/

#if RELEASE_MAJOR < 6
#define BZERO_2ND_ARG int
#else
#define BZERO_2ND_ARG size_t
#endif
extern void
bzero(void *b, BZERO_2ND_ARG length)/* XXX why is this not getting found by rld? */
{
    int i;
    _malloc_init();
    for (i = 0; i < length; ++i)
	((char *)b)[i] = 0;
}


/*=============================================================================
    Implement all of malloc(3C)'s and malloc(3X)'s utility routines...
*/

extern void *
amemalign(size_t alignment, size_t n, void *ap)
{
    void *ret = a_malloc(n, ap, alignment);
    if (malloc_trace)
	fdprintf(2, "%s(%d): amemalign(%d, %d, 0x%p) returning 0x%p\n",
		 stacktrace_get_argv0() ? stacktrace_get_argv0() : "", getpid(),
		 alignment, n, ap, ret);
    return ret;
}

extern void *
memalign(size_t alignment, size_t n)
{
    void *ret = a_malloc(n, NULL, alignment);
    if (malloc_trace)
	fdprintf(2, "%s(%d): memalign(%d, %d) returning 0x%p\n",
		 stacktrace_get_argv0() ? stacktrace_get_argv0() : "", getpid(),
		 alignment, n, ret);
    return ret;
}

extern void *
valloc(size_t n)
{
    if (getenv("MALLOC_VERBOSE"))	/* XXX overhead */
	fdprintf(2, "%s(%d): valloc(%d) called\n",
		    stacktrace_get_argv0() ? stacktrace_get_argv0() : "",
		    getpid(),
		    n);

    return memalign(sysconf(_SC_PAGESIZE), n);
}


static void *
a_recalloc(void *p, size_t n, size_t siz, void *ap)
{
    int old_n;

    if (getenv("MALLOC_VERBOSE"))	/* XXX overhead */
	fdprintf(2, "%s(%d): %srecalloc(%#x, %d, %d) called\n",
		    stacktrace_get_argv0() ? stacktrace_get_argv0() : "",
		    getpid(),
		    ap ? "a" : "",
		    p, n, siz);

    if (!p)
	return calloc(n, siz);
    if (!amalloc_isgoodblock_during(p, ap, ap ? "arecalloc" : "recalloc"))
	return 0;	/* failure */
    old_n = (((struct header *)p) - 1)->n;
    p = arealloc(p, n*siz, ap);
    if (p && n*siz > old_n) {
	bzero((char *)p+old_n, n*siz - old_n);
    }
    return p;
}

extern void *
arecalloc(void *p, size_t n, size_t siz, void *ap)
{
    return a_recalloc(p, n, siz, ap);
}

extern void *
recalloc(void *p, size_t n, size_t siz)
{
    return a_recalloc(p, n, siz, NULL);
}

static size_t
a_mallocblksize(void *p, void *ap)
{
    char *e;
    if (!amalloc_isgoodblock_during(p, ap, ap ? "amallocblksize" : "mallocblksize"))
	return 0;	/* failure */

    if ((e = getenv("MALLOC_VERBOSE")) != NULL && atoi(e) >= 2)	/* XXX overhead */
	fdprintf(2, "%s(%d): %smallocblksize(%#x) called, returning %d\n",
		    stacktrace_get_argv0() ? stacktrace_get_argv0() : "",
		    getpid(),
		    ap ? "a" : "",
		    p, (((struct header *)p) - 1)->n);

    return (((struct header *)p) - 1)->n;
}

extern size_t
amallocblksize(void *p, void *ap)
{
    return a_mallocblksize(p, ap);
}

extern size_t
_amallocblksize(void *p, void *ap)
{
    return a_mallocblksize(p, ap);
}

extern size_t
mallocblksize(void *p)
{
    return a_mallocblksize(p, NULL);
}
/* XXX redefine the strong symbol too, to combat
   swmgr's bad behavior (see incident #238834) */
extern size_t
_mallocblksize(void *p)
{
    return a_mallocblksize(p, NULL);
}
