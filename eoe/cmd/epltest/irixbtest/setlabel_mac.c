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
 * setlabel_mac.c -- MAC tests for mac_set_file() system call.
 */
static int setlabel_mac0(struct macinfo *parmsptr);

static struct macinfo
setlabel_minfo0[] = 
{
/*
            c_flag  expect expect        Directory     File     Process
 name   num         success errno  Suser msen mint   msen mint  msen mint
*/   
"pos00", 0,     0,    PASS,      0, PASS, MSL, MIH,   MSA, MIL,  MSA, MIL,
"neg00", 1,     0,    FAIL, EACCES, PASS, MSL, MIH,   MSL, MIL,  MSA, MIL,
"neg01", 2,     0,    FAIL, EACCES, PASS, MSL, MIH,   MSA, MIH,  MSA, MIL,
"neg02", 3,     0,    FAIL, EACCES, PASS, MSH, MIH,   MSA, MIL,  MSA, MIL,
"neg03", 4,     0,    FAIL, EACCES, PASS, MSL, MIL,   MSA, MIH,  MSA, MIH,
"pos01", 5,     0,    PASS,      0, PASS, MSL, MIH,   MSL, MIL,  MSH, MIL,
"neg04", 6,     0,    FAIL, EPERM,  FAIL, MSL, MIH,   MSA, MIL,  MSA, MIL,
"neg05", 7,     0,    FAIL, EPERM,  FAIL, MSL, MIH,   MSL, MIL,  MSH, MIL,
"neg06", 8,     0,    FAIL, EACCES, PASS, MSH, MIH,   MSL, MIL,  MSA, MIL,
"neg07", 9,     0,    FAIL, EACCES, PASS, MSL, MIL,   MSA, MIL,  MSA, MIH,
};

static char *setlabel_mdesc0[] = { 
"Suser; MSEN:P > D, P = F; MI:P < D, P = F",  
"Suser; MSEN:P > D, P != F; MI:P < D, P = F",  
"Suser; MSEN:P > D, P = F; MI:P < D, P != F",  
"Suser; MSEN:P !> D, P = F; MI:P < D, P = F",  
"Suser; MSEN:P > D, P = F; MI:P !< D, P = F",  
"Suser at Sys High; MSEN:P > D, P > F; MI:P < D, P = F",  
"Not Suser; MSEN:P > D, P = F; MI:P < D, P = F",  
"Not Suser at Sys High; MSEN:P > D, P > F; MI:P < D, P = F",  
"Suser; MSEN:P !> D, P != F; MI:P < D, P = F",  
"Suser; MSEN:P > D, P = F; MI:P !< D, P != F"
};

