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

#include "mls.h"

static int mac_dom_mls0(struct mlsinfo *parmsptr, int num, char *name);
static int mac_dom_mls1(struct mlsinfo *parmsptr, int num, char *name);

/*
 * mac_dom_mls.c -- Tests for mac_dominate() function.
 */

/* 
 * msendom[] and mintdom[] are arrays of mlsinfo structs, declared 
 * in mls.h.  The struct consists of 3 fields, i0, i1, and
 * expect_retval.  i0 and i1 are indices into the array msenlist[],
 * for msendom, and mintlist[], for mintdom.  The list item indexed by
 * i0 is used to construct the label pointed to by the first argument
 * to mac_dominate() and the item indexed by i1 is used to construct the
 * label pointed to by the second argument.  The expect_retval is 1
 * for positive cases and 0 for negative.
 */

static struct mlsinfo 
msendom[] = 
{
 0,0,1,    0,1,1,    0,2,0,    0,3,0,    0,4,1,    0,5,1,  /* 0-35 */
 0,6,0,    0,7,0,    0,8,0,    0,9,0,    0,10,0,   0,11,0,
 0,12,0,   0,13,0,   0,14,0,   0,15,0,   0,16,0,   0,17,0,
 1,0,1,    1,1,1,    1,2,1,    1,3,1,    1,4,1,    1,5,1,  
 1,6,1,    1,7,1,    1,8,1,    1,9,1,    1,10,1,   1,11,1, 
 1,12,1,   1,13,1,   1,14,1,   1,15,1,   1,16,1,   1,17,1, 

 2,0,1,    2,1,1,    2,2,1,    2,3,1,    2,4,1,    2,5,1,  /* 36-71 */
 2,6,1,    2,7,1,    2,8,1,    2,9,1,    2,10,1,   2,11,1, 
 2,12,1,   2,13,1,   2,14,1,   2,15,1,   2,16,1,   2,17,1, 
 3,0,1,    3,1,1,    3,2,1,    3,3,1,    3,4,1,    3,5,1,  
 3,6,1,    3,7,1,    3,8,1,    3,9,1,    3,10,1,   3,11,1, 
 3,12,1,   3,13,1,   3,14,1,   3,15,1,   3,16,1,   3,17,1, 

 4,0,0,    4,1,1,    4,2,0,    4,3,0,    4,4,1,    4,5,1,  /* 72-107 */
 4,6,0,    4,7,0,    4,8,0,    4,9,0,    4,10,0,   4,11,0, 
 4,12,0,   4,13,0,   4,14,0,   4,15,0,   4,16,0,   4,17,0, 
 5,0,0,    5,1,1,    5,2,0,    5,3,0,    5,4,1,    5,5,1,  
 5,6,0,    5,7,0,    5,8,0,    5,9,0,    5,10,0,   5,11,0, 
 5,12,0,   5,13,0,   5,14,0,   5,15,0,   5,16,0,   5,17,0, 

 6,0,0,    6,1,1,    6,2,0,    6,3,0,    6,4,1,    6,5,1,  /* 108-143 */
 6,6,1,    6,7,0,    6,8,0,    6,9,0,    6,10,0,   6,11,0, 
 6,12,1,   6,13,0,   6,14,0,   6,15,0,   6,16,0,   6,17,0, 
 7,0,0,    7,1,1,    7,2,0,    7,3,0,    7,4,1,    7,5,1,  
 7,6,1,    7,7,1,    7,8,0,    7,9,0,    7,10,0,   7,11,0, 
 7,12,1,   7,13,1,   7,14,0,   7,15,0,   7,16,0,   7,17,0, 

 8,0,0,    8,1,1,    8,2,0,    8,3,0,    8,4,1,    8,5,1,  /* 144-179 */
 8,6,1,    8,7,0,    8,8,1,    8,9,0,    8,10,0,   8,11,0, 
 8,12,1,   8,13,0,   8,14,1,   8,15,0,   8,16,0,   8,17,0, 
 9,0,0,    9,1,1,    9,2,0,    9,3,0,    9,4,1,    9,5,1,  
 9,6,1,    9,7,1,    9,8,1,    9,9,1,    9,10,0,   9,11,0, 
 9,12,1,   9,13,1,   9,14,1,   9,15,1,   9,16,0,   9,17,0, 

10,0,0,   10,1,1,   10,2,0,   10,3,0,   10,4,1,   10,5,1,  /* 180-215 */
10,6,1,   10,7,0,   10,8,0,   10,9,0,   10,10,1,  10,11,0, 
10,12,1,  10,13,0,  10,14,0,  10,15,0,  10,16,1,  10,17,0, 
11,0,0,   11,1,1,   11,2,0,   11,3,0,   11,4,1,   11,5,1,  
11,6,1,   11,7,1,   11,8,1,   11,9,1,   11,10,1,  11,11,1, 
11,12,1,  11,13,1,  11,14,1,  11,15,1,  11,16,1,  11,17,1, 

12,0,0,   12,1,1,   12,2,0,   12,3,0,   12,4,1,   12,5,1,  /* 216-251 */
12,6,1,   12,7,0,   12,8,0,   12,9,0,   12,10,0,  12,11,0, 
12,12,1,  12,13,0,  12,14,0,  12,15,0,  12,16,0,  12,17,0, 
13,0,0,   13,1,1,   13,2,0,   13,3,0,   13,4,1,   13,5,1,  
13,6,1,   13,7,1,   13,8,0,   13,9,0,   13,10,0,  13,11,0, 
13,12,1,  13,13,1,  13,14,0,  13,15,0,  13,16,0,  13,17,0, 

14,0,0,   14,1,1,   14,2,0,   14,3,0,   14,4,1,   14,5,1,  /* 252-287 */
14,6,1,   14,7,0,   14,8,1,   14,9,0,   14,10,0,  14,11,0, 
14,12,1,  14,13,0,  14,14,1,  14,15,0,  14,16,0,  14,17,0, 
15,0,0,   15,1,1,   15,2,0,   15,3,0,   15,4,1,   15,5,1,  
15,6,1,   15,7,1,   15,8,1,   15,9,1,   15,10,0,  15,11,0, 
15,12,1,  15,13,1,  15,14,1,  15,15,1,  15,16,0,  15,17,0, 

16,0,0,   16,1,1,   16,2,0,   16,3,0,   16,4,1,   16,5,1,  /* 288-323 */
16,6,1,   16,7,0,   16,8,0,   16,9,0,   16,10,1,  16,11,0, 
16,12,1,  16,13,0,  16,14,0,  16,15,0,  16,16,1,  16,17,0, 
17,0,0,   17,1,1,   17,2,0,   17,3,0,   17,4,1,   17,5,1,  
17,6,1,   17,7,1,   17,8,1,   17,9,1,   17,10,1,  17,11,1, 
17,12,1,  17,13,1,  17,14,1,  17,15,1,  17,16,1,  17,17,1,
};

