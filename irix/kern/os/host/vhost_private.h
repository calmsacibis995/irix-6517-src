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
#ident	"$Id: vhost_private.h,v 1.3 1996/10/07 16:33:19 cp Exp $"

#ifndef	_VHOST_PRIVATE_H_
#define	_VHOST_PRIVATE_H_	1

#include <sys/ktrace.h>
#include <ksys/vproc.h>
#include <ksys/vhost.h>

#define	VHOST_BHV_PP	BHV_POSITION_BASE
#define	VHOST_BHV_DS	VHOST_BHV_PP+1
#define	VHOST_BHV_DC	VHOST_BHV_PP+2

#define BHV_TO_VHOST(bdp)	((struct vhost *)BHV_VOBJ(bdp))
#define	VHOST_BHV_FIRST(v)	((bhv_desc_t *)BHV_HEAD_FIRST(&(v)->vh_bhvh))
#define VHOST_BHV_HEAD_PTR(v)	(&(v)->vh_bhvh)

extern void	vhost_cell_init(void);
extern vhost_t	*vhost_create(void);

#ifdef	DEBUG
extern struct ktrace *vhost_trace;
extern cell_t	vhost_trace_id;
extern void	idbg_vhost_bhv_print(vhost_t *);

#define	HOST_TRACE2(a,b) { \
	if (vhost_trace_id == (b) || vhost_trace_id == -1) { \
		KTRACE4(vhost_trace, \
			(a), (void *)(__psint_t)(b), \
			"pid", (void *)(__psint_t)current_pid()); \
	} \
}
#define	HOST_TRACE4(a,b,c,d) { \
	if (vhost_trace_id == (b) || vhost_trace_id == -1) { \
		KTRACE6(vhost_trace, \
			(a), (void *)(__psint_t)(b), \
			"pid", (void *)(__psint_t)current_pid(), \
			(c), (void *)(__psint_t)(d)); \
	} \
}
#define	HOST_TRACE6(a,b,c,d,e,f) { \
	if (vhost_trace_id == (b) || vhost_trace_id == -1) { \
		 KTRACE8(vhost_trace, \
			(a), (void *)(__psint_t)(b), \
			"pid", (void *)(__psint_t)current_pid(), \
			(c), (void *)(__psint_t)(d), \
			(e), (void *)(__psint_t)(f)); \
	} \
}
#define	HOST_TRACE8(a,b,c,d,e,f,g,h) { \
	if (vhost_trace_id == (b) || vhost_trace_id == -1) { \
		KTRACE10(vhost_trace, \
			(a), (void *)(__psint_t)(b), \
			"pid", (void *)(__psint_t)current_pid(), \
			(c), (void *)(__psint_t)(d), \
			(e), (void *)(__psint_t)(f), \
			(g), (void *)(__psint_t)(h)); \
	} \
}
#define	HOST_TRACE10(a,b,c,d,e,f,g,h,i,j) { \
	if (vhost_trace_id == (b) || vhost_trace_id == -1) { \
		KTRACE12(vhost_trace, \
			(a), (void *)(__psint_t)(b), \
			"pid", (void *)(__psint_t)current_pid(), \
			(c), (void *)(__psint_t)(d), \
			(e), (void *)(__psint_t)(f), \
			(g), (void *)(__psint_t)(h), \
			(i), (void *)(__psint_t)(j)); \
	} \
}
#else
#define	HOST_TRACE2(a,b)
#define	HOST_TRACE4(a,b,c,d)
#define	HOST_TRACE6(a,b,c,d,e,f)
#define	HOST_TRACE8(a,b,c,d,e,f,g,h)
#define	HOST_TRACE10(a,b,c,d,e,f,g,h,i,j)
#endif

#endif	/* _VHOST_PRIVATE_H_ */
