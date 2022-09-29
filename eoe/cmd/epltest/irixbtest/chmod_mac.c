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
 * chmod_mac.c -- MAC read tests for chmod() system call.
 */
static int chmod_mac1(struct macinfo *parmsptr);

static struct macinfo
chmod_minfo[] = 
{
/*
 case   case  c_flag  expect errno unused directory   file       process
 name  number         success             msen/mint   msen/mint  msen/mint
*/

"pos00",  0,      0,  PASS,    0,     0,  MSL, MIH,   MSA, MIL, MSA, MIL,
"pos01",  1,      1,  PASS,    0,     0,  MSL, MIH,   MSL, MIL, MSH, MIL,

"neg00",  2,      0,  FAIL, EACCES,   0,  MSL, MIH,   MSL, MIL, MSA, MIL,
"neg01",  3,      0,  FAIL, EACCES,   0,  MSH, MIH,   MSA, MIL, MSA, MIL, 
"neg02",  4,      0,  FAIL, EACCES,   0,  MSL, MIL,   MSA, MIH, MSA, MIL,
"neg03",  5,      0,  FAIL, EACCES,   0,  MSL, MIL,   MSA, MIH, MSA, MIH,

"neg04",  6,      0,  FAIL, EACCES,   0,  MSH, MIH,   MSL, MIL, MSA, MIL, 
"neg05",  7,      0,  FAIL, EACCES,   0,  MSA, MIH,   MSA, MIH, MSA, MIL, 
"neg06",  8,      0,  FAIL, EACCES,   0,  MSL, MIH,   MSL, MIL, MSH, MIL,

};

static char *chmod_mdesc[] = { 
"MS:P > D, P = F; MI:P < D, P = F",   
"MS:P > D, P != F, P MS_HIGH; MI:P < D, P = F capability",  
 
"MS:P > D, P != F; MI:P < D, P = F",   
"MS:P !> D, P = F; MI:P < D, P = F",   
"MS:P > D, P = F; MI:P < D, P != F",   
"MS:P > D, P = F; MI:P !< D, P = F",   

"MS:P !> D, P != F; MI:P < D, P = F",   
"MS:P > D, P = F; MI:P !< D, P != F",   
"MS:P > D, P != F, P MS_HIGH; MI:P < D, P = F capability",  

};

