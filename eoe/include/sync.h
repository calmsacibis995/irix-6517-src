/**************************************************************************
 *									  *
 * 		 Copyright (C) 1997 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ifndef __SYNC_H__
#define __SYNC_H__

#ifndef _SPIN_SGI_INTERNAL
typedef struct spinlock_s {
	__uint64_t	 spin_reserved[6];
} spinlock_t;
#endif

typedef struct spinlock_trace_s {
	unsigned int	st_spins;	/* # times lock spun out  */
	unsigned int	st_tries;	/* # times lock requested */
	unsigned int	st_hits;	/* # times lock acquired  */
	unsigned int	st_unlocks;	/* # times lock released  */  
	int		st_opid;	/* Owner process ID	  */
	int		st_otid;	/* Owner thread ID	  */
	unsigned int	st_reserved[10];
} spinlock_trace_t;

#define _SPINDEFSPINC	600
#define _SPINDEFSLEEPC	10

/*
 * spin_mode commands
 */
#define SPIN_MODE_TRACEINIT	100
#define SPIN_MODE_TRACEON	101
#define SPIN_MODE_TRACEPLUSON	102
#define SPIN_MODE_TRACEOFF	103
#define SPIN_MODE_TRACERESET	104
#define SPIN_MODE_QUEUEFIFO	105
#define SPIN_MODE_QUEUEPRIO	106
#define SPIN_MODE_UP		243

#undef spin_lock
#define spin_lock posix_spin_lock

extern int spin_init(spinlock_t *);
extern int spin_destroy(spinlock_t *);
extern int spin_lock(spinlock_t *);
extern int spin_lockw(spinlock_t *, unsigned int);
extern int spin_lockc(spinlock_t *, unsigned int);
extern int spin_trylock(spinlock_t *);
extern int spin_unlock(spinlock_t *);
extern int spin_print(spinlock_t *, FILE *, const char *);
extern int spin_mode(spinlock_t *, int, ...);

#endif /* __SYNC_H__ */




