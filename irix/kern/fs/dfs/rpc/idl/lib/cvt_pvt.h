/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: cvt_pvt.h,v $
 * Revision 65.1  1997/10/20 19:15:31  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.10.2  1996/02/18  23:45:44  marty
 * 	Update OSF copyright years
 * 	[1996/02/18  22:44:33  marty]
 *
 * Revision 1.1.10.1  1995/12/07  22:24:55  root
 * 	Submit OSF/DCE 1.2.1
 * 	[1995/12/07  21:14:02  root]
 * 
 * Revision 1.1.6.1  1994/01/21  22:30:36  cbrooks
 * 	RPC Code Cleanup - Initial Submission
 * 	[1994/01/21  21:51:20  cbrooks]
 * 
 * Revision 1.1.4.2  1993/07/07  20:05:18  ganni
 * 	reduced stub idl sources
 * 	[1993/07/07  19:35:47  ganni]
 * 
 * $EndLog$
 */
/*
**
**  Copyright (c) 1989 by
**      Hewlett-Packard Company, Palo Alto, Ca. &
**  Digital Equipment Corporation, Maynard, Mass.
**  All Rights Reserved.  Unpublished rights reserved
**  under the copyright laws of the United States.
**
**  The software contained on this media is proprietary
**  to and embodies the confidential technology of
**  Digital Equipment Corporation.  Possession, use,
**  duplication or dissemination of the software and
**  media is authorized only pursuant to a valid written
**  license from Digital Equipment Corporation.
**
**  RESTRICTED RIGHTS LEGEND   Use, duplication, or
**  disclosure by the U.S. Government is subject to
**  restrictions as set forth in Subparagraph (c)(1)(ii)
**  of DFARS 252.227-7013, or in FAR 52.227-19, as
**  applicable.
**
**
**  NAME:
**
**      cvt_pvt.h
**
**  FACILITY:
**
**      IDL Stub Runtime Support
**
**  ABSTRACT:
**
**  Private include file for floating point conversion routines.
**
**  VERSION: DCE 1.0
**
*/

#ifndef CVT__PRIVATE
#define CVT__PRIVATE

#include <dce/idlbase.h>
#define C_TAB   	'\t'
#define C_BLANK 	' '
#define C_PLUS  	'+'
#define C_MINUS 	'-'
#define C_ASTERISK 	'*'
#define C_UNDERSCORE 	'_'
#define C_DECIMAL_POINT	'.'
#define C_DOT	 	'.'
#define C_ZERO  	'0'
#define C_ONE   	'1'
#define C_TWO   	'2'
#define C_THREE 	'3'
#define C_FOUR  	'4'
#define C_FIVE  	'5'
#define C_SIX   	'6'
#define C_SEVEN 	'7'
#define C_EIGHT 	'8'
#define C_NINE  	'9'
#define C_A     	'A'
#define C_B     	'B'
#define C_C     	'C'
#define C_D     	'D'
#define C_E     	'E'
#define C_F     	'F'
#define C_L     	'L'
#define C_R     	'R'
#define C_S     	'S'
#define C_T     	'T'
#define C_U     	'U'
#define C_W     	'W'
#define C_a     	'a'
#define C_b     	'b'
#define C_c     	'c'
#define C_d     	'd'
#define C_e     	'e'
#define C_f     	'f'
#define C_t     	't'


/*
UNPACKED REAL:

[0]: excess 2147483648 (2 ^ 31) binary exponent
[1]: mantissa: msb ------>
[2]: -------------------->
[3]: -------------------->
[4]: ----------------> lsb
[5]: 28 unused bits, invalid bit, infinity bit, zero bit, negative bit

All fraction bits are explicit and are normalized s.t. 0.5 <= fraction < 1.0

*/

typedef idl_ulong_int UNPACKED_REAL[6];
typedef UNPACKED_REAL *UNPACKED_REAL_PTR;

#define U_R_EXP 0
#define U_R_FLAGS 5

#define U_R_NEGATIVE 1
#define U_R_ZERO 2
#define U_R_INFINITY 4
#define U_R_INVALID  8
#define U_R_UNUSUAL (U_R_ZERO | U_R_INFINITY | U_R_INVALID)

#if defined(ultrix) && defined(vax)
#define U_R_BIAS 2147483648L
#else
#define U_R_BIAS 2147483648UL
#endif



extern idl_ulong_int vax_c[];

#define VAX_F_INVALID &vax_c[0]
#define VAX_D_INVALID &vax_c[0]
#define VAX_G_INVALID &vax_c[0]
#define VAX_H_INVALID &vax_c[0]

#define VAX_F_ZERO &vax_c[4]
#define VAX_D_ZERO &vax_c[4]
#define VAX_G_ZERO &vax_c[4]
#define VAX_H_ZERO &vax_c[4]

#define VAX_F_POS_HUGE &vax_c[8]
#define VAX_D_POS_HUGE &vax_c[8]
#define VAX_G_POS_HUGE &vax_c[8]
#define VAX_H_POS_HUGE &vax_c[8]

