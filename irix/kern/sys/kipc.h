/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989-1992 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ifndef _SYS_KIPC_H
#define _SYS_KIPC_H

#ifdef _KERNEL

#include <sys/ktypes.h>

struct irix5_ipc_perm {
	irix5_uid_t	uid;	/* owner's user id */
	irix5_gid_t	gid;	/* owner's group id */
	irix5_uid_t	cuid;	/* creator's user id */
	irix5_gid_t	cgid;	/* creator's group id */
	app32_ulong_t	mode;	/* access modes */
	app32_ulong_t	seq;	/* slot usage sequence number */
	app32_int_t	key;	/* key */
	app32_long_t	pad[4];	/* reserve area */
};

struct irix5_msqid_ds {
	struct irix5_ipc_perm	msg_perm; /* operation permission struct */
	app32_ptr_t 	msg_first;	/* ptr to first message on q */
	app32_ptr_t	msg_last;	/* ptr to last message on q */
	app32_ulong_t	msg_cbytes;	/* current # bytes on q */
	app32_ulong_t	msg_qnum;	/* # of messages on q */
	app32_ulong_t	msg_qbytes;	/* max # of bytes on q */
	irix5_pid_t	msg_lspid;	/* pid of last msgsnd */
	irix5_pid_t	msg_lrpid;	/* pid of last msgrcv */
	app32_long_t	msg_stime;	/* last msgsnd time */
	app32_long_t	msg_pad1;	/* reserved for time_t expansion */
	app32_long_t	msg_rtime;	/* last msgrcv time */
	app32_long_t	msg_pad2;	/* time_t expansion */
	app32_long_t	msg_ctime;	/* last change time */
	app32_long_t	msg_pad3;	/* last change time */
	app32_long_t	msg_pad4[4];	/* reserve area */
};

struct irix5_semid_ds {
	struct irix5_ipc_perm	sem_perm; /* operation permission struct */
	app32_ptr_t	sem_base;	/* ptr to first semaphore in set */
	ushort		sem_nsems;	/* # of semaphores in set */
	app32_long_t	sem_otime;	/* last semop time */
	app32_long_t	sem_pad1;	/* reserved for time_t expansion */
	app32_long_t	sem_ctime;	/* last change time */
	app32_long_t	sem_pad2;	/* time_t expansion */
	app32_long_t	sem_pad3[4];	/* reserve area */
};

struct irix5_shmid_ds {
	struct irix5_ipc_perm	shm_perm; /* operation permission struct */
	int		shm_segsz;	/* size of segment in bytes */
	app32_ptr_t	shm_amp;	/* ptr to nothing - MIPSABI */
	short		shm_lkcnt;	/* nothing - MIPSABI */
	char		pad[2];		/* for swap compatibility */
	irix5_pid_t	shm_lpid;	/* pid of last shmop */
	irix5_pid_t	shm_cpid;	/* pid of creator */
	app32_ulong_t	shm_nattch;	/* used only for shminfo */
	app32_ulong_t	shm_cnattch;	/* used only for shminfo */
	app32_long_t	shm_atime;	/* last shmat time */
	app32_long_t	shm_pad1;	/* reserved for time_t expansion */
	app32_long_t	shm_dtime;	/* last shmdt time */
	app32_long_t	shm_pad2;	/* reserved for time_t expansion */
	app32_long_t	shm_ctime;	/* last change time */
	app32_long_t	shm_pad3;	/* reserved for time_t expansion */
	app32_long_t	shm_pad4[4];	/* reserve area  */
};

struct ipc_perm;
extern void ipc_perm_to_irix5(struct ipc_perm *, struct irix5_ipc_perm *);
extern void irix5_to_ipc_perm(struct irix5_ipc_perm *, struct ipc_perm *);

#endif	/* _KERNEL */
#endif	/* _SYS_KIPC_H */
