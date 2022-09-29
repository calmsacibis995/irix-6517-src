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
#include "mac.h"

/*
 * bsdsetpgrp_mac.c -- MAC read tests for BSDsetpgrp() system call.
 */
static int bsdsetpgrp_mac1(struct macinfo_2 *parmsptr);

static struct macinfo_2
bsdsetpgrp_minfo[] = 
{
/*
 case  case    c_flag   expect              Object      Subject   
 name  number           success  errno     MSEN MINT   MSEN MINT  UID
*/
"pos00",  0,      0,     PASS,   0,        MSA, MIL,   MSA, MIL, SUPER,
"pos01",  1,      0,     PASS,   0,        MSL, MIH,   MSH, MIH, SUPER,
"neg00",  2,      0,     FAIL,   EACCES,   MSL, MIH,   MSA, MIH, SUPER,
"neg01",  3,      0,     FAIL,   EACCES,   MSA, MIH,   MSA, MIL, SUPER,

"pos02",  4,      0,     PASS,   0,        MSL, MIL,   MSL, MIL, TEST0,
"neg02",  5,      0,     FAIL,   EACCES,   MSL, MIH,   MSH, MIH, TEST0,
"neg03",  6,      0,     FAIL,   EACCES,   MSA, MIH,   MSH, MIH, TEST0,
"neg04",  7,      0,     FAIL,   EACCES,   MSA, MIH,   MSA, MIL, TEST0,
};

static char *bsdsetpgrp_mdesc[] = { 
"SUSER, MS:S = O; MI:S = O", 
"SUSER, MS:S != O, MS HIGH; MI:S = O",   
"SUSER, MS:S != O; MI:S = O",  
"SUSER, MS:S = O; MI:S != O", 
  
"Not SUSER, MS:S = O; MI:S = O", 
"Not SUSER, MS:S != O, MS HIGH; MI:S = O",   
"Not SUSER, MS:S != O; MI:S = O",  
"Not SUSER, MS:S = O; MI:S != O",   
};

