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
 * bsdsetpgrp_dac.c -- DAC tests for BSDsetpgrp() system call.
 */
static int bsdsetpgrp_dac1(struct dacparms_4 *parmsptr); 

/*
 * Each structure contains information for one test case.
 * Variables are uids of Subject and Object processes, whether or not
 * Subject and Object are in the same session, whether the test 
 * call is expected to succeed (positive case) or fail (negative
 * case), and errno expected in each negative case.  To effect a 
 * difference in session id, the object process calls setsid().
 * Cases 6 and 7 are commented out because setreuid no longer
 * sets the object's saved uid.
 */
static struct dacparms_4
bsdsetpgrp_dinfo[] = 
{
/*
 * name    #  c_flag Oruid  Oeuid  Sruid  Seuid unused setsid?  success?  errno
 */
  "pos00", 0,     0, SUPER, SUPER, SUPER, SUPER,  0,   FAIL,     PASS,        0,
  "pos01", 1,     0, TEST0, TEST0, TEST1, TEST1,  0,   FAIL,     PASS,        0,
  "neg00", 2,     0, TEST0, TEST0, TEST0, TEST0,  0,   PASS,     FAIL,    EPERM,
  "pos03", 3,     0, TEST0, TEST0, TEST1, SUPER,  0,   PASS,     PASS,        0,
  "neg01", 4,     0, TEST0, TEST0, SUPER, TEST1,  0,   PASS,     FAIL,    EPERM,
  "neg02", 5,     0, TEST0, TEST2, TEST1, TEST0,  0,   PASS,     FAIL,    EPERM,
  "neg03", 6,     0, TEST2, TEST0, TEST1, TEST0,  0,   PASS,     FAIL,    EPERM,
  "neg04", 7,     0, TEST2, TEST0, TEST0, TEST1,  0,   PASS,     FAIL,    EPERM,
  "neg05", 8,     0, TEST0, TEST2, TEST0, TEST1,  0,   PASS,     FAIL,    EPERM,
  "neg06", 9,     0, TEST0, TEST0, TEST1, TEST1,  0,   PASS,     FAIL,    EPERM  };

/* 
 * Description strings, corresponding with test cases, 
 * to be printed to the raw log if that case fails.
 */
static char *bsdsetpgrp_ddesc[] = 
{ "S & O ruids & euids all are suser, S & O in same sid",
  "S uids diff from O uids, S and O in same sid",
  "S uids = O uids, S & O in diff sids",
  "S uids diff from O uids, S euid is suser, S & O in diff sids",
  "S uids diff from O uids, S ruid is suser, S & O in diff sids",
  "S euid = O ruid, S & O in diff sids",
  "S euid = O euid, S & O in diff sids",
  "S ruid = O euid, S & O in diff sids",
  "S ruid = O ruid, S & O in different sids",
  "S uids diff from O uids, S & O in diff sids"   };

