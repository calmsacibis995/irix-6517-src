/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(_KERNEL)
static const char *_identString = "$Id: rpcuuid.c,v 65.5 1998/03/23 17:28:39 gwehrman Exp $";
static void _identFunc(const char *x) {if(!x)_identFunc(_identString);}
#endif



/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: rpcuuid.c,v $
 * Revision 65.5  1998/03/23 17:28:39  gwehrman
 * Source Identification changed to bracket with SGIMIPS && !_KERNEL.
 *
 * Revision 65.4  1998/01/07 17:21:12  jdoak
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS
 *
 * Revision 65.3  1997/11/06  19:57:13  jdoak
 * Source Identification added.
 *
 * Revision 65.2  1997/10/20  19:17:13  jdoak
 * Initial IRIX 6.4 code merge
 *
 * Revision 1.1.621.2  1996/02/18  00:05:54  marty
 * 	Update OSF copyright years
 * 	[1996/02/17  22:57:32  marty]
 *
 * Revision 1.1.621.1  1995/12/08  00:22:27  root
 * 	Submit OSF/DCE 1.2.1
 * 
 * 	HP revision /main/HPDCE02/jrr_1.2_mothra/1  1995/11/16  21:35 UTC  jrr
 * 	Remove Mothra specific code
 * 
 * 	HP revision /main/HPDCE02/2  1994/09/21  14:50 UTC  tatsu_s
 * 	Merged mothra up to dce1_1_b18.
 * 	[1995/12/08  00:00:40  root]
 * 
 * Revision 1.1.619.7  1994/08/16  20:23:14  tom
 * 	Bug 10052 - never print a uuid in the old string format.
 * 	[1994/08/16  20:22:49  tom]
 * 
 * 	HP revision /main/HPDCE02/1  1994/08/03  16:37 UTC  tatsu_s
 * 	Merged mothra up to dce 1.1.
 * 
 * 	HP revision /main/HPDCE01/2  1994/07/19  20:23 UTC  mk
 * 	merge in fixes from mk_mothra_repair branch
 * 
 * 	HP revision /main/HPDCE01/mk_mothra_repair/1  1994/07/19  20:21 UTC  mk
 * 	conditionalize references to uuid__create_standalone out of krpc build
 * 
 * 	HP revision /main/HPDCE01/1  1994/06/14  22:14 UTC  mk
 * 	merge in mk_sd_uuids branch
 * 
 * 	HP revision /main/mk_sd_uuids/1  1994/05/27  16:23 UTC  mk
 * 	standalone uuid generation support for sd
 * 
 * Revision 1.1.619.6  1994/06/24  20:29:15  tom
 * 	Bug 11076 - call uuid__get_os_address since krpc has its own version.
 * 	[1994/06/24  20:28:46  tom]
 * 
 * Revision 1.1.619.5  1994/06/23  17:40:19  tom
 * 	Call the new dce_get_802_addr function instead of uuid__get_os_address.
 * 	[1994/06/22  20:40:37  tom]
 * 
 * Revision 1.1.619.4  1994/06/21  21:54:19  hopkins
 * 	More serviceability
 * 	[1994/06/08  21:34:27  hopkins]
 * 
 * Revision 1.1.619.3  1994/01/26  22:12:52  tom
 * 	Fix typo in uuid_equal.
 * 	[1994/01/26  21:52:24  tom]
 * 
 * Revision 1.1.619.2  1994/01/24  18:34:23  cbrooks
 * 	Change external definition of UUID_NIL -> uuid_nil
 * 	[1994/01/24  18:31:01  cbrooks]
 * 
 * Revision 1.1.619.1  1994/01/21  22:39:28  cbrooks
 * 	RPC Code Cleanyp - Initial Submission
 * 	[1994/01/21  22:00:25  cbrooks]
 * 
 * Revision 1.1.5.4  1993/01/12  15:29:09  markar
 * 	    OT CR 2153 fix: stop converting UUID strings to upper case.
 * 	[1992/11/13  19:21:41  markar]
 * 
 * Revision 1.1.5.3  1993/01/03  23:55:49  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  20:13:53  bbelch]
 * 
 * Revision 1.1.5.2  1992/12/23  21:18:22  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  15:45:35  zeliff]
 * 
 * Revision 1.1.2.3  1992/07/07  21:28:43  rsalz
 * 	NULL or "" in string to UID gets the null UID.
 * 	[1992/07/07  21:25:54  rsalz]
 * 
 * Revision 1.1.2.2  1992/05/01  16:05:44  rsalz
 * 	 22-apr-92 nm    Fix comments about variants (OT 2906).
 * 	[1992/05/01  00:19:01  rsalz]
 * 
 * Revision 1.1  1992/01/19  03:11:50  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */
/*
**  Copyright (c) 1989 by
**      Hewlett-Packard Company, Palo Alto, Ca. &
**      Digital Equipment Corporation, Maynard, Mass.
**
**
**  NAME:
**
**      uuid.c
**
**  FACILITY:
**
**      UUID
**
**  ABSTRACT:
**
**      UUID - routines that manipulate uuid's
**
**
*/

#if defined(SGIMIPS) && !defined(KERNEL)
#include <stdio.h>
#endif 
#include <commonp.h>            /* common definitions                   */
#include <dce/uuid.h>           /* uuid idl definitions (public)        */
#include <uuidp.h>              /* uuid idl definitions (private)       */


