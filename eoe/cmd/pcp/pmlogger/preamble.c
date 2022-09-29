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

#ident "$Id: preamble.c,v 2.18 1998/11/15 08:35:24 kenmcd Exp $"

#include <unistd.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>
#include "pmapi.h"
#include "impl.h"
#include "pmda.h"
#include "./logger.h"

extern char	    	*archBase;	/* base name for log files */
extern int		ctlport;	/* pmlogger control port number */
extern int		archive_version; 

extern int	errno;

/*
 * this routine creates the "fake" pmResult to be added to the
 * start of the archive log to identify information about the
 * archive beyond what is in the archive label.
 */

#ifdef HAVE_NETWORK_BYTEORDER

/* encode the domain(x), cluster (y) and item (z) parts of the PMID */
#define PMID(x,y,z) ((x<<22)|(y<<10)|z)

/* encode the domain(x) and serial (y) parts of the pmInDom */
#define INDOM(x,y) ((x<<22)|y)

#else /* Intel byte order */

/* encode the domain(x), cluster (y) and item (z) parts of the PMID */
#define PMID(x,y,z) ((x<<2)|(y<<10)|(z<<22))

/* encode the domain(x) and serial (y) parts of the pmInDom */
#define INDOM(x,y) ((x<<2)|(y<<10))

#endif /* HAVE_NETWORK_BYTEORDER */

/*
 * Note: these pmDesc entries MUST correspond to the corrsponding
 *	 entries from the real PMDA ...
 *	 We fake it out here to accommodate logging from PCP 1.1
 *	 PMCD's and to avoid round-trip dependencies in setting up
 *	 the preamble
 */
static pmDesc	desc[] = {
/* pmcd.pmlogger.host */
    { PMID(2,3,3), PM_TYPE_STRING, INDOM(2,1), PM_SEM_DISCRETE, {0,0,0,0,0,0} },
/* pmcd.pmlogger.port */
    { PMID(2,3,0), PM_TYPE_U32, INDOM(2,1), PM_SEM_DISCRETE, {0,0,0,0,0,0} },
/* pmcd.pmlogger.archive */
    { PMID(2,3,2), PM_TYPE_STRING, INDOM(2,1), PM_SEM_DISCRETE, {0,0,0,0,0,0} },
};
/* names added for version 2 archives */
static char*	names[] = {
"pmcd.pmlogger.host",
"pmcd.pmlogger.port",
"pmcd.pmlogger.archive"
};

static int	n_metric = sizeof(desc) / sizeof(desc[0]);

