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

#ident "$Id: pmcd.c,v 2.46 1999/08/17 04:13:41 kenmcd Exp $"

#include <unistd.h>
#include <stdio.h>
#include <syslog.h>
#include <errno.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <dirent.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <time.h>
#include "pmapi.h"
#include "impl.h"
#include "pmda.h"
#include "pmcd/src/pmcd.h"
#include "pmcd/src/client.h"
#include "pmie/src/pmiestats.h"

#if defined(IRIX6_5)
#include <optional_sym.h>
#endif

/*
 * To debug with dbpmda, add this so the symbols from pmcd(1) will
 * be faked out:
 *
 * #define DEBUG_WITH_DBPMDA
 */

#define PMCD			2

/*
 * Note: strange numbering for pmcd.pdu_{in,out}.total for
 * compatibility with earlier PCP versions ... this is the "item"
 * field of the PMID
 */
#define _TOTAL			16

extern int		errno;
extern unsigned int	__pmPDUCntIn[];
extern unsigned int	__pmPDUCntOut[];
extern int		pmDebug;	/* pmcd's debug flag */

/* from pmcd's address space, not in headers */
extern int		_pmcd_done;	/* pending request to terminate */
extern char		*_pmcd_data;	/* base size of data */
extern int		_pmcd_trace_nbufs;	/* number of trace buffers */

/* encode the domain(x), cluster (y) and item (z) parts of the PMID */
#ifdef HAVE_NETWORK_BYTEORDER
#define PMID(x,y) ((x<<10)|y)
#else
#define PMID(x,y) ((x<<10)|(y<<22))
#endif

/*
 * all metrics supported in this PMD - one table entry for each
 */
static pmDesc	desctab[] = {
/* control.debug */
    { PMID(0,0), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_DISCRETE, {0,0,0,0,0,0} },
/* datasize */
    { PMID(0,1), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, {1,0,0,PM_SPACE_KBYTE,0,0} },
/* numagents */
    { PMID(0,2), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, {0,0,0,0,0,0} },
/* numclients */
    { PMID(0,3), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, {0,0,0,0,0,0} },
/* control.timeout */
    { PMID(0,4), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE, {0,0,0,0,0,0} },
/* timezone -- local $TZ -- for pmlogger */
    { PMID(0,5), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_DISCRETE, {0,0,0,0,0,0} },
/* simabi -- Subprogram Interface Model ABI version of this pmcd */
    { PMID(0,6), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_DISCRETE, {0,0,0,0,0,0} },
/* version -- pmcd version */
    { PMID(0,7), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_DISCRETE, {0,0,0,0,0,0} },
/* control.register -- bulletin board */
    { PMID(0,8), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_DISCRETE, {0,0,0,0,0,0} },
/* control.traceconn -- trace connections */
    { PMID(0,9), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_DISCRETE, {0,0,0,0,0,0} },
/* control.tracepdu -- trace PDU traffic */
    { PMID(0,10), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_DISCRETE, {0,0,0,0,0,0} },
/* control.tracebufs -- number of trace buffers */
    { PMID(0,11), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_DISCRETE, {0,0,0,0,0,0} },
/* control.dumptrace -- push-button, pmStore to dump trace */
    { PMID(0,12), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_DISCRETE, {0,0,0,0,0,0} },
/* control.dumpconn -- push-button, pmStore to dump connections */
    { PMID(0,13), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_DISCRETE, {0,0,0,0,0,0} },
/* control.tracenobuf -- unbuffered tracing  */
    { PMID(0,14), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_DISCRETE, {0,0,0,0,0,0} },
/* control.sighup -- push-button, pmStore to SIGHUP pmcd */
    { PMID(0,15), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_DISCRETE, {0,0,0,0,0,0} },
/* license -- bit-vector of license capabilities */
    { PMID(0,16), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_DISCRETE, {0,0,0,0,0,0} },
/* openfds -- number of open file descriptors */
    { PMID(0,17), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, {0,0,0,0,0,0} },
/* buf.alloc */
    { PMID(0,18), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, {0,0,1,0,0,PM_COUNT_ONE} },
/* buf.free */
    { PMID(0,19), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, {0,0,1,0,0,PM_COUNT_ONE} },

/* pdu-in.error */
    { PMID(1,0), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} },
/* pdu-in.result */
    { PMID(1,1), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} },
/* pdu-in.profile */
    { PMID(1,2), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} },
/* pdu-in.fetch */
    { PMID(1,3), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} },
/* pdu-in.desc-req */
    { PMID(1,4), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} },
/* pdu-in.desc */
    { PMID(1,5), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} },
/* pdu-in.instance-req */
    { PMID(1,6), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} },
/* pdu-in.instance */
    { PMID(1,7), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} },
/* pdu-in.text-req */
    { PMID(1,8), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} },
/* pdu-in.text */
    { PMID(1,9), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} },
/* pdu-in.control-req */
    { PMID(1,10), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} },
/* pdu-in.datax */
    { PMID(1,11), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} },
/* pdu-in.creds */
    { PMID(1,12), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} },
/* pdu-in.pmns-ids */
    { PMID(1,13), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} },
/* pdu-in.pmns-names */
    { PMID(1,14), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} },
/* pdu-in.pmns-child */
    { PMID(1,15), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} },
/* pdu-in.total */
    { PMID(1,_TOTAL), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} },
/* pdu-in.pmns-traverse */
    { PMID(1,17), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} },

/* pdu-out.error */
    { PMID(2,0), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} },
/* pdu-out.result */
    { PMID(2,1), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} },
/* pdu-out.profile */
    { PMID(2,2), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} },
/* pdu-out.fetch */
    { PMID(2,3), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} },
/* pdu-out.desc-req */
    { PMID(2,4), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} },
/* pdu-out.desc */
    { PMID(2,5), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} },
/* pdu-out.instance-req */
    { PMID(2,6), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} },
/* pdu-out.instance */
    { PMID(2,7), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} },
/* pdu-out.text-req */
    { PMID(2,8), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} },
/* pdu-out.text */
    { PMID(2,9), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} },
/* pdu-out.control-req */
    { PMID(2,10), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} },
/* pdu-out.datax */
    { PMID(2,11), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} },
/* pdu-out.creds */
    { PMID(2,12), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} },
/* pdu-out.pmns-ids */
    { PMID(2,13), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} },
