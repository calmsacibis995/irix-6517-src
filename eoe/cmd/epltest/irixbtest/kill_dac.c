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
#include <signal.h>

/*
 * kill_dac.c -- DAC tests for kill() system call.
 */
static int kill_dac1(struct dacparms_4 *parmsptr); 
static int kill_dac2(struct dacparms_4 *parmsptr);

/*
 * Each structures contains information for one test case.
 * Variables are uids of Subject and Object processes, signal
 * number, whether or not Subject and Object are in the same session,
 * whether the test call is expected to succeed (positive case) or fail
 * (negative case), and errno expected in each negative case.  To effect
 * a difference in session id, the object process calls setsid().
 *
 * Note that cases 6 and 7 have been removed because a fix
 * to setreuid alters the action of that call so that effective
 * (i.e. saved) uid of the object process is no longer changed.
 *
 */
static struct dacparms_4
kill_dparms[] = 
{
/*
 * name    #   c_flag  Ouid   unused  Sruid  Seuid  signal  setsid  success  errno
 */
  "pos00",  0,     1,  TEST0, TEST0, TEST2, SUPER, SIGCONT,  PASS,  PASS,       0,
  "neg00",  1,     0,  TEST0, TEST0, TEST1, TEST1, SIGCONT,  PASS,  FAIL,   EPERM,
  "neg01",  2,     0,  TEST0, TEST0, TEST1, TEST1, SIGINT,   FAIL,  FAIL,   EPERM,
  "pos02",  3,     1,  TEST0, TEST0, TEST1, SUPER, SIGINT,   FAIL,  PASS,       0,
  "pos03",  4,     0,  TEST0, TEST0, TEST0, TEST1, SIGINT,   FAIL,  PASS,       0,
  "pos04",  5,     0,  TEST0, TEST0, TEST1, TEST0, SIGINT,   FAIL,  PASS,       0,
/*
  "pos05",  6,     0,  TEST0, TEST0, TEST0, TEST1, SIGINT,   FAIL,  PASS,   0,
  "pos06",  7,     0,  TEST0, TEST0, TEST0, TEST1, SIGINT,   FAIL,  PASS,   0,
 */
  "pos07",  8,     0,  TEST0, TEST0, TEST1, TEST1, SIGCONT,  FAIL,  PASS,   0, 
  "neg02",  9,     0,  TEST0, TEST0, TEST1, TEST1, SIGCONT,  PASS,  FAIL,   EPERM,
  "neg03", 10,     0,  TEST0, TEST0, SUPER, TEST1, SIGINT,   FAIL,  FAIL,   EPERM,

  "neg04", 11,     0,  TEST0, TEST0, TEST1, TEST1, SIGINT,   FAIL,  FAIL,   EPERM,
  "neg05", 12,     0,  TEST0, TEST0, TEST1, TEST1, SIGHUP,   FAIL,  FAIL,   EPERM,
  "neg06", 13,     0,  TEST0, TEST0, TEST1, TEST1, SIGQUIT,  FAIL,  FAIL,   EPERM,
  "neg07", 14,     0,  TEST0, TEST0, TEST1, TEST1, SIGILL,   FAIL,  FAIL,   EPERM,
  "neg08", 15,     0,  TEST0, TEST0, TEST1, TEST1, SIGTRAP,  FAIL,  FAIL,   EPERM,
  "neg09", 16,     0,  TEST0, TEST0, TEST1, TEST1, SIGABRT,  FAIL,  FAIL,   EPERM,
  "neg10", 17,     0,  TEST0, TEST0, TEST1, TEST1, SIGEMT,   FAIL,  FAIL,   EPERM,
  "neg11", 18,     0,  TEST0, TEST0, TEST1, TEST1, SIGFPE,   FAIL,  FAIL,   EPERM,
  "neg12", 19,     0,  TEST0, TEST0, TEST1, TEST1, SIGKILL,  FAIL,  FAIL,   EPERM,
  "neg13", 20,     0,  TEST0, TEST0, TEST1, TEST1, SIGBUS,   FAIL,  FAIL,   EPERM,
  "neg14", 21,     0,  TEST0, TEST0, TEST1, TEST1, SIGSEGV,  FAIL,  FAIL,   EPERM,
  "neg15", 22,     0,  TEST0, TEST0, TEST1, TEST1, SIGSYS,   FAIL,  FAIL,   EPERM,
  "neg16", 23,     0,  TEST0, TEST0, TEST1, TEST1, SIGPIPE,  FAIL,  FAIL,   EPERM,
  "neg17", 24,     0,  TEST0, TEST0, TEST1, TEST1, SIGALRM,  FAIL,  FAIL,   EPERM,
  "neg18", 25,     0,  TEST0, TEST0, TEST1, TEST1, SIGTERM,  FAIL,  FAIL,   EPERM,
  "neg19", 26,     0,  TEST0, TEST0, TEST1, TEST1, SIGUSR1,  FAIL,  FAIL,   EPERM,
  "neg20", 27,     0,  TEST0, TEST0, TEST1, TEST1, SIGUSR2,  FAIL,  FAIL,   EPERM,
  "neg21", 28,     0,  TEST0, TEST0, TEST1, TEST1, SIGCLD,   FAIL,  FAIL,   EPERM,
  "neg22", 29,     0,  TEST0, TEST0, TEST1, TEST1, SIGPWR,   FAIL,  FAIL,   EPERM,
  "neg23", 30,     0,  TEST0, TEST0, TEST1, TEST1, SIGSTOP,  FAIL,  FAIL,   EPERM,
  "neg24", 31,     0,  TEST0, TEST0, TEST1, TEST1, SIGTSTP,  FAIL,  FAIL,   EPERM,
  "neg25", 32,     0,  TEST0, TEST0, TEST1, TEST1, SIGPOLL,  FAIL,  FAIL,   EPERM,
  "neg26", 33,     0,  TEST0, TEST0, TEST1, TEST1, SIGIO,    FAIL,  FAIL,   EPERM,
  "neg27", 34,     0,  TEST0, TEST0, TEST1, TEST1, SIGURG,   FAIL,  FAIL,   EPERM,
  "neg28", 35,     0,  TEST0, TEST0, TEST1, TEST1, SIGWINCH, FAIL,  FAIL,   EPERM,
  "neg29", 36,     0,  TEST0, TEST0, TEST1, TEST1, SIGVTALRM,FAIL,  FAIL,   EPERM,
  "neg30", 37,     0,  TEST0, TEST0, TEST1, TEST1, SIGPROF,  FAIL,  FAIL,   EPERM,
  "neg31", 38,     0,  TEST0, TEST0, TEST1, TEST1, SIGTTIN,  FAIL,  FAIL,   EPERM,
  "neg32", 39,     0,  TEST0, TEST0, TEST1, TEST1, SIGTTOU,  FAIL,  FAIL,   EPERM,
  "neg33", 40,     0,  TEST0, TEST0, TEST1, TEST1, SIGXCPU,  FAIL,  FAIL,   EPERM,
  "neg34", 41,     0,  TEST0, TEST0, TEST1, TEST1, SIGXFSZ,  FAIL,  FAIL,   EPERM, };

