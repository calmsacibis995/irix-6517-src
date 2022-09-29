#ifndef TRANSARC_ICL_MACROS_H
#define TRANSARC_ICL_MACROS_H 1

/*
 * @OSF_COPYRIGHT@
 */

/* $Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/icl/RCS/icl_macros.h,v 65.2 1998/03/02 22:31:51 bdr Exp $ */

/* Copyright (C) 1996, 1993 Transarc Corporation - All rights reserved */

/*
 * HISTORY
 * $Log: icl_macros.h,v $
 * Revision 65.2  1998/03/02 22:31:51  bdr
 * Added ICL_TYPE_64BITLONG define for icl tracing on 64 bit machines.
 *
 * Revision 65.1  1997/10/20  19:17:40  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.5.1  1996/10/02  17:52:17  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:40:56  damon]
 *
 * $EndLog$
 */

#define ICL_LOGSPERSET		8	/* max logs per set */
#define ICL_LOGPERTHREAD	6	/* ln2(max logs) in log-per-thr mode */
#define ICL_DEFAULTEVENTS	1024	/* default events per event set */
#define ICL_DEFAULT_LOGSIZE	60*1024	/* number of words in default log size */
#define ICL_MAXLOGS		1000    /* bump this limit up if necessary */

/* locking hierarchy: lock icl_set first, then icl_log, then the
 * low-level icl_lock.
 */

/* icl set flags */
#define ICL_SETF_DELETED	1
#define	ICL_SETF_ACTIVE		2
#define	ICL_SETF_FREED		4
#define	ICL_SETF_PERSISTENT	8

#ifdef ICL_DEFAULT_ENABLED
#define ICL_DEFAULT_SET_STATES	ICL_SETF_ACTIVE
#else /* ICL_DEFAULT_ENABLED */

#ifdef ICL_DEFAULT_DISABLED
#define ICL_DEFAULT_SET_STATES	ICL_SETF_FREED
#endif /* ICL_DEFAULT_DISABLED */

#endif /* ICL_DEFAULT_ENABLED */

#ifndef ICL_DEFAULT_SET_STATES
/* if not changed already, default to ACTIVE on created sets */
#define ICL_DEFAULT_SET_STATES	ICL_SETF_ACTIVE
#endif /* ICL_DEFAULT_SET_STATES */

/* bytes required by eventFlags array, for x events */
#define ICL_EVENTBYTES(x)	((((x) - 1) | 7) + 1)

/* functions for finding a particular event */
#define ICL_EVENTBYTE(x)	(((x) & 0x3ff) >> 3)
#define ICL_EVENTMASK(x)	(1 << ((x) & 0x7))
#define ICL_EVENTOK(setp, x)	((x&0x3ff) >= 0 && (x&0x3ff) < (setp)->nevents)

/* define ICL syscalls by name!! */
#define ICL_OP_COPYOUT		1
#define	ICL_OP_ENUMLOGS		2
#define	ICL_OP_CLRLOG		3
#define	ICL_OP_CLRSET		4
#define ICL_OP_CLRALL		5
#define ICL_OP_ENUMSETS		6
#define	ICL_OP_SETSTAT		7
#define	ICL_OP_SETSTATALL	8
#define ICL_OP_SETLOGSIZE	9
#define	ICL_OP_ENUMLOGSBYSET	10
#define ICL_OP_GETSETINFO	11
#define ICL_OP_GETLOGINFO	12
#define ICL_OP_COPYOUTCLR	13

/* define setstat commands */
#define ICL_OP_SS_ACTIVATE	1
#define ICL_OP_SS_DEACTIVATE	2
#define ICL_OP_SS_FREE		3

/* define set status flags */
#define	ICL_FLAG_ACTIVE		1
#define	ICL_FLAG_FREED		2

/* The format of the circular log is:
 * 1'st word:
 * <8 bits: size of record in longs> <6 bits: p1 type> <6 bits: p2 type>
 *     <6 bits: p3 type> <6 bits: p4 type>
 * <32 bits: opcode>
 * <32 bits: pid>
 * <32 bits: timestamp in microseconds>
 * <p1 rep>
 * <p2 rep>
 * <p3 rep>
 * <p4 rep>
 *
 * Note that the representation for each parm starts at a new 32 bit
 * offset, and only the represented parameters are marshalled.
 * You can tell if a particular parameter exists by looking at its
 * type; type 0 means the parameter does not exist.
 */

