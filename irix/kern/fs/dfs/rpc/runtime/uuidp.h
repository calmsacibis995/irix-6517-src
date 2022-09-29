/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: uuidp.h,v $
 * Revision 65.2  1998/03/21 19:05:14  lmc
 * Changed all "#ifdef DEBUG" to use "defined(SGIMIPS) &&
 * defined(SGI_RPC_DEBUG)".  This allows RPC debugging to be
 * turned on in the kernel without turning on any kernel
 * debugging.
 *
 * Revision 65.1  1997/10/20  19:17:13  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.324.2  1996/02/18  22:57:01  marty
 * 	Update OSF copyright years
 * 	[1996/02/18  22:16:00  marty]
 *
 * Revision 1.1.324.1  1995/12/08  00:22:29  root
 * 	Submit OSF/DCE 1.2.1
 * 	[1995/12/08  00:00:42  root]
 * 
 * Revision 1.1.322.3  1994/06/24  20:29:17  tom
 * 	Put back prototype for uuid__get_os_address. (OT 11076)
 * 	[1994/06/24  20:28:50  tom]
 * 
 * Revision 1.1.322.1  1994/01/21  22:39:35  cbrooks
 * 	RPC Code Cleanyp - Initial Submission
 * 	[1994/01/21  22:00:29  cbrooks]
 * 
 * Revision 1.1.2.3  1993/01/03  23:56:02  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  20:14:16  bbelch]
 * 
 * Revision 1.1.2.2  1992/12/23  21:18:56  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  15:45:58  zeliff]
 * 
 * Revision 1.1  1992/01/19  03:07:58  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */
#ifndef _UUIDP_H
#define _UUIDP_H	1
/*
**  Copyright (c) 1989 by
**      Hewlett-Packard Company, Palo Alto, Ca. & 
**      Digital Equipment Corporation, Maynard, Mass.
**
**
**  NAME:
**
**      uuidp.h
**
**  FACILITY:
**
**      UUID
**
**  ABSTRACT:
**
**      Interface Definitions for UUID internal subroutines used by
**      the uuid module
**
**
*/

#ifndef _DCE_PROTOTYPE_
#include <dce/dce.h>
#endif

/*
 * Max size of a uuid string: tttttttt-tttt-cccc-cccc-nnnnnnnnnnnn
 * Note: this includes the implied '\0'
 */
#define UUID_C_UUID_STRING_MAX          37

/*
 * defines for time calculations
 */
#ifndef UUID_C_100NS_PER_SEC
#define UUID_C_100NS_PER_SEC            10000000
#endif

#ifndef UUID_C_100NS_PER_USEC
#define UUID_C_100NS_PER_USEC           10
#endif



/*
 * UADD_UVLW_2_UVLW - macro to add two unsigned 64-bit long integers
 *                      (ie. add two unsigned 'very' long words)
 *
 * Important note: It is important that this macro accommodate (and it does)
 *                 invocations where one of the addends is also the sum.
 *
 * This macro was snarfed from the DTSS group and was originally:
 *
 * UTCadd - macro to add two UTC times
 *
 * add lo and high order longword separately, using sign bits of the low-order
 * longwords to determine carry.  sign bits are tested before addition in two
 * cases - where sign bits match. when the addend sign bits differ the sign of
 * the result is also tested:
 *
 *        sign            sign
 *      addend 1        addend 2        carry?
 *
 *          1               1            TRUE
 *          1               0            TRUE if sign of sum clear
 *          0               1            TRUE if sign of sum clear
 *          0               0            FALSE
 */
#define UADD_UVLW_2_UVLW(add1, add2, sum)                               \
    if (!(((add1)->lo&0x80000000UL) ^ ((add2)->lo&0x80000000UL)))           \
    {                                                                   \
        if (((add1)->lo&0x80000000UL))                                    \
        {                                                               \
            (sum)->lo = (add1)->lo + (add2)->lo ;                       \
            (sum)->hi = (add1)->hi + (add2)->hi+1 ;                     \
        }                                                               \
        else                                                            \
        {                                                               \
            (sum)->lo  = (add1)->lo + (add2)->lo ;                      \
            (sum)->hi = (add1)->hi + (add2)->hi ;                       \
        }                                                               \
    }                                                                   \
    else                                                                \
    {                                                                   \
        (sum)->lo = (add1)->lo + (add2)->lo ;                           \
        (sum)->hi = (add1)->hi + (add2)->hi ;                           \
        if (!((sum)->lo&0x80000000UL))                                    \
            (sum)->hi++ ;                                               \
    }


