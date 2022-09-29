/**************************************************************************
 *									  *
 * 		 Copyright (C) 1986-1995 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ifndef __SYS_MON_H__
#define __SYS_MON_H__

/*
 * Monitor definitions
 */
struct monitor;
struct mon_state;

typedef struct mon_func {
	void		(*mf_init)(struct monitor *);
	void		(*mf_service)(void *);
	void		(*mf_p_mon)(struct monitor *);
	void		(*mf_v_mon)(struct monitor *);
	void		(*mf_q_mon_sav)(struct monitor *, void **);
	void		(*mf_q_mon_rst)(struct monitor *, void *);
	void		(*mf_r_mon)(struct monitor *, struct mon_state *);
	void		(*mf_a_mon)(struct monitor *, struct mon_state *);
} mon_func_t;

#define MON_LOCKED	0x01	/* monitor in use */
#define MON_WAITING	0x02	/* someone waiting for monitor */
#define MON_TIMEOUT	0x04	/* timeout to run mon_runq pending */
#define MON_DOSRV	0x08	/* monitor needs monq serviced */
#define MON_RUN		0x10	/* monitor service thread running */

typedef struct monitor {
	struct monitor	*mon_next;
	struct monitor	*mon_prev;
	lock_t		mon_lock;		/* private (local) lock */
	uchar_t		mon_lock_flags;
	sv_t		mon_wait;		/* wait here to lock */
	struct monitor	**mon_monpp;
	lock_t		*mon_monp_lock;
	uint64_t	mon_id;			/* current owner's threadid */
	int		mon_trips;		/* number of trips */
	void		*mon_p_arg;		/* arg to last pmonitor */
	void		**mon_queue;		/* queue of entries */
	mon_func_t	*mon_funcp;		/* private functions */
	void		*mon_private;		/* private data */
	sv_t		mon_sv;			/* wakeup for service_monq */
} mon_t;

typedef struct mon_state {
	mon_t		*ms_mon;
	mon_t		**ms_monpp;		/* monitor */
	lock_t		*ms_monp_lock;
	uint64_t	ms_id;			/* owner threadid */
	int		ms_trips;		/* # of trips */
	void		*ms_p_arg;		/* arg to pass to pmonitor
						   when re-acquired */
} mon_state_t;

/*
 * NOTE - mon_queue points to anything - assumes that first entry
 * is a list pointer.
 */

#ifdef _KERNEL
extern void	initmonitor(mon_t *, char *, mon_func_t *);
extern int	pmonitor(mon_t *, int, void *);
extern int	qmonitor(mon_t *, void *, void *);
extern void	vmonitor(mon_t *);
extern int	amonitor(mon_state_t *);
extern void	rmonitor(mon_t *, mon_state_t *);
extern mon_t	*allocmonitor(char *, mon_func_t *);
extern void	freemonitor(mon_t *);
#endif /* _KERNEL */

#if _KERNEL && !_STANDALONE
extern int	spinunlock_pmonitor(lock_t *, int, mon_t **, int, void *);
extern int	spinunlock_qmonitor(lock_t *, int, mon_t **, void *, void *, int);
#endif /* _KERNEL */
#endif /* __SYS_MON_H__ */
