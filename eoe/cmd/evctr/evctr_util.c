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
#ident "$Id: evctr_util.c,v 1.7 1999/04/30 15:24:33 steiner Exp $"

#include <sys/types.h>
#include <sys/stat.h>
#include <procfs/procfs.h>
#include <sys/syssgi.h>
#include <sys/systeminfo.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <libgen.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include "evctr_util.h"

EventDesc_t	DescR10K[] = {
    { HWPERF_C0PRFCNT0_CYCLES,	"CYCLES",	"Cycles" },
    { HWPERF_C0PRFCNT0_IINSTR,	"INSTRI",	"Instructions issued" },
    { HWPERF_C0PRFCNT0_ILD,	"LOADI",	"Loads, etc. issued" },
    { HWPERF_C0PRFCNT0_IST,	"STOREI",	"Stores issued" },
    { HWPERF_C0PRFCNT0_ISC,	"SCONDI",	"Store conditionals issued" },
    { HWPERF_C0PRFCNT0_FAILSC,	"SCONDF",	"Store conditionals failed" },
    { HWPERF_C0PRFCNT0_DECBR,	"BRD",		"Branches decoded" },
    { HWPERF_C0PRFCNT0_QWWSC,	"WB2$",		"Quadwords written back from scache" },
    { HWPERF_C0PRFCNT0_SCDAECC,	"ECC2$",	"Single-bit ECC errors on scache data" },
    { HWPERF_C0PRFCNT0_PICMISS,	"1$IMISS",	"Pcache instruction misses" },
    { HWPERF_C0PRFCNT0_SICMISS,	"2$IMISS",	"Scache instruction misses" },
    { HWPERF_C0PRFCNT0_IMISPRED,"2$IWAYMP",	"Scache instruction way misprediction" },
    { HWPERF_C0PRFCNT0_EXTINT,	"INT",		"External intervention requests" },
    { HWPERF_C0PRFCNT0_EXTINV,	"INV",		"External invalidate requests" },
    { HWPERF_C0PRFCNT0_VCOH,	"FUCOMP",	"ALU/FPU completion cycles" },
    { HWPERF_C0PRFCNT0_GINSTR,	"INSTRG",	"Instructions graduated" },
    { HWPERF_C0PRFCNT1_CYCLES,	"CYCLES",	"Cycles" },
    { HWPERF_C0PRFCNT1_GINSTR,	"INSTRG",	"Instructions graduated" },
    { HWPERF_C0PRFCNT1_GLD,	"LOADG",	"Loads graduated" },
    { HWPERF_C0PRFCNT1_GST,	"STOREG",	"Stores graduated" },
    { HWPERF_C0PRFCNT1_GSC,	"SCONDG",	"Store conditionals graduated" },
    { HWPERF_C0PRFCNT1_GFINSTR,	"FPG",		"Floating point instructions graduated" },
    { HWPERF_C0PRFCNT1_QWWPC,	"WB1$",		"Quadwords written back from pcache" },
    { HWPERF_C0PRFCNT1_TLBMISS,	"TLBMISS",	"TLB refill exceptions" },
    { HWPERF_C0PRFCNT1_BRMISS,	"BRMP",		"Branches mispredicted" },
    { HWPERF_C0PRFCNT1_PDCMISS,	"1$DMISS",	"Pcache data misses" },
    { HWPERF_C0PRFCNT1_SDCMISS,	"2$DMISS",	"Scache data misses" },
    { HWPERF_C0PRFCNT1_DMISPRED,"2$DWAYMP",	"Scache data way misprediction" },
    { HWPERF_C0PRFCNT1_EXTINTHIT,"INTHIT",	"External intervention hits in scache" },
    { HWPERF_C0PRFCNT1_EXTINVHIT,"INVHIT",	"External invalidate hits in scache" },
    { HWPERF_C0PRFCNT1_SPEXCLEAN,"UPCLEAN",	"Upgrade requests on clean scache lines" },
    { HWPERF_C0PRFCNT1_SPEXSHR,	"UPSHARE",	"Upgrade requests on shared scache lines" }
};

