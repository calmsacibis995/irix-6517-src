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

#include "cap.h"

/*
 * setpgrp_pcap.c -- Capability tests for setpgrp() system call.
 */
static int setpgrp_pcap1(struct capparms_3 *parmsptr);
static int setpgrp_pcap2(struct capparms_2 *parmsptr);

/* 
 * Description strings, corresponding with test cases, 
 * to be printed to the raw log if that case fails.
 */
static char *setpgrp_ddesc[] = 
{ 
  "Suser, Effe",
  "Suser, Perm",
  "Not suser, Effe",
  "Not suser, Perm",
};


/*
 * Each structures contains information for one test case.
 * Variables are uids of Subject and Object processes,
 * whether or not the subject is already a session leader,
 * and whether the pid argument is that of the current process
 * or a different process.  Different test functions are called
 * for cases that require an extra process and cases that do not.
 * Tests are currently coded to expect success when the
 * subject is a session leader, but this may be changed by 
 * a future bug fix.
 */
static struct capparms_3
setpgrp_pinfo1[] = 
{
/*
 * name    #  Sruid  Seuid  prior_setsid?  success?  errno
 */
  "pos00", 0, SUPER, SUPER,  FAIL,        PASS ,      0,
	"CAP_SETGID+e",
  "pos01", 1, SUPER, SUPER,  FAIL,        PASS ,      0,
	"CAP_SETGID+p",
  "pos02", 2, TEST1, TEST1,  FAIL,        PASS ,      0,
	"CAP_SETGID+e",
  "pos03", 3, TEST1, TEST1,  FAIL,        PASS ,      0,
	"CAP_SETGID+p"
};

int
setpgrp_pcap(void)
{
    char str[MSGLEN];        /* used to write to logfiles */
    char testname[SLEN];     /* used to write to logfiles */
    char *name;              /* holds casename from struct */
    short num;               /* holds casenum from struct */
    short ncases1 = 4;       /* number of test cases using setpgrp_pcap1 */
    short i = 0;             /* test case loop counter */
    int fail = 0;            /* set when a test case fails */
    int incomplete = 0;      /* set when a test case is incomplete */
    int retval;              /* return value of test func */
    strcpy(testname,"setpgrp_pcap");
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
	 * Call function setpgrp_pcap1 for each test case that does not
	 * require an extra process.
	 */    
	retval = setpgrp_pcap1(&setpgrp_pinfo1[i]);
	num  = setpgrp_pinfo1[i].casenum; 
        name = setpgrp_pinfo1[i].casename;
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
static int setpgrp_pcap1(struct capparms_3 *parmsptr)
{
    char buf[4];                 /* Used in pipe read/writes. */
    char testname[SLEN];         /* used to write to logfiles */
    char namestring[MSGLEN];     /* Used in error messages. */
    pid_t forkval1 = 0;          /* O's pid returned to parent */ 
    pid_t retval = 0;            /* Return value of test call. */
    int status;                  /* For wait. */   
    cap_t cptr;		         /* Pointer to cap label */
   
    strcpy(testname,"setpgrp_pcap");
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
     * Create capability label and set Subject.
     */
    if ( ( cptr = cap_from_text(parmsptr->cflag) ) == (cap_t) NULL  ) {
	w_error(SYSCALL, namestring, err[F_CREPLABEL], errno);
	exit(INCOMPLETE);
    }	
    if ( cap_set_proc(cptr) == -1  ) {
	w_error(SYSCALL, namestring, err[F_SETPLABEL], errno);
	exit(INCOMPLETE);
     }	
    cap_free(cptr);
    /*
     * Set ruid and euid to test value and verify.
     */
    if (  ( setreuid(parmsptr->ruid, parmsptr->euid) ) == -1  ) {
	w_error(SYSCALL, namestring, err[SETREUID_S], errno);
	exit(INCOMPLETE);
    }	
    if ( (getuid() != parmsptr->ruid)||(geteuid() != parmsptr->euid) ){
	w_error(GENERAL, namestring, err[BADUIDS_S], 0);
	exit(INCOMPLETE);
    }	
    
    /*
     * If required for this case, do a prior setsid. 
     */
    if (parmsptr->flag == PASS ) {
	setsid();
    }
    /*
     * Here's the test: Call setpgrp.
     * For positive cases, retval should be same as pid.
     */
    if (parmsptr->expect_success == PASS ) {
	if (  ( retval = setpgrp() ) == -1 ){
	    w_error(SYSCALL, namestring, err[TESTCALL], retval);
	    exit(FAIL);
	} 
	if (retval != getpid()) {
	    w_error(PRINTNUM, namestring, err[TEST_RETVAL], retval);
	    exit(FAIL);
	}
	exit(PASS);
    }
    
    /*
     * For negative test cases, retval should be -1 and
     * errno should equal parmsptr->expect_errno.
     */
    else {
	if (  ( retval = setpgrp() ) != -1 ){
	    w_error(PRINTNUM, namestring, err[TEST_RETVAL], retval);
	    exit(FAIL);
	}
	if (errno != parmsptr->expect_errno) {
	    w_error(SYSCALL, namestring, err[TEST_ERRNO], errno);
	    exit(FAIL);
	}
	exit(PASS);
    }
    return(PASS);
}

