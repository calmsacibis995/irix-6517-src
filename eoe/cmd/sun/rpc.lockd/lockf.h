/*	@(#)lockf.h	1.4 91/06/25 4.1NFSSRC SMI	 */

#ifndef _lockf_h
#define	_lockf_h

/* constants and structures for the locking code ... */

struct data_lock {
	struct data_lock *Next;		/* Next lock in the list	   */
	struct data_lock *Prev;		/* Previous Lock in the list	   */
	struct process_locks *MyProc;	/* Points to process lock list	   */
	struct reclock	*req;		/* A back pointer to the reclock   */
	long		base;		/* Region description.		   */
	long		length;		/* Length of lock		   */
	int		type;		/* EXCLUSIVE or SHARED		   */
	int		LockID;		/* ID of this lock		   */
	int		pid;		/* "Owner" of lock.		   */
	long		ipaddr;		/* System 'pid' is on.             */
	sysid_t		sysid;		/* Unique # based on pid and ipaddr */
	int		magic;
};

/* process lock structure holds locks owned by a given process */
struct process_locks {
	int			pid;
	struct process_locks	*next;
	struct data_lock	*lox;
};


/* Reference 'types' also BLOCKING below is used */
#define	FREE	0
#define	BLOCKED 1

#define	EXCLUSIVE	1	/* Lock is an Exclusive lock		*/
#define	BLOCKING	2	/* Block process for this lock. 	*/


/* These defines are used with lock structures to determine various things */
#define	LOCK_TO_EOF	-1
#define	END(l)	(((l)->length == LOCK_TO_EOF || ((l)->length == 0)) ? \
	LOCK_TO_EOF : (l)->base + (l)->length - 1)

/*
 * Determine if locks intersect.  Lock intersection is computed
 * by determining:
 *
 *	If			Then they intersect if
 *	--			----------------------
 * 	Both not to-EOF locks	bases and ends are mutually out of range
 *	One only is EOF lock	If the EOF-lock base is within end of
 *				non-EOF lock
 *	Both are EOF locks	Always
 */
#define	TOUCHING(a, b) \
	((END(a) >= 0 && END(b) >= 0 && \
	(!((END(a) < (b)->base) || ((a)->base > END(b))))) || \
	(END(a) < 0 && END(b) >= 0 && END(b) >= (a)->base) || \
	(END(a) >= 0 && END(b) < 0 && END(a) >= (b)->base) || \
	(END(a) < 0 && END(b) < 0))

/* Is TRUE if A and B are adjacent */
#define	ADJACENT(a, b) ((END(a)+1 == (b)->base) || ((a)->base == END(b)+1))

/* Is TRUE if a is completely contained within b */
#define	WITHIN(a, b) \
	(((a)->base >= (b)->base) && \
	((END(a) >= 0 && END(b) >= 0 && END(a) <= END(b)) || (END(b) < 0) || \
	((b)->length == 0x7fffffff)))

/* Is TRUE if a and b are owned by the same process... (even remotely) */
#define	SAMEOWNER(a, b)  ((a)->pid == (b)->pid)

#endif /*!_lockf_h*/
