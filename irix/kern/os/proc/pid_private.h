/**************************************************************************
 *									  *
 * 		 Copyright (C) 1994-1995 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ifndef	_OS_PROC_PID_PRIVATE_H_
#define	_OS_PROC_PID_PRIVATE_H_	1

#include <sys/types.h>
#include <ksys/kqueue.h>

extern void pidtab_init(void);

struct vproc;
extern struct vproc *pid_to_vproc_locked(pid_t);

#define PID_INDEX(pid)          (((pid) - pid_base) % pidtabsz)
#define PID_SLOT(pid)		(&pidtab[PID_INDEX(pid)])

#define pidtab_lock_init()	spinlock_init(&pidlock, "pidlock")
#define pidtab_lock()		mutex_spinlock(&pidlock);
#define pidtab_unlock(spl)	mutex_spinunlock(&pidlock, spl)
#define pidtab_nested_lock()	nested_spinlock(&pidlock)
#define pidtab_nested_unlock()	nested_spinunlock(&pidlock)

typedef struct pid_slot {
	pid_t	ps_pid;
	lock_t	ps_lock;
	union {
		struct {
			int	psu_forw;	/* next index in free list */
			int	psu_back;	/* prev index in free list */
		} psu_free;
		struct {
			int	psu_active;	/* Has active proc? */
			int	psu_busycnt;	/* How many active procs */
		} psu_busy;
	} psu;
	struct pid_entry	*ps_chain;	/* busy pid link */
} pid_slot_t;

#define	ps_forw		psu.psu_free.psu_forw
#define	ps_back		psu.psu_free.psu_back
#define	ps_active	psu.psu_busy.psu_active
#define	ps_busycnt	psu.psu_busy.psu_busycnt

/*
 * Note -- ps_lock could be changed to a bitlock if flags bits are
 * ever needed in the pid_slot structure.  The pid code should all
 * be using the following macros to use the locks.
 */
#define pidslot_lock_init(ps,i)	init_spinlock(&(ps)->ps_lock, "pidslot", i)
#define pidslot_lock(ps)	mutex_spinlock(&(ps)->ps_lock)
#define pidslot_nested_lock(ps)	nested_spinlock(&(ps)->ps_lock)
#define pidslot_nested_unlock(ps)  nested_spinunlock(&(ps)->ps_lock)
#define pidslot_unlock(ps,p)	mutex_spinunlock(&(ps)->ps_lock, p)
#define pidslot_wait(ps,x)	sv_wait(&pid_dealloc_sv, PZERO, \
					&(ps)->ps_lock, x)

#define	pidslot_setactive(p)	{ (p)->ps_active = -2; (p)->ps_busycnt = 1; }
#define	pidslot_isactive(p)	((p)->ps_active == -2)

typedef struct pid_entry {
	kqueue_t	pe_queue;		/* active list */
	pid_t		pe_pid;			/* pid busy in this entry */
	union {
		struct {
			uint	peu_nodealloc : 8, /* can't dealocate */
				peu_dealloc : 1, /* want to dealocate */
				peu_pad : 19,
				peu_sess : 1,	/* used as session id? */
				peu_pgrp : 1,	/* used as pgrp id? */
				peu_batch : 1,	/* used as batch id? */
				peu_proc : 1;	/* used as proc id? */
		} peu_bit;
		struct {
			uint	peu_pad : 28,
				peu_busy : 4;
		} peu_busybit;
	} pe_ubusy;

	struct vproc		*pe_vproc;	/* vproc ptr when active */
	struct pid_entry	*pe_next;	/* busy pid link */

} pid_entry_t;

#define	pe_sess		pe_ubusy.peu_bit.peu_sess
#define	pe_pgrp		pe_ubusy.peu_bit.peu_pgrp
#define	pe_batch	pe_ubusy.peu_bit.peu_batch
#define	pe_proc		pe_ubusy.peu_bit.peu_proc
#define pe_busy		pe_ubusy.peu_busybit.peu_busy

#define pe_isactive(pe)	(pe)->pe_vproc

#define PE_PROC		0x1
#define PE_BATCH	0x2
#define PE_PGRP		0x4
#define PE_SESS		0x8

#define	pe_dealloc	pe_ubusy.peu_bit.peu_dealloc
#define	pe_nodealloc	pe_ubusy.peu_bit.peu_nodealloc

typedef struct pid_active {
	kqueue_t	pa_queue;		/* active list */
	lock_t  	pa_lock;
} pid_active_t;
#ifdef MP
#pragma set type attribute pid_active_t align=128
#endif

#define pidactive_lock()	mutex_spinlock(&pid_active_list.pa_lock)
#define pidactive_nested_lock()	nested_spinlock(&pid_active_list.pa_lock)
#define pidactive_nested_unlock() nested_spinunlock(&pid_active_list.pa_lock)
#define pidactive_unlock(X)	mutex_spinunlock(&pid_active_list.pa_lock, X)

#include	<sys/sema.h>

extern pid_t		pid_base;
extern int		pidtabsz;		/* size of pid table */
extern pid_slot_t	*pidtab;		/* pid table */
extern pid_t		pid_wrap;
extern lock_t		pidlock;
extern sv_t		pid_dealloc_sv;
extern pid_active_t	pid_active_list;

/*
 * Private procedure prototypes
 */
extern int pid_getlist_local(pidord_t start, size_t *count, 
			     pid_t *pidbuf, pidord_t *ordbuf);

#endif	/* _OS_PROC_PID_PRIVATE_H_ */