EventDesc_t	DescR12K[HWPERF_EVENTMAX] = {
    { HWPERF_PRFCNT_CYCLES,	"CYCLES",	"Cycles" },
    { HWPERF_PRFCNT_DINSTR,	"INSTRD",	"Decoded instructions" },
    { HWPERF_PRFCNT_DLD,	"LOADD",	"Decoded loads" },
    { HWPERF_PRFCNT_DST,	"STORED",	"Decoded stores" },
    { HWPERF_PRFCNT_MTHO,	"MHTO",		"Miss Handling Table occupancy" },
    { HWPERF_PRFCNT_FAILSC,	"SCONDF",	"Failed store conditionals" },
    { HWPERF_PRFCNT_RESCB,	"RESCBR",	"Resolved conditional branches" },
    { HWPERF_PRFCNT_QWWSC,	"WB2$",		"Quadwords written back to scache" },
    { HWPERF_PRFCNT_SCDAECC,	"ECC2$",	"Correctable scache data array errors" },  
    { HWPERF_PRFCNT_PICMISS,	"1$IMISS",	"Primary instruction cache misses" },           
    { HWPERF_PRFCNT_SICMISS,	"2$IMISS",	"Secondary instruction cache misses" },         
    { HWPERF_PRFCNT_IMISPRED,	"2$IWAYMP",	"Instruction misprediction from scache way predication table" },
    { HWPERF_PRFCNT_EXTINT,	"INT",		"External interventions" },
    { HWPERF_PRFCNT_EXTINV,	"INV",		"External invalidates" },
    { HWPERF_PRFCNT_ALUFPUFP,	"ALUFPUP",	"ALU/FPU forward progress" },
    { HWPERF_PRFCNT_GINSTR,	"INSTRG",	"Graduated instructions" },
    { HWPERF_PRFCNT_EXPREI,	"INSTRPE",	"Executed Prefetch Instructions" },
    { HWPERF_PRFCNT_PDCMISSPI,	"1$DMPI",	"Primary data cache misses by prefetch instructions" },
    { HWPERF_PRFCNT_GLD,	"LOADG",	"Graduated loads" },
    { HWPERF_PRFCNT_GST,	"STOREG",	"Graduated stores" }, 
    { HWPERF_PRFCNT_GSC,	"SCONDG",	"Graduated store conditionals" },
    { HWPERF_PRFCNT_GFINSTR,	"FPG",		"Graduated floating point instructions" },
    { HWPERF_PRFCNT_QWWPC,	"WB1$",		"Quadwords written back from primary data cache" }, 
    { HWPERF_PRFCNT_TLBMISS,	"TLBMISS",	"Translation lookaside buffer misses" },
    { HWPERF_PRFCNT_BRMISS,	"BRMP",		"Mispredicted branches" },
    { HWPERF_PRFCNT_PDCMISS,	"1$DMISS",	"Primary data cache misses" },
    { HWPERF_PRFCNT_SDCMISS,	"2$DMISS",	"Secondary data cache misses" },
    { HWPERF_PRFCNT_DMISPRED,	"2$DWAYMP",	"Data misprediction from scache way predication table" },
    { HWPERF_PRFCNT_EXTINTHIT,	"INTHIT",	"State of External intervention hits in scache" }, 
    { HWPERF_PRFCNT_EXTINVHIT,	"INVHIT",	"State of External invalidate hits in scache" }, 
    { HWPERF_PRFCNT_NHTAM,	"NHTAM",	"Miss Handling Table entries accessing memory" },
    { HWPERF_PRFCNT_SPEX,	"2$SPEX",	"Store/prefetch exclusive to block in secondary cache" }
};

EventDesc_t	DescR10K_Rev2_Ev14 =
    { HWPERF_C0PRFCNT0_VCOH,	"VCC",		"Virtual coherency condition" };

EventDesc_t	*EventDesc = DescR10K;

static hwperf_eventctrl_t	evctr_set;	/* counter control */
static hwperf_profevctrarg_t	evctr_args_set;	/* global counter control */
static hwperf_profevctrarg_t	evctr_args_get;	/* ditto */

