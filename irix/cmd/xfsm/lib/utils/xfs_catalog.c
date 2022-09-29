/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1995, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#ident "$Revision: 1.1 $"


#include <stdio.h>

enum {
	XFSM_CAT_DK_DISK_ACCESS1 = 1,
	XFSM_CAT_DK_DISK_ACCESS2,

	XFS_CAT_MAX
};

char	*xfsm_cat_msgs[XFS_CAT_MAX];


