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

#include "dac.h"

/*
 * setregid_proc.c -- PROC tests for setregid() system call.
 */
int setregid_proc1(struct setregid_pinfo1 *parmsptr);

/*
 * Each structure contains information for one test case.
 * Variables are current uids and gids of Subject processes
 * real and effective gid arguments to test call, expected
 * outcome and errno.
 */
static struct setregid_pinfo1 
setregid_pinfo1[] = 
{
/*
 * name    #  c_flag  Sruid  Seuid  Srgid0  Segid0  Srgid1  Segid1  success?  errno
 */
  "pos00", 0,     1,  SUPER, SUPER, TESTG0, TESTG0, TESTG1, TESTG1,   PASS ,      0,
  "pos01", 1,     0,  TEST1, TEST0, TESTG1, TESTG0, TESTG1, TESTG0,   PASS ,      0,
  "pos02", 2,     0,  TEST1, TEST0, TESTG1, TESTG0, TESTG0, TESTG1,   PASS ,      0,
  "pos03", 3,     0,  TEST1, TEST0, TESTG1, TESTG0, TESTG0, TESTG0,   PASS ,      0,
  "pos04", 4,     0,  TEST1, TEST0, TESTG1, TESTG0, TESTG1, TESTG1,   PASS ,      0,
  "neg00", 5,     0,  TEST1, TEST0, TESTG1, TESTG0, TESTG2, TESTG0,   FAIL,   EPERM,
  "neg01", 6,     0,  TEST1, TEST0, TESTG1, TESTG0, TESTG1, TESTG2,   FAIL,   EPERM
};
 
/* 
 * Description strings, corresponding with test cases, 
 * to be printed to the raw log if that case fails.
 */
static char *setregid_ddesc1[] = 
{ 
  "Suser changing both GIDs",
  "Unpriv user, no change",
  "Unpriv user, swapping EGID and RGID",
  "Unpriv user, changing EGID to RGID",
  "Unpriv user, changing RGID to EGID",
  "Unpriv user, unauthorized change to RGID",
  "Unpriv user, unauthorized change to EGID"
};

int
setregid_proc(void)
{
    short ncases1 = 7;       /* number of test cases using setregid_proc1 */
    int fail = 0;            /* set when a test case fails */
    int incomplete = 0;      /* set when a test case is incomplete */
    short i = 0;             /* test case loop counter */
    char str[MSGLEN];        /* used to write to logfiles */
    char testname[SLEN];     /* used to write to logfiles */

    strcpy(testname,"setregid_proc");
    /*
     * Write formatted info to raw log.
     */   
    RAWLOG_SETUP(testname, ncases1 );
    /*
     * Call function setregid_proc1 for each test case.
     */    
    for (i = 0; i < ncases1; i++) {
	/*
	 * Flush output streams.
	 */    
	flush_raw_log();   
	switch (setregid_proc1(&setregid_pinfo1[i]) ) {
	/*
	 * Write formatted result to raw log.
	 */    
	case PASS:      /* Passed */
	    RAWPASS(setregid_pinfo1[i].casenum, setregid_pinfo1[i].casename);
	    break;
	case FAIL:      /* Failed */
	    RAWFAIL(setregid_pinfo1[i].casenum, setregid_pinfo1[i].casename,
		    setregid_ddesc1[i]);
	    fail = 1;
	    break;
	default:
	    RAWINC(setregid_pinfo1[i].casenum, setregid_pinfo1[i].casename);
	    incomplete = 1;
	    break;  /* Incomplete */
	}
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
setregid_proc1(struct setregid_pinfo1 *parmsptr)
{
    char testname[SLEN];         /* used to write to logfiles */
    char namestring[MSGLEN];     /* Used in error messages. */
    pid_t forkval1 = 0;          /* O's pid returned to parent */ 
    pid_t retval = 0;            /* Return value of test call. */
    int status;                  /* For wait. */   
    
    strcpy(testname,"setregid_proc");
    sprintf(namestring, "%s, case %d, %s:\n   ", testname,
	     parmsptr->casenum, parmsptr->casename);
    /*
     * Fork Subject
     */
    if ( ( forkval1 = fork() ) == -1 ) {
	w_error(SYSCALL, namestring, err[F_FORK], errno);
	return(INCOMPLETE);
    }

    /* 
     * This is the parent after forking Subject.
     */
    if ( forkval1 )  { 
	/*
	 * Wait for Child. 
	 */

	if ( wait(&status) == -1 ) {
	    w_error(SYSCALL, namestring, err[F_WAIT], errno);
	    return(INCOMPLETE);
    	}
	if ( !WIFEXITED(status) ) {
	    w_error(GENERAL, namestring, err[C_NOTEXIT], 0);
	    return(INCOMPLETE);
    	}

	return(WEXITSTATUS(status));
    }


    /* 
     * This is S, the Subject process. 
     */
    /*
     * Set rgid and egid to first value and verify.
     */
    if ( ( cap_setregid(parmsptr->Srgid0, parmsptr->Segid0) ) == -1 ) {
	w_error(SYSCALL, namestring, err[SETREGID_S], errno);
	exit(INCOMPLETE);
    }	
    if ( ( getgid() != parmsptr->Srgid0 ) || ( getegid() != parmsptr->Segid0)) {
	w_error(GENERAL, namestring, err[BADGIDS_S], 0);
	exit(INCOMPLETE);
    }	
    
    /*
     * Set ruid and euid to test value and verify.
     */
    if ( ( cap_setreuid(parmsptr->Sruid, parmsptr->Seuid) ) == -1 ) {
	w_error(SYSCALL, namestring, err[SETREUID_S], errno);
	exit(INCOMPLETE);
    }	
    if ( ( getuid() != parmsptr->Sruid ) || ( geteuid() != parmsptr->Seuid ) ) {
	w_error(GENERAL, namestring, err[BADUIDS_S], 0);
	exit(INCOMPLETE);
    }	
    
    /*
     * Here's the test: Call setregid(Srgid1, Segid1).
     * For positive cases, retval should be zero.
     */
    if (parmsptr->c_flag != 0) {
	retval = cap_setregid(parmsptr->Srgid1, parmsptr->Segid1);
    }else{
	retval = setregid(parmsptr->Srgid1, parmsptr->Segid1);
    }

    if ( parmsptr->expect_success == PASS  ) {
	if ( retval == -1 ) {
	    w_error(SYSCALL, namestring, err[TESTCALL], errno);
	    exit(FAIL);
	} 
	if ( retval != 0 ) {
	    w_error(PRINTNUM, namestring, err[TEST_RETVAL],  retval);
	    exit(FAIL);
	}
	exit(PASS);
    }
    
    /*
     * For negative test cases, retval should be -1 and
     * errno should equal parmsptr->expect_errno.
     * If not, write to stderr or log.
     */
    if ( retval != -1 ) {
	w_error(PRINTNUM, namestring, err[TEST_RETVAL], retval);
	exit(FAIL);
    }
    if ( errno != parmsptr->expect_errno ) {
	w_error(SYSCALL, namestring, err[TEST_ERRNO], errno);
	exit(FAIL);
    }
    exit(PASS);
    return(PASS);
}
