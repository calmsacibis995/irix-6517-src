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
#ident "$Id: mqueue.h,v 1.6 1997/11/15 03:36:01 joecd Exp $"

#ifndef __MQUEUE_H__
#define __MQUEUE_H__

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Interface definition file for POSIX 1003.1b message passing
 */

#include <sys/signal.h>

typedef __psint_t mqd_t;

typedef struct mq_attr {
    long mq_flags;
    long mq_maxmsg;
    long mq_msgsize;
    long mq_curmsgs;
} mq_attr_t;

#if	_SGIAPI
/* This definition is not for application use */
#define _MQ_SGI_PRIO_MAX	32
#endif	/* _SGIAPI */

mqd_t mq_open(const char *, int, ...);
int mq_close(mqd_t);
int mq_unlink(const char *);
int mq_send(mqd_t , const char *, size_t, unsigned int);
ssize_t mq_receive(mqd_t, char *, size_t, unsigned int *);
int mq_notify(mqd_t, const struct sigevent *);
int mq_setattr(mqd_t, const struct mq_attr *, struct mq_attr *);
int mq_getattr(mqd_t, struct mq_attr *);

#ifdef __cplusplus
}
#endif
#endif /* __MQUEUE_H__ */
