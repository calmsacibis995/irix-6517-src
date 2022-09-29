/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1994-1995 Silicon Graphics, Inc.           *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ident "$Revision: 1.1 $"


#ifndef _KLSTAT_H
#define _KLSTAT_H


/*---------------------------------------------------
 *                   lstat.h
 *-------------------------------------------------*/


/*
 * lstat_update(lockp, ra, action, return_val
 */
void	*lstat_update(void*, void*, int, void*);
void	*lstat_update_time(void*, void*, int, void*, uint32_t);



/*
 * Values for the "action" parameter passed to lstat_update.
 */
#define LSTAT_ACT_NO_WAIT		0	/* acquired lock without spin/sleep */
#define LSTAT_ACT_SPIN			1	/* had to spin for the lock */
#define LSTAT_ACT_SLEPT			2	/* had to sleep for the lock */
#define LSTAT_ACT_REJECT		3	/* "try" lock that failed */
#define LSTAT_ACT_MAX_VALUES		4	/* Number of action values */

/*
 * Special values for the low 2 bits of an RA passed to
 * lstat_update.
 */
#define LSTAT_RA_MRSPIN			1
#define LSTAT_RA_MR			2
#define LSTAT_RA_SEMA			3

#define LSTAT_RA(n)			((void*) (((__psunsigned_t)__return_address) | n))

/*
 * Macros for recording statistics.
 */
#if defined(SEMASTAT)
#define LSTAT_UPDATE(lock,action)		lstat_update(lock,LSTAT_RA(0),action,0)
#define LSTAT_UPDATE_TIME(lock,action,t)	lstat_update_time(lock,LSTAT_RA(0),action,0,t)
#define LSTAT_SEMA_UPDATE(lock,action)		lstat_update(lock,LSTAT_RA(LSTAT_RA_SEMA),action,0)
#define LSTAT_MR_UPDATE(lock,action)		lstat_update(lock,LSTAT_RA(LSTAT_RA_MR),action,0)
#define LSTAT_MR_UPDATE_TIME(lock,action,t)	lstat_update_time(lock,LSTAT_RA(LSTAT_RA_MR),action,0,t)
#define LSTAT_MR_COND_UPDATE_TIME(lock,t) 	lstat_update_time(lock,LSTAT_RA(LSTAT_RA_MR),	\
							t?LSTAT_ACT_SLEPT:LSTAT_ACT_NO_WAIT,0,t)
#else
#define LSTAT_UPDATE(lock,action)
#define LSTAT_UPDATE_TIME(lock,action,t)
#define LSTAT_SEMA_UPDATE(lock,action)
#define LSTAT_MR_UPDATE(lock,action)
#define LSTAT_MR_UPDATE_TIME(lock,action,t)
#define LSTAT_MR_COND_UPDATE_TIME(lock,t)
#endif


/*
 * Constants used for lock addresses in the lstat_directory
 * to indicate special values of the lock address. 
 */
#define	LSTAT_MULTI_LOCK_ADDRESS	NULL	/* Same code address sets different locks */


/*
 * Maximum size of the lockstats tables. Increase this value 
 * if its not big enough. (Nothing bad happens if its not
 * big enough although some locks will not be monitored.
 */
#define LSTAT_MAX_STAT_INDEX		2000


/* 
 * Size and mask for the hash table into the directory.
 */
#define LSTAT_HASH_TABLE_SIZE		4096			/* must be 2**N */
#define LSTAT_HASH_TABLE_MASK		(LSTAT_HASH_TABLE_SIZE-1)

#define DIRHASH(ra)			(((((__psunsigned_t)(ra)) ) & LSTAT_HASH_TABLE_MASK)+1)


/*
 *	This defines an entry in the lockstat directory. It contains
 *	information about a lock being monitored.
 *	A directory entry only contains the lock identification - 
 *	counts on usage of the lock are kept elsewhere in a per-cpu
 *	data structure to minimize cache line pinging.
 */
typedef struct {
	void	*caller_ra;		/* RA of function that set lock */
	void	*lock_ptr;		/* lock address */
	ushort	next_stat_index;	/* Used to link multiple locks that have the same hash table value */
} lstat_directory_entry_t;



/*
 *	A multi-dimensioned array used to contain counts for lock accesses.
 *	The array is 3-dimensional:
 *		- CPU number. Keep from thrashing cache lines between CPUs
 *		- Directory entry index. Identifies the lock
 *		- Action. Indicates what kind of contention occurred on an access
 *		  to the lock.
 *
 *	The index of an entry in the directory is the same as the 2nd index
 *	of the entry in the counts array.
 */
typedef struct {
	uint32_t	count[LSTAT_ACT_MAX_VALUES];
	uint32_t	ticks;
} lstat_lock_counts_t;

typedef lstat_lock_counts_t	lstat_cpu_counts_t[LSTAT_MAX_STAT_INDEX];
typedef lstat_cpu_counts_t	lstat_counts_t[];

#ifdef _KERNEL
/*
 * Main control structure for lockstat. Used to turn statistics on/off
 * and to maintain directory info.
 */
typedef struct {
	mutex_t			control_lock;		/* used to serialize turning statistics on/off */
	int			state;
	int			next_free_dir_index;	/* next free entry in the directory */
	int			enabled_lbolt;		/* lbolt when collection was enabled */
	lstat_directory_entry_t	*dir;			/* directory */
	ushort			*hashtab;		/* hash table for quick dir scans */
	lstat_cpu_counts_t	*counts[MAXCPUS];	/* Array of pointers to per-cpu stats */
	int			directory_lock;		/* used to serialize adding new entries to the directory */
} lstat_control_t;

/* Make sure that the lock is in its own cache line */
#pragma set field attribute lstat_control_t directory_lock align=128

int lstat_user_command(int, void *);

#endif /* _KERNEL */





/*
 * User request to:
 *	- turn statistic collection on/off
 *		sysmp (MP_KLSTAT, LSTAT_OFF, NULL)
 *		sysmp (MP_KLSTAT, LSTAT_ON, NULL)
 *	- read statistics
 *		sysmp (MP_KLSTAT, LSTAT_READ, dirp, countp)
 */
#define LSTAT_OFF	0
#define LSTAT_ON	1
#define LSTAT_READ	2
#define LSTAT_STAT	3

typedef struct {
	int			lstat_is_enabled;	/* the current state is returned */
	int			maxcpus;		/* Number of cpus present */
	int			next_free_dir_index;	/* index of the next free directory entry */
	time_t			current_time;		/* current time in secs since 1969 */
	uint			cycleval;		/* value of timer tick in picosec */
	int			enabled_lbolt;		/* lbolt when collection was enabled */
	int			current_lbolt;		/* current value of lbolt */
	void			*kernel_magic_addr;	/* address of kernel_magic */
	void			*kernel_end_addr;	/* contents of kernel magic (points to "end") */
	lstat_cpu_counts_t	*cpu_counts_ptr;	/* pointer to the user buffer for returning counts */
	lstat_directory_entry_t	*directory_ptr;		/* pointer to the user buffer for returning dir info */
} lstat_user_request_t;

#endif /* _KLSTAT_H */

