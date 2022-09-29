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
#include "lbl.h"
  
/*
 * setplabel_lbl.c -- Tests for mac_set_proc() system call.
 */
static int setplabel_lbl0(struct label_info *parmsptr);
static int setplabel_lbl1(struct err_info *parmsptr);

static struct label_info 
setplabel_linfo0[] = 
{
"pos00",  0,  MSA, 0,  0,  0,0,0,0,0,0,0,0,0,0, 
              MIE, 0,  0,  0,0,0,0,0,0,0,0,0,0, PASS , 0,
"neg00",  1,  MSE, 0,  0,  0,0,0,0,0,0,0,0,0,0,  
              MIE, 0,  0,  0,0,0,0,0,0,0,0,0,0, FAIL, EINVAL,
"pos02",  2,  MSH, 0,  0,  0,0,0,0,0,0,0,0,0,0,  
              MIE, 0,  0,  0,0,0,0,0,0,0,0,0,0, PASS , 0,
"pos03",  3, MSMH, 0,  0,  0,0,0,0,0,0,0,0,0,0, 
              MIE, 0,  0,  0,0,0,0,0,0,0,0,0,0, PASS , 0,
"pos04",  4,  MSL, 0,  0,  0,0,0,0,0,0,0,0,0,0,  
              MIE, 0,  0,  0,0,0,0,0,0,0,0,0,0, PASS , 0,
"pos05",  5, MSML, 0,  0,  0,0,0,0,0,0,0,0,0,0, 
              MIE, 0,  0,  0,0,0,0,0,0,0,0,0,0, PASS , 0,

"pos06", 6,  MST,  200, 10,  2, 4, 6, 8,10,12,14,16,60000,65534, 
             MIE,    0,  0,  0,0,0,0,0,0,0,0,0,0, PASS , 0,
"pos07", 7,  MST,    1,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
             MIE,    0,  0,  0,0,0,0,0,0,0,0,0,0, PASS , 0,
"pos08", 8,  MSM,  200, 10,  2, 4, 6, 8,10,12,14,16,60000,65534, 
             MIE,    0,  0,  0,0,0,0,0,0,0,0,0,0, PASS , 0,
"pos09", 9,  MSM,    1,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
             MIE,    0,  0,  0,0,0,0,0,0,0,0,0,0, PASS , 0,

"pos10", 10, MSA, 0,  0,  0,0,0,0,0,0,0,0,0,0,  
             MIH, 0,  0,  0,0,0,0,0,0,0,0,0,0, PASS , 0,
"neg01", 11, MSE, 0,  0,  0,0,0,0,0,0,0,0,0,0,  
             MIH, 0,  0,  0,0,0,0,0,0,0,0,0,0, FAIL, EINVAL,
"pos12", 12, MSH, 0,  0,  0,0,0,0,0,0,0,0,0,0,  
             MIH, 0,  0,  0,0,0,0,0,0,0,0,0,0, PASS , 0,
"pos13", 13, MSMH,0,  0,  0,0,0,0,0,0,0,0,0,0,
             MIH, 0,  0,  0,0,0,0,0,0,0,0,0,0, PASS , 0,
"pos14", 14, MSL, 0,  0,  0,0,0,0,0,0,0,0,0,0,  
             MIH, 0,  0,  0,0,0,0,0,0,0,0,0,0, PASS , 0,
"pos15", 15, MSML,0,  0,  0,0,0,0,0,0,0,0,0,0, 
             MIH, 0,  0,  0,0,0,0,0,0,0,0,0,0, PASS , 0,

"pos16", 16, MST,  200, 10,  2, 4, 6, 8,10,12,14,16,60000,65534, 
             MIH,    0,  0,  0,0,0,0,0,0,0,0,0,0, PASS , 0,
"pos17", 17, MST,    1,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
             MIH,    0,  0,  0,0,0,0,0,0,0,0,0,0, PASS , 0,
"pos18", 18, MSM,  200, 10,  2, 4, 6, 8,10,12,14,16,60000,65534, 
             MIH,    0,  0,  0,0,0,0,0,0,0,0,0,0, PASS , 0,
"pos19", 19, MSM,    1,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
             MIH,    0,  0,  0,0,0,0,0,0,0,0,0,0, PASS , 0,

"pos20", 20, MSA, 0,  0,  0,0,0,0,0,0,0,0,0,0,  
             MIL, 0,  0,  0,0,0,0,0,0,0,0,0,0, PASS , 0,
"neg02", 21, MSE, 0,  0,  0,0,0,0,0,0,0,0,0,0,  
             MIL, 0,  0,  0,0,0,0,0,0,0,0,0,0,FAIL, EINVAL,
"pos22", 22, MSH, 0,  0,  0,0,0,0,0,0,0,0,0,0,  
             MIL, 0,  0,  0,0,0,0,0,0,0,0,0,0, PASS , 0,
"pos23", 23, MSMH,0,  0,  0,0,0,0,0,0,0,0,0,0, 
             MIL, 0,  0,  0,0,0,0,0,0,0,0,0,0, PASS , 0,
"pos24", 24, MSL, 0,  0,  0,0,0,0,0,0,0,0,0,0,  
             MIL, 0,  0,  0,0,0,0,0,0,0,0,0,0, PASS , 0,
"pos25", 25, MSML,0,  0,  0,0,0,0,0,0,0,0,0,0, 
             MIL, 0,  0,  0,0,0,0,0,0,0,0,0,0, PASS , 0,

"pos26", 26, MST,  200, 10,  2, 4, 6, 8,10,12,14,16,60000,65534, 
             MIL,    0,  0,  0,0,0,0,0,0,0,0,0,0, PASS , 0,
"pos27", 27, MST,    1,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
             MIL,    0,  0,  0,0,0,0,0,0,0,0,0,0, PASS , 0,
"pos28", 28, MSM,  200, 10,  2, 4, 6, 8,10,12,14,16,60000,65534, 
             MIL,    0,  0,  0,0,0,0,0,0,0,0,0,0, PASS , 0,
"pos29", 29, MSM,    1,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
             MIL,    0,  0,  0,0,0,0,0,0,0,0,0,0, PASS , 0,

"pos30", 30, MSA,   0,  0,  0,0,0,0,0,0,0,0,0,0,  
             MIB, 200, 10,  3, 6, 9,12,15,18,21,24,30000,65533, PASS , 0,
"neg03", 31, MSE,   0,  0,  0,0,0,0,0,0,0,0,0,0,  
             MIB, 200, 10,  3, 6, 9,12,15,18,21,24,30000,65533,FAIL, EINVAL,
"pos32", 32, MSH,   0,  0,  0,0,0,0,0,0,0,0,0,0,  
             MIB, 200, 10,  3, 6, 9,12,15,18,21,24,30000,65533, PASS , 0,
"pos33", 33, MSMH,  0,  0,  0,0,0,0,0,0,0,0,0,0, 
             MIB, 200, 10,  3, 6, 9,12,15,18,21,24,30000,65533, PASS , 0,
"pos34", 34, MSL,   0,  0,  0,0,0,0,0,0,0,0,0,0,  
             MIB, 200, 10,  3, 6, 9,12,15,18,21,24,30000,65533, PASS , 0,
"pos35", 35, MSML,  0,  0,  0,0,0,0,0,0,0,0,0,0, 
             MIB, 200, 10,  3, 6, 9,12,15,18,21,24,30000,65533, PASS , 0,

"pos36", 36, MST,  200, 10,  2, 4, 6, 8,10,12,14,16,60000,65534,  
             MIB,  200, 10,  3, 6, 9,12,15,18,21,24,30000,65533, PASS , 0,
"pos37", 37, MST,    1,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
             MIB,  200, 10,  3, 6, 9,12,15,18,21,24,30000,65533, PASS , 0,
"pos38", 38, MSM,  200, 10,  2, 4, 6, 8,10,12,14,16,60000,65534, 
             MIB,  200, 10,  3, 6, 9,12,15,18,21,24,30000,65533, PASS , 0,
"pos39", 39, MSM,    1,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
             MIB,  200, 10,  3, 6, 9,12,15,18,21,24,30000,65533, PASS , 0,

"pos40", 40, MSA, 0,  0,  0,0,0,0,0,0,0,0,0,0,  
             MIB, 1,  0,  0,0,0,0,0,0,0,0,0,0, PASS , 0,
"neg04", 41, MSE, 0,  0,  0,0,0,0,0,0,0,0,0,0,  
             MIB, 1,  0,  0,0,0,0,0,0,0,0,0,0,FAIL, EINVAL,
"pos42", 42, MSH, 0,  0,  0,0,0,0,0,0,0,0,0,0,  
             MIB, 1,  0,  0,0,0,0,0,0,0,0,0,0, PASS , 0,
"pos43", 43, MSMH,0,  0,  0,0,0,0,0,0,0,0,0,0, 
             MIB, 1,  0,  0,0,0,0,0,0,0,0,0,0, PASS , 0,
"pos44", 44, MSL, 0,  0,  0,0,0,0,0,0,0,0,0,0,  
             MIB, 1,  0,  0,0,0,0,0,0,0,0,0,0, PASS , 0,
"pos45", 45, MSML,0,  0,  0,0,0,0,0,0,0,0,0,0, 
             MIB, 1,  0,  0,0,0,0,0,0,0,0,0,0, PASS , 0,

"pos46", 46, MST,  200, 10,  2, 4, 6, 8,10,12,14,16,60000,65534, 
             MIB,    1,  0,  0,0,0,0,0,0,0,0,0,0, PASS , 0,
"pos47", 47, MST,    1,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
             MIB,    1,  0,  0,0,0,0,0,0,0,0,0,0, PASS , 0,
"pos48", 48, MSM,  200, 10,  2, 4, 6, 8,10,12,14,16,60000,65534, 
             MIB,    1,  0,  0,0,0,0,0,0,0,0,0,0, PASS , 0,
"pos49", 49, MSM,    1,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
             MIB,    1,  0,  0,0,0,0,0,0,0,0,0,0, PASS , 0,

};

