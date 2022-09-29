/**************************************************************************
 *									  *
 * 		 Copyright (C) 1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ifndef	_PROC_TRACE_H_
#define	_PROC_TRACE_H_	1

#ifdef	DEBUG
#define	PROC_TRACE 1
#endif

#ifdef	PROC_TRACE

#include <sys/ktrace.h>

#define	PROC_TRACE_REFERENCE	0x01
#define	PROC_TRACE_EXISTENCE	0x02
#define PROC_TRACE_STATE	0x04
#define PROC_TRACE_MIGRATE	0x08
#define PROC_TRACE_OPS		0x10

extern struct ktrace	*proc_trace;
extern pid_t		proc_trace_pid;
extern unsigned		proc_trace_mask;

#define PROC_TRACE2(mask,a,b) { \
	if ((proc_trace_pid == (b) || \
	    proc_trace_pid == -current_pid() || \
	    proc_trace_pid == -1) && proc_trace_mask&(mask)) { \
		KTRACE4(proc_trace, \
                        (a), (b), \
                        "cell", cellid()); \
	} \
}

#define PROC_TRACE4(mask,a,b,c,d) { \
	if ((proc_trace_pid == (b) || \
	    proc_trace_pid == -current_pid() || \
	    proc_trace_pid == -1) && proc_trace_mask&(mask)) { \
		KTRACE6(proc_trace, \
                        (a), (b), \
                        "cell", cellid(), \
                        (c), (d)); \
	} \
}

#define PROC_TRACE6(mask,a,b,c,d,e,f) { \
	if ((proc_trace_pid == (b) || \
	    proc_trace_pid == -current_pid() || \
	    proc_trace_pid == -1) && proc_trace_mask&(mask)) { \
		KTRACE8(proc_trace, \
                        (a), (b), \
                        (c), (d), \
                        "cell", cellid(), \
			(e), (f)); \
	} \
}

#define PROC_TRACE8(mask,a,b,c,d,e,f,g,h) { \
	if ((proc_trace_pid == (b) || \
	    proc_trace_pid == -current_pid() || \
	    proc_trace_pid == -1) && proc_trace_mask&(mask)) { \
		KTRACE10(proc_trace, \
                        (a), (b), \
                        (c), (d), \
                        "cell", cellid(), \
                        (e), (f), \
                        (g), (h)); \
	} \
}

#define PROC_TRACE10(mask,a,b,c,d,e,f,g,h,i,j) { \
	if ((proc_trace_pid == (b) || \
	    proc_trace_pid == -current_pid() || \
	    proc_trace_pid == -1) && proc_trace_mask&(mask)) { \
		KTRACE12(proc_trace, \
                        (a), (b), \
                        (c), (d), \
                        "cell", cellid(), \
                        (e), (f), \
                        (g), (h), \
                        (i), (j)); \
	} \
}

#define PROC_TRACE12(mask,a,b,c,d,e,f,g,h,i,j,k,l) { \
	if ((proc_trace_pid == (b) || \
	    proc_trace_pid == -current_pid() || \
	    proc_trace_pid == -1) && proc_trace_mask&(mask)) { \
		KTRACE14(proc_trace, \
                        (a), (b), \
                        (c), (d), \
                        "cell", cellid(), \
                        (e), (f), \
                        (g), (h), \
                        (i), (j), \
			(k), (l)); \
	} \
}

#else

#define PROC_TRACE2(mask,a,b)
#define PROC_TRACE4(mask,a,b,c,d)
#define PROC_TRACE6(mask,a,b,c,d,e,f)
#define PROC_TRACE8(mask,a,b,c,d,e,f,g,h)
#define PROC_TRACE10(mask,a,b,c,d,e,f,g,h,i,j)
#define PROC_TRACE12(mask,a,b,c,d,e,f,g,h,i,j,k,l)

#endif	/* PROC_TRACE */

#endif	/* _PROC_TRACE_H_ */
