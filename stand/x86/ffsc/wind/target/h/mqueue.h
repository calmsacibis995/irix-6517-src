/* mqueue.h - POSIX message queue library header */

/* Copyright 1984-1994 Wind River Systems, Inc. */

/*
modification history
--------------------
01b,05jan94,kdl	 added modhist, added include of sigevent.h, general cleanup.
01a,01dec93,rrr  written.
*/

#ifndef __INCmqueueh
#define __INCmqueueh


#ifdef __cplusplus
extern "C" {
#endif

/* includes */

#include "sigevent.h"


/* type defs */

struct mq_attr
    {
    size_t	mq_maxmsg;
    size_t	mq_msgsize;
    unsigned	mq_flags;
    size_t	mq_curmsgs;
    };

struct mq_des;

typedef struct mq_des *mqd_t;


/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern mqd_t mq_open (const char *, int, ...);
extern int mq_close (mqd_t);
extern int mq_unlink (const char *);
extern int mq_send (mqd_t, const void *, size_t, int);
extern int mq_receive (mqd_t, void *, size_t, int *);
extern int mq_notify (mqd_t, const struct sigevent *);
extern int mq_setattr (mqd_t, const struct mq_attr *, struct mq_attr *);
extern int mq_getattr (mqd_t, struct mq_attr *);

#else	/* __STDC__ */

extern mqd_t mq_open ();
extern int mq_close ();
extern int mq_unlink ();
extern int mq_send ();
extern int mq_receive ();
extern int mq_notify ();
extern int mq_setattr ();
extern int mq_getattr ();

#endif	/* __STDC__ */


#ifdef __cplusplus
}
#endif

#endif	/* INCmqueue.h */