/*
 * Internal structure of universal unique IDs (UUIDs).
 *
 * There are three "variants" of UUIDs that this code knows about.  The
 * variant #0 is what was defined in the 1989 HP/Apollo Network Computing
 * Architecture (NCA) specification and implemented in NCS 1.x and DECrpc
 * v1.  Variant #1 is what was defined for the joint HP/DEC specification
 * for the OSF (in DEC's "UID Architecture Functional Specification Version
 * X1.0.4") and implemented in NCS 2.0, DECrpc v2, and OSF 1.0 DCE RPC.
 * Variant #2 is defined by Microsoft.
 *
 * This code creates only variant #1 UUIDs.
 * 
 * The three UUID variants can exist on the same wire because they have
 * distinct values in the 3 MSB bits of octet 8 (see table below).  Do
 * NOT confuse the version number with these 3 bits.  (Note the distinct
 * use of the terms "version" and "variant".) Variant #0 had no version
 * field in it.  Changes to variant #1 (should any ever need to be made)
 * can be accomodated using the current form's 4 bit version field.
 * 
 * The UUID record structure MUST NOT contain padding between fields.
 * The total size = 128 bits.
 *
 * To minimize confusion about bit assignment within octets, the UUID
 * record definition is defined only in terms of fields that are integral
 * numbers of octets.
 *
 * Depending on the network data representation, the multi-octet unsigned
 * integer fields are subject to byte swapping when communicated between
 * dissimilar endian machines.  Note that all three UUID variants have
 * the same record structure; this allows this byte swapping to occur.
 * (The ways in which the contents of the fields are generated can and
 * do vary.)
 *
 * The following information applies to variant #1 UUIDs:
 *
 * The lowest addressed octet contains the global/local bit and the
 * unicast/multicast bit, and is the first octet of the address transmitted
 * on an 802.3 LAN.
 *
 * The adjusted time stamp is split into three fields, and the clockSeq
 * is split into two fields.
 *
 * |<------------------------- 32 bits -------------------------->|
 *
 * +--------------------------------------------------------------+
 * |                     low 32 bits of time                      |  0-3  .time_low
 * +-------------------------------+-------------------------------
 * |     mid 16 bits of time       |  4-5               .time_mid
 * +-------+-----------------------+
 * | vers. |   hi 12 bits of time  |  6-7               .time_hi_and_version
 * +-------+-------+---------------+
 * |Res|  clkSeqHi |  8                                 .clock_seq_hi_and_reserved
 * +---------------+
 * |   clkSeqLow   |  9                                 .clock_seq_low
 * +---------------+----------...-----+
 * |            node ID               |  8-16           .node
 * +--------------------------...-----+
 *
 * --------------------------------------------------------------------------
 *
 * The structure layout of all three UUID variants is fixed for all time.
 * I.e., the layout consists of a 32 bit int, 2 16 bit ints, and 8 8
 * bit ints.  The current form version field does NOT determine/affect
 * the layout.  This enables us to do certain operations safely on the
 * variants of UUIDs without regard to variant; this increases the utility
 * of this code even as the version number changes (i.e., this code does
 * NOT need to check the version field).
 *
 * The "Res" field in the octet #8 is the so-called "reserved" bit-field
 * and determines whether or not the uuid is a old, current or other
 * UUID as follows:
 *
 *      MS-bit  2MS-bit  3MS-bit      Variant
 *      ---------------------------------------------
 *         0       x        x       0 (NCS 1.5)
 *         1       0        x       1 (DCE 1.0 RPC)
 *         1       1        0       2 (Microsoft)
 *         1       1        1       unspecified
 *
 * --------------------------------------------------------------------------
 *
 * Internal structure of variant #0 UUIDs
 *
 * The first 6 octets are the number of 4 usec units of time that have
 * passed since 1/1/80 0000 GMT.  The next 2 octets are reserved for
 * future use.  The next octet is an address family.  The next 7 octets
 * are a host ID in the form allowed by the specified address family.
 *
 * Note that while the family field (octet 8) was originally conceived
 * of as being able to hold values in the range [0..255], only [0..13]
 * were ever used.  Thus, the 2 MSB of this field are always 0 and are
 * used to distinguish old and current UUID forms.
 *
 * +--------------------------------------------------------------+
 * |                    high 32 bits of time                      |  0-3  .time_high
 * +-------------------------------+-------------------------------
 * |     low 16 bits of time       |  4-5               .time_low
 * +-------+-----------------------+
 * |         reserved              |  6-7               .reserved
 * +---------------+---------------+
 * |    family     |   8                                .family
 * +---------------+----------...-----+
 * |            node ID               |  9-16           .node
 * +--------------------------...-----+
 *
 */

/***************************************************************************
 *
 * Local definitions
 *
 **************************************************************************/

#ifdef  UUID_DEBUG
#define DEBUG_PRINT(msg, st)    RPC_DBG_GPRINTF (( "%s: %08x\n", msg, st ))
#else
#define DEBUG_PRINT             { /* hello sailor */ }
#endif

#ifndef NO_SSCANF
#  define UUID_SSCANF          sscanf
#  define UUID_OLD_SSCANF      sscanf
#else
#  define UUID_SSCANF          uuid__sscanf
#  define UUID_OLD_SSCANF      uuid__old_sscanf
#endif

#ifndef NO_SPRINTF
#  define UUID_SPRINTF         sprintf
#  define UUID_OLD_SPRINTF     sprintf
#else
#  define UUID_SPRINTF         uuid__sprintf
#  define UUID_OLD_SPRINTF     uuid__old_sprintf
#endif

/*
 * the number of elements returned by sscanf() when converting
 * string formatted uuid's to binary
 */
#define UUID_ELEMENTS_NUM       11
#define UUID_ELEMENTS_NUM_OLD   10

/*
 * local defines used in uuid bit-diddling
 */
#define HI_WORD(w)                  ((w) >> 16)
#define RAND_MASK                   0x3fff      /* same as CLOCK_SEQ_LAST */

#define TIME_MID_MASK               0x0000ffff
#define TIME_HIGH_MASK              0x0fff0000
#define TIME_HIGH_SHIFT_COUNT       16

#define MAX_TIME_ADJUST             0x7fff

#define CLOCK_SEQ_LOW_MASK          0xff
#define CLOCK_SEQ_HIGH_MASK         0x3f00
#define CLOCK_SEQ_HIGH_SHIFT_COUNT  8
#define CLOCK_SEQ_FIRST             1
#define CLOCK_SEQ_LAST              0x3fff      /* same as RAND_MASK */

/*
 * Note: If CLOCK_SEQ_BIT_BANG == TRUE, then we can avoid the modulo
 * operation.  This should save us a divide instruction and speed
 * things up.
 */

#ifndef CLOCK_SEQ_BIT_BANG
#define CLOCK_SEQ_BIT_BANG          1
#endif

#if CLOCK_SEQ_BIT_BANG
#define CLOCK_SEQ_BUMP(seq)         ((*seq) = ((*seq) + 1) & CLOCK_SEQ_LAST)
#else
#define CLOCK_SEQ_BUMP(seq)         ((*seq) = ((*seq) + 1) % (CLOCK_SEQ_LAST+1))
#endif

#define UUID_VERSION_BITS           (uuid_c_version << 12)
#define UUID_RESERVED_BITS          0x80

#define IS_OLD_UUID(uuid) (((uuid)->clock_seq_hi_and_reserved & 0xc0) != 0x80)



/****************************************************************************
 *
 * global data declarations
 *
 ****************************************************************************/

GLOBAL uuid_t uuid_g_nil_uuid = { 0 };
GLOBAL uuid_t uuid_nil = { 0 };

/****************************************************************************
 *
 * local data declarations
 *
 ****************************************************************************/
#ifndef	SGIMIPS

/*
 * saved copy of our IEEE 802 address for quick reference
 */
INTERNAL uuid_address_t  saved_addr;

