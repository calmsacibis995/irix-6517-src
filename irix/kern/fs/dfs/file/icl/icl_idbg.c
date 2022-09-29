/*
 * icl_idbg.c
 *
 *      idbg entry points for icl functions.
 *
 *
 * Copyright  1998 Silicon Graphics, Inc.  ALL RIGHTS RESERVED
 * 
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND
 * 
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the 
 * Rights in Technical Data and Computer Software clause at DFARS 252.227-7013 
 * and/or in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement. Unpublished -- rights reserved under the Copyright Laws 
 * of the United States. Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd., Mountain View, CA 94039-7311.
 * 
 * THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF SGI
 * 
 * The copyright notice above does not evidence any actual or intended 
 * publication or disclosure of this source code, which includes information 
 * that is the confidential and/or proprietary, and is a trade secret,
 * of Silicon Graphics, Inc. Any use, duplication or disclosure not 
 * specifically authorized in writing by Silicon Graphics is strictly 
 * prohibited. ANY DUPLICATION, MODIFICATION, DISTRIBUTION,PUBLIC PERFORMANCE,
 * OR PUBLIC DISPLAY OF THIS SOURCE CODE WITHOUT THE EXPRESS WRITTEN CONSENT 
 * OF SILICON GRAPHICS, INC. IS STRICTLY PROHIBITED.  THE RECEIPT OR POSSESSION
 * OF THIS SOURCE CODE AND/OR INFORMATION DOES NOT CONVEY ANY RIGHTS 
 * TO REPRODUCE, DISCLOSE OR DISTRIBUTE ITS CONTENTS, OR TO MANUFACTURE, USE, 
 * OR SELL ANYTHING THAT IT MAY DESCRIBE, IN WHOLE OR IN PART.
 *
 * $Log: icl_idbg.c,v $
 * Revision 65.1  1998/09/22 20:30:52  gwehrman
 * Definitions for two idbg commands, dfslogs and dfstrace, and supporting
 * functions.
 *
 */

/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */

/*
 * Copyright (C) 1992, 1996 Transarc Corporation.
 * All Rights Reserved.
 */

#ident "$Revision: 65.1 $"


#include <sys/types.h>
#include <sys/idbg.h>
#include <sys/idbgentry.h>

#include <icl.h>        /* includes standard stuff */
#include <icl_errs.h>
#include <icl_idbg.h>

static char*
idbgicl_ctime(signed32 *utime)
{
#define SECS_PER_4YR	126230400	/* # secs per 4 years		  */
#define SECS_PER_DAY    86400           /* # secs per day       	  */
#define SECS_PER_HOUR   3600            /* # secs per hour      	  */
#define SECS_PER_MIN    60              /* # secs per minute    	  */
#define DAYS_PER_4YR	1461		/* # days per 4 years   	  */
#define DAYS_PER_2YR	730		/* # days per 2 years (non-leap)  */
#define DAYS_PER_1YR	365		/* # days per 1 year  (non-leap)  */
#define LEAP_DAY	59

    static char ts[20];
    int cal[12] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
    int year;
    int days;
    int mnth;
    int hour;
    int mins;
    int rems;

    /* calculate year */
    year = 1970;
    days = *utime / SECS_PER_DAY;
    rems = *utime % SECS_PER_DAY;
    while (days >= DAYS_PER_4YR) {
	year += 4;
	days -= DAYS_PER_4YR;
    }
    if (days >= DAYS_PER_2YR) {
	year += 2;
	days -= DAYS_PER_2YR;
    }
    if (days > LEAP_DAY) {
	days -= 1;
    }
    else if (days == LEAP_DAY) {
	mnth = 1;
	days = 28;
	goto leap;
    }
    if (days >= DAYS_PER_1YR) {
	year += 1;
	days -= DAYS_PER_1YR;
    }

leap:
    /* calculate month */
    mnth=0;
    while ((mnth < 11) && (days < cal[mnth])) {
	mnth++;
    }

    /* calculate day */
    days -= cal[mnth];

    /* calculate hour */
    hour = rems / SECS_PER_HOUR;
    rems %= SECS_PER_HOUR;

    /* calculate minutes */
    mins = rems / SECS_PER_MIN;

    /* calculate seconds */
    rems %= SECS_PER_MIN;

    sprintf(ts, "%04d-%02d-%02d-%02d:%02d:%02d",
	    year, mnth+1, days+1, hour, mins, rems);

    return ts;
}

/* display a single record.
 * alp points at the first word in the array to be interpreted
 * rsize gives the # of words in the array
 */
