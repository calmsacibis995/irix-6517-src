/**************************************************************************
 *									  *
 * 		 Copyright (C) 1994, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident "$Revision: 1.3 $ $Author: jwag $"

/*
 * Initialize global variable so bcopy/bzero can decide best algorithm
 * for block copy/zero.
 */
#include "synonyms.h"
#include <sys/types.h>
#include <sys/syssgi.h>

int	_blk_fp = -1;

void
_blk_init(void)
{
	_blk_fp = (int)syssgi(SGI_USE_FP_BCOPY);
	if (_blk_fp == -1)
		_blk_fp = 0;
}
