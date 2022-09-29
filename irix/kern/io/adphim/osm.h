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
* $Id: osm.h,v 1.4 1997/04/12 03:52:59 wmesard Exp $
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
#define	DWORD		int
#define	UDWORD		unsigned int
#define	UBYTE		unsigned char
#define UWORD		unsigned short
#define OS_TYPE		void *


#define INDWORD		osm_indword
#define	OUTDWORD	osm_outdword
#define	INBYTE		osm_inbyte
#define	OUTBYTE		osm_outbyte

#define DCACHE_INVAL(a,b)	dki_dcache_inval(a, b)
#define DCACHE_WB(a,b)		dki_dcache_wb(a, b)
#define DCACHE_WBINVAL(a,b)	dki_dcache_wbinval(a, b)

#define ADP78_NUMSCBS	16

/*
 * Interrupt and semaphore routines.
 */
#ifdef IP32
#define	INTR_OFF		splhintr()
#define	INTR_ON(s)		splx(s)
#define BLOCK_INTERRUPTS()	splhintr();
#define UNBLOCK_INTERRUPTS(s)	splx(s);
#else
#define	INTR_OFF		splhintr()
#define	INTR_ON(s)		splx(s)
#define BLOCK_INTERRUPTS()	splhintr();
#define UNBLOCK_INTERRUPTS(s)	splx(s);
#endif
#define ACQUIRE_LOCK(a)
#define RELEASE_LOCK(a)


/*
 * Flags needed for the HIM
 */
#define _DRIVER
#define _FAULT_TOLERANCE
#define _EX_SEQ00_
#define MIPS_BE

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
DWORD osm_indword(volatile DWORD *dword_addr);
void osm_outdword(volatile DWORD *dword_addr, DWORD write_dword);
DWORD osm_swizzle(UBYTE *buf, uint len);
UBYTE osm_conv_byteptr(UBYTE src);

extern void osm_init();
/* extern void PH_ScbCompleted(struct sp *); */

#endif  /*__HIM_OSM_H_ */