/* pdu-out.pmns-names */
    { PMID(2,14), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} },
/* pdu-out.pmns-child */
    { PMID(2,15), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} },
/* pdu-out.total */
    { PMID(2,_TOTAL), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} },
/* pdu-out.pmns-traverse */
    { PMID(2,17), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} },

/* pmlogger.port */
    { PMID(3,0), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE, {0,0,0,0,0,0} },
/* pmlogger.pmcd_host */
    { PMID(3,1), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_DISCRETE, {0,0,0,0,0,0} },
/* pmlogger.archive */
    { PMID(3,2), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_DISCRETE, {0,0,0,0,0,0} },
/* pmlogger.host */
    { PMID(3,3), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_DISCRETE, {0,0,0,0,0,0} },

/* agent.type */
    { PMID(4,0), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE, {0,0,0,0,0,0} },
/* agent.status */
    { PMID(4,1), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_DISCRETE, {0,0,0,0,0,0} },

/* pmie.configfile */
    { PMID(5,0), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_DISCRETE, {0,0,0,0,0,0} },
/* pmie.logfile */
    { PMID(5,1), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_DISCRETE, {0,0,0,0,0,0} },
/* pmie.pmcd_host */
    { PMID(5,2), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_DISCRETE, {0,0,0,0,0,0} },
/* pmie.numrules */
    { PMID(5,3), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE, {0,0,0,0,0,0} },
/* pmie.actions */
    { PMID(5,4), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} },
/* pmie.eval.true */
    { PMID(5,5), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} },
/* pmie.eval.false */
    { PMID(5,6), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} },
/* pmie.eval.unknown */
    { PMID(5,7), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} },
/* pmie.eval.expected */
    { PMID(5,8), PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_DISCRETE, {0,-1,1,0,PM_TIME_SEC,PM_COUNT_ONE} },
/* pmie.eval.actual */
    { PMID(5,9), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, {0,0,1,0,0,PM_COUNT_ONE} },
/* End-of-List */
    { PM_ID_NULL, 0, 0, 0, { 0, 0, 0, 0, 0, 0 } }
};
static int		ndesc = sizeof(desctab)/sizeof(desctab[0]);

static __pmProfile	*_profile;	/* last received profile */

/* there are four instance domains: pmlogger, register, PMDA, and pmie */
#define INDOM_PMLOGGERS	1
static pmInDom		logindom;
#define INDOM_REGISTER	2
static pmInDom		regindom;
#define INDOM_PMDAS	3
static pmInDom		pmdaindom;
#define INDOM_PMIES	4
static pmInDom		pmieindom;
#define INDOM_POOL	5
static pmInDom		bufindom;

#define NUMREG 16
static int		reg[NUMREG];

typedef struct {
    pid_t	pid;
    char	*name;
    void	*mmap;
} pmie_t;
static pmie_t		*pmies;
static unsigned int	npmies;

static struct {
    int		inst;
    char	*iname;
} bufinst[] = {
    {	  12,	"0012" },
    {	  20,	"0020" },
    {	1024,	"1024" },
    {	2048,	"2048" },
    {	4196,	"4196" },
    {	8192,	"8192" },
    {   8193,	"8192+" },
};
static int	nbufsz = sizeof(bufinst) / sizeof(bufinst[0]);

#if defined(HAVE_PROCFS_PINFO)
#define PROCFS_INFO	"/proc/pinfo"	/* from proc pmda - proc.h */
#elif defined(HAVE_PROCFS)
#define PROCFS_INFO	"/proc"
#else
!bozo!
#endif
#define PROCFS_ENTRYLEN	20		/* from proc pmda - proc.h */
#define PROCFS_PATHLEN	(sizeof(PROCFS_INFO)+PROCFS_ENTRYLEN)

static int
exists_process(pid_t pid)
{
    static char proc_buf[PROCFS_PATHLEN];

    sprintf(proc_buf, "%s/%u", PROCFS_INFO, pid);
    return (access(proc_buf, F_OK) == 0);
}

/*
 * this routine is called at initialization to patch up any parts of the
 * desctab that cannot be statically initialized, and to optionally
 * modify our Performance Metrics Domain Id (dom)
 */
static void
init_tables(int dom)
{
    int			i;
    __pmID_int		*pmidp;
    __pmInDom_int	*indomp;

    /* set domain in instance domain correctly */
    indomp = (__pmInDom_int *)&logindom;
    indomp->pad = 0;
    indomp->domain = dom;
    indomp->serial = INDOM_PMLOGGERS;
    indomp = (__pmInDom_int *)&regindom;
    indomp->pad = 0;
    indomp->domain = dom;
    indomp->serial = INDOM_REGISTER;
    indomp = (__pmInDom_int *)&pmdaindom;
    indomp->pad = 0;
    indomp->domain = dom;
    indomp->serial = INDOM_PMDAS;
    indomp = (__pmInDom_int *)&pmieindom;
    indomp->pad = 0;
    indomp->domain = dom;
    indomp->serial = INDOM_PMIES;
    indomp = (__pmInDom_int *)&bufindom;
    indomp->pad = 0;
    indomp->domain = dom;
    indomp->serial = INDOM_POOL;

    /* merge performance domain id part into PMIDs in pmDesc table */
    for (i = 0; desctab[i].pmid != PM_ID_NULL; i++) {
	pmidp = (__pmID_int *)&desctab[i].pmid;
	pmidp->domain = dom;
	if (pmidp->cluster == 0 && pmidp->item == 8)
	    desctab[i].indom = regindom;
	else if (pmidp->cluster == 0 && (pmidp->item == 18 || pmidp->item == 19))
	    desctab[i].indom = bufindom;
	else if (pmidp->cluster == 3)
	    desctab[i].indom = logindom;
	else if (pmidp->cluster == 4)
	    desctab[i].indom = pmdaindom;
	else if (pmidp->cluster == 5)
	    desctab[i].indom = pmieindom;
    }
    ndesc--;
}


/*ARGSUSED*/
static int
pmcd_profile(__pmProfile *prof, pmdaExt *pmda)
{
    _profile = prof;	
    return 0;
}