int
bsdsetpgrp_mac(void)
{
    short ncases =  8;        /* number of total test cases */
    int fail = 0;            /* set when a test case fails */
    int incomplete = 0;      /* set when a test case is incomplete */
    short i = 0;             /* test case loop counter */
    char str[MSGLEN];        /* used to write to logfiles */
    char testname[SLEN];     /* used to write to logfiles */

    strcpy(testname,"bsdsetpgrp_mac");
    /*
     * Write formatted info to raw log.
     */   
    RAWLOG_SETUP(testname, ncases);
    /*
     * Call function for each test case.
     */    
    for (i = 0; i < ncases; i++) {
	/*
	 * Flush output streams before calling a function that forks! 
	 */    
	flush_raw_log();   
	switch (bsdsetpgrp_mac1(&bsdsetpgrp_minfo[i]) ) {
	/*
	 * Write formatted result to raw log.
	 */    
	case PASS:      /* Passed */
	    RAWPASS(bsdsetpgrp_minfo[i].casenum, bsdsetpgrp_minfo[i].casename);
	    break;
	case FAIL:      /* Failed */
	    RAWFAIL(bsdsetpgrp_minfo[i].casenum, bsdsetpgrp_minfo[i].casename,
		    bsdsetpgrp_mdesc[i]);
	    fail = 1;
	    break;
	default:
	    RAWINC(bsdsetpgrp_minfo[i].casenum, bsdsetpgrp_minfo[i].casename);
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

static int bsdsetpgrp_mac1(struct macinfo_2 *parmsptr)
{
    char buf[4];                 /* Used in pipe read/writes. */
    char testname[SLEN];         /* used to write to logfiles */
    char namestring[MSGLEN];     /* Used in error messages. */
    int O_S_fd[2];               /* O writes, S reads. */
    int S_O_fd[2];               /* S writes, O reads. */
    pid_t retval = 0;            /* Return value of test call. */
    pid_t forkval1 = 0;          /* O's pid returned to parent */ 
    pid_t forkval2 = 0;          /* S's pid returned to parent */ 
    mac_t lptr;                  /* MAC label pointer. */
    int status0;                 /* For wait. */   
    int exitstatus0;             /* For wait. */   
    int status1;                 /* For wait. */   
    int exitstatus1;             /* For wait. */   

    strcpy(testname,"bsdsetpgrp_mac");
    sprintf(namestring, "%s, case %d, %s:\n   ", testname,
	     parmsptr->casenum, parmsptr->casename);

    /*
     * Fork Subject.
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
	 * Wait for Child. Return 2 if child
	 * has been killed or returns 2 or any other
	 * unexpected event occurs.  Otherwise, return 1 
	 * if child returns 1, otherwise 0.
	 */
	if ( wait(&status0) == -1 ) {
	    w_error(SYSCALL, namestring, err[F_WAIT], errno);
	    return(INCOMPLETE);
    	}
	if ( !WIFEXITED(status0) ) {
	    w_error(GENERAL, namestring, err[C_NOTEXIT], 0);
	    return(INCOMPLETE);
	}
	if ( (exitstatus0 = WEXITSTATUS(status0)) == 2) {
	    return(INCOMPLETE);
	}
	if ( exitstatus0 == 1) 
	    return(FAIL);
	if ( exitstatus0 == 0)
	    return(PASS);
    }

    /* 
     * This is the Subject.
     */
    if ( !forkval1 )  { 

	strcpy (buf, "abc");      /* Stuff to write to pipe. */
	/*
	 * Make 2 pipes.
	 */
	if ( pipe(O_S_fd) || pipe(S_O_fd) )  {
	    w_error(SYSCALL, namestring, err[F_PIPE], errno);
	    exit(INCOMPLETE);
	}
	/*
	 * Fork Object.
	 */
	if (  ( forkval2 = fork() ) == -1  ) {
	    w_error(SYSCALL, namestring, err[F_FORK], errno);
	    exit(INCOMPLETE);
	}
    }
    /*
     * This is the Subject after forking the Object.
     */
    if ( (!forkval1) && (forkval2) ) {
	/*
	 * Close pipe descriptors S does not use.
	 */
	close(O_S_fd[1]);
	close(S_O_fd[0]);
	/*
	 * Set Subject pgid to pid
	 */
	if ( BSDsetpgrp(0, getpid()) == -1 ){
		w_error(SYSCALL, namestring, err[F_BSDSETPGRP], errno);
	}

	/*
	 * Malloc a mac_label pointer
	 */
	lptr = mac_get_proc();

	/*
	 * Set S process label.
	 */
	lptr->ml_msen_type = parmsptr->S_msen;
	lptr->ml_mint_type = parmsptr->S_mint;
	if ( cap_setplabel(lptr) ) {
	    w_error(SYSCALL, namestring, err[F_SETPLABEL], errno);
	    exit(INCOMPLETE);
	}
	mac_free(lptr);

	/*
	 * Set ruid and euid to test value and verify.
	 */
	if (  ( cap_setreuid(parmsptr->uid, parmsptr->uid) ) == -1  ) {
	    w_error(SYSCALL, namestring, err[SETREUID_S], errno);
	    exit(INCOMPLETE);
	}	
	if ( (getuid() != parmsptr->uid)||(geteuid() != parmsptr->uid) ) {
	    w_error(GENERAL, namestring, err[BADUIDS_S], 0);
	    exit(INCOMPLETE);
	}	
	/*
	 * Read what O wrote to the pipe.
	 */
	if ( (read(O_S_fd[0], buf, sizeof(buf))) != sizeof(buf)) {
	    w_error(SYSCALL, namestring, err[PIPEREAD_S], errno);
	    exit(INCOMPLETE);
	}

	/*
	 * Here's the test: Call BSDsetpgrp with O's pid as
	 * arg1 and S's pid as arg2.
	 */
	retval = BSDsetpgrp(forkval2, getpid());

	 /*
	  * Write to the pipe so O can go away.
	  */
	if ( (write(S_O_fd[1], buf, sizeof(buf)))
	    != (sizeof(buf)) ) {
	    w_error(SYSCALL, namestring, err[PIPEWRITE_S], errno);
	    exit(INCOMPLETE);
	}

	/*
	 * For positive cases, retval should be 0.
	 */
	if (parmsptr->expect_success == PASS ) {
	    if (  retval == -1 ){
		w_error(SYSCALL, namestring, err[TESTCALL], errno);
		exit(FAIL);
	    } 
	    if (retval != 0) {
		w_error(PRINTNUM, namestring, err[TEST_RETVAL],
			retval);
		exit(FAIL);
	    }
	    /*
	     * Wait for Child. Return 2 if child
	     * has been killed or returns 2 or any other
	     * unexpected event occurs.  Otherwise, return 1 
	     * if child returns 1, otherwise 0.
	     */
	    if ( wait(&status1) == -1 ) {
		w_error(SYSCALL, namestring, err[F_WAIT], errno);
		exit(INCOMPLETE);
    	    }
	    if ( !WIFEXITED(status1) ) {
		w_error(GENERAL, namestring, err[C_NOTEXIT], 0);
		exit(INCOMPLETE);
	    }
	    if ( (exitstatus1 = WEXITSTATUS(status1)) == 2) {
		exit(INCOMPLETE);
	    }
	    if ( exitstatus1 == 1 ) {
		exit(FAIL);
	    }
	    if ( exitstatus1 == 0 )  {
		exit(PASS);
	    }
	}

	/*
	 * For negative test cases, retval should be -1 and
	 * errno should equal parmsptr->expect_errno.
	 */
	if (  retval != -1 ){
	    w_error(PRINTNUM, namestring, err[TEST_RETVAL], retval);
	    exit(FAIL);
	}
	if (errno != parmsptr->expect_errno) {
	    w_error(SYSCALL, namestring, err[TEST_ERRNO], errno);
	    exit(FAIL);
	}
	/*
	 * Wait for Child. Return 2 if child
	 * has been killed or returns 2 or any other
	 * unexpected event occurs.  Otherwise, return 1 
	 * if child returns 1, otherwise 0.
	 */
	if ( wait(&status1) == -1 ) {
	    w_error(SYSCALL, namestring, err[F_WAIT], errno);
	    exit(INCOMPLETE);
    	}
	if ( !WIFEXITED(status1) ) {
	    w_error(GENERAL, namestring, err[C_NOTEXIT], 0);
	    exit(INCOMPLETE);
	}
	if ( (exitstatus1 = WEXITSTATUS(status1)) == 2) {
	    exit(INCOMPLETE);
	}
	if ( exitstatus1 == 1 ) {
	    exit(FAIL);
	}
	if ( exitstatus1 == 0 )  {
	    exit(PASS);
	}
    }


    /* 
     * This is O, the Object process. 
     */
    /*
     * Begin O code. Close pipe descriptors that O does not use.
     */
    close(O_S_fd[0]);
    close(S_O_fd[1]);
    
    /*
     * Malloc a mac_label pointer
     */
    lptr = mac_get_proc();
    
    /*
     * Set O process label.
     */
    lptr->ml_msen_type = parmsptr->O_msen;
    lptr->ml_mint_type = parmsptr->O_mint;
    if ( cap_setplabel(lptr) ) {
	w_error(SYSCALL, namestring, err[F_SETPLABEL], errno);
	exit(INCOMPLETE);
    }
    mac_free(lptr);
    /*
     * Set Object ruid and euid to test value and verify.
     */
    if (  ( cap_setreuid(parmsptr->uid, parmsptr->uid) ) == -1  ) {
	w_error(SYSCALL, namestring, err[SETREUID_S], errno);
	exit(INCOMPLETE);
    }	
    if ( (getuid() != parmsptr->uid)||(geteuid() != parmsptr->uid) ) {
	w_error(GENERAL, namestring, err[BADUIDS_S], 0);
	exit(INCOMPLETE);
    }	
    /*
     * Write to the pipe so S can proceed.
     */
    if  ( write(O_S_fd[1], buf, sizeof(buf))
	 != (sizeof(buf)) )      {
	w_error(SYSCALL, namestring, err[PIPEWRITE_O], errno);
	exit(INCOMPLETE);
    }
    /*
     * Stay alive until S writes to the pipe.
     */
    if ( (read(S_O_fd[0], buf, sizeof(buf))) != sizeof(buf) ) {
	w_error(SYSCALL, namestring, err[PIPEREAD_O], errno);
	exit(INCOMPLETE);
    }
    exit(PASS);
    return(PASS);
    /*
     * End O code.
     */
}
