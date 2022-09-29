/**************************************************************************
 *									  *
 * 		 Copyright (C) 1993 Silicon Graphics, Inc.	          *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#include "r10000-setup.h"

	.text
	.ent	ip25Config
ip25Config:	
	.dword	DWORD_SWAP(		+\
		IP25C_R10000_KSEG0CA(5)	+ \
		IP25C_R10000_DEVNUM(0)	+ \
		IP25C_R10000_CPRT(0)	+ \
		IP25C_R10000_PER(0)	+ \
		IP25C_R10000_PRM(0)	+ \
		IP25C_R10000_SCD(2)	+ \
		IP25C_R10000_SCBS(1)	+ \
		IP25C_R10000_ME(1)	+ \
		IP25C_R10000_SCS(1)	+ \
		IP25C_R10000_SCCD(1)	+ \
		IP25C_R10000_SCCT(0x2)	+ \
		IP25C_R10000_ODSC(0)	+ \
		IP25C_R10000_ODSYS(0)	+ \
		IP25C_R10000_CTM(0))
	
	.dword	IP25C_SCC(0x0a)
	.dword	0
	.dword	0

	.byte	0xe8,0x9b,0xd6,0x02	/* EBUS */
	.byte	0x01			/* Piggy back read enabled */
	.byte	22
	.byte	0			/* IW_TRIG */
	.byte	0			/* RR_TRIG */

	.byte	0,0,0,0			/* CPU Frequency */
	.byte	0xe8,0x9b,0xd6,0x02	/* RTC Frequency */

	.byte	0,0
	.byte	1			/* ECC - enabled */
	.byte	0,0,0,0,0,0,0,0
	.byte	0,0,0,0,0,0,0,0
	.byte	0,0,0,0,0,0,0,0
	.byte	0,0,0,0,0,0,0,0

	.end	ip25Config
