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

#ident "$Id: pstatus.c,v 1.8 1998/09/09 19:04:35 kenmcd Exp $"

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

#define HAVE_PSTATUS_NTHREADS defined(IRIX6_5)
#define HAVE_PSTATUS_THSIGPEND defined(IRIX6_5)
#define HAVE_PSTATUS_WHO defined(IRIX6_5)

/* buffers for psinfo metrics, per pid */
static prstatus_t *pstatus_buf = NULL;

static pmDesc	desctab[] = {
    /* proc.pstatus.flags */
#if _MIPS_SZLONG == 64
    { PMID(2,0), PM_TYPE_U64, PM_INDOM_PROC, PM_SEM_INSTANT, {0,0,0,0,0,0} },
#elif _MIPS_SZLONG == 32
    { PMID(2,0), PM_TYPE_U32, PM_INDOM_PROC, PM_SEM_INSTANT, {0,0,0,0,0,0} },
#else
    bozo
#endif

    /* proc.pstatus.why */
    { PMID(2,1), PM_TYPE_32, PM_INDOM_PROC, PM_SEM_INSTANT, {0,0,0,0,0,0} },

    /* proc.pstatus.what */
    { PMID(2,2), PM_TYPE_32, PM_INDOM_PROC, PM_SEM_INSTANT, {0,0,0,0,0,0} },

    /* proc.pstatus.cursig */
    { PMID(2,3), PM_TYPE_32, PM_INDOM_PROC, PM_SEM_INSTANT, {0,0,0,0,0,0} },

    /* proc.pstatus.sigpend */
    { PMID(2,4), PM_TYPE_AGGREGATE, PM_INDOM_PROC, PM_SEM_INSTANT, {0,0,0,0,0,0} },

    /* proc.pstatus.sighold */
    { PMID(2,5), PM_TYPE_AGGREGATE, PM_INDOM_PROC, PM_SEM_INSTANT, {0,0,0,0,0,0} },

    /* proc.pstatus.info */
    { PMID(2,6), PM_TYPE_AGGREGATE, PM_INDOM_PROC, PM_SEM_INSTANT, {0,0,0,0,0,0} },

    /* proc.pstatus.altstack */
    { PMID(2,7), PM_TYPE_AGGREGATE, PM_INDOM_PROC, PM_SEM_INSTANT, {0,0,0,0,0,0} },

    /* proc.pstatus.action */
    { PMID(2,8), PM_TYPE_AGGREGATE, PM_INDOM_PROC, PM_SEM_INSTANT, {0,0,0,0,0,0} },

    /* proc.pstatus.syscall */
#if _MIPS_SZLONG == 64
    { PMID(2,9), PM_TYPE_64, PM_INDOM_PROC, PM_SEM_INSTANT, {0,0,0,0,0,0} },
#elif _MIPS_SZLONG == 32
    { PMID(2,9), PM_TYPE_32, PM_INDOM_PROC, PM_SEM_INSTANT, {0,0,0,0,0,0} },
#else
    bozo
#endif

    /* proc.pstatus.nsysarg */
#if _MIPS_SZLONG == 64
    { PMID(2,10), PM_TYPE_64, PM_INDOM_PROC, PM_SEM_INSTANT, {0,0,0,0,0,0} },
#elif _MIPS_SZLONG == 32
    { PMID(2,10), PM_TYPE_32, PM_INDOM_PROC, PM_SEM_INSTANT, {0,0,0,0,0,0} },
#else
    bozo
#endif

    /* proc.pstatus.errno */
#if _MIPS_SZLONG == 64
    { PMID(2,11), PM_TYPE_64, PM_INDOM_PROC, PM_SEM_INSTANT, {0,0,0,0,0,0} },
#elif _MIPS_SZLONG == 32
    { PMID(2,11), PM_TYPE_32, PM_INDOM_PROC, PM_SEM_INSTANT, {0,0,0,0,0,0} },
#else
    bozo
#endif

    /* proc.pstatus.rval1 */
#if _MIPS_SZLONG == 64
    { PMID(2,12), PM_TYPE_64, PM_INDOM_PROC, PM_SEM_INSTANT, {0,0,0,0,0,0} },
#elif _MIPS_SZLONG == 32
    { PMID(2,12), PM_TYPE_32, PM_INDOM_PROC, PM_SEM_INSTANT, {0,0,0,0,0,0} },
#else
    bozo
#endif

    /* proc.pstatus.rval2 */
#if _MIPS_SZLONG == 64
    { PMID(2,13), PM_TYPE_64, PM_INDOM_PROC, PM_SEM_INSTANT, {0,0,0,0,0,0} },
#elif _MIPS_SZLONG == 32
    { PMID(2,13), PM_TYPE_32, PM_INDOM_PROC, PM_SEM_INSTANT, {0,0,0,0,0,0} },
#else
    bozo
#endif

    /* proc.pstatus.sysarg */
    { PMID(2,14), PM_TYPE_AGGREGATE, PM_INDOM_PROC, PM_SEM_INSTANT, {0,0,0,0,0,0} },

    /* proc.pstatus.pid */
    { PMID(2,15), PM_TYPE_32, PM_INDOM_PROC, PM_SEM_DISCRETE, {0,0,0,0,0,0} },

    /* proc.pstatus.ppid */
    { PMID(2,16), PM_TYPE_32, PM_INDOM_PROC, PM_SEM_INSTANT, {0,0,0,0,0,0} },

    /* proc.pstatus.pgrp */
    { PMID(2,17), PM_TYPE_32, PM_INDOM_PROC, PM_SEM_INSTANT, {0,0,0,0,0,0} },

    /* proc.pstatus.sid */
    { PMID(2,18), PM_TYPE_32, PM_INDOM_PROC, PM_SEM_INSTANT, {0,0,0,0,0,0} },

    /* proc.pstatus.utime */
    { PMID(2,19), PM_TYPE_U32, PM_INDOM_PROC, PM_SEM_COUNTER, {0,1,0,0,PM_TIME_MSEC,0} },

    /* proc.pstatus.stime */
    { PMID(2,20), PM_TYPE_U32, PM_INDOM_PROC, PM_SEM_COUNTER, {0,1,0,0,PM_TIME_MSEC,0} },

    /* proc.pstatus.cutime */
    { PMID(2,21), PM_TYPE_U32, PM_INDOM_PROC, PM_SEM_COUNTER, {0,1,0,0,PM_TIME_MSEC,0} },

    /* proc.pstatus.cstime */
    { PMID(2,22), PM_TYPE_U32, PM_INDOM_PROC, PM_SEM_COUNTER, {0,1,0,0,PM_TIME_MSEC,0} },

    /* proc.pstatus.clname */
    { PMID(2,23), PM_TYPE_STRING, PM_INDOM_PROC, PM_SEM_INSTANT, {0,0,0,0,0,0} },

    /* proc.pstatus.instr */
#if _MIPS_SZLONG == 64
    { PMID(2,24), PM_TYPE_64, PM_INDOM_PROC, PM_SEM_INSTANT, {0,0,0,0,0,0} },
#elif _MIPS_SZLONG == 32
    { PMID(2,24), PM_TYPE_32, PM_INDOM_PROC, PM_SEM_INSTANT, {0,0,0,0,0,0} },
#else
    bozo
#endif

    /* proc.pstatus.reg */
    { PMID(2,25), PM_TYPE_AGGREGATE, PM_INDOM_PROC, PM_SEM_INSTANT, {0,0,0,0,0,0} },

    /* proc.pstatus.nthreads */
#if HAVE_PSTATUS_NTHREADS
    { PMID(2,26), PM_TYPE_32, PM_INDOM_PROC, PM_SEM_INSTANT, {0,0,0,0,0,0} },
#else
    { PMID(2,26), PM_TYPE_NOSUPPORT, PM_INDOM_PROC, PM_SEM_INSTANT, {0,0,0,0,0,0} },
#endif

    /* proc.pstatus.thsigpend */
#if HAVE_PSTATUS_THSIGPEND
    { PMID(2,27), PM_TYPE_AGGREGATE, PM_INDOM_PROC, PM_SEM_INSTANT, {0,0,0,0,0,0} },
#else
    { PMID(2,27), PM_TYPE_NOSUPPORT, PM_INDOM_PROC, PM_SEM_INSTANT, {0,0,0,0,0,0} },
#endif

    /* proc.pstatus.who */
#if HAVE_PSTATUS_WHO
    { PMID(2,28), PM_TYPE_32, PM_INDOM_PROC, PM_SEM_INSTANT, {0,0,0,0,0,0} },
#else
    { PMID(2,28), PM_TYPE_NOSUPPORT, PM_INDOM_PROC, PM_SEM_INSTANT, {0,0,0,0,0,0} },
#endif


};

