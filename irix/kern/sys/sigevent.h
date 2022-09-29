/*
 *
 * Copyright 1996-1997, Silicon Graphics, Inc.
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
#ifndef _SYS_SIGEVENT_H
#define _SYS_SIGEVENT_H
#ident	"$Revision: 1.8 $"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * sigevent is part of POSIX1c - but a bunch of other non-XPG and SGI
 * things need it.
 *
 * Thus, non XPG/POSIX headers that need a sigevent should include
 * this rather than (or in addition to) sys/time.h
 */
#include <standards.h>
#include <sys/types.h>
#include <sys/pthread.h>

#if (_MIPS_SZPTR == 32)
typedef union sigval {
	int	sival_int;
	void	*sival_ptr;
} sigval_t;
#endif
#if (_MIPS_SZPTR == 64)
typedef union sigval {
	long	sival_int;
	void	*sival_ptr;
} sigval_t;
#endif

#if _XOPEN4 && _NO_XOPEN5
#define __notifyinfo notifyinfo
#define __nisigno nisigno
#define __nifunc nifunc
#endif
typedef union __notifyinfo {
	int	__nisigno;			/* signal info */
	void	(*__nifunc) (sigval_t);	 	/* callback data */
} notifyinfo_t;

typedef struct sigevent {
	int			sigev_notify;
	notifyinfo_t		sigev_notifyinfo;
	sigval_t		sigev_value;
	void 			(*sigev_notify_function) (sigval_t);
        pthread_attr_t 		*sigev_notify_attributes;
	unsigned long		sigev_reserved[11];
	unsigned long		sigev_pad[6];
} sigevent_t;
#define sigev_func	sigev_notifyinfo.__nifunc
#define sigev_signo	sigev_notifyinfo.__nisigno

#define SIGEV_NONE	128
#define SIGEV_SIGNAL	129
#define SIGEV_CALLBACK	130
#define SIGEV_THREAD	131

#ifdef __cplusplus
}
#endif

#endif	/* _SYS_SIGEVENT_H */
