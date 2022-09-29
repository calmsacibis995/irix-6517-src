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

#ident "$Id: pracinfo.c,v 1.6 1998/09/09 19:04:35 kenmcd Exp $"

#include <unistd.h>
#include <stdio.h>
#include <syslog.h>
#include <errno.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>
#include <sys/stat.h>
#include <dirent.h>
#include <sys/procfs.h>
#include <sys/immu.h>
#include <sys/sysmacros.h>
#include <pwd.h>
#include <grp.h>

#if !defined(IRIX5_3)
#include <sys/extacct.h>
#endif

#include "pmapi.h"
#include "proc.h"
#include "proc_aux.h"
#include "cluster.h"


#if !defined(IRIX5_3)
/* buffers for pracinfo metrics, per pid */
static pracinfo_t *pracinfo_buf = NULL;
#endif

static pmDesc	desctab[] = {
    /* proc.accounting.flag */
#if !defined(IRIX5_3)
#if _MIPS_SZLONG == 64
    { PMID(6,0), PM_TYPE_64, PM_INDOM_PROC, PM_SEM_INSTANT, {0,0,0,0,0,0} },
#elif _MIPS_SZLONG == 32
    { PMID(6,0), PM_TYPE_32, PM_INDOM_PROC, PM_SEM_INSTANT, {0,0,0,0,0,0} },
#endif
#else
    { PMID(6,0), PM_TYPE_NOSUPPORT, PM_INDOM_PROC, PM_SEM_INSTANT, {0,0,0,0,0,0} },
#endif

    /* proc.accounting.ash */
#if !defined(IRIX5_3)
#if _MIPS_SZLONG == 64
    { PMID(6,1), PM_TYPE_64, PM_INDOM_PROC, PM_SEM_DISCRETE, {0,0,0,0,0,0} },
#elif _MIPS_SZLONG == 32
    { PMID(6,1), PM_TYPE_32, PM_INDOM_PROC, PM_SEM_DISCRETE, {0,0,0,0,0,0} },
#endif
#else
    { PMID(6,1), PM_TYPE_NOSUPPORT, PM_INDOM_PROC, PM_SEM_DISCRETE, {0,0,0,0,0,0} },
#endif

    /* proc.accounting.prid */
#if !defined(IRIX5_3)
#if _MIPS_SZLONG == 64
    { PMID(6,2), PM_TYPE_64, PM_INDOM_PROC, PM_SEM_DISCRETE, {0,0,0,0,0,0} },
#elif _MIPS_SZLONG == 32
    { PMID(6,2), PM_TYPE_32, PM_INDOM_PROC, PM_SEM_DISCRETE, {0,0,0,0,0,0} },
#endif
#else
    { PMID(6,2), PM_TYPE_NOSUPPORT, PM_INDOM_PROC, PM_SEM_DISCRETE, {0,0,0,0,0,0} },
#endif

    /* proc.accounting.timers.utime */
#if !defined(IRIX5_3)
    { PMID(6,3), PM_TYPE_U64, PM_INDOM_PROC, PM_SEM_COUNTER, {0,1,0,0,PM_TIME_NSEC,0} },
#else
    { PMID(6,3), PM_TYPE_NOSUPPORT, PM_INDOM_PROC, PM_SEM_COUNTER, {0,1,0,0,PM_TIME_NSEC,0} },
#endif

    /* proc.accounting.timers.stime */
#if !defined(IRIX5_3)
    { PMID(6,4), PM_TYPE_U64, PM_INDOM_PROC, PM_SEM_COUNTER, {0,1,0,0,PM_TIME_NSEC,0} },
#else
    { PMID(6,4), PM_TYPE_NOSUPPORT, PM_INDOM_PROC, PM_SEM_COUNTER, {0,1,0,0,PM_TIME_NSEC,0} },
#endif

    /* proc.accounting.timers.bwtime */
#if !defined(IRIX5_3)
    { PMID(6,5), PM_TYPE_U64, PM_INDOM_PROC, PM_SEM_COUNTER, {0,1,0,0,PM_TIME_NSEC,0} },
#else
    { PMID(6,5), PM_TYPE_NOSUPPORT, PM_INDOM_PROC, PM_SEM_COUNTER, {0,1,0,0,PM_TIME_NSEC,0} },
#endif

    /* proc.accounting.timers.rwtime */
#if !defined(IRIX5_3)
    { PMID(6,6), PM_TYPE_U64, PM_INDOM_PROC, PM_SEM_COUNTER, {0,1,0,0,PM_TIME_NSEC,0} },
#else
    { PMID(6,6), PM_TYPE_NOSUPPORT, PM_INDOM_PROC, PM_SEM_COUNTER, {0,1,0,0,PM_TIME_NSEC,0} },
#endif

    /* proc.accounting.timers.qwtime */
#if !defined(IRIX5_3)
    { PMID(6,7), PM_TYPE_U64, PM_INDOM_PROC, PM_SEM_COUNTER, {0,1,0,0,PM_TIME_NSEC,0} },
#else
    { PMID(6,7), PM_TYPE_NOSUPPORT, PM_INDOM_PROC, PM_SEM_COUNTER, {0,1,0,0,PM_TIME_NSEC,0} },
#endif

    /* proc.accounting.counts.mem */
#if !defined(IRIX5_3)
    { PMID(6,8), PM_TYPE_U64, PM_INDOM_PROC, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} },