static int	gen;			/* generation number */
static char	*cmd;			/* trimmed argv[0] */
static int	trustme = 0;

cpucount_t	cpucount;
int		evctr_debug = 0;
int		ActiveChanged;
int		Active[HWPERF_EVENTMAX];
int		Mux[HWPERF_EVENTMAX];
int		Sem[HWPERF_EVENTMAX];
__uint64_t	OldCount[HWPERF_EVENTMAX];
__uint64_t	Count[HWPERF_EVENTMAX];
int		Use[2];			/* mux count for each physical ctr */

static void
remap(hwperf_ctrl_t *cp)
{
    int		ctr;

    /*
     * try to remap if either counter is cycles(0,16) or
     * graduated instructions(15,17)
     * Warning ... this assumes we have only 2 physical registers, and is
     * R10000-specific
     */
    for (ctr = 0; ctr < HWPERF_EVENTMAX; ctr++) {
	if (!cp[ctr].hwperf_spec)
	    continue;
	if (ctr < HWPERF_CNT1BASE)
	    Use[0]++;
	else
	    Use[1]++;
    }

    if (Use[0] > Use[1] + 1 && cp[0].hwperf_spec) {
	cp[0].hwperf_spec = 0;
	Use[0]--;
	cp[16].hwperf_spec = 1;
	Use[1]++;
    }
    if (Use[1] > Use[0] + 1 && cp[16].hwperf_spec) {
	cp[16].hwperf_spec = 0;
	Use[1]--;
	cp[0].hwperf_spec = 1;
	Use[0]++;
    }

    if (Use[0] > Use[1] + 1 && cp[15].hwperf_spec) {
	cp[15].hwperf_spec = 0;
	Use[0]--;
	cp[17].hwperf_spec = 1;
	Use[1]++;
    }
    if (Use[1] > Use[0] + 1 && cp[17].hwperf_spec) {
	cp[17].hwperf_spec = 0;
	Use[1]--;
	cp[15].hwperf_spec = 1;
	Use[0]++;
    }

    if (evctr_debug) {
	fprintf(stderr, "After counter remap ...\n");
	for (ctr = 0; ctr < HWPERF_EVENTMAX; ctr++ ) {
	    if (!cp[ctr].hwperf_spec)
		continue;
	    fprintf(stderr, "Enable [%2d] %s %s\n", ctr, EventDesc[ctr].name, EventDesc[ctr].text);
	}
    }

}

/*
 * report control ...
 *	0	no messages
 *	1	messages to stderr
 */
static int
check_semantics(int ctr, int report)
{
    int		sts = EVCTR_SEM_OK;

    if (cpucount.r10k_rev2 > 0 && cpucount.r10k_rev3 > 0) {
	/* mixed rev R10Ks */
	if (ctr == 14) {	/* Virtual coherency condition */
	    if (report) {
		fprintf(stderr, "%s: Warning: counter %d has conflicting meaning:\n", cmd, ctr);
		fprintf(stderr, "\tR10000 Rev. 2: %s\n", DescR10K_Rev2_Ev14.text);
		fprintf(stderr, "\tR10000 Rev. 3: %s\n", DescR10K[ctr].text);
	    }
	    if (!trustme) {
		sts = EVCTR_SEM_BAD;
	    }
	    else {
		sts = EVCTR_SEM_R10K_REV;
	    }
	}
    }

    if (cpucount.r10k > 0 && cpucount.r12k > 0) {
	if (ctr == 1 ||		/* Issued instructions */
	    ctr == 2 ||		/* Issued loads */
	    ctr == 3 ||		/* Issued stores */
	    ctr == 4 ||		/* Issued store conditionals */
	    ctr == 5 ||		/* Failed store conditionals */
	    ctr == 6 ||		/* Decoded branches */
	    ( ctr == 14 && cpucount.r10k_rev3 == 0) ||		/* Virtual coherency conditon */
	    ctr == 16 ||		/* Cycles */
	    ctr == 17 ||		/* Graduated instructions */
	    ctr == 30 ||		/* Store/prefetch exclusive to clean block in secondary cache */
	    ctr == 31) {		/* Store/prefetch exclusive to shared block in secondary cache */
	    if (report) {
		fprintf(stderr, "%s: Warning: counter %d has conflicting meaning:\n", cmd, ctr);
		fprintf(stderr, "\tR10000: %s\n", DescR10K[ctr].text);
		fprintf(stderr, "\tR12000: %s\n", DescR12K[ctr].text);
	    }
	    if (!trustme) {
		sts = EVCTR_SEM_BAD;
	    }
	    else if (sts == EVCTR_SEM_OK) {
		sts = EVCTR_SEM_R10K_R12K;
	    }
	}
    }
    return sts;
}