static struct mlsinfo 
mintdom[] = 
{
0,0,1, 0,1,1, 0,2,1, 0,3,1, 0,4,1, 0,5,1, 0,6,1, 0,7,1, 0,8,1, /*324-359*/
1,0,1, 1,1,1, 1,2,0, 1,3,0, 1,4,0, 1,5,0, 1,6,0, 1,7,0, 1,8,0,
2,0,1, 2,1,1, 2,2,1, 2,3,1, 2,4,1, 2,5,1, 2,6,1, 2,7,1, 2,8,1,
3,0,1, 3,1,1, 3,2,0, 3,3,1, 3,4,1, 3,5,1, 3,6,1, 3,7,1, 3,8,1,

4,0,1, 4,1,1, 4,2,0, 4,3,0, 4,4,1, 4,5,0, 4,6,1, 4,7,0, 4,8,1, /*360-395*/
5,0,1, 5,1,1, 5,2,0, 5,3,0, 5,4,0, 5,5,1, 5,6,1, 5,7,0, 5,8,1,
6,0,1, 6,1,1, 6,2,0, 6,3,0, 6,4,0, 6,5,0, 6,6,1, 6,7,0, 6,8,1,
7,0,1, 7,1,1, 7,2,0, 7,3,0, 7,4,0, 7,5,0, 7,6,0, 7,7,1, 7,8,1,

8,0,1, 8,1,1, 8,2,0, 8,3,0, 8,4,0, 8,5,0, 8,6,0, 8,7,0, 8,8,1, /*396-405*/
};

