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
#include "dac.h"

/*
 * chown_dac.c -- DAC tests for chown() system call.
 */
static int chown_dac1(struct chown_dparm1 *parmsptr); 

/*
 * Each structure contains information for one test case.
 * Variables are uids of Subject and Object processes,
 * whether the test call is expected to succeed (positive case) or fail
 * (negative case), and errno expected in each negative case. 
 */
static struct chown_dparm1
chown_dparms[] = 
{
/*					 new_   new_
 *name    #  c_flag  Sruid  Seuid  Ouid   Ogid   Ouid   Ogid   success  errno
 */
"pos00",  0,     1,  SUPER, SUPER, TEST0, UNCHG, TEST1, UNCHG, PASS,      0,
"pos01",  1,     0,  TEST2, TEST2, TEST2, UNCHG, TEST1, UNCHG, PASS,      0,
"neg01",  2,     0,  TEST0, TEST0, TEST2, UNCHG, TEST3, UNCHG, FAIL,   EPERM,
"neg00",  0,     0,  SUPER, SUPER, TEST0, UNCHG, TEST1, UNCHG, PASS,   EPERM,
};

/* 
 * Description strings, corresponding with test cases, 
 * to be printed to the raw log if that case fails.
 */
static char *chown_ddesc[] = 
{ 
"Seuid super, Oeuid differ",	/* 0 */
"Seuid = Oeuid",		/* 1 */
"Seuid != Oeuid",		/* 2 */
};

int
chown_dac(void) 
{
	char str[MSGLEN];        /* used to write to logfiles */
	char testname[SLEN];     /* used to write to logfiles */
	short ncases1 = 3;       /* number of test cases */
	int fail = 0;            /* set when test case fails */
	int incomplete = 0;      /* set when test case incomplete */
	int retval;              /* value returned on function call */
	short i = 0;             /* test case loop counter */
	
	strcpy(testname,"chown_dac");
	/*
	 * Write formatted info to raw log.
	 */    
	RAWLOG_SETUP(testname, (ncases1) );
	
	for (i = 0; i < (ncases1); i++) {
		/*
		 * Flush output streams
		 */    
		flush_raw_log(); 
		/*
		 * Call chown_dac1()
		 */    
		retval = chown_dac1(&chown_dparms[i]);
		/*
		 * Write formatted result to raw log.
		 */    
		switch ( retval ) {
			case PASS:      /* Passed */
			RAWPASS(chown_dparms[i].casenum, chown_dparms[i].casename);
			break;
			case FAIL:      /* Failed */
			RAWFAIL(chown_dparms[i].casenum, chown_dparms[i].casename,
				chown_ddesc[i]);
			fail = 1;
			break;
			default:
			RAWINC(chown_dparms[i].casenum, chown_dparms[i].casename);
			incomplete = 1;
			break;  /* Incomplete */
		}
	}
	/*
	 * Return 0, 1, or 2 to calling function, which records
	 * result in summary log.  If ANY case failed or was
	 * incomplete, the whole test is recorded as failed or
	 * incomplete.
	 */    
	if (incomplete)
		return(INCOMPLETE);
	if (fail)
		return(FAIL);
	return(PASS);
}


/* 
 * Test function.
 */
static int chown_dac1(struct chown_dparm1 *parmsptr) 
{
	char testname[SLEN];         /* used to write to logfiles */
	char namestring[MSGLEN];     /* Used in error messages. */
	char file[NLEN + NLEN];      /* File name passed to chmod(). */
 	int fildes;                  /* File descriptor.*/
	pid_t forkval = 0;           /* pid returned to parent */ 
	pid_t retval = 0;            /* Return value of test call. */
	int status;		     /* For wait. */   
	
	strcpy(testname,"chown_dac");
	sprintf(namestring, "%s, case %d, %s:\n   ", testname,
		parmsptr->casenum, parmsptr->casename);

	/*
	 * Create names for temp file and directory
	 */
	sprintf(file,  "%s%d", testname, parmsptr->casenum);
 	/*
	 * Create a file, close it.
	 */
	cap_unlink(file);
	if ( (fildes = creat(file, 0700)) == -1 ) {
		w_error(SYSCALL, namestring, err[F_CREAT], errno);
		exit(INCOMPLETE);
	}	
	close(fildes);
	if ( cap_chown(file, parmsptr->Ouid, parmsptr->Ogid) ) {
		w_error(SYSCALL, namestring, err[F_CHOWN], errno);
		exit(INCOMPLETE);
	}

	/*
	 * Fork a child to do some testing.
	 */
	if (( forkval = fork() ) == -1  ) {
		w_error(SYSCALL, namestring, err[F_FORK], errno);
		return(INCOMPLETE);
	}
		
	/* 
	 * This is the parent after forking.
	 */
	if ( forkval )  { 
		/* 
		 * This is the parent.	Wait for child and return
		 * 2 on unexpected error. Otherwise, return child's 
		 * exit code.
		 */
		if ( wait(&status) == -1 ) {
			w_error(SYSCALL, namestring, err[F_WAIT], errno);
			return(INCOMPLETE);
    		}
		if (!WIFEXITED(status)) {
			w_error(GENERAL, namestring, err[C_NOTEXIT], 0);
			return(INCOMPLETE);
		}
		if ( WEXITSTATUS(status) ) {
		    w_error(GENERAL, namestring, err[C_BADEXIT], 0);
		    exit(INCOMPLETE);
    		}

		/* 
		 * If we got here, child exited with status 0
		 */
		cap_unlink(file);
		return(PASS);
	} /*end parent*/
	

	/* 
	 * This is the child process. 
	 * Set ruid and euid to test value and verify.
	 */
	if ( ( cap_setreuid(parmsptr->Sruid, parmsptr->Seuid) ) == -1  ) {
		w_error(SYSCALL, namestring, err[SETUID_S], errno);
		cap_unlink(file);
		exit(INCOMPLETE);
	}	
	if ( ( getuid() != parmsptr->Sruid) ||
		(geteuid() != parmsptr->Seuid) ) { 
		w_error(GENERAL, namestring, err[BADUIDS_S], 0);
		cap_unlink(file);
		exit(INCOMPLETE);
	}
	/*
	 * Here's the test: Call chown( uid, gid)
	 */
	if (parmsptr->c_flag != 0) {
		retval = cap_chown(file, parmsptr->new_Ouid, parmsptr->new_Ogid);
	}else{
		retval = chown(file, parmsptr->new_Ouid, parmsptr->new_Ogid);
	}

	/*
	 * Positive test cases
	 */
	if (parmsptr->expect_success == PASS ) {


		if ( retval == -1 ) {
			w_error(SYSCALL, namestring, err[TESTCALL], errno);
			exit(FAIL);
		}
		if ( retval ) {
			w_error(PRINTNUM, namestring, err[TEST_RETVAL], retval);
			exit(FAIL);
		}
	}
	else {
	/*
	 * For negative test cases, retval should be -1 and
	 * errno should equal parmsptr->expect_errno.
	 */

		if ( retval != -1 ) {
			w_error(PRINTNUM, namestring, err[TEST_RETVAL], retval);
			exit(FAIL);
		}
		if (errno != parmsptr->expect_errno) {
			w_error(SYSCALL, namestring, err[TEST_ERRNO], errno);
			exit(FAIL);
		}
	} /*endif*/
	exit(PASS);
	return(PASS);
}