static int
idbgicl_DisplayRecord(register signed32 *alp)
{
    int i;
    long temp;
    int type;
    int pix;                    /* index in alp */

    temp = alp[0];      /* type encoded in low-order 24 bits, t0 high */
    pix = 4;

    /*
     * The w32 print format size flag is an undocumented feature of
     * qprintf().  The implementation is found in errprintf(), os/printf.c.
     */

    qprintf("  %d.%06d ", alp[3]/1000000, alp[3] % 1000000); /* time */
#if (_MIPS_SZLONG == 32)
    qprintf("0x%x ", alp[2]); /* kthread address */
#endif
#if (_MIPS_SZLONG == 64)
    /*
     * Only the lower 32 bits of the kthread address are saved.  We need
     * some way to get the upper 32 bits of the kthread id.  I'm guessing
     * that I can add 0xa800000000000000 and get the proper result.
     */
    qprintf("0x%llx ", (0xa800000000000000 + alp[2])); /* kthread address */
#endif
    qprintf("%d ", alp[1]); /* message code */

    /* now decode each parameter and print it */
    pix = 4;
    for (i = 0; i < 4; i++) {
	type = (temp >> (18 - i*6)) & 0x3f;
	if (type == ICL_TYPE_NONE) break;
	else if (type == ICL_TYPE_LONG) {
#if (_MIPS_SZLONG == 32)
	    qprintf("0x%x ", alp[pix]);
	    pix += 1;
#endif
#if (_MIPS_SZLONG == 64)
	    qprintf("0x%llx ", alp[pix+1]);
	    pix += 2;
#endif
	}
	else if (type == ICL_TYPE_POINTER) {
#if (_MIPS_SZPTR == 32)
	    qprintf("0x%x ", alp[pix]);
	    pix += 1;
#endif
#if (_MIPS_SZPTR == 64)
	    qprintf("0x%llx ", alp[pix+1]);
	    pix += 2;
#endif
	}
	else if (type == ICL_TYPE_HYPER) {
	    qprintf("0x%w32x%w32x ", alp[pix], alp[pix+1]);
	    pix += 2;
	} else if (type == ICL_TYPE_FID || type == ICL_TYPE_UUID) {
	    qprintf("%w32x.%w32x.%w32x.%w32x ",  alp[pix], alp[pix+1],
		   alp[pix+2], alp[pix+3]);
	    pix += 4;
	} else if (type == ICL_TYPE_STRING) {
	    qprintf("%s ", (char *) &alp[pix]);
	    pix += (strlen((char *)&alp[pix]) + 4) >> 2;
	}
	else if (type == ICL_TYPE_UNIXDATE) {
	    qprintf("%s ", idbgicl_ctime(&alp[pix]));
	    pix += 1;
	}
    }
    qprintf("\n");        /* done with line */
    return 0;
}

#define ICL_MAXNAMELEN 16

struct logInfo {
    struct logInfo *li_nextp;
    struct icl_log *li_log;
} *allInfo = 0;

/*
 * Enumerate the dfstrace logs available
 */
void
idbg_dfslogs (void)
{
    char spstr[80];
    int spsz = 0;
    int i = 1;
    struct icl_log *logp;

#define ADDSP(n) {int j = spsz + n; \
	while (spsz < j) spstr[spsz++] = ' '; \
	spstr[spsz] = NULL;}
#define DELSP(n) {spsz -= n; spstr[spsz] = NULL;}

    qprintf("DFS logs:\n");
    spstr[spsz]=NULL;
    logp = icl_allLogs;
    ADDSP(2);
    while (logp) {
	qprintf("%s%d  %s\n", spstr, i++, logp->name);
	ADDSP(2);
	qprintf("%slogSize=%d  logElements=%d  firstUsed=%d  firstFree=%d\n",
		spstr, logp->logSize, logp->logElements,
		logp->firstUsed, logp->firstFree);
	qprintf("%sdatap=0x%x", spstr, logp->datap);
	qprintf("  states=0x%x < ", logp->states);
	if (logp->states & ICL_LOGF_DELETED) {
	    qprintf("DELETED ");
	}
	if (logp->states & ICL_LOGF_WAITING) {
	    qprintf("WAITING ");
	}
	if (logp->states & ICL_LOGF_PERSISTENT) {
	    qprintf("PERSISTENT ");
	}
	if (logp->states & ICL_LOGF_NOLOCK) {
	    qprintf("NOLOCK ");
	}
	qprintf(">\n");
	qprintf("%slastTS=%d.%d\n", spstr,
		logp->lastTS/1000000, logp->lastTS%1000000);

	DELSP(2);
	logp = logp->nextp;
    }
    DELSP(2);
}

/*
 * Dump the given dfstrace log.  If log == 0, dump all dfstrace logs.
 */
void
idbg_dfstrace (int log)
{
    signed32 *bufferp;
    signed32 i;
    signed32 nwords;
    signed32 ix;
    signed32 rlength;
    struct icl_log *logp;

    /* now print out the contents of each log */
    for (logp = icl_allLogs, i = 1; logp; logp = logp->nextp, i++) {
	if (log && (i != log)) {
	    continue;
	}

        qprintf("\nContents of log %s:\n", logp->name);
        nwords = 0;             /* total words in log */
	/* display all the entries in the log */
	bufferp = logp->datap;
	if (logp->baseCookie > 0) {
	    qprintf("Log wrapped; data missing.\n");
	}
	qprintf("  time    kthread code    p1      p2      p3      p4\n");

	if (logp->firstUsed > logp->firstFree) {
	    nwords = logp->logSize - logp->firstUsed;
	    for(ix = logp->firstUsed; ix < logp->logSize;) {
		/* start of a record */
		rlength = (bufferp[ix] >> 24) & 0xff;
		if (rlength <= 0) {
		    qprintf("Internal error: 0 length record at 0x%x\n",
			    &bufferp[ix]);
		    ix++;
		    continue;
		}
		/* ensure that entire record fits */
		if (ix + rlength > nwords) {
		    /* doesn't fit */
		    break;
		}
		/* print the record */
		idbgicl_DisplayRecord(&bufferp[ix]);
		ix += rlength;
	    } /* for loop displaying buffer */
	    ix = 0;
	}
	else {
	    ix = logp->firstUsed;
	}
	nwords = logp->firstFree - ix;
        while(ix < logp->firstFree) {
            /* start of a record */
            rlength = (bufferp[ix] >> 24) & 0xff;
            if (rlength <= 0) {
                qprintf("Internal error: 0 length record at 0x%x\n",
			&bufferp[ix]);
                continue;
            }
            /* ensure that entire record fits */
            if (ix + rlength > nwords) {
                /* doesn't fit */
                break;
            }
            /* print the record */
            idbgicl_DisplayRecord(&bufferp[ix]);
            ix += rlength;
        } /* for loop displaying buffer */
	if (!log) {
	    break;
	}
    } /* for loop displaying logs */
}

