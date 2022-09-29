/*************************************************************************
*
* Copyright 1994 Adaptec, Inc.,  All Rights Reserved.
*
* This software contains the valuable trade secrets of Adaptec.  The
* software is protected under copyright laws as an unpublished work of
* Adaptec.  Notice is for informational purposes only and does not imply
* publication.  The user of this software may make copies of the software
* for use with parts manufactured by Adaptec or under license from Adaptec
* and for no other use.
*
*
* $Id: osm.h,v 1.2 1997/07/24 00:33:28 philw Exp $
*
*************************************************************************/


/***************************************************************
 *   Operating System specific type definitions
 ***************************************************************/

#ifndef __HIM_OSM_H_
#define __HIM_OSM_H_

#include "sys/types.h"
#ifdef IP32
#include "sys/IP32.h"
#endif
#include "sys/systm.h"

#define BYTE		char
#define WORD		short
#define	DWORD		unsigned int
#define	UBYTE		unsigned char
#define UWORD		unsigned short
#define OS_TYPE		void *


#define INDWORD		osm_indword
#define	OUTDWORD	osm_outdword
#define	INBYTE		osm_inbyte
#define	OUTBYTE		osm_outbyte

#define DCACHE_INVAL(a,b)	__dcache_inval((caddr_t) a, (uint) b)
#define DCACHE_WB(a,b)		__dcache_wb((caddr_t) a, (uint) b)
#define DCACHE_WBINVAL(a,b)	__dcache_wb_inval((caddr_t) a, (uint) b)

#define ADP78_NUMSCBS		16

#ifdef MHSIM
#define PRINT		DPRINTF
#else
#define PRINT		printf
#endif

/*
 * Interrupt and semaphore routines.
 */
#ifdef IP32
#define	INTR_OFF		0
#define	INTR_ON(s)		
#define BLOCK_INTERRUPTS()	0
#define UNBLOCK_INTERRUPTS(s)	
#else
#define	INTR_OFF		splhintr()
#define	INTR_ON(s)		splx(s)
#define BLOCK_INTERRUPTS()	splhintr();
#define UNBLOCK_INTERRUPTS(s)	splx(s);
#endif
#define ACQUIRE_LOCK(a)
#define RELEASE_LOCK(a)

#define Seq_00    P_Seq_00      /* Resolve name conflict with arrow */
#define Seq_01    P_Seq_01      /* Resolve name conflict with arrow */
#define SeqExist  P_SeqExist    /* Resolve name conflict with arrow */


/*
 * Flags needed for the HIM and adp78.c
 */
#define _DRIVER
#define _FAULT_TOLERANCE
#define _EX_SEQ00_
#define MIPS_BE
/* #define LE_DISK */

/*
 * macros to make big endien conversions look cleaner.
 */
#ifdef MIPS_BE
#define OSM_CONV_BYTEPTR(a)	osm_conv_byteptr(a)
#else
#define OSM_CONV_BYTEPTR(a)	a
#endif



/***************************************************************
 %%%   Function prototypes			%%%
***************************************************************/

UBYTE osm_inbyte(volatile BYTE *byte_addr);
void osm_outbyte(volatile BYTE *byte_addr, UBYTE write_byte);
DWORD osm_indword(DWORD *dword_addr);
void osm_outdword(DWORD *dword_addr, DWORD write_dword);
DWORD osm_swizzle(UBYTE *buf, uint len);
UBYTE osm_conv_byteptr(UBYTE src);

#endif  /*__HIM_OSM_H_ */
