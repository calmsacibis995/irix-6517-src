#include <sys/param.h>
#include <sys/immu.h>
#include <sys/syssgi.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <strings.h>
#include "fsdump.h"
#include "pagesize.h"
#include <sys/resource.h>
#include <errno.h>

/*
 *  PtoD:
 *	converts Pointer to heap base (e.g. &mhnew.hp_inum) TO that heaps Descriptor (des_t *)
 *	Cheats: assumes that des_t is right after pun_t.  See struct memory_heap layout.
 */

#define PtoD(hpp) ((des_t *)((pun_t *)(hpp) + 1))

/*
 * Heap management routines:
 *	roundup	-- rounds up the amount to allocate to a heap by 1 to 2 pages.
 *	init	-- calloc space for estimated size of heap and setup heap struct
 *	reinit	-- realloc space for a larger size heap
 *	finit	-- initialize memory heap from mmap'd file
 *	alloc	-- get space from within heap, adjust heap struct
 *	realloc	-- grow item on heap, perhaps allocating new space, copying over
 *	shrink	-- just moves top down to specified value - within current space.
 *	bsize	-- return size in bytes of portion of heap alloc'd so far.
 *	btop	-- return total number of bytes (alloc'd or not) on heap.
 *	nsize	-- return size in number of allocated elements of heap.
 *	ntop	-- return total number of elements (alloc'd or not) on heap.
 */

#include <pagesize.h>

static hpindex_t heaproundup (hpindex_t nel, uint64_t sz) {
	uint64_t nbytes = nel * sz;

	nbytes = ctob (btoc(nbytes) + 1);
	return nbytes / sz;
}

void heapinit (void **hpp, hpindex_t top, uint64_t sz) {
	des_t *dp = PtoD (hpp);

	top = heaproundup (top, sz);
	if ((*hpp = calloc (top, sz)) == NULL) {
		printf ("heapinit: no memory : %lld of size %lld\n", top, sz);
		error ("no memory");
	}
	dp->hd_elsz = sz;
	dp->hd_next = 0;
	dp->hd_top  = top;
	dp->hd_mflg = MALC;
}

static void heapreinit (void **hpp, hpindex_t oldtop, hpindex_t newtop) {
	des_t *dp = PtoD (hpp);
	uint64_t sz = dp->hd_elsz;
	uint64_t nbytes;
	void *oldhp;

	newtop = heaproundup (newtop, sz);
	nbytes = newtop * sz;
	if (dp->hd_mflg == MALC) {
		oldhp = *hpp;
		if ((*hpp = realloc (*hpp, nbytes)) == NULL) {
			printf ("heapreinit: realloc: no memory:  size %lld\n", nbytes);
			error ("no memory");
		}
	} else {
		oldhp = *hpp;
		if ((*hpp = calloc (newtop, sz)) == NULL) {
			printf ("heapreinit: calloc:  no memory: %lld %lld size %lld\n", oldtop, newtop, sz);
			error ("no memory");
		}

		bcopy (oldhp, *hpp, oldtop*sz);
		dp->hd_mflg = MALC;
	}
	if (newtop > dp->hd_top)
		bzero ((char *)(*hpp) + sz * dp->hd_top, nbytes - sz * dp->hd_top);
	dp->hd_top = newtop;
}

void heapfinit (
	void **hpp,		/* address of heap base */
	char *mapaddr,		/* base of mmap'd buffer == base of fsdump file */
	uint64_t boff,		/* base offset in buffer of this heap */
	uint64_t toff,		/* top offset (end) in buffer of this heap */
	uint64_t elsz,		/* size of each element in this heap */
	uint64_t bsize		/* number of bytes of alloc'd elements */
) {
	des_t *dp = PtoD (hpp);

	dp->hd_elsz = elsz;
	dp->hd_next = bsize/elsz;
	dp->hd_top = (toff - boff)/elsz;
	dp->hd_mflg = MMAP;
	*hpp = mapaddr + boff;
}

hpindex_t heapalloc (void **hpp, hpindex_t nel) {
	des_t *dp = PtoD (hpp);
	hpindex_t newndx;

	newndx = dp->hd_next;
	if (dp->hd_next + nel > dp->hd_top)
		heapreinit (hpp, dp->hd_next, dp->hd_next + nel);
	dp->hd_next += nel;
	return newndx;
}

hpindex_t heaprealloc (void **hpp, hpindex_t oldndx, hpindex_t oldnel, hpindex_t incrnel) {
	des_t *dp = PtoD (hpp);
	hpindex_t newndx;

	if (oldndx + oldnel == dp->hd_next) {
		if (dp->hd_next + incrnel > dp->hd_top)
			heapreinit (hpp, dp->hd_next, dp->hd_next + incrnel);
		newndx = oldndx;
		dp->hd_next += incrnel;
	} else {
		uint64_t sz = dp->hd_elsz;
		void *oldbp;
		void *newbp;

		if (dp->hd_next + oldnel + incrnel > dp->hd_top)
			heapreinit (hpp, dp->hd_next, dp->hd_next + oldnel + incrnel);
		newndx = dp->hd_next;
		dp->hd_next += oldnel + incrnel;

		oldbp = (void *)(*(char **)hpp + sz * oldndx);
		newbp = (void *)(*(char **)hpp + sz * newndx);
		if (oldnel == 1 && sz == sizeof(uint64_t)) {
			*(uint64_t *)newbp = *(uint64_t *)oldbp;
			*(uint64_t *)oldbp = NULL;
		} else if (oldnel != 0) {
			bcopy (oldbp, newbp, sz*oldnel);
			bzero (oldbp, sz*oldnel);
		}
	}
	return newndx;
}