#define VAX_F_NEG_HUGE &vax_c[12]
#define VAX_D_NEG_HUGE &vax_c[12]
#define VAX_G_NEG_HUGE &vax_c[12]
#define VAX_H_NEG_HUGE &vax_c[12]


extern idl_ulong_int ieee_s[];

#define IEEE_S_INVALID ((options & CVT_C_BIG_ENDIAN) ? \
	&ieee_s[1] : &ieee_s[0])
#define IEEE_S_POS_ZERO ((options & CVT_C_BIG_ENDIAN) ? \
	&ieee_s[3] : &ieee_s[2])
#define IEEE_S_NEG_ZERO ((options & CVT_C_BIG_ENDIAN) ? \
	&ieee_s[5] : &ieee_s[4])
#define IEEE_S_POS_HUGE ((options & CVT_C_BIG_ENDIAN) ? \
	&ieee_s[7] : &ieee_s[6])
#define IEEE_S_NEG_HUGE ((options & CVT_C_BIG_ENDIAN) ? \
	&ieee_s[9] : &ieee_s[8])
#define IEEE_S_POS_INFINITY ((options & CVT_C_BIG_ENDIAN) ? \
	&ieee_s[11] : &ieee_s[10])
#define IEEE_S_NEG_INFINITY ((options & CVT_C_BIG_ENDIAN) ? \
	&ieee_s[13] : &ieee_s[12])


extern idl_ulong_int ieee_t[];

#define IEEE_T_INVALID ((options & CVT_C_BIG_ENDIAN) ? \
	&ieee_t[2] : &ieee_t[0])
#define IEEE_T_POS_ZERO ((options & CVT_C_BIG_ENDIAN) ? \
	&ieee_t[6] : &ieee_t[4])
#define IEEE_T_NEG_ZERO ((options & CVT_C_BIG_ENDIAN) ? \
	&ieee_t[10] : &ieee_t[8])
#define IEEE_T_POS_HUGE ((options & CVT_C_BIG_ENDIAN) ? \
	&ieee_t[14] : &ieee_t[12])
#define IEEE_T_NEG_HUGE ((options & CVT_C_BIG_ENDIAN) ? \
	&ieee_t[18] : &ieee_t[16])
#define IEEE_T_POS_INFINITY ((options & CVT_C_BIG_ENDIAN) ? \
	&ieee_t[22] : &ieee_t[20])
#define IEEE_T_NEG_INFINITY ((options & CVT_C_BIG_ENDIAN) ? \
	&ieee_t[26] : &ieee_t[24])


extern idl_ulong_int ibm_s[];

#define IBM_S_INVALID 	&ibm_s[0]
#define IBM_S_POS_ZERO 	&ibm_s[1]
#define IBM_S_NEG_ZERO 	&ibm_s[2]
#define IBM_S_POS_HUGE 	&ibm_s[3]
#define IBM_S_NEG_HUGE 	&ibm_s[4]
#define IBM_S_POS_INFINITY  &ibm_s[5]
#define IBM_S_NEG_INFINITY  &ibm_s[6]


extern idl_ulong_int ibm_l[];

#define IBM_L_INVALID 	&ibm_l[0]
#define IBM_L_POS_ZERO 	&ibm_l[2]
#define IBM_L_NEG_ZERO 	&ibm_l[4]
#define IBM_L_POS_HUGE 	&ibm_l[6]
#define IBM_L_NEG_HUGE 	&ibm_l[8]
#define IBM_L_POS_INFINITY  &ibm_l[10]
#define IBM_L_NEG_INFINITY  &ibm_l[12]


extern idl_ulong_int cray[];

#define CRAY_INVALID	&cray[0]
#define CRAY_POS_ZERO	&cray[2]
#define CRAY_NEG_ZERO	&cray[4]
#define CRAY_POS_HUGE	&cray[6]
#define CRAY_NEG_HUGE	&cray[8]
#define CRAY_POS_INFINITY  &cray[10]
#define CRAY_NEG_INFINITY  &cray[12]


extern idl_ulong_int int_c[];

#define INT_INVALID ((options & CVT_C_BIG_ENDIAN) ? \
	&int_c[1] : &int_c[0])
#define INT_ZERO ((options & CVT_C_BIG_ENDIAN) ? \
	&int_c[3] : &int_c[2])
#define INT_POS_HUGE ((options & CVT_C_BIG_ENDIAN) ? \
	&int_c[5] : &int_c[4])
#define INT_NEG_HUGE ((options & CVT_C_BIG_ENDIAN) ? \
	&int_c[7] : &int_c[6])
#define INT_POS_INFINITY ((options & CVT_C_BIG_ENDIAN) ? \
	&int_c[9] : &int_c[8])
#define INT_NEG_INFINITY ((options & CVT_C_BIG_ENDIAN) ? \
	&int_c[11] : &int_c[10])


#endif
