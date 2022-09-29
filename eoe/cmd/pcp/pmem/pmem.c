/*
 * pmem - report per-process memory statistics
 *
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

#ident "$Id: pmem.c,v 2.26 1999/05/11 01:00:47 kenmcd Exp $"

#include <unistd.h>
#include <libgen.h>
#include "pmapi.h"
#include "pmapi_dev.h"
#include "impl.h"

static char *proc_names[] = {
    "proc.memory.virtual.txt",
    "proc.memory.virtual.dat",
    "proc.memory.virtual.bss",
    "proc.memory.virtual.stack",
    "proc.memory.virtual.shm",
    "proc.memory.physical.txt",
    "proc.memory.physical.dat",
    "proc.memory.physical.bss",
    "proc.memory.physical.stack",
    "proc.memory.physical.shm",
    "proc.psinfo.ppid",
    "proc.psinfo.uname",
    "proc.psinfo.psargs"
};
#define NUM_PROC (sizeof(proc_names)/sizeof(proc_names[0]))

static char *hinv_names[] = {
    "hinv.physmem",
    "mem.freemem"
};
#define NUM_HINV (sizeof(hinv_names)/sizeof(hinv_names[0]))

static char *shm_names[] = {
    "ipc.shm.nattch",
    "ipc.shm.segsz"
};
#define NUM_SHM (sizeof(shm_names)/sizeof(shm_names[0]))

/*
 * Assume NUM_PROC > NUM_HINV and NUM_PROC > NUM_SHM
 * ... size below is really max(NUM_PROC,NUM_HINV,NUM_PROC)
 */
static pmID pmidlist[NUM_PROC];

static pmDesc desc;
static int numproc;
static int numseg;
static int *instlist;
static char **namelist;
static pmResult *proc_res;
static pmResult *hinv_res;
static pmResult *shm_res;

static int		type = 0;
static struct timeval	startTime;

static double
getValue(pmValueSet *vset, int pid)
{
    __int64_t val;
    double ret;
    static int last = 0;
    int i, j = PM_IN_NULL;

    if (vset->numval > last && vset->vlist[last].inst == pid)
	j = last;
    else {
	for (i=0; i < vset->numval; i++) {
	    if (vset->vlist[i].inst == pid) {
		last = j = i;
		break;
	    }
	}
    }

    if (j != PM_IN_NULL) {
	if (vset->valfmt == PM_VAL_INSITU)
	    ret = (double)vset->vlist[j].value.lval;
	else {
	    memcpy(&val, vset->vlist[j].value.pval->vbuf, sizeof(val));
	    ret = (double)val;
	}
    }
    else {
	/* fail */
	return -1.0;
    }

    return ret;
}

static char *
getStringValue(pmValueSet *vset, int pid)
{
    int i;
    int j = PM_IN_NULL;

    for (i=0; i < vset->numval; i++) {
	if (vset->vlist[i].inst == pid) {
	    j = i;
	    break;
	}
    }

    if (j != PM_IN_NULL) {
	if (vset->valfmt == PM_VAL_INSITU)
	    return NULL;
	else {
	    return (char *)vset->vlist[j].value.pval->vbuf;
	}
    }

    /* fail */
    return NULL;
}

static void
print_mem(double k)
{
    static char sbuf[80];

    if (k < 0.0)
	/* blank */
	sbuf[0] = NULL;
    else if (k < 10000.0)
	sprintf(sbuf, "%4.0f ", k);			/* 9999_ */
    else if (k < 10000000.0)
	sprintf(sbuf, "%4.0fM", k / 1024.0);		/* 9999M */
    else
	sprintf(sbuf, "%4.0fG", k / (1024.0 * 1024.0));	/* 9999G */
    printf("%5s ", sbuf);
}

