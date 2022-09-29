#ifndef _CVATTR_H_
#define _CVATTR_H_

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
#define cvattr_default	PFX_NAME(cvattr_default)


/* condition variable attributes
 */
typedef struct cvattr {
	unsigned char	ca_pshared:1;	/* pshared condition variable */
} cvattr_t;

extern cvattr_t cvattr_default;

#endif /* !_CVATTR_H_ */

