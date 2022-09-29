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

#ident "$Id: psusage.c,v 1.7 1998/09/09 19:04:35 kenmcd Exp $"

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

#include "pmapi.h"
#include "proc.h"
#include "proc_aux.h"
#include "cluster.h"

static prusage_t *psrusage_buf = NULL;

static pmDesc	desctab[] = {
    /* proc.psusage.tstamp */
    { PMID(4,0), PM_TYPE_U32, PM_INDOM_PROC, PM_SEM_INSTANT, {0,1,0,0,PM_TIME_SEC,0} },

    /* proc.psusage.starttime */
    { PMID(4,1), PM_TYPE_U32, PM_INDOM_PROC, PM_SEM_DISCRETE, {0,1,0,0,PM_TIME_SEC,0} },

    /* proc.psusage.utime */
    { PMID(4,2), PM_TYPE_U32, PM_INDOM_PROC, PM_SEM_COUNTER, {0,1,0,0,PM_TIME_MSEC,0} },

    /* proc.psusage.stime */
    { PMID(4,3), PM_TYPE_U32, PM_INDOM_PROC, PM_SEM_COUNTER, {0,1,0,0,PM_TIME_MSEC,0} },

    /* proc.psusage.minf */
#if _MIPS_SZLONG == 64
    { PMID(4,4), PM_TYPE_U64, PM_INDOM_PROC, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} },
#elif _MIPS_SZLONG == 32
    { PMID(4,4), PM_TYPE_U32, PM_INDOM_PROC, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} },
#else
    bozo
#endif

    /* proc.psusage.majf */
#if _MIPS_SZLONG == 64
    { PMID(4,5), PM_TYPE_U64, PM_INDOM_PROC, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} },
#elif _MIPS_SZLONG == 32
    { PMID(4,5), PM_TYPE_U32, PM_INDOM_PROC, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} },
#else
    bozo
#endif

    /* proc.psusage.utlb */
#if _MIPS_SZLONG == 64
    { PMID(4,6), PM_TYPE_U64, PM_INDOM_PROC, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} },
#elif _MIPS_SZLONG == 32
    { PMID(4,6), PM_TYPE_U32, PM_INDOM_PROC, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} },
#else
    bozo
#endif

    /* proc.psusage.nswap */
#if _MIPS_SZLONG == 64
    { PMID(4,7), PM_TYPE_U64, PM_INDOM_PROC, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} },
#elif _MIPS_SZLONG == 32
    { PMID(4,7), PM_TYPE_U32, PM_INDOM_PROC, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} },
#else
    bozo
#endif

    /* proc.psusage.gbread */
#if _MIPS_SZLONG == 64
    { PMID(4,8), PM_TYPE_U64, PM_INDOM_PROC, PM_SEM_COUNTER, {1,0,0,PM_SPACE_GBYTE,0,0} },
#elif _MIPS_SZLONG == 32
    { PMID(4,8), PM_TYPE_U32, PM_INDOM_PROC, PM_SEM_COUNTER, {1,0,0,PM_SPACE_GBYTE,0,0} },
#else
    bozo
#endif

    /* proc.psusage.bread */
#if _MIPS_SZLONG == 64
    { PMID(4,9), PM_TYPE_U64, PM_INDOM_PROC, PM_SEM_COUNTER, {1,0,0,PM_SPACE_BYTE,0,0} },
#elif _MIPS_SZLONG == 32
    { PMID(4,9), PM_TYPE_U32, PM_INDOM_PROC, PM_SEM_COUNTER, {1,0,0,PM_SPACE_BYTE,0,0} },
#else
    bozo
#endif

    /* proc.psusage.gbwrit */
#if _MIPS_SZLONG == 64
    { PMID(4,10), PM_TYPE_U64, PM_INDOM_PROC, PM_SEM_COUNTER, {1,0,0,PM_SPACE_GBYTE,0,0} },
#elif _MIPS_SZLONG == 32
    { PMID(4,10), PM_TYPE_U32, PM_INDOM_PROC, PM_SEM_COUNTER, {1,0,0,PM_SPACE_GBYTE,0,0} },
#else
    bozo
#endif

    /* proc.psusage.bwrit */
