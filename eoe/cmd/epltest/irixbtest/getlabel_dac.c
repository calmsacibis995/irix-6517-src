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

 * getlabel_dac.c -- DAC tests for mac_get_file() system call.  For 
 * mac_get_file(file) to succeed, the process must have search permission on
 * the parent directory and all directories above it.  If the process is
 * owner of the directory, only the owner bits are checked.  Otherwise,
 * if the process's group id equals the directory's group id, only the
 * group bits are checked.  Otherwise, only the other bits are checked.
 * 
 * Structure template is defined in a header file.
 */
#include "dac.h"
#include "cap.h"

static int getlabel_dac0(struct dacinfo *parmsptr);

/*
 * Each structures contains information for one test case.
 * Variables are uid and gid of Subject process, file owner,
 * group and mode, directory owner group and mode, expected
 * outcome, and expected errno for negative cases.
 */
static struct dacinfo 
getlabel_dinfo0[] = 
{
/* 
 case case c_flag expect  errno  --Subject-  -----FILE------   ------DIR-----
 name  num        success          uid  gid  owner group mode  owner group mode
*/
"pos00", 0,    0, PASS,       0, TEST0, 998, TEST0, 995, 0400, TEST0, 995, 0100,
"pos01", 1,    0, PASS,       0, TEST0, 998, TEST0, 995, 0377, TEST0, 995, 0100,
"pos02", 2,    0, PASS,       0, TEST0, 998, TEST1, 998, 0040, TEST0, 995, 0100,
"pos03", 3,    0, PASS,       0, TEST0, 998, TEST1, 998, 0737, TEST0, 995, 0100,
"pos04", 4,    0, PASS,       0, TEST0, 998, TEST2, 995, 0004, TEST0, 995, 0100,
"pos05", 5,    0, PASS,       0, TEST0, 998, TEST2, 995, 0773, TEST0, 995, 0100,

"neg00", 6,    0, FAIL,  EACCES, TEST0, 998, TEST0, 995, 0400, TEST0, 995, 0677,
"neg01", 7,    0, FAIL,  EACCES, TEST0, 998, TEST0, 995, 0377, TEST0, 995, 0677,
"neg02", 8,    0, FAIL,  EACCES, TEST0, 998, TEST1, 998, 0040, TEST0, 995, 0677,
"neg03", 9,    0, FAIL,  EACCES, TEST0, 998, TEST1, 998, 0737, TEST0, 995, 0677,
"neg04",10,    0, FAIL,  EACCES, TEST0, 998, TEST2, 995, 0004, TEST0, 995, 0677,
"neg05",11,    0, FAIL,  EACCES, TEST0, 998, TEST2, 995, 0773, TEST0, 995, 0677,

"pos06",12,    0, PASS,       0, TEST0, 998, TEST0, 995, 0400, TEST1, 998, 0010,
"pos07",13,    0, PASS,       0, TEST0, 998, TEST0, 995, 0377, TEST1, 998, 0010,
"pos08",14,    0, PASS,       0, TEST0, 998, TEST1, 998, 0040, TEST1, 998, 0010,
"pos09",15,    0, PASS,       0, TEST0, 998, TEST1, 998, 0737, TEST1, 998, 0010,
"pos10",16,    0, PASS,       0, TEST0, 998, TEST2, 995, 0004, TEST1, 998, 0010,
"pos11",17,    0, PASS,       0, TEST0, 998, TEST2, 995, 0773, TEST1, 998, 0010,

"neg06",18,    0, FAIL,  EACCES, TEST0, 998, TEST0, 995, 0400, TEST1, 998, 0767,
"neg07",19,    0, FAIL,  EACCES, TEST0, 998, TEST0, 995, 0377, TEST1, 998, 0767,
"neg08",20,    0, FAIL,  EACCES, TEST0, 998, TEST1, 998, 0040, TEST1, 998, 0767,
"neg09",21,    0, FAIL,  EACCES, TEST0, 998, TEST1, 998, 0737, TEST1, 998, 0767,
"neg10",22,    0, FAIL,  EACCES, TEST0, 998, TEST2, 995, 0004, TEST1, 998, 0767,
"neg11",23,    0, FAIL,  EACCES, TEST0, 998, TEST2, 995, 0773, TEST1, 998, 0767,

"pos12",24,    0, PASS,       0, TEST0, 998, TEST0, 995, 0400, TEST2, 995, 0001,
"pos13",25,    0, PASS,       0, TEST0, 998, TEST0, 995, 0377, TEST2, 995, 0001,
"pos14",26,    0, PASS,       0, TEST0, 998, TEST1, 998, 0040, TEST2, 995, 0001,
"pos15",27,    0, PASS,       0, TEST0, 998, TEST1, 998, 0737, TEST2, 995, 0001,
"pos16",28,    0, PASS,       0, TEST0, 998, TEST2, 995, 0004, TEST2, 995, 0001,
"pos17",29,    0, PASS,       0, TEST0, 998, TEST2, 995, 0773, TEST2, 995, 0001,

"neg12",30,    0, FAIL,  EACCES, TEST0, 998, TEST0, 995, 0400, TEST2, 995, 0776,
"neg13",31,    0, FAIL,  EACCES, TEST0, 998, TEST0, 995, 0377, TEST2, 995, 0776,
"neg14",32,    0, FAIL,  EACCES, TEST0, 998, TEST1, 998, 0040, TEST2, 995, 0776,
"neg15",33,    0, FAIL,  EACCES, TEST0, 998, TEST1, 998, 0737, TEST2, 995, 0776,
"neg16",34,    0, FAIL,  EACCES, TEST0, 998, TEST2, 995, 0004, TEST2, 995, 0776,
"neg17",35,    0, FAIL,  EACCES, TEST0, 998, TEST2, 995, 0773, TEST2, 995, 0776,

"pos18",36,    1, PASS,       0, SUPER, 998, TEST2, 995, 0000, TEST2, 995, 0000,
};

