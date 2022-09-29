/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: cvt.h,v $
 * Revision 65.1  1997/10/20 19:15:31  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.10.2  1996/02/18  23:45:43  marty
 * 	Update OSF copyright years
 * 	[1996/02/18  22:44:33  marty]
 *
 * Revision 1.1.10.1  1995/12/07  22:24:40  root
 * 	Submit OSF/DCE 1.2.1
 * 	[1995/12/07  21:13:57  root]
 * 
 * Revision 1.1.6.1  1994/01/21  22:30:35  cbrooks
 * 	RPC Code Cleanup - Initial Submission
 * 	[1994/01/21  21:51:19  cbrooks]
 * 
 * Revision 1.1.4.2  1993/07/07  20:05:05  ganni
 * 	reduced stub idl sources
 * 	[1993/07/07  19:35:40  ganni]
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
**      cvt.h
**
**  FACILITY:
**
**      IDL Stub Runtime Support
**
**  ABSTRACT:
**
**  Header file for floating point conversion routines.
**
**  VERSION: DCE 1.0
**
*/

#ifndef CVT
#define CVT 1

/*
 *
 *    Type Definitions
 *
 */

typedef unsigned char CVT_BYTE;
typedef CVT_BYTE *CVT_BYTE_PTR;

typedef CVT_BYTE CVT_VAX_F[4];
typedef CVT_BYTE CVT_VAX_D[8];
typedef CVT_BYTE CVT_VAX_G[8];
typedef CVT_BYTE CVT_VAX_H[16];
typedef CVT_BYTE CVT_IEEE_SINGLE[4];
typedef CVT_BYTE CVT_IEEE_DOUBLE[8];
typedef CVT_BYTE CVT_IBM_SHORT[4];
typedef CVT_BYTE CVT_IBM_LONG[8];
typedef CVT_BYTE CVT_CRAY[8];
typedef float    CVT_SINGLE;
typedef double   CVT_DOUBLE;
typedef long     CVT_SIGNED_INT;
typedef unsigned long CVT_UNSIGNED_INT;
typedef unsigned long CVT_STATUS;

/*
 *
 *    Constant Definitions
 *
 */

#define CVT_C_ROUND_TO_NEAREST		     1
#define CVT_C_TRUNCATE			     2
#define CVT_C_ROUND_TO_POS		     4
#define CVT_C_ROUND_TO_NEG		     8
#define CVT_C_VAX_ROUNDING                  16
#define CVT_C_BIG_ENDIAN		    32
#define CVT_C_ERR_UNDERFLOW		    64
#define CVT_C_ZERO_BLANKS		   128
#define CVT_C_SKIP_BLANKS		   256
#define CVT_C_SKIP_UNDERSCORES		   512
#define CVT_C_SKIP_UNDERSCORE		   512
#define CVT_C_SKIP_TABS			  1024
#define CVT_C_ONLY_E                      2048
#define CVT_C_EXP_LETTER_REQUIRED	  4096
#define CVT_C_FORCE_SCALE		  8192
#define CVT_C_EXPONENTIAL_FORMAT	 16384
#define CVT_C_FORCE_PLUS		 32768
#define CVT_C_FORCE_EXPONENT_SIGN	 65536
#define CVT_C_SUPPRESS_TRAILING_ZEROES	131072
#define CVT_C_FORCE_EXPONENTIAL_FORMAT	262144
#define CVT_C_FORCE_FRACTIONAL_FORMAT	524288
#define CVT_C_EXPONENT_D		1048576
#define CVT_C_EXPONENT_E		2097152
#define CVT_C_SEMANTICS_FORTRAN		4194304
#define CVT_C_SEMANTICS_PASCAL		8388608


#define cvt__normal 			1
#define cvt__invalid_character 		2
#define cvt__invalid_option 		3
#define cvt__invalid_radix 		4
#define cvt__invalid_size 		5
#define cvt__invalid_value 		6
#define cvt__neg_infinity 		7
#define cvt__output_conversion_error 	8
#define cvt__overflow 			9
#define cvt__pos_infinity 		10
#define cvt__underflow 			11
#define cvt__input_conversion_error 	12

#define cvt_s_normal 			cvt__normal
#define cvt_s_invalid_character 	cvt__invalid_character
#define cvt_s_invalid_option 		cvt__invalid_option
#define cvt_s_invalid_radix 		cvt__invalid_radix
#define cvt_s_invalid_size 		cvt__invalid_size
#define cvt_s_invalid_value 		cvt__invalid_value
#define cvt_s_neg_infinity 		cvt__neg_infinity
#define cvt_s_input_conversion_error 	cvt__input_conversion_error
#define cvt_s_output_conversion_error 	cvt__output_conversion_error
#define cvt_s_overflow 			cvt__overflow
#define cvt_s_pos_infinity 		cvt__pos_infinity
#define cvt_s_underflow 		cvt__underflow

#define CVT_C_BIN  2
#define CVT_C_OCT  8
#define CVT_C_DEC 10
#define CVT_C_HEX 16

#endif
