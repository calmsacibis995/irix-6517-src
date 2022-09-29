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

#ident "$Id: psinfo.c,v 1.9 1998/09/09 19:04:35 kenmcd Exp $"

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
#include "nameinfo.h"

/* whether metric exists for particular version or IRIX */
#define HAVE_PSINFO_CTIME !defined(IRIX5_3)
#define HAVE_PSINFO_SHAREUID !defined(IRIX5_3) && !defined(IRIX6_4)
#define HAVE_PSINFO_PSET BEFORE_IRIX6_4
#define HAVE_PSINFO_SPID !defined(IRIX5_3) && !defined(IRIX6_5)
#define HAVE_PSINFO_QTIME !defined(IRIX5_3) && !defined(IRIX6_5)
#define HAVE_PSINFO_THDS defined(IRIX6_5)
#define HAVE_PSINFO_WNAME defined(IRIX6_5)

extern char *get_ttyname_info(dev_t dev);

/* buffers for psinfo metrics, per pid */
static prpsinfo_t *psinfo_buf = NULL;

static pmDesc	desctab[] = {
    /* proc.psinfo.state */
    { PMID(1,0), PM_TYPE_32, PM_INDOM_PROC, PM_SEM_INSTANT, {0,0,0,0,0,0} },

    /* proc.psinfo.sname */
    { PMID(1,1), PM_TYPE_32, PM_INDOM_PROC, PM_SEM_INSTANT, {0,0,0,0,0,0} },

    /* proc.psinfo.zomb */
    { PMID(1,2), PM_TYPE_32, PM_INDOM_PROC, PM_SEM_INSTANT, {0,0,0,0,0,0} },

    /* proc.psinfo.nice */
    { PMID(1,3), PM_TYPE_32, PM_INDOM_PROC, PM_SEM_INSTANT, {0,0,0,0,0,0} },

    /* proc.psinfo.flag */
#if _MIPS_SZLONG == 64
    { PMID(1,4), PM_TYPE_U64, PM_INDOM_PROC, PM_SEM_INSTANT, {0,0,0,0,0,0} },
#elif _MIPS_SZLONG == 32
    { PMID(1,4), PM_TYPE_U32, PM_INDOM_PROC, PM_SEM_INSTANT, {0,0,0,0,0,0} },
#else
    bozo
#endif

    /* proc.psinfo.uid */
    { PMID(1,5), PM_TYPE_32, PM_INDOM_PROC, PM_SEM_INSTANT, {0,0,0,0,0,0} },

    /* proc.psinfo.gid */
    { PMID(1,6), PM_TYPE_32, PM_INDOM_PROC, PM_SEM_INSTANT, {0,0,0,0,0,0} },

    /* proc.psinfo.pid */
    { PMID(1,7), PM_TYPE_32, PM_INDOM_PROC, PM_SEM_DISCRETE, {0,0,0,0,0,0} },

    /* proc.psinfo.ppid */
    { PMID(1,8), PM_TYPE_32, PM_INDOM_PROC, PM_SEM_INSTANT, {0,0,0,0,0,0} },

    /* proc.psinfo.pgrp */
    { PMID(1,9), PM_TYPE_32, PM_INDOM_PROC, PM_SEM_INSTANT, {0,0,0,0,0,0} },

    /* proc.psinfo.sid */
    { PMID(1,10), PM_TYPE_32, PM_INDOM_PROC, PM_SEM_INSTANT, {0,0,0,0,0,0} },

    /* proc.psinfo.addr */
#if _MIPS_SZLONG == 64
    { PMID(1,11), PM_TYPE_U64, PM_INDOM_PROC, PM_SEM_INSTANT, {0,0,0,0,0,0} },
#elif _MIPS_SZLONG == 32
    { PMID(1,11), PM_TYPE_U32, PM_INDOM_PROC, PM_SEM_INSTANT, {0,0,0,0,0,0} },
#else
    bozo
#endif

    /* proc.psinfo.size */
#if _MIPS_SZLONG == 64
    { PMID(1,12), PM_TYPE_U64, PM_INDOM_PROC, PM_SEM_INSTANT, {1,0,0,PM_SPACE_KBYTE,0,0} },
#elif _MIPS_SZLONG == 32
    { PMID(1,12), PM_TYPE_U32, PM_INDOM_PROC, PM_SEM_INSTANT, {1,0,0,PM_SPACE_KBYTE,0,0} },
#else
    bozo
#endif

    /* proc.psinfo.rssize */
#if _MIPS_SZLONG == 64
    { PMID(1,13), PM_TYPE_U64, PM_INDOM_PROC, PM_SEM_INSTANT, {1,0,0,PM_SPACE_KBYTE,0,0} },
#elif _MIPS_SZLONG == 32
    { PMID(1,13), PM_TYPE_U32, PM_INDOM_PROC, PM_SEM_INSTANT, {1,0,0,PM_SPACE_KBYTE,0,0} },
#else
    bozo
#endif

    /* proc.psinfo.wchan */
#if _MIPS_SZLONG == 64
    { PMID(1,15), PM_TYPE_U64, PM_INDOM_PROC, PM_SEM_INSTANT, {0,0,0,0,0,0} },
#elif _MIPS_SZLONG == 32
    { PMID(1,15), PM_TYPE_U32, PM_INDOM_PROC, PM_SEM_INSTANT, {0,0,0,0,0,0} },
#else
    bozo
#endif

    /* proc.psinfo.start */
    { PMID(1,16), PM_TYPE_U32, PM_INDOM_PROC, PM_SEM_DISCRETE, {0,1,0,0,PM_TIME_SEC,0} },

    /* proc.psinfo.time */
    { PMID(1,17), PM_TYPE_U32, PM_INDOM_PROC, PM_SEM_COUNTER, {0,1,0,0,PM_TIME_MSEC,0} },

    /* proc.psinfo.pri */
#if _MIPS_SZLONG == 64
    { PMID(1,18), PM_TYPE_U64, PM_INDOM_PROC, PM_SEM_INSTANT, {0,0,0,0,0,0} },
#elif _MIPS_SZLONG == 32
    { PMID(1,18), PM_TYPE_U32, PM_INDOM_PROC, PM_SEM_INSTANT, {0,0,0,0,0,0} },
#else
    bozo
#endif

    /* proc.psinfo.oldpri */
#if _MIPS_SZLONG == 64
    { PMID(1,19), PM_TYPE_U64, PM_INDOM_PROC, PM_SEM_INSTANT, {0,0,0,0,0,0} },
#elif _MIPS_SZLONG == 32
    { PMID(1,19), PM_TYPE_U32, PM_INDOM_PROC, PM_SEM_INSTANT, {0,0,0,0,0,0} },
#else
    bozo
#endif

    /* proc.psinfo.cpu */
    { PMID(1,20), PM_TYPE_U32, PM_INDOM_PROC, PM_SEM_INSTANT, {0,0,0,0,0,0} },

    /* proc.psinfo.ttydev */
    { PMID(1,21), PM_TYPE_AGGREGATE, PM_INDOM_PROC, PM_SEM_DISCRETE, {0,0,0,0,0,0} },

    /* proc.psinfo.clname */
    { PMID(1,22), PM_TYPE_STRING, PM_INDOM_PROC, PM_SEM_INSTANT, {0,0,0,0,0,0} },

    /* proc.psinfo.fname */
    { PMID(1,23), PM_TYPE_STRING, PM_INDOM_PROC, PM_SEM_DISCRETE, {0,0,0,0,0,0} },

    /* proc.psinfo.psargs */
    { PMID(1,24), PM_TYPE_STRING, PM_INDOM_PROC, PM_SEM_DISCRETE, {0,0,0,0,0,0} },

    /* proc.psinfo.uname (string version of proc.psinfo.uid) */
    { PMID(1,25), PM_TYPE_STRING, PM_INDOM_PROC, PM_SEM_INSTANT, {0,0,0,0,0,0} },

    /* proc.psinfo.gname (string version of proc.psinfo.gid) */
    { PMID(1,26), PM_TYPE_STRING, PM_INDOM_PROC, PM_SEM_INSTANT, {0,0,0,0,0,0} },

    /* proc.psinfo.ttyname (string version of proc.psinfo.ttydev) */
    { PMID(1,27), PM_TYPE_STRING, PM_INDOM_PROC, PM_SEM_DISCRETE, {0,0,0,0,0,0} },

    /* proc.psinfo.ttymajor */
    { PMID(1,28), PM_TYPE_U32, PM_INDOM_PROC, PM_SEM_INSTANT, {0,0,0,0,0,0} },

    /* proc.psinfo.ttyminor */
    { PMID(1,29), PM_TYPE_U32, PM_INDOM_PROC, PM_SEM_INSTANT, {0,0,0,0,0,0} },

    /* proc.psinfo.ctime */
#if HAVE_PSINFO_CTIME
    { PMID(1,30), PM_TYPE_U32, PM_INDOM_PROC, PM_SEM_INSTANT, {0,1,0,0,PM_TIME_MSEC,0} },
#else
    { PMID(1,30), PM_TYPE_NOSUPPORT, PM_INDOM_PROC, PM_SEM_INSTANT, {0,1,0,0,PM_TIME_MSEC,0} },
#endif

    /* proc.psinfo.shareuid */
#if HAVE_PSINFO_SHAREUID
    { PMID(1,31), PM_TYPE_32, PM_INDOM_PROC, PM_SEM_INSTANT, {0,0,0,0,0,0} },
#else
    { PMID(1,31), PM_TYPE_NOSUPPORT, PM_INDOM_PROC, PM_SEM_INSTANT, {0,0,0,0,0,0} },
#endif

    /* proc.psinfo.pset */
#if HAVE_PSINFO_PSET
    { PMID(1,32), PM_TYPE_U32, PM_INDOM_PROC, PM_SEM_DISCRETE, {0,0,0,0,0,0} },
#else
    { PMID(1,32), PM_TYPE_NOSUPPORT, PM_INDOM_PROC, PM_SEM_DISCRETE, {0,0,0,0,0,0} },
#endif

    /* proc.psinfo.sonproc */
    { PMID(1,33), PM_TYPE_32, PM_INDOM_PROC, PM_SEM_INSTANT, {0,0,0,0,0,0} },

    /* proc.psinfo.spid */
#if HAVE_PSINFO_SPID
    { PMID(1,34), PM_TYPE_U32, PM_INDOM_PROC, PM_SEM_DISCRETE, {0,0,0,0,0,0} },
#else
    { PMID(1,34), PM_TYPE_NOSUPPORT, PM_INDOM_PROC, PM_SEM_DISCRETE, {0,0,0,0,0,0} },
#endif

    /* proc.psinfo.qtime */
#if HAVE_PSINFO_QTIME
    { PMID(1,35), PM_TYPE_U32, PM_INDOM_PROC, PM_SEM_COUNTER, {0,1,0,0,PM_TIME_MSEC,0} },
#else
    { PMID(1,35), PM_TYPE_NOSUPPORT, PM_INDOM_PROC, PM_SEM_COUNTER, {0,1,0,0,PM_TIME_MSEC,0} },
#endif

    /* proc.psinfo.thds */
#if HAVE_PSINFO_THDS
    { PMID(1,36), PM_TYPE_U32, PM_INDOM_PROC, PM_SEM_INSTANT, {0,0,0,0,0,0} },
#else
    { PMID(1,36), PM_TYPE_NOSUPPORT, PM_INDOM_PROC, PM_SEM_INSTANT, {0,0,0,0,0,0} },
#endif

    /* proc.psinfo.wname */
#if HAVE_PSINFO_WNAME
    { PMID(1,37), PM_TYPE_STRING, PM_INDOM_PROC, PM_SEM_INSTANT, {0,0,0,0,0,0} },
#else
    { PMID(1,37), PM_TYPE_NOSUPPORT, PM_INDOM_PROC, PM_SEM_INSTANT, {0,0,0,0,0,0} },
#endif

};