/* 
 * Description strings, corresponding with test cases, 
 * to be printed to the raw log if that case fails.
 */
static char *kill_ddesc[] = 
{ 
  "O is child, S euid suser, diff sid, SIGCONT",        /* 0 */
  "O is child, S & O uids differ, diff sid, SIGCONT",	/* 1 */
  "O is child, S & O uids differ, same sid, SIGINT",    /* 2 */
  "O not child, S euid suser, S ruid differs, SIGINT",  /* 3 */
  "O not child, S ruid = O uid, SIGINT",                /* 4 */
  "O not child, S euid = O uid, SIGINT",                /* 5 */

/*  "O not child, S ruid = O euid, SIGINT",    */           /* 6 */
/*  "O not child, S euid = O euid, SIGINT",    */           /* 7 */
  "O not child, S uids diff from O, SIGCONT, same sid", /* 8 */
  "O not child, S uids diff from O, SIGCONT, diff sid", /* 9 */
  "O not child, S ruid suser, S euid diff, SIGINT",     /* 10 */

  "O not child, S uids diff from O, SIGINT",
  "O not child, S uids diff from O, SIGHUP",
  "O not child, S uids diff from O, SIGQUIT",
  "O not child, S uids diff from O, SIGILL",
  "O not child, S uids diff from O, SIGTRAP",
  "O not child, S uids diff from O, SIGABRT",
  "O not child, S uids diff from O, SIGEMT",
  "O not child, S uids diff from O, SIGFPE",
  "O not child, S uids diff from O, SIGKILL",
  "O not child, S uids diff from O, SIGBUS",
  "O not child, S uids diff from O, SIGSEGV",
  "O not child, S uids diff from O, SIGSSYS",
  "O not child, S uids diff from O, SIGPIPE",
  "O not child, S uids diff from O, SIGALRM",
  "O not child, S uids diff from O, SIGTERM",
  "O not child, S uids diff from O, SIGUSR1",
  "O not child, S uids diff from O, SIGUSR2",
  "O not child, S uids diff from O, SIGCLD",
  "O not child, S uids diff from O, SIGPWR",
  "O not child, S uids diff from O, SIGSTOP",
  "O not child, S uids diff from O, SIGTSTP",
  "O not child, S uids diff from O, SIGPOLL",
  "O not child, S uids diff from O, SIGIO",
  "O not child, S uids diff from O, SIGURG",
  "O not child, S uids diff from O, SIGWINCH",
  "O not child, S uids diff from O, SIGVTALRM",
  "O not child, S uids diff from O, SIGPROF",
  "O not child, S uids diff from O, SIGTTIN",
  "O not child, S uids diff from O, SIGTTOU",
  "O not child, S uids diff from O, SIGXCPU",
  "O not child, S uids diff from O, SIGXFSZ"  };

