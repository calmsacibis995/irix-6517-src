/**************************************************************************
 *									  *
 * 		 Copyright (C) 1993 Silicon Graphics, Inc.	          *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$: $"

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/sysmp.h>
#include <sys/prctl.h>
#include <sys/resource.h>

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <invent.h>
#include <ulocks.h>
#include <signal.h>
#include <malloc.h>

#include <errno.h>

typedef	uchar_t	bool_t;

#define	CACHE_LINE_SIZE		128

#define	MEMORY_SIZE_INCREMENT	(16*1024*1024)

#ifndef	FALSE
#   define	FALSE	0
#endif

#ifndef	TRUE
#   define	TRUE	(!(FALSE))
#endif

#define	DEBUG	1
#if	DEBUG
#   undef	DEBUG
#   define	DEBUG(x)	printf x
#else
#   undef	DEBUG
#   define	DEBUG
#endif

static	char	*mem_prog	= NULL;
static	uint	mem_cpu		= 0;
static	ulong_t	mem_size	= 0;	/* memory size in bytes */
static	uint	mem_dCache	= 0;
static	uint	mem_iCache	= 0;
static	uint	mem_sdCache	= 0;
static	uint	mem_siCache	= 0;
static	uint	mem_sidCache	= 0;
static	char	*mem_device	= "/dev/kmem";

typedef	struct child {
    ulong_t	*c_s;			/* start address */
    ulong_t	*c_e;			/* end address */
    bool_t	c_read, 
                c_write, 
                c_verify,
                c_random;		/* random pattern, random places */
    ulong_t	c_inc;			/* increment value */
    ulong_t	c_loop;			/* # times to loop */
} child_t;

static	bool_t
getInt(const char *s, ulong_t *n)
/*
 * Function: getInt
 * Purpose: To convert a number in a nice checking sorta way.
 * Parameters:	s - string to convert
 *		n - pointer to store number into.
 * Returns:	TRUE for success, FALSE otherwise.
 */
{
    char	*sp;

    *n = strtol(s, &sp, 0);
    if (*sp == 'M' || *sp == 'm') {
	*n *= 0x100000L;		/* 1MB */
	sp++;
    } else if (*sp == 'K' || *sp == 'k') {
	*n *= 0x400L;			/* 1KB */
	sp++;
    }
    return('\0' == *sp);
}

static	void
sighup(void)
{
    if (1 == getppid()) {		/* check for parent's death */
	exit(0);
    }
}

static	ulong_t
random(unsigned int *s)
/*
 * Function: random
 * Purpose: To produce a 32-bit random value from the 16-bit random
 *	    value produced in rand ...
 * Parameters: s - seed
 * Returns: random value
 */
{
    return((rand_r(s) << 16) | (rand_r(s) & 0xffff));
}    

