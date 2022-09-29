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

/*
 * setlabel_sup.c -- Tests for mac_set_file() system call.
 * For mac_set_file(file) to succeed, the process must be the owner of the
 * file and have search permission on the parent directory and all
 * directories above it.  The permission bits on the file are
 * irrelevant.
 * If the process is owner of a directory, only the owner bits are
 * checked.  Otherwise, if the process's group id equals the
 * directory's group id, only the group bits are checked.  Otherwise,
 * only the other bits are checked.
 *
 * If the caller is not the superuser, the new label must dominate
 * the old.
 * 
 * Structure template is defined in a header file.
 */
#include "dac.h"
#include "mac.h"

static int setlabel_sup0(struct dacinfo *parmsptr);

/*
 * Each structures contains information for one test case.
 * Variables are uid and gid of Subject, owner, group, and
 * mode of the file an directory, expected outcome and, for
 * negative cases, expected errno.
 */
static struct dacinfo 
setlabel_dinfo0[] = 
{
/* 
 case case c_flag expect  errno  --Subject-  -----FILE------   ------DIR-----
 name  num        success          uid  gid  owner group mode  owner group mode
*/
"neg00", 0,    0, FAIL, EPERM, TEST0, 998, TEST0, 995, 0200, TEST0, 995, 0100,
"neg01", 1,    0, FAIL, EPERM, TEST0, 998, TEST0, 995, 0577, TEST0, 995, 0100,
"pos01", 2,    1, PASS,     0, SUPER, 998, TEST2, 995, 0000, TEST2, 995, 0000,
};

/* 
 * Description strings, corresponding with test cases, 
 * to be printed to the raw log if that case fails.
 */
static char *setlabel_ddesc[] = { 
    /* Abbreviations: S   Subject (Process)
     *                D   Directory
     *                F   FILE
     *                +   has
     *                -   does not have
     *                w   write permission
     *                x   search permission
     */
"S is F owner, D owner, F mode owner +w, D mode owner +x",
"S is F owner, D owner, F mode owner -w, D mode owner +x",
"S suser, F mode 000, D mode 000",
};