/*
 * saved copy of the status associated with saved_addr
 */
INTERNAL unsigned32     saved_st;

/*
 * declarations used in UTC time calculations
 */
INTERNAL uuid_time_t      time_now;     /* utc time as of last query        */
INTERNAL uuid_time_t      time_last;    /* 'saved' value of time_now        */
INTERNAL unsigned16       time_adjust;  /* 'adjustment' to ensure uniqness  */
INTERNAL unsigned16       clock_seq;    /* 'adjustment' for backwards clocks*/

/*
 * true_random variables
 */
INTERNAL unsigned32     rand_m;         /* multiplier                       */
INTERNAL unsigned32     rand_ia;        /* adder #1                         */
INTERNAL unsigned32     rand_ib;        /* adder #2                         */
INTERNAL unsigned32     rand_irand;     /* random value                     */

typedef enum
{
    uuid_e_less_than, uuid_e_equal_to, uuid_e_greater_than
} uuid_compval_t;

/*
 * boolean indicating we've already determined our IEEE 802 address
 */
INTERNAL boolean got_address = FALSE;

/****************************************************************************
 *
 * local function declarations
 *
 ****************************************************************************/

/*
 * I N I T
 *
 * Startup initialization routine for UUID module.
 */

INTERNAL void init _DCE_PROTOTYPE_ (( unsigned32 * /*st*/ ));

/*
 * T R U E _ R A N D O M _ I N I T
 */

INTERNAL void true_random_init _DCE_PROTOTYPE_ ((void));

/*
 * T R U E _ R A N D O M
 */
INTERNAL unsigned16 true_random _DCE_PROTOTYPE_ ((void));


/*
 * N E W _ C L O C K _ S E Q
 *
 * Ensure clock_seq is up-to-date
 *
 * Note: clock_seq is architected to be 14-bits (unsigned) but
 *       I've put it in here as 16-bits since there isn't a
 *       14-bit unsigned integer type (yet)
 */ 
INTERNAL void new_clock_seq _DCE_PROTOTYPE_(( unsigned16 * /*clock_seq*/));

/*
 * S T R U C T U R E _ I S _ K N O W N
 *
 * Does the UUID have the known standard structure layout?
 */
boolean structure_is_known _DCE_PROTOTYPE_(( uuid_p_t /*uuid*/));

/*
 * T I M E _ C M P
 *
 * Compares two UUID times (64-bit DEC UID UTC values)
 */
INTERNAL uuid_compval_t time_cmp _DCE_PROTOTYPE_ ((
        uuid_time_p_t        /*time1*/,
        uuid_time_p_t        /*time2*/
    ));

/*
 * U U I D _ G E T _ A D D R E S S
 *
 * Get our IEEE 802 address (calls uuid__get_os_address)
 */

PUBLIC void uuid_get_address _DCE_PROTOTYPE_ ((
        uuid_address_t      * /*address*/,
        unsigned32          * /*st*/
    ));



/*****************************************************************************
 *
 *  Macro definitions
 *
 ****************************************************************************/

#endif

#ifndef	SGIMIPS
/*
 * ensure we've been initialized
 */
INTERNAL boolean uuid_init_done = FALSE;

#define EmptyArg
#define UUID_VERIFY_INIT(Arg)          \
    if (! uuid_init_done)           \
    {                               \
        init (status);              \
        if (*status != uuid_s_ok)   \
        {                           \
            return Arg;                 \
        }                           \
    }
#else
#define EmptyArg
#define UUID_VERIFY_INIT(Arg)
#endif

/*
 * Check the reserved bits to make sure the UUID is of the known structure.
 */

#define CHECK_STRUCTURE(uuid) \
( \
    (((uuid)->clock_seq_hi_and_reserved & 0x80) == 0x00) || /* var #0 */ \
    (((uuid)->clock_seq_hi_and_reserved & 0xc0) == 0x80) || /* var #1 */ \
    (((uuid)->clock_seq_hi_and_reserved & 0xe0) == 0xc0)    /* var #2 */ \
)

/*
 * The following macros invoke CHECK_STRUCTURE(), check that the return
 * value is okay and if not, they set the status variable appropriately
 * and return either a boolean FALSE, nothing (for void procedures),
 * or a value passed to the macro.  This has been done so that checking
 * can be done more simply and values are returned where appropriate
 * to keep compilers happy.
 *
 * bCHECK_STRUCTURE - returns boolean FALSE
 * vCHECK_STRUCTURE - returns nothing (void)
 * rCHECK_STRUCTURE - returns 'r' macro parameter
 */

#define bCHECK_STRUCTURE(uuid, status) \
{ \
    if (!CHECK_STRUCTURE (uuid)) \
    { \
        *(status) = uuid_s_bad_version; \
        return (FALSE); \
    } \
}

#define vCHECK_STRUCTURE(uuid, status) \
{ \
    if (!CHECK_STRUCTURE (uuid)) \
    { \
        *(status) = uuid_s_bad_version; \
        return; \
    } \
}

#define rCHECK_STRUCTURE(uuid, status, result) \
{ \
    if (!CHECK_STRUCTURE (uuid)) \
    { \
        *(status) = uuid_s_bad_version; \
        return (result); \
    } \
}

#ifndef	SGIMIPS

/*
**++
**
**  ROUTINE NAME:       init
**
**  SCOPE:              INTERNAL - declared locally
**
**  DESCRIPTION:
**
**  Startup initialization routine for the UUID module.
**
**  INPUTS:             none
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      status          return status value
**
**          uuid_s_ok
**          uuid_s_coding_error
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:     void
**
**  SIDE EFFECTS:       sets uuid_init_done so this won't be done again
**
**--
**/

INTERNAL void init 
#ifdef _DCE_PROTO_
(
    unsigned32              *status
)
#else
(status)
unsigned32              *status;
#endif
{
#ifdef CMA_INCLUDE
    /*
     * Hack for CMA pthreads.  Some users will call uuid_ stuff before
     * doing any thread stuff (CMA is uninitialized).  Some uuid_
     * operations alloc memory and in a CMA pthread environment,
     * the malloc wrapper uses a mutex but doesn't do self initialization
     * (which results in segfault'ing inside of CMA).  Make sure that
     * CMA is initialized.
     */
    pthread_t   t;
    t = pthread_self();
#endif /* CMA_INCLUDE */

    CODING_ERROR (status);

    /*
     * init the random number generator
     */
    true_random_init();

    uuid__get_os_time (&time_last);

#ifdef UUID_NONVOLATILE_CLOCK
    clock_seq = uuid__read_clock();
#else
    clock_seq = true_random();
#endif

    uuid_init_done = TRUE;

    *status = uuid_s_ok;
}

