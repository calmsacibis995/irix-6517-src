/*
 * Copyright 1993, Silicon Graphics, Inc. 
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
#include <sys/prctl.h>
int blockproc_dac();

/*
 * blockproc_dac.c -- DAC tests for blockproc() system call.
 */
static int blockproc_dac1(struct dacparms_5 *parmsptr); 

/*
 * Each structure contains information for one test case.
 * Variables are uids of Subject and Object processes,
 * whether the test call is expected to succeed (positive case) or fail
 * (negative case), and errno expected in each negative case. 
 */
static struct dacparms_5
	blockproc_dparms[] = 
{
	/*
	 * name   #  Oruid  unused Sruid  Seuid  c_flag  success  errno
	 */
	"pos00",  0, TEST0, TEST0, TEST1, SUPER,  1,     PASS ,      0,
	"pos01",  1, TEST0, TEST2, TEST0, TEST1,  0,     PASS ,      0,
	"pos02",  2, TEST0, TEST2, TEST1, TEST0,  0,     PASS ,      0,
	"neg01",  3, TEST2, TEST3, TEST1, TEST0,  0,     FAIL,    EPERM,
	"neg02",  4, TEST0, TEST0, TEST1, SUPER,  0,     FAIL ,   EPERM,
};

/* 
 * Description strings, corresponding with test cases, 
 * to be printed to the raw log if that case fails.
 */
static char *blockproc_ddesc[] = 
{ 
	"S euid suser, S ruid differs",   /* 0 */
	"S ruid = O uid",                 /* 1 */
	"S euid = O uid",                 /* 2 */
	"S ruid & euid != O uid", 	    /* 3 */
};

int
blockproc_dac(void)
{
	char str[MSGLEN];        /* used to write to logfiles */
	char testname[SLEN];     /* used to write to logfiles */
	short ncases1 = 4;       /* number of test cases */
	int fail = 0;            /* set when test case fails */
	int incomplete = 0;      /* set when test case incomplete */
	int retval;              /* value returned on function call */
	short i = 0;             /* test case loop counter */
	
	strcpy(testname,"blockproc_dac");
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
		 * Call blockproc_dac1()
		 */    
		retval = blockproc_dac1(&blockproc_dparms[i]);
		/*
		 * Write formatted result to raw log.
		 */    
		switch ( retval ) {
			case PASS:      /* Passed */
			RAWPASS(blockproc_dparms[i].casenum, blockproc_dparms[i].casename);
			break;
			case FAIL:      /* Failed */
			RAWFAIL(blockproc_dparms[i].casenum, blockproc_dparms[i].casename,
				blockproc_ddesc[i]);
			fail = 1;
			break;
			default:
			RAWINC(blockproc_dparms[i].casenum, blockproc_dparms[i].casename);
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
 * Test function.
 */
static int blockproc_dac1(struct dacparms_5 *parmsptr) 
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
	
	strcpy(testname,"blockproc_dac");
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
		 * have been blockproced, or one returns 2, or any other
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
		 * If we got here, both exited with status 0
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
		if (  ( cap_setreuid(parmsptr->Sruid, parmsptr->Seuid) ) == -1 ) {
			w_error(SYSCALL, namestring, err[SETUID_S], errno);
			exit(INCOMPLETE);
		}	
		if ( ( getuid() != parmsptr->Sruid ) ||
		    (geteuid() != parmsptr->Seuid) ) { 
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
		 * Here's the test: Call blockproc with O's pid
		 * as arg1.
		 */
		if (parmsptr->c_flag != 0) {
			retval = cap_blockproc(forkval1);
		}else{
			retval = blockproc(forkval1);
		}
		/*
		 * For positive cases, retval should be zero and
		 * prctl(PR_ISBLOCKED) should equal 1.
		 */
		if (parmsptr->expect_success == PASS ) {
			if ( retval == -1 ) {
				w_error(SYSCALL, namestring, err[TESTCALL], errno);
				exit(FAIL);
			}
			if ( retval ) {
				w_error(PRINTNUM, namestring, err[TEST_RETVAL], retval);
				if (parmsptr->c_flag != 0) {
					retval = cap_unblockproc(forkval1);
				}else{
					retval = unblockproc(forkval1);
				}
				exit(FAIL);
			}
			if (cap_prctl(PR_ISBLOCKED, forkval1) != 1) {
				w_error(GENERAL, namestring, err[NOTBLOCKED], 0);
				exit(FAIL);
			}
			if (parmsptr->c_flag != 0) {
				retval = cap_unblockproc(forkval1);
			}else{
				retval = unblockproc(forkval1);
			}
			exit(PASS);
		}
		
		/*
		 * For negative test cases, retval should be -1 and
		 * errno should equal parmsptr->expect_errno.
		 */
		if ( retval != -1 ) {
			w_error(PRINTNUM, namestring, err[TEST_RETVAL], retval);
			if (parmsptr->c_flag != 0) {
				retval = cap_unblockproc(forkval1);
			}else{
				retval = unblockproc(forkval1);
			}
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
	if ((cap_setuid(parmsptr->ruid)) == -1) {
		w_error(SYSCALL, namestring, err[SETUID_O], errno);
		exit(4);
	}
	
	if ( (getuid() != parmsptr->ruid) || (geteuid() !=
					      parmsptr->ruid) ) {
		w_error(GENERAL, namestring, err[BADUIDS_O], errno);
		exit(4);
	}
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
	 * be blocked while waiting to read.
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
