/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: ndrp.h,v $
 * Revision 65.1  1997/10/20 19:17:04  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.310.2  1996/02/18  22:56:32  marty
 * 	Update OSF copyright years
 * 	[1996/02/18  22:15:36  marty]
 *
 * Revision 1.1.310.1  1995/12/08  00:21:05  root
 * 	Submit OSF/DCE 1.2.1
 * 	[1995/12/07  23:59:48  root]
 * 
 * Revision 1.1.308.1  1994/01/21  22:38:14  cbrooks
 * 	RPC Code Cleanyp - Initial Submission
 * 	[1994/01/21  21:58:28  cbrooks]
 * 
 * Revision 1.1.2.3  1993/01/03  23:53:09  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  20:09:26  bbelch]
 * 
 * Revision 1.1.2.2  1992/12/23  21:12:31  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  15:41:11  zeliff]
 * 
 * Revision 1.1  1992/01/19  03:06:39  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */
#ifndef _NDRP_H
#define _NDRP_H	1
/*
**  Copyright (c) 1989 by
**      Hewlett-Packard Company, Palo Alto, Ca. & 
**      Digital Equipment Corporation, Maynard, Mass.
**
**
**  NAME:
**
**      ndrp.h
**
**  FACILITY:
**
**      Network Data Representation (NDR)
**
**  ABSTRACT:
**
**  System (machine-architecture) -dependent definitions.
**
**
*/

/*
 * Data representation descriptor (drep)
 *
 * Note that the form of a drep "on the wire" is not captured by the
 * the "ndr_format_t" data type.  The actual structure -- a "packed drep"
 * -- is a vector of four bytes:
 *
 *      | MSB           LSB |
 *      |<---- 8 bits ----->|
 *      |<-- 4 -->|<-- 4 -->|
 *
 *      +---------+---------+
 *      | int rep | chr rep |
 *      +---------+---------+
 *      |     float rep     |
 *      +-------------------+
 *      |     reserved      |
 *      +-------------------+
 *      |     reserved      |
 *      +-------------------+
 *
 * The following macros manipulate data representation descriptors.
 * "NDR_COPY_DREP" copies one packed drep into another.  "NDR_UNPACK_DREP"
 * copies from a packed drep into a variable of the type "ndr_format_t".
 * 
 */

#ifdef CONVENTIONAL_ALIGNMENT
#  define NDR_COPY_DREP(dst, src) \
    (*((signed32 *) (dst)) = *((signed32 *) (src)))
#else
#  define NDR_COPY_DREP(dst, src) { \
    (dst)[0] = (src)[0]; \
    (dst)[1] = (src)[1]; \
    (dst)[2] = (src)[2]; \
    (dst)[3] = (src)[3]; \
  }
#endif

#define NDR_DREP_INT_REP(drep)   ((drep)[0] >> 4)
#define NDR_DREP_CHAR_REP(drep)  ((drep)[0] & 0xf)
#define NDR_DREP_FLOAT_REP(drep) ((drep)[1])

#define NDR_UNPACK_DREP(dst, src) {             \
    (dst)->int_rep   = NDR_DREP_INT_REP(src);   \
    (dst)->char_rep  = NDR_DREP_CHAR_REP(src);  \
    (dst)->float_rep = NDR_DREP_FLOAT_REP(src); \
    (dst)->reserved  = 0;                   \
}

#endif /* _NDRP_H */