void
evctr_init(char *myname)
{
    static char	*buf;
    static int	buflen = 0;
    static char *template="RXX000 x.x, xxx"; /* contains a few extra chars just in case */
    char	*p_type;
    char	*ver;
    int		need;
    int		i;
    int		ncpu;

    cmd = strdup(basename(myname));

    if (buflen == 0) {
	/*
	 * enough for 32 processors, expand as required below
	 */
	buflen = 32 * strlen(template);
	if ((buf = (char  *)malloc(buflen)) == NULL) {
	    fprintf(stderr, "%s: Error: malloc(%d): %s\n",
		    cmd, buflen, strerror(errno));
	    exit(1);
	}
    }

    if (sysinfo(_MIPS_SI_NUM_PROCESSORS, buf, buflen) < 0) {
	fprintf(stderr, "%s: Error: sysinfo(_MIPS_SI_NUM_PROCESSORS, ...): %s\n",
		cmd, strerror(errno));
	exit(1);
    }

    ncpu = atoi(buf);
    if (ncpu <= 0 || ncpu > 4096) {
	/* not reasonable */
	fprintf(stderr, "%s: Error: number of CPUs from sysinfo (%d) is not believable\n",
		cmd, ncpu);
	exit(1);
    }

    need = ncpu * strlen(template);
    if (need > buflen) {
	buflen = need;
	if ((buf = (char  *)realloc(buf, buflen)) == NULL) {
	    fprintf(stderr, "%s: Error: realloc(..., %d): %s\n",
			cmd, buflen, strerror(errno));
	    exit(1);
	}
    }

    if (sysinfo(_MIPS_SI_PROCESSORS, buf, buflen) < 0) {
	fprintf(stderr, "%s: Error: sysinfo(_MIPS_SI_PROCESSORS, ...): %s\n",
		cmd, strerror(errno));
	exit(1);
    }

    for (i = 0; i < ncpu; i++) {
	if (i == 0)
	    p_type = strtok(buf, " ,");
	else
	    p_type = strtok(NULL, " ,");
	if (p_type == NULL)
	    break;
	ver = strtok(NULL, " ,");

	if (evctr_debug)
	    fprintf(stderr, "CPU[%d]: %s Rev. %s\n", i, p_type, ver == NULL ? "?" : ver);

	if (strcmp(p_type, "R10000") == 0) {

	    /* Hw counter change in R10K from rev 2.x to rev 3.x !!
	     *
	     ****     14 = Virtual coherency conditions
	     * In rev 2.x this counter is incremented on the cycle after a virtual
	     *   address coherence condition is detected, provided that the
	     *   access was not flagged as a miss.  This condition can only be
	     *   realized for virtual page sizes of 4KB.
	     *
	     ****     14 = Functional unit completion cycles
	     * In rev 3.x this counter's meaning is changed.  It is incremented on
	     *   the cycle after either ALU1, ALU2, FPU1, or FPU2 marks an
	     *   instruction as "done."
	     ****
	     */
	    int		major;
	    int		minor;

	    cpucount.r10k++;
	    if (ver != NULL) {
		if (sscanf(ver, "%d.%d", &major, &minor) == 2) {
		    if (major == 2) {
			cpucount.r10k_rev2++;
			continue;
		    }
		    else if (major == 3) {
			cpucount.r10k_rev3++;
			continue;
		    }
		}
	    }
	    fprintf(stderr, "%s: Error: R10000 unknown Rev. %s\n",
		    cmd, ver == NULL ? "<NULL>" : ver);
	    exit(1);
	}
	else if (strcmp(p_type, "R12000") == 0) {
	    cpucount.r12k++;
	}
	else {
	    fprintf(stderr, "%s: Error: counters not supported on %s CPUs\n", cmd, buf);

	    exit(1);
	}
    }

    if (cpucount.r10k_rev2 > 0 && cpucount.r10k_rev3 == 0) {
	/* remap to R10000 rev 2 semantics */
	if (evctr_debug) {
	    fprintf(stderr, "%s: Warning: using R10000 Rev. 2 interpretation for counter 14\n", cmd);
	    fprintf(stderr, "\tR10000 Rev. 2: %s\n", DescR10K_Rev2_Ev14.text);
	}
	DescR10K[14] = DescR10K_Rev2_Ev14;
    }

    if (cpucount.r10k == 0 && cpucount.r12k > 0)
	/* pure R12000 system */
	EventDesc = DescR12K;

}

