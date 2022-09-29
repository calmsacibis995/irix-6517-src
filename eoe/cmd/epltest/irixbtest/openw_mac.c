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
 * openw_mac.c -- MAC write tests for open() system call. 
 * Opening a file with mode O_WRONLY, O_TRUNC, or O_RDWR, MAC write 
 * access to file and MAC read access to directory.  Process label 
 * must dominate directory label.  Process label must equal file 
 * label or process must have uid of superuser and MSEN_HIGH_LABEL 
 * and MINT part of process label must equal MINT part of file label.
 *
 * Opening a non-existant file with mode O_CREAT requires write 
 * permission to the directory. Process label must equal directory
 * label or process must have uid of superuser and MSEN_HIGH_LABEL 
 * and MINT part of process label must equal MINT part of directory
 * label.
 *
 * Structure prototypes and abbreviated names for MSEN and MINT types
 * are defined in a header file.  For example, MSH is MSEN_HIGH_LABEL.
 */
#include "mac.h"

static int openw_mac1(struct macinfo *parmsptr);

static struct macinfo
openw_minfo[] = 
{
/*
 case   case  c_flag  expect  errno   open    directory   file      process
 name  number         success         mode    msen/mint   msen/mint msen/mint
*/
"pos00",  0,      0,  PASS,   0,    O_WRONLY, MSL, MIH,   MSA, MIH, MSA, MIH,
"pos01",  1,      1,  PASS,   0,    O_WRONLY, MSL, MIH,   MSA, MIH, MSH, MIH,
"neg00",  2,      0,  FAIL, EACCES, O_WRONLY, MSL, MIE,   MSL, MIE, MSA, MIE,
"neg01",  3,      0,  FAIL, EACCES, O_WRONLY, MSH, MIE,   MSE, MIE, MSA, MIE,
"neg02",  4,      0,  FAIL, EACCES, O_WRONLY, MSH, MIE,   MSH, MIE, MSA, MIE,
"neg03",  5,      0,  FAIL, EACCES, O_WRONLY, MSA, MIH,   MSA, MIH, MSA, MIL,
"neg04",  6,      0,  FAIL, EACCES, O_WRONLY, MSA, MIL,   MSA, MIH, MSA, MIH,
"neg05",  7,      0,  FAIL, EACCES, O_WRONLY, MSA, MIL,   MSA, MIL, MSA, MIH,

"pos02",  8,      0,  PASS,   0,    O_TRUNC,  MSL, MIH,   MSA, MIH, MSA, MIH,
"pos03",  9,      1,  PASS,   0,    O_TRUNC,  MSL, MIH,   MSA, MIH, MSH, MIH,
"neg06", 10,      0,  FAIL, EACCES, O_TRUNC,  MSL, MIE,   MSL, MIE, MSA, MIE,
"neg07", 11,      0,  FAIL, EACCES, O_TRUNC,  MSH, MIE,   MSE, MIE, MSA, MIE,
"neg08", 12,      0,  FAIL, EACCES, O_TRUNC,  MSH, MIE,   MSH, MIE, MSA, MIE,
"neg09", 13,      0,  FAIL, EACCES, O_TRUNC,  MSA, MIH,   MSA, MIH, MSA, MIL,
"neg10", 14,      0,  FAIL, EACCES, O_TRUNC,  MSA, MIL,   MSA, MIH, MSA, MIH,
"neg11", 15,      0,  FAIL, EACCES, O_TRUNC,  MSA, MIL,   MSA, MIL, MSA, MIH,

"pos04", 16,      0,  PASS,   0,    O_RDWR,   MSL, MIH,   MSA, MIH, MSA, MIH,
"pos05", 17,      1,  PASS,   0,    O_RDWR,   MSL, MIH,   MSA, MIH, MSH, MIH,
"neg12", 18,      0,  FAIL, EACCES, O_RDWR,   MSL, MIE,   MSL, MIE, MSA, MIE,
"neg13", 19,      0,  FAIL, EACCES, O_RDWR,   MSH, MIE,   MSE, MIE, MSA, MIE,
"neg14", 20,      0,  FAIL, EACCES, O_RDWR,   MSH, MIE,   MSH, MIE, MSA, MIE,
"neg15", 21,      0,  FAIL, EACCES, O_RDWR,   MSA, MIH,   MSA, MIH, MSA, MIL,
"neg16", 22,      0,  FAIL, EACCES, O_RDWR,   MSA, MIL,   MSA, MIH, MSA, MIH,
"neg17", 23,      0,  FAIL, EACCES, O_RDWR,   MSA, MIL,   MSA, MIL, MSA, MIH,

"pos06", 24,      0,  PASS,   0,    O_CREAT,  MSL, MIH,     0,   0, MSL, MIH,
"pos07", 25,      1,  PASS,   0,    O_CREAT,  MSL, MIH,     0,   0, MSH, MIH,
"neg18", 26,      0,  FAIL, EACCES, O_CREAT,  MSL, MIH,     0,   0, MSA, MIH,
"neg19", 27,      0,  FAIL, EACCES, O_CREAT,  MSA, MIH,     0,   0, MSA, MIL,
};