int
do_preamble(void)
{
    int		sts;
    int		i;
    int		j;
    pid_t	mypid = getpid();
    pmResult	*res;
    __pmPDU	*pb;
    pmAtomValue	atom;
    __pmTimeval	tmp;
    char	path[MAXPATHLEN];
    char	host[MAXHOSTNAMELEN];
    extern struct timeval       last_stamp;

    /* start to build the pmResult */
    res = (pmResult *)malloc(sizeof(pmResult) + (n_metric - 1) * sizeof(pmValueSet *));
    if (res == NULL)
	return -errno;

    res->numpmid = n_metric;
    last_stamp = res->timestamp = epoch;	/* struct assignment */
    tmp.tv_sec = (__int32_t)epoch.tv_sec;
    tmp.tv_usec = (__int32_t)epoch.tv_usec;

    for (i = 0; i < n_metric; i++) {
	res->vset[i] = (pmValueSet *)__pmPoolAlloc(sizeof(pmValueSet));
	if (res->vset[i] == NULL)
	    return -errno;
	res->vset[i]->pmid = desc[i].pmid;
	res->vset[i]->numval = 1;
	/* special case for each value 0 .. n_metric-1 */
	if (desc[i].pmid == PMID(2,3,3)) {
	    /* my fully qualified hostname, cloned from the pmcd PMDA */
	    struct hostent	*hep = NULL;
	    (void)gethostname(host, MAXHOSTNAMELEN);
	    host[MAXHOSTNAMELEN-1] = '\0';
	    hep = gethostbyname(host);
	    if (hep != NULL)
		atom.cp = hep->h_name;
	    else
		atom.cp = host;
	 }
	 else if (desc[i].pmid == PMID(2,3,0)) {
	    /* my control port number, from ports.c */
	    atom.l = ctlport;
	 }
	 else if (desc[i].pmid == PMID(2,3,2)) {
	    /*
	     * the full pathname to the base of the archive, cloned
	     * from GetPort() in ports.c
	     */
	    if (*archBase == '/')
		atom.cp = archBase;
	    else {
		if (getcwd(path, MAXPATHLEN) == NULL)
		    atom.cp = archBase;
		else {
		    strcat(path, "/");
		    strcat(path, archBase);
		    atom.cp = path;
		}
	    }
	}

	sts = __pmStuffValue(&atom, 0,  &res->vset[i]->vlist[0], desc[i].type);
	if (sts < 0)
	    return sts;
	res->vset[i]->vlist[0].inst = mypid;
	res->vset[i]->valfmt = sts;
    }

    if ((sts = __pmEncodeResult(PDU_BINARY, res, &pb)) < 0)
	return sts;

    if (archive_version != PM_LOG_VERS02)
	pb = rewrite_pdu(pb, archive_version);

    __pmOverrideLastFd(fileno(logctl.l_mfp));	/* force use of log version */
    /* and start some writing to the archive log files ... */
    if ((sts = __pmLogPutResult(&logctl, pb)) < 0)
	return sts;

    for (i = 0; i < n_metric; i++) {
	if (archive_version == PM_LOG_VERS02) {
	    if ((sts = __pmLogPutDesc(&logctl, &desc[i], 1, &names[i])) < 0)
		return sts;
        }
        else {
	    if ((sts = __pmLogPutDesc(&logctl, &desc[i], 0, NULL)) < 0)
		return sts;
        }
	if (desc[i].indom == PM_INDOM_NULL)
	    continue;
	for (j = 0; j < i; j++) {
	    if (desc[i].indom == desc[i].indom)
		break;
	}
	if (j == i) {
	    /* need indom ... force one with my PID as the only instance */
	    int		*instid;
	    char	**instname;

	    if ((instid = (int *)malloc(sizeof(*instid))) == NULL)
		return -errno;
	    *instid = mypid;
	    sprintf(path, "%d", mypid);
	    if ((instname = (char **)malloc(sizeof(char *)+strlen(path)+1)) == NULL)
		return -errno;
	    /*
	     * this _is_ correct ... instname[] is a one element array
	     * with the string value immediately following
	     */
	    instname[0] = (char *)&instname[1];
            strcpy(instname[0], path);
	    /*
	     * Note.	DO NOT free instid and instname ... they get hidden
	     *		away in addindom() below __pmLogPutInDom()
	     */
	    if ((sts = __pmLogPutInDom(&logctl, desc[i].indom, &tmp, 1, instid, instname)) < 0)
		return sts;
	}
    }

    /* fudge the temporal index */
    fflush(logctl.l_mfp);
    fseek(logctl.l_mfp, sizeof(__pmLogLabel)+2*sizeof(int), SEEK_SET);
    fflush(logctl.l_mdfp);
    fseek(logctl.l_mdfp, sizeof(__pmLogLabel)+2*sizeof(int), SEEK_SET);
    __pmLogPutIndex(&logctl, &tmp);
    fseek(logctl.l_mfp, 0L, SEEK_END);
    fseek(logctl.l_mdfp, 0L, SEEK_END);

    /*
     * and now free stuff
     * Note:	error returns cause mem leaks, but this routine
     *		is only ever called once, so tough luck
     */
    for (i = 0; i < n_metric; i++)
	__pmPoolFree(res->vset[i], sizeof(pmValueSet));
    free(res);

    return 0;
}