int
setlabel_sup(void)
{
    char str[MSGLEN];        /* used to write to logfiles */
    char testname[SLEN];     /* used to write to logfiles */
    char *name;              /* holds casename from struct */
    short num;               /* holds casenum from struct */
    short ncases0 =  3;      /* number of test cases for file and dir */
    short ncases1 =  0;      /* number of test cases for label changes */

    short i = 0;             /* test case loop counter */
    int fail = 0;            /* set when a test case fails */
    int incomplete = 0;      /* set when a test case is incomplete */
    int retval;              /* return value of test func */

    strcpy(testname,"setlabel_sup");
    /*
     * Write formatted info to raw log.
     */   
    RAWLOG_SETUP(testname, (ncases0 + ncases1 ) );
    /*
     * Call function for each test case.
     */    
    for ( i = 0; i < ( ncases0  + ncases1 ); i++ ) {
	/*
	 * Flush output streams before calling a function that forks! 
	 */    
	flush_raw_log();   
	/*
	 * Call setlabel_sup0() for each test case, passing a pointer to
         * the structure containing the parameters for that case.
	 * A return value of 0 indicates PASS, 1 indicates FAIL, 2
	 * indicates the test was aborted due to an unexpected error.
	 */
	if ( i < ncases0 ) {
	    retval = setlabel_sup0(&setlabel_dinfo0[i]);
	    num = setlabel_dinfo0[i].casenum;
	    name = setlabel_dinfo0[i].casename;
	    }
	switch ( retval ) {
	/*
	 * Write formatted result to raw log.
	 */    
	case PASS:      /* Passed */
	    RAWPASS( num, name );
	    break;
	case FAIL:      /* Failed */
	    RAWFAIL( num, name, setlabel_ddesc[i] );
	    fail = 1;
	    break;
	default:
	    RAWINC( num, name );
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

/*
 * This function sets up the conditions for a given case based on the
 * parameters passed in the structure.  Then it does the test and
 * returns 0, 1, or 2 for PASS, FAIL, or INCOMPLETE, respectively.
 */
static int
setlabel_sup0(struct dacinfo *parmsptr)
{
    char testname[SLEN];         /* used to write to logfiles */
    char namestring[MSGLEN];     /* Used in error messages. */
    char file[NLEN + NLEN];      /* File name passed to mac_set_file(). */
    char dir[NLEN];              /* Directory containing file. */
    int retval = 0;              /* Return value of test call. */
    int fildes;                  /* File descriptor.*/
    mac_t testptr;               /* MAC label pointer for test call. */
    mac_t lptr;                  /* MAC label pointer for mac_set_proc call. */
    pid_t forkval;               /* Returned value of fork call. */
    int status;                  /* For wait. */   
    int loc_errno;               /* Saves test call errno. */

    strcpy(testname,"setlabel_sup");
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
     * Setplabel to msenhigh/mintlow.
     */
    lptr = mac_get_proc();
    lptr->ml_msen_type = MSH;
    lptr->ml_mint_type = MIL;
    if ( cap_setplabel(lptr) < 0 ) {
	w_error(SYSCALL, namestring, err[F_SETPLABEL], errno);
	exit(INCOMPLETE);
    }
    mac_free(lptr);
    
    /* 
     * Make a temp directory
     */
    cap_rmdir(dir);
    if ( cap_mkdir(dir, 0700) ) {
	w_error(SYSCALL, namestring, err[F_MKDIR], errno);
	exit(INCOMPLETE);
    }
    
    /*
     * Chown directory to D_owner, D_group
     */
    if ( cap_chown(dir, parmsptr->D_owner, parmsptr->D_group) ) {
	w_error(SYSCALL, namestring, err[F_CHOWN], errno);
	exit(INCOMPLETE);
    }
    /* 
     * Chmod directory to parmsptr->D_mode
     */
    
    if ( cap_chmod(dir, parmsptr->D_mode) ) {
	w_error(SYSCALL, namestring, err[F_CHMOD], errno);
	exit(INCOMPLETE);
    }
    /*
     * Create a file with mode F_mode and close it
     */
    if ( ( fildes = creat(file, parmsptr->F_mode) ) == -1 ) {
	w_error(SYSCALL, namestring, err[F_CREAT], errno);
	exit(INCOMPLETE);
    }
    close(fildes);
    
    /*
     * Chown file to F_owner, F_group
     */
    if ( cap_chown(file, parmsptr->F_owner, parmsptr->F_group) ) {
	w_error(SYSCALL, namestring, err[F_CHOWN], errno);
	exit(INCOMPLETE);
    }
    
    /* 
     * Set process egid to test value and verify.
     */
    if ( ( cap_setregid(-1, parmsptr->gid) ) == -1 ) {
	w_error(SYSCALL, namestring, err[SETREGID_S], errno);
	exit(INCOMPLETE);
    }	
    if ( getegid() != parmsptr->gid ) {
	w_error(GENERAL, namestring, err[BADGIDS_S], 0);
	exit(INCOMPLETE);
    }	
    /* 
     * Set process euid to test value and verify.
     */
    if ( ( cap_setreuid(-1, parmsptr->uid) ) == -1 ) {
	w_error(SYSCALL, namestring, err[SETREUID_S], errno);
	exit(INCOMPLETE);
    }	
    if ( geteuid() != parmsptr->uid ) {
	w_error(GENERAL, namestring, err[BADUIDS_S], 0);
	exit(INCOMPLETE);
    }	
    
    /*
     * Malloc a mac_label pointer for test call and
     * put some values in the buffer.  File and dir
     * should have MSEN_HIGH, MINT_LOW label when 
     * created, so use that label to test DAC.
     */
    testptr = mac_get_proc();
    testptr->ml_msen_type = MSH;
    testptr->ml_mint_type = MIL;
    /*
     * Here's the test: Call mac_set_file(file, testptr).
     */
    retval = mac_set_file(file, testptr);
    mac_free(testptr);
    loc_errno = errno;
    
    /* 
     * Reset process euid to Suser and unlink dir.
     */
    cap_setreuid(parmsptr->uid, SUPER);
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
	    w_error(PRINTNUM, namestring, err[TEST_RETVAL], retval);
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