int
setlabel_mac(void)
{
    short ncases =  10;       /* number of mac test cases */
    int fail = 0;            /* set when a test case fails */
    int incomplete = 0;      /* set when a test case is incomplete */
    short i = 0;             /* test case loop counter */
    char str[MSGLEN];        /* used to write to logfiles */
    char testname[SLEN];     /* used to write to logfiles */

    strcpy(testname,"setlabel_mac");
    /*
     * Write formatted info to raw log.
     */   
    RAWLOG_SETUP(testname, (ncases));
    /*
     * Call function for each test case.
     */    
    for (i = 0; i < ncases; i++) {
	/*
	 * Flush output streams before calling a function that forks! 
	 */    
	flush_raw_log();   
	switch (setlabel_mac0(&setlabel_minfo0[i]) ) {
	/*
	 * Write formatted result to raw log.
	 */    
	case PASS:      /* Passed */
	    RAWPASS(setlabel_minfo0[i].casenum, setlabel_minfo0[i].casename);
	    break;
	case FAIL:      /* Failed */
	    RAWFAIL(setlabel_minfo0[i].casenum, setlabel_minfo0[i].casename,
		    setlabel_mdesc0[i]);
	    fail = 1;
	    break;
	default:
	    RAWINC(setlabel_minfo0[i].casenum, setlabel_minfo0[i].casename);
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
setlabel_mac0(struct macinfo *parmsptr)
{
    char testname[SLEN];         /* used to write to logfiles */
    char namestring[MSGLEN];     /* Used in error messages. */
    char file[NLEN + NLEN];      /* File name passed to mac_set_file(). */
    char dir[NLEN];              /* Directory containing file. */
    int retval = 0;              /* Return value of test call. */
    int fildes;                  /* File descriptor.*/
    pid_t forkval;               /* Returned value of fork call. */
    int status;                  /* For wait. */   
    int loc_errno;               /* Saves test call errno. */
    mac_t lptr;                  /* MAC label pointer. */

    strcpy(testname,"setlabel_mac");
    sprintf(namestring, "%s, case %d, %s:\n   ", testname,
	     parmsptr->casenum, parmsptr->casename);
    /*
     * Create names for temp file and directory.
     */
    sprintf(dir, "%s%d", testname, parmsptr->casenum);
    sprintf(file, "%s/tmp%d", dir, parmsptr->casenum);

    /* 
     * Fork a child to do setup and test call.
     */
    if ( ( forkval = fork() ) == -1 ) {
	w_error(SYSCALL, namestring, err[F_FORK], errno);
	return(INCOMPLETE);
    }

    if ( forkval ) { 
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
	return(WEXITSTATUS(status) );
    }

    /* 
     * This is the child.  
     */
    
    /*
     * Malloc a mac_label pointer
     */
    lptr = mac_get_proc();

    /* 
     * Make a temp directory
     */
    cap_rmdir(dir);
    if ( cap_mkdir(dir, 0700) ) {
	w_error(SYSCALL, namestring, err[F_MKDIR], errno);
	exit(INCOMPLETE);
    }
    
    /* 
     * Set directory label
     */
    lptr->ml_msen_type = parmsptr->dir_msen;
    lptr->ml_mint_type = parmsptr->dir_mint;
    if ( cap_setlabel(dir, lptr) ) {
	w_error(SYSCALL, namestring, err[SETLABEL_DIR], errno);
	cap_rmdir(dir);
	exit(INCOMPLETE);
    }
    
    /*
     * Create a file, close it, and set its label 
     */
    if ( (fildes = cap_open(file, O_CREAT, 0700)) < 0 ) {
	w_error(SYSCALL, namestring, err[F_CREAT], errno);
	cap_rmdir(dir);
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
     * If this is not a Superuser case, chown file and dir to TEST0
     */
    if ( parmsptr->flag == FAIL ) {
	if ( cap_chown(file, TEST0, 995) ) {
	    w_error(SYSCALL, namestring, err[F_CHOWN], errno);
	    cap_unlink(file);
	    cap_rmdir(dir);
	    exit(INCOMPLETE);
	}
	if ( cap_chown(dir, TEST0, 995) ) {
	    w_error(SYSCALL, namestring, err[F_CHOWN], errno);
	    cap_unlink(file);
	    cap_rmdir(dir);
	    exit(INCOMPLETE);
	}
    }
    /* 
     * Set process label for this test case.
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
     * If this is not a Superuser case, set EUID to TEST0
     */
    if (parmsptr->flag == FAIL) {
	if ( ( cap_setreuid(-1, TEST0) ) == -1 ) {
	    w_error(SYSCALL, namestring, err[SETREUID_S], errno);
	    cap_unlink(file);
	    cap_rmdir(dir);
	    exit(INCOMPLETE);
	}	
	if ( geteuid() != TEST0 ) {
	    w_error(GENERAL, namestring, err[BADUIDS_S], 0);
	    cap_unlink(file);
	    cap_rmdir(dir);
	    exit(INCOMPLETE);
	}	
    }

    /*
     * Set up buffer for test call.
     * Make it the same as file's current label.
     */
    lptr->ml_msen_type = parmsptr->file_msen;
    lptr->ml_mint_type = parmsptr->file_mint;
    /*
     * Here's the test: Call mac_set_file(file, testptr).
     */
    retval = mac_set_file(file, lptr);
    loc_errno = errno;
    

    /* 
     * Reset process EUID and label, then unlink dir.
     */
    cap_setreuid(TEST0, SUPER);
    lptr->ml_msen_type = MSH;
    lptr->ml_mint_type = MIE;
    if ( cap_setplabel(lptr) ) {
	w_error(SYSCALL, namestring, err[F_SETPLABEL], errno);
	cap_unlink(file);
	cap_rmdir(dir);
	exit(INCOMPLETE);
    }
    cap_unlink(file);
    cap_rmdir(dir);
    /*
     * For positive cases: retval should be 0.
     */
    if ( parmsptr->expect_success == PASS  ) {
	if ( retval == -1 ) {
	    w_error(SYSCALL, namestring, err[TESTCALL], loc_errno);
	    exit(FAIL);
	} 
	if ( retval != 0 ) {
	    w_error(PRINTNUM, namestring, err[TEST_RETVAL],retval);
	    exit(FAIL);
	}
	exit(PASS);
    }
    
    /*
     * For negative test cases, retval should be -1 and
     * errno should equal parmsptr->expect_errno.  If not, 
     * write error message. Exit 0 for PASS, 1 for FAIL.	 
     */
    if ( retval != -1 ) {
	w_error(PRINTNUM, namestring, err[TEST_RETVAL],retval);
	exit(FAIL);
    }
    if ( loc_errno != parmsptr->expect_errno ) {
	w_error(SYSCALL, namestring, err[TEST_ERRNO],loc_errno);
	exit(FAIL);
    }
    exit(PASS);
    return(PASS);
}
