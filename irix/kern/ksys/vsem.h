/**************************************************************************
 *									  *
 * 		 Copyright (C) 1994-1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ifndef	_KSYS_VSEM_H_
#define	_KSYS_VSEM_H_	1
#ident "$Id: vsem.h,v 1.2 1997/08/01 21:01:11 sp Exp $"

#include <ksys/behavior.h>
#include <ksys/kqueue.h>

struct cred;

/*
 * A virtual semaphore object. This is the object handle to be passed
 * to all VSEM_* calls.
 */
typedef struct vsem {
	kqueue_t	vsm_queue;	/* Hash queue link */
	int		vsm_refcnt;	/* Local access count */
	int		vsm_key;	/* Key used to create the semaphore */
	int		vsm_id;		/* The id of the semaphore */
	int		vsm_nsems;	/* the number of semaphores */
	bhv_head_t	vsm_bh;		/* behavior chain head */
} vsem_t;

struct ipc_perm;
struct cred;
struct semid_ds;
typedef struct semops {
	bhv_position_t	sem_position;	/* position within behavior chain */
	int	(*vsem_keycheck)(
			bhv_desc_t *, struct cred *, int, int);
	int	(*vsem_rmid)(
			bhv_desc_t *, struct cred *);
	int	(*vsem_ipcset)(
			bhv_desc_t *, struct ipc_perm *, struct cred *);
	int	(*vsem_getstat)(
			bhv_desc_t *, struct cred *, struct semid_ds *, cell_t *);
	int	(*vsem_getncnt)(
			bhv_desc_t *, struct cred *, int, int *);
	int	(*vsem_getpid)(
			bhv_desc_t *, struct cred *, int, pid_t *);
	int	(*vsem_getval)(
			bhv_desc_t *, struct cred *, int, ushort_t *);
	int	(*vsem_getzcnt)(
			bhv_desc_t *, struct cred *, int, int *);
	int	(*vsem_getall)(
			bhv_desc_t *, struct cred *, ushort *);
	int	(*vsem_setval)(
			bhv_desc_t *, struct cred *, int, ushort_t);
	int	(*vsem_setall)(
			bhv_desc_t *, struct cred *, ushort_t *);
	int	(*vsem_op)(
			bhv_desc_t *, struct cred *, struct sembuf *, int);
	void	(*vsem_exit)(
			bhv_desc_t *, pid_t);
	int	(*vsem_mac_access)(
			bhv_desc_t *, struct cred *);
	void	(*vsem_destroy)(
			bhv_desc_t *);
} semops_t ;

struct semstat;
extern	int 	vsem_create(int, int, int, struct cred *, vsem_t **);
extern	int 	vsem_lookup_key(int, int, int, struct cred *, vsem_t **);
extern	int	vsem_lookup_id(int, vsem_t **);
extern	void	vsem_rele(vsem_t *);
extern	void	vsem_freeid(int);

#define vsm_fbhv	vsm_bh.bh_first		/* first behavior */
#define vsm_fops	vsm_bh.bh_first->bd_ops /* ops for first behavior */

static __inline int
VSEM_KEYCHECK(
	vsem_t		*vsem,
	struct cred	*cred,
	int		flags,
	int		nsems)
{
	int	rv;

	BHV_READ_LOCK(&vsem->vsm_bh);
	rv = (*((semops_t *)vsem->vsm_fops)->vsem_keycheck)(vsem->vsm_fbhv,
		cred, flags, nsems);
	BHV_READ_UNLOCK(&vsem->vsm_bh);
	return(rv);
}

static __inline int
VSEM_RMID(
	vsem_t		*vsem,
	struct cred	*cred)
{
	int	rv;

	BHV_READ_LOCK(&vsem->vsm_bh);
	rv = (*((semops_t *)vsem->vsm_fops)->vsem_rmid)(vsem->vsm_fbhv,
		cred);
	BHV_READ_UNLOCK(&vsem->vsm_bh);
	return(rv);
}

static __inline int
VSEM_IPCSET(
	vsem_t		*vsem,
	struct ipc_perm *perm,
	struct cred	*cred)
{
	int	rv;

	BHV_READ_LOCK(&vsem->vsm_bh);
	rv = (*((semops_t *)vsem->vsm_fops)->vsem_ipcset)(vsem->vsm_fbhv,
		perm, cred);
	BHV_READ_UNLOCK(&vsem->vsm_bh);
	return(rv);
}

static __inline int
VSEM_GETSTAT(
	vsem_t		*vsem,
	struct cred	*cred,
	struct semid_ds	*sp,
	cell_t		*cellp)
{
	int	rv;

	BHV_READ_LOCK(&vsem->vsm_bh);
	rv = (*((semops_t *)vsem->vsm_fops)->vsem_getstat)(vsem->vsm_fbhv,
		cred, sp, cellp);
	BHV_READ_UNLOCK(&vsem->vsm_bh);
	return(rv);
}