#if _MIPS_SZLONG == 64
    { PMID(4,11), PM_TYPE_U64, PM_INDOM_PROC, PM_SEM_COUNTER, {1,0,0,PM_SPACE_BYTE,0,0} },
#elif _MIPS_SZLONG == 32
    { PMID(4,11), PM_TYPE_U32, PM_INDOM_PROC, PM_SEM_COUNTER, {1,0,0,PM_SPACE_BYTE,0,0} },
#else
    bozo
#endif

    /* proc.psusage.sigs */
#if _MIPS_SZLONG == 64
    { PMID(4,12), PM_TYPE_U64, PM_INDOM_PROC, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} },
#elif _MIPS_SZLONG == 32
    { PMID(4,12), PM_TYPE_U32, PM_INDOM_PROC, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} },
#else
    bozo
#endif

    /* proc.psusage.vctx */
#if _MIPS_SZLONG == 64
    { PMID(4,13), PM_TYPE_U64, PM_INDOM_PROC, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} },
#elif _MIPS_SZLONG == 32
    { PMID(4,13), PM_TYPE_U32, PM_INDOM_PROC, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} },
#else
    bozo
#endif

    /* proc.psusage.ictx */
#if _MIPS_SZLONG == 64
    { PMID(4,14), PM_TYPE_U64, PM_INDOM_PROC, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} },
#elif _MIPS_SZLONG == 32
    { PMID(4,14), PM_TYPE_U32, PM_INDOM_PROC, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} },
#else
    bozo
#endif

    /* proc.psusage.sysc */
#if _MIPS_SZLONG == 64
    { PMID(4,15), PM_TYPE_U64, PM_INDOM_PROC, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} },
#elif _MIPS_SZLONG == 32
    { PMID(4,15), PM_TYPE_U32, PM_INDOM_PROC, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} },
#else
    bozo
#endif

    /* proc.psusage.syscr */
#if _MIPS_SZLONG == 64
    { PMID(4,16), PM_TYPE_U64, PM_INDOM_PROC, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} },
#elif _MIPS_SZLONG == 32
    { PMID(4,16), PM_TYPE_U32, PM_INDOM_PROC, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} },
#else
    bozo
#endif

    /* proc.psusage.syscw */
#if _MIPS_SZLONG == 64
    { PMID(4,17), PM_TYPE_U64, PM_INDOM_PROC, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} },
#elif _MIPS_SZLONG == 32
    { PMID(4,17), PM_TYPE_U32, PM_INDOM_PROC, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} },
#else
    bozo
#endif

    /* proc.psusage.syscps */
#if _MIPS_SZLONG == 64
    { PMID(4,18), PM_TYPE_U64, PM_INDOM_PROC, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} },
#elif _MIPS_SZLONG == 32
    { PMID(4,18), PM_TYPE_U32, PM_INDOM_PROC, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} },
#else
    bozo
#endif

    /* proc.psusage.sysci */
#if _MIPS_SZLONG == 64
    { PMID(4,19), PM_TYPE_U64, PM_INDOM_PROC, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} },
#elif _MIPS_SZLONG == 32
    { PMID(4,19), PM_TYPE_U32, PM_INDOM_PROC, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} },
#else
    bozo
#endif

    /* proc.psusage.graphfifo */
#if _MIPS_SZLONG == 64
    { PMID(4,20), PM_TYPE_U64, PM_INDOM_PROC, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} },
#elif _MIPS_SZLONG == 32
    { PMID(4,20), PM_TYPE_U32, PM_INDOM_PROC, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} },
#else
    bozo
#endif

    /* proc.psusage.graph_req */
    { PMID(4,21), PM_TYPE_AGGREGATE, PM_INDOM_PROC, PM_SEM_INSTANT, {0,0,0,0,0,0} },

    /* proc.psusage.graph_wait */
    { PMID(4,22), PM_TYPE_AGGREGATE, PM_INDOM_PROC, PM_SEM_INSTANT, {0,0,0,0,0,0} },

    /* proc.psusage.size */
#if _MIPS_SZLONG == 64
    { PMID(4,23), PM_TYPE_U64, PM_INDOM_PROC, PM_SEM_INSTANT, {1,0,0,PM_SPACE_KBYTE,0,0} },
#elif _MIPS_SZLONG == 32
    { PMID(4,23), PM_TYPE_U32, PM_INDOM_PROC, PM_SEM_INSTANT, {1,0,0,PM_SPACE_KBYTE,0,0} },
