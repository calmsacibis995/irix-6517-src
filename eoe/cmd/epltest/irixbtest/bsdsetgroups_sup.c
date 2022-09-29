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
#include <sys/param.h>

/*
 * bsdsetgroups_sup.c -- PROC tests for bsdsetgroups() system call.
 */
static int bsdsetgroups_sup1(struct dacparms_2 *parmsptr);

/*
 * Each structures contains information for one test case.
 * Variables are uids of Subject and Object processes
 */
static struct dacparms_2
bsdsetgroups_pinfo1[] = 
{
/*
 * name    #  c_flag  Sruid  Seuid   success?  errno
 */
  "pos00", 0,      1, SUPER, SUPER,   PASS,       0,
  "neg00", 1,      0, TEST1, TEST1,   FAIL,   EPERM,
  "neg00", 2,      0, SUPER, SUPER,   FAIL,   EPERM };


/* 
 * Description strings, corresponding with test cases, 
 * to be printed to the raw log if that case fails.
 */
static char *bsdsetgroups_ddesc1[] = 
{ 
  "Suser",
  "Not suser"
   };


int
bsdsetgroups_sup(void)
{
    short ncases1 = 2;       /* number of test cases */
    int fail = 0;            /* set when a test case fails */
    int incomplete = 0;      /* set when a test case is incomplete */
    short i = 0;             /* test case loop counter */
    char str[MSGLEN];        /* used to write to logfiles */
    char testname[SLEN];     /* used to write to logfiles */

    strcpy(testname,"bsdsetgroups_sup");
    /*
     * Write formatted info to raw log.
     */   
    RAWLOG_SETUP(testname, ncases1 );
    /*
     * Call function bsdsetgroups_sup1 for each test case.
     */    
    for (i = 0; i < ncases1; i++) {
	/*
	 * Flush output streams
	 */    
	flush_raw_log();   
	switch (bsdsetgroups_sup1(&bsdsetgroups_pinfo1[i]) ) {
	/*
	 * Write formatted result to raw log.
	 */    
	case PASS:      /* Passed */
	    RAWPASS(bsdsetgroups_pinfo1[i].casenum, 
		    bsdsetgroups_pinfo1[i].casename);
	    break;
	case FAIL:      /* Failed */
	    RAWFAIL(bsdsetgroups_pinfo1[i].casenum, 
		    bsdsetgroups_pinfo1[i].casename,
		    bsdsetgroups_ddesc1[i]);
	    fail = 1;
	    break;
	default:
	    RAWINC(bsdsetgroups_pinfo1[i].casenum, 
		   bsdsetgroups_pinfo1[i].casename);
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


    int gidset[] = { 1, 2, 3, 4, 5 };  /* Passed on test call */
    int ngroups = 5;                   /* Passed on test call */
/*
 * This function is used for all test cases.
 */
static int bsdsetgroups_sup1(struct dacparms_2 *parmsptr)
{
    char testname[SLEN];         /* used to write to logfiles */
    char namestring[MSGLEN];     /* Used in error messages. */
    pid_t forkval1 = 0;          /* O's pid returned to parent */ 
    pid_t retval = 0;            /* Return value of test call. */
    int status;                  /* For wait. */   
    
    strcpy(testname,"bsdsetgroups_sup");
    sprintf(namestring, "%s, case %d, %s:\n   ", testname,
	     parmsptr->casenum, parmsptr->casename);
    /*
     * Fork Subject
     */
    if (  ( forkval1 = fork() ) == -1  ) {
	w_error(SYSCALL, namestring, err[F_FORK], errno);
	return(INCOMPLETE);
    }

    /* 
     * This is the parent after forking Subject.
     */
    if (forkval1)  { 
	/*
	 * Wait for Child. 
	 */

	if ( wait(&status) == -1 ) {
	    w_error(SYSCALL, namestring, err[F_WAIT], errno);
	    return(INCOMPLETE);
    	}
	if ( !WIFEXITED(status) ) {
	    w_error(GENERAL, namestring, err[C_NOTEXIT], 0);
	    return(INCOMPLETE);;
    	}
	return(WEXITSTATUS(status));
    }


    /* 
     * This is S, the Subject process. 
     */
    /*
     * Set ruid and euid to test value and verify.
     */
    if (  ( cap_setreuid(parmsptr->ruid, parmsptr->euid) ) == -1  ) {
	w_error(SYSCALL, namestring, err[SETREUID_S], errno);
	exit(INCOMPLETE);
    }	
    if ( (getuid() != parmsptr->ruid)||(geteuid() != parmsptr->euid) ){
	w_error(GENERAL, namestring, err[BADUIDS_S], 0);
	exit(INCOMPLETE);
    }	
    
    /*
     * Here's the test: Call BSDsetgroups(ngroups, gidset).
     * For positive cases, retval should be same as ngroups.
     */
    if (parmsptr->expect_success == PASS ) {
	if (parmsptr->c_flag != 0) {
	    if (  ( retval = cap_BSDsetgroups(ngroups, gidset) ) == -1 ){
	        w_error(SYSCALL, namestring, err[TESTCALL], errno);
	        exit(FAIL);
	    }
	}else{
	    if (  ( retval = BSDsetgroups(ngroups, gidset) ) == -1 ){
	        w_error(SYSCALL, namestring, err[TESTCALL], errno);
	        exit(FAIL);
	    }
	}
	if (retval != 0) {
	    w_error(PRINTNUM, namestring, err[TEST_RETVAL], retval);
	    exit(FAIL);
	}
	exit(PASS);
    }
    
    /*
     * For negative test cases, retval should be -1 and
     * errno should equal parmsptr->expect_errno.
     */
    if (  ( retval = BSDsetgroups(ngroups, gidset) ) != -1 ){
	w_error(PRINTNUM, namestring, err[TEST_RETVAL],retval);
	exit(FAIL);
    }
    if (errno != parmsptr->expect_errno) {
	w_error(SYSCALL, namestring, err[TEST_ERRNO], errno);
	exit(FAIL);
    }
    exit(PASS);
    return(PASS);
}
