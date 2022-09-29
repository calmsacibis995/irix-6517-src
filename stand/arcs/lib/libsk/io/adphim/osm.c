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
* $Id: osm.c,v 1.3 1997/07/24 22:01:10 wmesard Exp $
*
***********************************************************************/

#ifndef SIMHIM

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
/* #include "sys/user.h" */
#include "sys/immu.h"
#include "sys/uio.h"
#include "sys/edt.h"
#include "sys/select.h"
#include "sys/cmn_err.h"
#include "sys/debug.h"
#include <arcs/hinv.h>
#include <pci_intf.h>
#include <pci/PCI_defs.h>
#include "osm.h"
#include "him_scb.h"
#include "him_equ.h"

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

#define SWIZZLEBUF_SIZE		(4096 * 12)
UBYTE swizzlebuf[SWIZZLEBUF_SIZE];

uint
osm_swizzle(UBYTE *buf, uint len)
{
	int word, s;
	uint actual_len;

	if (len%4 != 0) {
		cmn_err(CE_WARN, "osm_swizzle: len not mod 4 (%d)!", len);
		return -1;
	}


	if (((DWORD) buf)%4 != 0) {
		cmn_err(CE_WARN, "osm_swizzle: buf addr not mod 4 (0x%x)!",
			buf);
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
	UBYTE readval;
#ifdef DEBUG
	if (!IS_KSEG1(byte_addr)) {
		cmn_err(CE_WARN, "osm_inbyte: bad address 0x%x", byte_addr);
		return 0;
	}
#endif
	readval = *byte_addr;
/*	printf("Read byte 0x%x = 0x%x\n", byte_addr, readval); */
	return (readval);
}

/*
 * write the write_byte to byte_addr.
 * Returns nothing.
 */
void
osm_outbyte(volatile BYTE *byte_addr, UBYTE write_byte)
{
#ifdef DEBUG
	if (!IS_KSEG1(byte_addr)) {
		cmn_err(CE_WARN, "osm_outbyte: bad address 0x%x", byte_addr);
		return;
	}
#endif
/*	printf("Write byte 0x%x = 0x%x\n", byte_addr, write_byte); */
	*byte_addr = write_byte;
}

/*
 * return the DWORD pointed by addr dword_addr.
 */
DWORD
osm_indword(DWORD *dword_addr)
{
#ifdef DEBUG
	if (!IS_KSEG1(dword_addr)) {
		cmn_err(CE_WARN, "osm_indword: bad address 0x%x", dword_addr);
		return 0;
	}

/*	cmn_err(CE_WARN, "Read dword 0x%x = 0x%x", dword_addr, *dword_addr); */
#endif
	return (*dword_addr);
}


/*
 * write the write_dword or dword_addr.
 * Returns nothing.
 */
void
osm_outdword(DWORD *dword_addr, DWORD write_dword)
{
#ifdef DEBUG
	if (!IS_KSEG1(dword_addr)) {
		cmn_err(CE_WARN, "osm_outdword: bad address 0x%x", dword_addr);
		return;
	}

/*	cmn_err(CE_WARN, "Write dword 0x%x = 0x%x", dword_addr, write_dword); */
#endif
	*dword_addr = write_dword;
}

#endif /* SIMHIM */