static	void
readWrite(child_t *c)
/*
 * Function: readWRite
 * Purpose: To read and write memory over a given range
 * Parameters: 	c - pointer to child parameters.
 * Returns: When done.
 */
{
#    define	THRESHOLD		10
    register	volatile ulong_t	*p;
    register	ulong	rVal;
    register	int	errors, inc;
    bool_t	forever;
    unsigned	int	seed;
    register 	ulong_t	pattern = 0xaaaaaaaa;
#   define	PATTERN 	pattern
#   define	CHECK_RANGE(p, e, i)\
                     if ((ulong_t)(p) + 8 * (i) > (ulong_t)(e)) {\
			 (p) = (ulong_t *)(p) - 8 * (i);\
		     }

    DEBUG(("%s: debug: child(%d): Started \n", mem_prog, (int)getpid()));
    (void)signal(SIGHUP, sighup);
    if ((ptrdiff_t)-1 == prctl(PR_TERMCHILD)) {
	fprintf(stderr, "%s: child failed prctl: %s\n", mem_prog, 
		strerror(errno));
	exit(1);
    }
    sleep(5);
    errors = 0;
    inc = c->c_inc;
    forever = (c->c_loop == 0);
    if (c->c_random) {
	seed = getpid() ^ (ulong_t)time(NULL);
	DEBUG(("%s: debug: child(%d): Random mode - seed = 0x%8.8x\n", 
	       mem_prog, (int)getpid(), seed));
    }

    if (c->c_read && !c->c_write) {
	/* Just cause a read load on this region */
	while (forever || c->c_loop--) {
	    p = c->c_s;
	    while (p < c->c_e) {
		/*
		 * If every word to be touched, unroll the loop
		 * to be sure we get lots of accesses without 
		 * the branch delays.
		 */
		*p;
		p = (ulong_t *)((ulong_t)p + inc);
		*p;
		p = (ulong_t *)((ulong_t)p + inc);
		*p;
		p = (ulong_t *)((ulong_t)p + inc);
		*p;
		p = (ulong_t *)((ulong_t)p + inc);
		*p;
		p = (ulong_t *)((ulong_t)p + inc);
		*p;
		p = (ulong_t *)((ulong_t)p + inc);
		*p;
		p = (ulong_t *)((ulong_t)p + inc);
		*p;
		p = (ulong_t *)((ulong_t)p + inc);
		if (c->c_random) {		/* pick next address */
		    p = (ulong_t *)
			((random(&seed) & ~(sizeof(ulong_t) - 1))
			 % (ulong_t)c->c_s+(u_long)c->c_s);
		    CHECK_RANGE(p, c->c_e, inc);
		    break;
		}
	    }
	} 
    } else if (c->c_write && !c->c_read) {
	/* Just a write load */
	while (forever || c->c_loop--) {
	    if (c->c_random) {
		pattern = random(&seed);
	    } 
	    p = c->c_s;
	    while (p < c->c_e) {
		/*
		 * If every word to be touched, unroll the loop
		 * to be sure we get lots of accesses without 
		 * the branch delays.
		 */
		*p = PATTERN;
		p = (ulong_t *)((ulong_t)p + inc);
		*p = PATTERN;
		p = (ulong_t *)((ulong_t)p + inc);
		*p = PATTERN;
		p = (ulong_t *)((ulong_t)p + inc);
		*p = PATTERN;
		p = (ulong_t *)((ulong_t)p + inc);
		*p = PATTERN;
		p = (ulong_t *)((ulong_t)p + inc);
		*p = PATTERN;
		p = (ulong_t *)((ulong_t)p + inc);
		*p = PATTERN;
		p = (ulong_t *)((ulong_t)p + inc);
		*p = PATTERN;
		p = (ulong_t *)((ulong_t)p + inc);
		if (c->c_random) {		/* pick next address */
		    p = (ulong_t *)
			((random(&seed) & ~(sizeof(ulong_t) - 1))
			 % (ulong_t)c->c_s+(u_long)c->c_s);
		    CHECK_RANGE(p, c->c_e, inc);
		    break;
		}
	    }
	}
    } else if (c->c_read && c->c_write) {
	while (forever || c->c_loop--) {
	    if (c->c_random) {
		pattern = random(&seed);
	    } 
	    p = c->c_s;
	    while (p < c->c_e) {
		*p = PATTERN;
		p = (ulong_t *)((ulong_t)p + inc);
#if 0
		*p = PATTERN;
		p = (ulong_t *)((ulong_t)p + inc);
		*p = PATTERN;
		p = (ulong_t *)((ulong_t)p + inc);
		*p = PATTERN;
		p = (ulong_t *)((ulong_t)p + inc);
		*p = PATTERN;
		p = (ulong_t *)((ulong_t)p + inc);
		*p = PATTERN;
		p = (ulong_t *)((ulong_t)p + inc);
		*p = PATTERN;
		p = (ulong_t *)((ulong_t)p + inc);
		*p = PATTERN;
		p = (ulong_t *)((ulong_t)p + inc);
#endif
	    }
	    p = c->c_s;
	    while (p < c->c_e) {
		rVal = *p;
		if (PATTERN != rVal) {
		    if (errors++ < THRESHOLD) {
			fprintf(stderr, "ERROR *** memory mismatch @ 0x%8.8x: "
				        "exp 0x%8.8x recv 0x%8.8x\n", 
				(ulong_t)p, PATTERN, rVal);
		    }
		    free(c);
		    exit(0);
		}
		p = (ulong_t *)((ulong_t)p + inc);
	    }
	    if (c->c_random) {		/* pick next address */
		p = (ulong_t *)
		    ((random(&seed) & ~(sizeof(ulong_t) - 1))
		     % (ulong_t)c->c_s+(u_long)c->c_s);
		CHECK_RANGE(p, c->c_e, inc);
	    }
	}
    }
    free(c);
    exit(0);
}