int
kill_dac(void)
{
    char str[MSGLEN];        /* used to write to logfiles */
    char testname[SLEN];     /* used to write to logfiles */
    short ncases1 = 3;       /* number of test cases with child object */
    short ncases2 = 37;      /* number of test cases with non-child object */
    int fail = 0;            /* set when test case fails */
    int incomplete = 0;      /* set when test case incomplete */
    int retval;              /* value returned on function call */
    short i = 0;             /* test case loop counter */

    strcpy(testname,"kill_dac");
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
	 * Call kill_dac1() for cases 0-ncases1 and
	 * kill_dac2() for the rest.
	 */    
	if (i < ncases1) {
	    retval = kill_dac1(&kill_dparms[i]);
	}
	if (i >= ncases1) {
	    retval = kill_dac2(&kill_dparms[i]);
	}
	/*
	 * Write formatted result to raw log.
	 */    
	switch ( retval ) {
	case PASS:      /* Passed */
	    RAWPASS(kill_dparms[i].casenum, kill_dparms[i].casename);
	    break;
	case FAIL:      /* Failed */
	    RAWFAIL(kill_dparms[i].casenum, kill_dparms[i].casename,
		    kill_ddesc[i]);
	    fail = 1;
	    break;
	default:
	    RAWINC(kill_dparms[i].casenum, kill_dparms[i].casename);
	    incomplete = 1;
	    break;  /* Incomplete */
	}
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



/* 
 * Test function for cases where Object is child of Subject.
 */
static int kill_dac1(struct dacparms_4 *parmsptr)  
{
    char buf[4];                 /* Used in pipe read/writes. */
    char testname[SLEN];         /* used to write to logfiles */
    char namestring[MSGLEN];     /* Used in error messages. */
    int S_O_fd[2];               /* Subject writes, object reads. */
    int O_S_fd[2];               /* Subject writes, object reads. */
    pid_t Spid = 0;              /* Subject's pid returned to parent. */ 
    pid_t Opid = 0;              /* Object's pid returned to subject */ 
    pid_t retval = 0;            /* Return value of test call. */
    int status;                  /* For wait. */   
    int loc_errno;               /* Save errno on test call. */
 
    strcpy(testname,"kill_dac");
    strcpy (buf, "abc");      /* Stuff to write to pipe. */
    sprintf(namestring, "%s, case %d, %s:\n   ", testname,
	     parmsptr->casenum, parmsptr->casename);
    /*
     * Make 2 pipes.
     */
    if ( pipe(S_O_fd) || pipe(O_S_fd) )  {
	w_error(SYSCALL, namestring, err[F_PIPE], errno);
	return(INCOMPLETE);
    }

    /*
     * Fork Subject.
     */
    if (  ( Spid = fork() ) == -1  ) {
	w_error(SYSCALL, namestring, err[F_FORK], errno);
	return(INCOMPLETE);
    }

    /* 
     * This is the parent after forking Subject.
     */
    if ( Spid  ) { 
	/* 
	 * Close all pipe fds.
	 */
	close(S_O_fd[0]);
	close(S_O_fd[1]);
	close(O_S_fd[0]);
	close(O_S_fd[1]);
	/*
	 * Wait for child (Subject).
	 */
	if ( wait(&status) == -1 ) {
	    w_error(SYSCALL, namestring, err[F_WAIT], errno);
	    return(INCOMPLETE);
    	}
	if ( !WIFEXITED(status) ) {
	    w_error(SYSCALL, namestring, err[C_NOTEXIT], errno);
	    return(INCOMPLETE);
    	}
	return(  WEXITSTATUS(status)  );
    }


    /*
     * This is the Subject before forking the Object.
     */
    /*
     * Fork Object.
     */
    if (  ( Opid = fork() ) == -1  ) {
	w_error(SYSCALL, namestring, err[F_FORK], errno);
	exit(INCOMPLETE);
    }
    

    /*
     * This is the Subject after forking the Object.
     */    
    if (  ( !Spid ) && ( Opid ) ) {
	close(S_O_fd[0]);
	close(O_S_fd[1]);
	/*
	 * Set Subject ruid and euid to test values and verify.
	 */
	if (  ( cap_setreuid(parmsptr->Sruid, parmsptr->Seuid) ) == -1  ) {
	    w_error(SYSCALL, namestring, err[SETREUID_S], errno);
	    exit(INCOMPLETE);
	}	
	if ( (getuid() != parmsptr->Sruid)||(geteuid() != parmsptr->Seuid) ) {
	    w_error(GENERAL, namestring, err[BADUIDS_S], 0);
	    exit(INCOMPLETE);
	}	
	/*
	 * Do a setsid If this test case requires it.
	 */
	if (parmsptr->flag2 == PASS ) {
	    if ( setsid() == -1 ){
		w_error(SYSCALL, namestring, err[F_SETSID], errno);
		exit(INCOMPLETE);
	    }
	    if ( getpgrp() != getpid() ) {
		w_error(GENERAL, namestring, err[BADPGID], 0);
		exit(INCOMPLETE);
	    }
	}

	/*
	 * Read what Object wrote to the pipe.
	 */
	if ( (read(O_S_fd[0], buf, sizeof(buf))) != sizeof(buf)) {
	    w_error(SYSCALL, namestring, err[PIPEREAD_S], errno);
	    exit(INCOMPLETE);
	}
	/*
	 * Here's the test: Call kill with Object's pid
	 * as arg1 and appropriate signal as arg2.  Save
	 * errno because it could be reset by wait.
	 */
	if (parmsptr->c_flag == 0) {
		retval = kill(Opid, parmsptr->flag1);
	}else{
		retval = cap_kill(Opid, parmsptr->flag1);
	}
	loc_errno = errno;
	 /*
	  * Write to the pipe so Object can proceed.
	  */
	if ( (write(S_O_fd[1], buf, sizeof(buf)))
	    != (sizeof(buf)) ) {
	    w_error(SYSCALL, namestring, err[PIPEWRITE_S], errno);
	    exit(INCOMPLETE);
	}
	wait(&status);

	/*
	 * For positive cases, retval should be zero.
	 */
	if (parmsptr->expect_success == PASS )  {
	    if ( retval == -1 ) {
		w_error(SYSCALL, namestring, err[TESTCALL], loc_errno);
		exit(FAIL);
	    }
	    if ( retval ) {
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
	if (loc_errno != parmsptr->expect_errno) {
	    w_error(SYSCALL, namestring, err[TEST_ERRNO], loc_errno);
	    exit(FAIL);
	}
	exit(PASS);
    }

    /*
     * This is the Object.
     */    
    close(S_O_fd[1]);
    close(O_S_fd[0]);
    /*
     * Set Object uids to test values and verify.
     */
    if (  ( cap_setuid(parmsptr->ruid) ) == -1  ) {
	w_error(SYSCALL, namestring, err[SETUID_O], errno);
	exit(INCOMPLETE);
    }	
    if ( (getuid() != parmsptr->ruid)||(geteuid() != parmsptr->ruid) ) {
	w_error(GENERAL, namestring, err[BADUIDS_O], 0);
	exit(INCOMPLETE);
    }	
    /*
     * Arrange to be killed by the signal
     */
    signal(parmsptr->flag1, SIG_DFL);
    /*
     * Write to the pipe so Subject can proceed.
     */
    if ( (write(O_S_fd[1], buf, sizeof(buf))) != (sizeof(buf)) ) {
	w_error(SYSCALL, namestring, err[PIPEWRITE_O], errno);
	exit(INCOMPLETE);
    }
    
    /*
     * Read what Subject wrote to the pipe, then exit.
     */
    read(S_O_fd[0], buf, sizeof(buf));
    exit(PASS); 
    return(PASS);
}

/* 
 * Test function for cases where Object is NOT child of Subject.
 */
static int kill_dac2(struct dacparms_4 *parmsptr) 
{
    register char i;             /* Wait loop counter. */
    char buf[4];                 /* Used in pipe read/writes. */
    char testname[SLEN];         /* used to write to logfiles */
    char namestring[MSGLEN];     /* Used in error messages. */
    int OtoS_fd[2];              /* O writes, S reads. */
    int StoO_fd[2];              /* S writes, O reads. */
    pid_t forkval1 = 0;          /* O's pid returned to parent */ 
    pid_t forkval2 = 0;          /* S's pid returned to parent */ 
    pid_t retval = 0;            /* Return value of test call. */
    int status[2], waitret[2];   /* For wait. */   

    strcpy(testname,"kill_dac");
    strcpy (buf, "abc");      /* Stuff to write to pipe. */
    sprintf(namestring, "%s, case %d, %s:\n   ", testname,
	     parmsptr->casenum, parmsptr->casename);
    /*
     * Make 2 pipes.
     */
    if ( pipe(OtoS_fd) || pipe(StoO_fd) )  {
	w_error(SYSCALL, namestring, err[F_PIPE], errno);
	return(INCOMPLETE);
    }
    /*
     * Fork O.
     */
    if (  ( forkval1 = fork() ) == -1  ) {
	w_error(SYSCALL, namestring, err[F_FORK], errno);
	return(INCOMPLETE);
    }

    /* 
     * This is the parent after forking O.
     */
    if ( forkval1 )  { 
	/*
	 * Fork S.
	 */
	if (  ( forkval2 = fork() ) == -1  ) {
	    w_error(SYSCALL, namestring, err[F_FORK], errno);
	    return(INCOMPLETE);
	}
    }

    /* 
     * This is the parent after forking S.
     */
    if ( (forkval2) && (forkval1) )  { 
	/* 
	 * Close all pipe fds.
	 */
	close(OtoS_fd[0]);
	close(OtoS_fd[1]);
	close(StoO_fd[0]);
	close(StoO_fd[1]);
	/*
	 * Wait for Children.  For this test, it's OK if one
	 * of the children exits due to a signal.  If both
	 * have been killed, or one returns 2, or any other
	 * unexpected event occurs, return 2. Otherwise, return
	 * 1 if one returns 1, otherwise 0 if one returns 0.
	 */
	waitret[0] = wait(&status[0]);
	waitret[1] = wait(&status[1]);
	for (i = 0; i < 2; i++ ) {
	    if ( waitret[i] < 0 ) {
		w_error(SYSCALL, namestring, err[F_WAIT], errno);
		return(INCOMPLETE);
	    }
	    if (waitret[i] == forkval1) {  
		/* 
		 * OK if object process was kiled 
		 */
		if (WIFSIGNALED(status[i])) {
		    break;
		}
		if (!WIFEXITED(status[i])) {
		    w_error(GENERAL, namestring, err[C_NOTEXIT], 0);
		    return(INCOMPLETE);
		}
	    }
	    if (waitret[i] == forkval2) {  
		/* 
		 * Subject process should exit normally 
		 */
		if (!WIFEXITED(status[i])) {
		    w_error(GENERAL, namestring, err[C_NOTEXIT], 0);
		    return(INCOMPLETE);
		}
	    }
	    if (WEXITSTATUS(status[i])) {
		return(WEXITSTATUS(status[i]));
	    }
	}
	/* 
	 * If we got here, both exited with status 0 or
	 * subject exited 0 and object process was killed. 
	 */
	return(PASS);  
    }


    /* 
     * This is S, the Subject process. 
     */
    if ( !(forkval2) && (forkval1) ) {  
	/*
	 * Close pipe descriptors that S does not use.
	 */
	close(OtoS_fd[1]);
	close(StoO_fd[0]);
	/*
	 * Set ruid and euid to test value and verify.
	 */
	if (  ( cap_setreuid(parmsptr->Sruid, parmsptr->Seuid) ) == -1  ) {
	    w_error(SYSCALL, namestring, err[SETUID_S], errno);
	    exit(INCOMPLETE);
	}	
	if ( ( getuid() != parmsptr->Sruid ) ||
	     (geteuid() != parmsptr->Seuid) ) { 
	    w_error(GENERAL, namestring, err[BADUIDS_S], 0);
	    exit(INCOMPLETE);
	}	
	/*
	 * Do a setsid If this test case requires it.
	 */
	if (parmsptr->flag2 == PASS ) {
	    if ( setsid() == -1 ){
		w_error(SYSCALL, namestring, err[F_SETSID], errno);
		exit(INCOMPLETE);
	    }
	    if ( getpgrp() != getpid() ) {
		w_error(GENERAL, namestring, err[BADPGID], 0);
		exit(INCOMPLETE);
	    }
	}

	/*
	 * Read what O wrote to the pipe.
	 */
	if ( (read(OtoS_fd[0], buf, sizeof(buf))) != sizeof(buf)) {
	    w_error(SYSCALL, namestring, err[PIPEREAD_S], errno);
	    exit(INCOMPLETE);
	}

	/*
	 * Here's the test: Call kill with O's pid
	 * as arg1 and signal as arg2.
	 */
	if (parmsptr->c_flag == 0) {
		retval = kill(forkval1, parmsptr->flag1);
	}else{
		retval = cap_kill(forkval1, parmsptr->flag1);
	}
	/*
	 * For positive cases, retval should be zero.
	 */
	if (parmsptr->expect_success == PASS ) {
	    if ( retval == -1 ) {
		w_error(SYSCALL, namestring, err[TESTCALL], errno);
		exit(FAIL);
	    }
	    if ( retval ) {
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
	if (errno != parmsptr->expect_errno) {
	    w_error(SYSCALL, namestring, err[TEST_ERRNO], errno);
	    exit(FAIL);
	}
	exit(PASS); 
    }

    /* 
     * This is O, the object process. Exit values are all > 2
     * so that parent can distinguish them from Subject's
     * exit values.  If O exits before writing, it will
     * cause the read by S to fail.
     */
    /*
     * Begin O code. Close pipe descriptors that O does not use.
     */
    close(OtoS_fd[0]);
    close(StoO_fd[1]);
    /*
     * Set O's ruid and euid to test value and verify.
     */
    if ( ( cap_setuid(parmsptr->ruid) ) == -1) {
	w_error(SYSCALL, namestring, err[SETUID_O], errno);
	exit(4);
    }
    
    if ( (getuid() != parmsptr->ruid) || (geteuid() !=
					  parmsptr->ruid) ) {
	w_error(GENERAL, namestring, err[BADUIDS_O], errno);
	exit(4);
    }
    /*
     * Arrange to be killed by signal.
     */
    signal(parmsptr->flag1, SIG_DFL);
    
    /*
     * Write to the pipe so S can proceed.
     */
    if ( (write(OtoS_fd[1], buf, sizeof(buf)))
	!= (sizeof(buf)) )      {
	w_error(SYSCALL, namestring, err[PIPEWRITE_O], errno);
	exit(4);
    }
    /*
     * Read what Subject wrote to the pipe, then exit.
     * On positive tests, this child should
     * be killed while waiting to read.
     * On negative tests, subject exits without
     * writing, so read will fail because
     * there's no writer to the pipe.
     */
    read(StoO_fd[0], buf, sizeof(buf));
    exit(PASS);
    return(PASS);
    /*
     * End O code.
     */
}
