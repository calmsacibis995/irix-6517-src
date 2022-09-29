/**************************************************************************
 *									  *
 * 		 Copyright (C) 1986-1993, Silicon Graphics, Inc.	  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 3.46 $"

#include "ml/ml.h"

	/*
	 * This must be the first address in the loaded kernel
	 */
	EXPORT(first_address)

#if IP22
	/*
	 * Work around cache flushing bug in Indy PROMs
	 */
	.text
	.space 256
#endif


#if !MP
/* Empty stub as we only migrate on MP kernels */
LEAF(migrate_timeouts)
	j	ra
END(migrate_timeouts)

#else /* MP */
/* Some compilers (i.e. 64-bit) don't like to compile this module if there
 * is no code present.
 */
LEAF(_dummy_proc)
	j	ra
END(_dummy_proc)	
#endif /* !EVEREST */

/* cpu_waitforreset - wait for reset or poweroff after a panic
 *
 * Processors with soft power-off should override this to 
 * poll the power or reset switches.  Never returns.
 */
#if IP20 || EVEREST || SN0
LEAF(cpu_waitforreset)
1:	b	1b
    END(cpu_waitforreset)
#endif /* IP20 || EVEREST */
