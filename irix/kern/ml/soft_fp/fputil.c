/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1994, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/* ====================================================================
 * ====================================================================
 *
 * Module: fputil.c
 * $Revision: 1.17 $
 * $Date: 1998/09/11 17:01:54 $
 * $Author: sprasad $
 * $Source: /proj/irix6.5.7m/isms/irix/kern/ml/soft_fp/RCS/fputil.c,v $
 *
 * Revision history:
 *  12-Jun-94 - Original Version
 *
 * Description:	utility routines used by softfp emulation package
 *
 *
 * ====================================================================
 * ====================================================================
 */

#ident "$Revision: 1.17 $"

/* define __FPT to compile for the fpt test package */

#ifndef _KERNEL
#include <inttypes.h>
#endif

#include "fpparams.h"
#include "fpexterns.h"

#ifdef	_KERNEL
#include <sys/types.h>
#include <sys/pda.h>
#include <sys/systm.h>
#include <ksys/exception.h>
#include <sys/debug.h>
#include <sys/fpu.h>
#include <sys/proc.h>
#endif

void
fpstore_sd(int32_t precision, int64_t val)
{
	if ( precision == SINGLE )
	{
		FPSTORE_S( val );
	}
	else
	{
		FPSTORE_D( val );
	}
}

void
fpstore_d(int64_t val, int regnum, int extregmode)
{
	int s = splhi();
	int cu_bits;

	if (curuthread == private.p_fpowner) {
		cu_bits = extregmode ? SR_CU1 | SR_FR : SR_CU1;
		setsr(getsr() | cu_bits);
		fpunit_fpstore_d(val, regnum, extregmode);
		splx(s);
		return;
	}

	splx(s);

	if (extregmode) {
		PCB(pcb_fpregs)[regnum] = val;
	} else {
		ASSERT( !(regnum & 1));
		PCB(pcb_fpregs)[regnum] = val & 0x00000000ffffffffull;
		PCB(pcb_fpregs)[regnum + 1] =
					(val >> 32) & 0x00000000ffffffffull;
	}
}

void
fpstore_s(int64_t val, int regnum, int extregmode)
{
	int s = splhi();
	int cu_bits;

	if (curuthread == private.p_fpowner) {
		cu_bits = extregmode ? SR_CU1 | SR_FR : SR_CU1;
		setsr(getsr() | cu_bits);
		fpunit_fpstore_s(val, regnum);
		splx(s);
		return;
	}
	splx(s);

	PCB(pcb_fpregs)[regnum] = val;
}

int64_t
fpload_d(int32_t regnum, int extregmode)
{
	int s = splhi();
	int64_t t;
	int cu_bits;

	if (curuthread == private.p_fpowner) {
		cu_bits = extregmode ? SR_CU1 | SR_FR : SR_CU1;
		setsr(getsr() | cu_bits);
		t = fpunit_fpload_d(regnum, extregmode);
		splx(s);
		return t;
	}

	splx(s);

	if (extregmode) {
		t = PCB(pcb_fpregs)[regnum];
	} else {
		uint32_t temp;

		ASSERT( !(regnum & 1));
		temp = (uint32_t)PCB(pcb_fpregs)[regnum];

		t = (PCB(pcb_fpregs)[regnum + 1]) << 32;
		t |= (int64_t)temp & 0x00000000ffffffffull;
	}

	return t;
}

int64_t
fpload_s(int32_t regnum, int extregmode)
{
	int s = splhi();
	int64_t t;
	int cu_bits;

	if (curuthread == private.p_fpowner) {
		cu_bits = extregmode ? SR_CU1 | SR_FR : SR_CU1;
		setsr(getsr() | cu_bits);
		t = fpunit_fpload_s(regnum);
		splx(s);
		return t;
	}

	splx(s);

	t = PCB(pcb_fpregs)[regnum] & 0x00000000ffffffffull;
	return t;
}

void
store_fpcsr()
{

	int32_t csr;
	int32_t enables;
	int s;

	csr = GET_LOCAL_CSR();

	if (!(GET_SOFTFP_FLAGS() & SIG_POSTED) &&
	     curuthread->ut_fp_enables != 0) {
		/* If ut_fp_enables is non-zero, then this exception
		 * occurred in the nofpefrom/nofpeto range - we have
		 * to restore the enables, also masking out
		 * cause bits in csr if the corresponding enable is set.
		 */

		/* ut_fp_enables was previously masked with CSR_ENABLES */
		enables = curuthread->ut_fp_enables;

		/* restore the enables into csr */
		csr |= enables;

		/* isolate the bits that are set in both the 'cause'
		 * and 'enables' field of the csr, and shut those off
		 * in the csr cause field.
		 */
		enables = (enables << 5) & (csr & CSR_EXCEPT);
		csr ^= enables;
	}

	s = splhi();

	if ((GET_SOFTFP_FLAGS() & SIG_POSTED) ||
	    (curuthread != private.p_fpowner)) {
		PCB(pcb_fpc_csr) = csr;
	} else {
		/* Store into fp unit. */
		setsr(getsr() | SR_CU1);
		set_fpc_csr(csr);
	}

	splx(s);
}

