/*
 * Copyright 1996, Silicon Graphics, Inc. 
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
 * symlink_mac.c -- MAC read tests for symlinks.  Opening a file
 * with a symlink component in its path requires
 * MAC read access to the symlink, the file and the directory.  
 *
 * Structure prototypes and abbreviated names for MSEN and MINT types
 * are defined in a header file.  For example, MSH is MSEN_HIGH_LABEL.
 */
#include "mac.h"

static int symlink_mac1(struct macinfo *parmsptr);

static struct macinfo
symlink_minfo1[] = 
{
/*
 case   case  c_flag  expect   errno   not   directory   symlink    process
 name  number         success         used   [not used]  msen/mint  msen/mint
*/
 /* These tests actually use the symlink to open a file */
"pos00",  0,      0,  PASS,     0,      0,     0,   0,   MSL, MIH,   MSA, MIL,
"neg00",  1,      0,  FAIL,   EACCES,   0,     0,   0,   MSH, MIE,   MSA, MIE,
"neg01",  2,      0,  FAIL,   EACCES,   0,     0,   0,   MSH, MIE,   MSA, MIE,
"neg02",  3,      0,  FAIL,   EACCES,   0,     0,   0,   MSA, MIL,   MSA, MIH,
"neg03",  4,      0,  FAIL,   EACCES,   0,     0,   0,   MSA, MIL,   MSA, MIH,
};
static struct macinfo
symlink_minfo2[] = 
{
/*
 case   case  c_flag  expect   errno   not   directory   symlink    process
 name  number         success         used   [not used]  msen/mint  msen/mint
*/
 /* These tests use readlink() to access the symlink */
"pos01",  5,      0,  PASS,     0,      0,     0,   0,   MSL, MIH,   MSA, MIL,
"neg04",  6,      0,  FAIL,   EACCES,   0,     0,   0,   MSH, MIE,   MSA, MIE,
"neg05",  7,      0,  FAIL,   EACCES,   0,     0,   0,   MSH, MIE,   MSA, MIE,
"neg06",  8,      0,  FAIL,   EACCES,   0,     0,   0,   MSA, MIL,   MSA, MIH,
"neg07",  9,      0,  FAIL,   EACCES,   0,     0,   0,   MSA, MIL,   MSA, MIH,
};

/* Abbreviations:
*    P   Process
*    S   Symlink
*    >   dominates
*    <   is dominated by
*    !   not
*/
static char *symlink_mdesc1[] = { 
"Open;  MSEN: P > S;  MINT: P < S",   
"Open;  MSEN: P !> S;  MINT: P < S",   
"Open;  MSEN: P !> S;  MINT: P < S",   
"Open;  MSEN: P > S;  MINT: P !< S",   
"Open;  MSEN: P > S;  MINT: P !< S",   
};
static char *symlink_mdesc2[] = { 
"readlink;  MSEN: P > S;  MINT: P < S",   
"readlink;  MSEN: P !> S;  MINT: P < S",   
"readlink;  MSEN: P !> S;  MINT: P < S",   
"readlink;  MSEN: P > S;  MINT: P !< S",   
"readlink;  MSEN: P > S;  MINT: P !< S",   
};