int
chmod_mac(void)
{
    short ncases = 8;        /* number of total test cases */
    int fail = 0;            /* set when a test case fails */
    int incomplete = 0;      /* set when a test case is incomplete */
    short i = 0;             /* test case loop counter */
    char str[MSGLEN];        /* used to write to logfiles */
    char testname[SLEN];     /* used to write to logfiles */

    strcpy(testname,"chmod_mac");
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
	switch (chmod_mac1(&chmod_minfo[i]) ) {
	/*
	 * Write formatted result to raw log.
	 */    
	case PASS:      /* Passed */
	    RAWPASS(chmod_minfo[i].casenum, chmod_minfo[i].casename);
	    break;
	case FAIL:      /* Failed */
	    RAWFAIL(chmod_minfo[i].casenum, chmod_minfo[i].casename,
		    chmod_mdesc[i]);
	    fail = 1;
	    break;
	default:
	    RAWINC(chmod_minfo[i].casenum, chmod_minfo[i].casename);
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

static int chmod_mac1(struct macinfo *parmsptr)
{
    char testname[SLEN];         /* used to write to logfiles */
    char namestring[MSGLEN];     /* Used in error messages. */
    char file[NLEN + NLEN];      /* File name passed to chmod(). */
    char dir[NLEN];              /* Directory containing file. */
    int retval = 0;              /* Return value of test call. */
    int fildes;                  /* File descriptor.*/
    pid_t forkval;               /* Returned value of fork call. */
    int status;                  /* For wait. */   
    int loc_errno;               /* Saves test call errno. */
    mac_t lptr;                  /* MAC label pointer. */

    strcpy(testname,"chmod_mac");
    sprintf(namestring, "%s, case %d, %s:\n   ", testname,
	     parmsptr->casenum, parmsptr->casename);
    /*
     * Create names for temp file and directory.
     */
    sprintf(dir,  "%s%d", testname, parmsptr->casenum);
    sprintf(file, "%s/tmp%d", dir, parmsptr->casenum);

    /* 
     * Fork a child to do setup and test call.
     */
    if (  ( forkval = fork() ) == -1  ) {
	w_error(SYSCALL, namestring, err[F_FORK], errno);
	return(INCOMPLETE);
    }

    if ( forkval  ) { 
	/* 
	 * This is the parent.	Wait for child and return
	 * 2 on unexpected error. Otherwise, return child's 
         * exit code.
	 */
	if ( wait(&status) == -1 ) {
	    w_error(SYSCALL, namestring, err[F_WAIT], errno);
	    return(INCOMPLETE);
    	}
	if ( !WIFEXITED(status)  ) {
	    w_error(GENERAL, namestring, err[C_NOTEXIT], 0);
	    return(INCOMPLETE);
    	}
	return(WEXITSTATUS(status) );
    }

    /* 
     * This is the child.
     */
    /* 
     * Make a temp directory, and set its label.
     */
    cap_rmdir(dir);
    if ( cap_mkdir(dir, 0700) ) {
	w_error(SYSCALL, namestring, err[F_MKDIR], errno);
	exit(INCOMPLETE);
    }
    
    /*
     * Malloc a mac label pointer.
     */
    lptr = mac_get_proc();
    
    /* 
     * Set directory label.
     */
    lptr->ml_msen_type = parmsptr->dir_msen;
    lptr->ml_mint_type = parmsptr->dir_mint;
    if ( cap_setlabel(dir, lptr) ) {
	w_error(SYSCALL, namestring, err[SETLABEL_DIR], errno);
	cap_rmdir(dir);
	exit(INCOMPLETE);
    }
    
    /*
     * Create a file, close it, and set its label.
     */
    if ( (fildes = cap_open(file, O_CREAT, 0700)) < 0 ) {
	w_error(SYSCALL, namestring, err[F_CREAT], errno);
	exit(INCOMPLETE);
    }
    close(fildes);
    lptr->ml_msen_type = parmsptr->file_msen;
    lptr->ml_mint_type = parmsptr->file_mint;
    if ( cap_setlabel(file, lptr) ) {
	w_error(SYSCALL, namestring, err[SETLABEL_FILE], errno);
	cap_unlink(file);
	cap_rmdir(dir);
	exit(INCOMPLETE);
    }
    
    
    /* 
     * Set process label
     */
    lptr->ml_msen_type = parmsptr->proc_msen;
    lptr->ml_mint_type = parmsptr->proc_mint;
    if ( cap_setplabel(lptr) ) {
	w_error(SYSCALL, namestring, err[F_SETPLABEL], errno);
	cap_unlink(file);
	cap_rmdir(dir);
	exit(INCOMPLETE);
    }
    
    /*
     * Here's the test: Call chmod(file, 0700).
     */
    if (parmsptr->c_flag == 0) {
	retval = chmod(file, 0700);
    }else{
	retval = cap_chmod(file, 0700);
    }
    loc_errno = errno;
    
    /* 
     * Reset process label and unlink dir.
     */
    lptr->ml_msen_type = MSH;
    lptr->ml_mint_type = MIE;
    if ( cap_setplabel(lptr) ) {
	w_error(SYSCALL, namestring, err[F_SETPLABEL], errno);
	exit(INCOMPLETE);
    }
    mac_free(lptr);
    cap_unlink(file);
    cap_rmdir(dir);
    /*
     * Positive cases: Retval should be 0.
     */
    if (parmsptr->expect_success == PASS ) {
	if ( retval == -1 ) {
	    w_error(SYSCALL, namestring, err[TESTCALL], loc_errno);
	    exit(FAIL);
	} 
	if (retval) {
	    w_error(PRINTNUM, namestring, err[TEST_RETVAL], retval);
	    exit(FAIL);
	}
	exit(PASS);
    }
    
    /*
     * For negative test cases, retval should be -1 and
     * errno should equal parmsptr->expect_errno.
     * If not, write error message and exit 1.
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
    return(PASS);
}
