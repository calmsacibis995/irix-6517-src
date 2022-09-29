/**************************************************************************
 *									  *
 * 		 Copyright (C) 1994-1995 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ifndef	_KSYS_PID_H_
#define	_KSYS_PID_H_	1

/* 
 * For pid_getlist, we define the concept of a pid ordinal.  There is a 
 * bijection betwen pid ordinals and possible pid's.  Scanning through
 * valid pids in order of ascending pid ordinals will generally be the
 * most efficient way of enumerating pids in use.
 */
typedef pid_t pidord_t;

struct vproc;
extern void	pid_init(void);
extern int	pid_alloc(pid_t, pid_t *);
extern void	pid_associate(pid_t, struct vproc *);
extern void	pid_op(int, pid_t);
extern int      pid_getlist(pidord_t *next, size_t *count, 
			    pid_t *pidbuf, pidord_t *ordbuf);

extern struct vproc	*pid_to_vproc(pid_t, int flags);
extern struct vproc	*idbg_pid_to_vproc(pid_t);
extern struct proc	*idbg_pid_to_proc(pid_t);

#define P_PROC_FREE		1
#define P_PGRP_JOIN		2
#define P_PGRP_LEAVE		3
#define P_SESS_JOIN		4
#define P_SESS_LEAVE		5
#define P_BATCH_JOIN		6
#define P_BATCH_LEAVE		7

#define PID_PROC_FREE(pid)		pid_op(P_PROC_FREE, pid)
#define PID_PGRP_JOIN(pid)		pid_op(P_PGRP_JOIN, pid)
#define PID_PGRP_LEAVE(pid)		pid_op(P_PGRP_LEAVE, pid)
#define PID_SESS_JOIN(pid)		pid_op(P_SESS_JOIN, pid)
#define PID_SESS_LEAVE(pid)		pid_op(P_SESS_LEAVE, pid)
#define PID_BATCH_JOIN(pid)		pid_op(P_BATCH_JOIN, pid)
#define PID_BATCH_LEAVE(pid)		pid_op(P_BATCH_LEAVE, pid)

#endif	/* _KSYS_PID_H_ */
