
#ifndef __MEMSTR_H__
#define __MEMSTR_H__

/*
 * include/memstr.h
 *
 *
 * Copyright 1991, Silicon Graphics, Inc.
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

#ident "$Revision: 1.3 $"



/*
 * memstr -- parameters for address test
 */

struct memstr {
	__psunsigned_t lomem;		/* base address */
	__psunsigned_t himem;		/* last address */
	__psunsigned_t mask;
};

#endif  /* !__MEMSTR_H__ */
