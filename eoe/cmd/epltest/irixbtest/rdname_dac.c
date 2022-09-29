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
#include <sys/syssgi.h>
#include <sys/proc.h>

/*
 * rdname_dac.c -- DAC tests for syssgi(SGI_RDNAME) system call.
 */
static int rdname_dac1(struct dacparms_2 *parmsptr);


/*
 * Each structures contains information for one test case.
 * Variables are uids of Subject and Object processes.
 */
static struct dacparms_2
rdname_dinfo[] = { "pos00", 0, 0, SUPER, SUPER, PASS , 0,
		   "pos01", 1, 0, TEST0, TEST0, PASS , 0  };

/*
 * Each structures contains information for one test case.
 * Variables are uids of Subject and Object processes.
 */
static char 
*rdname_ddesc[] = { "S not suser, O is process 1",
		         "S is suser, O is process 1" };

int
rdname_dac(void)
{
    short ncases = 2;        /* number of total test cases */
    int fail = 0;            /* set when a test case fails */
    int incomplete = 0;      /* set when a test case is incomplete */
    short i = 0;             /* test case loop counter */
    char str[MSGLEN];        /* used to write to logfiles */
    char testname[SLEN];     /* used to write to logfiles */

    strcpy(testname,"rdname_dac");
    /*
     * Write formatted info to raw log.
     */    
    RAWLOG_SETUP( testname,  ncases );
		      
    /*
     * Call function for each test case.
     */    
    for (i = 0; i < ncases; i++) {
	/*
	 * Flush output streams before calling a function that forks! 
	 */    
	flush_raw_log();   
	switch ( rdname_dac1(&rdname_dinfo[i]) ) {
	/*
	 * Write formatted result to raw log.
	 */    
	case PASS:      /* Passed */
	    RAWPASS(rdname_dinfo[i].casenum, rdname_dinfo[i].casename);
	    break;
	case FAIL:      /* Failed */
	    RAWFAIL(rdname_dinfo[i].casenum, rdname_dinfo[i].casename,
		    rdname_ddesc[i]);
	    fail = 1;
	    break;
	default:
	    RAWINC(rdname_dinfo[i].casenum, rdname_dinfo[i].casename);
	    incomplete = 1;
	    break;  /* Incomplete */
	}
	flush_raw_log();
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

static int
rdname_dac1(struct dacparms_2 *parmsptr)
{
    char testname[SLEN];         /* used to write to logfiles */
    char namestring[MSGLEN];     /* Used in error messages. */
    char buffer[MSGLEN];         /* Place for rdname to put name */
    pid_t forkval = 0;           /* pid returned by fork call */ 
    pid_t retval = 0;            /* Return value of test call. */
    int status;                  /* For wait. */   

    strcpy(testname,"rdname_dac");
    sprintf(namestring, "%s, case %d, %s:\n   ", testname,
	     parmsptr->casenum, parmsptr->casename);

    /*
     * Fork Child.
     */
    if (  ( forkval = fork() ) == -1  ) {
	w_error(SYSCALL, namestring, err[F_FORK], errno);
	return(INCOMPLETE);
    }

    /* 
     * This is the parent after forking.
     */
    if ( forkval )  { 
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
     * This is the child (subject process). 
     */
    /*
     * Set ruid and euid to test value and verify.
     */
    if ( ( cap_setreuid(parmsptr->ruid, parmsptr->euid) ) == -1 ) {
	w_error(SYSCALL, namestring, err[SETREUID_S], errno);
	exit(INCOMPLETE);
    }	
    if ( ( getuid() != parmsptr->ruid ) || ( geteuid() != parmsptr->euid ) ){
	w_error(GENERAL, namestring, err[BADUIDS_S], 0);
	exit(INCOMPLETE);
    }	
    /*
     * Here's the test: Call syssgi(SGI_RDNAME, pid, &buffer,
     * sizeof(buffer)), with pid = 1.
     */
    retval = syssgi(SGI_RDNAME, 1, buffer, sizeof(buffer));
    
    /*
     * Positive cases:  Call to strstr() should
     * verify that the buffer contains the substring "init".
     */
    if ( parmsptr->expect_success == PASS  ) {
	if  (retval < 0 ) {
	    w_error(SYSCALL, namestring, err[TESTCALL], errno);
	    exit(FAIL);
	}
	if  (strstr(buffer, "init") == ((char *)NULL) ) {
	    w_error(SYSCALL, namestring, err[BADRDNAME], errno);
	    exit(FAIL);
	}
	exit(PASS);
    }
    
    /*
     * For negative test cases, retval should be -1 and
     * errno should equal parmsptr->expect_errno.
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
