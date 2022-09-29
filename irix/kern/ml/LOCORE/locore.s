/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989-1995, Silicon Graphics, Inc.	  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident	"$Revision: 1.1 $"

#include "ml/ml.h"
#include "sys/traplog.h"

/* physical address of the first memory location */
#if MCCHIP
ABS(_physmem_start,SEG0_BASE)
#elif HEART_CHIP
ABS(_physmem_start,PHYS_RAMBASE)
#else
ABS(_physmem_start,0x0)
#endif