/*
**++
**
**  ROUTINE NAME:       uuid_create
**
**  SCOPE:              PUBLIC - declared in UUID.IDL
**
**  DESCRIPTION:
**
**  Create a new UUID. Note: we only know how to create the new
**  and improved UUIDs.
**
**  INPUTS:             none
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      uuid            A new UUID value
**
**      status          return status value
**
**          uuid_s_ok
**          uuid_s_coding_error
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:     void
**
**  SIDE EFFECTS:       none
**
**--
**/

PUBLIC void uuid_create 
#ifdef _DCE_PROTO_
(
    uuid_t                  *uuid,
    unsigned32              *status
)
#else
(uuid, status)
uuid_t                  *uuid;
unsigned32              *status;
#endif
{
    uuid_address_t          eaddr;      /* our IEEE 802 hardware address */
    boolean32               got_no_time = FALSE;

    RPC_LOG_UUID_CREATE_NTR;

    CODING_ERROR (status);
    UUID_VERIFY_INIT (EmptyArg);

    /*
     * get our hardware network address
     */
    uuid_get_address (&eaddr, status);
    if (*status != uuid_s_ok)
    {
        DEBUG_PRINT("uuid_create:uuid_get_address", *status);
        return;
    }

    do
    {
        /*
         * get the current time
         */
        uuid__get_os_time (&time_now);

        /*
         * do stuff like:
         *
         *  o check that our clock hasn't gone backwards and handle it
         *    accordingly with clock_seq
         *  o check that we're not generating uuid's faster than we
         *    can accommodate with our time_adjust fudge factor
         */
        switch (time_cmp (&time_now, &time_last))
        {
            case uuid_e_less_than:
                new_clock_seq (&clock_seq);
                time_adjust = 0;
                break;
            case uuid_e_greater_than:
                time_adjust = 0;
                break;
            case uuid_e_equal_to:
                if (time_adjust == MAX_TIME_ADJUST)
                {
                    /*
                     * spin your wheels while we wait for the clock to tick
                     */
                    got_no_time = TRUE;
                }
                else
                {
                    time_adjust++;
                }
                break;
            default:
                *status = uuid_s_internal_error;
                DEBUG_PRINT ("uuid_create", *status);
                return;
        }
    } while (got_no_time);

    time_last.lo = time_now.lo;
    time_last.hi = time_now.hi;

    if (time_adjust != 0)
    {
        UADD_UW_2_UVLW (&time_adjust, &time_now, &time_now);
    }

    /*
     * now construct a uuid with the information we've gathered
     * plus a few constants
     */
    uuid->time_low = time_now.lo;
    uuid->time_mid = time_now.hi & TIME_MID_MASK;

    uuid->time_hi_and_version =
        (time_now.hi & TIME_HIGH_MASK) >> TIME_HIGH_SHIFT_COUNT;
    uuid->time_hi_and_version |= UUID_VERSION_BITS;

    uuid->clock_seq_low = clock_seq & CLOCK_SEQ_LOW_MASK;
    uuid->clock_seq_hi_and_reserved =
        (clock_seq & CLOCK_SEQ_HIGH_MASK) >> CLOCK_SEQ_HIGH_SHIFT_COUNT;

    uuid->clock_seq_hi_and_reserved |= UUID_RESERVED_BITS;

    memcpy (uuid->node, &eaddr, sizeof (uuid_address_t));

    *status = uuid_s_ok;
    RPC_LOG_UUID_CREATE_XIT;
}

/*
**++
**
**  ROUTINE NAME:       uuid_create_nil
**
**  SCOPE:              PUBLIC - declared in UUID.IDL
**
**  DESCRIPTION:
**
**  Create a 'nil' uuid.
**
**  INPUTS:             none
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      uuid            A nil UUID
**
**      status          return status value
**
**          uuid_s_ok
**          uuid_s_coding_error
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:     void
**
**  SIDE EFFECTS:       none
**
**--
**/

PUBLIC void uuid_create_nil 
#ifdef _DCE_PROTO_
(
    uuid_t              *uuid,
    unsigned32          *status
)
#else
(uuid, status)
uuid_t              *uuid;
unsigned32          *status;
#endif
{
    CODING_ERROR (status);
    UUID_VERIFY_INIT (EmptyArg);
    memset (uuid, 0, sizeof (uuid_t));

    *status = uuid_s_ok;
}

/*
**++
**
**  ROUTINE NAME:       uuid_to_string
**
**  SCOPE:              PUBLIC - declared in UUID.IDL
**
**  DESCRIPTION:
**
**  Encode a UUID into a printable string.
**
**  INPUTS:
**
**      uuid            A binary UUID to be converted to a string UUID.
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      uuid_string     The string representation of the given UUID.
**
**      status          return status value
**
**          uuid_s_ok
**          uuid_s_bad_version
**          uuid_s_coding_error
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:     void
**
**  SIDE EFFECTS:       none
**
**--
**/
#endif

PUBLIC void uuid_to_string 
#ifdef _DCE_PROTO_
(
    uuid_p_t                uuid,
    unsigned_char_p_t       *uuid_string,
    unsigned32              *status
)
#else
(uuid, uuid_string, status)
uuid_p_t                uuid;
unsigned_char_p_t       *uuid_string;
unsigned32              *status;
#endif
{
#ifdef KERNEL
    extern void uuid__sprintf(char *, char *, unsigned32, unsigned32, 
		unsigned32, unsigned32, unsigned32, unsigned32, unsigned32, 
		unsigned32, unsigned32, unsigned32, unsigned32);

#endif /* KERNEL */
    CODING_ERROR (status);
    UUID_VERIFY_INIT (EmptyArg);

    /*
     * don't do anything if the output argument is NULL
     */
    if (uuid_string == NULL)
    {
        *status = uuid_s_ok;
        return;
    }

    vCHECK_STRUCTURE (uuid, status);

    RPC_MEM_ALLOC (
        *uuid_string,
        unsigned_char_p_t,
        UUID_C_UUID_STRING_MAX,
        RPC_C_MEM_STRING,
        RPC_C_MEM_WAITOK);

    if (*uuid_string == NULL)
    {
        *status = uuid_s_no_memory;
        return;
    }

    UUID_SPRINTF(
        (char *) *uuid_string,
        "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        uuid->time_low, uuid->time_mid, uuid->time_hi_and_version,
        uuid->clock_seq_hi_and_reserved, uuid->clock_seq_low,
        (unsigned8) uuid->node[0], (unsigned8) uuid->node[1],
        (unsigned8) uuid->node[2], (unsigned8) uuid->node[3],
        (unsigned8) uuid->node[4], (unsigned8) uuid->node[5]);

    *status = uuid_s_ok;
}

