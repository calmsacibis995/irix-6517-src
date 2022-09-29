#ifndef __SYS_SEM_H__
#define __SYS_SEM_H__

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Copyright 1990, Silicon Graphics, Inc. 
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or 
 * duplicated in any form, in whole or in part, without the prior written 
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions 
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or 
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished - 
 * rights reserved under the Copyright Laws of the United States.
 */
/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"$Revision: 3.38 $"

#include <standards.h>

/*
**	IPC Semaphore Facility.
*/

#include <sys/ipc.h>

/*
**	Permission Definitions.
*/

#define SEM_A	0200	/* alter permission */
#define SEM_R	0400	/* read permission */

/*
**	Semaphore Operation Flags.
*/

#define	SEM_UNDO	010000	/* set up adjust on exit entry */

/*
**	Semctl Command Definitions.
*/

#define	GETNCNT	3	/* get semncnt */
#define	GETPID	4	/* get sempid */
#define	GETVAL	5	/* get semval */
#define	GETALL	6	/* get all semval's */
#define	GETZCNT	7	/* get semzcnt */
#define	SETVAL	8	/* set semval */
#define	SETALL	9	/* set all semval's */

/*
**	Structure Definitions.
*/

/*
**	There is one semaphore id data structure for each set of semaphores
**		in the system.
*/

struct semid_ds {
	struct ipc_perm	sem_perm;	/* operation permission struct */
	void		*sem_base;	/* nothing ... */
	ushort_t	sem_nsems;	/* # of semaphores in set */
	time_t		sem_otime;	/* last semop time */
	long		sem_pad1;	/* reserved for time_t expansion */
	time_t		sem_ctime;	/* last change time */
	long		sem_pad2;	/* time_t expansion */
	long		sem_pad3[4];	/* reserve area */
};

#if _SGIAPI
struct semstat {
	int		sm_id;
	uint64_t	sm_location;
	cell_t		sm_cell;
	struct semid_ds	sm_semds;
};
#endif /* _SGIAPI */

/*
**	User semaphore template for semop system calls.
*/
struct sembuf {
	ushort_t sem_num;	/* semaphore # */
	short	sem_op;		/* semaphore operation */
	short	sem_flg;	/* operation flags */
};


#if _NO_XOPEN4
union semun {
        int val;
        struct semid_ds *buf;
        ushort_t *array;
};
#endif	/* _NO_XOPEN4 */

#ifndef _KERNEL
extern int 	semctl (int, int, int, ...);
extern int	semget (key_t, int, int);
extern int	semop  (int, struct sembuf *, size_t);
#endif

#ifdef __cplusplus
}
#endif

#endif /* !__SYS_SEM_H__ */
