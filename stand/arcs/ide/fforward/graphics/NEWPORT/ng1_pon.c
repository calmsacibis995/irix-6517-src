/**************************************************************************
 * 		 Copyright (C) 1988, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#include "ide_msg.h"
#include "sys/ng1hw.h"
#include "libsk.h"
#include "libsc.h"
#include "uif.h"

extern Rex3chip *REX;
extern int REXindex;

/* Preliminary power on test */

int 
ng1_pon(struct rex3chip *base)
{
	extern int test_rex3(void);

	REXindex = Ng1Index(base);
	REX = base;

	if (test_rex3()) {
		msg_printf(VRB, "REX power on test failed.\n");
		return -1;
	}

	return 0;
}