static char *openw_mdesc[] = { 
    /* Abbreviations: P   Process
     *                D   Directory
     *                F   FILE
     *                >   dominates
     *                <   is dominated by
     *                =   equals
     *                !   not
     */
"MSEN:P > D, P = F; MINT:P < D, P = F; O_WRONLY",   
"MSEN:P > D, P != F; MINT:P < D, P = F; O_WRONLY",   
"MSEN:P !> D, P = F; MINT:P < D, P = F; O_WRONLY",   
"MSEN:P !> D, P != F; MINT:P < D, P = F; O_WRONLY",   
"MSEN:P > D, P = F; MINT:P < D, P != F; O_WRONLY",   
"MSEN:P > D, P = F; MINT:P !< D, P = F; O_WRONLY",   
"MSEN:P > D, P = F; MINT:P !< D, P != F; O_WRONLY",   
"MSEN:P > D, P != F, P MSEN_HIGH; MINT:P < D, P = F; O_WRONLY",   

"MSEN:P > D, P = F; MINT:P < D, P = F; O_TRUNC", 
"MSEN:P > D, P != F; MINT:P < D, P = F; O_TRUNC",   
"MSEN:P !> D, P = F; MINT:P < D, P = F; O_TRUNC",   
"MSEN:P !> D, P != F; MINT:P < D, P = F; O_TRUNC",   
"MSEN:P > D, P = F; MINT:P < D, P != F; O_TRUNC",   
"MSEN:P > D, P = F; MINT:P !< D, P = F; O_TRUNC",   
"MSEN:P > D, P = F; MINT:P !< D, P != F; O_TRUNC",   
"MSEN:P > D, P != F, P MSEN_HIGH; MINT:P < D, P = F; O_TRUNC",   
  
"MSEN:P > D, P = F; MINT:P < D, P = F; O_RDWR",   
"MSEN:P > D, P != F; MINT:P < D, P = F; O_RDWR",   
"MSEN:P !> D, P = F; MINT:P < D, P = F; O_RDWR",   
"MSEN:P !> D, P != F; MINT:P < D, P = F; O_RDWR",   
"MSEN:P > D, P = F; MINT:P < D, P != F; O_RDWR",   
"MSEN:P > D, P = F; MINT:P !< D, P = F; O_RDWR",   
"MSEN:P > D, P = F; MINT:P !< D, P != F; O_RDWR",   
"MSEN:P > D, P != F, P MSEN_HIGH; MINT:P < D, P = F; O_RDWR",   

"MSEN:P = D; MINT:P = D; O_CREAT, new file",   
"MSEN:P > D; MINT:P = D; O_CREAT, new file",   
"MSEN:P = D; MINT:P < D; O_CREAT, new file",   
"MSEN:P > D, P MSEN_HIGH; MINT:P = D; O_CREAT, new file",   
};

