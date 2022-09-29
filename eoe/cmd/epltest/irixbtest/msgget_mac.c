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
#include <sys/ipc.h>
#include <sys/msg.h>

/*
 * msgget_mac.c -- MAC read tests for Msgget() system call.
 */
static int msgget_mac1(struct macinfo_2 *parmsptr);

static struct macinfo_2
msgget_minfo[] = 
{
/*
 case  case    c_flag   expect              Object      Subject   
 name  number           success  errno     MSEN MINT   MSEN MINT  UID
*/
"pos00",  0,       0,   PASS,    0,        MSL, MIE,   MSL, MIE, SUPER,
"pos01",  1,       0,   PASS,    0,        MSL, MIE,   MSH, MIE, SUPER,
"neg00",  2,       0,   FAIL,    EACCES,   MSH, MIE,   MSA, MIE, SUPER,
"neg01",  3,       0,   FAIL,    EACCES,   MSA, MIL,   MSA, MIH, SUPER,
"pos02",  4,       0,   PASS,    0,        MSL, MIE,   MSL, MIE, TEST0,
"neg02",  5,       0,   FAIL,    EACCES,   MSL, MIE,   MSH, MIE, TEST0,
"neg03",  6,       0,   FAIL,    EACCES,   MSL, MIL,   MSL, MIH, TEST0,
};

static char *msgget_mdesc[] = { 
"SUSER, MS:S = O; MI:S = O", 
"SUSER, MS:S != O, MS HIGH; MI:S = O",   
"SUSER, MS:S != O; MI:S = O",  
"SUSER, MS:S = O; MI:S != O",   
"Not SUSER, MS:S = O; MI:S = O", 
"Not SUSER, MS:S != O, MS HIGH; MI:S = O",   
"Not SUSER, MS:S = O; MI:S != O",  
};


