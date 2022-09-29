#ifndef _NANOTHREAD_H
#define _NANOTHREAD_H

#define _KMEMUSER
#include <sys/kusharena.h>

/*	Ids of virtual processors (VPs)	*/
typedef	int	vpid_t;

/* 	Thread entry points are of this type.  */
typedef void ((*nanostartf_t)(void *));

/*
 * 	set_num_processors(nr):
 *		Set number of requested processors to nr.
 * 		Allocate shared arena if needed.
 */
int	set_num_processors(int);

/*
 *	start_threads(sf,arg): 	Start 'nrequested' VPs with threads starting
 * 		at entry point sf; arg is argument to be passed to sf.
 */
int	start_threads(nanostartf_t, void *);

/*
 * 	resume_vp(rvpid): Give-up processor of the calling VP to
 * 		resume execution the VP numbered 'rvpid'
 */
int	resume(vpid_t);

/*
 * 	getvpid(): Return id of calling VP.
 */
vpid_t	getvpid(void);

/*
 *	block_vp(v), unblock_vp(v): Suspend or resume execution of VP, v. 
 */
void	block_vp(vpid_t);
void	unblock_vp(vpid_t);

/*
 * 	kusp: Pointer to kernel-user shared arena, accessible to all VPs.
 *		Setup by set_num_processors().
 */
extern	kusharena_t	*kusp;

#endif /* _NANOTHREAD_H */
