#ifndef _STK_H_
#define _STK_H_

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
#define stk_alloc		PFX_NAME(stk_alloc)
#define stk_free		PFX_NAME(stk_free)
#define stk_fork_child		PFX_NAME(stk_fork_child)

#include <stddef.h>

void	*stk_alloc(void *, size_t, size_t);
void	stk_free(void *);
void	stk_fork_child(void);

#endif /* !_STK_H_ */