void heapshrink (void **hpp, hpindex_t nel) {
	des_t *dp = PtoD (hpp);
	dp->hd_next = dp->hd_top = nel;
	return;
}

uint64_t heapbsize (void **hpp) {			/* allocated heap byte size */
	des_t *dp = PtoD (hpp);
	return dp->hd_elsz * dp->hd_next;
}

hpindex_t heapnsize (void **hpp) {			/* number allocated elements in heap */
	des_t *dp = PtoD (hpp);
	return dp->hd_next;
}

hpindex_t heapntop (void **hpp) {			/* total number elements available */
	des_t *dp = PtoD (hpp);
	return dp->hd_top;
}


static void xheapreinit (void **hpp, hpindex_t oldtop, hpindex_t newtop) {
    des_t *dp = PtoD (hpp);
    uint64_t sz = dp->hd_elsz;
    void *oldhp;
    int fd;

    newtop = heaproundup (newtop, sz);
    if (dp->hd_mflg != MMPA) {
	size_t heapgrowth;
	oldhp = *hpp;
	if ((fd = open ("/dev/zero", O_RDWR)) < 0) {  
	    error ("cannot open temp file");
	}
	/* Hardcoded 4 Gbyte limit -- this code logic doesn't handle growing fenv twice.
	 * fsdump dumps core if fenv exceeds this limit (as it did July 98, on the file
	 * system bonnie.engr.sgi.com:/disks/patches, with older 300 Mb limit).  It might
	 * be that limit can be set much higher than 4 Gb -- don't know.    -- pj
	 */
#if (_MIPS_SZLONG == 32)
	heapgrowth = 300000000;
#else
	heapgrowth = 0x100000000LL;	/* 4 Gbyte */
#endif
	*hpp = mmap ((void *)0, heapgrowth, (PROT_READ|PROT_WRITE),
	    MAP_PRIVATE|MAP_AUTOGROW|MAP_AUTORESRV, fd, 0);

	if (*hpp == (char *)MAP_FAILED) {
	    /* See further PV Incident 754183 for explanation */
	    rlim_t vmemory;
	    extern int errno;
	    int mmap_errno = errno;
	    struct rlimit rl;

	    if (getrlimit (RLIMIT_VMEM, &rl) == 0)
		vmemory = rl.rlim_cur;
	    else
		vmemory = heapgrowth;

	    if (mmap_errno == ENOMEM && vmemory < heapgrowth) {
		fprintf (stderr, "fsdump unable to mmap %lld bytes,\n",
		    (long long) heapgrowth);
		fprintf (stderr, "\tbecause rlimit vmemory is only %lld bytes.\n",
		    (long long) vmemory);
		error ("fsdump mmap autogrow #2 failed: ulimit vmemory set too low");
	    } else {
		error("error in mmap autogrow #2");
	    }
	}
	bcopy (oldhp, *hpp, oldtop*sz);
	dp->hd_mflg = MMPA;
    }
    dp->hd_top = newtop;
}

hpindex_t xheaprealloc (void **hpp, hpindex_t oldndx, hpindex_t oldnel, hpindex_t incrnel) {
        des_t *dp = PtoD (hpp);
        hpindex_t newndx;

        if (oldndx + oldnel == dp->hd_next) {
                if (dp->hd_next + incrnel > dp->hd_top)
                        xheapreinit (hpp, dp->hd_next, dp->hd_next + incrnel);
                newndx = oldndx;
                dp->hd_next += incrnel;
        } else {
                uint64_t sz = dp->hd_elsz;
                void *oldbp;
                void *newbp;

                if (dp->hd_next + oldnel + incrnel > dp->hd_top) {
                        xheapreinit (hpp, dp->hd_next, dp->hd_next + oldnel + incrnel);
			newbp = (void *)(*(char **)hpp + sz * (dp->hd_top - 1));
		}
                newndx = dp->hd_next;
                dp->hd_next += oldnel + incrnel;

                oldbp = (void *)(*(char **)hpp + sz * oldndx);
                newbp = (void *)(*(char **)hpp + sz * newndx);
                if (oldnel == 1 && sz == sizeof(uint64_t)) {
                        *(uint64_t *)newbp = *(uint64_t *)oldbp;
                        *(uint64_t *)oldbp = NULL;
                } else if (oldnel != 0) {
                        bcopy (oldbp, newbp, sz*oldnel);
                        bzero (oldbp, sz*oldnel);
                }
        }
        return newndx;
}
