/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1990, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 *  PP1 - BackEnd  Diagnostics.
 *
 *  $Revision: 1.16 $
 */

#include <math.h>
#include <libsc.h>
#include "uif.h"
#include "sys/mgrashw.h"
#include "ide_msg.h"
#include "mgras_diag.h"


/**************************************************************************/



/*ARGSUSED*/
int
mgras_DCBus_Test(int argc, char **argv)
{
	 ushort_t status;
	 __uint32_t errors = 0;

#if 0
	/* 1. Read CMAP Revisions (Both CMAPS) */
	CmapRev = mgras_CmapRevision();
	if (CmapRev == CMAP_REV) 
		msg_printf(VRB, "CMAP's Revisions %d\n" ,CmapRev);
	else 
		msg_printf(ERR, "Cmap Revision Mismatch\n");

	/* 2. Read PP1 Revision */
	PP1Rev = 0xdeadbeef;
	mgras_xmapSetPP1Select(mgbase, MGRAS_XMAP_SELECT_PP1(0));
	mgras_xmapGetConfig(mgbase, PP1Rev);
	msg_printf(VRB, "XMAP Revision %x\n" ,PP1Rev);

	/* 3. Read VC3 Revision */
	mgras_vc3GetReg(mgbase, VC3_CONFIG, VC3Rev);
	VC3Rev >>=6;
	msg_printf(VRB, "VC3 Revision %x\n" ,VC3Rev);
#endif

	
	status = _mgras_CmapDataBusTest(0);
	if (status) {
		msg_printf(ERR, "%s Test Failed while running %s on Cmap%d.\n" ,argv[0], "_mgras_CmapDataBusTest", 0);
	}
	status = _mgras_CmapDataBusTest(1);
	if (status) {
		msg_printf(ERR, "%s Test Failed while running %s on Cmap%d.\n" ,argv[0], "_mgras_CmapDataBusTest", 0);
	}

	status = mgras_VC3InternalRegTest();
	if (status) {
		msg_printf(ERR, "%s Test Failed while running %s.\n" ,argv[0], "mgras_VC3InternalRegTest");
	}

	status = mgras_VC3DataBusTest();
	if (status) {
		msg_printf(ERR, "%s Test Failed while running %s.\n" ,argv[0], "mgras_VC3DataBusTest");
	}

	status = _mgras_XmapDcbRegTest(0);
	if (status) {
		msg_printf(ERR, "%s Test Failed while running %s on Xmap%d.\n" ,argv[0], "_mgras_XmapDcbRegTest", 0);
	}
	status = _mgras_XmapDcbRegTest(1);
	if (status) {
		msg_printf(ERR, "%s Test Failed while running %s on Xmap%d.\n" ,argv[0], "_mgras_XmapDcbRegTest", 1);
	}

	return(errors);
}

__uint32_t 
_mgras_PP1PathSelect( ushort_t WhichPP1)
{
	msg_printf(DBG, "MGRAS PP1 Select Path  %d\n" ,WhichPP1);

	return 0;
}

__uint32_t 
mgras_PP1Select(int argc, char **argv)
{
	 __uint32_t Path;
	
	if (argc != 2) {
		msg_printf(SUM, "usage: %s mgras_pp1_select \n", argv[0]);
		return -1;
	}

	atohex(argv[1], (int*)&Path);
	_mgras_PP1PathSelect(Path);

	return 0;
}
