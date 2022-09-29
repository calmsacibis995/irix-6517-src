
/**************************************************************************
 *									  *
 * 		 Copyright (C) 1990-1992, Silicon Graphics, Inc.	  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
/*
 * Copyright 1985 by MIPS Computer Systems, Inc.
 */
#ifndef __SYS_FPREGDEF_H__
#define __SYS_FPREGDEF_H__

#ident	"$Revision: 1.4 $"

#include "sgidefs.h"

/**************************************************************************
 *
 *			Floating Point Register Usage 
 *	
 *	ABI		Register	Purpose			Save By
 *	-------------------------------------------------------------------
 *	o32/n32/n64	$f0, $f2	Function Return		Caller
 *	o32/n32/n64	$f1, $f3	Temp			Caller
 *	o32/n32/n64	$f4..$f11	Temp			Caller
 *
 *	o32		$f12..$f15	2 Function Arguments	Caller
 *	o32		$f16..$f19	Temp			Caller
 *	n32/n64		$f12..$f19	8 Function Arguments	Caller
 *	
 *	o32/n32		$f20..$f31(even)Temp			Callee
 *	n32		$f20..$f31(odd)	Temp			Caller
 *	n64		$f20..$f23	Temp			Caller
 *	n64		$f24..$f31	Temp			Callee
 *	
 *	Remember:  o32 assumes that the only references to odd floating
 *	point registers are as part of 32 bit operations to even/odd pairs
 *	of 32-bit registers.
 *
 **************************************************************************/

#if (_MIPS_SIM == _ABIO32)
#define fv0	$f0	/* return value */
#define fv0f	$f1
#define fv1	$f2
#define fv1f	$f3
#define fa0	$f12	/* argument registers */
#define fa0f	$f13
#define fa1	$f14
#define fa1f	$f15
#define ft0	$f4	/* caller saved */
#define ft0f	$f5
#define ft1	$f6
#define ft1f	$f7
#define ft2	$f8
#define ft2f	$f9
#define ft3	$f10
#define ft3f	$f11
#define ft4	$f16
#define ft4f	$f17
#define ft5	$f18
#define ft5f	$f19
#define fs0	$f20	/* callee saved */
#define fs0f	$f21
#define fs1	$f22
#define fs1f	$f23
#define fs2	$f24
#define fs2f	$f25
#define fs3	$f26
#define fs3f	$f27
#define fs4	$f28
#define fs4f	$f29
#define fs5	$f30
#define fs5f	$f31
#endif

#if (_MIPS_SIM == _ABI64)
#define fv0	$f0	/* return value */
#define fv1	$f2
#define fa0	$f12	/* argument registers */
#define fa1	$f13
#define fa2	$f14
#define fa3	$f15
#define fa4	$f16
#define fa5	$f17
#define fa6	$f18
#define fa7	$f19
#define ft0	$f4	/* caller saved */
#define ft1	$f5
#define ft2	$f6
#define ft3	$f7
#define ft4	$f8
#define ft5	$f9
#define ft6	$f10
#define ft7	$f11
#define ft8	$f20
#define ft9	$f21
#define ft10	$f22
#define ft11	$f23
#define ft12	$f1
#define ft13	$f3
#define fs0	$f24	/* callee saved */
#define fs1	$f25
#define fs2	$f26
#define fs3	$f27
#define fs4	$f28
#define fs5	$f29
#define fs6	$f30
#define fs7	$f31
#endif

#if (_MIPS_SIM == _ABIN32)
/* return values 		caller saved	*/
#define fv0	$f0	
#define fv1	$f2

/* argument registers		caller saved	*/
#define fa0	$f12	
#define fa1	$f13
#define fa2	$f14
#define fa3	$f15
#define fa4	$f16
#define fa5	$f17
#define fa6	$f18
#define fa7	$f19

/* temp				caller saved	*/
#define ft0	$f4	
#define ft1	$f5
#define ft2	$f6
#define ft3	$f7
#define ft4	$f8
#define ft5	$f9
#define ft6	$f10
#define ft7	$f11

/* temp				caller saved	*/
#define	ft8	$f21	
#define	ft9	$f23
#define	ft10	$f25
#define	ft11	$f27
#define	ft12	$f29
#define	ft13	$f31
#define	ft14	$f1
#define	ft15	$f3

/* temp				callee saved	*/
#define	fs0	$f20	
#define	fs1	$f22
#define	fs2	$f24
#define fs3	$f26
#define	fs4	$f28
#define	fs5	$f30

#endif

#define fcr31	$31

#endif /* __SYS_FPREGDEF_H__ */
