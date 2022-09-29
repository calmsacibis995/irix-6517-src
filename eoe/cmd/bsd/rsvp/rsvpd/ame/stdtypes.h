/*sccs************************************************************************
 *sccs Audit trail generated for Peer sccs tracking
 *sccs module stdtypes.h, release 1.15 dated 96/07/11
 *sccs retrieved from /home/peer+bmc/ame/common/include/src/SCCS/s.stdtypes.h
 *sccs
 *sccs    1.15	96/07/11 17:54:48 sshanbha
 *sccs	Get around AIX's macro expansion problems (PR#684)
 *sccs
 *sccs    1.14	96/06/07 18:20:45 sshanbha
 *sccs	Appropriate handling of PEER_PORTABLE_ULONG for dgux (PR#645)
 *sccs
 *sccs    1.13	96/02/12 13:21:04 randy
 *sccs	update company name (PR#613)
 *sccs
 *sccs    1.12	95/05/24 13:57:25 randy
 *sccs	old lynx cc doesn't do token paste (PR#338)
 *sccs
 *sccs    1.11	95/05/05 16:59:30 randy
 *sccs	added macro to deal with differing compiler support for ulong constants (PR#338)
 *sccs
 *sccs    1.10	95/01/24 13:01:52 randy
 *sccs	update company addres (PR#183)
 *sccs
 *sccs    1.9	95/01/16 16:20:21 randy
 *sccs	verified 64-bit support on Alpha (PR#173)
 *sccs
 *sccs    1.8	93/08/19 13:55:15 randy
 *sccs	64-bit counter support
 *sccs
 *sccs    1.7	92/11/12 16:34:49 randy
 *sccs	Copyright PEER Networks, Inc.
 *sccs
 *sccs    1.6	92/09/11 19:13:56 randy
 *sccs	removed superfluous definitions, added void_function
 *sccs
 *sccs    1.5	92/08/31 14:08:11 randy
 *sccs	portability improvements
 *sccs
 *sccs    1.4	92/03/20 11:29:03 randy
 *sccs	get rid of ulong (portability problem in some systems)
 *sccs
 *sccs    1.3	91/12/30 13:59:26 randy
 *sccs	added void control for dos port
 *sccs
 *sccs    1.2	91/10/25 14:58:16 randy
 *sccs	support wider range of types
 *sccs
 *sccs    1.1	91/09/03 16:51:58 randy
 *sccs	date and time created 91/09/03 16:51:58 by randy
 *sccs
 *sccs
 *sccs************************************************************************/

#ifndef AME_STDTYPESH
#define AME_STDTYPESH

/************************************************************************
 *                                                                      *
 *          PEER Networks, a division of BMC Software, Inc.             *
 *                   CONFIDENTIAL AND PROPRIETARY                       *
 *                                                                      *
 *	This product is the sole property of PEER Networks, a		*
 *	division of BMC Software, Inc., and is protected by U.S.	*
 *	and other copyright laws and the laws protecting trade		*
 *	secret and confidential information.				*
 *									*
 *	This product contains trade secret and confidential		*
 *	information, and its unauthorized disclosure is			*
 *	prohibited.  Authorized disclosure is subject to Section	*
 *	14 "Confidential Information" of the PEER Networks, a		*
 *	division of BMC Software, Inc., Standard Development		*
 *	License.							*
 *									*
 *	Reproduction, utilization and transfer of rights to this	*
 *	product, whether in source or binary form, is permitted		*
 *	only pursuant to a written agreement signed by an		*
 *	authorized officer of PEER Networks, a division of BMC		*
 *	Software, Inc.							*
 *									*
 *	This product is supplied to the Federal Government as		*
 *	RESTRICTED COMPUTER SOFTWARE as defined in clause		*
 *	55.227-19 of the Federal Acquisitions Regulations and as	*
 *	COMMERCIAL COMPUTER SOFTWARE as defined in Subpart		*
 *	227.401 of the Defense Federal Acquisition Regulations.		*
 *									*
 * Unpublished, Copr. PEER Networks, a division of BMC	Software, Inc.  *
 *                     All Rights Reserved				*
 *									*
 *	PEER Networks, a division of BMC Software, Inc.			*
 *	1190 Saratoga Avenue, Suite 130					*
 *	San Jose, CA 95129-3433 USA					*
 *									*
 ************************************************************************/