static struct err_info
setplabel_linfo1[] =
{
"neg05", 50, EFAULT
};

static char *setplabel_ldesc0[] = {
"MSEN_ADMIN, MINT_EQUAL",
"MSEN_EQUAL, MINT_EQUAL",
"MSEN_HIGH,  MINT_EQUAL",
"MSEN_MLD_HIGH, MINT_EQUAL",
"MSEN_LOW,   MINT_EQUAL",
"MSEN_MLD_LOW, MINT_EQUAL",

"MSEN_TCSEC, level, cats, MINT_EQUAL",
"MSEN_TCSEC, level, MINT_EQUAL",
"MSEN_MLD, level, cats, MINT_EQUAL, plabel MLD",
"MSEN_MLD, level, MINT_EQUAL, plabel MLD",

"MSEN_ADMIN, MINT_HIGH",
"MSEN_EQUAL, MINT_HIGH",
"MSEN_HIGH,  MINT_HIGH",
"MSEN_MLD_HIGH, MINT_HIGH, plabel MLD",
"MSEN_LOW,   MINT_HIGH",
"MSEN_MLD_LOW, MINT_HIGH, plabel MLD",

"MSEN_TCSEC, level, cats, MINT_HIGH",
"MSEN_TCSEC, level, MINT_HIGH",
"MSEN_MLD, level, cats, MINT_HIGH, plabel MLD",
"MSEN_MLD, level, MINT_HIGH, plabel MLD",

"MSEN_ADMIN, MINT_LOW",
"MSEN_EQUAL, MINT_LOW",
"MSEN_HIGH,  MINT_LOW",
"MSEN_MLD_HIGH, MINT_LOW, plabel MLD",
"MSEN_LOW,   MINT_LOW",
"MSEN_MLD_LOW, MINT_LOW, plabel MLD",

"MSEN_TCSEC, level, cats, MINT_LOW",
"MSEN_TCSEC, level, MINT_LOW",
"MSEN_MLD, level, cats, MINT_LOW, plabel MLD",
"MSEN_MLD, level, MINT_LOW, plabel MLD",

"MSEN_ADMIN, MINT_BIBA, grade, divs",
"MSEN_EQUAL, MINT_BIBA, grade, divs",
"MSEN_HIGH,  MINT_BIBA, grade, divs",
"MSEN_MLD_HIGH, MINT_BIBA, grade, divs, plabel MLD",
"MSEN_LOW,   MINT_BIBA, grade, divs",
"MSEN_MLD_LOW, MINT_BIBA, grade, divs, plabel MLD",

"MSEN_TCSEC, level, cats, MINT_BIBA, grade, divs",
"MSEN_TCSEC, level, MINT_BIBA, grade, divs",
"MSEN_MLD, level, cats, MINT_BIBA, grade, divs, plabel MLD",
"MSEN_MLD, level, MINT_BIBA, grade, divs, plabel MLD",


"MSEN_ADMIN, MINT_BIBA, grade",
"MSEN_EQUAL, MINT_BIBA, grade",
"MSEN_HIGH,  MINT_BIBA, grade",
"MSEN_MLD_HIGH, MINT_BIBA, grade, plabel MLD",
"MSEN_LOW,   MINT_BIBA, grade" ,
"MSEN_MLD_LOW, MINT_BIBA, grade, plabel MLD",

"MSEN_TCSEC, level, cats, MINT_BIBA, grade",
"MSEN_TCSEC, level, MINT_BIBA, grade",
"MSEN_MLD, level, cats, MINT_BIBA, grade, plabel MLD",
"MSEN_MLD, level, MINT_BIBA, grade, plabel MLD",

"Invalid address",
};