int
mac_dom_mls(void)
{
    char str[MSGLEN];        /* used to write to logfiles */
    char testname[SLEN];     /* used to write to logfiles */
    char desc[MSGLEN];       /* case description */
    char name[SLEN];         /* holds name of case */
    short msencases = 324;   /* number of msen label test cases */
    short mintcases =  81;   /* number of mint label test cases */
    short i = 0;             /* test case loop counter */
    short npos = 0;          /* positive case counter */
    short nneg = 0;          /* negative case counter */
    int retval;              /* value returned on function call */
    int fail = 0;            /* set when a test case fails */
    int incomplete = 0;      /* set when a test case is incomplete */

    strcpy(testname,"mac_dom_mls");
    /*
     * Write formatted info to raw log.
     */   
    RAWLOG_SETUP( testname, (msencases + mintcases) );
    for (i = 0; i < (msencases + mintcases); i++) {
	/*
	 * Flush output stream. 
	 */    
	flush_raw_log();
	/*
	 * Call mac_dom_mls0() to do each msen case.
	 */    
	if (i < msencases) {
	    retval = mac_dom_mls0(&msendom[i], i, name);
	    if (msendom[i].expect_retval == 1) 
		sprintf(name, "pos%03d", npos++);
	    else
		sprintf(name, "neg%03d", nneg++);
	    /*
	     * Concatenate descriptions of label0 and label1.
	     */    
	    sprintf(desc, "%s/%s", msendesc[msendom[i].i0],
		    msendesc[msendom[i].i1]);
	}
	/*
	 * Call mac_dom_mls1() to do each mint case.
	 */    
	else {

	    retval = mac_dom_mls1(&mintdom[i-msencases], i, name);
	    if (mintdom[i-msencases].expect_retval == 1) 
		sprintf(name, "pos%03d", npos++);
	    else
		sprintf(name, "neg%03d", nneg++);
	    /*
	     * Concatenate descriptions of label0 and label1.
	     */    
	    sprintf(desc, "%s; %s", mintdesc[mintdom[i-msencases].i0],
		    		    mintdesc[mintdom[i-msencases].i1]);
	}
	switch (retval) {
	/*
	 * Write formatted result to raw log.
	 */    
	case PASS:      /* Passed */
	    RAWPASS(i, name);
	    break;
	case FAIL:      /* Failed */
	    RAWFAIL(i, name, desc);
	    fail = 1;
	    break;
	default:
	    RAWINC(i, name);
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
mac_dom_mls0(struct mlsinfo *parmsptr, int num, char* name)
{
    register short i;           /* Loop counter for cats & divs */
    char testname[SLEN];        /* used to write to logfiles */
    char namestring[MSGLEN];    /* Used in error messages. */
    int retval = 0;             /* Return value of test call. */
    pid_t forkval;		/* Returned value of fork call. */
    int status;			/* For wait. */   
    mac_t lptr0;		/* First MAC label pointer. */
    mac_t lptr1;		/* Second MAC label pointer. */

    strcpy(testname,"mac_dom_mls");
    sprintf(namestring, "%s, case %d, %s:\n   ", testname, num, name);

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
     * Set up buffer0
     */
    lptr0 = mac_get_proc();
    lptr0->ml_msen_type = msenlist[parmsptr->i0].msen;
    lptr0->ml_level = msenlist[parmsptr->i0].level;
    lptr0->ml_catcount = msenlist[parmsptr->i0].catcount;
    for (i = 0; i < msenlist[parmsptr->i0].catcount; i++) { 
	lptr0->ml_list[i] = msenlist[parmsptr->i0].catlist[i];
    }
    lptr0->ml_mint_type = MIE;
    /*
     * Set up buffer1
     */
    lptr1 = mac_get_proc();
    lptr1->ml_msen_type = msenlist[parmsptr->i1].msen;
    lptr1->ml_level = msenlist[parmsptr->i1].level;
    lptr1->ml_catcount = msenlist[parmsptr->i1].catcount;
    for (i = 0; i < msenlist[parmsptr->i1].catcount; i++) { 
	lptr1->ml_list[i] = msenlist[parmsptr->i1].catlist[i];
    }
    lptr1->ml_mint_type = MIE;

    
    /*
     * Here's the test: Call mac_dominate(lpr0, lpr1).
     */
    retval = mac_dominate(lptr0, lptr1);
    
    /*
     * For positive cases: retval should be 1.
     */
    if (parmsptr->expect_retval == 1) {
	if ( retval == 0 ) {
	    w_error(GENERAL, namestring, err[TESTCALL], 0);
	    exit(FAIL);
	} 
	if (retval != 1) {
	    w_error(PRINTNUM, namestring, err[TEST_RETVAL],retval);
	    exit(FAIL);
	}
	exit(PASS);
    }
    /*
     * For negative test cases, retval should be 0.  If not, 
     * write error message. Exit 0 for PASS, 1 for FAIL.	 
     */
    
    if ( retval != 0 ) {
	w_error(PRINTNUM, namestring, err[TEST_RETVAL],retval);
	exit(FAIL);
    }
    exit(PASS);
    return(PASS);
}

static int mac_dom_mls1(struct mlsinfo *parmsptr, int num, char* name)
{
    register short i;            /* Loop counter for cats & divs */
    char testname[SLEN];         /* used to write to logfiles */
    char namestring[MSGLEN];     /* Used in error messages. */
    int retval = 0;              /* Return value of test call. */
    pid_t forkval;               /* Returned value of fork call. */
    int status;                  /* For wait. */   
    mac_t lptr0;                 /* First MAC label pointer. */
    mac_t lptr1;                 /* Second MAC label pointer. */

    strcpy(testname,"mac_dom_mls");
    sprintf(namestring, "%s, case %d, %s:\n   ", testname, num, name);

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
     * Set up buffer0
     */
    lptr0 = mac_get_proc();
    lptr0->ml_msen_type = MSE;
    lptr0->ml_mint_type = mintlist[parmsptr->i0].mint;
    lptr0->ml_grade = mintlist[parmsptr->i0].grade;
    lptr0->ml_divcount = mintlist[parmsptr->i0].divcount;
    for (i = 0; i < mintlist[parmsptr->i0].divcount; i++) {
	lptr0->ml_list[i] = mintlist[parmsptr->i0].divlist[i];
    }

    /*
     * Set up buffer1
     */
    lptr1 = mac_get_proc();
    lptr1->ml_msen_type = MSE;
    lptr1->ml_mint_type = mintlist[parmsptr->i1].mint;
    lptr1->ml_grade = mintlist[parmsptr->i1].grade;
    lptr1->ml_divcount = mintlist[parmsptr->i1].divcount;
    for (i = 0; i < mintlist[parmsptr->i1].divcount; i++) {
	lptr1->ml_list[i] = mintlist[parmsptr->i1].divlist[i];
    }

    /*
     * Here's the test: Call mac_dominate(lpr0, lpr1).
     */
    retval = mac_dominate(lptr0, lptr1);
    
    /*
     * For positive cases: retval should be 1.
     */
    if (parmsptr->expect_retval == 1) {
	if ( retval == 0 ) {
	    w_error(GENERAL, namestring, err[TESTCALL], 0);
	    exit(FAIL);
	} 
	if (retval != 1) {
	    w_error(PRINTNUM, namestring, err[TEST_RETVAL],retval);
	    exit(FAIL);
	}
	exit(PASS);
    }
    /*
     * For negative test cases, retval should be 0.  If not, 
     * write error message. Exit 0 for PASS, 1 for FAIL.	 
     */
    
    if ( retval != 0 ) {
	w_error(PRINTNUM, namestring, err[TEST_RETVAL],retval);
	exit(FAIL);
    }
    exit(PASS);
    return(PASS);
}
