/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 * HISTORY
 * $Log: ktime.h,v $
 * Revision 65.3  1999/07/21 19:01:14  gwehrman
 * Defined SGIMIPS for this development header.  Fix for bug 657352.
 *
 * Revision 65.2  1997/12/16 17:05:49  lmc
 *  1.2.2 initial merge of file directory
 *
 * Revision 65.1  1997/10/20  19:19:24  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.9.1  1996/10/02  21:13:22  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:51:08  damon]
 *
 * Revision 1.1.4.1  1994/06/09  14:24:59  annie
 * 	fixed copyright in src/file
 * 	[1994/06/09  13:37:01  annie]
 * 
 * Revision 1.1.2.3  1993/01/21  16:31:42  cjd
 * 	embedded copyright notice
 * 	[1993/01/20  18:25:23  cjd]
 * 
 * Revision 1.1.2.2  1992/11/24  20:42:55  bolinger
 * 	Change include file install directory from .../afs to .../dcedfs.
 * 	[1992/11/22  19:38:35  bolinger]
 * 
 * Revision 1.1  1992/01/19  02:45:38  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */
/*
*/
/* Copyright (C) 1990 Transarc Corporation - All rights reserved */
/*
 * P_R_P_Q_# (C) COPYRIGHT IBM CORPORATION 1987, 1988
 * LICENSED MATERIALS - PROPERTY OF IBM
 * REFER TO COPYRIGHT INSTRUCTIONS FORM NUMBER G120-2083
 */

#ifndef __INCL_KTIME_
#define __INCL_KTIME_ 1
#ifndef SGIMIPS
#define SGIMIPS
#endif /* !SGIMIPS */

#include <dcedfs/param.h>
#include <dcedfs/stds.h>

struct ktime_date {
    long mask;		/* mask of valid fields */
    short year;
    short month;
    short day;
    short hour;
    short min;
    short sec;
};

/* could we make these bits the same? -ota */

#define KTIMEDATE_YEAR		1
#define KTIMEDATE_MONTH		2
#define KTIMEDATE_DAY		4
#define KTIMEDATE_HOUR		8
#define KTIMEDATE_MIN		0x10
#define KTIMEDATE_SEC		0x20
#define KTIMEDATE_NEVER		0x1000	/* special case for never */
#define KTIMEDATE_NOW		0x2000	/* special case for now */

/* note that this if different from the value used by ParcePeriodic (sigh). */
#define KTIMEDATE_NEVERDATE	0xffffffff

struct ktime {
    int mask;
    short hour;	    /* 0 - 23 */
    short min;	    /* 0 - 60 */
    short sec;	    /* 0 - 60 */
    short day;	    /* 0 is sunday */
};

#define	KTIME_HOUR	1	/* hour should match */
#define	KTIME_MIN	2
#define	KTIME_SEC	4
#define	KTIME_TIME	7	/* all times should match */
#define	KTIME_DAY	8	/* day should match */
#define	KTIME_NEVER	0x10	/* special case: never */
#define	KTIME_NOW	0x20	/* special case: right now */

/* the exported routines from this package */
#ifdef SGIMIPS
IMPORT char * ktime_DateOf _TAKES((time_t atime));
#else
IMPORT char * ktime_DateOf _TAKES((long atime));
#endif /* SGIMIPS */

IMPORT int ktime_ParsePeriodic _TAKES((
				       char *		adate,
				       struct ktime *	ak
				     ));
IMPORT int ktime_DisplayString _TAKES((
				       struct ktime *	aparm,
				       char *		string
				     ));
IMPORT long ktime_next _TAKES((
			       struct ktime *	aktime,
			       long		afrom
			     ));
IMPORT long ktime_DateToLong _TAKES((
				     char *	adate,
				     long *	along
				   ));
IMPORT long ktime_ParseDate _TAKES((
				    char *		adate,
				    struct ktime_date *	akdate
				  ));
IMPORT char * ktime_GetDateUsage _TAKES((void));
IMPORT long ktime_InterpretDate _TAKES((struct ktime_date * akdate));

#endif /* __INCL_KTIME_ */
