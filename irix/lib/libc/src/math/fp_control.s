/* ------------------------------------------------------------------ */
/* | Copyright Unpublished, MIPS Computer Systems, Inc.  All Rights | */
/* | Reserved.  This software contains proprietary and confidential | */
/* | information of MIPS and its suppliers.  Use, disclosure or     | */
/* | reproduction is prohibited without the prior express written   | */
/* | consent of MIPS.                                               | */
/* ------------------------------------------------------------------ */
/* $Header: /proj/irix6.5.7m/isms/irix/lib/libc/src/math/RCS/fp_control.s,v 1.5 1997/10/12 08:29:50 jwag Exp $ */

/*
 * This file contains routines to get and set the floating point control
 * registers.
 */

.weakext get_fpc_csr, _get_fpc_csr
.weakext set_fpc_csr, _set_fpc_csr
.weakext get_fpc_irr, _get_fpc_irr

#include "regdef.h"
#include "asm.h"
#ifdef BSD
#include "mips/fpu.h"
#endif /* BSD */
#include "sys/fpu.h"

/*
 * get_fpc_csr returns the fpc_csr.
 */
LEAF(_get_fpc_csr)
	cfc1	v0,fpc_csr
	j	ra
	END(_get_fpc_csr)

/*
 * set_fpc_csr sets the fpc_csr and returns the old fpc_csr.
 */
LEAF(_set_fpc_csr)
	cfc1	v0,fpc_csr
	ctc1	a0,fpc_csr
	j	ra
	END(_set_fpc_csr)

/*
 * get_fpc_irr returns the fpc_irr.
 */
LEAF(_get_fpc_irr)
	cfc1	v0,fpc_irr
	j	ra
	END(_get_fpc_irr)