/*
**++
**
**  ROUTINE NAME:       uuid_from_string
**
**  SCOPE:              PUBLIC - declared in UUID.IDL
**
**  DESCRIPTION:
**
**  Decode a UUID from a printable string.
**
**  INPUTS:
**
**      uuid_string     The string UUID to be converted to a binary UUID
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      uuid            The binary representation of the given UUID
**
**      status          return status value
**
**          uuid_s_ok
**          uuid_s_bad_version
**          uuid_s_invalid_string_uuid
**          uuid_s_coding_error
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:     void
**
**  SIDE EFFECTS:       none
**
**--
**/

PUBLIC void uuid_from_string 
#ifdef _DCE_PROTO_
(
    unsigned_char_p_t       uuid_string,
    uuid_t                  *uuid,
    unsigned32              *status
)
#else
(uuid_string, uuid, status)
unsigned_char_p_t       uuid_string;
uuid_t                  *uuid;
unsigned32              *status;
#endif
{
    uuid_t              uuid_new;       /* used for sscanf for new uuid's */
    uuid_old_t          uuid_old;       /* used for sscanf for old uuid's */
    uuid_p_t            uuid_ptr;       /* pointer to correct uuid (old/new) */
    int                 i;


    CODING_ERROR (status);
    UUID_VERIFY_INIT(EmptyArg);

    /*
     * If a NULL pointer or empty string, give the nil UUID.
     */
    if (uuid_string == NULL || *uuid_string == '\0')
    {
	memcpy (uuid, &uuid_g_nil_uuid, sizeof *uuid);
	*status = uuid_s_ok;
	return;
    }

    /*
     * check to see that the string length is right at least
     */
    if (strlen ((char *) uuid_string) != UUID_C_UUID_STRING_MAX - 1)
    {
        *status = uuid_s_invalid_string_uuid;
        return;
    }

    /*
     * check for a new uuid
     */
    if (uuid_string[8] == '-')
    {
        long    time_low;
        int     time_mid;
        int     time_hi_and_version;
        int     clock_seq_hi_and_reserved;
        int     clock_seq_low;
        int     node[6];
#ifdef KERNEL
        extern int UUID_SSCANF(char * , char * , unsigned32 * , int * , 
		int * , int *, int *, int *, int *, int *, int *,
  		int *, int *);
#endif /* KERNEL */

        i = UUID_SSCANF(
            (char *) uuid_string, "%8lx-%4x-%4x-%2x%2x-%2x%2x%2x%2x%2x%2x",
#ifdef KERNEL
            (unsigned32 *)&time_low,
#else
            &time_low,
#endif /* KERNEL */
            &time_mid,
            &time_hi_and_version,
            &clock_seq_hi_and_reserved,
            &clock_seq_low,
            &node[0], &node[1], &node[2], &node[3], &node[4], &node[5]);

        /*
         * check that sscanf worked
         */
        if (i != UUID_ELEMENTS_NUM)
        {
            *status = uuid_s_invalid_string_uuid;
            return;
        }

        /*
         * note that we're going through this agony because scanf is defined to
         * know only to scan into "int"s or "long"s.
         */
        uuid_new.time_low                   = time_low;
        uuid_new.time_mid                   = time_mid;
        uuid_new.time_hi_and_version        = time_hi_and_version;
        uuid_new.clock_seq_hi_and_reserved  = clock_seq_hi_and_reserved;
        uuid_new.clock_seq_low              = clock_seq_low;
        uuid_new.node[0]                    = node[0];
        uuid_new.node[1]                    = node[1];
        uuid_new.node[2]                    = node[2];
        uuid_new.node[3]                    = node[3];
        uuid_new.node[4]                    = node[4];
        uuid_new.node[5]                    = node[5];

        /*
         * point to the correct uuid
         */
        uuid_ptr = &uuid_new;
    }
    else
    {
        long    time_high;
        int     time_low;
        int     family;
        int     host[7];
#ifdef KERNEL
	extern int UUID_OLD_SSCANF(char *, char *, unsigned32 *, int *,
  			int *, int *, int *, int *, int *, int *,
  			int *, int *);
#endif /* KERNEL */


        /*
         * format = tttttttttttt.ff.h1.h2.h3.h4.h5.h6.h7
         */
        i = UUID_OLD_SSCANF(
            (char *) uuid_string, "%8lx%4x.%2x.%2x.%2x.%2x.%2x.%2x.%2x.%2x",
#ifdef KERNEL
            (unsigned32 *)&time_high, &time_low, &family,
#else /* !KERNEL */
            &time_high, &time_low, &family,
#endif /* KERNEL */
            &host[0], &host[1], &host[2], &host[3],
            &host[4], &host[5], &host[6]);

        /*
         * check that sscanf worked
         */
        if (i != UUID_ELEMENTS_NUM_OLD)
        {
            *status = uuid_s_invalid_string_uuid;
            return;
        }

        /*
         * note that we're going through this agony because scanf is defined to
         * know only to scan into "int"s or "long"s.
         */
        uuid_old.time_high      = time_high;
        uuid_old.time_low       = time_low;
        uuid_old.family         = family;
        uuid_old.host[0]        = host[0];
        uuid_old.host[1]        = host[1];
        uuid_old.host[2]        = host[2];
        uuid_old.host[3]        = host[3];
        uuid_old.host[4]        = host[4];
        uuid_old.host[5]        = host[5];
        uuid_old.host[6]        = host[6];

        /*
         * fix up non-string field, and point to the correct uuid
         */
        uuid_old.reserved = 0;
        uuid_ptr = (uuid_p_t) (&uuid_old);
    }

    /*
     * sanity check, is version ok?
     */
    vCHECK_STRUCTURE (uuid_ptr, status);

    /*
     * copy the uuid to user
     */
    memcpy (uuid, uuid_ptr, sizeof (uuid_t));

    *status = uuid_s_ok;
}
#ifndef	SGIMIPS

/*
**++
**
**  ROUTINE NAME:       uuid_equal
**
**  SCOPE:              PUBLIC - declared in UUID.IDL
**
**  DESCRIPTION:
**
**  Compare two UUIDs.
**
**  INPUTS:
**
**      uuid1           The first UUID to compare
**
**      uuid2           The second UUID to compare
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      status          return status value
**
**          uuid_s_ok
**          uuid_s_bad_version
**          uuid_s_coding_error
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:
**
**      result          true if UUID's are equal
**                      false if UUID's are not equal
**
**  SIDE EFFECTS:       none
**
**--
**/

