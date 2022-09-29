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
#include "mac.h"
#include <sys/prctl.h>
#include <signal.h>
int blockproc_mac();
/*
 * blockproc_mac.c -- MAC read tests for Blockproc() system call.
 */
static int blockproc_mac1(struct macinfo_2 *parmsptr);

static struct macinfo_2
	blockproc_minfo[] = 
{
	/*
	  case  case   c_flag  expect              Object      Subject   
	  name  number         success  errno     MSEN MINT   MSEN MINT  UID
	  */
	"pos00",  0,     1,    PASS,  0,          MSL, MIE,   MSA, MIE, SUPER,
	"pos01",  1,     1,    PASS,  0,          MSL, MIH,   MSH, MIL, SUPER,
	"pos02",  2,     0,    PASS,  0,          MSL, MIE,   MSL, MIE, TEST0,
	"neg00",  3,     0,    FAIL,  EPERM,      MSL, MIE,   MSH, MIE, TEST0,
	"neg01",  4,     0,    FAIL,  EPERM,      MSH, MIL,   MSL, MIH, TEST0,
	"neg02",  0,     0,    FAIL,  EPERM,      MSL, MIE,   MSA, MIE, SUPER,
	"neg03",  1,     0,    FAIL,  EPERM,      MSL, MIH,   MSH, MIL, SUPER,
};

static char *blockproc_mdesc[] = { 
	"SUSER, MS:S = O; MI:S = O",
	"SUSER, MS:S != O; MI:S != O",   
	"Not SUSER, MS:S = O; MI:S = O",
	
	"Not SUSER, MS:S != O, MS HIGH; MI:S = O",   
	"Not SUSER, MS:S = O; MI:S != O",   
};

int
blockproc_mac(void)
{
	short ncases =  5;        /* number of total test cases */
	int fail = 0;            /* set when a test case fails */
	int incomplete = 0;      /* set when a test case is incomplete */
	short i = 0;             /* test case loop counter */
	char str[MSGLEN];        /* used to write to logfiles */
	char testname[SLEN];     /* used to write to logfiles */
	
	strcpy(testname,"blockproc_mac");
	/*
	 * Write formatted info to raw log.
	 */   
	RAWLOG_SETUP(testname, ncases);
	/*
	 * Call function for each test case.
	 */    
	for (i = 0; i < ncases; i++) {
		flush_raw_log();   
		switch (blockproc_mac1(&blockproc_minfo[i]) ) {
			/*
			 * Write formatted result to raw log.
			 */    
			case PASS:      /* Passed */
			RAWPASS(blockproc_minfo[i].casenum,
				blockproc_minfo[i].casename);
			break;
			case FAIL:      /* Failed */
			RAWFAIL(blockproc_minfo[i].casenum,
				blockproc_minfo[i].casename,
				blockproc_mdesc[i]);
			fail = 1;
			break;
			default:
			RAWINC(blockproc_minfo[i].casenum,
			       blockproc_minfo[i].casename);
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

static int blockproc_mac1(struct macinfo_2 *parmsptr)
{
	char buf[4];                 	/* Used in pipe read/writes. */
	char testname[SLEN];         	/* used to write to logfiles */
	char namestring[MSGLEN];     	/* Used in error messages. */
	int OtoS_fd[2];              	/* O writes, S reads. */
	int StoO_fd[2];              	/* S writes, O reads. */
	pid_t retval = 0;            	/* Return value of test call. */
	pid_t forkval1 = 0;          	/* O's pid returned to parent */ 
	pid_t forkval2 = 0;          	/* S's pid returned to parent */ 
	mac_t lptr;                     /* MAC label pointer. */
	int loc_errno = 0;          	/* Saves test call errno. */
	int blockval = 0;               /* Saves block test value. */
	int waitret0 = 0;      		/* For wait. */   
	int waitret1 = 0;      		/* For wait. */   
	int status0 = 0;                /* For wait. */   
	int status1 = 0;                /* For wait. */   
	int exitstatus0 = 0;            /* For wait. */   
	int exitstatus1 = 0;            /* For wait. */   
	
	strcpy(testname,"blockproc_mac");
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
		close(OtoS_fd[0]);
		close(OtoS_fd[1]);
		close(StoO_fd[0]);
		close(StoO_fd[1]);
		/*
		 * Wait for Children. Return 2 if either child
		 * has been killed or returns 2 or any other
		 * unexpected event occurs.  Otherwise, return 1 
		 * if one child returns 1, otherwise 0.
		 */
		waitret0 = wait(&status0);
		waitret1 = wait(&status1);
		/* 
		 * kludge to prevent irixbtest from hanging due to
		 * procblock bug: don't wait again, leave zombies.
		 * 
		 */
		if (( waitret0 == -1 ) || ( waitret1 == -1 )) {
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
		close(OtoS_fd[1]);
		close(StoO_fd[0]);
		
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
		if (  ( cap_setreuid(SUPER, parmsptr->uid) ) == -1  ) {
			w_error(SYSCALL, namestring, err[SETREUID_S], errno);
			exit(INCOMPLETE);
		}	
		if ( (getuid() != SUPER) || (geteuid() !=
						   parmsptr->uid) ) {
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
		 * Here's the test: Call Blockproc with O's pid as
		 * arg1.
		 */
		if (parmsptr->c_flag != 0) {
			retval = cap_blockproc(forkval1);
		}else{
			retval = blockproc(forkval1);
		}
		loc_errno = errno;

		/*
		 * Set ruid and euid back to Superuser.
		 */
		if (  ( cap_setreuid(SUPER, SUPER) ) == -1  ) {
			w_error(SYSCALL, namestring, err[SETREUID_S], errno);
			exit(INCOMPLETE);
		}	

		/*
		 * Call prctl to get blockval, then call unblockproc.
		 */
		if (parmsptr->c_flag != 0) {
			blockval = cap_prctl(PR_ISBLOCKED, forkval1);
			cap_unblockproc(forkval1);
		}else{
			blockval = prctl(PR_ISBLOCKED, forkval1);
			unblockproc(forkval1);
		}
		/*
		 * Write to the pipe so O can go away.
		 */
		if ( (write(StoO_fd[1], buf, sizeof(buf))) != (sizeof(buf)) ) {
			w_error(SYSCALL, namestring, err[PIPEWRITE_S],
				loc_errno);
			exit(INCOMPLETE);
		}

		/*
		 * For positive cases, retval should be 0  and
		 * prctl(PR_ISBLOCKED) should equal 1.
		 */
		if (parmsptr->expect_success == PASS ) {
			if ( retval == -1 ){
				w_error(SYSCALL, namestring,
					err[TESTCALL], loc_errno);
				exit(FAIL);
			} 
			if (retval != 0) {
				w_error(PRINTNUM, namestring,
					err[TEST_RETVAL], retval);
				exit(FAIL);
			}
			if (blockval != 1) {
				w_error(GENERAL, namestring,
					err[NOTBLOCKED], 0);
				exit(FAIL);
			}
			exit(PASS);
		}
		
		/*
		 * For negative test cases, retval should be -1 and
		 * loc_errno should equal parmsptr->expect_errno.
		 */
		if (retval != -1){
			w_error(PRINTNUM, namestring,
				err[TEST_RETVAL], retval);
			exit(FAIL);
		}
		if (loc_errno != parmsptr->expect_errno) {
			w_error(SYSCALL, namestring, err[TEST_ERRNO],
				loc_errno);
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
	close(OtoS_fd[0]);
	close(StoO_fd[1]);
	
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
	if  ( write(OtoS_fd[1], buf, sizeof(buf))
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