static int      ndesc = (sizeof(desctab)/sizeof(desctab[0]));

void
pstatus_init(int dom)
{
    init_table(ndesc, desctab, dom);
}

int 
pstatus_getdesc(pmID pmid, pmDesc *desc)
{
    return getdesc(ndesc, desctab, pmid, desc);
}




int
pstatus_setatom(int item, pmAtomValue *atom, int j)
{
    int aggregate_len = 0;

    switch (item) {

	/* proc.pstatus.flags */
	case 0:
	    set_atom_ulong(atom, pstatus_buf[j].pr_flags);
	    break;

	/* proc.pstatus.why */
	case 1:
	    atom->l = (__int32_t)(pstatus_buf[j].pr_why);
	    break;

	/* proc.pstatus.what */
	case 2:
	    atom->l = (__int32_t)(pstatus_buf[j].pr_what);
	    break;

	/* proc.pstatus.cursig */
	case 3:
	    atom->l = (__int32_t)(pstatus_buf[j].pr_cursig);
	    break;

	/* proc.pstatus.sigpend */
	case 4:
	    atom->vp = &pstatus_buf[j].pr_sigpend;
	    aggregate_len = sizeof(pstatus_buf[j].pr_sigpend);
	    break;

	/* proc.pstatus.sighold */
	case 5:
	    atom->vp = &pstatus_buf[j].pr_sighold;
	    aggregate_len = sizeof(pstatus_buf[j].pr_sighold);
	    break;

	/* proc.pstatus.info */
	case 6:
	    atom->vp = &pstatus_buf[j].pr_info;
	    aggregate_len = sizeof(pstatus_buf[j].pr_info);
	    break;

	/* proc.pstatus.altstack */
	case 7:
	    atom->vp = &pstatus_buf[j].pr_altstack;
	    aggregate_len = sizeof(pstatus_buf[j].pr_altstack);
	    break;

	/* proc.pstatus.action */
	case 8:
	    atom->vp = &pstatus_buf[j].pr_action;
	    aggregate_len = sizeof(pstatus_buf[j].pr_action);
	    break;

	/* proc.pstatus.syscall */
	case 9:
	    set_atom_long(atom, pstatus_buf[j].pr_syscall);
	    break;

	/* proc.pstatus.nsysarg */
	case 10:
	    set_atom_long(atom, pstatus_buf[j].pr_nsysarg);
	    break;

	/* proc.pstatus.errno */
	case 11:
	    set_atom_long(atom, pstatus_buf[j].pr_errno);
	    break;

	/* proc.pstatus.rval1 */
	case 12:
	    set_atom_long(atom, pstatus_buf[j].pr_rval1);
	    break;

	/* proc.pstatus.rval2 */
	case 13:
	    set_atom_long(atom, pstatus_buf[j].pr_rval2);
	    break;

	/* proc.pstatus.sysarg */
	case 14:
	    atom->vp = &pstatus_buf[j].pr_sysarg;
	    aggregate_len = sizeof(pstatus_buf[j].pr_sysarg);
	    break;

	/* proc.pstatus.pid */
	case 15:
	    atom->l = (__int32_t)(pstatus_buf[j].pr_pid);
	    break;

	/* proc.pstatus.ppid */
	case 16:
	    atom->l = (__int32_t)(pstatus_buf[j].pr_ppid);
	    break;

	/* proc.pstatus.pgrp */
	case 17:
	    atom->l = (__int32_t)(pstatus_buf[j].pr_pgrp);
	    break;

	/* proc.pstatus.sid */
	case 18:
	    atom->l = (__int32_t)(pstatus_buf[j].pr_sid);
	    break;

	/* proc.pstatus.utime */
	case 19:
	    atom->ul = (__uint32_t)(pstatus_buf[j].pr_utime.tv_sec*1000 + NSEC_MSEC(pstatus_buf[j].pr_utime.tv_nsec));
	    break;

	/* proc.pstatus.stime */
	case 20:
	    atom->ul = (__uint32_t)(pstatus_buf[j].pr_stime.tv_sec*1000 + NSEC_MSEC(pstatus_buf[j].pr_stime.tv_nsec));
	    break;

	/* proc.pstatus.cutime */
	case 21:
	    atom->ul = (__uint32_t)(pstatus_buf[j].pr_cutime.tv_sec*1000 + NSEC_MSEC(pstatus_buf[j].pr_cutime.tv_nsec));
	    break;

	/* proc.pstatus.cstime */
	case 22:
	    atom->ul = (__uint32_t)(pstatus_buf[j].pr_cstime.tv_sec*1000 + NSEC_MSEC(pstatus_buf[j].pr_cstime.tv_nsec));
	    break;

	/* proc.pstatus.clname */
	case 23:
	    atom->cp = &pstatus_buf[j].pr_clname[0];
	    aggregate_len = (unsigned int)strlen(atom->cp);
	    break;

	/* proc.pstatus.instr */
	case 24:
	    set_atom_long(atom, pstatus_buf[j].pr_instr);
	    break;

	/* proc.pstatus.reg */
	case 25:
	    atom->vp = &pstatus_buf[j].pr_reg;
	    aggregate_len = sizeof(pstatus_buf[j].pr_reg);
	    break;

	/* proc.pstatus.nthreads */
	case 26:
#if HAVE_PSTATUS_NTHREADS
	    atom->l = (__int32_t)(pstatus_buf[j].pr_nthreads);
#endif
	    break;

	/* proc.pstatus.thsigpend */
	case 27:
#if HAVE_PSTATUS_THSIGPEND
	    atom->vp = &pstatus_buf[j].pr_thsigpend;
	    aggregate_len = (unsigned int)sizeof(sigset_t);
#endif
	    break;

	/* proc.pstatus.who */
	case 28:
#if HAVE_PSTATUS_WHO
	    atom->l = (__int32_t)(pstatus_buf[j].pr_who);
#endif
	    break;

    }/*switch*/
    return aggregate_len;
}

/*
 * getinfo from ioctl
 */

int
pstatus_getbuf(pid_t pid, prstatus_t *buf)
{
    int e=0;
    int fd;
    char *path;

    proc_pid_to_path(pid, NULL, &path, PROC_PATH);

    if ((fd = open(path, O_RDONLY)) == -1)
	e = -errno;
    else if (ioctl(fd, PIOCSTATUS, buf) == -1)
	e = -errno;
    if (fd != -1)
	(void)close(fd);
    return e;
}

int
pstatus_getinfo(pid_t pid, int j)
{
    return pstatus_getbuf(pid, &pstatus_buf[j]);
}


int
pstatus_allocbuf(int size)
{
    static int max_size = 0;
    prstatus_t *pb;

    if (size > max_size) {
	pb = realloc(pstatus_buf, size * sizeof(prstatus_t));  
	if (pb == NULL)
	    return -errno;
	pstatus_buf = pb;
	max_size = size;
    }

    return 0;
}