#else
    { PMID(6,8), PM_TYPE_NOSUPPORT, PM_INDOM_PROC, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} },
#endif

    /* proc.accounting.counts.swaps */
#if !defined(IRIX5_3)
    { PMID(6,9), PM_TYPE_U64, PM_INDOM_PROC, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} },
#else
    { PMID(6,9), PM_TYPE_NOSUPPORT, PM_INDOM_PROC, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} },
#endif

    /* proc.accounting.counts.chr */
#if !defined(IRIX5_3)
    { PMID(6,10), PM_TYPE_U64, PM_INDOM_PROC, PM_SEM_COUNTER, {1,0,0,PM_SPACE_BYTE,0,0} },
#else
    { PMID(6,10), PM_TYPE_NOSUPPORT, PM_INDOM_PROC, PM_SEM_COUNTER, {1,0,0,PM_SPACE_BYTE,0,0} },
#endif

    /* proc.accounting.counts.chw */
#if !defined(IRIX5_3)
    { PMID(6,11), PM_TYPE_U64, PM_INDOM_PROC, PM_SEM_COUNTER, {1,0,0,PM_SPACE_BYTE,0,0} },
#else
    { PMID(6,11), PM_TYPE_NOSUPPORT, PM_INDOM_PROC, PM_SEM_COUNTER, {1,0,0,PM_SPACE_BYTE,0,0} },
#endif

    /* proc.accounting.counts.br */
#if !defined(IRIX5_3)
    { PMID(6,12), PM_TYPE_U64, PM_INDOM_PROC, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} },
#else
    { PMID(6,12), PM_TYPE_NOSUPPORT, PM_INDOM_PROC, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} },
#endif

    /* proc.accounting.counts.bw */
#if !defined(IRIX5_3)
    { PMID(6,13), PM_TYPE_U64, PM_INDOM_PROC, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} },
#else
    { PMID(6,13), PM_TYPE_NOSUPPORT, PM_INDOM_PROC, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} },
#endif

    /* proc.accounting.counts.syscr */
#if !defined(IRIX5_3)
    { PMID(6,14), PM_TYPE_U64, PM_INDOM_PROC, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} },
#else
    { PMID(6,14), PM_TYPE_NOSUPPORT, PM_INDOM_PROC, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} },
#endif

    /* proc.accounting.counts.syscw */
#if !defined(IRIX5_3)
    { PMID(6,15), PM_TYPE_U64, PM_INDOM_PROC, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} },
#else
    { PMID(6,15), PM_TYPE_NOSUPPORT, PM_INDOM_PROC, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} },
#endif

    /*
     * NOTE: in accounting structure but not implemented in 5.3, 6.1 (or 6.2?)
     * Always returns a large, constant value (12105675799276118504)
     */
    /* proc.accounting.counts.disk */
#if defined(IRIX5_3) || defined(IRIX6_1) || defined(IRIX6_2)
    { PMID(6,16), PM_TYPE_NOSUPPORT, PM_INDOM_PROC, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} },
#else
    { PMID(6,16), PM_TYPE_U64, PM_INDOM_PROC, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} },
#endif

};

static int      ndesc = (sizeof(desctab)/sizeof(desctab[0]));

void
pracinfo_init(int dom)
{
    init_table(ndesc, desctab, dom);
}

int 
pracinfo_getdesc(pmID pmid, pmDesc *desc)
{
    return getdesc(ndesc, desctab, pmid, desc);
}


