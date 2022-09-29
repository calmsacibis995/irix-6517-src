/**************************************************************************
 *									  *
 * Copyright (C) 1986-1995 Silicon Graphics, Inc.			  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ifndef __SIGEVTOP_H__
#define __SIGEVTOP_H__

#include <signal.h>
#include <time.h>
#include <mqueue.h>

/* Each op has enough info to validate a key, start a thread and
 * identify an entry when the key is not known (e.g. for deletion).
 */
#define EVTOP_MQ	0x001
#define EVTOP_TMR	0x002

#define EVTOP_FBUSY	0x100	/* op is in use */
#define EVTOP_FFREE	0x200	/* free op when executed */

typedef union evtop_data {
	timer_t	evtop_tid;
	mqd_t	evtop_mqid;
} evtop_data_t;


extern int	__evtop_alloc(const sigevent_t *, uint_t, evtop_data_t *, int*);
extern void	__evtop_free(int);
extern int	__evtop_lookup(uint_t, evtop_data_t *, int *);
extern void	__evtop_setid(int, evtop_data_t *);


#endif
