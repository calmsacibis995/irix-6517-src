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

#ident "$Id: pmemory.c,v 1.5 1998/09/09 19:04:35 kenmcd Exp $"

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
#include "procmem.h"
#include "proc.h"
#include "proc_aux.h"
#include "cluster.h"


static _pmProcMem_t *pmem_buf = NULL;

static pmDesc	desctab[] = {
    /* proc.memory.physical.txt */
#if _MIPS_SZLONG == 64
    { PMID(5,0), PM_TYPE_U64, PM_INDOM_PROC, PM_SEM_INSTANT, {1,0,0,PM_SPACE_KBYTE,0,0} },
#elif _MIPS_SZLONG == 32
    { PMID(5,0), PM_TYPE_U32, PM_INDOM_PROC, PM_SEM_INSTANT, {1,0,0,PM_SPACE_KBYTE,0,0} },
#else
    bozo
#endif

    /* proc.memory.virtual.txt */
#if _MIPS_SZLONG == 64
    { PMID(5,1), PM_TYPE_U64, PM_INDOM_PROC, PM_SEM_INSTANT, {1,0,0,PM_SPACE_KBYTE,0,0} },
#elif _MIPS_SZLONG == 32
    { PMID(5,1), PM_TYPE_U32, PM_INDOM_PROC, PM_SEM_INSTANT, {1,0,0,PM_SPACE_KBYTE,0,0} },
#else
    bozo
#endif

    /* proc.memory.physical.dat */
#if _MIPS_SZLONG == 64
    { PMID(5,2), PM_TYPE_U64, PM_INDOM_PROC, PM_SEM_INSTANT, {1,0,0,PM_SPACE_KBYTE,0,0} },
#elif _MIPS_SZLONG == 32
    { PMID(5,2), PM_TYPE_U32, PM_INDOM_PROC, PM_SEM_INSTANT, {1,0,0,PM_SPACE_KBYTE,0,0} },
#else
    bozo
#endif

    /* proc.memory.virtual.dat */
#if _MIPS_SZLONG == 64
    { PMID(5,3), PM_TYPE_U64, PM_INDOM_PROC, PM_SEM_INSTANT, {1,0,0,PM_SPACE_KBYTE,0,0} },
#elif _MIPS_SZLONG == 32
    { PMID(5,3), PM_TYPE_U32, PM_INDOM_PROC, PM_SEM_INSTANT, {1,0,0,PM_SPACE_KBYTE,0,0} },
#else
    bozo
#endif

    /* proc.memory.physical.bss */
#if _MIPS_SZLONG == 64
    { PMID(5,4), PM_TYPE_U64, PM_INDOM_PROC, PM_SEM_INSTANT, {1,0,0,PM_SPACE_KBYTE,0,0} },
#elif _MIPS_SZLONG == 32
    { PMID(5,4), PM_TYPE_U32, PM_INDOM_PROC, PM_SEM_INSTANT, {1,0,0,PM_SPACE_KBYTE,0,0} },
#else
    bozo
#endif

    /* proc.memory.virtual.bss */
#if _MIPS_SZLONG == 64
    { PMID(5,5), PM_TYPE_U64, PM_INDOM_PROC, PM_SEM_INSTANT, {1,0,0,PM_SPACE_KBYTE,0,0} },
#elif _MIPS_SZLONG == 32
    { PMID(5,5), PM_TYPE_U32, PM_INDOM_PROC, PM_SEM_INSTANT, {1,0,0,PM_SPACE_KBYTE,0,0} },
#else
    bozo
#endif

    /* proc.memory.physical.stack */
#if _MIPS_SZLONG == 64
    { PMID(5,6), PM_TYPE_U64, PM_INDOM_PROC, PM_SEM_INSTANT, {1,0,0,PM_SPACE_KBYTE,0,0} },
#elif _MIPS_SZLONG == 32
    { PMID(5,6), PM_TYPE_U32, PM_INDOM_PROC, PM_SEM_INSTANT, {1,0,0,PM_SPACE_KBYTE,0,0} },
#else
    bozo
#endif

    /* proc.memory.virtual.stack */
#if _MIPS_SZLONG == 64
    { PMID(5,7), PM_TYPE_U64, PM_INDOM_PROC, PM_SEM_INSTANT, {1,0,0,PM_SPACE_KBYTE,0,0} },
#elif _MIPS_SZLONG == 32
    { PMID(5,7), PM_TYPE_U32, PM_INDOM_PROC, PM_SEM_INSTANT, {1,0,0,PM_SPACE_KBYTE,0,0} },
#else
    bozo
#endif

    /* proc.memory.physical.shm */
#if _MIPS_SZLONG == 64
    { PMID(5,8), PM_TYPE_U64, PM_INDOM_PROC, PM_SEM_INSTANT, {1,0,0,PM_SPACE_KBYTE,0,0} },
#elif _MIPS_SZLONG == 32
    { PMID(5,8), PM_TYPE_U32, PM_INDOM_PROC, PM_SEM_INSTANT, {1,0,0,PM_SPACE_KBYTE,0,0} },
#else
    bozo
#endif

    /* proc.memory.virtual.shm */
#if _MIPS_SZLONG == 64
    { PMID(5,9), PM_TYPE_U64, PM_INDOM_PROC, PM_SEM_INSTANT, {1,0,0,PM_SPACE_KBYTE,0,0} }
#elif _MIPS_SZLONG == 32
    { PMID(5,9), PM_TYPE_U32, PM_INDOM_PROC, PM_SEM_INSTANT, {1,0,0,PM_SPACE_KBYTE,0,0} }
#else
    bozo
#endif

};