static void
print_args(int width, char *args)
{
    char *p, *b;
    static char buf[1024];

    for (b=buf, p=args; b < buf+width-4 && *p != NULL; p++) {
        if (*p & 0x80) {
            sprintf(b, "\\%03o", *p);
            b += 4;
            continue;
        }

        switch(*p) {
        case '\n': strcpy(b, "\\n"); b += 2; break;
        case '\r': strcpy(b, "\\r"); b += 2; break;
        case '\t': strcpy(b, "\\t"); b += 2; break;
        default:
            *b++ = *p;
            break;
        }
    }
    *b = NULL;
    puts(buf);
}

static int
myFetch(int numpmid, pmID *pmidlist, pmResult **result)
{
    if (type == PM_CONTEXT_ARCHIVE) {
	/*
	 * archives are real messy
	 *
	 * The desired metrics may be spread across multiple records in
	 * the archive, and if there is only 1 record for each metric,
	 * then PM_MODE_INTERP won't work.
	 *
	 * We have to do it the hard way.
	 */
	pmResult	*res;
	pmResult	*rp;
	int		i;
	int		sts;

	res = (pmResult *)malloc(sizeof(pmResult)+(numpmid-1)*sizeof(pmValueSet));
	res->numpmid = numpmid;

	for (i = 0; i < numpmid; i++) {

	    if ((sts = pmSetMode(PM_MODE_FORW, &startTime, 0)) < 0)
		fprintf(stderr, "%s: pmSetMode: %s\n", pmProgname, pmErrStr(sts));
	    if ((sts = pmFetch(1, &pmidlist[i], &rp)) < 0) {
		fprintf(stderr, "%s: Failed fetch for archive metric PMID: %s: %s\n", pmProgname, pmIDStr(pmidlist[i]), pmErrStr(sts));
		exit(1);
	    }
	    if (i == 0)
		res->timestamp = rp->timestamp;
	    res->vset[i] = rp->vset[0];
	}
	*result = res;
	return 0;
    }
    else
	/*
	 * live mode is easy
	 */
	return pmFetch(numpmid, pmidlist, result);
}


