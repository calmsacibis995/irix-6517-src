/**************************************************************************
 *									  *
 * 		 Copyright (C) 1990-1995, Silicon Graphics, Inc.	  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ifndef __SYS_SPLOCK_H__
#define __SYS_SPLOCK_H__

#ident "$Revision: 3.30 $"

#if !_LANGUAGE_ASSEMBLY

#include <sys/avl.h>
#include <sys/sema_private.h>

typedef struct k_splockmeter {
	avlnode_t m_avlnode;		/* tree linkage -- must be first */
	char	m_inuse;		/* use count */
	char	m_type;			/* type of spinlock */
	short	m_owner;		/* current owner of lock */
	uint	m_ts;			/* timestamp when got lock */
	void	*m_addr;		/* lock address */
	ulong 	m_wait;			/* count of times locker had to wait */
	ulong 	m_lcnt;			/* count of 'lock' operations */
	ulong 	m_ucnt;			/* count of 'unlock' operations */
	inst_t	*m_lastlock;		/* return pc of last lock */
	inst_t	*m_lastunlock;		/* return pc of last unlock */
	uint	m_elapsed;		/* longest time held lock */
	short	m_elapsedcpu;		/* cpu holding lock */
	uint64_t	m_totalheld;		/* total time held lock */
	short	m_waitcpu;		/* cpu waiting for lock */
	inst_t	*m_elapsedpc;		/* and the culprit */
	inst_t	*m_elapsedpcfree;	/* freeing pc of longest time held */
	void	*m_splevel;		/* spl function used to set spl */
	inst_t	*m_waitpc;		/* and the victim */
	uint	m_waittime;		/* longest time wait for lock */
	uint64_t	m_totalwait;		/* total time waiting for lock */
	int	m_racers;		/* number of cpus waiting for lock */
	int	m_racers_num;		/* times lock has been waited for */
	int	m_racers_total;		/* total cpus that have waited */
	int	m_racers_max;		/* max cpus waiting simultaneously */
	int	m_gotfirst;		/* #times got lock on first try... */
	int	m_gotfirst_num;		/* ...out of this many calls */
	char	m_name[METER_NAMSZ];	/* text name for lock */
} k_splockmeter_t;

#define next_lockmeter(m)	((k_splockmeter_t *)(m)->m_avlnode.avl_nextino)

extern struct k_splockmeter *lockmeter_find(void *);
extern struct k_splockmeter *lockmeter_chain;

extern int splockmeter;			/* if non-zero, do spin lock metering */

#endif /* !_LANGUAGE_ASSEMBLY */

#define SPIN_LOCK	0x1
#define SPIN_INIT	0x2

#define SPINTYPE_SPIN	1
#define SPINTYPE_BIT	2
#define SPINTYPE_64BIT	3

#define	OWNER_NONE	-1

#endif /* !__SYS_SPLOCK_H__ */