/*
 * UADD_ULW_2_UVLW - macro to add a 32-bit unsigned integer to
 *                   a 64-bit unsigned integer
 *
 * Note: see the UADD_UVLW_2_UVLW() macro
 *
 */
#define UADD_ULW_2_UVLW(add1, add2, sum)                                \
{                                                                       \
    (sum)->hi = (add2)->hi;                                             \
    if ((*add1) & (add2)->lo & 0x80000000UL)                              \
    {                                                                   \
        (sum)->lo = (*add1) + (add2)->lo;                               \
        (sum)->hi++;                                                    \
    }                                                                   \
    else                                                                \
    {                                                                   \
        (sum)->lo = (*add1) + (add2)->lo;                               \
        if (!((sum)->lo & 0x80000000UL))                                  \
        {                                                               \
            (sum)->hi++;                                                \
        }                                                               \
    }                                                                   \
}


/*
 * UADD_UW_2_UVLW - macro to add a 16-bit unsigned integer to
 *                   a 64-bit unsigned integer
 *
 * Note: see the UADD_UVLW_2_UVLW() macro
 *
 */
#define UADD_UW_2_UVLW(add1, add2, sum)                                 \
{                                                                       \
    (sum)->hi = (add2)->hi;                                             \
    if ((add2)->lo & 0x80000000UL)                                        \
    {                                                                   \
        (sum)->lo = (*add1) + (add2)->lo;                               \
        if (!((sum)->lo & 0x80000000UL))                                  \
        {                                                               \
            (sum)->hi++;                                                \
        }                                                               \
    }                                                                   \
    else                                                                \
    {                                                                   \
        (sum)->lo = (*add1) + (add2)->lo;                               \
    }                                                                   \
}



/*
 * a macro to set *status uuid_s_coding_error
 */
#ifdef  CODING_ERROR
#undef  CODING_ERROR
#endif

#if defined(DEBUG) || (defined(SGIMIPS) && defined(SGI_RPC_DEBUG))
#define CODING_ERROR(status)        *(status) = uuid_s_coding_error
#else
#define CODING_ERROR(status)
#endif


typedef struct
{
    char eaddr[6];      /* 6 bytes of ethernet hardware address */
} uuid_address_t, *uuid_address_p_t;


typedef struct
{
    unsigned32  lo;
    unsigned32  hi;
} uuid_time_t, *uuid_time_p_t;


typedef struct
{
    unsigned32  lo;
    unsigned32  hi;
} unsigned64_t, *unsigned64_p_t;


/*
 * U U I D _ _ U E M U L
 *
 * 32-bit unsigned * 32-bit unsigned multiply -> 64-bit result
 */
PRIVATE void uuid__uemul _DCE_PROTOTYPE_ ((
        unsigned32           /*u*/,
        unsigned32           /*v*/,
        unsigned64_t        * /*prodPtr*/
    ));

/*
 * U U I D _ _ R E A D _ C L O C K
 */
PRIVATE unsigned16 uuid__read_clock _DCE_PROTOTYPE_ (( void ));


/*
 * U U I D _ _ W R I T E _ C L O C K
 */
PRIVATE void uuid__write_clock _DCE_PROTOTYPE_ (( unsigned16 /*time*/ ));


/*
 * U U I D _ _ G E T _ O S _ P I D
 *
 * Get the process id
 */
PRIVATE unsigned32 uuid__get_os_pid _DCE_PROTOTYPE_ (( void ));


/*
 * U U I D _ _ G E T _ O S _ T I M E
 *
 * Get OS time
 */
PRIVATE void uuid__get_os_time _DCE_PROTOTYPE_(( uuid_time_t * /*uuid_time*/));


/*
 * U U I D _ _ G E T _ O S _ A D D R E S S
 *
 * Get ethernet hardware address from the OS
 */
PRIVATE void uuid__get_os_address _DCE_PROTOTYPE_ ((
        uuid_address_t          * /*address*/,
        unsigned32              * /*st*/
    ));

#endif /* _UUIDP_H */
