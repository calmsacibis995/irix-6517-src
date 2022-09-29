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
#ifndef	_KSYS_VSHM_H_
#define	_KSYS_VSHM_H_	1
#ident "$Id: vshm.h,v 1.22 1998/09/14 16:14:27 jh Exp $"

#include <ksys/behavior.h>
#include <ksys/kqueue.h>
#include <ksys/as.h>

/*
 * A virtual shared memory object. This is the object handle to be passed
 * to all VSHM_* calls.
 */
typedef struct vshm {
	kqueue_t	vsm_queue;	/* Hash queue link */
	int		vsm_refcnt;	/* Local access count */
	int		vsm_key;	/* Key used to create the segment */
	int		vsm_id;		/* The id of the segment */
	bhv_head_t	vsm_bh;		/* behavior chain head */
} vshm_t;

struct ipc_perm;
struct cred;
struct shmid_ds;
typedef struct shmops {
	bhv_position_t	shm_position;	/* position within behavior chain */
	void	(*vshm_getmo)(
			bhv_desc_t *, as_mohandle_t *);
	int	(*vshm_keycheck)(
			bhv_desc_t *, struct cred *, int, size_t);
	int	(*vshm_attach)(
			bhv_desc_t *, int, asid_t, pid_t,
					struct cred *, caddr_t, caddr_t *);
	void	(*vshm_setdtime)(
			bhv_desc_t *, int, pid_t);
	int	(*vshm_rmid)(
			bhv_desc_t *, struct cred *, int);
	int	(*vshm_ipcset)(
			bhv_desc_t *, struct ipc_perm *, struct cred *);
	int	(*vshm_getstat)(
			bhv_desc_t *, struct cred *, struct shmid_ds *, cell_t *);
	int	(*vshm_mac_access)(
			bhv_desc_t *, struct cred *);
	void	(*vshm_destroy)(
			bhv_desc_t *);
} shmops_t ;

struct shmstat;
extern	int 	vshm_create(int, int, size_t, pid_t, struct cred *, vshm_t **, int);
extern	int 	vshm_lookup_key(int, int, size_t, pid_t, struct cred *, vshm_t **, int);
extern	int	vshm_lookup_id(int, vshm_t **);
extern	void	vshm_ref(vshm_t *);
extern	void	vshm_rele(vshm_t *);
extern	void	vshm_freeid(int);

/*
 * Iterate through all the shared memory segments attached to this process
 * and call function func with two arguments, the vshm of the segment
 * and "arg".
 */
extern	void	vshm_iterate(void (*func)(vshm_t *, void *), void *);

#define vsm_fbhv	vsm_bh.bh_first		/* first behavior */
#define vsm_fops	vsm_bh.bh_first->bd_ops /* ops for first behavior */

#define VSHM_GETMO(v, m) \
	{ \
	BHV_READ_LOCK(&v->vsm_bh);	\
	(*((shmops_t *)(v)->vsm_fops)->vshm_getmo)((v)->vsm_fbhv, m); \
	BHV_READ_UNLOCK(&v->vsm_bh);	\
	}

#define VSHM_KEYCHECK(v, c, f, s, rv) \
	{ \
	BHV_READ_LOCK(&v->vsm_bh);	\
	rv = (*((shmops_t *)(v)->vsm_fops)->vshm_keycheck)((v)->vsm_fbhv, c, f, s); \
	BHV_READ_UNLOCK(&v->vsm_bh);	\
	}

#define	VSHM_ATTACH(v, f, asid, pi, c, a, aa, rv) \
	{ \
	BHV_READ_LOCK(&v->vsm_bh);	\
	rv = (*((shmops_t *)(v)->vsm_fops)->vshm_attach)((v)->vsm_fbhv, f, asid, pi, c, a, aa); \
	BHV_READ_UNLOCK(&v->vsm_bh);	\
	}

#define	VSHM_SETDTIME(v, t, p) \
	{ \
	BHV_READ_LOCK(&v->vsm_bh);	\
	(*((shmops_t *)(v)->vsm_fops)->vshm_setdtime)((v)->vsm_fbhv, t, p); \
	BHV_READ_UNLOCK(&v->vsm_bh);	\
	}

#define	VSHM_RMID(v, c, f, rv) \
	{ \
	BHV_READ_LOCK(&v->vsm_bh);	\
	rv = (*((shmops_t *)(v)->vsm_fops)->vshm_rmid)((v)->vsm_fbhv, c, f); \
	BHV_READ_UNLOCK(&v->vsm_bh);	\
	}

#define	VSHM_IPCSET(v, p, c, rv) \
	{ \
	BHV_READ_LOCK(&v->vsm_bh);	\
	rv = (*((shmops_t *)(v)->vsm_fops)->vshm_ipcset)((v)->vsm_fbhv, p, c); \
	BHV_READ_UNLOCK(&v->vsm_bh);	\
	}

#define	VSHM_GETSTAT(v, cr, s, c, rv) \
	{ \
	BHV_READ_LOCK(&v->vsm_bh);	\
	rv = (*((shmops_t *)(v)->vsm_fops)->vshm_getstat)((v)->vsm_fbhv, cr, s, c); \
	BHV_READ_UNLOCK(&v->vsm_bh);	\
	}


#define VSHM_MAC_ACCESS(v,c,rv) \
	{ \
	BHV_READ_LOCK(&v->vsm_bh);	\
	rv = (*((shmops_t *)(v)->vsm_fops)->vshm_mac_access)((v)->vsm_fbhv, c); \
	BHV_READ_UNLOCK(&v->vsm_bh);	\
	}

#define VSHM_DESTROY(v) \
	{ \
	(*((shmops_t *)(v)->vsm_fops)->vshm_destroy)((v)->vsm_fbhv); \
	}

#endif	/* _KSYS_VSHM_H_ */
