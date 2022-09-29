/***************************************************************************
*									   *
* Copyright 1996 Adaptec, Inc.,  All Rights Reserved.			   *
*									   *
* This software contains the valuable trade secrets of Adaptec.  The	   *
* software is protected under copyright laws as an unpublished work of	   *
* Adaptec.  Notice is for informational purposes only and does not imply   *
* publication.	The user of this software may make copies of the software  *
* for use with parts manufactured by Adaptec or under license from Adaptec *
* and for no other use.							   *
*									   *
***************************************************************************/

#ident "%Z% %M% %D% %I%"

#ifndef _HWCUSTOM_H
#define _HWCUSTOM_H

/***************************************************************************
*
*  Module Name:   custom.h
*
*  Description:   Definitions for customization
*    
*  Owners:	  ECX IC Firmware Team
*    
*  Notes:	  1. Custimizable parameters for overall Common HIM
*		  2. Definitions defined here must be examined carefully
*		     by OSM developers. It's up to OSM developers to make
*		     sure the correct configuration get defined to fit
*		     their requirements.
*		  3. This file is included through GENERAL.H
*
***************************************************************************/


#ifdef sgi
#include <sys/types.h>
#include <sys/ddi.h>
#include <sys/systm.h>
#endif


/*
* Definition for BUS_ADDRESS
*/
#undef _ADDRESS_WIDTH32
typedef uint64_t BUS_ADDRESS;

/*
 * Definition for virtual address
 */
typedef void * VIRTUAL_ADDRESS;


/*
 * Scatter/Gather descriptor for 64 bit addressing
 */
typedef struct _SG_DESCRIPTOR {
   uint8_t	busAddress[8];	     /* 64 bit little endian address */
   uint8_t	segmentLength[4];    /* 32 bit little endian segment length */
   uint8_t	reserved[4];
} SH_SG_DESCRIPTOR;

/* Other customizable definitions					*/
typedef int32_t BYTE4;		      /* exact 4 byte entity	      */
typedef uint32_t UBYTE4;	      /* exact 4 byte entity	      */


/***************************************************************************
* Definitions for a accessing a two byte value one byte at a time.
***************************************************************************/
typedef union _DOUBLET		       /* two bytes entity		*/
{
   struct UBYTE_DOUBLET
   {
      UBYTE byte1;		/* byte0 is LSB, byte1 is MSB of addr */
      UBYTE byte0;
   } ubyteDoublet;
} DOUBLET;

#define  DU_ubyte0		       ubyteDoublet.byte0
#define  DU_ubyte1		       ubyteDoublet.byte1

/***************************************************************************
* Definitions for accessing a four byte value one byte at a time
***************************************************************************/
typedef union _QUADLET		       /* four bytes entity		*/
{
   struct UBYTE_QUADLET
   {
      UBYTE byte3;		/* byte0 is LSB, byte 3 is MSB of addr */
      UBYTE byte2;
      UBYTE byte1;
      UBYTE byte0;
   } ubyteQuadlet;
} QUADLET;

#define  QU_byte0		      ubyteQuadlet.byte0
#define  QU_byte1		      ubyteQuadlet.byte1
#define  QU_byte2		      ubyteQuadlet.byte2
#define  QU_byte3		      ubyteQuadlet.byte3

/***************************************************************************
* Definitions for an eight byte value one byte at a time.
***************************************************************************/
typedef union _OCTLET		       /* 8 byte entity			*/
{
   struct UBYTE_OCTLET
   {
      UBYTE byte7;	/* byte0 is LSB, byte 7 is MSB of addr */
      UBYTE byte6;
      UBYTE byte5;
      UBYTE byte4;
      UBYTE byte3;
      UBYTE byte2;
      UBYTE byte1;
      UBYTE byte0;
   } ubyteOctlet;
} OCTLET;

/*
 * Note that the ordering of these bytes must be adjusted to reflect
 * the endian characteristics of the platform
 */
#define  OU_byte0		       ubyteOctlet.byte0
#define  OU_byte1		       ubyteOctlet.byte1
#define  OU_byte2		       ubyteOctlet.byte2
#define  OU_byte3		       ubyteOctlet.byte3
#define  OU_byte4		       ubyteOctlet.byte4
#define  OU_byte5		       ubyteOctlet.byte5
#define  OU_byte6		       ubyteOctlet.byte6
#define  OU_byte7		       ubyteOctlet.byte7

#if defined(_SOLARIS) || defined(sgi)
#include <sys/cmn_err.h>
#define ulm_printf printf
#endif


/*
 * Definitions for register, scratch ram and scb ram addressing
 */
#define  INBYTE(reg)		 ((UBYTE) inMemByte((uint64_t) reg))
#define  OUTBYTE(reg,value)	 outMemByte((uint64_t) reg,(UBYTE)(value))
#define  INWORD(reg)		 ((USHORT) inMemWord((uint64_t) reg))
#define  OUTWORD(reg,value)	 outMemWord((uint64_t) reg,(USHORT)(value))
#define  INPCIBYTE(reg, ui)	 ((UBYTE) inMemByte((uint64_t) reg))
#define  OUTPCIBYTE(reg,value,ui)	 outMemByte((uint64_t) reg,(UBYTE)(value))

#define AICREG(a)  &base.reg[(a)]
#define PCIREG(a)  &(c->pcicfgbase).reg[(a)]
#define SEQMEM(a)  &(c->seqmembase).reg[(a)]
#define TCBMEM(a)  &(c->seqmembase).reg[(a)]

/***************************************************************************
* Definition of register
***************************************************************************/
typedef union _REGISTER
{
	BUS_ADDRESS baseAddress;	/* base address			 */
	UBYTE *reg;			/* for register addressing	 */
	USHORT offset;
} REGISTER;

/* Prototypes							*/
UBYTE inMemByte(uint64_t reg);
void outMemByte(uint64_t reg, UBYTE value);
USHORT inMemWord(uint64_t reg);
void outMemWord(uint64_t reg, USHORT value);

/*
 * Swap shorts
 */
USHORT ULM_SWAP_SHORT(USHORT data);

#define PCI_DATA_64		1	/* 64 bit data xfers if 1 */
#define OSM_CPU_BIGENDIAN	1
#define PCI_BIGENDIAN		0
#define OSM_DMA_SWAP_WIDTH	64

#define ADPFC_PAYLOAD_SIZE	2048 /* must change together with ADPFC_HST_CFG_CTL */
#define ADPFC_HST_CFG_CTL	4   /* must change together with ADPFC_PAYLOAD_SIZE */


/* Hardware platform specific customization				*/
#if PCI_DATA_64
#define HCOMMAND1_DEFAULT (CLINE_XFER | MASTER_CTL_64 | STOPONPERR)
#else
#define HCOMMAND1_DEFAULT (CLINE_XFER | MASTER_CTL_32 | STOPONPERR)
#endif
#define HCOMMAND0_DEFAULT	0xc0 

/* Misc definitions */
BUS_ADDRESS ulm_kvtop(VIRTUAL_ADDRESS vaddr, void *ulm_info);

#endif /* _HWCUSTOM_H */
