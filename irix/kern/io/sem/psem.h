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
#ifndef	_PSEM_H
#define	_PSEM_H 1
#ident "$Id: psem.h,v 1.3 1999/05/19 20:21:21 bcasavan Exp $"

#ifdef DEBUG
#define SEMDEBUG 1
#endif

#include <ksys/vsem.h>
#include <sys/ipc.h>

typedef struct svsem {
	sv_t		svs_nwait;	/* to wait for non-zero value */
	sv_t		svs_zwait;	/* to wait for zero value */
	pid_t		svs_pid;	/* pid of last operation */
	ushort_t	svs_val;	/* semaphore value */
	ushort_t	svs_ncnt;	/* # awaiting semval > cval */
	ushort_t	svs_zcnt;	/* # awaiting semval = 0 */
	ushort_t	svs_flags;	/* internal flags */
} svsem_t;

#define SVSEM_WAITMULTI ((ushort_t)0x1)	/* At least 1 thread waiting on */
					/* a value of 2 or more */

typedef struct semundo {
	struct semundo	*su_next;
	pid_t		su_pid;
	short		*su_entry;	/* psem->psm_nsems long */
} semundo_t;

/*
 * physical sem descriptor
 */
typedef struct psem {
	struct ipc_perm psm_perm;	/* operation permission struct */
	bhv_desc_t	psm_bd;		/* behavior descriptor */
	struct mac_label *psm_maclabel;	/* MAC label */
	mutex_t		psm_lock;
	int		psm_nsems;
	svsem_t		*psm_semarray;	/* The semaphores themselves */
	time_t		psm_optime;	/* last semop time */
	time_t		psm_ctime;	/* last change time */
	semundo_t	*psm_undo;	/* undo table */
} psem_t;

#endif	/* _PSEM_H */
