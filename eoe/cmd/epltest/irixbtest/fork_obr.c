/*
 * Copyright 1993, Silicon Graphics, Inc. 
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

static char DIR1[] = "fork_obr_dir1";
static char DIR2[] = "fork_obr_dir2";
static char CORE1[] = "fork_obr_dir1/core";
static char CORE2[] = "fork_obr_dir2/core";
static char OD1[] = "fork_obr_od1";
static char OD2[] = "fork_obr_od2";
static char COMM[] = "fork_obr_comm";
static char cmdstring[MSGLEN];
static int  fork_obr1(void);

int
fork_obr(void)
{
	char str[MSGLEN];        /* used to write to logfiles */
	int fail = 0;            /* set when a test case fails */
	int incomplete = 0;      /* set when a test case is incomplete */
	char testname[SLEN];     /* used to write to logfiles */
	char name[SLEN];            
	short num = 1;             
	
	strcpy(testname,"fork_obr");
	strcpy(name, "pos01");
	
	/*
	 * Write formatted info to raw log.
	 */   
	RAWLOG_SETUP(testname, 1);
	flush_raw_log();   
	/*
	 * Call function 
	 */    
	switch ( fork_obr1() ) {
		/*
		 * Write formatted result to raw log.
		 */    
		case PASS:      /* Passed */
		RAWPASS( num, name );
		break;
		case FAIL:      /* Failed */
		RAWFAIL(  num, name, "Unaccessed bytes should be null" );
		fail = 1;
		break;
		default:
		RAWINC( num, name );
		incomplete = 1;
		break;  /* Incomplete */
	}
	flush_raw_log();
	
	/*
	 * Return 0, 1, or 2 to calling function, which records
	 * result in summary log.  If ANY case failed or was
	 * incomplete, the whole test is recorded as failed or
	 * incomplete. Incomplete supercedes fail.
	 */    
	if (incomplete)
		return(INCOMPLETE);
	if (fail)
		return(FAIL);
	return(PASS);
}

static int
fork_obr1(void)
{
	char namestring[MSGLEN];     /* Used in error messages. */
	int forkval1 = 0;
	int forkval2 = 0;
	
	cap_unlink(CORE1);
	cap_unlink(CORE2);
	cap_rmdir(DIR1);
	cap_rmdir(DIR2);
	cap_unlink(OD1);
	cap_unlink(OD2);
	
	cap_mkdir(DIR1, 0777);
	cap_mkdir(DIR2, 0777);
	if (  ( forkval1 = fork() ) == -1  ) {
		w_error(SYSCALL, namestring, err[F_FORK], errno);
		return(INCOMPLETE);
	}
	if ( forkval1 == 0 ) {
		if (  ( forkval2 = fork() ) == -1  ) {
			w_error(SYSCALL, namestring, err[F_FORK], errno);
			return(INCOMPLETE);
		}
		if ( forkval2 == 0 ) {
			if ( cap_chdir(DIR2) == -1) {
				w_error(SYSCALL, namestring,
					err[F_CHDIR], errno);
				return(INCOMPLETE);
			}
			abort();
		}
		else {
			if ( cap_chdir(DIR1) == -1) {
				w_error(SYSCALL, namestring, 
					err[F_CHDIR], errno);
				return(INCOMPLETE);
			}	
			abort();
		}
	}
	else {
		sleep(2);
		sprintf(cmdstring, "od -x %s > %s \n", CORE1, OD1);
		system(cmdstring);
		sprintf(cmdstring, "od -x %s > %s \n", CORE2, OD2);
		system(cmdstring);	
		sprintf(cmdstring, "comm %s %s > %s \n", OD1, OD2, COMM);
		system(cmdstring);		
	}
	return(PASS);
}
