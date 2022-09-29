#ifndef _SYSTEM_H_
#define _SYSTEM_H_

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

/*
 * System dependent types and definitions.
 */

#include <task.h>
#include <sys/types.h>
#include <sys/prctl.h>

struct vp;
struct pt;

/* PRDA access
 */
/* VP is the location of the vp state for the caller.
 * It is set at creation time and never changes.
 */
#define VP		((struct vp *)PRDALIB->pthread_data[0])
#define SET_VP(vp)	PRDALIB->pthread_data[0] = (void *)(vp)

/* PT is the location of the pthread state for the caller.
 * It is set on context switch but is always valid; VP->vp_pt
 * is _only_ valid if the host vp cannot be preempted.
 */
#define PT		((struct pt *)PRDALIB->pthread_data[1])
#define SET_PT(pt)	PRDALIB->pthread_data[1] = (void *)(pt), \
			PRDA->t_sys.t_nid = ((pt)->pt_nid)
#define CLR_PT(pt)	PRDALIB->pthread_data[1] = 0

#define	PRDA_PID	(PRDA->t_sys.t_pid)
#define	VP_PID		(PRDA->t_sys.t_rpid)
#define VP_RESCHED	(PRDA->t_sys.t_resched)
#define VP_SIGMASK	(PRDA->t_sys.t_hold)

#endif /* !_SYSTEM_H_ */