static int      ndesc = (sizeof(desctab)/sizeof(desctab[0]));

void
pmem_init(int dom)
{
    init_table(ndesc, desctab, dom);
}

int 
pmem_getdesc(pmID pmid, pmDesc *desc)
{
    return getdesc(ndesc, desctab, pmid, desc);
}




int
pmem_setatom(int item, pmAtomValue *atom, int j)
{
    switch (item) {
	/* proc.memory.physical.txt */
	case 0:
	    set_atom_ulong(atom, pmem_buf[j].ptxt);
	    break;

	/* proc.memory.virtual.txt */
	case 1:
	    set_atom_ulong(atom, pmem_buf[j].vtxt);
	    break;

	/* proc.memory.physical.dat */
	case 2:
	    set_atom_ulong(atom, pmem_buf[j].pdat);
	    break;

	/* proc.memory.virtual.dat */
	case 3:
	    set_atom_ulong(atom, pmem_buf[j].vdat);
	    break;

	/* proc.memory.physical.bss */
	case 4:
	    set_atom_ulong(atom, pmem_buf[j].pbss);
	    break;

	/* proc.memory.virtual.bss */
	case 5:
	    set_atom_ulong(atom, pmem_buf[j].vbss);
	    break;

	/* proc.memory.physical.stack */
	case 6:
	    set_atom_ulong(atom, pmem_buf[j].pstk);
	    break;

	/* proc.memory.virtual.stack */
	case 7:
	    set_atom_ulong(atom, pmem_buf[j].vstk);
	    break;

	/* proc.memory.physical.shm */
	case 8:
	    set_atom_ulong(atom, pmem_buf[j].pshm);
	    break;

	/* proc.memory.virtual.shm */
	case 9:
	    set_atom_ulong(atom, pmem_buf[j].vshm);
	    break;

    }/*switch*/

    return 0;
}

/*
 * getinfo from ioctl
 */

int
pmem_getbuf(pid_t pid, _pmProcMem_t *buf)
{
    int e=0;
    int fd;
    char *path;

    proc_pid_to_path(pid, NULL, &path, PROC_PATH);

    if ((fd = open(path, O_RDONLY)) == -1)
	e = -errno;
    else 
	e = _pmProcMem(fd, buf);
    if (fd != -1)
	(void)close(fd);
    return e;
}

int
pmem_getinfo(pid_t pid, int j)
{
    return pmem_getbuf(pid, &pmem_buf[j]);
}


int
pmem_allocbuf(int size)
{
    static int max_size = 0;
    _pmProcMem_t *pb;

    if (size > max_size) {
	pb = realloc(pmem_buf, size * sizeof(_pmProcMem_t));  
	if (pb == NULL)
	    return -errno;
	pmem_buf = pb;
	max_size = size;
    }

    return 0;
}