/* 
 * Description strings, corresponding with test cases, 
 * to be printed to the raw log if that case fails.
 */
static char *getlabel_ddesc0[] = {
    /* Abbreviations: S   Subject (Process)
     *                D   Directory
     *                F   FILE
     *                +   has
     *                -   does not have
     *                r   read permission
     *                x   search permission
     */
"S is F owner, D owner, F mode owner +r, D mode owner +x",
"S is F owner, D owner, F mode owner -r, D mode owner +x",
"S is F group, D owner, F mode group +r, D mode owner +x",
"S is F group, D owner, F mode group -r, D mode owner +x",
"S is F other, D owner, F mode other +r, D mode owner +x",
"S is F other, D owner, F mode other -r, D mode owner +x",

"S is F owner, D owner, F mode owner +r, D mode owner -x",
"S is F owner, D owner, F mode owner -r, D mode owner -x",
"S is F group, D owner, F mode group +r, D mode owner -x",
"S is F group, D owner, F mode group -r, D mode owner -x",
"S is F other, D owner, F mode other +r, D mode owner -x",
"S is F other, D owner, F mode other -r, D mode owner -x",

"S is F owner, D group, F mode owner +r, D mode group +x",
"S is F owner, D group, F mode owner -r, D mode group +x",
"S is F group, D group, F mode group +r, D mode group +x",
"S is F group, D group, F mode group -r, D mode group +x",
"S is F other, D group, F mode other +r, D mode group +x",
"S is F other, D group, F mode other -r, D mode group +x",

"S is F owner, D group, F mode owner +r, D mode group -x",
"S is F owner, D group, F mode owner -r, D mode group -x",
"S is F group, D group, F mode group +r, D mode group -x",
"S is F group, D group, F mode group -r, D mode group -x",
"S is F other, D group, F mode other +r, D mode group -x",
"S is F other, D group, F mode other -r, D mode group -x",

"S is F owner, D other, F mode owner +r, D mode other +x",
"S is F owner, D other, F mode owner -r, D mode other +x",
"S is F group, D other, F mode group +r, D mode other +x",
"S is F group, D other, F mode group -r, D mode other +x",
"S is F other, D other, F mode other +r, D mode other +x",
"S is F other, D other, F mode other -r, D mode other +x",

"S is F owner, D other, F mode owner +r, D mode other -x",
"S is F owner, D other, F mode owner -r, D mode other -x",
"S is F group, D other, F mode group +r, D mode other -x",
"S is F group, D other, F mode group -r, D mode other -x",
"S is F other, D other, F mode other +r, D mode other -x",
"S is F other, D other, F mode other -r, D mode other -x",

"S suser, F mode 000, D mode 000",
};