int32_t
load_fpcsr(void)
{
	int s = splhi();
	int32_t csr;

	if (curuthread == private.p_fpowner) {
		setsr(getsr() | SR_CU1);
		csr = get_fpc_csr();		/* reads the fp unit */
	} else {
		csr = PCB(pcb_fpc_csr);
	}
	splx(s);
	return csr;
}

void
post_sig( int32_t sig )
{
	if ( !(GET_SOFTFP_FLAGS() & SIG_POSTED) ) {
		softfp_psignal( sig,
				GET_LOCAL_CSR(), curuthread->ut_excpt_fr_ptr);
		SET_SOFTFP_FLAGS(GET_SOFTFP_FLAGS() | SIG_POSTED);
                checkfp(curuthread, 0);
	}
}

/*
 * parameters:
 * p_excpt_fr (exception frame pointer)
 * reg_mode (extended register mode)
 * fpcsr (floating point control status register)
 * ioflush (control flushing to zero on input and output)
 * oreg (number of output register)
 */

void
fp_init_glob(
	void	*p_excpt_fr,
	int32_t	reg_mode,
	int32_t	fpcsr,
	int32_t fp_enables,
	int32_t	ioflush,
	int32_t	oreg)
{
	char ext_reg = reg_mode ? EXT_REG_MODE : 0;

	curuthread->ut_excpt_fr_ptr = p_excpt_fr;
	SET_LOCAL_CSR(fpcsr);
	curuthread->ut_fp_enables = fp_enables;
	SET_FP_CSR_RM(fpcsr & CSR_RM_MASK);
	SET_SOFTFP_FLAGS(ioflush | ext_reg | STORE_ON_ERROR);
	curuthread->ut_rdnum = (char)oreg;

#ifdef __FPT
	curuthread->ut_fpval.ll = 0;
#endif
}


void
breakout_sd(precision, rs, rssign, rsexp, rsfrac)
int32_t	precision;
int64_t *rs;
uint32_t *rssign;
int32_t *rsexp;
uint64_t *rsfrac;
{

	*rsexp = (*rs >> EXP_SHIFT[precision]);
	*rsexp &= EXP_MASK[precision];

	*rssign = (*rs >> SIGN_SHIFT[precision]);
	*rssign &= 1;

	*rsfrac = (*rs & FRAC_MASK[precision]);
 
	if ( GET_SOFTFP_FLAGS() & FLUSH_INPUT_DENORMS )
	{
		if ( (*rsexp == 0) && (*rsfrac != 0ull) )
		{
			/* input is a denormal; flush to zero and
			 * set inexact exception bit in fp csr
			 */

			*rsfrac = 0ull;
			*rs = (((int64_t)(*rssign)) << SIGN_SHIFT[precision]);

			SET_LOCAL_CSR(GET_LOCAL_CSR() | INEXACT_EXC);

			/* post signal if inexact trap is enabled */

			if ( GET_LOCAL_CSR() & INEXACT_ENABLE )
			{
				post_sig(SIGFPE);
			}
		}
	}
}

/* ARGSUSED */
int32_t
screen_nan_sd(precision, rs, rsexp, rsfrac)
int32_t	precision;
int64_t rs;
int32_t rsexp;
uint64_t rsfrac;
{

	if ( rsexp != EXP_NAN[precision] )
		return ( 0 );

	if ( rsfrac & SNANBIT_MASK[precision] )
	{
		SET_LOCAL_CSR(GET_LOCAL_CSR() | INVALID_EXC);

		/* post signal if invalid trap is enabled,
		 * otherwise store a quiet NaN
		 */

		if ( GET_LOCAL_CSR() & INVALID_ENABLE )
		{
			post_sig(SIGFPE);
		}
		else
		{
			fpstore_sd(precision, QUIETNAN[precision] );
		}

		store_fpcsr();

		return ( 1 );

	}

	if ( rsfrac != 0 )
	{
		fpstore_sd(precision, rs );

		store_fpcsr();

		return ( 1 );
	}

	return ( 0 );
}