int
openw_mac(void)
{
    short ncases = 28;       /* number of total test cases */
    int fail = 0;            /* set when a test case fails */
    int incomplete = 0;      /* set when a test case is incomplete */
    short i = 0;             /* test case loop counter */
    char str[MSGLEN];        /* used to write to logfiles */
    char testname[SLEN];     /* used to write to logfiles */

    strcpy(testname,"openw_mac");
    /*
     * Write formatted info to raw log.
     */   
    RAWLOG_SETUP(testname, ncases);
    /*
     * Call function for each test case.
     */    
    for (i = 0; i < ncases; i++) {
        /*
	 * Flush output streams
	 */    
	flush_raw_log();   
	/*
	 * Call openw_mac1() for each test case, passing a pointer to
         * the structure containing the parameters for that case.
	 * A return value of 0 indicates PASS, 1 indicates FAIL, 2
	 * indicates the test was aborted due to an unexpected error.
	 */
	switch (openw_mac1(&openw_minfo[i]) ) {
	/*
	 * Write formatted result to raw log.
	 */    
	case PASS:      /* Passed */
	    RAWPASS(openw_minfo[i].casenum, openw_minfo[i].casename);
	    break;
	case FAIL:      /* Failed */
	    RAWFAIL(openw_minfo[i].casenum, openw_minfo[i].casename,
		    openw_mdesc[i]);
	    fail = 1;
	    break;
	default:
	    RAWINC(openw_minfo[i].casenum, openw_minfo[i].casename);
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
openw_mac1(struct macinfo *parmsptr)
{
    char testname[SLEN];         /* used to write to logfiles */
    char namestring[MSGLEN];     /* Used in error messages. */
    char file[NLEN + NLEN];      /* File name passed to open(). */
    char dir[NLEN];              /* Directory containing file. */
    int retval = 0;              /* Return value of test call. */
    int fildes;                  /* File descriptor.*/
    pid_t forkval;               /* Returned value of fork call. */
    int status;                  /* For wait. */   
    int loc_errno;               /* Saves test call errno. */
    mac_t lptr;                  /* MAC label pointer. */

    strcpy(testname,"openw_mac");
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
    if (  ( forkval = fork() ) == -1 ) {
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
     * Make a temp directory
     */
    cap_rmdir(dir);
    cap_unlink(file);
    if ( cap_mkdir(dir, 0700) ) {
	w_error(SYSCALL, namestring, err[F_MKDIR], errno);
	exit(INCOMPLETE);
    }
    
    /*
     * Malloc a mac pointer for getting & setting labels.
     */
    lptr = mac_get_proc();
    
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
     * Create a file, close it, and set its label, unless
     * this is an O_CREAT case, which opens a nonexistant file.
     */
    if (parmsptr->flag != O_CREAT) {
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
     * Here's the test: Call open(file, flag)
     */
    if (parmsptr->c_flag == 0) {
	retval = open(file, parmsptr->flag);
    }else{
	retval = cap_open(file, parmsptr->flag);
    }
    loc_errno = errno;
    close(retval);
    /* 
     * Reset process label and unlink dir.
     */
    lptr->ml_msen_type = MSH;
    lptr->ml_mint_type = MIE;
    if ( cap_setplabel(lptr) ) {
	w_error(SYSCALL, namestring, err[F_SETPLABEL], errno);
	cap_unlink(file);
	cap_rmdir(dir);
	exit(INCOMPLETE);
    }
    mac_free(lptr);
    cap_unlink(file);
    cap_rmdir(dir);
    
    /*
     * Positive cases:
     * Retval should be > 2, since descriptors 0-2 
     * are in use. If not, write error message.
     * Exit 0 for PASS, 1 for FAIL.
     */
    if (parmsptr->expect_success == PASS ) {
	if ( retval == -1 ) {
	    w_error(SYSCALL, namestring, err[TESTCALL], loc_errno);
	    exit(FAIL);
	} 
	if ( retval <= 2 ) {
	    w_error(PRINTNUM, namestring, err[TEST_RETVAL],
		    retval);
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
