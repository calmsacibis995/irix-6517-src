/***********************************************************************
*
* Copyright 1994 Adaptec, Inc.,  All Rights Reserved.
*
* This software contains the valuable trade secrets of Adaptec.  The
* software is protected under copyright laws as an unpublished work of
* Adaptec.  Notice is for informational purposes only and does not 
* imply publication.  The user of this software may make copies of 
* the software for use with parts manufactured by Adaptec or under 
* license from Adaptec and for no other use.
*
***********************************************************************
*
*  Module Name:  AIC-7870 OSM (OS Specific Module) for SCO UNIX v3.2.4
*  Source Code:  osm.c osm.h
*  Description:  OS Specific Module for linking/compiling with HIM
*                to support the AIC-7870 and AIC-7870 based host 
*		  adapters (ie. AHA-2840).
*  History    :
*     03/04/94   V1.0 Release. 
*
* $Id: osm.c,v 1.7 1998/02/07 02:17:50 joshd Exp $
*
***********************************************************************/

#include "sys/types.h"
#include "sys/sysmacros.h"
#include "sys/systm.h"
#include "sys/sema.h"
#include "sys/iobuf.h"
#include "sys/param.h"
#include "sys/conf.h"
#include "sys/dir.h"
#include "sys/signal.h"
#include "sys/errno.h"
#include "sys/immu.h"
#include "sys/uio.h"
#include "sys/select.h"
#include "sys/debug.h"
#include "osm.h"
#include "him_scb.h"
#include "him_equ.h"
#include "sys/ddi.h"
#include "sys/PCI/pciio.h"

extern char adp_verbose;

/*
 * Converts big endian byte offsets within a word to little endian byte
 * offsets and vice versa.
 */
UBYTE
osm_conv_byteptr(UBYTE src)
{

	switch(src & 0x3) {
	case 0:
		return ((src & 0xfc) | 0x3);

	case 1:
		return ((src & 0xfc) | 0x2);

	case 2:
		return ((src & 0xfc) | 0x1);

	case 3:
		return (src & 0xfc);
	}
	/* not reached */
	ASSERT(0);
	return 0;
}


/*
 * Take a UBTYE ptr to an array of len bytes and swizzle it (convert from
 * big to little endian or little to big.  Len must be a multiple of 4
 * and less than 4096.  The buf address also must me mod 4.
 * 
 * Returns 0 if the swizzle was successful, -1 if not.
 */

#define SWIZZLEBUF_SIZE		4096
UBYTE swizzlebuf[SWIZZLEBUF_SIZE];

int
osm_swizzle(UBYTE *buf, uint len)
{
	int word, s;
	uint actual_len;

	if (len%4 != 0) {
#ifdef DEBUG
		printf("osm_swizzle: len not mod 4 (%d)!\n", len);
#endif
		return -1;
	}


	if (((DWORD) buf)%4 != 0) {
#ifdef DEBUG
		printf("osm_swizzle: buf addr not mod 4 (0x%x)!\n",
			buf);
#endif
		return -1;
	}

	s = BLOCK_INTERRUPTS();
	actual_len = MIN(len, SWIZZLEBUF_SIZE);
	while (len > 0) {
		bcopy(buf, swizzlebuf, actual_len);
		word = 0;
		while (word < actual_len) {
			buf[word + 0] = swizzlebuf[word + 3];
			buf[word + 1] = swizzlebuf[word + 2];
			buf[word + 2] = swizzlebuf[word + 1];
			buf[word + 3] = swizzlebuf[word + 0];
			word += 4;
		}

		len -= actual_len;
		buf += actual_len;
	}
 	UNBLOCK_INTERRUPTS(s);

	return 0;
}


/*
 * return the byte pointed by addr byte_addr.
 */
UBYTE
osm_inbyte(volatile BYTE *byte_addr)
{
	return pciio_pio_read8((uint8_t *)byte_addr);
}

int verbose_outbyte=0;
/*
 * write the write_byte to byte_addr.
 * Returns nothing.
 */
void
osm_outbyte(volatile BYTE *byte_addr, UBYTE write_byte)
{
	pciio_pio_write8((uint8_t)write_byte, (uint8_t *)byte_addr);
}

/*
 * return the DWORD pointed by addr dword_addr.
 */
DWORD
osm_indword(volatile DWORD *dword_addr)
{
#ifdef DEBUG
	if (!IS_KSEG1(dword_addr)) {
		printf("osm_indword: bad address 0x%x\n", dword_addr);
		return 0;
	}
#endif
	return pciio_pio_read64((uint64_t *)dword_addr);
}


/*
 * write the write_dword or dword_addr.
 * Returns nothing.
 */
void
osm_outdword(volatile DWORD *dword_addr, DWORD write_dword)
{
#ifdef DEBUG
	if (!IS_KSEG1(dword_addr)) {
		printf("osm_outdword: bad address 0x%x\n", dword_addr);
		return;
	}
#endif
	pciio_pio_write64((uint64_t)write_dword, (uint64_t *)dword_addr);
}


