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
#include "cap.h"

/*
 * set_pcap.c -- Capability tests for chown() system call.
 */
static int set_pcap1(struct set_pcap1 *parmsptr);

/*
 * Each structure contains information for one test case.
 * Variables are uids of Subject and Object processes,
 * whether the test call is expected to succeed (positive case) or fail
 * (negative case), and errno expected in each negative case. 
 */
static struct set_pcap1
set_pcaps[] = {
/*
 *name    #  Sruid  Seuid  success  errno   cap flags
 */
"pos00",  0, SUPER, SUPER, PASS ,      0,    "CAP_SETPCAP+e",
"pos01",  1, SUPER, SUPER, PASS ,      0,    "CAP_SETPCAP+p",
"pos02",  2, TEST0, TEST0, PASS ,      0,    "CAP_SETPCAP+e",
"neg00",  3, TEST0, TEST0, FAIL,   EPERM,  "CAP_SETPCAP+p"
};

/* 
 * Description strings, corresponding with test cases, 
 * to be printed to the raw log if that case fails.
 */
static char *pcap_cdesc[] = { 
"Seuid super, Effe",	/* 0 */
"Seuid super, Perm",	/* 1 */
"Seuid, Effe",		/* 2 */
"Seuid, Perm",		/* 3 */
};

int
set_pcap(void)
{
	char str[MSGLEN];        /* used to write to logfiles */
	char testname[SLEN];     /* used to write to logfiles */
	short ncases1 = 4;       /* number of test cases */
	int fail = 0;            /* set when test case fails */
	int incomplete = 0;      /* set when test case incomplete */
	int retval;              /* value returned on function call */
	short i = 0;             /* test case loop counter */
	
	strcpy(testname,"set_pcap");
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
		 * Call set_pcap1()
		 */    
		retval = set_pcap1(&set_pcaps[i]);
		/*
		 * Write formatted result to raw log.
		 */    
		switch ( retval ) {
			case PASS:      /* Passed */
			RAWPASS(set_pcaps[i].casenum, set_pcaps[i].casename);
			break;
			case FAIL:      /* Failed */
			RAWFAIL(set_pcaps[i].casenum, set_pcaps[i].casename,
				pcap_cdesc[i]);
			fail = 1;
			break;
			default:
			RAWINC(set_pcaps[i].casenum, set_pcaps[i].casename);
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
static int
set_pcap1(struct set_pcap1 *parmsptr)
{
	char testname[SLEN];         /* used to write to logfiles */
	char namestring[MSGLEN];     /* Used in error messages. */
	pid_t forkval = 0;           /* pid returned to parent */ 
	pid_t retval = 0;            /* Return value of test call. */
	int errval = 0;		     /* Error value of test call. */
	int status;		     /* For wait. */
	cap_t cptr;		     /* Pointer to cap label */
	
	strcpy(testname,"set_pcap");
	sprintf(namestring, "%s, case %d, %s:\n   ", testname,
		parmsptr->casenum, parmsptr->casename);

	/*
	 * Fork a child to do some testing.
	 */
	if ( ( forkval = fork() ) == -1 ) {
		w_error(SYSCALL, namestring, err[F_FORK], errno);
		return(INCOMPLETE);
	}
		
	/* 
	 * This is the parent after forking.
	 */
	if ( forkval )  { 
		/* 
		 * This is the parent.	Wait for child and return
		 * 2 on unexpected error. Otherwise, return child's 
		 * exit code.
		 */
		if ( wait(&status) == -1 ) {
			w_error(SYSCALL, namestring, err[F_WAIT], errno);
			return(INCOMPLETE);
    		}
		if ( !WIFEXITED(status) ) {
			w_error(GENERAL, namestring, err[C_NOTEXIT], 0);
			return(INCOMPLETE);
		}
		if ( WEXITSTATUS(status) ) {
			w_error(GENERAL, namestring, err[C_BADEXIT], 0);
			return(INCOMPLETE);
    		}

		/* 
		 * If we got here, child exited with status 0
		 */
		return(PASS);
	} /*end parent*/
	

	/* 
	 * This is the child process.
	 * Create capability label and set Subject.
	 */
	if ( ( cptr = cap_from_text(parmsptr->cflag) ) == (cap_t) NULL ) {
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
	if ( ( cap_setreuid(parmsptr->Sruid, parmsptr->Seuid) ) == -1 ) {
		w_error(SYSCALL, namestring, err[SETUID_S], errno);
		exit(INCOMPLETE);
	}	
	if ( ( getuid() != parmsptr->Sruid) ||
		(geteuid() != parmsptr->Seuid) ) { 
		w_error(GENERAL, namestring, err[BADUIDS_S], 0);
		exit(INCOMPLETE);
	}
	if ( ( cptr = cap_from_text("CAP_FOWNER+e") ) == (cap_t) NULL ) {
		w_error(SYSCALL, namestring, err[F_CREPLABEL], errno);
		exit(INCOMPLETE);
	}	
	/*
	 * Here's the test: Call cap_set_proc(cptr)
	 */
	retval = cap_set_proc(cptr);
	errval = errno;
	cap_free(cptr);
	/*
	 * Positive test cases
	 */
	if ( parmsptr->expect_success == PASS  ) {


		if ( retval == -1 ) {
			w_error(SYSCALL, namestring, err[TESTCALL], errval);
			exit(FAIL);
		}
		if ( retval ) {
			w_error(PRINTNUM, namestring, err[TEST_RETVAL], retval);
			exit(FAIL);
		}
	}
	exit(PASS);
	/*
	 * For negative test cases, retval should be -1 and
	 * errno should equal parmsptr->expect_errno.
	 */
	if ( retval != -1 ) {
		w_error(PRINTNUM, namestring, err[TEST_RETVAL], retval);
		exit(FAIL);
	}
	if ( errno != parmsptr->expect_errno ) {
		w_error(SYSCALL, namestring, err[TEST_ERRNO], errval);
		exit(FAIL);
	}
	exit(PASS);
	return(PASS);
}