#else
    bozo
#endif

    /* proc.psusage.rss */
#if _MIPS_SZLONG == 64
    { PMID(4,24), PM_TYPE_U64, PM_INDOM_PROC, PM_SEM_INSTANT, {1,0,0,PM_SPACE_KBYTE,0,0} },
#elif _MIPS_SZLONG == 32
    { PMID(4,24), PM_TYPE_U32, PM_INDOM_PROC, PM_SEM_INSTANT, {1,0,0,PM_SPACE_KBYTE,0,0} },
#else
    bozo
#endif

    /* proc.psusage.inblock */
#if _MIPS_SZLONG == 64
    { PMID(4,25), PM_TYPE_U64, PM_INDOM_PROC, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} },
#elif _MIPS_SZLONG == 32
    { PMID(4,25), PM_TYPE_U32, PM_INDOM_PROC, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} },
#else
    bozo
#endif

    /* proc.psusage.oublock */
#if _MIPS_SZLONG == 64
    { PMID(4,26), PM_TYPE_U64, PM_INDOM_PROC, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} },
#elif _MIPS_SZLONG == 32
    { PMID(4,26), PM_TYPE_U32, PM_INDOM_PROC, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} },
#else
    bozo
#endif

    /* proc.psusage.ktlb */
#if !defined(IRIX5_3)
#if _MIPS_SZLONG == 64
    { PMID(4,27), PM_TYPE_U64, PM_INDOM_PROC, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} },
#elif _MIPS_SZLONG == 32
    { PMID(4,27), PM_TYPE_U32, PM_INDOM_PROC, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} },
#endif
#else
    { PMID(4,27), PM_TYPE_NOSUPPORT, PM_INDOM_PROC, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} },
#endif

    /* proc.psusage.vfault */
#if _MIPS_SZLONG == 64
    { PMID(4,28), PM_TYPE_U64, PM_INDOM_PROC, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} },
#elif _MIPS_SZLONG == 32
    { PMID(4,28), PM_TYPE_U32, PM_INDOM_PROC, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} },
#else
    bozo
#endif


};

static int      ndesc = (sizeof(desctab)/sizeof(desctab[0]));

void
psusage_init(int dom)
{
    init_table(ndesc, desctab, dom);
}

int 
psusage_getdesc(pmID pmid, pmDesc *desc)
{
    return getdesc(ndesc, desctab, pmid, desc);
}