int
getlabel_dac(void)
{
    short ncases = 37;       /* number of dac test cases */
    int fail = 0;            /* set when a test case fails */
    int incomplete = 0;      /* set when a test case is incomplete */
    short i = 0;             /* test case loop counter */
    char str[MSGLEN];        /* used to write to logfiles */
    char testname[SLEN];     /* used to write to logfiles */

    strcpy(testname,"getlabel_dac");
    /*
     * Write formatted info to raw log.
     */   
    RAWLOG_SETUP(testname, (ncases));
    /*
     * Call function for each test case.
     */    
    for (i = 0; i < ncases; i++) {
	/*
	 * Flush output streams
	 */    
	flush_raw_log();   
	/*
	 * Call getlabel_dac0() for each test case, passing a pointer to
         * the structure containing the parameters for that case.
	 * A return value of 0 indicates PASS, 1 indicates FAIL, 2
	 * indicates the test was aborted due to an unexpected error.
	 */
	switch (getlabel_dac0(&getlabel_dinfo0[i]) ) {
	/*
	 * Write formatted result to raw log.
	 */    
	case PASS:      /* Passed */
	    RAWPASS(getlabel_dinfo0[i].casenum, getlabel_dinfo0[i].casename);
	    break;
	case FAIL:      /* Failed */
	    RAWFAIL(getlabel_dinfo0[i].casenum, getlabel_dinfo0[i].casename,
		    getlabel_ddesc0[i]);
	    fail = 1;
	    break;
	default:
	    RAWINC(getlabel_dinfo0[i].casenum, getlabel_dinfo0[i].casename);
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
static int getlabel_dac0(struct dacinfo *parmsptr)
{
    char testname[SLEN];         /* used to write to logfiles */
    char namestring[MSGLEN];     /* Used in error messages. */
    char file[NLEN + NLEN];      /* File name passed to mac_get_file(). */
    char dir[NLEN];              /* Directory containing file. */
    int retval = 0;              /* Return value of test call. */
    int fildes;                  /* File descriptor.*/
    mac_t testptr;               /* MAC label pointer for test call. */
    pid_t forkval;               /* Returned value of fork call. */
    int status;                  /* For wait. */   
    int loc_errno;               /* Saves test call errno. */

    strcpy(testname,"getlabel_dac");
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
     * Make a temp directory
     */
    cap_unlink(file);
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
	cap_rmdir(dir);
	exit(INCOMPLETE);
    }
    /* 
     * Chmod directory to parmsptr->D_mode
     */
    
    if ( cap_chmod(dir, parmsptr->D_mode) ) {
	w_error(SYSCALL, namestring, err[F_CHMOD], errno);
	cap_rmdir(dir);
	exit(INCOMPLETE);
    }
    /*
     * Create a file with mode F_mode and close it
     */
    if ( (fildes = cap_open(file, O_CREAT, 0700)) < 0 ) {
	w_error(SYSCALL, namestring, err[F_CREAT], errno);
	cap_rmdir(dir);
	exit(INCOMPLETE);
    }
    close(fildes);
    
    /*
     * Chown file to F_owner, F_group
     */
    if ( cap_chown(file, parmsptr->F_owner, parmsptr->F_group) ) {
	w_error(SYSCALL, namestring, err[F_CHOWN], errno);
	cap_unlink(file);
	cap_rmdir(dir);
	exit(INCOMPLETE);
    }
    
    /* 
     * Set process egid to test value and verify.
     */
    if ( ( cap_setregid(-1, parmsptr->gid)) == -1  ) {
	w_error(SYSCALL, namestring, err[SETREGID_S], errno);
	exit(INCOMPLETE);
    }	
    if (getegid() != parmsptr->gid) {
	w_error(GENERAL, namestring, err[BADGIDS_S], 0);
	exit(INCOMPLETE);
    }	
    /* 
     * Set process euid to test value and verify.
     */
    if ( ( cap_setreuid(-1, parmsptr->uid)) == -1  ) {
	w_error(SYSCALL, namestring, err[SETREUID_S], errno);
	exit(INCOMPLETE);
    }	
    if ( geteuid() != parmsptr->uid ) {
	w_error(GENERAL, namestring, err[BADUIDS_S], 0);
	exit(INCOMPLETE);
    }	
    
    /*
     * Here's the test: Call mac_get_file(file)
     */
    if (parmsptr->c_flag != 0) {
	testptr = cap_mac_get_file(file);
    }else{
	testptr = mac_get_file(file);
    }
    retval = (testptr == NULL ? -1 : 0);
    loc_errno = errno;
    
    /* 
     * Reset process euid to Suser and unlink dir.
     */
    cap_setreuid(parmsptr->uid, SUPER);
    cap_unlink(file);
    cap_rmdir(dir);
    
    /*
     * For positive cases: retval should be 0,
     * msen and mint should be MSH,MIE.
     */
    if (parmsptr->expect_success == PASS ) {
	if ( retval == -1 ) {
	    w_error(SYSCALL, namestring, err[TESTCALL], loc_errno);
	    exit(FAIL);
	} 
	if ( retval != 0 ) {
	    w_error(PRINTNUM, namestring, err[TEST_RETVAL],retval);
	    exit(FAIL);
	}
	if ( (testptr->ml_msen_type != MSEN_HIGH_LABEL) ||
	    (testptr->ml_mint_type != MINT_EQUAL_LABEL) ) {
	    w_error(GENERAL, namestring, err[BADLABEL], 0);
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
