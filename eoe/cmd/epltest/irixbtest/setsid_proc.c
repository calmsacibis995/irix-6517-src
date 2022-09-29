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
 * setsid_proc.c -- PROC tests for setsid() system call.
 */
static int setsid_proc1(struct dacparms_3 *parmsptr);
static int setsid_proc2(struct dacparms_2 *parmsptr);

/* 
 * Description strings, corresponding with test cases, 
 * to be printed to the raw log if that case fails.
 */
static char *setsid_ddesc[] = 
{ 
  "Suser",
  "Not suser",
  "Suser, already a session leader",
  "Not suser, already a session leader",
};

/*
 * Each structures contains information for one test case.
 * Variables are uids of Subject and Object processes,
 * whether or not the subject is already a session leader,
 * and whether the pid argument is that of the current process
 * or a different process.  Different test functions are called
 * for cases that require an extra process and cases that do not.
 */
static struct dacparms_3
setsid_pinfo1[] = 
{
/*
 * name    #  Sruid  Seuid  prior_setsid?  success?  errno
 */
  "pos00", 0, SUPER, SUPER,  FAIL,         PASS ,     0,
  "pos01", 1, TEST1, TEST1,  FAIL,         PASS ,     0,
  "neg00", 2, SUPER, SUPER,  PASS ,         FAIL,  EPERM,
  "neg01", 3, TEST1, TEST1,  PASS ,         FAIL,  EPERM
};

static struct dacparms_2
setsid_pinfo2[] = 
{
/*
 * name    #  c_flag  Sruid  Seuid  success?  errno
 */
  "neg02", 4,    0,   SUPER, SUPER,  FAIL,  EPERM,
  "neg03", 5,    0,   TEST1, TEST1,  FAIL,  EPERM,
};

