/* mutexPxLibP.h - kernel mutex and condition header file */

/* Copyright 1993 Wind River Systems, Inc. */

/*
modification history
--------------------
01a,18feb93,rrr  written.
*/

#ifndef __INCmutexPxLibPh
#define __INCmutexPxLibPh

#include "qlib.h"

typedef struct
    {
    int		m_owner;
    Q_HEAD	m_queue;
    } mutex_t;

typedef struct
    {
    mutex_t	*c_mutex;
    Q_HEAD	c_queue;
    } cond_t;

struct timespec;

#define __P(x)	x

extern void mutex_init __P((mutex_t *, void *));
extern void mutex_destroy __P((mutex_t *));

extern void cond_init __P((cond_t *, void *));
extern void cond_destroy __P((cond_t *));

extern void mutex_lock __P((mutex_t *));
extern void mutex_unlock __P((mutex_t *));

extern void cond_signal __P((cond_t *));
extern int cond_timedwait __P((cond_t *, mutex_t *, const struct timespec *));

#endif /* __INCmutexPxLibPh */
