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

#ident	"$Revision: 1.4 $"

#include "ml/ml.h"

#if IP20 || IP22 || IP32 || IPMHSIM
LEAF(_j_exceptnorm)
XLEAF(_j_exceptnorm_nosymmon)
	.set    noreorder
	.set	noat		# must be set so li doesn't use at
	nop			# need 4 instruction HW delay 
	nop			#
	j	exception
	nop			# BDSLOT
	.set	at
	.set 	reorder
EXPORT(_j_endexceptnorm_nosymmon)
EXPORT(_j_endexceptnorm)
	END(_j_exceptnorm)
#endif	/* IP20 || IP22 || IP32 || IPMHSIM */

#if !(IP20 || IP22 || IP32 || IPMHSIM)
/*
 * The following jump is copied by hook_exceptions to location E_VEC.
 *
 * To make it possible to have a mix of cpus which require unique
 * general exception handlers in the same machine, this code loads
 * the exception address via private.
 *
 * R10000_SPECULATION_WAR: $sp not valid here.
 */
	AUTO_CACHE_BARRIERS_DISABLE
LEAF(_j_exceptnorm)
	.set	noat		# must be set so li doesn't use at
	.set    noreorder
#ifdef ULI_TSTAMP1
	ULI_GET_TS(k0, k1, TS_GENEX, uli_tstamps)
	.set    noreorder
#endif
	PTR_L	k0, VPDA_EXCNORM
	nop			# LDSLOT
	j	k0
	nop			# BDSLOT
	.set 	reorder
	.set	at
EXPORT(_j_endexceptnorm)
	END(_j_exceptnorm)
	
LEAF(_j_exceptnorm_nosymmon)
	.set	noat		# must be set so li doesn't use at
	.set    noreorder
#ifdef ULI_TSTAMP1
	ULI_GET_TS(k0, k1, TS_GENEX, uli_tstamps)
	.set    noreorder
#endif
	nop
	nop
	j	exception
	nop
	.set 	reorder
	.set	at
EXPORT(_j_endexceptnorm_nosymmon)
	END(_j_exceptnorm_nosymmon)
	AUTO_CACHE_BARRIERS_ENABLE
#endif	/* !(IP20 || IP22 || IP32 || IPMHSIM) */
	
LEAF(_exceptnorm_prolog)
	.set	noat		# must be set so li doesn't use at
	.set    noreorder
#ifdef ULI_TSTAMP1
	ULI_GET_TS(k0, k1, TS_GENEX, uli_tstamps)
	.set    noreorder
#endif
#if R4000
	/* R4000 needs delay before reading C0_CAUSE */
	nop
	nop
	nop
	nop
#endif
#if R10000
#endif /* R10000 */	
	.set 	reorder
	.set	at
EXPORT(_endexceptnorm_prolog)
	END(_exceptnorm_prolog)
