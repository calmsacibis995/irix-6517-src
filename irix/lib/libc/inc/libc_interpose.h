/*
 *
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
 * rights reserved under the Copyright Laws of the United States.
 */
#ifndef _INTERPOSE_H
#define _INTERPOSE_H
#ifdef __cplusplus
extern "C" {
#endif
#ident "$Revision: 1.5 $"

/*
 * prototypes for the various interposition routines within libc.
 * there are currently 2:
 * 1) SpeedShop - trace points for 'important' process events
 * 2) pthreads - all threading goes through these functions that libpthread
 *	can supply different versions for
 */
#include <stdio.h>

/*
 * Tracepoints
 */
extern void __tp_sproc_pre(unsigned int);
extern void __tp_sproc_parent(int, int);
extern void __tp_sproc_child(void);
extern void __tp_fork_pre(void);
extern void __tp_fork_parent(int, int);
extern void __tp_fork_child(void);
extern void __tp_execve_error(int);
extern char *__tp_execve(char *, char **, char **);
extern char **__tp_pcreateve(const char *, char * const*, char * const*);
extern char *__tp_system_pre(const char *);
extern char *__tp_dlinsert_pre(const char *, int);
extern char *__tp_dlinsert_version_pre(const char *, int, const char *, int);
extern void __tp_dlinsert_post(const char *, void *);
extern void __tp_dlremove_pre(void *);
extern void __tp_dlremove_post(void *, int);
extern void __tp_main(int, char **, char **);
extern void __tp_exit(void);

/*
 * Threading routines
 */
extern void *__libc_lockmisc(void);
extern int __libc_islockmisc(void);
extern void __libc_unlockmisc(void *);
extern void *__libc_lockdir(void);
extern int __libc_islockdir(void);
extern void __libc_unlockdir(void *);
extern void *__libc_lockopen(void);
extern int __libc_islockopen(void);
extern void __libc_unlockopen(void *);
extern void *__libc_locklocale(void);
extern int __libc_islocklocale(void);
extern void __libc_unlocklocale(void *);
extern void *__libc_lockfile(FILE *);
extern int __libc_islockfile(FILE *);
extern void __libc_unlockfile(FILE *, void *);
extern int __fislockfile(FILE *);
extern int __libc_lockmalloc(void);
extern int __libc_unlockmalloc(void);
extern int __libc_lockpmq(void);
extern int __libc_unlockpmq(void);
extern void __libc_lockrand(void);
extern void __libc_unlockrand(void);
extern void __libc_lockmon(void);
extern void __libc_unlockmon(void);

/*
 * This routine should be called by libraries that have threading interposers
 * that wish to have libc start making the thread-safe calls
 */
extern void libc_threadinit(void);

#ifdef __cplusplus
}
#endif

#endif /* _INTERPOSE_H */
