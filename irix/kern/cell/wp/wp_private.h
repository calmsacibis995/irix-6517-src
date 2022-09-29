/**************************************************************************
 *									  *
 * 		 Copyright (C) 1994-1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident "$Id: wp_private.h,v 1.4 1997/10/07 20:51:48 sp Exp $"

#ifndef	_WP_PRIVATE_H_
#define	_WP_PRIVATE_H_ 1

#define	WP_AGAIN	1
#define	WP_FOUND	2
#define	WP_NOTFOUND	3
#define WP_ERROR	4

#ifdef	DEBUG
#include <sys/ktrace.h>
#include <sys/idbgentry.h>

extern struct ktrace *wp_trace;
extern long wp_trace_id;

#define	WP_KTRACE2(a,b) { \
	if (wp_trace_id == (b) || wp_trace_id == -1) { \
		KTRACE4(wp_trace, \
			(a), (void *)(__psint_t)(b), \
			"cell", (void *)(__psint_t)cellid()); \
	} \
}
#define	WP_KTRACE4(a,b,c,d) { \
	if (wp_trace_id == (b) || wp_trace_id == -1) { \
		KTRACE6(wp_trace, \
			(a), (b), \
			"cell", cellid(), \
			(c), (d)); \
	} \
}
#define	WP_KTRACE6(a,b,c,d,e,f) { \
	if (wp_trace_id == (b) || wp_trace_id == -1) { \
		KTRACE8(wp_trace, \
			(a), (b), \
			"cell", cellid(), \
			(c), (d), \
			(e), (f)); \
	} \
}
#define	WP_KTRACE8(a,b,c,d,e,f,g,h) { \
	if (wp_trace_id == (b) || wp_trace_id == -1) { \
		KTRACE10(wp_trace, \
			(a), (b), \
			"cell", cellid(), \
			(c), (d), \
			(e), (f), \
			(g), (h)); \
	} \
}
#define	WP_KTRACE10(a,b,c,d,e,f,g,h,i,j) { \
	if (wp_trace_id == (b) || wp_trace_id == -1) { \
		KTRACE12(wp_trace, \
			(a), (b), \
			"cell", cellid(), \
			(c), (d), \
			(e), (f), \
			(g), (h), \
			(i), (j)); \
	} \
}
#else
#define	WP_KTRACE2(a,b)
#define	WP_KTRACE4(a,b,c,d)
#define	WP_KTRACE6(a,b,c,d,e,f)
#define	WP_KTRACE8(a,b,c,d,e,f,g,h)
#define	WP_KTRACE10(a,b,c,d,e,f,g,h,i,j)
#endif

#endif	/* _WP_PRIVATE_H_ */
