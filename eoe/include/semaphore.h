/*
 * Copyright 1995, Silicon Graphics, Inc. 
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
 * rights reserved under the Copyright Lanthesaws of the United States.
 */
#ident "$Id: semaphore.h,v 1.22 1998/12/02 22:10:19 jph Exp $"

#ifndef __SEMAPHORE_H__
#define __SEMAPHORE_H__

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Interface definition file for POSIX 1003.1b semaphores
 */

#include <sys/types.h>
#include <fcntl.h>

#if _NO_XOPEN5
#include <stdio.h>
#endif

#ifndef _SEM_SGI_INTERNAL
typedef struct sem_s {
	__uint64_t	sem_reserved[8];
} sem_t;
#endif

/*
 * Structures for semaphore metering and tracing
 */

struct sem_meter_s {
	int	sem_m_waithits;		/* # of wait ops w/o waiting	   */
	int	sem_m_waits;		/* # of wait & trywait ops	   */
	int	sem_m_posts;		/* # of post ops		   */
	int	sem_m_posthits;		/* # of post ops w/o waking	   */
	int	sem_m_waiters;		/* current length of wait queue	   */
	int	sem_m_maxwaiters;	/* max length of wait queue	   */
	int	sem_m_reserved[4];	/* reserved for future use	   */
};

struct sem_debug_s {
	pid_t	sem_d_owner_pid;	/* pid of current owner (or -1)	   */
 	int 	sem_d_owner_tid;	/* tid of current owner (or -1)	   */
	pid_t	sem_d_last_pid;		/* pid of last op caller	   */
 	int 	sem_d_last_tid;		/* tid of last owner (or -1)	   */
	union {  
		char*	    sem_d_ownerpc;    /* PC of current owner	   */
	 	__uint64_t  sem_d_align;
	} sem_d_opc;
	union {
		char*	    sem_d_lastpc;     /* PC of last caller	   */
	 	__uint64_t  sem_d_align;
	} sem_d_lpc;
	int	sem_d_reserved[4];	      /* reserved for future use   */
};

typedef struct sem_trace_s {
	struct sem_meter_s	sem_t_meter;  /* Meter information	   */
	struct sem_debug_s	sem_t_debug;  /* Debug information	   */
	int    __st_l[12];		      /* reserved for system use   */
} sem_trace_t;

/*
 * Semaphore debug and trace easy access field definitions
 */
#define sem_waithits	sem_t_meter.sem_m_waithits
#define sem_waits	sem_t_meter.sem_m_waits
#define sem_posts	sem_t_meter.sem_m_posts
#define sem_posthits	sem_t_meter.sem_m_posthits
#define sem_waiters	sem_t_meter.sem_m_waiters
#define sem_maxwaiters	sem_t_meter.sem_m_maxwaiters

#define sem_owner_pid	sem_t_debug.sem_d_owner_pid
#define sem_owner_tid	sem_t_debug.sem_d_owner_tid
#define sem_owner_pc	sem_t_debug.sem_d_opc.sem_d_ownerpc
#define sem_last_pid	sem_t_debug.sem_d_last_pid
#define sem_last_tid	sem_t_debug.sem_d_last_tid
#define sem_last_pc	sem_t_debug.sem_d_lpc.sem_d_lastpc

/*
 * sem_mode commands
 */
#define SEM_MODE_TRACEINIT	1
#define SEM_MODE_METERON	2
#define SEM_MODE_METEROFF	3
#define SEM_MODE_METERRESET	4
#define SEM_MODE_METERFETCH	5
#define SEM_MODE_DEBUGON	6
#define SEM_MODE_DEBUGRESET	7
#define SEM_MODE_DEBUGOFF	8
#define SEM_MODE_DEBUGFETCH	9
#define SEM_MODE_NOCNCL		10
#define SEM_MODE_SPINSET	12

/*
 * sem_open error return
 */
#define SEM_FAILED		((sem_t *) -1L)

/*
 * Semaphore interface prototypes
 */

int sem_init(sem_t *, int, unsigned int);
int sem_destroy(sem_t *);
sem_t *sem_open(const char *, int, ...);
int sem_close(sem_t *);
int sem_unlink(const char *);
int sem_wait(sem_t *);
int sem_trywait(sem_t *);
int sem_post(sem_t *);
int sem_getvalue(sem_t *, int *);
#if _NO_XOPEN5
int sem_mode(sem_t *, int, ...);
int sem_print(sem_t *, FILE *, const char *);
#endif

#ifdef __cplusplus
}
#endif
#endif /* __SEMAPHORE_H__ */