/*************************************************************************
 *
 *	stdtypes.h  -  standard types used throughout the application
 *		       management environment
 *
 ************************************************************************/

#include "ame/machtype.h"

/*****************************************************************************
 *
 *	Type definitions for basic types with precision / range constraints
 *
 *	These need to be adjusted to match the underlying hardware.  The
 *	manifests defined in machtype.h control the whole process.
 *
 ***************************************************************************/

typedef unsigned char	ubyte;		/* must not have sign extension */

typedef short		int16;		/* trades off storage space for */
typedef unsigned short	uint16;		/* range of 16-bit numbers	*/

typedef long		int32;		/* basic signed integer		*/
typedef unsigned long	uint32;		/* basic unsigned 32-bit integer*/


/*
 *	Opaque types are used for generic pointers.  These are needed to
 *	keep compilers that don't fully understand the (void *) idiom happy.
 */
#ifdef	VOID_NOT_SUPPORTED

typedef long		Void;		/* for opaque pointer references*/

#else

typedef void		Void;		/* for opaque pointer references*/

#endif


/*
 *	Support for 64-bit integers depends on the underlying hardware.
 */
#ifdef	BIT64

typedef long int	int64;		/* native, signed 64-bit support*/
typedef unsigned long	uint64;		/* native unsigned 64 bit	*/

#define BUILD_64(DEST,HIGH,LOW) DEST = (((((uint64) (HIGH)) << 32))+LOW)
#define HIGH_OF_64(X)		(((X) >> 32) & 0xffffffffL)
#define LOW_OF_64(X)		((X) & 0xffffffffL)

#else

/*
 *	If the machine has an underlying 32-bit architecture, we emulate
 *	sixty-four bit support, even to the extent of prefered byte-ordering
 */
struct	int_64
{
#ifdef LITTLE_ENDIAN
	uint32	low;			/* least-significant portion	*/
	int32	high;			/* most-significant part	*/
#else
	int32	high;			/* most-significant part	*/
	uint32	low;			/* least signifcant part	*/
#endif
};

struct	uint_64
{
#ifdef LITTLE_ENDIAN
	uint32	low;			/* least-significant portion	*/
	uint32	high;			/* most-significant part	*/
#else
	uint32	high;			/* most-significant part	*/
	uint32	low;			/* least signifcant part	*/
#endif
};

typedef struct int_64	int64;		/* signed sixty-four bit things */
typedef struct uint_64	uint64;		/* unsigned 64-bit integers	*/

#define HIGH_OF_64(X)		(X).high
#define LOW_OF_64(X)		(X).low
#define BUILD_64(DEST,HIGH,LOW) (DEST).high = HIGH; (DEST).low = LOW

#endif


/*
 *	Handy constants
 */
#ifndef NULL
#define NULL	0
#endif


/*
 *	Define a pointer to a void function with no parameters.	 This helps
 *	reduce spurious lint complaints.
 */
#ifndef USE_PROTOTYPES
typedef void	(*void_function)();		/* pointer to void function  */
#else
typedef void	(*void_function)(void);		/* pointer to void function  */
#endif

/*
 *	Define a macro to deal with the initialization of unsigned long
 *	constants.  This reduces complaints from compilers that insist
 *	on a "u" suffix instead of an "L" suffix.
 */
#ifdef USE_PROTOTYPES
#define PEER_PORTABLE_ULONG(x)  x##u
#else
#ifdef PEER_AIX_PORT
#define PEER_PORTABLE_ULONG(x) x##L /* avoid AIX macro expansion problems */
#else
#ifdef PEER_LYNX_PORT
#define PEER_PORTABLE_ULONG(x)  x	/* only old lynx cc needs this	*/
#else
#define PEER_PASTE(x)           x
#ifdef  PEER_DGUX_PORT
#define PEER_PORTABLE_ULONG(x)  PEER_PASTE(x)u /* dgux 5.4 cc needs 'u' suffix*/
#else
#define PEER_PORTABLE_ULONG(x)  PEER_PASTE(x)L
#endif
#endif
#endif
#endif

#endif