static int      ndesc = (sizeof(desctab)/sizeof(desctab[0]));

void
psinfo_init(int dom)
{
  init_table(ndesc, desctab, dom);
}

int 
psinfo_getdesc(pmID pmid, pmDesc *desc)
{
    return getdesc(ndesc, desctab, pmid, desc);
}


int
psinfo_setatom(int item, pmAtomValue *atom, int j)
{
    int aggregate_len = 0;

    switch (item) {

    /* proc.psinfo.state */
    case 0:
	atom->l = (__int32_t)(psinfo_buf[j].pr_state);
	break;

    /* proc.psinfo.sname */
    case 1:
	atom->l = (__int32_t)(psinfo_buf[j].pr_sname);
	break;

    /* proc.psinfo.zomb */
    case 2:
	atom->l = (__int32_t)(psinfo_buf[j].pr_zomb);
	break;

    /* proc.psinfo.nice */
    case 3:
	atom->l = (__int32_t)(psinfo_buf[j].pr_nice);
	break;

    /* proc.psinfo.flag */
    case 4:
	set_atom_ulong(atom, psinfo_buf[j].pr_flag);
	break;

    /* proc.psinfo.uid */
    case 5:
	atom->l = psinfo_buf[j].pr_uid;
	break;

    /* proc.psinfo.gid */
    case 6:
	atom->l = psinfo_buf[j].pr_gid;
	break;

    /* proc.psinfo.pid */
    case 7:
	atom->l = psinfo_buf[j].pr_pid;
	break;

    /* proc.psinfo.ppid */
    case 8:
	atom->l = psinfo_buf[j].pr_ppid;
	break;

    /* proc.psinfo.pgrp */
    case 9:
	atom->l = psinfo_buf[j].pr_pgrp;
	break;

    /* proc.psinfo.sid */
    case 10:
	atom->l = psinfo_buf[j].pr_sid;
	break;

    /* proc.psinfo.addr */
    case 11:
	set_atom_ulong(atom, psinfo_buf[j].pr_addr);
	break;

    /* proc.psinfo.size */
    case 12:
	set_atom_ulong(atom, psinfo_buf[j].pr_size * _pagesize / 1024);
	break;

    /* proc.psinfo.rssize */
    case 13:
	set_atom_ulong(atom, psinfo_buf[j].pr_rssize * _pagesize / 1024);
	break;

    /* proc.psinfo.wchan */
    case 15:
	set_atom_ulong(atom, psinfo_buf[j].pr_wchan);
	break;

    /* proc.psinfo.start */
    case 16:
	atom->ul = (__uint32_t)(psinfo_buf[j].pr_start.tv_sec + NSEC_SEC(psinfo_buf[j].pr_start.tv_nsec));
	break;

    /* proc.psinfo.time */
    case 17:
	atom->ul = (__uint32_t)(psinfo_buf[j].pr_time.tv_sec*1000 + NSEC_MSEC(psinfo_buf[j].pr_time.tv_nsec));
	break;

    /* proc.psinfo.pri */
    case 18:
	set_atom_ulong(atom, psinfo_buf[j].pr_pri);
	break;

    /* proc.psinfo.oldpri */
    case 19:
	set_atom_ulong(atom, psinfo_buf[j].pr_oldpri);
	break;

    /* proc.psinfo.cpu */
    case 20:
	set_atom_ulong(atom, psinfo_buf[j].pr_cpu);
	break;

    /* proc.psinfo.ttydev */
    case 21:
	atom->vp = &psinfo_buf[j].pr_ttydev;
	aggregate_len = sizeof(dev_t);
	break;

    /* proc.psinfo.clname */
    case 22:
	atom->cp = (char *)&psinfo_buf[j].pr_clname[0];
	aggregate_len = (unsigned int)strlen(atom->cp);
	break;

    /* proc.psinfo.fname */
    case 23:
	atom->cp = (char *)&psinfo_buf[j].pr_fname[0];
	aggregate_len = (unsigned int)strlen(atom->cp);
	break;

    /* proc.psinfo.psargs */
    case 24:
	atom->cp = &psinfo_buf[j].pr_psargs[0];
	aggregate_len = (unsigned int)strlen(atom->cp);
	break;

    /* proc.psinfo.uname */
    case 25:
	atom->cp = get_uname_info(psinfo_buf[j].pr_uid);
	aggregate_len = (unsigned int)strlen(atom->cp);
	break;

    /* proc.psinfo.gname */
    case 26:
	atom->cp = get_gname_info(psinfo_buf[j].pr_gid);
	aggregate_len = (unsigned int)strlen(atom->cp);
	break;

    /* proc.psinfo.ttyname */
    case 27:
	atom->cp = get_ttyname_info(psinfo_buf[j].pr_ttydev);
	aggregate_len = (unsigned int)strlen(atom->cp);
	break;

    /* proc.psinfo.ttymajor */
    case 28:
	atom->ul = major(psinfo_buf[j].pr_ttydev);
	break;

    /* proc.psinfo.ttyminor */
    case 29:
	atom->ul = minor(psinfo_buf[j].pr_ttydev);
	break;

    /* proc.psinfo.ctime */
    case 30:
#if HAVE_PSINFO_CTIME
	atom->ul = (__uint32_t)(psinfo_buf[j].pr_ctime.tv_sec*1000 + NSEC_MSEC(psinfo_buf[j].pr_ctime.tv_nsec));
#endif
	break;

    /* proc.psinfo.shareuid */
    case 31:
#if HAVE_PSINFO_SHAREUID
	atom->l = psinfo_buf[j].pr_shareuid;
#endif
	break;

    /* proc.psinfo.pset */
    case 32:
#if HAVE_PSINFO_PSET
	atom->ul = psinfo_buf[j].pr_pset;
#endif
	break;

    /* proc.psinfo.sonproc */
    case 33:
	atom->l = psinfo_buf[j].pr_sonproc;
	break;

    /* proc.psinfo.spid */
    case 34:
#if HAVE_PSINFO_SPID
	atom->ul = psinfo_buf[j].pr_spid;
#endif
	break;

    /* proc.psinfo.qtime */
    case 35:
#if HAVE_PSINFO_QTIME
	atom->ul = (__uint32_t)(psinfo_buf[j].pr_qtime.tv_sec*1000 +
	    NSEC_MSEC(psinfo_buf[j].pr_qtime.tv_nsec));
#endif
	break;

    /* proc.psinfo.thds */
    case 36:
#if HAVE_PSINFO_THDS
	atom->ul = psinfo_buf[j].pr_thds;
#endif
	break;

    /* proc.psinfo.wname */
    case 37:
#if HAVE_PSINFO_WNAME
	atom->cp = (char *)&psinfo_buf[j].pr_wname[0];
	aggregate_len = (unsigned int)strlen(atom->cp);
#endif
	break;

    }

    return aggregate_len;

}

/*
 * getinfo from ioctl
 */

int
psinfo_getbuf(pid_t pid, prpsinfo_t *buf)
{
    int e=0;
    int fd;
    char *path;

    proc_pid_to_path(pid, NULL, &path, PINFO_PATH);

    if ((fd = open(path, O_RDONLY)) == -1)
	e = -errno;
    else if (ioctl(fd, PIOCPSINFO, buf) == -1)
	e = -errno;
    if (fd != -1)
	(void)close(fd);
    return e;
}

int
psinfo_getinfo(pid_t pid, int j)
{
    return psinfo_getbuf(pid, &psinfo_buf[j]);
}


int
psinfo_allocbuf(int size)
{
    static int max_size = 0;
    prpsinfo_t *pb;

    if (size > max_size) {
	pb = realloc(psinfo_buf, size * sizeof(prpsinfo_t));  
	if (pb == NULL)
	    return -errno;
	psinfo_buf = pb;
	max_size = size;
    }

    return 0;
}