int
symlink_mac(void)
{
    short ncases1 =	     /* number of access test cases */
    	  sizeof (symlink_minfo1) / sizeof (symlink_minfo1[0]);
    short ncases2 =	     /* number of readlink test cases */
    	  sizeof (symlink_minfo2) / sizeof (symlink_minfo2[0]);
    short ncases;	     /* number of total test cases */
    int fail = 0;            /* set when a test case fails */
    int incomplete = 0;      /* set when a test case is incomplete */
    short i = 0;             /* test case loop counter */
    char str[MSGLEN];        /* used to write to logfiles */
    char testname[SLEN];     /* used to write to logfiles */

    ncases = ncases1 + ncases2;

    strcpy(testname,"symlink_mac");
    /*
     * Write formatted info to raw log.
     */   
    RAWLOG_SETUP(testname, ncases);

    /*
     * Call function for each access test case.
     */
    for ( i = 0; i < ncases1; i++ ) {
	/*
	 * Flush output streams
	 */    
	flush_raw_log();   
	/*
	 * Call symlink_mac1() for each test case, passing a pointer to
         * the structure containing the parameters for that case.
	 * A return value of 0 indicates PASS, 1 indicates FAIL, 2
	 * indicates the test was aborted due to an unexpected error.
	 */
	switch( symlink_mac1(&symlink_minfo1[i]) ) {
	/*
	 * Write formatted result to raw log.
	 */    
	case PASS:      /* Passed */
	    RAWPASS(symlink_minfo1[i].casenum, symlink_minfo1[i].casename);
	    break;
	case FAIL:      /* Failed */
	    RAWFAIL(symlink_minfo1[i].casenum, symlink_minfo1[i].casename,
		    symlink_mdesc1[i]);
	    fail = 1;
	    break;
	default:
	    RAWINC(symlink_minfo1[i].casenum, symlink_minfo1[i].casename);
	    incomplete = 1;
	    break;  /* Incomplete */
	}
	flush_raw_log();
    }

    /*
     * Call function for each readlink test case.
     */
    for ( i = 0; i < ncases2; i++ ) {
	/*
	 * Flush output streams
	 */    
	flush_raw_log();   
	/*
	 * Call symlink_mac2() for each test case, passing a pointer to
         * the structure containing the parameters for that case.
	 * A return value of 0 indicates PASS, 1 indicates FAIL, 2
	 * indicates the test was aborted due to an unexpected error.
	 */
	switch( symlink_mac2(&symlink_minfo2[i]) ) {
	/*
	 * Write formatted result to raw log.
	 */    
	case PASS:      /* Passed */
	    RAWPASS(symlink_minfo2[i].casenum, symlink_minfo2[i].casename);
	    break;
	case FAIL:      /* Failed */
	    RAWFAIL(symlink_minfo2[i].casenum, symlink_minfo2[i].casename,
		    symlink_mdesc2[i]);
	    fail = 1;
	    break;
	default:
	    RAWINC(symlink_minfo2[i].casenum, symlink_minfo2[i].casename);
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
symlink_mac1(struct macinfo *parmsptr)
{
    extern errno;
    char testname[SLEN];         /* used to write to logfiles */
    char namestring[MSGLEN];     /* Used in error messages. */
    char file[NLEN + NLEN];      /* File name we will link to. */
    char slink[NLEN + NLEN];     /* name passed to symlink(). */
    int retval = 0;              /* Return value of test call. */
    int fildes;                  /* File descriptor.*/
    pid_t forkval;               /* Returned value of fork call. */
    int status;                  /* For wait. */   
    int loc_errno;               /* Saves test call errno. */
    mac_label *lptr;             /* MAC label pointer. */

    strcpy(testname,"symlink_mac");
    sprintf(namestring, "%s, case %d, %s:\n   ", testname,
	     parmsptr->casenum, parmsptr->casename);
    /*
     * Create names for file and symlink.
     */
    sprintf(file, "tmp%d", parmsptr->casenum);
    sprintf(slink, "stmp%d", parmsptr->casenum);

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
     * Malloc a mac pointer for getting & setting labels.
     */
    lptr = mac_get_proc();
    
    /*
     * Create a file, close it, and set its label.
     */
    if ( ( fildes = creat(file, 0700)) == -1 ) {
	w_error(SYSCALL, namestring, err[F_CREAT], errno);
	exit(INCOMPLETE);
    }
    close(fildes);
    /* Note we are setting the file's label to WILDCARD
     * (this ensures we have access, we want to test the SYMLINK's MAC)
     */
    lptr->ml_msen_type = MSE;
    lptr->ml_mint_type = MIE;
    if ( cap_setlabel(file, lptr) ) {
	w_error(SYSCALL, namestring, err[SETLABEL_FILE], errno);
	cap_unlink(file);
	exit(INCOMPLETE);
    }
    
    /* 
     * Set process label (for symlink creation)
     */
    lptr->ml_msen_type = parmsptr->file_msen;
    lptr->ml_mint_type = parmsptr->file_mint;
    if ( cap_setplabel(lptr) ) {
	w_error(SYSCALL, namestring, err[F_SETPLABEL], errno);
	cap_unlink(file);
	exit(INCOMPLETE);
    }
    /*
     * Now create the symlink (should get created with process' label)
     */
    if ( symlink(file, slink) != 0 ) {
	w_error(SYSCALL, namestring, err[SETLABEL_DIR], errno);
	cap_unlink(file);
	exit(INCOMPLETE);
    }
    
    /* 
     * Set process label to value for test
     */
    lptr->ml_msen_type = parmsptr->proc_msen;
    lptr->ml_mint_type = parmsptr->proc_mint;
    if ( cap_setplabel(lptr) ) {
	w_error(SYSCALL, namestring, err[F_SETPLABEL], errno);
	cap_unlink(file);
	cap_unlink(slink);
	exit(INCOMPLETE);
    }

#if 0
{
  char	sbuf[160];

  sprintf(sbuf, "set -x ; id -MP ; ls -ldM %s %s", file, slink);
  system(sbuf);
}
#endif /* 0 */
    /*
     * Here's the test: Call open(slink, flag).
     */
    retval = open(slink, parmsptr->flag);
    loc_errno = errno;
    if ( retval >= 0 )
	    close(retval);
    
    /* 
     * Clean up.
     */
    cap_unlink(file);
    cap_unlink(slink);
    
    /*
     * For positive cases:
     * retval should be > 2, since descriptors 0-2 
     * are in use.
     */
    if ( parmsptr->expect_success == PASS  ) {
	if ( retval == -1 ) {
	    w_error(SYSCALL, namestring, err[TESTCALL], loc_errno);
	    exit(FAIL);
	} 
	if ( retval <= 2 ) {
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
	w_error(PRINTNUM, namestring, err[TEST_RETVAL],retval);
	exit(FAIL);
    }
    if ( loc_errno != parmsptr->expect_errno ) {
	w_error(SYSCALL, namestring, err[TEST_ERRNO], loc_errno);
	exit(FAIL);
    }
    exit(PASS);
    return(PASS);
}


/*
 * This function sets up the conditions for a given case based on the
 * parameters passed in the structure.  Then it does the test and
 * returns 0, 1, or 2 for PASS, FAIL, or INCOMPLETE, respectively.
 */
static int
symlink_mac2(struct macinfo *parmsptr)
{
    extern errno;
    char testname[SLEN];         /* used to write to logfiles */
    char namestring[MSGLEN];     /* Used in error messages. */
    char slink[NLEN + NLEN];     /* name passed to symlink(). */
    char file[NLEN + NLEN];      /* name passed to symlink(). */
    char testbuf[NLEN + NLEN];   /* buffer used for readlink test. */
    int retval = 0;              /* Return value of test call. */
    int fildes;                  /* File descriptor.*/
    pid_t forkval;               /* Returned value of fork call. */
    int status;                  /* For wait. */   
    int loc_errno;               /* Saves test call errno. */
    mac_t lptr;                  /* MAC label pointer. */

    strcpy(testname,"symlink_mac");
    sprintf(namestring, "%s, case %d, %s:\n   ", testname,
	     parmsptr->casenum, parmsptr->casename);
    /*
     * Create names for symlink.
     */
    sprintf(file, "tmp%d", parmsptr->casenum);	/* Never actually exists */
    sprintf(slink, "stmp%d", parmsptr->casenum);

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
     * Malloc a mac pointer for getting & setting labels.
     */
    lptr = mac_get_proc();
    
    /* 
     * Set process label (for symlink creation)
     */
    lptr->ml_msen_type = parmsptr->file_msen;
    lptr->ml_mint_type = parmsptr->file_mint;
    if ( cap_setplabel(lptr) ) {
	w_error(SYSCALL, namestring, err[F_SETPLABEL], errno);
	exit(INCOMPLETE);
    }
    /*
     * Now create the symlink (should get created with process' label)
     */
    if ( symlink(file, slink) != 0 ) {
	w_error(SYSCALL, namestring, err[SETLABEL_DIR], errno);
	exit(INCOMPLETE);
    }
    
    /* 
     * Set process label to value for test
     */
    lptr->ml_msen_type = parmsptr->proc_msen;
    lptr->ml_mint_type = parmsptr->proc_mint;
    if ( cap_setplabel(lptr) ) {
	w_error(SYSCALL, namestring, err[F_SETPLABEL], errno);
	cap_unlink(slink);
	exit(INCOMPLETE);
    }

#if 0
{
  char	sbuf[160];

  sprintf(sbuf, "set -x ; id -MP ; ls -ldM %s", slink);
  system(sbuf);
}
#endif /* 0 */
    /*
     * Here's the test: Call readlink(slink).
     */
    retval = readlink(slink, testbuf, sizeof (testbuf));
    loc_errno = errno;
    if ( retval >= 0 )
	    close(retval);
    
    /* 
     * Clean up.
     */
    cap_unlink(slink);
    
    /*
     * For positive cases:
     * retval should be == strlen(file)
     */
    if ( parmsptr->expect_success == PASS  ) {
	if ( retval == -1 ) {
	    w_error(SYSCALL, namestring, err[TESTCALL], loc_errno);
	    exit(FAIL);
	} 
	if ( retval != strlen(file) ) {
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
