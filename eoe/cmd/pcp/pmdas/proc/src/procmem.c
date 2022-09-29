/*
 * Copyright 1995, Silicon Graphics, Inc.
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
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */

#ident "$Id: procmem.c,v 1.6 1998/11/15 08:35:24 kenmcd Exp $"

#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#if defined(sgi)
#include <sys/procfs.h>
#include <sys/immu.h>
#endif

#include "pmapi.h"
#include "procmem.h"

/*
 * Determine the virtual and physical (pro-rated) memory usage
 * for the process for which the fd argument is an open file descriptor.
 * Return 0 for success else errno.
 */

#if defined(sgi)
int
_pmProcMem(int fd, _pmProcMem_t *membuf)
{
    register struct prmap_sgi *map, *end_map;
    prmap_sgi_arg_t		maparg;
    int				nmaps;
    int				e;
    unsigned long		wrss;
    unsigned long		refcnt;
    unsigned long		flags;
    unsigned long		virtual;
    unsigned long		physical;
    static int			_pagesize = 0;
    static int			max_maps = 0;
    static struct prmap_sgi	*maps = NULL;

    if (_pagesize == 0)
	_pagesize = getpagesize();

    (void)memset(membuf, 0, sizeof(_pmProcMem_t));

    if ((e = ioctl(fd, PIOCNMAP, &nmaps)) < 0) {
	return -errno;
    }

    if (nmaps == 0) {
        /*
	 * kernel process or zombie?
	 * No error (and zero mem usage)
	 */
        return 0;
    }

    /* allow 16 more region maps in case it's currently growing */
    nmaps += 16;

AGAIN:
    if (nmaps > max_maps) {
	max_maps = nmaps;
	maps = (struct prmap_sgi *)realloc(maps, max_maps * sizeof(struct prmap_sgi));
	if (maps == NULL) {
	    max_maps = 0;
	    return -errno;
	}
    }

    maparg.pr_vaddr = (caddr_t)maps;
    maparg.pr_size = max_maps * sizeof(struct prmap_sgi);

    if ((e = ioctl(fd, PIOCMAP_SGI, &maparg)) < 0) {
	return -errno;
    }

    /* e is the real number of maps returned */
    if ((nmaps = e) > max_maps) {
	/* whoops, go again */
	goto AGAIN;
    }

    for (map = maps, end_map = maps + nmaps; map < end_map; map++) {

	flags = map->pr_mflags;
	virtual = map->pr_size;
	refcnt = flags >> MA_REFCNT_SHIFT;

	if (refcnt == 0) /* uh? */
	    continue;

	if ((flags & MA_PHYS) == 0) {
	    wrss = map->pr_wsize * _pagesize;
	    wrss /= MA_WSIZE_FRAC;
	    wrss /= refcnt; /* if refcnt > 1 => sprocs sharing region */

	    physical = (unsigned long)wrss;
	}
	else {
	    /* mapping is a physical device! */
	    physical = 0;
	}

#if 0 /* too verbose for any pmDebug settings */
	__pmNotifyErr(LOG_WARNING, "virt %6d phys %6d flags=0x%04x\n", virtual, physical, flags);
	__pmNotifyErr(LOG_WARNING, "	(%s%s%s%s OTHER=0x%04x)\n",
		(flags & MA_EXEC) ? "EXEC " : "",
		(flags & MA_SHMEM) ? "SHMEM " : "",
		(flags & MA_BREAK) ? "BREAK " : "",
		(flags & MA_STACK) ? "STACK " : "",
		flags & ~(MA_EXEC|MA_SHMEM|MA_BREAK|MA_STACK));
#endif

        if (flags & MA_EXEC) {
            /* executable text */
	    membuf->vtxt += virtual;
	    membuf->ptxt += physical;
	}
	else {
	    if (flags & MA_STACK) {
		/* stack */
		membuf->vstk += virtual;
		membuf->pstk += physical;
	    }
	    else {
		if (flags & MA_SHMEM) {
		    /* shared memory */
		    membuf->vshm += virtual;
		    membuf->pshm += physical;
		}
		else {
		    if (flags & MA_BREAK) {
			/* bss */
			membuf->vbss += virtual;
			membuf->pbss += physical;
		    }
		    else {
			/* initialized data */
			membuf->vdat += virtual;
			membuf->pdat += physical;
		    }
		}
	    }
	}
    }

    /* convert from bytes to Kbytes */
    membuf->ptxt >>= 10;
    membuf->vtxt >>= 10;
    membuf->pdat >>= 10;
    membuf->vdat >>= 10;
    membuf->pbss >>= 10;
    membuf->vbss >>= 10;
    membuf->pstk >>= 10;
    membuf->vstk >>= 10;
    membuf->pshm >>= 10;
    membuf->vshm >>= 10;

    /* success */
    return 0;
}

#else /* not supported on non-sgi platforms */

int
_pmProcMem(int fd, _pmProcMem_t *membuf)
{
    (void)memset(membuf, 0, sizeof(_pmProcMem_t));
    return 0;
}

#endif