int
main(int argc, char **argv)
{
    int		c;
    int		i;
    int		pid;
    int		ppid;
    char	*user;
    char	*psargs;
    int		wide = 0;
    char	*userFlag = NULL;
    long	physmem = -1;
    long	freemem = -1;
    time_t	now;
    int		sts;
    char	timebuf[32];
    char	*p;
    int		errflag = 0;
    int 	verbose = 0;
    char	*host;
    char 	*logfile = NULL;
    char	*start = NULL;
    pmLogLabel	label;				/* get hostname for archives */
    int		zflag = 0;			/* for -z */
    char 	*tz = NULL;		/* for -Z timezone */
    int		tzh;				/* initial timezone handle */
    char	local[MAXHOSTNAMELEN];
    char	*pmnsfile = PM_NS_DEFAULT;
    double	delta = 1.0;
    double	sum_vtxt = 0;
    double	sum_ptxt = 0;
    double	sum_vdat = 0;
    double	sum_pdat = 0;
    double	sum_vshm = 0;
    double	sum_pshm = 0;
    double	vtxt;
    double	ptxt;
    double	vdat;
    double	pdat;
    double	vshm;
    double	pshm;
    struct timeval endTime;
    struct timeval appStart;
    struct timeval appDummy;
    char	*endnum;
    extern char	*optarg;
    extern int	optind;
    extern int	pmDebug;

    __pmSetAuthClient();

    /* trim command name of leading directory components */
    pmProgname = basename(argv[0]);

    while ((c = getopt(argc, argv, "wa:D:h:n:u:S:zZ:?")) != EOF) {
	switch (c) {

	case 'a':	/* archive name */
	    if (type != 0) {
		fprintf(stderr, "%s: at most one of -a and/or -h allowed\n", pmProgname);
		errflag++;
	    }
	    type = PM_CONTEXT_ARCHIVE;
	    host = optarg;
	    break;

	case 'D':	/* debug flag */
	    sts = __pmParseDebug(optarg);
	    if (sts < 0) {
		fprintf(stderr, "%s: unrecognized debug flag specification (%s)\n",
		    pmProgname, optarg);
		errflag++;
	    }
	    else
		pmDebug |= sts;
	    break;

	case 'h':	/* contact PMCD on this hostname */
	    if (type != 0) {
		fprintf(stderr, "%s: at most one of -a and/or -h allowed\n", pmProgname);
		errflag++;
	    }
	    host = optarg;
	    type = PM_CONTEXT_HOST;
	    break;

	case 'n':	/* alternative name space file */
	    pmnsfile = optarg;
	    break;

	case 'S':	/* start time */
	    start = optarg;
	    break;

	case 't':	/* delta seconds (double) */
	    delta = strtod(optarg, &endnum);
	    if (*endnum != '\0' || delta <= 0.0) {
		fprintf(stderr, "%s: -t requires floating point argument\n", pmProgname);
		errflag++;
	    }
	    break;

	case 'V':	/* verbose */
	    verbose++;
	    break;

	case 'z':	/* timezone from host */
	    if (tz != NULL) {
		fprintf(stderr, "%s: at most one of -Z and/or -z allowed\n", pmProgname);
		errflag++;
	    }
	    zflag++;
	    break;

	case 'Z':	/* $TZ timezone */
	    if (zflag) {
		fprintf(stderr, "%s: at most one of -Z and/or -z allowed\n", pmProgname);
		errflag++;
	    }
	    tz = optarg;
	    break;

	case 'w':	/* Wide */
	    wide = 1;
	    break;
	
	case 'u':	/* user */
	    userFlag = optarg;
	    break;

	case '?':
	default:
	    errflag++;
	    break;
	}
    }

    if (zflag && type == 0) {
	fprintf(stderr, "%s: -z requires an explicit -a or -h option\n", pmProgname);
	errflag++;
    }

    if (type != PM_CONTEXT_ARCHIVE && start != NULL) {
	fprintf(stderr, "%s: -S requires an explicit -a option\n", pmProgname);
	errflag++;
    }

    if (errflag) {
	fprintf(stderr,
"Usage: %s [options]\n\
\n\
Options:\n\
  -a archive    metrics source is a PCP log archive\n\
  -h host       metrics source is PMCD on host\n\
  -n pmnsfile   use an alternative PMNS\n\
  -S starttime  start of the time window\n\
  -u username   only report for username\n\
  -w            wide output for command name\n\
  -z            set reporting timezone to local time of metrics source\n\
  -Z timezone   set reporting timezone\n",
		  pmProgname);
	exit(1);
    }

    if (logfile != NULL) {
	__pmOpenLog(pmProgname, logfile, stderr, &sts);
	if (sts < 0) {
	    fprintf(stderr, "%s: Could not open logfile\n", pmProgname);
	}
    }

    if (pmnsfile != PM_NS_DEFAULT) {
	if ((sts = pmLoadNameSpace(pmnsfile)) < 0) {
	    printf("%s: Cannot load namespace from \"%s\": %s\n", pmProgname, pmnsfile, pmErrStr(sts));
	    exit(1);
	}
    }

#ifdef MALLOC_AUDIT
    _malloc_reset_();
    atexit(_malloc_audit_);
#endif

    if (type == 0) {
	type = PM_CONTEXT_HOST;
	(void)gethostname(local, MAXHOSTNAMELEN);
	local[MAXHOSTNAMELEN-1] = '\0';
	host = local;
    }
    if ((sts = pmNewContext(type, host)) < 0) {
	if (type == PM_CONTEXT_HOST)
	    fprintf(stderr, "%s: Cannot connect to PMCD on host \"%s\": %s\n",
		pmProgname, host, pmErrStr(sts));
	else
	    fprintf(stderr, "%s: Cannot open archive \"%s\": %s\n",
		pmProgname, host, pmErrStr(sts));
	exit(1);
    }

    if (zflag) {
	if ((tzh = pmNewContextZone()) < 0) {
	    fprintf(stderr, "%s: Cannot set context timezone: %s\n",
		pmProgname, pmErrStr(tzh));
	    exit(1);
	}
	if (type == PM_CONTEXT_ARCHIVE)
	    printf("Note: timezone set to local timezone of host \"%s\" from archive\n\n",
		label.ll_hostname);
	else
	    printf("Note: timezone set to local timezone of host \"%s\"\n\n", host);
    }
    else if (tz != NULL) {
	if ((tzh = pmNewZone(tz)) < 0) {
	    fprintf(stderr, "%s: Cannot set timezone to \"%s\": %s\n",
		pmProgname, tz, pmErrStr(tzh));
	    exit(1);
	}
	printf("Note: timezone set to \"TZ=%s\"\n\n", tz);
    }
    else
	tzh = pmNewContextZone();

    if (type == PM_CONTEXT_ARCHIVE) {
	if ((sts = pmGetArchiveLabel(&label)) < 0) {
	    fprintf(stderr, "%s: Cannot get archive label record: %s\n",
		pmProgname, pmErrStr(sts));
	    exit(1);
	}
  	startTime = label.ll_start;
	if ((sts = pmGetArchiveEnd(&endTime)) < 0) {
	    endTime.tv_sec = INT_MAX;
	    endTime.tv_usec = 0;
	    fflush(stdout);
	    fprintf(stderr, "%s: Cannot locate end of archive: %s\n",
		pmProgname, pmErrStr(sts));
	    fprintf(stderr, "\nWARNING: This archive is sufficiently damaged that it may not be possible to\n");
	    fprintf(stderr, "         produce complete information.  Continuing and hoping for the best.\n\n");
	    fflush(stderr);
	}

	sts = pmParseTimeWindow(start, NULL, NULL, NULL, &startTime,
				&endTime, &appStart, &appDummy, &appDummy,
				&p);

	if ((sts = pmSetMode(PM_MODE_FORW, &startTime, 0)) < 0) {
	    fprintf(stderr, "%s: pmSetMode: %s\n", pmProgname, pmErrStr(sts));
	    exit(1);
	}
    }

    /* non-flag args are argv[optind] ... argv[argc-1] */
    while (optind < argc) {
	printf("extra argument[%d]: %s\n", optind, argv[optind]);
	optind++;
    }

    /*
     * configured/free memory sizes
     */
    if ((sts = pmLookupName(NUM_HINV, hinv_names, pmidlist)) < 0) {
	for (i = 0; i < NUM_HINV; i++) {
	    if (pmidlist[i] == PM_ID_NULL)
		fprintf(stderr, "%s: failed to lookup metric: %s\n", pmProgname, hinv_names[i]);
	}
    }
    else {
	if ((sts = myFetch(NUM_HINV, pmidlist, &hinv_res)) < 0) {
	    fprintf(stderr, "%s: Failed fetch for hinv metrics: %s\n", pmProgname, pmErrStr(sts));
	}
	else {
	    __int64_t val;

	    if (hinv_res->vset[0]->valfmt == PM_VAL_INSITU)
		physmem = (long)hinv_res->vset[0]->vlist[0].value.lval;
	    else {
		memcpy(&val, hinv_res->vset[0]->vlist[0].value.pval->vbuf, sizeof(val));
		physmem = (long)val;
	    }

	    if (hinv_res->vset[1]->valfmt == PM_VAL_INSITU)
		freemem = (long)hinv_res->vset[1]->vlist[0].value.lval;
	    else {
		memcpy(&val, hinv_res->vset[1]->vlist[0].value.pval->vbuf, sizeof(val));
		freemem = (long)val;
	    }
	}
    }

    /*
     * all of the proc metrics
     */
    if ((sts = pmLookupName(NUM_PROC, proc_names, pmidlist)) < 0) {
	for (i = 0; i < NUM_PROC; i++) {
	    if (pmidlist[i] == PM_ID_NULL)
		fprintf(stderr, "%s: failed to lookup metric: %s\n", pmProgname, proc_names[i]);
	    exit(1);
	}
    }

    if ((sts = pmLookupDesc(pmidlist[0], &desc)) < 0) {
	fprintf(stderr, "%s: Failed to get descriptor for proc metrics: %s\n", pmProgname, pmErrStr(sts));
	exit(1);
    }

    if (type == PM_CONTEXT_ARCHIVE)
	sts = pmGetInDomArchive(desc.indom, &instlist, &namelist);
    else
	sts = pmGetInDom(desc.indom, &instlist, &namelist);
    if (sts  < 0) {
	fprintf(stderr, "%s: Failed to enumerate instances for proc metrics: %s\n", pmProgname, pmErrStr(sts));
	exit(1);
    }
    else
	numproc = sts;

    if ((sts = pmDelProfile(PM_INDOM_NULL, 0, NULL)) < 0) {
	fprintf(stderr, "%s: Failed to delete profile for proc metrics: %s\n", pmProgname, pmErrStr(sts));
	exit(1);
    }

    if ((sts = pmAddProfile(desc.indom, numproc, instlist)) < 0) {
	fprintf(stderr, "%s: Failed to add profile for %d proc metrics: %s\n", pmProgname, numproc, pmErrStr(sts));
	exit(1);
    }

    if ((sts = myFetch(NUM_PROC, pmidlist, &proc_res)) < 0) {
	fprintf(stderr, "%s: Failed fetch for proc metrics: %s\n", pmProgname, pmErrStr(sts));
	exit(1);
    }

    now = proc_res->timestamp.tv_sec;
    if (type == PM_CONTEXT_ARCHIVE)
	printf("Archive: %s\nHost: %s   ", host, label.ll_hostname);
    else
	printf("Host: %s   ", host);
    printf("Configured: ");
    if (physmem == -1)
	printf("????");
    else
	printf("%ld", physmem * 1024);
    printf("   Free: ");
    if (freemem == -1)
	printf("????");
    else
	printf("%ld", freemem);
    printf("   %s", pmCtime(&now, timebuf));

    /*
     * FMT - see below for lines that must reflect this format
     */
    printf("%7s %7s %8s %5s %5s %5s %5s %5s %5s %s\n",
    "pid","ppid","user","vtxt ","ptxt ","vdat ","pdat ","vshm ","pshm ","command");

    for (i = 0; i < numproc; i++) {
	pid = instlist[i];
	user = getStringValue(proc_res->vset[11], pid);
	ppid = getValue(proc_res->vset[10], pid);
	psargs = getStringValue(proc_res->vset[12], pid);

	vtxt = getValue(proc_res->vset[0], pid);

	if (vtxt < 0) {
	    /* process exited */
	    continue;
	}

	if (user == NULL || userFlag != NULL && strcmp(user, userFlag) != 0)
	    continue;

	vdat =
	    getValue(proc_res->vset[1], pid) +
	    getValue(proc_res->vset[2], pid) +
	    getValue(proc_res->vset[3], pid);
	vshm = getValue(proc_res->vset[4], pid);

	ptxt = getValue(proc_res->vset[5], pid);
	pdat = 
	    getValue(proc_res->vset[6], pid) +
	    getValue(proc_res->vset[7], pid) +
	    getValue(proc_res->vset[8], pid);
	pshm = getValue(proc_res->vset[9], pid);


	printf("%7d %7d %8s ", pid, ppid, user);

	print_mem(vtxt);
	print_mem(ptxt);
	print_mem(vdat);
	print_mem(pdat);
	print_mem(vshm);
	print_mem(pshm);
	print_args(wide ? 1024 : 21, psargs);

	sum_vtxt += vtxt;
	sum_ptxt += ptxt;
	sum_vdat += vdat;
	sum_pdat += pdat;
	sum_vshm += vshm;
	sum_pshm += pshm;
    }

    /*
     * the shm metrics
     */
    if ((sts = pmLookupName(NUM_SHM, shm_names, pmidlist)) < 0) {
	for (i = 0; i < NUM_SHM; i++) {
	    if (pmidlist[i] == PM_ID_NULL)
		fprintf(stderr, "%s: failed to lookup metric: %s\n", pmProgname, shm_names[i]);
	}
	goto summary;
    }

    if ((sts = pmLookupDesc(pmidlist[0], &desc)) < 0) {
	fprintf(stderr, "%s: Failed to get descriptor for shm metrics: %s\n", pmProgname, pmErrStr(sts));
	goto summary;
    }

    free(instlist);
    if (type == PM_CONTEXT_ARCHIVE)
	sts = pmGetInDomArchive(desc.indom, &instlist, &namelist);
    else
	sts = pmGetInDom(desc.indom, &instlist, &namelist);
    if (sts  < 0) {
	fprintf(stderr, "%s: Failed to enumerate instances for ipc.shm metrics: %s\n", pmProgname, pmErrStr(sts));
	goto summary;
    }
    else
	numseg = sts;

    if ((sts = pmDelProfile(PM_INDOM_NULL, 0, NULL)) < 0) {
	fprintf(stderr, "%s: Failed to delete profile for ipc.shm metrics: %s\n", pmProgname, pmErrStr(sts));
	goto summary;
    }


    if ((sts = pmAddProfile(desc.indom, numseg, instlist)) < 0) {
	fprintf(stderr, "%s: Failed to add profile for %d ipc.shm metrics: %s\n", pmProgname, numseg, pmErrStr(sts));
	goto summary;
    }

    if ((sts = myFetch(NUM_SHM, pmidlist, &shm_res)) < 0) {
	fprintf(stderr, "%s: Failed fetch for ipc.shm metrics: %s\n", pmProgname, pmErrStr(sts));
	goto summary;
    }

    vshm = 0;
    for (i = 0; i < numseg; i++) {
	int	nattach;
	int	seg = instlist[i];

	nattach = (int)getValue(shm_res->vset[0], seg);
	if (nattach == 0) {
	    vshm += getValue(shm_res->vset[1], seg);
	}
    }
    if (vshm > 4096) {
	vshm /= 1024;
	/* FMT */
	printf("%7s %7s %8s ", "???", "", "");
	print_mem(-1);
	print_mem(-1);
	print_mem(-1);
	print_mem(-1);
	print_mem(vshm);
	print_mem(-1);
	print_args(wide ? 1024 : 29, "orphaned shmem");
	sum_vshm += vshm;
    }


summary:

    /*
     * FMT
     * if you change this format, also change the heading line above,
     * the orphan line above and the second and third trailer lines
     * below
     */
    printf("\n%7s %7s %8s %5s %5s %5s %5s %5s %5s %d user processes\n",
	"Total", "", "",
	"vtxt ", "ptxt ", "vdat ", "pdat ", "vshm ", "pshm ", numproc);
    /* FMT */
    printf("%7s %7s %8s ", "", "", "");
    print_mem(sum_vtxt);
    print_mem(-1.0);
    print_mem(sum_vdat);
    print_mem(-1.0);
    print_mem(sum_vshm);
    print_mem(-1.0);
    printf("= ");
    print_mem(sum_vtxt + sum_vdat + sum_vshm);
    printf("virtual\n");

    /* FMT */
    printf("%7s %7s %8s ", "", "", "");
    print_mem(-1);
    print_mem(sum_ptxt);
    print_mem(-1);
    print_mem(sum_pdat);
    print_mem(-1);
    print_mem(sum_pshm);
    printf("= ");
    print_mem(sum_ptxt + sum_pdat + sum_pshm);
    printf("physical\n");

    return 0;
}
