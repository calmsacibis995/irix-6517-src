/**************************************************************************
 *									  *
 * 		 Copyright (C) 1995 Silicon Graphics, Inc.	  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident "$Revision: 1.2 $"

#include <sys/types.h>
#include <unistd.h>
#include <ckpt.h>
#include <ckpt_internal.h>

#define	ckpt_spinunlock(lockp)	(*(lockp) = 0)

extern pid_t vtoppid_low(pid_t);
extern int ckpt_sid_barrier(ckpt_pi_t *, pid_t);
extern void ckpt_update_proclist_states(unsigned long, uint_t);
extern unsigned long ckpt_spinlock(unsigned long *);
extern int ckpt_mutex_acquire(ckpt_mutex_t *);
extern int ckpt_mutex_rele(ckpt_mutex_t *);
extern int ckpt_test_then_add(ulong_t *, int);
extern int ckpt_add_then_test(ulong_t *, int);
extern void ckpt_perror(char *, int);
extern int __close(int);
extern void setbrk(caddr_t);
extern void *ckpt_malloc(long);
extern void ckpt_free(void *);
extern void ckpt_strcat(char *, char*);
extern char * ckpt_strcpy(char *, char *);
extern int ckpt_strlen(char *);
extern void ckpt_itoa(char *, int);
extern int ckpt_read_share_property_low(ckpt_ta_t *, ckpt_magic_t, int *);
extern int ckpt_init_proc_property(ckpt_ta_t *);
extern void ckpt_free_proc_property(ckpt_ta_t *);
extern int ckpt_read_proc_property_low(ckpt_ta_t *, ckpt_magic_t);
extern int ckpt_setroot(ckpt_ta_t *ta, pid_t mypid);
extern int ckpt_restore_identity_late(ckpt_ta_t *, pid_t);
extern char * ckpt_alloc_mempool(long, char *, long *);
#ifdef DEBUG_FDS
extern void ckpt_scan_fds(ckpt_ta_t *);
#endif
#if defined(DEBUG) || defined(DEBUG_READV)
extern void ckpt_debug(pid_t, char *, long, ulong_t);
extern pid_t debug_pid;
#endif
#ifdef DEBUG_READV
extern void ckpt_dump_iov(iovec_t *, int);
#endif
#ifdef DEBUG
extern void ckpt_log(char *, char *, long, long, long);
#endif
extern void ckpt_abort(int cprfd, pid_t mypid, ulong pindex);
extern int ckpt_relvm(ckpt_ta_t *, char *, int, ulong_t);
extern int ckpt_exit_barrier(void);


extern int *errfd_p;
extern pid_t *errpid_p;
extern int *__errnoaddr;
extern ckpt_sid_t *sidlist;

#define ERRNO *__errnoaddr

#define PROCNAMELEN     12
#define MAXMAP  20
#define MAXDATASIZE     4096


