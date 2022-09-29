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

/*
 * setplabel_proc.c -- Tests for mac_set_proc() system call.
 * For mac_set_proc() to succeed, the process must be the owner of the
 * file and have search permission on the parent directory and all
 * directories above it.  If the process is owner of a directory,
 * only the owner bits are checked.  Otherwise, if the process's group id
 * equals the directory's group id, only the group bits are checked.
 * Otherwise, only the other bits are checked.
 * 
 * Structure template is defined in a header file.
 */
#include "dac.h"
#include "mac.h"

static int setplabel_proc0(struct macinfo_2 *parmsptr);

static struct macinfo_2 
setplabel_dinfo0[] = 
{
/*                                    O          S
 case case   c_flag  expect  errno    - Old -    - New -     uid
 name  num           success          msen mint  msen mint
*/
"pos00",  0,     1,  PASS,       0,  MSL, MIE,  MSH, MIE,   SUPER,
"pos01",  1,     1,  PASS,       0,  MSH, MIE,  MSL, MIE,   SUPER,
"pos02",  2,     1,  PASS,       0,  MSH, MIL,  MSH, MIH,   SUPER,
"pos03",  3,     1,  PASS,       0,  MSH, MIH,  MSH, MIL,   SUPER,

"neg00",  4,     0,  FAIL,   EPERM,  MSL, MIE,  MSH, MIE,   TEST0,
"neg01",  5,     0,  FAIL,   EPERM,  MSH, MIE,  MSL, MIE,   TEST0,
"neg02",  6,     0,  FAIL,   EPERM,  MSH, MIL,  MSH, MIH,   TEST0,
"neg03",  7,     0,  FAIL,   EPERM,  MSH, MIH,  MSH, MIL,   TEST0,

"pos04",  8,     0,  PASS,       0,  MSH, MIE,  MSH, MIE,   TEST0,
"pos05",  9,     0,  PASS,       0,  MSH, MIE, MSMH, MIE,   TEST0,
};


static char *setplabel_ddesc[] = { 
"S suser, new MSEN > old, new MINT = old",
"S suser, new MSEN < old, new MINT = old",
"S suser, new MSEN = old, new MINT > old",
"S suser, new MSEN = old, new MINT < old",

"S not suser, new MSEN > old, new MINT = old",
"S not suser, new MSEN < old, new MINT = old",
"S not suser, new MSEN = old, new MINT > old",
"S not suser, new MSEN = old, new MINT < old",

"S not suser, setting label equal to current label",
"S not suser, setting label equal to equivalent MLD label",
};

