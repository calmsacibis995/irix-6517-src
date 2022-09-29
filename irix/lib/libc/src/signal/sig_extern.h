/**************************************************************************
 *									  *
 * Copyright (C) 1986-1993 Silicon Graphics, Inc.			  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ifndef __SIG_EXTERN_H__
#define __SIG_EXTERN_H__

#include <signal.h>
#include <siginfo.h>
#include <sys/timers.h>

/* setlinkctxt.s */
extern int __setlinkcontext(void);
extern int __getgp(void);

/* ssig.c */
extern int _ssig(int , sigaction_t *, sigaction_t *);

/* sigpoll.s */
extern int sigpoll(const sigset_t *, struct siginfo *, const struct timespec *);

/* psiginfo.c */
extern int _siginfo_msg_offset[];

/* possig.s */
extern int ksigaction(int , const struct sigaction *, struct sigaction *, void (*)(int ,...));

/* sigtramp.s */
extern void _sigtramp(int ,...);

/* ksigqueue.s */
extern int ksigqueue(pid_t, int, int, void *);

/* sigfillset.c - different from sigfillset prototyped in signal.h */
extern void __sigfillset(sigset_t *);

/* ksigprocmask.s */
extern int _ksigprocmask(int, const sigset_t *, sigset_t *);

/* sigset.s */
extern void __ksigorset(k_sigset_t *, k_sigset_t *);
extern void __ksigdiffset(k_sigset_t *, k_sigset_t *);

#endif
