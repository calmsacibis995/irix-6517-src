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

/*
*
*  Module Name:   ulmutil.c
*
*  Description:
*		  Code to implement utility for hardware management module
*
*  Owners:	  ULM Customer
*    
*  Notes:	  Utilities defined in this file should be generic and
*		  primitive.
*
*/

#include "hwgeneral.h"
#include "hwcustom.h"
#include "ulmconfig.h"
#include "hwref.h" 
#include "hwtcb.h"
#include "hwdef.h" 
#include "hwproto.h"


UBYTE
inMemByte(uint64_t reg)
{
	return *((uint8_t *)(reg ^ 3));
}

USHORT
inMemWord(uint64_t reg)
{
	return *((uint16_t *)(reg ^ 2));
}

void
outMemWord(uint64_t reg, USHORT value)
{
	*((uint16_t *)(reg ^ 2)) = value;
}

void
outMemByte(uint64_t reg, UBYTE value)
{
	*((uint8_t *)(reg ^ 3)) = value;
}

ushort
ULM_SWAP_SHORT(register ushort data)
{
	register u_char tmp;

	tmp = (u_char) data;
	data >>= 8;
	data |= ((ushort) tmp) << 8;
	return data;
}

/*ARGSUSED*/
void
ulm_swap8(register UBYTE *ptr, uint32_t count, void *ulm_info)
{
	void fc_swapctl(uint8_t *ptr, uint32_t count);

	fc_swapctl((uint8_t *)ptr, count);
}

/*
 * Adaptec has promised that this will only be called from sh_initialize
 * when we set SH_POWERUP_INITIALIZATION in the flags.	Therefore, it is
 * okay to just spin for now.
 */
/*ARGSUSED*/
void
ulm_delay(void *t, ULONG us)
{
	us_delay(us);
}
