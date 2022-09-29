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
 * bsdgetpgrp_mac.c -- MAC read tests for BSDgetpgrp() system call.
 */
static int bsdgetpgrp_mac1(struct macinfo_2 *parmsptr);

static struct macinfo_2
bsdgetpgrp_minfo[] = 
{
/*
 case  case    c_flag  expect              Object      Subject   unused
 name  number          success  errno     MSEN MINT   MSEN MINT
*/
"pos00",  0,   0,      PASS,   0,        MSL, MIE,   MSA, MIE,    0,
"neg00",  1,   0,      FAIL,   EACCES,   MSH, MIE,   MSA, MIE,    0,
"neg01",  2,   0,      FAIL,   EACCES,   MSA, MIL,   MSA, MIH,    0,
};

static char *bsdgetpgrp_mdesc[] = { 
"MSEN:S > O; MI:S < O; O_RDONLY",   
"MSEN:S !> O; MI:S < O; O_RDONLY",  
"MSEN:S > O; MI:S !< O; O_RDONLY",   
};

int
bsdgetpgrp_mac(void)
{
    short ncases =  3;        /* number of total test cases */
    int fail = 0;            /* set when a test case fails */
    int incomplete = 0;      /* set when a test case is incomplete */
    short i = 0;             /* test case loop counter */
    char str[MSGLEN];        /* used to write to logfiles */
    char testname[SLEN];     /* used to write to logfiles */

    strcpy(testname,"bsdgetpgrp_mac");
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
	switch (bsdgetpgrp_mac1(&bsdgetpgrp_minfo[i]) ) {
	/*
	 * Write formatted result to raw log.
	 */    
	case PASS:      /* Passed */
	    RAWPASS(bsdgetpgrp_minfo[i].casenum, bsdgetpgrp_minfo[i].casename);
	    break;
	case FAIL:      /* Failed */
	    RAWFAIL(bsdgetpgrp_minfo[i].casenum, bsdgetpgrp_minfo[i].casename,
		    bsdgetpgrp_mdesc[i]);
	    fail = 1;
	    break;
	default:
	    RAWINC(bsdgetpgrp_minfo[i].casenum, bsdgetpgrp_minfo[i].casename);
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

static int bsdgetpgrp_mac1(struct macinfo_2 *parmsptr)
{
    char buf[4];                 /* Used in pipe read/writes. */
    char testname[SLEN];         /* used to write to logfiles */
    char namestring[MSGLEN];     /* Used in error messages. */
    int O_S_fd[2];               /* O writes, S reads. */
    int S_O_fd[2];               /* S writes, O reads. */
    pid_t retval = 0;            /* Return value of test call. */
    pid_t forkval1 = 0;          /* O's pid returned to parent */
    pid_t forkval2 = 0;          /* S's pid returned to parent */
    pid_t Opid = 0;              /* O's pid returned by getpid in O */
    int status0;                 /* For wait. */
    int exitstatus0;             /* For wait. */
    int status1;                 /* For wait. */
    int exitstatus1;             /* For wait. */
    mac_t lptr;                  /* MAC label pointer. */

    strcpy(testname,"bsdgetpgrp_mac");
    strcpy (buf, "abc");      /* Stuff to write to pipe. */
    sprintf(namestring, "%s, case %d, %s:\n   ", testname,
	     parmsptr->casenum, parmsptr->casename);

    /*
     * Make 2 pipes.
     */
    if ( pipe(O_S_fd) || pipe(S_O_fd) )  {
	w_error(SYSCALL, namestring, err[F_PIPE], errno);
	return(INCOMPLETE);
    }
    /*
     * Fork Object.
     */
    if (  ( forkval1 = fork() ) == -1  ) {
	w_error(SYSCALL, namestring, err[F_FORK], errno);
	return(INCOMPLETE);
    }

    /* 
     * This is the parent after forking Object.
     */
    if (forkval1)  { 
	/*
	 * Fork S.
	 */
	if (  ( forkval2 = fork() ) == -1  ) {
	    w_error(SYSCALL, namestring, err[F_FORK], errno);
	    return(INCOMPLETE);
	}
    }


    /* 
     * This is the parent after forking Subject.
     */
    if ( (forkval2) && (forkval1) )  { 
	/* 
	 * Close all pipe fds.
	 */
	close(O_S_fd[0]);
	close(O_S_fd[1]);
	close(S_O_fd[0]);
	close(S_O_fd[1]);

	/*
	 * Wait for Children. Return 2 if either child
	 * has been killed or returns 2 or any other
	 * unexpected event occurs.  Otherwise, return 1 if one
	 * child returns 1, otherwise 0.
	 */
	if (( wait(&status0) == -1 ) || ( wait(&status1) == -1 )) {
	    w_error(SYSCALL, namestring, err[F_WAIT], errno);
	    wait(&status1);
	    return(INCOMPLETE);
    	}
	if (( !WIFEXITED(status0) ) || ( !WIFEXITED(status1) )) {
	    w_error(GENERAL, namestring, err[C_NOTEXIT], 0);
	    return(INCOMPLETE);
	}
	if ( ( (exitstatus0 = WEXITSTATUS(status0)) == 2) ||
	     ( (exitstatus1 = WEXITSTATUS(status1)) == 2) ) {
	    return(INCOMPLETE);
	}
	if ( (exitstatus0 == 1) || (exitstatus1 == 1) ) 
	    return(FAIL);
	if ( (exitstatus0 == 0) || (exitstatus1 == 0) ) 
	    return(PASS);
    }

    /* 
     * This is S, the Subject process. 
     */
    if ( (forkval1) && (!forkval2) ) {  
	/*
	 * Close pipe descriptors S does not use.
	 */
	close(O_S_fd[1]);
	close(S_O_fd[0]);

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
	 * Read what O wrote to the pipe.
	 */
	if ( (read(O_S_fd[0], buf, sizeof(buf))) != sizeof(buf)) {
	    w_error(SYSCALL, namestring, err[PIPEREAD_S], errno);
	    exit(INCOMPLETE);
	}

	/*
	 * Here's the test: Call BSDgetpgrp with O's pid.
	 */
	retval = BSDgetpgrp(forkval1);

	 /*
	  * Write to the pipe so O can go away.
	  */
	if ( (write(S_O_fd[1], buf, sizeof(buf)))
	    != (sizeof(buf)) ) {
	    w_error(SYSCALL, namestring, err[PIPEWRITE_S], errno);
	    exit(INCOMPLETE);
	}

	/*
	 * Positive test cases:
	 * Retval should be O's gid, which O set to its pid.
	 */
	if (parmsptr->expect_success == PASS ) {
	    if ( retval == -1 ){
		w_error(SYSCALL, namestring, err[TESTCALL], errno);
		exit(FAIL);
	    } 
	    if (retval != forkval1) {
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
	if ( retval != -1 ){
	    w_error(PRINTNUM, namestring, err[TEST_RETVAL], retval);
	    exit(FAIL);
	}
	if (errno != parmsptr->expect_errno) {
	    w_error(SYSCALL, namestring, err[TEST_ERRNO], errno);
	    exit(FAIL);
	}
	exit(PASS);
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
     * Set O's pgid to its pid.
     */
    Opid = getpid();
    if ( BSDsetpgrp(0, Opid) == -1 ){
	w_error(SYSCALL, namestring, err[F_BSDSETPGRP], errno);
    }
    if ( getpgrp() != Opid ) {
	w_error(GENERAL, namestring, err[BADPGID], 0);
	exit(INCOMPLETE);
    }
    
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