int
psusage_setatom(int item, pmAtomValue *atom, int j)
{
    int aggregate_len = 0;

    switch (item) {
	/* proc.psusage.tstamp */
	case 0:
	    atom->ul = (__uint32_t)(psrusage_buf[j].pu_tstamp.tv_sec + NSEC_SEC(psrusage_buf[j].pu_tstamp.tv_nsec));
	    break;

	/* proc.psusage.starttime */
	case 1:
	    atom->ul = (__uint32_t)(psrusage_buf[j].pu_starttime.tv_sec + NSEC_SEC(psrusage_buf[j].pu_starttime.tv_nsec));
	    break;

	/* proc.psusage.utime */
	case 2:
	    atom->ul = (__uint32_t)(psrusage_buf[j].pu_utime.tv_sec*1000 + NSEC_MSEC(psrusage_buf[j].pu_utime.tv_nsec));
	    break;

	/* proc.psusage.stime */
	case 3:
	    atom->ul = (__uint32_t)(psrusage_buf[j].pu_stime.tv_sec*1000 + NSEC_MSEC(psrusage_buf[j].pu_stime.tv_nsec));
	    break;

	/* proc.psusage.minf */
	case 4:
	    set_atom_ulong(atom, psrusage_buf[j].pu_minf);
	    break;

	/* proc.psusage.majf */
	case 5:
	    set_atom_ulong(atom, psrusage_buf[j].pu_majf);
	    break;

	/* proc.psusage.utlb */
	case 6:
	    set_atom_ulong(atom, psrusage_buf[j].pu_utlb);
	    break;

	/* proc.psusage.nswap */
	case 7:
	    set_atom_ulong(atom, psrusage_buf[j].pu_nswap);
	    break;

	/* proc.psusage.gbread */
	case 8:
	    set_atom_ulong(atom, psrusage_buf[j].pu_gbread);
	    break;

	/* proc.psusage.bread */
	case 9:
	    set_atom_ulong(atom, psrusage_buf[j].pu_bread);
	    break;

	/* proc.psusage.gbwrit */
	case 10:
	    set_atom_ulong(atom, psrusage_buf[j].pu_gbwrit);
	    break;

	/* proc.psusage.bwrit */
	case 11:
	    set_atom_ulong(atom, psrusage_buf[j].pu_bwrit);
	    break;

	/* proc.psusage.sigs */
	case 12:
	    set_atom_ulong(atom, psrusage_buf[j].pu_sigs);
	    break;

	/* proc.psusage.vctx */
	case 13:
	    set_atom_ulong(atom, psrusage_buf[j].pu_vctx);
	    break;

	/* proc.psusage.ictx */
	case 14:
	    set_atom_ulong(atom, psrusage_buf[j].pu_ictx);
	    break;

	/* proc.psusage.sysc */
	case 15:
	    set_atom_ulong(atom, psrusage_buf[j].pu_sysc);
	    break;

	/* proc.psusage.syscr */
	case 16:
	    set_atom_ulong(atom, psrusage_buf[j].pu_syscr);
	    break;

	/* proc.psusage.syscw */
	case 17:
	    set_atom_ulong(atom, psrusage_buf[j].pu_syscw);
	    break;

	/* proc.psusage.syscps */
	case 18:
	    set_atom_ulong(atom, psrusage_buf[j].pu_syscps);
	    break;

	/* proc.psusage.sysci */
	case 19:
	    set_atom_ulong(atom, psrusage_buf[j].pu_sysci);
	    break;

	/* proc.psusage.graphfifo */
	case 20:
	    set_atom_ulong(atom, psrusage_buf[j].pu_graphfifo);
	    break;

	/* proc.psusage.graph_req */
	case 21:
	    atom->vp = &psrusage_buf[j].pu_graph_req[0];
	    aggregate_len = sizeof(psrusage_buf[j].pu_graph_req);
	    break;

	/* proc.psusage.graph_wait */
	case 22:
	    atom->vp = &psrusage_buf[j].pu_graph_wait[0];
	    aggregate_len = sizeof(psrusage_buf[j].pu_graph_wait);
	    break;

	/* proc.psusage.size */
	case 23:
	    set_atom_ulong(atom, psrusage_buf[j].pu_size * _pagesize / 1024);
	    break;

	/* proc.psusage.rss */
	case 24:
	    set_atom_ulong(atom, psrusage_buf[j].pu_rss * _pagesize / 1024);
	    break;

	/* proc.psusage.inblock */
	case 25:
	    set_atom_ulong(atom, psrusage_buf[j].pu_inblock);
	    break;

	/* proc.psusage.oublock */
	case 26:
	    set_atom_ulong(atom, psrusage_buf[j].pu_oublock);
	    break;

#if !defined(IRIX5_3)
	/* proc.psusage.ktlb */
	case 27:
	    set_atom_ulong(atom, psrusage_buf[j].pu_ktlb);
	    break;
#endif

	/* proc.psusage.vfault */
	case 28:
	    set_atom_ulong(atom, psrusage_buf[j].pu_vfault);
	    break;

    }
    return aggregate_len;
}

/*
 * getinfo from ioctl
 */

int
psusage_getbuf(pid_t pid, prusage_t *buf)
{
    int e=0;
    int fd;
    char *path;

    proc_pid_to_path(pid, NULL, &path, PINFO_PATH);

    if ((fd = open(path, O_RDONLY)) == -1)
	e = -errno;
    else if (ioctl(fd, PIOCUSAGE, buf) == -1)
	e = -errno;
    if (fd != -1)
	(void)close(fd);
    return e;
}

int
psusage_getinfo(pid_t pid, int j)
{
    return psusage_getbuf(pid, &psrusage_buf[j]);
}


int
psusage_allocbuf(int size)
{
    static int max_size = 0;
    prusage_t *pb;

    if (size > max_size) {
	pb = realloc(psrusage_buf, size * sizeof(prusage_t));  
	if (pb == NULL)
	    return -errno;
	psrusage_buf = pb;
	max_size = size;
    }

    return 0;
}