/* use a static timestamp, stat /var/tmp/pmie, if changed update "pmies" */
static unsigned int
refresh_pmie_indom(void)
{
    static struct stat	lastsbuf;
    pid_t		pmiepid;
    struct dirent	*dp;
    struct stat		statbuf;
    size_t		size;
    char		*endp;
    char		fullpath[MAXPATHLEN];
    void		*ptr;
    DIR			*pmiedir;
    int			fd;

    if (stat(PMIE_DIR, &statbuf) == 0) {
#if defined(HAVE_ST_MTIME_WITH_E) && defined(HAVE_STAT_TIME_T)
	if (statbuf.st_mtime != lastsbuf.st_mtime) {
#elif defined(HAVE_STAT_TIMESTRUC) || defined(HAVE_STAT_TIMESPEC)
	if ((statbuf.st_mtim.tv_sec != lastsbuf.st_mtim.tv_sec) ||
	    (statbuf.st_mtim.tv_nsec != lastsbuf.st_mtim.tv_nsec)) {
#else
!bozo!
#endif
	    lastsbuf = statbuf;

	    /* tear down the old instance domain */
	    if (pmies) {
		free(pmies);
		pmies = NULL;
	    }
	    npmies = 0;

	    /* open the directory iterate through mmaping as we go */
	    if ((pmiedir = opendir(PMIE_DIR)) == NULL) {
		__pmNotifyErr(LOG_ERR, "pmcd pmda cannot open %s: %s",
				PMIE_DIR, strerror(errno));
		return 0;
	    }
	    /* NOTE:  all valid files are already mmapped by pmie */
	    while ((dp = readdir(pmiedir)) != NULL) {
		size = (npmies+1) * sizeof(pmie_t);
		pmiepid = (pid_t)strtoul(dp->d_name, &endp, 10);
		if (*endp != '\0')	/* skips over "." and ".." here */
		    continue;
		if (!exists_process(pmiepid))
		    continue;
		sprintf(fullpath, "%s/%s", PMIE_DIR, dp->d_name);
		if (stat(fullpath, &statbuf) < 0) {
		    __pmNotifyErr(LOG_WARNING, "pmcd pmda cannot stat %s: %s",
				fullpath, strerror(errno));
		    continue;
		}
		if (statbuf.st_size != sizeof(pmiestats_t))
		    continue;
		if  ((endp = strdup(dp->d_name)) == NULL) {
		    __pmNoMem("pmie iname", strlen(dp->d_name), PM_RECOV_ERR);
		    continue;
		}
		if ((pmies = (pmie_t *)realloc(pmies, size)) == NULL) {
		    __pmNoMem("pmie instlist", size, PM_RECOV_ERR);
		    continue;
		}
		if ((fd = open(fullpath, O_RDONLY)) < 0) {
		    __pmNotifyErr(LOG_WARNING, "pmcd pmda cannot open %s: %s",
				fullpath, strerror(errno));
		    continue;
		}
		ptr = mmap(NULL, statbuf.st_size, PROT_READ, MAP_SHARED, fd, 0);
		close(fd);
		if (ptr == MAP_FAILED) {
		    __pmNotifyErr(LOG_ERR, "pmcd pmda mmap of %s failed: %s",
				fullpath, strerror(errno));
		    continue;
		}
		else if (((pmiestats_t *)ptr)->version != 1) {
		    __pmNotifyErr(LOG_WARNING, "incompatible pmie version: %s",
				fullpath);
		    continue;
		}
		pmies[npmies].pid = pmiepid;
		pmies[npmies].mmap = ptr;
		pmies[npmies].name = endp;
		npmies++;
	    }
	    closedir(pmiedir);
	}
    }
    setoserror(0);
    return npmies;
}

static int
pmcd_instance_reg(int inst, char *name, __pmInResult **result)
{
    __pmInResult	*res;
    int		i;
    char	idx[3];		/* ok for NUMREG <= 99 */

    res = (__pmInResult *)malloc(sizeof(__pmInResult));
    if (res == NULL)
        return -errno;

    if (name == NULL && inst == PM_IN_NULL)
	res->numinst = NUMREG;
    else
	res->numinst = 1;

    if (inst == PM_IN_NULL) {
	if ((res->instlist = (int *)malloc(res->numinst * sizeof(res->instlist[0]))) == NULL) {
	    free(res);
	    return -errno;
	}
    }
    else
	res->instlist = NULL;

    if (name == NULL) {
	if ((res->namelist = (char **)malloc(res->numinst * sizeof(res->namelist[0]))) == NULL) {
	    __pmFreeInResult(res);
	    return -errno;
	}
	for (i = 0; i < res->numinst; i++)
	    res->namelist[0] = NULL;
    }
    else
	res->namelist = NULL;

    if (name == NULL && inst == PM_IN_NULL) {
	/* return inst and name for everything */
	for (i = 0; i < res->numinst; i++) {
	    res->instlist[i] = i;
	    sprintf(idx, "%d", i);
	    if ((res->namelist[i] = strdup(idx)) == NULL) {
		__pmFreeInResult(res);
		return -errno;
	    }
	}
    }
    else if (name == NULL) {
	/* given an inst, return the name */
	if (0 <= inst && inst < NUMREG) {
	    sprintf(idx, "%d", inst);
	    if ((res->namelist[0] = strdup(idx)) == NULL) {
		__pmFreeInResult(res);
		return -errno;
	    }
	}
	else {
	    __pmFreeInResult(res);
	    return PM_ERR_INST;
	}
    }
    else if (inst == PM_IN_NULL) {
	/* given a name, return an inst */
	char	*endp;
	i = (int)strtol(name, &endp, 10);
	if (*endp == '\0' && 0 <= i && i < NUMREG)
	    res->instlist[0] = i;
	else {
	    __pmFreeInResult(res);
	    return PM_ERR_INST;
	}
    }

    *result = res;
    return 0;
}

static int
pmcd_instance_pool(int inst, char *name, __pmInResult **result)
{
    __pmInResult	*res;
    int		i;

    res = (__pmInResult *)malloc(sizeof(__pmInResult));
    if (res == NULL)
        return -errno;

    if (name == NULL && inst == PM_IN_NULL)
	res->numinst = nbufsz;
    else
	res->numinst = 1;

    if (inst == PM_IN_NULL) {
	if ((res->instlist = (int *)malloc(res->numinst * sizeof(res->instlist[0]))) == NULL) {
	    free(res);
	    return -errno;
	}
    }
    else
	res->instlist = NULL;

    if (name == NULL) {
	if ((res->namelist = (char **)malloc(res->numinst * sizeof(res->namelist[0]))) == NULL) {
	    __pmFreeInResult(res);
	    return -errno;
	}
	for (i = 0; i < res->numinst; i++)
	    res->namelist[0] = NULL;
    }
    else
	res->namelist = NULL;

    if (name == NULL && inst == PM_IN_NULL) {
	/* return inst and name for everything */
	for (i = 0; i < nbufsz; i++) {
	    res->instlist[i] = bufinst[i].inst;
	    if ((res->namelist[i] = strdup(bufinst[i].iname)) == NULL) {
		__pmFreeInResult(res);
		return -errno;
	    }
	}
    }
    else if (name == NULL) {
	/* given an inst, return the name */
	for (i = 0; i < nbufsz; i++) {
	    if (inst == bufinst[i].inst) {
		if ((res->namelist[0] = strdup(bufinst[i].iname)) == NULL) {
		    __pmFreeInResult(res);
		    return -errno;
		}
		break;
	    }
	}
	if (i == nbufsz) {
	    __pmFreeInResult(res);
	    return PM_ERR_INST;
	}
    }
    else if (inst == PM_IN_NULL) {
	/* given a name, return an inst */
	for (i = 0; i < nbufsz; i++) {
	    if (strcmp(name, bufinst[i].iname) == 0) {
		res->instlist[0] = bufinst[i].inst;
		break;
	    }
	}
	if (i == nbufsz) {
	    __pmFreeInResult(res);
	    return PM_ERR_INST;
	}
    }

    *result = res;
    return 0;
}

/*ARGSUSED*/
static int
pmcd_instance(pmInDom indom, int inst, char *name, __pmInResult **result, pmdaExt *pmda)
{
    int			sts = 0;
    __pmInResult	*res;
    int			getall = 0;
    int			getname;
    int			nports;
    __pmLogPort		*ports;
    unsigned int	pmiecount;
    int			i;

    if (indom == regindom)
	return pmcd_instance_reg(inst, name, result);
    else if (indom == bufindom)
	return pmcd_instance_pool(inst, name, result);
    else if (indom == logindom || indom == pmdaindom || indom == pmieindom) {
	res = (__pmInResult *)malloc(sizeof(__pmInResult));
	if (res == NULL)
	    return -errno;
	res->instlist = NULL;
	res->namelist = NULL;

	if (indom == logindom) {
	    /* use the wildcard behaviour of __pmLogFindPort to find
	     * all pmlogger ports on localhost.  Note that
	     * __pmLogFindPort will not attempt to contact pmcd if
	     * localhost is specified---this means we don't get a
	     * recursive call to pmcd which would hang!
	     */
	    if ((nports = __pmLogFindPort("localhost", PM_LOG_ALL_PIDS, &ports)) < 0) {
		free(res);
		return nports;
	    }
	}
	else if (indom == pmieindom)
	    pmiecount = refresh_pmie_indom();

	if (name == NULL && inst == PM_IN_NULL) {
	    getall = 1;

	    if (indom == logindom)
		res->numinst = nports;
	    else if (indom == pmieindom)
		res->numinst = pmiecount;
	    else
		res->numinst = nAgents;
	}
	else {
	    getname = name == NULL;
	    res->numinst = 1;
	}

	if (getall || !getname) {
	    if ((res->instlist = (int *)malloc(res->numinst * sizeof(int))) == NULL) {
		sts = -errno;
		__pmNoMem("pmcd_instance instlist", res->numinst * sizeof(int), PM_RECOV_ERR);
		__pmFreeInResult(res);
		return sts;
	    }
	}
	if (getall || getname) {
	    if ((res->namelist = (char **)malloc(res->numinst * sizeof(char *))) == NULL) {
		sts = -errno;
		__pmNoMem("pmcd_instance namelist", res->numinst * sizeof(char *), PM_RECOV_ERR);
		free(res->instlist);
		__pmFreeInResult(res);
		return sts;
	    }
	}
    }
    else
	return PM_ERR_INDOM;

    if (indom == logindom) {
	res->indom = logindom;

	if (getall) {		/* get instance ids and names */
	    for (i = 0; i < nports; i++) {
		res->instlist[i] = ports[i].pid;
		res->namelist[i] = strdup(ports[i].name);
		if (res->namelist[i] == NULL) {
		    sts = -errno;
		    __pmNoMem("pmcd_instance pmGetInDom",
			     strlen(ports[i].name), PM_RECOV_ERR);
		    /* ensure pmFreeInResult only gets valid pointers */
		    res->numinst = i;
		    break;
		}
	    }
	}
	else if (getname) {	/* given id, get name */
	    for (i = 0; i < nports; i++) {
		if (inst == ports[i].pid)
		    break;
	    }
	    if (i == nports) {
		sts = PM_ERR_INST;
		res->namelist[0] = NULL;
	    }
	    else {
		res->namelist[0] = strdup(ports[i].name);
		if (res->namelist[0] == NULL) {
		    __pmNoMem("pmcd_instance pmNameInDom",
			     strlen(ports[i].name), PM_RECOV_ERR);
		    sts = -errno;
		}
	    }
	}
	else {			/* given name, get id */
	    for (i = 0; i < nports; i++) {
		if (strcmp(name, ports[i].name) == 0)
		    break;
	    }
	    if (i == nports)
		sts = PM_ERR_INST;
	    else
		res->instlist[0] = ports[i].pid;
	}
    }
    else if (indom == pmieindom) {
	res->indom = pmieindom;

	if (getall) {		/* get instance ids and names */
	    for (i = 0; i < pmiecount; i++) {
		res->instlist[i] = pmies[i].pid;
		res->namelist[i] = strdup(pmies[i].name);
		if (res->namelist[i] == NULL) {
		    sts = -errno;
		    __pmNoMem("pmie_instance pmGetInDom",
			     strlen(pmies[i].name), PM_RECOV_ERR);
		    /* ensure pmFreeInResult only gets valid pointers */
		    res->numinst = i;
		    break;
		}
	    }
	}
	else if (getname) {	/* given id, get name */
	    for (i = 0; i < pmiecount; i++) {
		if (inst == pmies[i].pid)
		    break;
	    }
	    if (i == pmiecount) {
		sts = PM_ERR_INST;
		res->namelist[0] = NULL;
	    }
	    else {
		res->namelist[0] = strdup(pmies[i].name);
		if (res->namelist[0] == NULL) {
		    sts = -errno;
		    __pmNoMem("pmcd_instance pmNameInDom",
			     strlen(pmies[i].name), PM_RECOV_ERR);
		}
	    }
	}
	else {			/* given name, get id */
	    for (i = 0; i < pmiecount; i++) {
		if (strcmp(name, pmies[i].name) == 0)
		    break;
	    }
	    if (i == pmiecount)
		sts = PM_ERR_INST;
	    else
		res->instlist[0] = pmies[i].pid;
	}
    }
    else {
	res->indom = pmdaindom;

	if (getall) {		/* get instance ids and names */
	    for (i = 0; i < nAgents; i++) {
		res->instlist[i] = agent[i].pmDomainId;
		res->namelist[i] = strdup(agent[i].pmDomainLabel);
		if (res->namelist[i] == NULL) {
		    sts = -errno;
		    __pmNoMem("pmcd_instance pmGetInDom",
			     strlen(agent[i].pmDomainLabel), PM_RECOV_ERR);
		    /* ensure pmFreeInResult only gets valid pointers */
		    res->numinst = i;
		    break;
		}
	    }
	}
	else if (getname) {	/* given id, get name */
	    for (i = 0; i < nAgents; i++) {
		if (inst == agent[i].pmDomainId)
		    break;
	    }
	    if (i == nAgents) {
		sts = PM_ERR_INST;
		res->namelist[0] = NULL;
	    }
	    else {
		res->namelist[0] = strdup(agent[i].pmDomainLabel);
		if (res->namelist[0] == NULL) {
		    sts = -errno;
		    __pmNoMem("pmcd_instance pmNameInDom",
			     strlen(agent[i].pmDomainLabel), PM_RECOV_ERR);
		}
	    }
	}
	else {			/* given name, get id */
	    for (i = 0; i < nAgents; i++) {
		if (strcmp(name, agent[i].pmDomainLabel) == 0)
		    break;
	    }
	    if (i == nAgents)
		sts = PM_ERR_INST;
	    else
		res->instlist[0] = agent[i].pmDomainId;
	}
    }

    if (sts < 0) {
	__pmFreeInResult(res);
	return sts;
    }

    *result = res;
    return 0;
}

/*
 * numval != 1, so re-do vset[i] allocation
 */
static int
vset_resize(pmResult *rp, int i, int onumval, int numval)
{
    int		expect = numval;

    if (rp->vset[i] != NULL) {
	if (onumval == 1)
	    __pmPoolFree(rp->vset[i], sizeof(pmValueSet));
	else
	    free(rp->vset[i]);
    }

    if (numval < 0)
	expect = 0;

    if (expect == 1)
	rp->vset[i] = (pmValueSet *)__pmPoolAlloc(sizeof(pmValueSet));
    else
	rp->vset[i] = (pmValueSet *)malloc(sizeof(pmValueSet) + (expect-1)*sizeof(pmValue));

    if (rp->vset[i] == NULL) {
	if (i) {
	    /* we're doomed ... reclaim pmValues 0, 1, ... i-1 */
	    rp->numpmid = i;
	    __pmFreeResultValues(rp);
	}
	return -1;
    }

    rp->vset[i]->numval = numval;
    return 0;
}

/*ARGSUSED*/
static int
pmcd_fetch(int numpmid, pmID pmidlist[], pmResult **resp, pmdaExt *pmda)
{
    int			i;		/* over pmidlist[] */
    int			j;
    int			sts, nports;
    int			need;
    int			numval;
    int			valfmt;
    static pmResult	*res = NULL;
    static int		maxnpmids = 0;
    static char		*hostname = NULL;
    pmiestats_t		*pmie;
    pmValueSet		*vset;
    pmDesc		*dp;
    __pmID_int		*pmidp;
    pmAtomValue		atom;
    __pmLogPort		*lpp;
    static char		*tz;

    if (numpmid > maxnpmids) {
	if (res != NULL)
	    free(res);
	/* (numpmid - 1) because there's room for one valueSet in a pmResult */
	need = (int)sizeof(pmResult) + (numpmid - 1) * (int)sizeof(pmValueSet *);
	if ((res = (pmResult *) malloc(need)) == NULL)
	    return -ENOMEM;
	maxnpmids = numpmid;
    }
    res->timestamp.tv_sec = 0;
    res->timestamp.tv_usec = 0;
    res->numpmid = numpmid;

    for (i = 0; i < numpmid; i++) {
	/* Allocate a pmValueSet with room for just one value.  Even for the
	 * pmlogger port metric which has an instance domain, most of the time
	 * there will only be one logger running (i.e. one instance).  For the
	 * infrequent cases resize the value set later.
	 */
	res->vset[i] = NULL;
	if (vset_resize(res, i, 0, 1) == -1)
		return -ENOMEM;
	vset = res->vset[i];
	vset->pmid = pmidlist[i];
	vset->vlist[0].inst = PM_IN_NULL;

	for (j = 0; j < ndesc; j++) {
	    if (desctab[j].pmid == pmidlist[i]) {
		dp = &desctab[j];
		break;
	    }
	}
	if (j == ndesc) {
	    /* Error, need a smaller vset */
	    if (vset_resize(res, i, 1, PM_ERR_PMID) == -1)
		return -ENOMEM;
	    res->vset[i]->pmid = pmidlist[i];
	    continue;
	}

	valfmt = -1;
	sts = 0;
	
	pmidp = (__pmID_int *)&pmidlist[i];
	switch (pmidp->cluster) {

	    case 0:	/* global metrics */
		    switch (pmidp->item) {
			case 0:		/* control.debug */
				atom.l = pmDebug;
				break;
			case 1:		/* datasize */
				atom.ul = (int)((__psint_t)sbrk(0) - (__psint_t)_pmcd_data) / 1024;
				break;
			case 2:		/* numagents */
				atom.ul = 0;
				for (j = 0; j < nAgents; j++)
				    if (agent[j].status.connected)
					atom.ul++;
				break;
			case 3:		/* numclients */
				atom.ul = 0;
				for (j = 0; j < nClients; j++)
				    if (client[j].status.connected)
					atom.ul++;
				break;
			case 4:		/* control.timeout */
				atom.ul = _pmcd_timeout;
				break;
			case 5:		/* timezone $TZ */
				if (tz == NULL) {
#if defined(IRIX6_5)
                                    if (_MIPS_SYMBOL_PRESENT(__pmTimezone))
                                        tz = __pmTimezone();
                                    else
                                        tz = getenv("TZ");
#else
                                    tz = __pmTimezone();
#endif
                                }
				atom.cp = tz;
				break;
			case 6:		/* simabi (pmcd calling convention) */
				atom.cp =
#if defined(_M_IX86)
				    "ia_32";
#elif   _MIPS_SIM == _MIPS_SIM_ABI32
				    "o32";
#elif _MIPS_SIM == _MIPS_SIM_NABI32
				    "n32";
#elif _MIPS_SIM == _MIPS_SIM_ABI64
				    "64";
#else
    !!! bozo : dont know which executable format pmcd should be!!!
#endif
				break;
			case 7:		/* version */
				atom.cp = PCP_VERSION;
				break;
			case 8:		/* register */
				for (j = numval = 0; j < NUMREG; j++) {
				    if (__pmInProfile(regindom, _profile, j))
					numval++;
				}
				if (numval != 1) {
				    /* need a different vset size */
				    if (vset_resize(res, i, 1, numval) == -1)
					return -ENOMEM;
				    vset = res->vset[i];
				    vset->pmid = pmidlist[i];
				}
				for (j = numval = 0; j < NUMREG; j++) {
				    if (!__pmInProfile(regindom, _profile, j))
					continue;
				    vset->vlist[numval].inst = j;
				    atom.l = reg[j];
				    sts = __pmStuffValue(&atom, 0,
							&vset->vlist[numval], dp->type);
				    if (sts < 0)
					break;
				    valfmt = sts;
				    numval++;
				}
				break;
			case 9:		/* traceconn */
				atom.l = (_pmcd_trace_mask & TR_MASK_CONN) ? 1 : 0;
				break;
			case 10:	/* tracepdu */
				atom.l = (_pmcd_trace_mask & TR_MASK_PDU) ? 1 : 0;
				break;
			case 11:	/* tracebufs */
				atom.l = _pmcd_trace_nbufs;
				break;
			case 12:	/* dumptrace ... always 0 */
				atom.l = 0;
				break;
			case 13:	/* dumpconn ... always 0 */
				atom.l = 0;
				break;
			case 14:	/* tracenobuf */
				atom.l = (_pmcd_trace_mask & TR_MASK_NOBUF) ? 1 : 0;
				break;
			case 15:	/* sighup ... always 0 */
				atom.l = 0;
				break;
			case 16:	/* license */
				/*
				 * be careful about errors, and
				 * strip the PM_LIC_PCP bit as this is
				 * redundant
				 */
				j = __pmGetLicenseCap();
				if (j < 0) j = 0;
				atom.l = j & (~0x4);
				break;
			case 17:	/* openfds */
				atom.ul = (unsigned int)pmcd_hi_openfds;
				break;
			case 18:	/* buf.alloc */
			case 19:	/* buf.free */
				for (j = numval = 0; j < nbufsz; j++) {
				    if (__pmInProfile(bufindom, _profile, bufinst[j].inst))
					numval++;
				}
				if (numval != 1) {
				    /* need a different vset size */
				    if (vset_resize(res, i, 1, numval) == -1)
					return -ENOMEM;
				    vset = res->vset[i];
				    vset->pmid = pmidlist[i];
				}
				for (j = numval = 0; j < nbufsz; j++) {
				    int		alloced;
				    int		free;
				    if (!__pmInProfile(bufindom, _profile, bufinst[j].inst))
					continue;
				    vset->vlist[numval].inst = bufinst[j].inst;
				    if (bufinst[j].inst < 1024) {
					/* PoolAlloc pool */
#if defined(IRIX6_5)
					if (_MIPS_SYMBOL_PRESENT(__pmPoolCount))
					    __pmPoolCount(bufinst[j].inst, &alloced, &free);
					else
					    alloced = free = -1;
#else
					__pmPoolCount(bufinst[j].inst, &alloced, &free);
#endif
				    }
				    else {
					/* PDUBuf pool */
					int	xtra_alloced;
					int	xtra_free;
#if defined(IRIX6_5)
					if (_MIPS_SYMBOL_PRESENT(__pmCountPDUBuf)) {
					    __pmCountPDUBuf(bufinst[j].inst, &alloced, &free);
					    if (bufinst[j].inst <= 8192) {
						/*
						 * adjust for larger buffers,
						 * e.g. the 2K buffer count
						 * also includes the 3K, 4K, ..
						 * buffers, so subtract these,
						 * which are reported as the
						 * 2K+1 buffer count
						 */
						__pmCountPDUBuf(bufinst[j].inst + 1, &xtra_alloced, &xtra_free);
						alloced -= xtra_alloced;
						free -= xtra_free;
					    }
					}
					else
					    alloced = free = -1;
#else
					__pmCountPDUBuf(bufinst[j].inst, &alloced, &free);
					/*
					 * the 2K buffer count also includes
					 * the 3K, 4K, ... buffers, so sub
					 * these ... which are reported as
					 * the 3K buffer count
					 */
					__pmCountPDUBuf(bufinst[j].inst + 1024, &xtra_alloced, &xtra_free);
					alloced -= xtra_alloced;
					free -= xtra_free;
#endif
				    }
				    if (pmidp->item == 18)
					atom.l = alloced;
				    else
					atom.l = free;
				    sts = __pmStuffValue(&atom, 0,
							&vset->vlist[numval], dp->type);
				    if (sts < 0)
					break;
				    valfmt = sts;
				    numval++;
				}
				break;
		    }
		    break;

	    case 1:	/* PDUs received */
		    if (pmidp->item == _TOTAL) {
			/* total */
			atom.ul = 0;
			for (j = 0; j <= PDU_MAX; j++)
			    atom.ul += __pmPDUCntIn[j];
		    }
		    else if (pmidp->item < _TOTAL)
			atom.ul = __pmPDUCntIn[pmidp->item];
		    else
			atom.ul = __pmPDUCntIn[pmidp->item-1];
		    break;

	    case 2:	/* PDUs sent */
		    if (pmidp->item == _TOTAL) {
			/* total */
			atom.ul = 0;
			for (j = 0; j <= PDU_MAX; j++)
			    atom.ul += __pmPDUCntOut[j];
		    }
		    else if (pmidp->item < _TOTAL)
			atom.ul = __pmPDUCntOut[pmidp->item];
		    else
			atom.ul = __pmPDUCntOut[pmidp->item-1];
		    break;

	    case 3:	/* pmlogger control port, pmcd_host, archive and host */
		    /* find all ports.  localhost => no recursive pmcd access */
		    nports = __pmLogFindPort("localhost", PM_LOG_ALL_PIDS, &lpp);
		    if (nports < 0) {
			sts = nports;
			break;
		    }
		    for (j = numval = 0; j < nports; j++) {
			if (__pmInProfile(logindom, _profile, lpp[j].pid))
			    numval++;
		    }
		    if (numval != 1) {
			/* need a different vset size */
			if (vset_resize(res, i, 1, numval) == -1)
			    return -ENOMEM;
			vset = res->vset[i];
			vset->pmid = pmidlist[i];
		    }
		    for (j = numval = 0; j < nports; j++) {
			if (!__pmInProfile(logindom, _profile, lpp[j].pid))
			    continue;
			vset->vlist[numval].inst = lpp[j].pid;
			switch (pmidp->item) {
			    case 0:		/* pmlogger.port */
				atom.ul = lpp[j].port;
				break;
			    case 1:		/* pmlogger.pmcd_host */
				if (lpp[j].pmcd_host != NULL)
				    atom.cp = lpp[j].pmcd_host;
				else
				    atom.cp = strdup("");
				break;
			    case 2:		/* pmlogger.archive */
				if (lpp[j].archive != NULL)
				    atom.cp = lpp[j].archive;
				else
				    atom.cp = strdup("");
				break;
			    case 3:		/* pmlogger.host */
				if (hostname == NULL) {
				    char		hbuf[MAXHOSTNAMELEN];
				    struct hostent	*hep;
				    (void)gethostname(hbuf, MAXHOSTNAMELEN);
				    hbuf[MAXHOSTNAMELEN-1] = '\0';
				    hep = gethostbyname(hbuf);
				    if (hep != NULL)
					hostname = strdup(hep->h_name);
				}
				if (hostname != NULL)
				    atom.cp = strdup(hostname);
				else
				    atom.cp = strdup("");
				break;
			}
			sts = __pmStuffValue(&atom, 0,
				    &vset->vlist[numval], dp->type);
			if (sts < 0)
			    break;
			valfmt = sts;
			numval++;
		    }
		    break;

	    case 4:	/* PMDA metrics */
		for (j = numval = 0; j < nAgents; j++) {
		    if (__pmInProfile(pmdaindom, _profile, agent[j].pmDomainId))
			numval++;
		}
		if (numval != 1) {
		    /* need a different vset size */
		    if (vset_resize(res, i, 1, numval) == -1)
			return -ENOMEM;
		    vset = res->vset[i];
		    vset->pmid = pmidlist[i];
		}
		for (j = numval = 0; j < nAgents; j++) {
		    if (!__pmInProfile(pmdaindom, _profile, agent[j].pmDomainId))
			continue;
		    vset->vlist[numval].inst = agent[j].pmDomainId;
		    switch (pmidp->item) {
			case 0:		/* agent.type */
			    atom.ul = (agent[j].ipcType << 1) | (agent[j].pduProtocol == PDU_ASCII);
			    break;
			case 1:		/* agent.status */
			    if (agent[j].status.notReady)
				atom.l = 1;
			    else if (agent[j].status.connected)
				atom.l = 0;
			    else
				atom.l = agent[j].reason;
			    break;
		    }
		    sts = __pmStuffValue(&atom, 0,
					&vset->vlist[numval], dp->type);
		    if (sts < 0)
			break;
		    valfmt = sts;
		    numval++;
		}
		if (numval > 0) {
		    pmResult	sortme;
		    sortme.numpmid = 1;
		    sortme.vset[0] = vset;
		    pmSortInstances(&sortme);
		}
		break;


	    case 5:	/* pmie metrics */
		refresh_pmie_indom();
		for (j = numval = 0; j < npmies; j++) {
		    if (__pmInProfile(pmieindom, _profile, pmies[j].pid))
			numval++;
		}
		if (numval != 1) {
		    /* need a different vset size */
		    if (vset_resize(res, i, 1, numval) == -1)
			return -ENOMEM;
		    vset = res->vset[i];
		    vset->pmid = pmidlist[i];
		}
		for (j = numval = 0; j < npmies; ++j) {
		    if (!__pmInProfile(pmieindom, _profile, pmies[j].pid))
			continue;
		    vset->vlist[numval].inst = pmies[j].pid;
		    pmie = (pmiestats_t *)pmies[j].mmap;
		    switch (pmidp->item) {
			case 0:		/* pmie.configfile */
			    atom.cp = strdup(pmie->config);
			    break;
			case 1:		/* pmie.logfile */
			    atom.cp = strdup(pmie->logfile);
			    break;
			case 2:		/* pmie.pmcd_host */
			    atom.cp = strdup(pmie->defaultfqdn);
			    break;
			case 3:		/* pmie.numrules */
			    atom.ul = pmie->numrules;
			    break;
			case 4:		/* pmie.actions */
			    atom.ul = pmie->actions;
			    break;
			case 5:		/* pmie.eval.true */
			    atom.ul = pmie->eval_true;
			    break;
			case 6:		/* pmie.eval.false */
			    atom.ul = pmie->eval_false;
			    break;
			case 7:		/* pmie.eval.unknown */
			    atom.ul = pmie->eval_unknown;
			    break;
			case 8:		/* pmie.eval.expected */
			    atom.f = pmie->eval_expected;
			    break;
			case 9:		/* pmie.eval.actual */
			    atom.ul = pmie->eval_actual;
			    break;
		    }
		    if ((sts = __pmStuffValue(&atom, 0,
				&vset->vlist[numval], dp->type)) < 0)
			break;
		    valfmt = sts;
		    numval++;
		}
		if (numval > 0) {
		    pmResult	sortme;
		    sortme.numpmid = 1;
		    sortme.vset[0] = vset;
		    pmSortInstances(&sortme);
		}
		break;
	}

	if (sts == 0 && valfmt == -1 && vset->numval == 1)
	    sts = valfmt = __pmStuffValue(&atom, 0, &vset->vlist[0], dp->type);

	if (sts < 0) {
	    /* failure, encode status in numval, need a different vset size */
	    if (vset_resize(res, i, vset->numval, sts) == -1)
		return -ENOMEM;
	}
	else
	    vset->valfmt = valfmt;
    }
    *resp = res;

    return 0;
}

/*ARGSUSED*/
static int
pmcd_desc(pmID pmid, pmDesc *desc, pmdaExt *pmda)
{
    int		i;

    for (i = 0; i < ndesc; i++) {
	if (desctab[i].pmid == pmid) {
	    *desc = desctab[i];
	    return 0;
	}
    }
    return PM_ERR_PMID;
}

/*ARGSUSED*/
static int
pmcd_store(pmResult *result, pmdaExt *pmda)
{
    int		i;
    pmValueSet	*vsp;
    int		sts = 0;
    __pmID_int	*pmidp;

    for (i = 0; i < result->numpmid; i++) {
	vsp = result->vset[i];
	pmidp = (__pmID_int *)&vsp->pmid;
	if (pmidp->cluster == 0) {
	    if (pmidp->item == 0) {	/* pmcd.control.debug */
		pmDebug = vsp->vlist[0].value.lval;
		if (pmDebug == -1)
		    /* terminate pmcd */
		    _pmcd_done = 1;
	    }
	    else if (pmidp->item == 4) { /* pmcd.control.timeout */
		int	val = vsp->vlist[0].value.lval;
		if (val < 0) {
		    sts = PM_ERR_SIGN;
		    break;
		}
		if (val != _pmcd_timeout) {
		    _pmcd_timeout = val;
		}
	    }
	    else if (pmidp->item == 8) { /* pmcd.control.register */
		int	j;
		for (j = 0; j < vsp->numval; j++) {
		    if (0 <= vsp->vlist[j].inst && vsp->vlist[j].inst < NUMREG)
			reg[vsp->vlist[j].inst] = vsp->vlist[j].value.lval;
		    else {
			sts = PM_ERR_INST;
			break;
		    }
		}
	    }
	    else if (pmidp->item == 9) { /* pmcd.control.traceconn */
		int	val = vsp->vlist[0].value.lval;
		if (val == 0)
		    _pmcd_trace_mask &= (~TR_MASK_CONN);
		else if (val == 1)
		    _pmcd_trace_mask |= TR_MASK_CONN;
		else {
		    sts = PM_ERR_CONV;
		    break;
		}
	    }
	    else if (pmidp->item == 10) { /* pmcd.control.tracepdu */
		int	val = vsp->vlist[0].value.lval;
		if (val == 0)
		    _pmcd_trace_mask &= (~TR_MASK_PDU);
		else if (val == 1)
		    _pmcd_trace_mask |= TR_MASK_PDU;
		else {
		    sts = PM_ERR_CONV;
		    break;
		}
	    }
	    else if (pmidp->item == 11) { /* pmcd.control.tracebufs */
		int	val = vsp->vlist[0].value.lval;
		if (val < 0) {
		    sts = PM_ERR_SIGN;
		    break;
		}
		pmcd_init_trace(val);
	    }
	    else if (pmidp->item == 12) { /* pmcd.control.dumptrace */
		pmcd_dump_trace(stderr);
	    }
	    else if (pmidp->item == 13) { /* pmcd.control.dumpconn */
		time_t	now;
		time(&now);
		fprintf(stderr, "\n->Current PMCD clients at %s", ctime(&now));
		ShowClients(stderr);
	    }
	    else if (pmidp->item == 14) { /* pmcd.control.tracenobuf */
		int	val = vsp->vlist[0].value.lval;
		if (val == 0)
		    _pmcd_trace_mask &= (~TR_MASK_NOBUF);
		else if (val == 1)
		    _pmcd_trace_mask |= TR_MASK_NOBUF;
		else {
		    sts = PM_ERR_CONV;
		    break;
		}
	    }
	    else if (pmidp->item == 15) { /* pmcd.control.sighup */
		/*
		 * send myself SIGHUP
		 */
		__pmNotifyErr(LOG_INFO, "pmcd reset via pmcd.control.sighup");
		kill(getpid(), SIGHUP);
	    }
	    else {
		sts = PM_ERR_PMID;
		break;
	    }
	}
	else {
	    /* not one of the metrics we are willing to change */
	    sts = PM_ERR_PMID;
	    break;
	}
    }

    return sts;
}

void
pmcd_init(pmdaInterface *dp)
{
    pmdaDSO(dp, PMDA_INTERFACE_2, "pmcd", "/usr/pcp/pmdas/pmcd/help");

    dp->version.two.profile = pmcd_profile;
    dp->version.two.fetch = pmcd_fetch;
    dp->version.two.desc = pmcd_desc;
    dp->version.two.instance = pmcd_instance;
    dp->version.two.store = pmcd_store;

    init_tables(dp->domain);

    pmdaInit(dp, NULL, 0, NULL, 0);

    return;
}

#ifdef DEBUG_WITH_DBPMDA
/*
 * from pmcd's address space  ... weak symbols added here for use
 * with dbpmda
 */
int		_pmcd_done;
#pragma weak _pmcd_done
char		*_pmcd_data;
#pragma weak _pmcd_data
int		_pmcd_timeout;
#pragma weak _pmcd_timeout
int		_pmcd_trace_nbufs;
#pragma weak _pmcd_trace_nbufs
int		_pmcd_trace_mask;
#pragma weak _pmcd_trace_mask
int		nAgents;
#pragma weak nAgents
AgentInfo	*agent;
#pragma weak agent
int		nClients;
#pragma weak nClients
ClientInfo	*client;
#pragma weak client
int		pmcd_hi_openfds = -1;
#pragma weak pmcd_hi_openfds

void pmcd_init_trace(int n)
{
    fprintf(stderr, "dummy pmcd_init_trace(%d) called\n", n);
}
#pragma weak pmcd_init_trace

void
pmcd_dump_trace(FILE *f)
{
    fprintf(f, "dummy pmcd_dump_trace called\n");
}
#pragma weak pmcd_dump_trace

void
ShowClients(FILE *f)
{
    fprintf(f, "dummy ShowClients called\n");
}
#pragma weak ShowClients

#endif /* DEBUG_WITH_DBPMDA */
