/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 * HISTORY
 * $Log: osi_calltrace.h,v $
 * Revision 65.1  1997/10/20 19:17:42  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.10.1  1996/10/02  18:10:27  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:44:48  damon]
 *
 * $EndLog$
 */
/*
 *      Copyright (C) 1992 Transarc Corporation
 *      All rights reserved.
 */

/*
 * osi_calltrace.h
 *
 * Support for function call tracing.
 */

#ifndef _OSI_CALLTRACE_
#define _OSI_CALLTRACE_

#include <dcedfs/param.h>

#if (!defined(HAS_OSI_CALLTRACE_MACH_H) || defined(TRACE_TYPE_ICL))

/* 
 * Assume icl.h has already been included. Otherwise if we include
 * icl.h here, we can end up redefining structures/typedefs.
 * Its possible to refine icl.h to prevent this, but given icl is used
 * all over DFS code, its a smarter hack to assume this.
 * We only include the header file containing macro defn for icl_Trace0.
 * 
 * icl.h is now explicity included in the first cpp phase via the modified
 * make rule (in src/lbe/mk/osf.obj.mk) for generating object files from
 * source files.
 */

#include <dcedfs/icl_macros.h>

#define OSI_CALLTRACE_ENTRY_EVENT	(0x4 << 28) 
#define OSI_CALLTRACE_EXIT_EVENT	(0x6 << 28)

extern struct icl_set *traceIclSetP;

#define OSI_CALLTRACE_ENTRY(id, name) \
    icl_Trace0(traceIclSetP, (OSI_CALLTRACE_ENTRY_EVENT | (id)))

#define OSI_CALLTRACE_EXIT(id, name) \
    icl_Trace0(traceIclSetP, (OSI_CALLTRACE_EXIT_EVENT | (id)))

#else 
#include <dcedfs/osi_calltrace_mach.h>
#endif /*!defined(HAS_OSI_CALLTRACE_MACH_H)||defined(TRACE_TYPE_ICL) */

/* The variable to hold the value of retExpr is 
 * _TRANSARC_TRACE_FUN_RETVAR which is declared for all functions
 * traced by the trace preprocessor 
 */
#define OSI_CALLTRACE_RETURN_EXPR(id, name, retExpr) \
    do { \
        _TRANSARC_TRACE_FUN_RETVAR = (retExpr); \
	OSI_CALLTRACE_EXIT(id, name); \
	return _TRANSARC_TRACE_FUN_RETVAR; \
    } while (0)

#define OSI_CALLTRACE_RETURN_NOEXPR(id, name) \
    do { \
	OSI_CALLTRACE_EXIT(id, name); \
	return; \
    } while (0)


#endif	/* _OSI_CALLTRACE_ */

