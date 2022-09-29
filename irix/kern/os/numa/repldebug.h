#ifndef	_NUMA_REPLDEBUG_H
#define	_NUMA_REPLDEBUG_H

/******************************************************************************
 * repldebug.h
 *
 *	Brief description of purpose.
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
 ******************************************************************************/

#include <sys/types.h>
#include "replattr.h"

#ident "$Revision: 1.4 $"

#define VROPS_MAX_SLOG     	512
#define SHOOT_LOG       	1


#ifdef  DEBUG
extern void repllist_shoot_log(vnode_t *, pgno_t, pgno_t ,long );

extern ktrace_t *replshoot_trace;
extern ktrace_t *pagerepl_trace;

#define VROPS_SHOOT_LOG(v,f,l,o)        repllist_shoot_log(v,f,l,o)

/* This interface is here primarily for debugging purpose.
 * a procedural interface is used to allow logging of events.
 * This would go away after some more debugging.
 */
struct pfdat;
extern void pageuseinc_nolock(struct pfdat *);

#else
#define pageuseinc_nolock(pfd)  pfdat_inc_usecnt(pfd)

#define VROPS_SHOOT_LOG(v,f,l,o)
#define VROPS_SCAN_REPLICA(v,f,l)
#define VROPS_REPL_LOG(v, s, e, t, p, ra)

#endif  /* DEBUG */

#ifdef DEBUG
extern void idbg_replfuncs(void);
extern int idbg_replprint(void *);
#endif /* DEBUG */

#endif /* _NUMA_REPLDEBUG_H */
