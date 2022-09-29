/*
 * Copyright 1991, Silicon Graphics, Inc. 
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or 
 * duplicated in any form, in whole or in part, without the prior written 
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions 
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or 
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished - 
 * rights reserved under the Copyright Laws of the United States.
*/
#include "irixbtest.h"

int
brk_obr(void)
{
	char *endds;
	char *endds2;
	char str[MSGLEN];        /* used to write to logfiles */
	char testname[SLEN];     /* used to write to logfiles */
	char name[SLEN];            
	char namestring[MSGLEN];     /* Used in error messages. */
	short num = 1;             

	strcpy(testname,"brk_obr");
	strcpy(name, "pos01");
	/*
	 * Write formatted info to raw log.
	 */   
	RAWLOG_SETUP(testname, 1);
	flush_raw_log();   

	endds = (char *)sbrk(0);
	if (brk((void *)(endds + 10000000)) < 0) {
		w_error(SYSCALL, namestring, err[F_BRK], errno);
		RAWINC( num, name );
		flush_raw_log();   
		return(INCOMPLETE);
	}
	endds2 = (char *)sbrk(0);
	while (endds <= endds2) {
		if (*endds++ != '\0') {
			RAWFAIL(  num, name, "New memory should be null" );
			flush_raw_log();   
			return(FAIL);
		}
	}
	RAWPASS( num, name );
	return(PASS);
}