int
setplabel_lbl(void)
{
    char str[MSGLEN];        /* used to write to logfiles */
    char testname[SLEN];     /* used to write to logfiles */
    char *name;              /* holds casename from struct */
    short ncases0  = 50;     /* number of label-matching test cases */
    short ncases1  = 1;      /* number of label-matching test cases */
    short num;               /* holds casenum from struct */
    int retval;              /* value returned on function call */
    int fail = 0;            /* set when a test case fails */
    int incomplete = 0;      /* set when a test case is incomplete */
    short i = 0;             /* test case loop counter */

    strcpy(testname,"setplabel_lbl");
    /*
     * Write formatted info to raw log.
     */   
    RAWLOG_SETUP( testname, (ncases0 + ncases1) );
    /*
     * Call function setplabel_lbl0() for each label comparison test case.
     */    
    for (i = 0; i < (ncases0 + ncases1); i++) {
	/*
	 * Flush output streams. 
	 */    
	flush_raw_log();   
	if (i < ncases0) {
	    retval = setplabel_lbl0(&setplabel_linfo0[i]);
	    num = setplabel_linfo0[i].casenum;
	    name = setplabel_linfo0[i].casename;
	}
	else {
	    retval = setplabel_lbl1(&setplabel_linfo1[i - ncases0]);
	    num = setplabel_linfo1[i - ncases0].casenum;
	    name = setplabel_linfo1[i - ncases0].casename;
	}
	switch (retval) {
	/*
	 * Write formatted result to raw log.
	 */    
	case PASS:      /* Passed */
	    RAWPASS(num, name);
	    break;
	case FAIL:      /* Failed */
	    RAWFAIL(num, name, setplabel_ldesc0[i]);
	    fail = 1;
	    break;
	default:
	    RAWINC(num, name);
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
setplabel_lbl0(struct label_info *parmsptr)
{
    register short i;            /* Loop counter for cats & divs */
    char testname[SLEN];         /* used to write to logfiles */
    char namestring[MSGLEN];     /* Used in error messages. */
    int retval = 0;              /* Return value of test call. */
    pid_t forkval;               /* Returned value of fork call. */
    int status;                  /* For wait. */   
    int loc_errno;               /* Saves test call errno. */
    mac_t lptr;                  /* MAC label pointer. */
    mac_t testptr;               /* MAC label pointer for test call. */

    strcpy(testname,"setplabel_lbl");
    sprintf(namestring, "%s, case %d, %s:\n   ", testname,
	     parmsptr->casenum, parmsptr->casename);

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
     * Malloc a mac_label pointer for test call and set
     * up the buffer.
     */
    testptr = mac_get_proc();

    testptr->ml_msen_type = parmsptr->obj_msen;
    testptr->ml_level = parmsptr->obj_level;
    testptr->ml_catcount = parmsptr->obj_catcount;
    for ( i = 0; i < parmsptr->obj_catcount; i++ ) { 
	testptr->ml_list[i] = parmsptr->obj_catlist[i];
    }
    testptr->ml_mint_type = parmsptr->obj_mint;
    testptr->ml_grade = parmsptr->obj_grade;
    testptr->ml_divcount = parmsptr->obj_divcount;
    for ( i = 0; i < parmsptr->obj_divcount; i++ ) {
	testptr->ml_list[i + parmsptr->obj_catcount] =
	    parmsptr->obj_divlist[i];
    }
    
    /*
     * Here's the test: Call mac_set_proc(testptr).
     * Save errno because it could be reset.  Do mac_get_proc,
     * then clean up.
     */
    retval = cap_setplabel(testptr);
    mac_free(testptr);
    loc_errno = errno;
    lptr = mac_get_proc();
    /*
     * For positive cases: retval should be 0, label
     * filled in by mac_get_proc should match what in the buffer.
     */
    if ( parmsptr->expect_success == PASS  ) {
	if ( retval == -1 ) {
	    w_error(SYSCALL, namestring, err[TESTCALL],  loc_errno);
	    exit(FAIL);
	} 
	if ( retval != 0 ) {
	    w_error(PRINTNUM, namestring, err[TEST_RETVAL],retval);
	    exit(FAIL);
	}
	if ( ( lptr->ml_msen_type != parmsptr->obj_msen ) ||
	    ( lptr->ml_mint_type != parmsptr->obj_mint ) ||
	    ( lptr->ml_level != parmsptr->obj_level ) ||
	    ( lptr->ml_grade != parmsptr->obj_grade ) ||
	    ( lptr->ml_catcount != parmsptr->obj_catcount ) ||
	    ( lptr->ml_divcount != parmsptr->obj_divcount ) ) {
	    w_error(GENERAL, namestring, err[BADLABEL], 0);
	    exit(FAIL);
	}
	for ( i = 0; i < parmsptr->obj_catcount; i++ ) {
	    if ( lptr->ml_list[i] != parmsptr->obj_catlist[i] ) {
		w_error(GENERAL, namestring, err[BADLABEL], 0);
		exit(FAIL);
	    }
	}
	for ( i = 0; i < parmsptr->obj_divcount; i++ ) {
	    if ( lptr->ml_list[i + parmsptr->obj_catcount] !=
		parmsptr->obj_divlist[i] ) {
		w_error(GENERAL, namestring, err[BADLABEL], 0);
		exit(FAIL);
	    }
	}
	exit(PASS);
    }
    /*
     * For negative test cases, retval should be -1 and
     * loc_errno should equal parmsptr->expect_errno.  If not, 
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

static int
setplabel_lbl1(struct err_info *parmsptr)
{
    char testname[SLEN];         /* used to write to logfiles */
    char namestring[MSGLEN];     /* Used in error messages. */
    int retval = 0;              /* Return value of test call. */
    pid_t forkval;               /* Returned value of fork call. */
    int status;                  /* For wait. */   

    strcpy(testname,"setlabel_lbl");
    sprintf(namestring, "%s, case %d, %s:\n   ", testname,
	     parmsptr->casenum, parmsptr->casename);

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
     * Here's the test: Call mac_set_proc((mac_label *)1).
     */
    retval = cap_setplabel((mac_t) 1);

    /*
     * All cases are negative.  Retval should be -1 and
     * errno should equal parmsptr->expect_errno.  If not, 
     * write error message. Exit 0 for PASS, 1 for FAIL.	 
     */
    
    if ( retval != -1 ) {
	w_error(PRINTNUM, namestring, err[TEST_RETVAL],retval);
	exit(FAIL);
    }
    if ( errno != parmsptr->expect_errno ) {
	w_error(SYSCALL, namestring, err[TEST_ERRNO],errno);
	exit(FAIL);
    }
    exit(PASS);
    return(PASS);
}