static __inline int
VSEM_GETNCNT(
        vsem_t          *vsem,
	struct cred	*cred,
	int		semnum,
	int		*cnt)
{
	int	rv;

	BHV_READ_LOCK(&vsem->vsm_bh);
	rv = (*((semops_t *)vsem->vsm_fops)->vsem_getncnt)(vsem->vsm_fbhv,
		cred, semnum, cnt);
	BHV_READ_UNLOCK(&vsem->vsm_bh);
	return(rv);
}

static __inline int
VSEM_GETPID(
	vsem_t		*vsem,
	struct cred	*cred,
	int		semnum,
	pid_t		*pid)
{
	int	rv;

	BHV_READ_LOCK(&vsem->vsm_bh);
	rv = (*((semops_t *)vsem->vsm_fops)->vsem_getpid)(vsem->vsm_fbhv,
		cred, semnum, pid);
	BHV_READ_UNLOCK(&vsem->vsm_bh);
	return(rv);
}

static __inline int
VSEM_GETVAL(
	vsem_t		*vsem,
	struct cred	*cred,
	int		semnum,
	ushort_t	*semval)
{
	int	rv;

	BHV_READ_LOCK(&vsem->vsm_bh);
	rv = (*((semops_t *)vsem->vsm_fops)->vsem_getval)(vsem->vsm_fbhv,
		cred, semnum, semval);
	BHV_READ_UNLOCK(&vsem->vsm_bh);
	return(rv);
}

static __inline int
VSEM_GETZCNT(
	vsem_t		*vsem,
	struct cred	*cred,
	int		semnum,
	int		*zcnt)
{
	int	rv;

	BHV_READ_LOCK(&vsem->vsm_bh);
	rv = (*((semops_t *)vsem->vsm_fops)->vsem_getzcnt)(vsem->vsm_fbhv,
		cred, semnum, zcnt);
	BHV_READ_UNLOCK(&vsem->vsm_bh);
	return(rv);
}

static __inline int
VSEM_GETALL(
	vsem_t		*vsem,
	struct cred	*cred,
	ushort		*semvals)
{
	int	rv;

	BHV_READ_LOCK(&vsem->vsm_bh);
	rv = (*((semops_t *)vsem->vsm_fops)->vsem_getall)(vsem->vsm_fbhv,
		cred, semvals);
	BHV_READ_UNLOCK(&vsem->vsm_bh);
	return(rv);
}

static __inline int
VSEM_SETVAL(
	vsem_t		*vsem,
	struct cred	*cred,
	int		semnum,
	ushort_t	semval)
{
	int	rv;

	BHV_READ_LOCK(&vsem->vsm_bh);
	rv = (*((semops_t *)vsem->vsm_fops)->vsem_setval)(vsem->vsm_fbhv,
		cred, semnum, semval);
	BHV_READ_UNLOCK(&vsem->vsm_bh);
	return(rv);
}

static __inline int
VSEM_SETALL(
	vsem_t		*vsem,
	struct cred	*cred,
	ushort		*semvals)
{
	int	rv;

	BHV_READ_LOCK(&vsem->vsm_bh);
	rv = (*((semops_t *)vsem->vsm_fops)->vsem_setall)(vsem->vsm_fbhv,
		cred, semvals);
	BHV_READ_UNLOCK(&vsem->vsm_bh);
	return(rv);
}

static __inline int
VSEM_OP(
	vsem_t		*vsem,
	struct cred	*cred,
	struct sembuf	*sembuf,
	int		nops)
{
	int	rv;

	BHV_READ_LOCK(&vsem->vsm_bh);
	rv = (*((semops_t *)vsem->vsm_fops)->vsem_op)(vsem->vsm_fbhv,
		cred, sembuf, nops);
	BHV_READ_UNLOCK(&vsem->vsm_bh);
	return(rv);
}

static __inline void
VSEM_EXIT(
	vsem_t		*vsem,
	pid_t		pid)
{
	BHV_READ_LOCK(&vsem->vsm_bh);
	(*((semops_t *)vsem->vsm_fops)->vsem_exit)(vsem->vsm_fbhv, pid);
	BHV_READ_UNLOCK(&vsem->vsm_bh);
}

static __inline int
VSEM_MAC_ACCESS(
	vsem_t		*vsem,
	struct cred	*cred)
{
	int	rv;

	BHV_READ_LOCK(&vsem->vsm_bh);
	rv = (*((semops_t *)vsem->vsm_fops)->vsem_mac_access)(vsem->vsm_fbhv,
		cred);
	BHV_READ_UNLOCK(&vsem->vsm_bh);
	return(rv);
}

static __inline void
VSEM_DESTROY(
	vsem_t	*vsem)
{
	(*((semops_t *)vsem->vsm_fops)->vsem_destroy)(vsem->vsm_fbhv);
}

#endif	/* _KSYS_VSEM_H_ */
