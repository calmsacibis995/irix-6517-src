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
	.dword	DWORD_SWAP(		+ \
		R10000_KSEG0CA(5)	+ \
		R10000_DEVNUM(0)	+ \
		R10000_CPRT(0)		+ \
		R10000_PER(0)		+ \
		R10000_PRM(PRM-1)	+ \
		R10000_SCD(SCD)		+ \
		R10000_SCBS(1)		+ \
		R10000_SCCE(0)		+ \
		R10000_ME(1)		+ \
		R10000_SCS(SCS)		+ \
		R10000_SCCD(SCCD)	+ \
		R10000_SCCT(SCCT)	+ \
		R10000_ODSC(0)		+ \
		R10000_ODSYS(0)		+ \
		R10000_CTM(0))
	
	.dword	IP25C_SCC(0x0a)
	.dword	0
	.dword	0
	.dword	0xaaaaaaaaaaaaaaaa
	.dword	0xaaaaaaaaaaaaaaaa
	.dword	0x5555555555555555	
	.dword	0x5555555555555555	
	.end	ip25Config
