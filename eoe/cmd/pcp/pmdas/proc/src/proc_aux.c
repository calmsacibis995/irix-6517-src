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

#ident "$Id: proc_aux.c,v 1.8 1998/09/09 19:04:35 kenmcd Exp $"

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
#include "impl.h"
#include "proc.h"
#include "proc_aux.h"
#include "cluster.h"

int	_pagesize;
pmInDom	proc_indom = 0;

#include "pglobal.h"
#include "pmemory.h"
#include "pracinfo.h"
#include "pscred.h"
#include "psinfo.h"
#include "pstatus.h"
#include "psusage.h"
#include "ctltab.h"


int 
getdesc(int ndesc, pmDesc desctab[], pmID pmid, pmDesc *desc)
{
    int         i;

    for (i = 0; i < ndesc; i++) {
        if (pmid == desctab[i].pmid) {
            *desc = desctab[i];     /* struct assignment */
            break;
        }
    }
    if (i == ndesc)
        return PM_ERR_PMID;
    else if (desc->type == PM_TYPE_NOSUPPORT)
	return PM_ERR_APPVERSION;
    else
	return 0;
}

/*
 * Make all characters from psargs printable
 * The resultant buf string can grow up to 4 times the args string
 * if it contained all non-printable characters converted to octal.
 */
static void
psargs_string(int len, char *buf, char *args)
{
    char ch, *p, *b;

    for (b=buf, p=args; b < buf+len-4 && *p != '\0'; p++) {
	ch = *p;

        switch(ch) {
	    case '\n': (void)strcpy(b, "\\n"); b += 2; break;
	    case '\r': (void)strcpy(b, "\\r"); b += 2; break;
	    case '\t': (void)strcpy(b, "\\t"); b += 2; break;
	    default:
		if (isprint((int)ch)) {
		    *b++ = ch;
		}
		else {
		    (void)sprintf(b, "\\%03o", ch);
		    b += 4;
		}
        }
    }
    *b = '\0';
}

/*
 * Get either path or filename for a process.
 * Use either /proc/pinfo or /proc depending on the setting of kind.
 */
void
proc_pid_to_path(pid_t pid, char **fname, char **path, path_kind kind)
{
    static char fnamebuf[PROC_FNAME_SIZE];
    static char pathbuf[PROC_PATH_SIZE];

    /* find the name for the given indom/instance */
    (void)sprintf(fnamebuf, proc_fmt, pid);
    if (fname)
        *fname = fnamebuf;
    if (path) {
	if (kind == PINFO_PATH)
	    (void)sprintf(pathbuf, "%s/%s", PROCFS_INFO, fnamebuf);
	else
	    (void)sprintf(pathbuf, "%s/%s", PROCFS, fnamebuf);
        *path = pathbuf;
    }
}

/*
 * Convert an instance id (pid) to instance name
 * for the full instance map
 */

int
proc_id_to_mapname(int id, char **name)
{
    prpsinfo_t info;
    static char args[INSTMAP_NAME_SIZE];
    static char inamebuf[INSTMAP_NAME_SIZE];
    static char fnamebuf[PROC_FNAME_SIZE];

    (void)sprintf(fnamebuf, proc_fmt, id);
    if (psinfo_getbuf(id, &info) == 0)
	psargs_string(sizeof(args), args, info.pr_psargs);
    else
	(void)strcpy(args, "<defunct>");
    (void)sprintf(inamebuf, "%s %s", fnamebuf, args);
    if ((*name = strdup(inamebuf)) == NULL) {
	return -errno;
    }
    return 0;
}

/*
 * Convert an instance id (pid) to instance name
 * for the one-way lookup
 */

int
proc_id_to_name(int id, char **name)
{
    struct stat	statbuf;
    char *fname, *path;

    proc_pid_to_path((pid_t)id, &fname, &path, PINFO_PATH);

    if (stat(path, &statbuf) < 0) {
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL0) {
	    __pmNotifyErr(LOG_ERR, "proc_id_to_name: unknown pid: %d :"
			    "(fname = %s) (path = %s) (errmsg = %s)\n", 
			    id, fname, path, pmErrStr(-errno));
	}
#endif
	return PM_ERR_INST;
    }

    if (name != NULL) {
	if ((*name = strdup(fname)) == NULL)
	    return -errno;
    }

    return 0;
}

/* 
 * find the instance id for the given indom/name
 */

int
proc_name_to_id(char *name, int *id)
{
    int sts;
    char *s, *t;
    char idname[PROC_FNAME_SIZE];
    int n;

    /* --- extract id from name --- */
    /* skip over leading white space */
    for (s = name; *s != '\0' && isspace(*s); s++)
        ;
    /* only read proc_entry_len digits */
    for (t = s, n = 0; *t != '\0' && !isspace(*t); t++, n++) {
	if (!isdigit(*t))
	    return PM_ERR_INST;
        if (n >= proc_entry_len)
	    return PM_ERR_INST;
	idname[n] = *t;
    }
    if (n == 0)
	return PM_ERR_INST;
    idname[n] = '\0';


    *id = atoi(idname);
    if ((sts = proc_id_to_name(*id, NULL)) < 0) {
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL0) {
	    __pmNotifyErr(LOG_ERR, "proc_name_to_id: failure for pid = %d, name = %s\n", id, name);
	}
#endif
	return sts;
    }
    return 0;
}

/*
 * This routine is called at initialization to patch up any parts of the
 * desctab that cannot be statically initialized, and to optionally
 * modify our Performance Metrics Domain Id (dom)
 */
void
init_table(int ndesc, pmDesc desctab[], int dom)
{
    int		i;
    __pmID_int	*pmidp;

    ((__pmInDom_int *)&(proc_indom))->domain = dom;
    /* merge performance domain id part into PMIDs, indoms in pmDesc table */
    for (i = 0; i < ndesc; i++) {
	pmidp = (__pmID_int *)&desctab[i].pmid;
	pmidp->domain = dom;
	if (desctab[i].indom != PM_INDOM_NULL)
	    desctab[i].indom = proc_indom;
    }

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL0) {
	(void)fprintf(stderr, "init_table(%d, *, %d):\n", ndesc, dom);
	(void)fprintf(stderr, "------------\n");
        for (i = 0; i < ndesc; i++) {
	    pmidp = (__pmID_int *)&desctab[i].pmid;
            (void)fprintf(stderr, "[%d]  <%d,%d,%d>\n", i, 
                    pmidp->domain, pmidp->cluster, pmidp->item);
	}
	(void)fprintf(stderr, "------------\n");
    }
#endif

}

void
init_tables(int dom)
{
    int i;

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL0) {
	(void)fprintf(stderr, "init_tables(%d)\n", dom);
    }
#endif

    for (i=0; i < nctltab; i++) {
	ctltab[i].init(dom);
    }

    _pagesize = getpagesize();
}
