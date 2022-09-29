#ifndef _DELAY_H_
#define _DELAY_H_

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

/* Global names
 */
#define timeout_bootstrap	PFX_NAME(timeout_bootstrap)
#define timeout_needed		PFX_NAME(timeout_needed)
#define timeout_enqueue		PFX_NAME(timeout_enqueue)
#define timeout_cancel		PFX_NAME(timeout_cancel)
#define timeout_evaluate	PFX_NAME(timeout_evaluate)


#include "q.h"
#include <sys/time.h>
#include <sys/timers.h>

struct pt;

#define	D_PENDING	0
#define	D_EXEC		1
#define	D_DEAD		2

typedef struct timeout {
	q_t		to_queue;
	timespec_t	to_abstime;	/* when request ripens */
	struct pt	*to_pt;		/* pt to which request is bound */
	void 		(*to_func)(struct pt *);	/* timeout function */
	unsigned int	to_state;	/* state of timeout struct */
} timeout_t;

extern void		timeout_bootstrap(void);
extern int		timeout_needed(timespec_t *, const timespec_t *);
extern void		timeout_enqueue(timeout_t *, const timespec_t *,
				const timespec_t *, void (*)(struct pt *));
extern void		timeout_cancel(timeout_t *);
extern timespec_t	*timeout_evaluate(timespec_t *);

#endif /* !_DELAY_H_ */