PUBLIC boolean32 uuid_equal 
#ifdef _DCE_PROTO_
(
    register uuid_p_t                uuid1,
    register uuid_p_t                uuid2,
    register unsigned32              *status
)
#else
(uuid1, uuid2, status)
register uuid_p_t                uuid1;
register uuid_p_t                uuid2;
register unsigned32              *status;
#endif
{
    RPC_LOG_UUID_EQUAL_NTR;
    CODING_ERROR (status);
    UUID_VERIFY_INIT (FALSE);

    bCHECK_STRUCTURE (uuid1, status);
    bCHECK_STRUCTURE (uuid2, status);

    *status = uuid_s_ok;

    /*
     * Note: This used to be a memcmp(), but changed to a field-by-field compare
     * because of portability problems with alignment and garbage in a UUID.
     */
    if ((uuid1->time_low == uuid2->time_low) && 
	(uuid1->time_mid == uuid2->time_mid) &&
	(uuid1->time_hi_and_version == uuid2->time_hi_and_version) && 
	(uuid1->clock_seq_hi_and_reserved == uuid2->clock_seq_hi_and_reserved) &&
	(uuid1->clock_seq_low == uuid2->clock_seq_low) &&
	(memcmp(uuid1->node, uuid2->node, 6) == 0))
    {
	RPC_LOG_UUID_EQUAL_XIT;
	return ( TRUE );
    }
    else
    {
        RPC_LOG_UUID_EQUAL_XIT;
        return (FALSE);
    }
}

/*
**++
**
**  ROUTINE NAME:       uuid_is_nil
**
**  SCOPE:              PUBLIC - declared in UUID.IDL
**
**  DESCRIPTION:
**
**  Check to see if a given UUID is 'nil'.
**
**  INPUTS:             none
**
**      uuid            A UUID
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      status          return status value
**
**          uuid_s_ok
**          uuid_s_bad_version
**          uuid_s_coding_error
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:
**
**      result          true if UUID is nil
**                      false if UUID is not nil
**
**  SIDE EFFECTS:       none
**
**--
**/

PUBLIC boolean32 uuid_is_nil 
#ifdef _DCE_PROTO_
(
    uuid_p_t            uuid,
    unsigned32          *status
)
#else
(uuid, status)
uuid_p_t            uuid;
unsigned32          *status;
#endif
{
    CODING_ERROR (status);
    UUID_VERIFY_INIT (FALSE);

    bCHECK_STRUCTURE (uuid, status);

    *status = uuid_s_ok;

    /*
     * Note: This should later be changed to a field-by-field compare
     * because of portability problems with alignment and garbage in a UUID.
     */
    if (memcmp (uuid, &uuid_g_nil_uuid, sizeof (uuid_t)) == 0)
    {
        return (TRUE);
    }
    else
    {
        return (FALSE);
    }
}

/*
**++
**
**  ROUTINE NAME:       uuid_compare
**
**  SCOPE:              PUBLIC - declared in UUID.IDL
**
**  DESCRIPTION:
**
**  Compare two UUID's "lexically"
**
**  If either of the two arguments is given as a NULL pointer, the other
**  argument will be compared to the nil uuid.
**
**  Note:   1) lexical ordering is not temporal ordering!
**          2) in the interest of keeping this routine short, I have
**             violated the coding convention that says all if/else
**             constructs shall have {}'s. There are a little million
**             return()'s in this routine. FWIW, the only {}'s that
**             are really required are the ones in the for() loop.
**
**  INPUTS:
**
**      uuid1           The first UUID to compare
**
**      uuid2           The second UUID to compare
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      status          return status value
**
**          uuid_s_ok
**          uuid_s_bad_version
**          uuid_s_coding_error
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:     uuid_order_t
**
**      -1   uuid1 is lexically before uuid2
**      1    uuid1 is lexically after uuid2
**
**  SIDE EFFECTS:       none
**
**--
**/

PUBLIC signed32 uuid_compare 
#ifdef _DCE_PROTO_
(
    uuid_p_t                uuid1,
    uuid_p_t                uuid2,
    unsigned32              *status
)
#else
(uuid1, uuid2, status)
uuid_p_t                uuid1;
uuid_p_t                uuid2;
unsigned32              *status;
#endif
{
    int                 i;

    CODING_ERROR (status);
    UUID_VERIFY_INIT (FALSE);


    /*
     * check to see if either of the arguments is a NULL pointer
     * - if so, compare the other argument to the nil uuid
     */
    if (uuid1 == NULL)
    {
        /*
         * if both arguments are NULL, so is this routine
         */
        if (uuid2 == NULL)
        {
            *status = uuid_s_ok;
            return (0);
        }

        rCHECK_STRUCTURE (uuid2, status, -1);
        return (uuid_is_nil (uuid2, status) ? 0 : -1);
    }

    if (uuid2 == NULL)
    {
        rCHECK_STRUCTURE (uuid1, status, -1);
        return (uuid_is_nil (uuid1, status) ? 0 : 1);
    }

    rCHECK_STRUCTURE (uuid1, status, -1);
    rCHECK_STRUCTURE (uuid2, status, -1);

    *status = uuid_s_ok;

    if (uuid1->time_low == uuid2->time_low)
    {
        if (uuid1->time_mid == uuid2->time_mid)
        {
            if (uuid1->time_hi_and_version == uuid2->time_hi_and_version)
            {
                if (uuid1->clock_seq_hi_and_reserved
                    == uuid2->clock_seq_hi_and_reserved)
                {
                    if (uuid1->clock_seq_low == uuid2->clock_seq_low)
                    {
                        for (i = 0; i < 6; i++)
                        {
                            if (uuid1->node[i] < uuid2->node[i])
                                return (-1);
                            if (uuid1->node[i] > uuid2->node[i])
                                return (1);
                        }
                        return (0);
                    }       /* end if - clock_seq_low */
                    else
                    {
                        if (uuid1->clock_seq_low < uuid2->clock_seq_low)
                            return (-1);
                        else
                            return (1);
                    }       /* end else - clock_seq_low */
                }           /* end if - clock_seq_hi_and_reserved */
                else
                {
                    if (uuid1->clock_seq_hi_and_reserved
                        < uuid2->clock_seq_hi_and_reserved)
                        return (-1);
                    else
                        return (1);
                }           /* end else - clock_seq_hi_and_reserved */
            }               /* end if - time_hi_and_version */
            else
            {
                if (uuid1->time_hi_and_version < uuid2->time_hi_and_version)
                    return (-1);
                else
                    return (1);
            }               /* end else - time_hi_and_version */
        }                   /* end if - time_mid */
        else
        {
            if (uuid1->time_mid < uuid2->time_mid)
                return (-1);
            else
                return (1);
        }                   /* end else - time_mid */
    }                       /* end if - time_low */
    else
    {
        if (uuid1->time_low < uuid2->time_low)
            return (-1);
        else
            return (1);
    }                       /* end else - time_low */
}