void
evctr_opt(char *optarg, int enable)
{
    int		ctr;
    char	*op;
    char	*p;

    if (strcmp(optarg, "?") == 0) {
	fprintf(stderr, "Available counters ...\n");
	for (ctr = 0; ctr < HWPERF_EVENTMAX; ctr++ ) {
	    if (check_semantics(ctr, 0) == EVCTR_SEM_BAD)
		continue;
	    fprintf(stderr, "[%2d] %-8s %s\n",
		ctr, EventDesc[ctr].name, EventDesc[ctr].text);
	}
	exit(0);
    }

    if (strcmp(optarg, "*") == 0) {
	for (ctr = 0; ctr < HWPERF_EVENTMAX; ctr++ ) {
	    if (check_semantics(ctr, 0) == EVCTR_SEM_BAD)
		continue;
	    if (cpucount.r12k == 0 && (ctr == 15 || ctr == 16))
		continue;
	    evctr_set.hwp_evctrl[ctr].hwperf_spec = enable;
	}
	return;
    }

    /*
     * process comma separated list of counter numbers and/or names
     */
    for ( ; ; ) {
	for (p = optarg; *p && *p != ','; p++)
	    ;
	if (*p == ',')
	    /* more */
	    *p++ = '\0';
	else
	    p = (char *)0;
	
	ctr = (int)strtol(optarg, &op, 10);
	if (*op != '\0') {
	    /* not a number, try name */
	    for (ctr = 0; ctr < HWPERF_EVENTMAX; ctr++) {
		if (strcasecmp(optarg, EventDesc[ctr].name) == 0)
		    break;
	    }
	}
	if (*optarg == '\0' || ctr < 0 || ctr >= HWPERF_EVENTMAX) {
	    fprintf(stderr, "%s: Error: illegal counter name or number (%s)\n",
		cmd, optarg);
	    exit(1);
	}
	evctr_set.hwp_evctrl[ctr].hwperf_spec = enable;
	if (p == (char *)0)
	    break;
	optarg = p;
    }
}

int
evctr_global_status(FILE *f)
{
    int		ctr;

    if (syssgi(SGI_EVENTCTR, HWPERF_GET_SYSEVCTRL, &evctr_args_get) < 0) {
	if (errno == EINVAL)
	    fprintf(stderr, "%s: Warning: global counters not enabled\n", cmd);
	else
	    fprintf(stderr, "%s: Warning: cannot get config for global counters: %s\n",
		    cmd, strerror(errno));
	return 0;
    }

    fprintf(f, "The following global counters are enabled:\n");
    for (ctr = 0; ctr < HWPERF_EVENTMAX; ctr++) {
	if (evctr_args_get.hwp_evctrargs.hwp_evctrl[ctr].hwperf_spec)
	    fprintf(f, "[%2d] %-8s %s\n",
		    ctr, EventDesc[ctr].name, EventDesc[ctr].text);
    }

    return 1;
}