int
setplabel_proc(void)
{
    char str[MSGLEN];        /* used to write to logfiles */
    char testname[SLEN];     /* used to write to logfiles */
    short ncases0 =  10;     /* number of test cases for label changes */
    short i = 0;             /* test case loop counter */
    int fail = 0;            /* set when a test case fails */
    int incomplete = 0;      /* set when a test case is incomplete */

    strcpy(testname,"setplabel_proc");
    /*
     * Write formatted info to raw log.
     */   
    RAWLOG_SETUP(testname, ncases0);
    /*
     * Call function for each test case.
     */    
    for ( i = 0; i < ncases0; i++ ) {
	/*
	 * Flush output streams before calling a function that forks! 
	 */    
	flush_raw_log();   
	/*
	 * Call setplabel_proc0() for each test case, passing a pointer to
         * the structure containing the parameters for that case.
	 * A return value of 0 indicates PASS, 1 indicates FAIL, 2
	 * indicates the test was aborted due to an unexpected error.
	 */
	    
	switch ( setplabel_proc0(&setplabel_dinfo0[i]) ) {
	/*
	 * Write formatted result to raw log.
	 */    
	case PASS:      /* Passed */
	    RAWPASS(setplabel_dinfo0[i].casenum,
		    setplabel_dinfo0[i].casename );
	    break;
	case FAIL:      /* Failed */
	    RAWFAIL(setplabel_dinfo0[i].casenum,
		    setplabel_dinfo0[i].casename, setplabel_ddesc[i] );
	    fail = 1;
	    break;
	default:
	    RAWINC(setplabel_dinfo0[i].casenum,
		   setplabel_dinfo0[i].casename );
	    incomplete = 1;
	    break;  /* Incomplete */
	}
	flush_raw_log();
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
 * This function sets up the conditions for a given case based on the
 * parameters passed in the structure.  Then it does the test and
 * returns 0, 1, or 2 for PASS, FAIL, or INCOMPLETE, respectively.
 */
static int
setplabel_proc0(struct macinfo_2 *parmsptr)
{
    char testname[SLEN];         /* used to write to logfiles */
    char namestring[MSGLEN];     /* Used in error messages. */
    int retval = 0;              /* Return value of test call. */
    mac_t testptr;               /* MAC label pointer for test call. */
    pid_t forkval;               /* Returned value of fork call. */
    int status;                  /* For wait. */   
    int loc_errno;               /* Saves test call errno. */

    strcpy(testname,"setplabel_proc");
    sprintf(namestring, "%s, case %d, %s:\n   ", testname,
	     parmsptr->casenum, parmsptr->casename);

    /* 
     * Fork a child to do setup and test call.
     */
    if ( ( forkval = fork() ) == -1 ) {
	w_error(SYSCALL, namestring, err[F_FORK], errno);
	return(INCOMPLETE);
    }

    if ( forkval ) { 
	/* 
	 * This is the parent.	Wait for child and return
	 * 2 on unexpected error. Otherwise, return child's 
         * exit code.
	 */
	if ( wait(&status) == -1 ) {
	    w_error(SYSCALL, namestring, err[F_WAIT], errno);
	    return(INCOMPLETE);
    	}
	if ( !WIFEXITED(status)  ) {
	    w_error(GENERAL, namestring, err[C_NOTEXIT], 0);
	    return(INCOMPLETE);
    	}
	return(WEXITSTATUS(status) );
    }

    /* 
     * This is the child.  
     */
    
    
    /* 
     * As suser, set process label to initial value.
     */
    testptr = mac_get_proc();
    testptr->ml_msen_type = parmsptr->O_msen;
    testptr->ml_mint_type = parmsptr->O_mint;
    if ( cap_setplabel(testptr) ) {
	w_error(SYSCALL,namestring,err[F_SETPLABEL],errno);
	exit(INCOMPLETE);
    }
    mac_free(testptr);
    
    /* 
     * Set process euid to test uid and verify.
     */
    if ( ( cap_setreuid(-1, parmsptr->uid) ) == -1 ) {
	w_error(SYSCALL, namestring, err[SETREUID_S], errno);
	exit(INCOMPLETE);
    }	
    if ( geteuid() != parmsptr->uid ) {
	w_error(GENERAL, namestring, err[BADUIDS_S], 0);
	exit(INCOMPLETE);
    }	
    
    /*
     * Malloc a mac_label pointer for test call and
     * set up the buffer.
     */
    testptr = mac_get_proc();
    testptr->ml_msen_type = parmsptr->S_msen;
    testptr->ml_mint_type = parmsptr->S_mint;
    /*
     * Here's the test:
     */
    if (parmsptr->c_flag != 0) {
	retval = cap_setplabel(testptr);
    }else{
    	retval = mac_set_proc(testptr);
    }
    loc_errno = errno;
    mac_free(testptr);
    
    /*
     * For positive cases: retval should be 0.
     */
    if ( parmsptr->expect_success == PASS  ) {
	if ( retval == -1 ) {
	    w_error(SYSCALL, namestring, err[TESTCALL], loc_errno);
	    exit(FAIL);
	} 
	if ( retval != 0 ) {
	    w_error(PRINTNUM, namestring, err[TEST_RETVAL],retval);
	    exit(FAIL);
	}
	exit(PASS);
    }
    
    /*
     * For negative test cases, retval should be -1 and
     * errno should equal parmsptr->expect_errno.  If not, 
     * write error message. Exit 0 for PASS, 1 for FAIL.	 
     */
    if ( retval != -1 ) {
	w_error(PRINTNUM, namestring, err[TEST_RETVAL],retval);
	exit(FAIL);
    }
    if ( loc_errno != parmsptr->expect_errno ) {
	w_error(SYSCALL, namestring, err[TEST_ERRNO],loc_errno);
	exit(FAIL);
    }
    exit(PASS);
    return(PASS);
}