/*
**++
**
**  ROUTINE NAME:       uuid_hash
**
**  SCOPE:              PUBLIC - declared in UUID.IDL
**
**  DESCRIPTION:
**
**  Return a hash value for a given UUID.
**
**  Note: Since the length of a UUID is architecturally defined to be
**        128 bits (16 bytes), we have forgone using a '#defined'
**        length.  In particular, since the 'loop' has been unrolled
**        (for performance) the length is by definition 'hard-coded'.
**
**  INPUTS:
**
**      uuid            A UUID for which a hash value is to be computed
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      status          return status value
**
**          uuid_s_ok
**          uuid_s_bad_version
**          uuid_s_coding_error
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:
**
**      hash_value      The hash value computed from the UUID
**
**  SIDE EFFECTS:       none
**
**--
**/

PUBLIC unsigned16 uuid_hash 
#ifdef _DCE_PROTO_
(
    uuid_p_t                uuid,
    unsigned32              *status
)
#else
(uuid, status)
uuid_p_t                uuid;
unsigned32              *status;
#endif
{
    short               c0, c1;
    short               x, y;
    byte_p_t            next_uuid;

    RPC_LOG_UUID_HASH_NTR;

    CODING_ERROR (status);
    UUID_VERIFY_INIT (FALSE);

    rCHECK_STRUCTURE (uuid, status, 0);

    /*
     * initialize counters
     */
    c0 = c1 = 0;
    next_uuid = (byte_p_t) uuid;

    /*
     * For speed lets unroll the following loop:
     *
     *   for (i = 0; i < UUID_K_LENGTH; i++)
     *   {
     *       c0 = c0 + *next_uuid++;
     *       c1 = c1 + c0;
     *   }
     */
    c0 = c0 + *next_uuid++;
    c1 = c1 + c0;
    c0 = c0 + *next_uuid++;
    c1 = c1 + c0;
    c0 = c0 + *next_uuid++;
    c1 = c1 + c0;
    c0 = c0 + *next_uuid++;
    c1 = c1 + c0;

    c0 = c0 + *next_uuid++;
    c1 = c1 + c0;
    c0 = c0 + *next_uuid++;
    c1 = c1 + c0;
    c0 = c0 + *next_uuid++;
    c1 = c1 + c0;
    c0 = c0 + *next_uuid++;
    c1 = c1 + c0;

    c0 = c0 + *next_uuid++;
    c1 = c1 + c0;
    c0 = c0 + *next_uuid++;
    c1 = c1 + c0;
    c0 = c0 + *next_uuid++;
    c1 = c1 + c0;
    c0 = c0 + *next_uuid++;
    c1 = c1 + c0;

    c0 = c0 + *next_uuid++;
    c1 = c1 + c0;
    c0 = c0 + *next_uuid++;
    c1 = c1 + c0;
    c0 = c0 + *next_uuid++;
    c1 = c1 + c0;
    c0 = c0 + *next_uuid++;
    c1 = c1 + c0;

    /*
     *  Calculate the value for "First octet" of the hash
     */
    x = -c1 % 255;
    if (x < 0)
    {
        x = x + 255;
    }

    /*
     *  Calculate the value for "second octet" of the hash
     */
    y = (c1 - c0) % 255;
    if (y < 0)
    {
        y = y + 255;
    }

    /*
     * return the pieces put together
     */
    *status = uuid_s_ok;

    RPC_LOG_UUID_HASH_XIT;
    return ((y * 256) + x);
}

/*****************************************************************************
 *
 *  LOCAL MATH PROCEDURES - math procedures used internally by the UUID module
 *
 ****************************************************************************/

/*
** T I M E _ C M P
**
** Compares two UUID times (64-bit UTC values)
**/

INTERNAL uuid_compval_t time_cmp 
#ifdef _DCE_PROTO_
(
    uuid_time_p_t           time1,
    uuid_time_p_t           time2
)
#else
(time1, time2)
uuid_time_p_t           time1;
uuid_time_p_t           time2;
#endif
{
    /*
     * first check the hi parts
     */
    if (time1->hi < time2->hi) return (uuid_e_less_than);
    if (time1->hi > time2->hi) return (uuid_e_greater_than);

    /*
     * hi parts are equal, check the lo parts
     */
    if (time1->lo < time2->lo) return (uuid_e_less_than);
    if (time1->lo > time2->lo) return (uuid_e_greater_than);

    return (uuid_e_equal_to);
}

/*
**  U U I D _ _ U E M U L
**
**  Functional Description:
**        32-bit unsigned quantity * 32-bit unsigned quantity
**        producing 64-bit unsigned result. This routine assumes
**        long's contain at least 32 bits. It makes no assumptions
**        about byte orderings.
**
**  Inputs:
**
**        u, v       Are the numbers to be multiplied passed by value
**
**  Outputs:
**
**        prodPtr    is a pointer to the 64-bit result
**
**  Note:
**        This algorithm is taken from: "The Art of Computer
**        Programming", by Donald E. Knuth. Vol 2. Section 4.3.1
**        Pages: 253-255.
**--
**/

PRIVATE void uuid__uemul 
#ifdef _DCE_PROTO_
(
    unsigned32          u,
    unsigned32          v,
    unsigned64_t        *prodPtr
)
#else
(u, v, prodPtr)
unsigned32          u;
unsigned32          v;
unsigned64_t        *prodPtr;
#endif
{
    /*
     * following the notation in Knuth, Vol. 2
     */
    unsigned32      uuid1, uuid2, v1, v2, temp;


    uuid1 = u >> 16;
    uuid2 = u & 0xffff;
    v1 = v >> 16;
    v2 = v & 0xffff;

    temp = uuid2 * v2;
    prodPtr->lo = temp & 0xffff;
    temp = uuid1 * v2 + (temp >> 16);
    prodPtr->hi = temp >> 16;
    temp = uuid2 * v1 + (temp & 0xffff);
    prodPtr->lo += (temp & 0xffff) << 16;
    prodPtr->hi += uuid1 * v1 + (temp >> 16);
}