int
setsid_proc(void)
{
    char str[MSGLEN];        /* used to write to logfiles */
    char testname[SLEN];     /* used to write to logfiles */
    char *name;              /* holds casename from struct */
    short num;               /* holds casenum from struct */
    short ncases1 = 4;       /* number of test cases using setsid_proc1 */
    short ncases2 = 2;       /* number of test cases using setpgrp_proc2 */
    short i = 0;             /* test case loop counter */
    int fail = 0;            /* set when a test case fails */
    int incomplete = 0;      /* set when a test case is incomplete */
    int retval;              /* return value of test func */

    strcpy(testname,"setsid_proc");
    /*
     * Write formatted info to raw log.
     */   
    RAWLOG_SETUP(testname, (ncases1 + ncases2) );
    for (i = 0; i < (ncases1 + ncases2); i++) {
	/*
	 * Flush output streams
	 */    
	flush_raw_log();   
	/*
	 * Call function setsid_proc1 for each test case.
	 */    
	if (i < ncases1) {
	    retval = setsid_proc1(&setsid_pinfo1[i]);
	    num = setsid_pinfo1[i].casenum;
	    name = setsid_pinfo1[i].casename;
	}
	/*
	 * Call function setpgrp_proc2 for each test case that 
	 * requires an extra process.
	 */    
	if (i >= ncases1) {
	    retval = setsid_proc2(&setsid_pinfo2[i - ncases1]);
	    num  = setsid_pinfo2[i - ncases1].casenum; 
            name = setsid_pinfo2[i - ncases1].casename;
	}

	/*
	 * Write formatted result to raw log.
	 */    
	switch ( retval ) {
	case PASS:      /* Passed */
	    RAWPASS( num, name );
	    break;
	case FAIL:      /* Failed */
	    RAWFAIL(  num, name, setsid_ddesc[i] );
	    fail = 1;
	    break;
	default:
	    RAWINC( num, name );
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
static int
setsid_proc1(struct dacparms_3 *parmsptr)
{
    char testname[SLEN];         /* used to write to logfiles */
    char namestring[MSGLEN];     /* Used in error messages. */
    pid_t forkval1 = 0;          /* O's pid returned to parent */ 
    pid_t retval = 0;            /* Return value of test call. */
    int status = 0;              /* For wait. */   
    
    strcpy(testname,"setsid_proc");
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
     * Set ruid and euid to test value and verify.
     */
    if ( ( cap_setreuid(parmsptr->ruid, parmsptr->euid) ) == -1 ) {
	w_error(SYSCALL, namestring, err[SETREUID_S], errno);
	exit(INCOMPLETE);
    }	
    if ( (getuid() != parmsptr->ruid) || (geteuid() != parmsptr->euid) ) {
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
     * Here's the test: Call setsid.
     */
    retval = setsid();
    
    /*
     * For positive cases, retval should be same as pid.
     */
    if ( parmsptr->expect_success == PASS  ) {
	if ( retval == -1 ) {
	    w_error(SYSCALL, namestring, err[TESTCALL], errno);
	    exit(FAIL);
	} 
	if ( retval != getpid() ) {
	    w_error(PRINTNUM, namestring, err[TEST_RETVAL], retval);
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

/*
 * This function is used for test cases that require an extra process.
 */
static int
setsid_proc2(struct dacparms_2 *parmsptr)
{
    char buf[4];                 /* Used in pipe read/writes. */
    char testname[SLEN];         /* used to write to logfiles */
    char namestring[MSGLEN];     /* Used in error messages. */
    int S_X_fd[2];               /* S writes, X reads. */
    int X_S_fd[2];               /* X writes, O reads. */
    pid_t forkval1 = 0;          /* S's pid returned to parent */ 
    pid_t forkval2 = 0;          /* X's pid returned to parent */ 
    pid_t retval = 0;            /* Return value of test call. */
    int status0;                 /* For wait. */   
    int exitstatus0;             /* For wait. */   
    int status1;                 /* For wait. */   
    int exitstatus1;             /* For wait. */   
    
    strcpy(testname,"setsid_proc");
    strcpy (buf, "abc");      /* Stuff to write to pipe. */
    sprintf(namestring, "%s, case %d, %s:\n   ", testname,
	     parmsptr->casenum, parmsptr->casename);
    /*
     * Fork Subject first so Extra process knows the value
     * of Subject's pid.
     */
    if ( ( forkval1 = fork() ) == -1 ) {
	w_error(SYSCALL, namestring, err[F_FORK], errno);
	return(INCOMPLETE);
    }

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
	if ( (exitstatus0 = WEXITSTATUS(status0) ) == 2) {
	    return(INCOMPLETE);
	}
	if ( exitstatus0 == 1) {
	    return(FAIL);
	}
	if ( exitstatus0 == 0) {
	    return(PASS);
	}
    }

    /* 
     * This is S, the Subject process. 
     */
    if ( !forkval1 ) {
	/*
	 * Make 2 pipes.
	 */
	if ( pipe(S_X_fd) || pipe(X_S_fd) ) {
	    w_error(SYSCALL, namestring, err[F_PIPE], errno);
	    return(INCOMPLETE);
	}
	/*
	 * Fork extra process, X
	 */
	if ( ( forkval2 = fork() ) == -1 ) {
	    w_error(SYSCALL, namestring, err[F_FORK], errno);
	    return(INCOMPLETE);
	}
    }
    /*
     * This is the Subject after forking the Object.
     */
    if ( (!forkval1) && (forkval2) ) {
	/*
	 * Close pipe descriptors that S does not use.
	 */
	close(S_X_fd[0]);
	close(X_S_fd[1]);
	/*
	 * Set Subj pgid to pid
	 */
	if ( BSDsetpgrp(0, getpid()) == -1 ){
	    w_error(SYSCALL, namestring, err[F_BSDSETPGRP], errno);
	    exit(INCOMPLETE);
	}

	/*
	 * Set X's pgid to forkval1, Subject's pid, and verify.
	 */
	if ( BSDsetpgrp(forkval2, getpid()) == -1 ){
	    w_error(SYSCALL, namestring, err[F_BSDSETPGRP], errno);
	    exit(INCOMPLETE);
	}

	/*
	 * Set ruid and euid to test value and verify.
	 */
	if ( ( cap_setreuid(parmsptr->ruid, parmsptr->euid) ) == -1 ) {
	    w_error(SYSCALL, namestring, err[SETREUID_S], errno);
	    exit(INCOMPLETE);
	}	
	if ( (getuid() != parmsptr->ruid)||(geteuid() != parmsptr->euid) ) {
	    w_error(GENERAL, namestring, err[BADUIDS_S], 0);
	    exit(INCOMPLETE);
	}	
	/*
	 * Read from the pipe, ensuring that X has done BSDsetpgrp.
	 */
	if ( ( read(X_S_fd[0], buf, sizeof(buf)) ) != sizeof(buf) ) {
	    w_error(SYSCALL, namestring, err[PIPEREAD_S], errno);
	    exit(INCOMPLETE);
	}
	/*
	 * Here's the test: Call Setsid.  
	 */
	retval = setsid();
    
	/*
	 * Write to the pipe so X can exit.
	 */
	if  ( write(S_X_fd[1], buf, sizeof(buf)) != (sizeof(buf)) ) {
	    w_error(SYSCALL, namestring, err[PIPEWRITE], errno);
	    exit(INCOMPLETE);
	}
    
	/*
	 * For positive cases, retval should be = getpid().
	 */
	if ( parmsptr->expect_success == PASS  ) {
	    if ( retval == -1 ) {
		w_error(SYSCALL, namestring, err[TESTCALL], errno);
		exit(FAIL);
	    } 
	    if ( retval != getpid() ) {
		w_error(PRINTNUM, namestring, err[TEST_RETVAL], retval);
		exit(FAIL);
	    }
	    /*
	     * Wait for Child. Return 2 if child
	     * has been killed or returns 2 or any other
	     * unexpected event occurs.  Otherwise, return 1 if one
	     * child returns 1, otherwise 0.
	     */
	    if ( wait(&status0) == -1 ) {
		w_error(SYSCALL, namestring, err[F_WAIT], errno);
		exit(INCOMPLETE);
	    }
	    if ( !WIFEXITED(status0) ) {
		w_error(GENERAL, namestring, err[C_NOTEXIT], 0);
		exit(INCOMPLETE);
	    }
	    if ( (exitstatus0 = WEXITSTATUS(status0) ) == 2) {
		exit(INCOMPLETE);
	    }
	    if ( exitstatus0 == 1) {
		exit(FAIL);
	    }
	    if ( exitstatus0 == 0) {
		exit(PASS);
	    }
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
	if ( (exitstatus1 = WEXITSTATUS(status1) ) == 2) {
	    exit(INCOMPLETE);
	}
	if ( exitstatus1 == 1) {
	    exit(FAIL);
	}
	if ( exitstatus1 == 0) {
	    exit(PASS);
	}
	return(PASS);
    }

    /*
     *  This is X, the extra process
     */

    if ( !forkval1 && !forkval2 ) {  
	/* 
	 * This is X, the Extra process that sets its pgid to the
	 * Subjects pid. Close pipe descriptors X does not use.
	 */
	close(S_X_fd[1]);
	close(X_S_fd[0]);

	/*
	 * verify that prgrp is same as subject PID
	 */
	if ( getpgrp() != getppid() ) {
	    w_error(GENERAL, namestring, err[BADPGID], 0);
	    exit(INCOMPLETE);
	}

	 /*
	  * Write to the pipe so Subject knows pgid is set.
	  */
	if ( ( write(X_S_fd[1], buf, sizeof(buf)) )
	    != (sizeof(buf)) ) {
	    w_error(SYSCALL, namestring, err[PIPEWRITE], errno);
	    exit(INCOMPLETE);
	}
	/*
	 * Stay alive until S writes to the pipe.
	 */
	if ( ( read(S_X_fd[0], buf, sizeof(buf)) ) != sizeof(buf)) {
	    w_error(SYSCALL, namestring, err[PIPEREAD], errno);
	    exit(INCOMPLETE);
	}
	exit(PASS);
    }
    return(PASS);
}
