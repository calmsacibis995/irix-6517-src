/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1986,1988, Silicon Graphics, Inc.          *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#include <stdio.h>
#include <locale.h>
#include <pfmt.h>

main()
{
        (void)setlocale(LC_ALL, "");
        (void)setcat("uxcore");
        (void)setlabel("UX:priocntl");

	pfmt(stderr, MM_ERROR,
		"uxsgicore:148:This command is not supported in this release.\n");
}