#if defined(IRIX5_3)
/*ARGSUSED0*/
#endif
int
pracinfo_setatom(int item, pmAtomValue *atom, int j)
{
#if !defined(IRIX5_3)
    switch (item) {
	/* proc.accounting.flag */
	case 0:
	    atom->l = pracinfo_buf[j].pr_flag;
	    break;

	/* proc.accounting.ash */
	case 1:
	    set_atom_long(atom, pracinfo_buf[j].pr_ash);
	    break;

	/* proc.accounting.prid */
	case 2:
	    set_atom_long(atom, pracinfo_buf[j].pr_prid);
	    break;

	/* proc.accounting.timers.utime */
	case 3:
	    atom->ull = (__uint64_t)(pracinfo_buf[j].pr_timers.ac_utime);
	    break;

	/* proc.accounting.timers.stime */
	case 4:
	    atom->ull = (__uint64_t)(pracinfo_buf[j].pr_timers.ac_stime);
	    break;

	/* proc.accounting.timers.bwtime */
	case 5:
	    atom->ull = (__uint64_t)(pracinfo_buf[j].pr_timers.ac_bwtime);
	    break;

	/* proc.accounting.timers.rwtime */
	case 6:
	    atom->ull = (__uint64_t)(pracinfo_buf[j].pr_timers.ac_rwtime);
	    break;

	/* proc.accounting.timers.qwtime */
	case 7:
	    atom->ull = (__uint64_t)(pracinfo_buf[j].pr_timers.ac_qwtime);
	    break;

	/* proc.accounting.counts.mem */
	case 8:
	    atom->ull = (__uint64_t)(pracinfo_buf[j].pr_counts.ac_mem);
	    break;

	/* proc.accounting.counts.swaps */
	case 9:
	    atom->ull = (__uint64_t)(pracinfo_buf[j].pr_counts.ac_swaps);
	    break;

	/* proc.accounting.counts.chr */
	case 10:
	    atom->ull = (__uint64_t)(pracinfo_buf[j].pr_counts.ac_chr);
	    break;

	/* proc.accounting.counts.chw */
	case 11:
	    atom->ull = (__uint64_t)(pracinfo_buf[j].pr_counts.ac_chw);
	    break;

	/* proc.accounting.counts.br */
	case 12:
	    atom->ull = (__uint64_t)(pracinfo_buf[j].pr_counts.ac_br);
	    break;

	/* proc.accounting.counts.bw */
	case 13:
	    atom->ull = (__uint64_t)(pracinfo_buf[j].pr_counts.ac_bw);
	    break;

	/* proc.accounting.counts.syscr */
	case 14:
	    atom->ull = (__uint64_t)(pracinfo_buf[j].pr_counts.ac_syscr);
	    break;

	/* proc.accounting.counts.syscw */
	case 15:
	    atom->ull = (__uint64_t)(pracinfo_buf[j].pr_counts.ac_syscw);
	    break;

	/*
	 * NOTE: in accounting structure but not implemented in IRIX 6.1
	 * Always returns a large, constant value (12105675799276118504)
	 * This metric is commented out in the namespace.
	 */
	/* proc.accounting.counts.disk */
	case 16:
	    atom->ull = (__uint64_t)(pracinfo_buf[j].pr_counts.ac_disk);
	    break;

    }/*switch*/
#endif
    return 0;
}

/*
 * getinfo from ioctl
 */

#if !defined(IRIX5_3)
int
pracinfo_getbuf(pid_t pid, pracinfo_t *buf)
{
    int e=0;
    int fd;
    char *path;

    proc_pid_to_path(pid, NULL, &path, PINFO_PATH);

    if ((fd = open(path, O_RDONLY)) == -1)
	e = -errno;
    else if (ioctl(fd, PIOCACINFO, buf) == -1)
	e = -errno;
    if (fd != -1)
	(void)close(fd);
    return e;
}
#endif

#if defined(IRIX5_3)
/*ARGSUSED0*/
#endif
int
pracinfo_getinfo(pid_t pid, int j)
{
#if !defined(IRIX5_3)
    return pracinfo_getbuf(pid, &pracinfo_buf[j]);
#else
    return 0;
#endif
}


#if defined(IRIX5_3)
/*ARGSUSED0*/
#endif
int
pracinfo_allocbuf(int size)
{
#if !defined(IRIX5_3)
    static int max_size = 0;
    pracinfo_t *pb;

    if (size > max_size) {
	pb = realloc(pracinfo_buf, size * sizeof(pracinfo_t));  
	if (pb == NULL)
	    return -errno;
	pracinfo_buf = pb;
	max_size = size;
    }
#endif
    return 0;
}
