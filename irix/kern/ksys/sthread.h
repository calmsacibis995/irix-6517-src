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
#ident	"$Revision: 1.20 $"

#ifndef	__STHREAD_H__
#define	__STHREAD_H__

#include <sys/types.h>
#include <sys/kthread.h>
#include <sys/resource.h>

/*
 * Service Threads
 * These can run most low level kernel code:
 * - device drivers
 * - file system code
 * - networking
 *
 * Locking:
 *	st_lock:
 *		protects st_state
 *		used with st_timewait
 *
 * They are always statically bound to a kthread
 *
 * sthreads support being scheduled on a processor set. To fix to a cpu
 * simply use usePset(private.p_cpset).
 */
#define KT_TO_ST(kt)	(ASSERT(KT_ISSTHREAD(kt)), (sthread_t *)(kt))
#define ST_TO_KT(st)	((kthread_t *)(st))

#define ST_NAMELEN	16

typedef struct sthread_s {
	struct kthread	st_ev;		/* execution vehicle */
	char st_name[ST_NAMELEN];	/* name */
	struct cred	*st_cred;	/* running credential */
	struct sthread_s *st_next;	/* list of sthreads */
	struct sthread_s *st_prev;	/* list of sthreads */
} sthread_t;

/* values for sthread_create flags argument */
	/* None currently defined */

/*
 * interface functions
 */
typedef void (st_func_t)(void *);
extern void sthread_exit(void);
extern int sthread_create(char *, caddr_t, uint_t, uint_t, uint_t, uint_t,
			  st_func_t, void *, void *, void *, void *);

/* context switch routines */
extern void stresume(sthread_t *, kthread_t *);
extern void stdestroy(sthread_t *);
extern void sthread_launch(void);

#endif /* __STHREAD_H__ */
