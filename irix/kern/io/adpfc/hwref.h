/***************************************************************************
*									   *
* Copyright 1997 Adaptec, Inc.,  All Rights Reserved.			   *
*									   *
* This software contains the valuable trade secrets of Adaptec.  The	   *
* software is protected under copyright laws as an unpublished work of	   *
* Adaptec.  Notice is for informational purposes only and does not imply   *
* publication.	The user of this software may make copies of the software  *
* for use with parts manufactured by Adaptec or under license from Adaptec *
* and for no other use.							   *
*									   *
***************************************************************************/

#ifndef _HWREF_H
#define _HWREF_H

/***************************************************************************
*
*  Module Name:   hwref.h
*
*  Description:   Definitions internal to Hardware management layer
*    
*  Owners:	  ECX IC Firmware Team
*    
*  Notes:	  1. This file should only be referenced by the hardware
*		     management layer.
*
***************************************************************************/


/***************************************************************************
* Macros for general purposes
***************************************************************************/

/***************************************************************************
* Miscellaneous
***************************************************************************/
/*	Maximum possible TCBs					*/
#define  MAX3_TCBS	1536	/* 3 64k(x2 byte) srams		*/
#define  MAX21_TCBS	1280	/* 2 64k(x2), 1 32k(x2) srams	*/
#define  MAX2_TCBS	1024	/* 2 64k(x2) srams		*/
#define  MAX11_TCBS	768	/* 2 64k(x2), 1 32k(x2) srams	*/
#define  MAX1_TCBS	512	/* 1 64k(x2) sram		*/

#define MIN_PAYLOAD_SIZE 128
#define MAX_PAYLOAD_SIZE 2048

#endif /* _HWREF_H */
