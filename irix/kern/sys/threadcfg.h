 /**************************************************************************
 *									  *
 *		Copyright ( C ) 1996, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 1.1 $"
#ifndef THREADCFG_H
#define	THREADCFG_H

/*
 * this data structure is used to do the initial thread
 * configuration, from the threadcfg master file.
 */

#define THREADCFG_REV	1

/* add to the end */
#define	THREADCFG_PRIO		0
#define	THREADCFG_STACKSIZE	1
#define	THREADCFG_MUSTRUN	2

typedef
struct	{	
	char	*name;		/* name of the thread	*/
	int	pri;		/* priority		*/
	int	stack;		/* size of its stack	*/
	int	mustrun;	/* mustrun on CPU	*/
} thread_cfg_t;

extern	int thread_cfg_rev;		/* the thread_cfg_t revision */
extern	thread_cfg_t thread_init[];	/* the list of thread config values */
#endif /* THREADCFG_H */
