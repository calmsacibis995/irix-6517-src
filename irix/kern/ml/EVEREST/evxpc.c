/**************************************************************************
 *									  *
 * 		 Copyright (C) 1997, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 1.1 $"

#if defined(MULTIKERNEL)

#include "sys/debug.h"
#include "sys/types.h"
#include "sys/pfdat.h"
#include "sys/cmn_err.h"
#include "ksys/xpc.h"
#include "ksys/partition.h"

#define	BTE_LEN_MINSIZE			0x80
#define REMOTE_HUB_ADDR(nasid, reg)	(uint64_t*)((uint64_t)nasid)
typedef uint64_t	hubreg_t;
typedef void		*bte_handle_t;

#define TO_PHYS(x)	(((x) & TO_PHYS_MASK))
#define cputonasid(x)	(x)

#include "ml/SN/xpc_private.h"

extern void evmk_send_ccintr( long long );
#define bte_copy(bte,src,dest,len,mode)	_bte_copy(src,dest,len)

int
sendint( xpr_t *xpr)
{
	evmk_send_ccintr( (long long) xpr->xpr_target );
	return(0);
}

/*ARGSUSED*/
void *
bte_acquire( int wait )
{
	return( (void *)1LL);
}

/*ARGSUSED*/
void
bte_release( void *cookie )
{
}

lock_t _bte_lock;

void
_bte_init(void)
{
	spinlock_init(&_bte_lock, "_bte_lock");
}

/*ARGSUSED*/
int
_bte_copy( paddr_t src, paddr_t dest, unsigned len)
{
	int	s;

	if (IS_KPHYS(src))
		src = PHYS_TO_K0(src);

	if (IS_KPHYS(dest))
		dest = PHYS_TO_K0(dest);

	if ((!IS_KSEG0(src)) || (!IS_KSEG0(dest)))
		cmn_err(CE_PANIC, "bte_copy: bad src (0x%x) or dest (0x%x)\n",
			src, dest);

	s = mutex_spinlock(&_bte_lock);
	bcopy( (void *)src, (void *)dest, len );
	mutex_spinunlock(&_bte_lock, s);
	return(0);
}

#include <ml/SN/xpc.c>

#endif /* MULTIKERNEL */
