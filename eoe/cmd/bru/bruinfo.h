/************************************************************************
 *									*
 *			Copyright (c) 1984, Fred Fish			*
 *			     All Rights Reserved			*
 *									*
 *	This software and/or documentation is protected by U.S.		*
 *	Copyright Law (Title 17 United States Code).  Unauthorized	*
 *	reproduction and/or sales may result in imprisonment of up	*
 *	to 1 year and fines of up to $10,000 (17 USC 506).		*
 *	Copyright infringers may also be subject to civil liability.	*
 *									*
 ************************************************************************
 */


/*
 *  FILE
 *
 *	bruinfo.h    information about bru invocation
 *
 *  SCCS
 *
 *	@(#)bruinfo.h	9.11	5/11/88
 *
 *  SYNOPSIS
 *
 *	#include "bruinfo.h"
 *
 *  DESCRIPTION
 *
 *	This structure contains generally useful information about
 *	the current bru invocation.
 *
 */

struct bru_info {
    long bru_time;		/* Time bru was invoked */
    int bru_uid;		/* Real user ID of invoking user */
    int bru_gid;		/* Real group ID of invoking user */
    char *bru_name;		/* Invocation name */
    char *bru_tty;		/* Name of controlling tty */
    char *bru_tmpdir;		/* Prefered directory for temporary files */
};