int
msgget_mac(void)
{
    short ncases =  7;       /* number of total test cases */
    int fail = 0;            /* set when a test case fails */
    int incomplete = 0;      /* set when a test case is incomplete */
    short i = 0;             /* test case loop counter */
    char str[MSGLEN];        /* used to write to logfiles */
    char testname[SLEN];     /* used to write to logfiles */

    strcpy(testname,"msgget_mac");
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
	switch (msgget_mac1(&msgget_minfo[i]) ) {
	/*
	 * Write formatted result to raw log.
	 */    
	case PASS:      /* Passed */
	    RAWPASS(msgget_minfo[i].casenum, msgget_minfo[i].casename);
	    break;
	case FAIL:      /* Failed */
	    RAWFAIL(msgget_minfo[i].casenum, msgget_minfo[i].casename,
		    msgget_mdesc[i]);
	    fail = 1;
	    break;
	default:
	    RAWINC(msgget_minfo[i].casenum, msgget_minfo[i].casename);
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

static int
msgget_mac1(struct macinfo_2 *parmsptr)
{
    char buf[4];                 /* Used in pipe read/writes. */
    char testname[SLEN];         /* used to write to logfiles */
    char namestring[MSGLEN];     /* Used in error messages. */
    pid_t retval = 0;            /* Return value of test call. */
    pid_t forkval1 = 0;          /* O's pid returned to parent */ 
    pid_t forkval2 = 0;          /* S's pid returned to parent */ 
    mac_t lptr;                  /* MAC label pointer. */
    int loc_errno;               /* Save errno on test call. */
    int status;                  /* For wait. */   
    int msqid;                   /* Returned by msgget create call. */
    key_t key;		         /* Key value. */

    strcpy(testname,"msgget_mac");
    strcpy (buf, "abc");      /* Stuff to write to pipe. */
    sprintf(namestring, "%s, case %d, %s:\n   ", testname,
	     parmsptr->casenum, parmsptr->casename);
    /*
     * Fork Child to do setup. Child will fork Grandchild (Subject)
     * to do test call.
     */
    if ( ( forkval1 = fork() ) == -1 ) {
	w_error(SYSCALL, namestring, err[F_FORK], errno);
	return(INCOMPLETE);
    }

    /* 
     * This is the parent after forking Child.
     */
    if (forkval1)  { 
	/*
	 * Wait for Child. Return 2 if child
	 * has been killed or returns 2 or any other
	 * unexpected event occurs.  Otherwise, return 1 
	 * if child returns 1, otherwise 0.
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
     * This is the Child process, before forking Grandchild. 
     */
    if (!forkval1)  {  
	/*
	 * Malloc a mac_label pointer.
	 */
	lptr = mac_get_proc();

	/*
	 * Set Object process label.
	 */
	lptr->ml_msen_type = parmsptr->O_msen;
	lptr->ml_mint_type = parmsptr->O_mint;
	if ( cap_setplabel(lptr) ) {
	    w_error(SYSCALL, namestring, err[F_SETPLABEL], errno);
	    exit(INCOMPLETE);
	}

	/*
	 * Set euid to test value and verify.
	 */
	if ( ( cap_setreuid(-1, parmsptr->uid) ) == -1  ) {
	    w_error(SYSCALL, namestring, err[SETREUID_O], errno);
	    exit(INCOMPLETE);
	}	
	if ( geteuid() != parmsptr->uid ) {
	    w_error(GENERAL, namestring, err[BADUIDS_O], 0);
	    exit(INCOMPLETE);
	}	
	/*
	 * Create a message queue with Object process label.
	 */
	key = getpid();
	if ( (msqid = msgget(key,(00666|IPC_EXCL|IPC_CREAT))) == -1 ) {
	    w_error (SYSCALL, namestring, err[F_MSGGET], errno);
	    exit(INCOMPLETE);
	}
	/*
	 * Fork Grandchild to do test call.
	 */
	if ( ( forkval2 = fork() ) == -1 ) {
	    w_error(SYSCALL, namestring, err[F_FORK], errno);
	    exit(INCOMPLETE);
	}
    }
    /* 
     * This is the Child after forking Grandchild.
     */
    if ( (!forkval1) && (forkval2) )  {  
	/*
	 * Wait for Grandchild and exit with 2 on
	 * unexpected error, otherwise Grandchild's exit value.
	 */
	if ( wait(&status) == -1 ) {
	    w_error(SYSCALL, namestring, err[F_WAIT], errno);
	    exit(INCOMPLETE);
    	}
	if ( !WIFEXITED(status) ) {
	    w_error(GENERAL, namestring, err[C_NOTEXIT], 0);
	    exit(INCOMPLETE);
	}
	exit(WEXITSTATUS(status));

    }
    /* 
     * This is the Grandchild (Subject).
     */
    
    /*
     * Reset euid to Suser 
     */
    if ( ( cap_setreuid(-1, SUPER)) == -1 ) {
	w_error(SYSCALL, namestring, err[SETREUID_S], errno);
	exit(INCOMPLETE);
    }	
    if (geteuid() != SUPER) {
	w_error(GENERAL, namestring, err[BADUIDS_S], 0);
	exit(INCOMPLETE);
    }	
    /*
     * Set Subject process label.
     */
    lptr->ml_msen_type = parmsptr->S_msen;
    lptr->ml_mint_type = parmsptr->S_mint;
    if ( cap_setplabel(lptr) ) {
	w_error(SYSCALL, namestring, err[F_SETPLABEL], errno);
	exit(INCOMPLETE);
    }
    
    /*
     * Set euid to test value and verify.
     */
    if ( (cap_setreuid(SUPER, parmsptr->uid) ) == -1  ) {
	w_error(SYSCALL, namestring, err[SETREUID_S], errno);
	exit(INCOMPLETE);
    }	
    if ( geteuid() != parmsptr->uid ) {
	w_error(GENERAL, namestring, err[BADUIDS_S], 0);
	exit(INCOMPLETE);
    }	
    /*
     * Here's the test: Call msgget(key, 0666), save
     * retval and errno.
     */
    retval = msgget(key, 0666);
    loc_errno = errno;
    
    /*
     * Cleanup: reset euid to suser, setplabel 
     * to MSH,MIE, then remove msqid with msgctl.
     */
    cap_setreuid(parmsptr->uid, SUPER);
    lptr->ml_msen_type = MSH;
    lptr->ml_mint_type = MIE;
    cap_setplabel(lptr);
    mac_free(lptr);
    msgctl(msqid, IPC_RMID, (char *)0);
    
    /*
     * For positive cases, retval should be msqid.
     */
    if ( parmsptr->expect_success == PASS  ) {
	if ( retval == -1 ){
	    w_error(SYSCALL, namestring, err[TESTCALL], loc_errno);
	    exit(FAIL);
	} 
	if ( retval != msqid ) {
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
    if ( loc_errno != parmsptr->expect_errno ) {
	w_error(SYSCALL, namestring, err[TEST_ERRNO], loc_errno);
	exit(FAIL);
    }
    exit(PASS);
    return(PASS);
}