int
evctr_global_release(void)
{
    if ((gen = syssgi(SGI_EVENTCTR, HWPERF_RELSYSCNTRS)) < 0) {
	if (errno == EINVAL)
	    fprintf(stderr, "%s: Warning: global counters already disabled\n", cmd);
	else
	    fprintf(stderr, "%s: Warning: cannot release global counters: %s\n",
		cmd, strerror(errno));
	return 0;
    }
    return 1;
}

int
evctr_global_set(void)
{
    int		ctr;
    int		active;
    int		enable;

    /* get current setup ... */
    if ((gen = syssgi(SGI_EVENTCTR, HWPERF_GET_SYSEVCTRL, &evctr_args_set)) < 0) {
	/*
	 * on error or if global ctrs inactive,
	 * assume everything is disabled
	 */
	active = 0;
	for (ctr = 0; ctr < HWPERF_EVENTMAX; ctr++)
	    evctr_args_set.hwp_evctrargs.hwp_evctrl[ctr].hwperf_spec = 0;
    }
    else {
	active = 1;
	/* mark active ones as "1" */
	for (ctr = 0; ctr < HWPERF_EVENTMAX; ctr++)
	    if (evctr_args_set.hwp_evctrargs.hwp_evctrl[ctr].hwperf_spec)
	        evctr_args_set.hwp_evctrargs.hwp_evctrl[ctr].hwperf_spec = 1;
    }

    /* turn some explicitly on/off ... */
    for (ctr = 0; ctr < HWPERF_EVENTMAX; ctr++) {
	if (evctr_set.hwp_evctrl[ctr].hwperf_spec == 0)
	    continue;

	if (evctr_set.hwp_evctrl[ctr].hwperf_spec == 1) {
	    /* turned on with -e */
	    if (check_semantics(ctr, 1) == EVCTR_SEM_BAD) {
		fprintf(stderr, "... this counter will not be enabled\n");
		evctr_set.hwp_evctrl[ctr].hwperf_spec = 0;
		continue;
	    }
	    evctr_args_set.hwp_evctrargs.hwp_evctrl[ctr].hwperf_spec = 1;
	}
	if (evctr_set.hwp_evctrl[ctr].hwperf_spec == -1) {
	    /*
	     * turned off with -d
	     * ... a bit trickier as we have to manage the equivalences
	     * [0,16] and [15,17]
	     */
	    evctr_args_set.hwp_evctrargs.hwp_evctrl[ctr].hwperf_spec = 0;
	    if (cpucount.r12k == 0) {
		if (ctr == 0)
		    evctr_args_set.hwp_evctrargs.hwp_evctrl[16].hwperf_spec = 0;
		else if (ctr == 16)
		    evctr_args_set.hwp_evctrargs.hwp_evctrl[0].hwperf_spec = 0;
		else if (ctr == 15)
		    evctr_args_set.hwp_evctrargs.hwp_evctrl[17].hwperf_spec = 0;
		else if (ctr == 17)
		    evctr_args_set.hwp_evctrargs.hwp_evctrl[15].hwperf_spec = 0;
	    }
	}
    }

    if (cpucount.r12k == 0)
	remap(evctr_args_set.hwp_evctrargs.hwp_evctrl);

    /*
     * set up the controls ...
     * if enabled thru here, we want no overflow thresholds,
     * no user signals, and all possible modes!
     */
    enable = 0;
    evctr_args_set.hwp_ovflw_sig = 0;
    for (ctr = 0; ctr < HWPERF_EVENTMAX; ctr++ ) {
	evctr_args_set.hwp_ovflw_freq[ctr] = 0;
	if (evctr_args_set.hwp_evctrargs.hwp_evctrl[ctr].hwperf_spec == 1) {
	    enable++;
	    evctr_args_set.hwp_evctrargs.hwp_evctrl[ctr].hwperf_creg.hwp_mode =
		HWPERF_CNTEN_U | HWPERF_CNTEN_K | HWPERF_CNTEN_E;
	    evctr_args_set.hwp_evctrargs.hwp_evctrl[ctr].hwperf_creg.hwp_ie = 1;
	    evctr_args_set.hwp_evctrargs.hwp_evctrl[ctr].hwperf_creg.hwp_ev =
			ctr < HWPERF_CNT1BASE ? ctr : ctr - HWPERF_CNT1BASE;
	}
	else
	    evctr_args_set.hwp_evctrargs.hwp_evctrl[ctr].hwperf_spec = 0;
    }

    if (active) {
	if (enable > 0) {
	    if ((gen = syssgi(SGI_EVENTCTR, HWPERF_SET_SYSEVCTRL, &evctr_args_set)) < 0) {
		fprintf(stderr, "%s: Warning: cannot modify global counter state: %s\n",
		    cmd, strerror(errno));
		return 0;
	    }
	}
	else
	    return evctr_global_release();
    } else if (enable > 0) {
        if ((gen = syssgi(SGI_EVENTCTR, HWPERF_ENSYSCNTRS, &evctr_args_set)) < 0) {
	    fprintf(stderr, "%s: Warning: cannot set global counters: %s\n",
		    cmd, strerror(errno));
	    return 0;
	}
    }

    return 1;
}

