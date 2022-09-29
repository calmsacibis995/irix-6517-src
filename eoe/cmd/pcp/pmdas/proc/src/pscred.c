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

#ident "$Id: pscred.c,v 1.5 1998/09/09 19:04:35 kenmcd Exp $"

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

static prcred_t *pscred_buf = NULL;

static pmDesc	desctab[] = {
    /* proc.pscred.euid */
    { PMID(3,0), PM_TYPE_32, PM_INDOM_PROC, PM_SEM_INSTANT, {0,0,0,0,0,0} },

    /* proc.pscred.ruid */
    { PMID(3,1), PM_TYPE_32, PM_INDOM_PROC, PM_SEM_INSTANT, {0,0,0,0,0,0} },

    /* proc.pscred.suid */
    { PMID(3,2), PM_TYPE_32, PM_INDOM_PROC, PM_SEM_INSTANT, {0,0,0,0,0,0} },

    /* proc.pscred.egid */
    { PMID(3,3), PM_TYPE_32, PM_INDOM_PROC, PM_SEM_INSTANT, {0,0,0,0,0,0} },

    /* proc.pscred.rgid */
    { PMID(3,4), PM_TYPE_32, PM_INDOM_PROC, PM_SEM_INSTANT, {0,0,0,0,0,0} },

    /* proc.pscred.sgid */
    { PMID(3,5), PM_TYPE_32, PM_INDOM_PROC, PM_SEM_INSTANT, {0,0,0,0,0,0} },

    /* proc.pscred.ngroups */
    { PMID(3,6), PM_TYPE_U32, PM_INDOM_PROC, PM_SEM_INSTANT, {0,0,0,0,0,0} }

};

static int      ndesc = (sizeof(desctab)/sizeof(desctab[0]));

void
pscred_init(int dom)
{
    init_table(ndesc, desctab, dom);
}

int 
pscred_getdesc(pmID pmid, pmDesc *desc)
{
    return getdesc(ndesc, desctab, pmid, desc);
}




int
pscred_setatom(int item, pmAtomValue *atom, int j)
{
    switch (item) {
	/* proc.pscred.euid */
	case 0:
	    atom->l = pscred_buf[j].pr_euid;
	    break;

	/* proc.pscred.ruid */
	case 1:
	    atom->l = pscred_buf[j].pr_ruid;
	    break;

	/* proc.pscred.suid */
	case 2:
	    atom->l = pscred_buf[j].pr_suid;
	    break;

	/* proc.pscred.egid */
	case 3:
	    atom->l = pscred_buf[j].pr_egid;
	    break;

	/* proc.pscred.rgid */
	case 4:
	    atom->l = pscred_buf[j].pr_rgid;
	    break;

	/* proc.pscred.sgid */
	case 5:
	    atom->l = pscred_buf[j].pr_sgid;
	    break;

	/* proc.pscred.ngroups */
	case 6:
	    atom->ul = (__uint32_t)pscred_buf[j].pr_ngroups;
	    break;

    }/*switch*/
    return 0;
}

/*
 * getinfo from ioctl
 */

int
pscred_getbuf(pid_t pid, prcred_t *buf)
{
    int e=0;
    int fd;
    char *path;

    proc_pid_to_path(pid, NULL, &path, PINFO_PATH);

    if ((fd = open(path, O_RDONLY)) == -1)
	e = -errno;
    else if (ioctl(fd, PIOCCRED, buf) == -1)
	e = -errno;
    if (fd != -1)
	(void)close(fd);
    return e;
}

int
pscred_getinfo(pid_t pid, int j)
{
    return pscred_getbuf(pid, &pscred_buf[j]);
}


int
pscred_allocbuf(int size)
{
    static int max_size = 0;
    prcred_t *pb;

    if (size > max_size) {
	pb = realloc(pscred_buf, size * sizeof(prcred_t));  
	if (pb == NULL)
	    return -errno;
	pscred_buf = pb;
	max_size = size;
    }

    return 0;
}
