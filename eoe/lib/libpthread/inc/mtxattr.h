#ifndef _MUTEXATTR_H_
#define _MUTEXATTR_H_

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

/*
 * Visible mutex attributes
 */

/* Global names
 */
#define mtxattr_default		PFX_NAME(mtxattr_default)


/* Mutex Attributes Structure */

typedef struct mtxattr {
	char		ma_protocol;		/* mutex protocol */
	schedpri_t	ma_priority;		/* mutex priority */
	char		ma_type;		/* mutex type */
	unsigned char	ma_pshared:1;		/* pshared mutex */
} mtxattr_t;

extern mtxattr_t mtxattr_default;

#define DEFAULT_MUTEX_CEILING   0		/* mutex ceiling priority */
#define DEFAULT_MUTEX_PROTOCOL  PTHREAD_PRIO_NONE	/* mutex protocol */

#endif /* !_MUTEXATTR_H_ */