int32_t
breakout_and_test_sd(precision, rs, rssign, rsexp, rsfrac)
int32_t	precision;
int64_t *rs;
uint32_t *rssign;
int32_t *rsexp;
uint64_t *rsfrac;
{

	breakout_sd(precision, rs, rssign, rsexp, rsfrac);

	if ( screen_nan_sd(precision, *rs, *rsexp, *rsfrac) )
		return ( 1 );

	return ( 0 );
}

int32_t
breakout_and_test2_sd(precision, rs, rssign, rsexp, rsfrac,
				 rt, rtsign, rtexp, rtfrac)
int32_t	precision;
int64_t *rs;
uint32_t *rssign;
int32_t *rsexp;
uint64_t *rsfrac;
int64_t *rt;
uint32_t *rtsign;
int32_t *rtexp;
uint64_t *rtfrac;
{

	breakout_sd(precision, rs, rssign, rsexp, rsfrac);

	breakout_sd(precision, rt, rtsign, rtexp, rtfrac);

	if ( (*rsexp == EXP_NAN[precision]) && (*rsfrac & SNANBIT_MASK[precision]) ||
	     (*rtexp == EXP_NAN[precision]) && (*rtfrac & SNANBIT_MASK[precision]) 
	   )
	{
		SET_LOCAL_CSR(GET_LOCAL_CSR() | INVALID_EXC);

		/* post signal if invalid trap is enabled,
		 * otherwise store a quiet NaN
		 */

		if ( GET_LOCAL_CSR() & INVALID_ENABLE )
		{
			post_sig(SIGFPE);
		}
		else
		{
			fpstore_sd(precision, QUIETNAN[precision] );
		}

		store_fpcsr();

		return ( 1 );
	}

	if ( (*rsexp == EXP_NAN[precision]) && (*rsfrac != 0) )
	{
		fpstore_sd(precision, *rs );

		store_fpcsr();

		return ( 1 );
	}

	if ( (*rtexp == EXP_NAN[precision]) && (*rtfrac != 0) )
	{
		fpstore_sd(precision, *rt );

		store_fpcsr();

		return ( 1 );
	}

	return ( 0 );
}

int32_t
breakout_and_test3_sd(precision, rs, rssign, rsexp, rsfrac,
				 rt, rtsign, rtexp, rtfrac,
				 rr, rrsign, rrexp, rrfrac)
int32_t	precision;
int64_t *rs;
uint32_t *rssign;
int32_t *rsexp;
uint64_t *rsfrac;
int64_t *rt;
uint32_t *rtsign;
int32_t *rtexp;
uint64_t *rtfrac;
int64_t *rr;
uint32_t *rrsign;
int32_t *rrexp;
uint64_t *rrfrac;
{

	breakout_sd(precision, rs, rssign, rsexp, rsfrac);

	breakout_sd(precision, rt, rtsign, rtexp, rtfrac);

	breakout_sd(precision, rr, rrsign, rrexp, rrfrac);

	if ( (*rsexp == EXP_NAN[precision]) && (*rsfrac & SNANBIT_MASK[precision]) ||
	     (*rtexp == EXP_NAN[precision]) && (*rtfrac & SNANBIT_MASK[precision]) ||
	     (*rrexp == EXP_NAN[precision]) && (*rrfrac & SNANBIT_MASK[precision]) 
	   )
	{
		SET_LOCAL_CSR(GET_LOCAL_CSR() | INVALID_EXC);

		/* post signal if invalid trap is enabled,
		 * otherwise store a quiet NaN
		 */

		if ( GET_LOCAL_CSR() & INVALID_ENABLE )
		{
			post_sig(SIGFPE);
		}
		else
		{
			fpstore_sd(precision, QUIETNAN[precision] );
		}

		store_fpcsr();

		return ( 1 );
	}

	if ( (*rsexp == EXP_NAN[precision]) && (*rsfrac != 0) )
	{
		fpstore_sd(precision, *rs );

		store_fpcsr();

		return ( 1 );
	}

	if ( (*rtexp == EXP_NAN[precision]) && (*rtfrac != 0) )
	{
		fpstore_sd(precision, *rt );

		store_fpcsr();

		return ( 1 );
	}

	if ( (*rrexp == EXP_NAN[precision]) && (*rrfrac != 0) )
	{
		fpstore_sd(precision, *rr );

		store_fpcsr();

		return ( 1 );
	}

	return ( 0 );
}