/* icl log flags */
#define ICL_LOGF_DELETED	1	/* freed */
#define ICL_LOGF_WAITING	2	/* waiting for output to appear */
#define ICL_LOGF_PERSISTENT	4	/* persistent */
#define ICL_LOGF_NOLOCK		8	/* !shared: no AppendRecord locking */

/* macros for calling these things easily */
#define ICL_SETACTIVE(setp)	((setp) && (setp->states & ICL_SETF_ACTIVE))

/*
 * DFS_NOTRACING compile time flag removes any icl traces.
 * Also DFS_NOCODECOVERAGE compile flag removes code coverage counting
 * mechanism in Episode and DFS_NOASSERTS compile flags removes all
 * assertions.
 */

#ifdef DFS_NOTRACING
#define icl_Trace0(set, id) 0
#define icl_Trace1(set, id, t1, p1) 0
#define icl_Trace2(set, id, t1, p1, t2, p2) 0
#define icl_Trace3(set, id, t1, p1, t2, p2, t3, p3) 0
#define icl_Trace4(set, id, t1, p1, t2, p2, t3, p3, t4, p4) 0
#else
#define icl_Trace0(set, id) \
    (ICL_SETACTIVE(set) ? icl_Event0(set, id, 1<<24) : 0)
#define icl_Trace1(set, id, t1, p1) \
    (ICL_SETACTIVE(set) ? icl_Event1(set, id, (1<<24)+((t1)<<18), (long)(p1)) : 0)
#define icl_Trace2(set, id, t1, p1, t2, p2) \
    (ICL_SETACTIVE(set) ? icl_Event2(set, id, (1<<24)+((t1)<<18)+((t2)<<12), \
				       (long)(p1), (long)(p2)) : 0)
#define icl_Trace3(set, id, t1, p1, t2, p2, t3, p3) \
    (ICL_SETACTIVE(set) ? icl_Event3(set, id, (1<<24)+((t1)<<18)+((t2)<<12)+((t3)<<6), \
				       (long)(p1), (long)(p2), (long)(p3)) : 0)
#define icl_Trace4(set, id, t1, p1, t2, p2, t3, p3, t4, p4) \
    (ICL_SETACTIVE(set) ? icl_Event4(set, id, (1<<24)+((t1)<<18)+((t2)<<12)+((t3)<<6)+(t4), \
				       (long)(p1), (long)(p2), (long)(p3), (long)(p4)) : 0)
#endif

/* types for icl_trace macros; values must be less than 64.  If
 * you add a new type here, don't forget to check for ICL_MAXEXPANSION
 * changing.
 */
#define ICL_TYPE_NONE		0	/* parameter doesn't exist */
#define ICL_TYPE_LONG		1
#define ICL_TYPE_POINTER	2
#ifdef SGIMIPS64
#define ICL_TYPE_64BITLONG      2       /* 64 bits when long is 64 bits */
#else  /* SGIMIPS64 */
#define ICL_TYPE_64BITLONG      1	/* 32 bit long for 32 bit systems */
#endif /* SGIMIPS64 */
#define ICL_TYPE_HYPER		3
#define ICL_TYPE_STRING		4
#define ICL_TYPE_FID		5
#define	ICL_TYPE_UNIXDATE	6
#define	ICL_TYPE_UUID		7

/* max # of words put in the printf buffer per parameter */
#define ICL_MAXEXPANSION	4

/* flags for icl dump routines */
#define ICL_DUMP_ALL 0
#define ICL_DUMP_SET 1
#define ICL_DUMP_LOG 2

/* flags for icl dump styles */
#define ICL_DUMP_FORMATTED 0
#define ICL_DUMP_RAW 1

/* define flags to be used by icl_CreateSetWithFlags */
#define ICL_CRSET_FLAG_DEFAULT_ON	1
#define ICL_CRSET_FLAG_DEFAULT_OFF	2
#define ICL_CRSET_FLAG_PERSISTENT	4
#define ICL_CRSET_FLAG_PERTHREAD	8

/* define flags to be used by icl_CreateLogWithFlags */
#define ICL_CRLOG_FLAG_PERSISTENT	1

/* input flags */
#define ICL_COPYOUTF_WAITIO		1	/* block for output */
#define ICL_COPYOUTF_CLRAFTERREAD	2	/* clear log after reading */
/* output flags */
#define ICL_COPYOUTF_MISSEDSOME		1	/* missed some output */

#endif /* TRANSARC_ICL_MACROS_H */
