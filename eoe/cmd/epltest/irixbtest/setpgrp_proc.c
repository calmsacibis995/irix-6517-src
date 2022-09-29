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
 * setpgrp_proc.c -- PROC tests for setpgrp() system call.
 */
static int setpgrp_proc0(struct dacparms_3 *parmsptr);

/* 
 * Description strings, corresponding with test cases, 
 * to be printed to the raw log if that case fails.
 */
static char *setpgrp_ddesc[] = 
{ 
  "Suser",
  "Not suser",
  "Suser, already a session leader",
  "Not suser, already a session leader",
};


/*
 * Each structures contains information for one test case.
 * Variables are uids of Subject and Object processes, and
 * whether or not the subject is already a session leader.
 * Tests are currently coded to expect success when the
 * subject is a session leader, but this may be changed by 
 * a future bug fix.
 */
static struct dacparms_3
setpgrp_pinfo0[] = 
{
/*
 * name    #  Sruid  Seuid  prior_setsid?  success?  errno
 */
  "pos00", 0, SUPER, SUPER,  FAIL,         PASS,     0,
  "pos01", 1, TEST1, TEST1,  FAIL,         PASS,     0,
  "pos02", 2, SUPER, SUPER,  PASS,         PASS,     0,
  "pos03", 3, TEST1, TEST1,  PASS,         PASS,     0,
};

int
setpgrp_proc(void)
{
    char str[MSGLEN];        /* used to write to logfiles */
    char testname[SLEN];     /* used to write to logfiles */
    char *name;              /* holds casename from struct */
    short num;               /* holds casenum from struct */
    short ncases0 = 4;	     /* number of test cases using setpgrp_proc0 */
    short i = 0;             /* test case loop counter */
    int fail = 0;            /* set when a test case fails */
    int incomplete = 0;      /* set when a test case is incomplete */
    int retval;              /* return value of test func */
    strcpy(testname,"setpgrp_proc");
    /*
     * Write formatted info to raw log.
     */   
    RAWLOG_SETUP(testname, ncases0);
    for (i = 0; i < ncases0; i++) {
	/*
	 * Flush output streams
	 */    
	flush_raw_log(); 
	/*
	 * Call function setpgrp_proc0 for each test case that does not
	 * require an extra process.
	 */    
	retval = setpgrp_proc0(&setpgrp_pinfo0[i]);
	num  = setpgrp_pinfo0[i].casenum; 
	name = setpgrp_pinfo0[i].casename;
	/*
	 * Write formatted result to raw log.
	 */    
	switch ( retval ) {
	case PASS:      /* Passed */
	    RAWPASS(num, name);
	    break;
	case FAIL:      /* Failed */
	    RAWFAIL(num, name, setpgrp_ddesc[i]);
	    fail = 1;
	    break;
	default:
	    RAWINC(num, name);
	    incomplete = 1;
	    break;  /* Incomplete */
	}
    }
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

/*
 * This function is used for test cases that do not require an extra
 * process.
 */
static int setpgrp_proc0(struct dacparms_3 *parmsptr)
{
    char buf[4];                 /* Used in pipe read/writes. */
    char testname[SLEN];         /* used to write to logfiles */
    char namestring[MSGLEN];     /* Used in error messages. */
    pid_t forkval1 = 0;          /* O's pid returned to parent */ 
    pid_t retval = 0;            /* Return value of test call. */
    int status;                  /* For wait. */   
    
    strcpy(testname,"setpgrp_proc");
    strcpy (buf, "abc");      /* Stuff to write to pipe. */
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
	    return(INCOMPLETE);
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
    if ( ( getuid() != parmsptr->ruid ) || ( geteuid() != parmsptr->euid ) ){
	w_error(GENERAL, namestring, err[BADUIDS_S], 0);
	exit(INCOMPLETE);
    }	
    
    /*
     * If required for this case, do a prior setsid. 
     */
    if ( parmsptr->flag == PASS  ) {
	setsid();
    }
    /*
     * Here's the test: Call setpgrp.
     * For positive cases, retval should be same as pid.
     */
    if ( parmsptr->expect_success == PASS  ) {
	if ( ( retval = setpgrp() ) == -1 ){
	    w_error(SYSCALL, namestring, err[TESTCALL], errno);
	    exit(FAIL);
	} 
	if ( retval != getpid() ) {
	    w_error(PRINTNUM, namestring, err[TEST_RETVAL],
		    retval);
	    exit(FAIL);
	}
	exit(PASS);
    }
    
    /*
     * For negative test cases, retval should be -1 and
     * errno should equal parmsptr->expect_errno.
     */
    else {
	if ( ( retval = setpgrp() ) != -1 ){
	    w_error(PRINTNUM, namestring, err[TEST_RETVAL],
		    retval);
	    exit(FAIL);
	}
	if ( errno != parmsptr->expect_errno ) {
	    w_error(SYSCALL, namestring, err[TEST_ERRNO], errno);
	    exit(FAIL);
	}
	exit(PASS);
    }
    return(PASS);
}