int
bsdsetpgrp_dac(void)
{
    short ncases =  8;       /* number of total test cases */
    int fail = 0;            /* set when a test case fails */
    int incomplete = 0;      /* set when a test case is incomplete */
    short i = 0;             /* test case loop counter */
    char str[MSGLEN];        /* used to write to logfiles */
    char testname[SLEN];     /* used to write to logfiles */

    strcpy(testname,"bsdsetpgrp_dac");
    /*
     * Write formatted info to raw log.
     */    
    RAWLOG_SETUP( testname, ncases );
		      
    /*
     * Call function for each test case.
     */    
    for (i = 0; i < ncases; i++) {
	/*
	 * Flush output streams
	 */    
	flush_raw_log();   
	switch ( bsdsetpgrp_dac1(&bsdsetpgrp_dinfo[i]) ) {
	/*
	 * Write formatted result to raw log.
	 */    
	case PASS:      /* Passed */
	    RAWPASS(bsdsetpgrp_dinfo[i].casenum, bsdsetpgrp_dinfo[i].casename);
	    break;
	case FAIL:      /* Failed */
	    RAWFAIL(bsdsetpgrp_dinfo[i].casenum, bsdsetpgrp_dinfo[i].casename,
		    bsdsetpgrp_ddesc[i]);
	    fail = 1;
	    break;
	default:
	    RAWINC(bsdsetpgrp_dinfo[i].casenum, bsdsetpgrp_dinfo[i].casename);
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

static int bsdsetpgrp_dac1(struct dacparms_4 *parmsptr) 
{
    char buf[4];                 /* Used in pipe read/writes. */
    char testname[SLEN];         /* used to write to logfiles */
    char namestring[MSGLEN];     /* Used in error messages. */
    int OtoS_fd[2];              /* O writes, S reads. */
    int StoO_fd[2];              /* S writes, O reads. */
    pid_t forkval1 = 0;          /* Subject's pid returned to parent */ 
    pid_t forkval2 = 0;          /* Object's pid returned to Subject */ 
    pid_t Opid = 0;              /* O's pid returned by getpid in O */ 
    pid_t retval = 0;            /* Return value of test call. */
    int status0;                 /* For wait. */   
    int exitstatus0;             /* For wait. */   
    int status1;                 /* For wait. */   
    int exitstatus1;             /* For wait. */   

    strcpy(testname,"bsdsetpgrp_dac");

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
    if ( forkval1 )  { 
 	/*
	 * Wait for Child. Return 2 if child
	 * has been killed or returns 2 or any other
	 * unexpected event occurs.  Otherwise, return 1 if one
	 * child returns 1, otherwise 0.
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
	if ( exitstatus0 == 1 ) {
	    return(FAIL);
	}
	if ( exitstatus0 == 0 ) {
	    return(PASS);
	}
    }


    /* 
     * This is the Subject. 
     */
    if ( !forkval1 ) {  

	strcpy (buf, "abc");      /* Stuff to write to pipe. */
	/*
	 * Make 2 pipes.
	 */
	if ( pipe(OtoS_fd) || pipe(StoO_fd) )  {
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
     *  This is the Subject after forking the Object.
     */
    if ( (!forkval1) && (forkval2) ) {
	/*
	 * Close pipe descriptors that Subject does not use.
	 */
	close(OtoS_fd[1]);
	close(StoO_fd[0]);
	/*
	 * Set Subject pgid to pid
	 */
	if ( BSDsetpgrp(0, getpid()) == -1 ){
		w_error(SYSCALL, namestring, err[F_BSDSETPGRP], errno);
	}
	/*
	 * Set ruid and euid to test value and verify.  For cases
	 * where ruid = euid, call setuid instead of setreuid
	 * so p_suid is also set.  It is not possible, in our 
	 * implementation, to set p_suid different from p_uid.
	 */
	if (parmsptr->Seuid == parmsptr->Sruid) {
	    if ( ( cap_setuid(parmsptr->Seuid) ) == -1  ) {
		w_error(SYSCALL, namestring, err[SETUID_S], errno);
		exit(INCOMPLETE);
	    }
	} else {
	    if ( ( cap_setreuid(parmsptr->Sruid, parmsptr->Seuid) ) == -1 ) {
		w_error(SYSCALL, namestring, err[SETREUID_S], errno);
		exit(INCOMPLETE);
	    }
	}	
	if ( (getuid() != parmsptr->Sruid)||(geteuid() != parmsptr->Seuid) ) {
	    w_error(GENERAL, namestring, err[BADUIDS_S], 0);
	    exit(INCOMPLETE);
	}	
	/*
	 * Read what O wrote to the pipe.
	 */
	if ( (read(OtoS_fd[0], buf, sizeof(buf))) != sizeof(buf)) {
	    w_error(SYSCALL, namestring, err[PIPEREAD_S], errno);
	    exit(INCOMPLETE);
	}

	/*
	 * Here's the test: Call BSDsetpgrp with O's pid
	 * as arg1 and S's pid as arg2.
	 */
	retval = BSDsetpgrp(forkval2,getpid());

	/*
	 * Write to the pipe so O can go away.
	 */
	if ( (write(StoO_fd[1], buf, sizeof(buf))) != (sizeof(buf)) ) {
	    w_error(SYSCALL, namestring, err[PIPEWRITE_S], errno);
	    exit(INCOMPLETE);
	}

	/*
	 * For positive cases, retval should be zero.
	 */
	if (parmsptr->expect_success == PASS ) {
	    if ( retval == -1 ){
		w_error(SYSCALL, namestring, err[TESTCALL], errno);
		exit(FAIL);
	    }
	    if ( retval ) {
		w_error(PRINTNUM, namestring, err[TEST_RETVAL], retval);
		exit(FAIL);
	    }
	    /*
	     * Wait for Child. Return 2 if child
	     * has been killed or returns 2 or any other
	     * unexpected event occurs.  Otherwise, return 1 if one
	     * child returns 1, otherwise 0.
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
	    if ( exitstatus1 == 0 ) {
		exit(PASS);
	    }
	}

	/*
	 * For negative test cases, retval should be -1 and
	 * errno should equal parmsptr->expect_errno.
	 */
	if ( retval != -1  ) {
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
	 * unexpected event occurs.  Otherwise, return 1 if one
	 * child returns 1, otherwise 0.
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
	if ( exitstatus1 == 1 ) 
	    exit(FAIL);
	if ( exitstatus1 == 0 ) 
	    exit(PASS);
	    return(PASS);
    }


    /* 
     * This is O, the object process. 
     */
    /*
     * Begin O code. Close pipe descriptors that O does not use.
     */
    close(OtoS_fd[0]);
    close(StoO_fd[1]);
    
    /*
     * Set O's ruid and euid to test value and verify.For cases
     * where ruid = euid, call setuid instead of setreuid
     * so p_suid is also set.  It is not possible, in our 
     * implementation, to set p_suid different from p_uid.
     */
    if (parmsptr->euid == parmsptr->ruid) {
	if (  ( cap_setuid(parmsptr->euid) ) == -1  ) {
	    w_error(SYSCALL, namestring, err[SETUID_O], errno);
	    exit(INCOMPLETE);
	}
    }	
    else {
	if ( ( cap_setreuid(parmsptr->ruid, parmsptr->euid)) == -1) {
	    w_error(SYSCALL, namestring, err[SETREUID_O], errno);
	    exit(INCOMPLETE);
	}
    }
    
    if ( (getuid() != parmsptr->ruid)||(geteuid() != parmsptr->euid) ) {
	w_error(GENERAL, namestring, err[BADUIDS_O], 0);
	exit(INCOMPLETE);
    }
    
    /*
     * Do a setsid If this test case requires it.
     */
    if (parmsptr->flag2 == PASS ) {
	Opid = getpid();
	if ( setsid() == -1 ){
	    w_error(SYSCALL, namestring, err[F_SETSID], errno);
	    exit(INCOMPLETE);
	}
	if ( getpgrp() != Opid ) {
	    w_error(GENERAL, namestring, err[BADPGID], 0);
	    exit(INCOMPLETE);
	}
    }
    
    /*
     * Write to the pipe so S can proceed.
     */
    if ( (write(OtoS_fd[1], buf, sizeof(buf)))
	!= (sizeof(buf)) )      {
	w_error(SYSCALL, namestring, err[PIPEWRITE_O], errno);
	exit(INCOMPLETE);
    }
    /*
     * Stay alive until S writes to the pipe.
     */
    if ( (read(StoO_fd[0], buf, sizeof(buf))) != sizeof(buf) ) {
	w_error(SYSCALL, namestring, err[PIPEREAD_O], errno);
	exit(INCOMPLETE);
    }
    exit(PASS);
    return(PASS);
    /*
     * End O code.
     */
}