static	void
sigalarm(void)
{
    fprintf(stderr, "%s: Run complete - time limit exceeded\n", mem_prog);
    exit(0);
}

int
main(int argc, char *argv[])
/*
 * Function: main (memory.c)
 * Purpose: To exercise main memory as much as possible.
 * Parameters: -c - Thrash the cache as much as possible
 *	      	-d <device> - set memory device  name.
 *	       	-i - tasks are independant (do not touch overlapping
 *		     regions.
 *		-l <count> - number of loops each task makes over its region.
 *		-m - malloc memory (use normal virtual pages.
 *		-r - read all of the memory regions specified
 *		-s - set explicite memory size.
 *		-t - expklicitely set # of tasks.
 *		-v - verify memory.
 *		-w - write all of the memory regions specified.
 *		
 *	       -t   - # tasks to run
 * Returns: 
 */
{
    inventory_t	*inv;
    int		c, i;
    int		fd;
    void	*p, *e;
    ulong_t	extent;
    int		iMem;			/* increment for hitting memory */
    ulong_t	tasks;			/* # tasks */
    bool_t	wMem, rMem;		/* boolean, write/read memory */
    bool_t	independant;		/* TRUE if tasks do NOT overlap */
    bool_t	mallocMem;		/* TRUE if tasks to malloc */
    bool_t	vMem;			/* TRUE if task to verify writes */
    bool_t	random;			/* TRUE ==> random accesses */
    ulong_t	mLength;		/* length of memory to access */
    int		errors;			/* # errors during parsing */
    ulong_t	loop;			/* # of loops to run 0 == forever */
    bool_t	child;			/* TRUE if we are a worker process */
    usptr_t	*smp;			/* shared memory pointer */
   ulong_t	timeout;		/* # seconds to run */
    struct rlimit rl;

    mem_prog = argv[0];
    errors = 0;
    rMem = TRUE;
    wMem = FALSE;
    child = FALSE;
    independant = FALSE;
    iMem = sizeof(ulong_t);
    loop = 0;
    smp = NULL;
    tasks = 0;
    vMem = FALSE;
    random = FALSE;
    timeout = 0;
    mLength = 0;

    while (EOF != (c = getopt(argc, argv, "acd:l:imp:s:rt:wv"))) {
	switch(c) {
	case 'a':			/* random */
	    random = TRUE;
	    break;
	case 'c':			/* thrash cache */
	    iMem = CACHE_LINE_SIZE;
	    break;
	case 'd':
	    mem_device = optarg;
	    break;
	case 'i':			/* tasks independant? */
	    independant = TRUE;
	    break;
	case 'l':
	    if (!getInt(optarg, &loop)) {
		fprintf(stderr, "%s: ERROR: invalid loop count: %s\n", 
			argv[0], optarg);
		errors++;
	    } else if (0 == loop) {
		fprintf(stderr, 
			"%s: ERROR: warning loop count 0 means loop forever\n",
			argv[0]);
	    }
	    break;
	case 'm':
	    mallocMem = TRUE;
	    break;
	case 'p':			/* Number of tasks */
	    if (!getInt(optarg, &tasks) || (0 == tasks)) {
		fprintf(stderr, "%s: ERROR: invalid task count: %s\n", 
			argv[0], optarg);
		errors++;
	    }
	    break;
	case 's':
	    if (!getInt(optarg, &mLength)) {
		fprintf(stderr, "%s: ERROR: invalid size: %s\n", 
			argv[0], optarg);
		errors++;
	    }
	    break;
	case 't':
	    if (!getInt(optarg, &timeout)) {
		fprintf(stderr, "%s: ERROR: invalid timeout: %s\n", 
			argv[0], optarg);
		errors++;
	    }
	    break;
	case 'r':
	    rMem = TRUE;
	    break;
	case 'v':
	    vMem = TRUE;
	    break;
	case 'w':
	    wMem = TRUE;
	    break;
	case '?':
	default:
	    fprintf(stderr, "usage: ...");
	    exit(1);
	}
    }
    if (errors) {
	exit(1);
    }

    while (NULL != (inv = getinvent())) {
	switch(inv->inv_class) {
	case INV_MEMORY:
	    switch(inv->inv_type) {
	    case INV_MAIN:
		if (mem_size == 0)
			mem_size = (long)inv->inv_state;
		break;
	    case INV_MAIN_MB:
		mem_size = (long)inv->inv_state * 0x100000L;
		break;
	    case INV_DCACHE:
		mem_dCache += inv->inv_state;
		break;
	    case INV_ICACHE:
		mem_iCache += inv->inv_state;
		break;
	    case INV_SDCACHE:
		mem_sdCache += inv->inv_state;
		break;
	    case INV_SICACHE:
		mem_siCache += inv->inv_state;
		break;
	    case INV_SIDCACHE:
		mem_sidCache += inv->inv_state;
		break;
	    default:
		fprintf(stderr, "Unknown memory type (%d)\n", inv->inv_type);
		exit(1);
	    }
	case INV_PROCESSOR:
	    break;
	default:
	    break;
	}
    }
    endinvent();
    
    mem_cpu = sysmp(MP_NPROCS);
    if (0 == tasks) {
	tasks = mem_cpu;
    }
    if (!independant && vMem) {
	fprintf(stderr, "%s: * warning * Verify (-v) ignored for "
		        "non-independant tasks\n", argv[0]);
	vMem = FALSE;
    }
    if (random && loop != 0) {
	fprintf(stderr, "%s: * warning * Using random mode should use"
		        " a time limit not a loop count\n", argv[0]);
    }


    if (0 == mLength) {
	mLength = mem_size;
    }

    if (independant)
	extent = (mLength / tasks) & ~(0x3L);
    else
	extent = mLength;

    fprintf(stderr, "%s: task(%d) memory(0x%8.8x) m/t(0x%x) %s\n", 
	    argv[0], tasks, mLength, extent,  
	    mallocMem ? "<allocate memory>" : mem_device);
    fprintf(stderr,"%s: configuration: cpu(%d) mem(%dMB) i$(%dKB) d$(%dKB)\n",
	    argv[0], mem_cpu, mem_size / 0x100000L, 
	    mem_iCache / 0x400L,
	    mem_dCache / 0x400L);

    if (mallocMem) {			/* doing this in virtual space */
	bool_t	downsized = FALSE;

	/* Try to get as much as possible */
	do {
	    p = malloc((size_t) mLength);
	    if (NULL == p) {
		downsized = TRUE;
		if (mLength > MEMORY_SIZE_INCREMENT) {
		    mLength -= MEMORY_SIZE_INCREMENT;
		} else {
		    break;
		}
	    }
	} while (NULL == p);
	if (NULL == p) {
	    fprintf(stderr,"%s: ERROR: malloc failed:%s\n", 
		    argv[0],strerror(errno));
	    exit(1);
	} else if (downsized) {
	    fprintf(stderr, "%s: WARNING: memory downsized to %dB (%dKB/%dMB)\nn",
		    argv[0], mLength, mLength/1024, mLength / (1024*1024));
	}
    } else {

	/*
	 * MMAP /dev/kmem for reading.
	 */

	fd = open(mem_device, wMem ? O_RDWR : O_RDONLY, 0);
	if (0 > fd) {
	    fprintf(stderr, "%s: ERROR: failed to open %s: %s\n", 
		    argv[0], mem_device, strerror(errno));
	    exit(1);
	}
	p = mmap(NULL, (size_t)mLength, PROT_READ + (wMem ? PROT_WRITE : 0), 
		 MAP_SHARED, fd, (off_t)0);
	if ((void *)-1L == p) {
	    fprintf(stderr, "%s: ERROR: mmap failed for %s: %s\n", argv[0], 
		    mem_device, strerror(errno));
	    exit(1);
	}
	if (independant) {
	    mLength /= tasks;
	}
    }

    DEBUG(("%s: parent pid(%d)\n", argv[0], (int)getpid()));

    /* Set time out if one specifid */

    if (timeout) {
	(void)signal(SIGALRM, sigalarm);
	(void)alarm((unsigned)timeout);
    }

    e = (void *)((ulong_t)p + extent);

    /* Try to set limits */

    rl.rlim_cur = RLIM_INFINITY;
    rl.rlim_max = RLIM_INFINITY;
    if (0 > setrlimit(RLIMIT_VMEM, &rl)) {
	fprintf(stderr, 
		"%s: WARNING: setrlimit failed (may not run): %s\n",
		argv[0], strerror(errno));
    }

    /* set shared parameters */

    (void )usconfig(CONF_INITUSERS, tasks + 10);

    for (i = 0; i < tasks; i++) {
	child_t *c;
	c = (child_t *)(malloc(sizeof(child_t)));
	if (NULL == c) {
	    fprintf(stderr, "%s: ERROR: malloc failed: %s", argv[0], strerror(errno));
	    break;
	}
	c->c_read = rMem;
	c->c_write = wMem;
	c->c_verify = vMem;
	c->c_inc = iMem;
	c->c_loop = loop;
	c->c_random = random;
	c->c_s    = p;
	c->c_e    = e;
	if (independant) {
	    p = c->c_e = (void *)((ulong_t)p + extent);
	    e = (ulong_t *)((ulong_t)e + extent);
	}

	if (((ulong_t)c->c_s & 0x3) || ((ulong_t)c->c_e & 0x3) || 
	    (c->c_inc & 0x3)) {
	    fprintf(stderr, "ERROR: Alignment problems exist!!!\n");
	    fprintf(stderr, "task %d, c_s = 0x%x, c_e = 0x%x, c_inc = 0x%x\n", 
		    i, c->c_s, c->c_e, c->c_inc); 
	    exit(1);
        }
	 
	if (0 > sproc((void *)readWrite, PR_SADDR, c)) {
	    if ((ENOMEM != errno) || (i * 100 / tasks < 75)) {
		fprintf(stderr, 
		"%s: ERROR: sproc failed with too few tasks(%d/%d): %s\n", 
			argv[0], i, tasks, strerror(errno));
		break;
	    } else if (ENOMEM == errno) {
		fprintf(stderr, "%s: WARNING: sproc failed, "
			        "downsized (%d/%d tasks): %s\n", 
			argv[0], i, tasks, strerror(errno));
		break;
	    } else {
		fprintf(stderr, "%s: ERROR: sproc failed: %s\n", argv[0], 
			strerror(errno));
		break;
	    }
	}
    }
    while (TRUE) {
	pid_t	pid;
	pid = wait(NULL);
	if (-1 == pid) {
	    if (ECHILD == errno) {	/* done! */
		break;
	    } else {
		fprintf(stderr, "%s: ERROR: wait failed: %s\n", argv[0], 
			strerror(errno));
		break;
	    }
	} 
	DEBUG(("%s: debug: child(%d): done\n", argv[0], (int)pid));
    }
    exit(0);
}
	   
