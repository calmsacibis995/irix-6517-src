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
int bsdgetpgrp_dac();
/*
 * bsdgetpgrp_dac.c -- DAC tests for BSDgetpgrp() system call.
 */
static int bsdgetpgrp_dac1(struct dacparms_5 *parmsptr);

/*
 * Each structures contains information for one test case.
 * Variables are uids of Subject and Object processes.
 */
static struct dacparms_5
bsdgetpgrp_dinfo[] = 
{
/*
 * name    #  Oruid  Oeuid  Sruid  Seuid  c_flag success?  errno
 */
  "pos00", 0, SUPER, SUPER, SUPER, SUPER,  0,    PASS ,      0,
  "pos01", 1, TEST0, TEST0, TEST1, TEST1,  0,    PASS ,      0,
  "pos02", 2, TEST0, TEST0, TEST0, TEST0,  0,    PASS ,      0,
  "pos03", 3, TEST0, TEST0, SUPER, SUPER,  0,    PASS ,      0,
};


/* 
 * Description strings, corresponding with test cases, 
 * to be printed to the raw log if that case fails.
 */
static char
*bsdgetpgrp_ddesc[] = 
{ "S & O ruids & euids are suser", 
  "S uids diff from O uids, neither is suser", 
  "S uids = O uids, not suser", 
  "S uids diff from O uids, S is suser", };

/*
 * This function calls the testing routine for each test case, 
 * passing the appropriate structure for that test case.
 */
int
bsdgetpgrp_dac(void)
{
    short ncases = 4;        /* number of total test cases */
    int fail = 0;            /* set when a test case fails */
    int incomplete = 0;      /* set when a test case is incomplete */
    short i = 0;             /* test case loop counter */
    char str[MSGLEN];        /* used to write to logfiles */
    char testname[SLEN];     /* used to write to logfiles */

    strcpy(testname,"bsdgetpgrp_dac");
    /*
     * Write formatted info to raw log.
     */   
    RAWLOG_SETUP(testname, ncases);
    /*
     * Call function for each test case.
     */    
    for (i = 0; i < ncases; i++) {
	/*
	 * Flush output streams
	 */    
	flush_raw_log();   
	switch (bsdgetpgrp_dac1(&bsdgetpgrp_dinfo[i]) ) {
	/*
	 * Write formatted result to raw log.
	 */    
	case PASS:      /* Passed */
	    RAWPASS(bsdgetpgrp_dinfo[i].casenum, bsdgetpgrp_dinfo[i].casename);
	    break;
	case FAIL:      /* Failed */
	    RAWFAIL(bsdgetpgrp_dinfo[i].casenum, bsdgetpgrp_dinfo[i].casename,
		    bsdgetpgrp_ddesc[i]);
	    fail = 1;
	    break;
	default:
	    RAWINC(bsdgetpgrp_dinfo[i].casenum, bsdgetpgrp_dinfo[i].casename);
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

static int bsdgetpgrp_dac1(struct dacparms_5 *parmsptr)
{
    char buf[4];                 /* Used in pipe read/writes. */
    char testname[SLEN];         /* used to write to logfiles */
    char namestring[MSGLEN];     /* Used in error messages. */
    int O_S_fd[2];               /* O writes, S reads. */
    int S_O_fd[2];               /* S writes, O reads. */
    pid_t forkval1 = 0;          /* O's pid returned to parent */ 
    pid_t forkval2 = 0;          /* S's pid returned to parent */ 
    pid_t Opid = 0;              /* O's pid returned by getpid in O */ 
    pid_t retval = 0;            /* Return value of test call. */
    int status0;                 /* For wait. */   
    int exitstatus0;             /* For wait. */   
    int status1;                 /* For wait. */   
    int exitstatus1;             /* For wait. */   
    
    strcpy(testname,"bsdgetpgrp_dac");
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
	 * Set ruid and euid to test value and verify.
	 */
	if (  ( cap_setreuid(parmsptr->Sruid, parmsptr->Seuid) ) == -1  ) {
	    w_error(SYSCALL, namestring, err[SETREUID_S], errno);
	    exit(INCOMPLETE);
	}	
	if ( (getuid() != parmsptr->Sruid)||(geteuid() != parmsptr->Seuid) ){
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
	 * Positive cases:
	 * Retval should be O's pgid, which O set to its pid.
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
	if ( retval != -1 ) {
	    w_error(PRINTNUM, namestring, err[TEST_RETVAL], retval);
	    exit(FAIL);
	}
	if (errno != parmsptr->expect_errno) {
	    w_error(SYSCALL, namestring, err[TEST_ERRNO], errno);
	    exit(FAIL);		
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
     * Set O's ruid and euid to test value and verify.
     */
    if ( ( cap_setreuid(parmsptr->ruid, parmsptr->euid)) == -1 ) {
	w_error(SYSCALL, namestring, err[SETREUID_O], errno);
	exit(INCOMPLETE);
    }
    
    if ( (getuid() != parmsptr->ruid) || (geteuid() !=
					  parmsptr->euid) ) {
	w_error(GENERAL, namestring, err[BADUIDS_O], 0);
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