/****************************************************************************
**
**    U U I D   T R U E   R A N D O M   N U M B E R   G E N E R A T O R
**
*****************************************************************************
**
** This random number generator (RNG) was found in the ALGORITHMS Notesfile.
**
** (Note 16.7, July 7, 1989 by Robert (RDVAX::)Gries, Cambridge Research Lab,
**  Computational Quality Group)
**
** It is really a "Multiple Prime Random Number Generator" (MPRNG) and is
** completely discussed in reference #1 (see below).
**
**   References:
**   1) "The Multiple Prime Random Number Generator" by Alexander Hass
**      pp. 368 to 381 in ACM Transactions on Mathematical Software,
**      December, 1987
**   2) "The Art of Computer Programming: Seminumerical Algorithms
**      (vol 2)" by Donald E. Knuth, pp. 39 to 113.
**
** A summary of the notesfile entry follows:
**
** Gries discusses the two RNG's available for ULTRIX-C.  The default RNG
** uses a Linear Congruential Method (very popular) and the second RNG uses
** a technique known as a linear feedback shift register.
**
** The first (default) RNG suffers from bit-cycles (patterns/repetition),
** ie. it's "not that random."
**
** While the second RNG passes all the emperical tests, there are "states"
** that become "stable", albeit contrived.
**
** Gries then presents the MPRNG and says that it passes all emperical
** tests listed in reference #2.  In addition, the number of calls to the
** MPRNG before a sequence of bit position repeats appears to have a normal
** distribution.
**
** Note (mbs): I have coded the Gries's MPRNG with the same constants that
** he used in his paper.  I have no way of knowing whether they are "ideal"
** for the range of numbers we are dealing with.
**
****************************************************************************/

/*
** T R U E _ R A N D O M _ I N I T
**
** Note: we "seed" the RNG with the bits from the clock and the PID
**
**/

INTERNAL void true_random_init (void)
{
    uuid_time_t         t;
    unsigned16          *seedp, seed=0;


    /*
     * optimal/recommended starting values according to the reference
     */
    static unsigned32   rand_m_init     = 971;
    static unsigned32   rand_ia_init    = 11113;
    static unsigned32   rand_ib_init    = 104322;
    static unsigned32   rand_irand_init = 4181;

    rand_m = rand_m_init;
    rand_ia = rand_ia_init;
    rand_ib = rand_ib_init;
    rand_irand = rand_irand_init;

    /*
     * Generating our 'seed' value
     *
     * We start with the current time, but, since the resolution of clocks is
     * system hardware dependent (eg. Ultrix is 10 msec.) and most likely
     * coarser than our resolution (10 usec) we 'mixup' the bits by xor'ing
     * all the bits together.  This will have the effect of involving all of
     * the bits in the determination of the seed value while remaining system
     * independent.  Then for good measure to ensure a unique seed when there
     * are multiple processes creating UUID's on a system, we add in the PID.
     */
    uuid__get_os_time(&t);
    seedp = (unsigned16 *)(&t);
    seed ^= *seedp++;
    seed ^= *seedp++;
    seed ^= *seedp++;
    seed ^= *seedp++;
    rand_irand += seed + uuid__get_os_pid();
}

/*
** T R U E _ R A N D O M
**
** Note: we return a value which is 'tuned' to our purposes.  Anyone
** using this routine should modify the return value accordingly.
**/

INTERNAL unsigned16 true_random (void)
{
    rand_m += 7;
    rand_ia += 1907;
    rand_ib += 73939;

    if (rand_m >= 9973) rand_m -= 9871;
    if (rand_ia >= 99991) rand_ia -= 89989;
    if (rand_ib >= 224729) rand_ib -= 96233;

    rand_irand = (rand_irand * rand_m) + rand_ia + rand_ib;

    return (HI_WORD (rand_irand) ^ (rand_irand & RAND_MASK));
}

/*****************************************************************************
 *
 *  LOCAL PROCEDURES - procedures used staticly by the UUID module
 *
 ****************************************************************************/

/*
** N E W _ C L O C K _ S E Q
**
** Ensure *clkseq is up-to-date
**
** Note: clock_seq is architected to be 14-bits (unsigned) but
**       I've put it in here as 16-bits since there isn't a
**       14-bit unsigned integer type (yet)
**/

INTERNAL void new_clock_seq 
#ifdef _DCE_PROTO_
(
    unsigned16              *clkseq
)
#else
(clkseq)
unsigned16              *clkseq;
#endif
{
    /*
     * A clkseq value of 0 indicates that it hasn't been initialized.
     */
    if (*clkseq == 0)
    {
#ifdef UUID_NONVOLATILE_CLOCK
        *clkseq = uuid__read_clock();           /* read nonvolatile clock */
        if (*clkseq == 0)                       /* still not init'd ???   */
        {
            *clkseq = true_random();      /* yes, set random        */
        }
#else
        /*
         * with a volatile clock, we always init to a random number
         */
        *clkseq = true_random();
#endif
    }

    CLOCK_SEQ_BUMP (clkseq);
    if (*clkseq == 0)
    {
        *clkseq = *clkseq + 1;
    }

#ifdef UUID_NONVOLATILE_CLOCK
    uuid_write_clock (clkseq);
#endif
}

/*
**++
**
**  ROUTINE NAME:       uuid_get_address
**
**  SCOPE:              PUBLIC
**
**  DESCRIPTION:
**
**  Return our IEEE 802 address.
**
**  This function is not really "public", but more like the SPI functions
**  -- available but not part of the official API.  We've done this so
**  that other subsystems (of which there are hopefully few or none)
**  that need the IEEE 802 address can use this function rather than
**  duplicating the gore it does (or more specifically, the gore that
**  "uuid__get_os_address" does).
**
**  INPUTS:             none
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      addr            IEEE 802 address
**
**      status          return status value
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:     none
**
**  SIDE EFFECTS:       none
**
**--
**/

PUBLIC void uuid_get_address 
#ifdef _DCE_PROTO_
(
    uuid_address_p_t        addr,
    unsigned32              *status
)
#else
(addr, status)
uuid_address_p_t        addr;
unsigned32              *status;
#endif
{
    /*
     * just return address we determined previously if we've
     * already got one
     */
    if (got_address)
    {
        memcpy (addr, &saved_addr, sizeof (uuid_address_t));
        *status = saved_st;
        return;
    }

    /*
     * Otherwise, call the system specific routine.
     */
    uuid__get_os_address (addr, status);

    if (*status == uuid_s_ok)
    {
        got_address = TRUE;
        memcpy (&saved_addr, addr, sizeof (uuid_address_t));

#ifdef  UUID_DEBUG
        RPC_DBG_GPRINTF ((
	    "uuid_get_address:        %02x-%02x-%02x-%02x-%02x-%02x\n",
            addr->eaddr[0], addr->eaddr[1], addr->eaddr[2],
            addr->eaddr[3], addr->eaddr[4], addr->eaddr[5] ));
#endif

    }
    else
    {
        *status = uuid_s_no_address;
    }
}

#endif
