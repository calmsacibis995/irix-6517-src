#ifndef _RWLATTR_H_
#define _RWLATTR_H_

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

/* Global names
 */
#define rwlattr_default		PFX_NAME(rwlattr_default)


/* read-write lock attributes
 */
typedef struct rwlattr {
	unsigned char	ra_pshared:1;	/* pshared rwlock */
} rwlattr_t;

extern rwlattr_t rwlattr_default;

#endif /* !_RWLATTR_H_ */