#define S_INIT		0
#define S_ACTIVE	1
#define S_INACTIVE	2
static int		state = S_INIT;

int
evctr_global_sample(int report)
{
    int			ctr;
    int			sts;

    ActiveChanged = 0;

    /* retrieve the counts */
    if ((sts = syssgi(SGI_EVENTCTR, HWPERF_GET_SYSCNTRS, (void *)&Count)) < 0) {
	if (state == S_INIT || state == S_ACTIVE) {
	    if (report) {
		if (errno == EINVAL)
		    fprintf(stderr, "%s: Warning: global counters not enabled\n", cmd);
		else
		    fprintf(stderr, "%s: Warning: cannot fetch global counters: %s\n",
			cmd, strerror(errno));
	    }
	    for (ctr = 0; ctr < HWPERF_EVENTMAX; ctr++) {
		Active[ctr] = 0;
		Sem[ctr] = EVCTR_SEM_OK;
	    }
	    ActiveChanged = 1;
	}
	state = S_INACTIVE;
	return 0;
    }

    if (state != S_ACTIVE || sts != gen) {
	/*
	 * config changed, mark old results as invalid ...
	 * necessary because counters maybe re-set to zero
	 */
	for (ctr = 0; ctr < HWPERF_EVENTMAX; ctr++)
	    OldCount[ctr] = -1;

	state = S_ACTIVE;	/* hope for the best */

	/* get current setup ... */
	if ((gen = syssgi(SGI_EVENTCTR, HWPERF_GET_SYSEVCTRL, &evctr_args_get)) < 0) {
	    if (report) {
		if (errno == EINVAL)
		    fprintf(stderr, "%s: Warning: global counters not enabled\n", cmd);
		else
		    fprintf(stderr, "%s: Warning: cannot get config for global counters: %s\n",
		    cmd, strerror(errno));
	    }
	    /* on error, assume everything is disabled */
	    for (ctr = 0; ctr < HWPERF_EVENTMAX; ctr++)
		evctr_args_get.hwp_evctrargs.hwp_evctrl[ctr].hwperf_spec = 0;
	    state = S_INACTIVE;
	}

	Use[0] = Use[1] = 0;
	for (ctr = 0; ctr < HWPERF_EVENTMAX; ctr++) {
	    if (!evctr_args_get.hwp_evctrargs.hwp_evctrl[ctr].hwperf_spec) {
		Active[ctr] = 0;
		continue;
	    }

	    Sem[ctr] = check_semantics(ctr, report);
	    Active[ctr] = 1;

	    if (ctr < HWPERF_CNT1BASE)
		Use[0]++;
	    else
		Use[1]++;
	}

	for (ctr = 0; ctr < HWPERF_EVENTMAX; ctr++) {
	    if (Active[ctr]) {
		if (ctr < HWPERF_CNT1BASE)
		    Mux[ctr] = Use[0];
		else
		    Mux[ctr] = Use[1];
	    }
	    else
		Mux[ctr] = 0;
	}
	ActiveChanged = 1;

	if (state == S_INACTIVE)
	    return 0;
    }

    return 1;
}


void
evctr_trustme(void)
{
    trustme = 1;
}

