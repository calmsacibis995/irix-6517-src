/**************************************************************************
 *									  *
 * 		 Copyright (C) 1996, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

/*
 * SN0 specific assembly routines
 */

#ident	"$Revision: 1.1 $"

#include <ml/ml.h>
#include <sys/loaddrs.h>
#include <sys/dump.h>
#include <sys/mapped_kernel.h>
#include <sys/sbd.h>
#include <sys/SN/klkernvars.h>

/* stubs that just return */
LEAF(dummy_func)
XLEAF(dcache_wb)                /* SN0 has coherent I/O - NOP */
XLEAF(dcache_wbinval)
XLEAF(dki_dcache_wb)
XLEAF(dki_dcache_wbinval)
XLEAF(dki_dcache_inval)
        j ra
END(dummy_func)


/*
 * all slave processors come here - assumed on boot stack
 */
BOOTFRM=	FRAMESZ((NARGSAVE+2)*SZREG)	# arg save + 5th/6th args to tlbwired
NESTED(bootstrap, BOOTFRM, zero)
	/* do some one-time only initialization */
	PTR_SUBU sp,BOOTFRM
	.set	noreorder


#ifdef MAPPED_KERNEL
	# Address of node 0 KLDIR structure's kern vars pointer
	LI	t0, KLDIR_OFFSET + (KLI_KERN_VARS * KLDIR_ENT_SIZE) + KLDIR_OFF_POINTER + K0BASE

	# Get our NASID, shift it, and OR it into the address.
	GET_NASID_ASM(t1)
	dsll	t1, NASID_SHFT
	or	t0, t1

	# t1 will be our pointer to the kern_vars structure.
	PTR_L	t1, 0(t0)

	# Load the read-write data and read-only data NASIDs from low memory
	NASID_L	a0, KV_RO_NASID_OFFSET(t1)
	NASID_L	a1, KV_RW_NASID_OFFSET(t1)

	jal	mapped_kernel_setup_tlb
	nop

	LA	t0, 1f			# Load the link address for label 1f
	jr	t0			# Jump to our link (mapped) address
	nop
1:
#endif /* MAPPED_KERNEL */

	/*
	 * Some of the calls below (size_2nd_cache on TFP for example) end up
	 * calling C code, so we need to init gp here.
	 */
	LA	gp,_gp

	j	cboot

	END(bootstrap)



