#ifndef _PTATTR_H_
#define _PTATTR_H_

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
#define ptattr_default		PFX_NAME(ptattr_default)
#define ptattr_bootstrap	PFX_NAME(ptattr_bootstrap)


#include <stddef.h>

/* pthread attributes
 */
typedef struct ptattr {
	struct {		/* defaults are always zero */
		__uint32_t	detached : 1;
		__uint32_t	system : 1;
		__uint32_t	inherit : 1;
		__uint32_t	policy : 2;
		__uint32_t	priority : 8;
	} pa_fields;
	size_t	pa_stacksize;
	void	*pa_stackaddr;
	size_t	pa_guardsize;
} ptattr_t;

#define pa_detached	pa_fields.detached
#define pa_system	pa_fields.system
#define pa_inherit	pa_fields.inherit
#define pa_policy	pa_fields.policy
#define pa_priority	pa_fields.priority

extern ptattr_t ptattr_default;
extern void	ptattr_bootstrap(void);

#endif /* !_PTATTR_H_ */
