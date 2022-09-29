#ifndef _CV_H_
#define	_CV_H_

/**************************************************************************
 *									  *
 * 		 Copyright (C) 1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#include "mtx.h"
#include "q.h"
#include "cvattr.h"


/* Condition variable
 */
typedef struct cv {
	slock_t		cv_lock;	/* lock and q must be together */
	q_t		cv_q;
	unsigned int	cv_waiters;	/* waiters in the kernel */
	cvattr_t	cv_attr;
} cv_t;

#endif /* !_CV_H_ */
