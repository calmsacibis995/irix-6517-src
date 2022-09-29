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
 * getlabel_lbl.c -- Tests for mac_get_file() system call.
 */
static int getlabel_lbl0(struct label_info *parmsptr);

static struct label_info 
getlabel_linfo0[] = 
{
"pos00",  0,  MSA, 0,  0,  0,0,0,0,0,0,0,0,0,0, 
              MIE, 0,  0,  0,0,0,0,0,0,0,0,0,0,  0,0,
"pos01",  1,  MSE, 0,  0,  0,0,0,0,0,0,0,0,0,0,  
              MIE, 0,  0,  0,0,0,0,0,0,0,0,0,0,  0,0,
"pos02",  2,  MSH, 0,  0,  0,0,0,0,0,0,0,0,0,0,  
              MIE, 0,  0,  0,0,0,0,0,0,0,0,0,0,  0,0,
"pos03",  3, MSMH, 0,  0,  0,0,0,0,0,0,0,0,0,0, 
              MIE, 0,  0,  0,0,0,0,0,0,0,0,0,0,  0,0,
"pos04",  4,  MSL, 0,  0,  0,0,0,0,0,0,0,0,0,0,  
              MIE, 0,  0,  0,0,0,0,0,0,0,0,0,0,  0,0,
"pos05",  5, MSML, 0,  0,  0,0,0,0,0,0,0,0,0,0, 
              MIE, 0,  0,  0,0,0,0,0,0,0,0,0,0,  0,0,

"pos06", 6,  MST,  200, 10,  2, 4, 6, 8,10,12,14,16,60000,65534, 
             MIE,    0,  0,  0,0,0,0,0,0,0,0,0,0,  0,0,
"pos07", 7,  MST,    1,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
             MIE,    0,  0,  0,0,0,0,0,0,0,0,0,0,  0,0,
"pos08", 8,  MSM,  200, 10,  2, 4, 6, 8,10,12,14,16,60000,65534, 
             MIE,    0,  0,  0,0,0,0,0,0,0,0,0,0,  0,0,
"pos09", 9,  MSM,    1,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
             MIE,    0,  0,  0,0,0,0,0,0,0,0,0,0,  0,0,

"pos10", 10, MSA, 0,  0,  0,0,0,0,0,0,0,0,0,0,  
             MIH, 0,  0,  0,0,0,0,0,0,0,0,0,0,  0,0,
"pos11", 11, MSE, 0,  0,  0,0,0,0,0,0,0,0,0,0,  
             MIH, 0,  0,  0,0,0,0,0,0,0,0,0,0,  0,0,
"pos12", 12, MSH, 0,  0,  0,0,0,0,0,0,0,0,0,0,  
             MIH, 0,  0,  0,0,0,0,0,0,0,0,0,0,  0,0,
"pos13", 13, MSMH,0,  0,  0,0,0,0,0,0,0,0,0,0,
             MIH, 0,  0,  0,0,0,0,0,0,0,0,0,0,  0,0,
"pos14", 14, MSL, 0,  0,  0,0,0,0,0,0,0,0,0,0,  
             MIH, 0,  0,  0,0,0,0,0,0,0,0,0,0,  0,0,
"pos15", 15, MSML,0,  0,  0,0,0,0,0,0,0,0,0,0, 
             MIH, 0,  0,  0,0,0,0,0,0,0,0,0,0,  0,0,

"pos16", 16, MST,  200, 10,  2, 4, 6, 8,10,12,14,16,60000,65534, 
             MIH,    0,  0,  0,0,0,0,0,0,0,0,0,0,  0,0,
"pos17", 17, MST,    1,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
             MIH,    0,  0,  0,0,0,0,0,0,0,0,0,0,  0,0,
"pos18", 18, MSM,  200, 10,  2, 4, 6, 8,10,12,14,16,60000,65534, 
             MIH,    0,  0,  0,0,0,0,0,0,0,0,0,0,  0,0,
"pos19", 19, MSM,    1,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
             MIH,    0,  0,  0,0,0,0,0,0,0,0,0,0,  0,0,

"pos20", 20, MSA, 0,  0,  0,0,0,0,0,0,0,0,0,0,  
             MIL, 0,  0,  0,0,0,0,0,0,0,0,0,0,  0,0,
"pos21", 21, MSE, 0,  0,  0,0,0,0,0,0,0,0,0,0,  
             MIL, 0,  0,  0,0,0,0,0,0,0,0,0,0,  0,0,
"pos22", 22, MSH, 0,  0,  0,0,0,0,0,0,0,0,0,0,  
             MIL, 0,  0,  0,0,0,0,0,0,0,0,0,0,  0,0,
"pos23", 23, MSMH,0,  0,  0,0,0,0,0,0,0,0,0,0, 
             MIL, 0,  0,  0,0,0,0,0,0,0,0,0,0,  0,0,
"pos24", 24, MSL, 0,  0,  0,0,0,0,0,0,0,0,0,0,  
             MIL, 0,  0,  0,0,0,0,0,0,0,0,0,0,  0,0,
"pos25", 25, MSML,0,  0,  0,0,0,0,0,0,0,0,0,0, 
             MIL, 0,  0,  0,0,0,0,0,0,0,0,0,0,  0,0,

"pos26", 26, MST,  200, 10,  2, 4, 6, 8,10,12,14,16,60000,65534, 
             MIL,    0,  0,  0,0,0,0,0,0,0,0,0,0,  0,0,
"pos27", 27, MST,    1,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
             MIL,    0,  0,  0,0,0,0,0,0,0,0,0,0,  0,0,
"pos28", 28, MSM,  200, 10,  2, 4, 6, 8,10,12,14,16,60000,65534, 
             MIL,    0,  0,  0,0,0,0,0,0,0,0,0,0,  0,0,
"pos29", 29, MSM,    1,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
             MIL,    0,  0,  0,0,0,0,0,0,0,0,0,0,  0,0,

"pos30", 30, MSA,   0,  0,  0,0,0,0,0,0,0,0,0,0,  
             MIB, 200, 10,  3, 6, 9,12,15,18,21,24,30000,65533,  0,0,
"pos31", 31, MSE,   0,  0,  0,0,0,0,0,0,0,0,0,0,  
             MIB, 200, 10,  3, 6, 9,12,15,18,21,24,30000,65533,  0,0,
"pos32", 32, MSH,   0,  0,  0,0,0,0,0,0,0,0,0,0,  
             MIB, 200, 10,  3, 6, 9,12,15,18,21,24,30000,65533,  0,0,
"pos33", 33, MSMH,  0,  0,  0,0,0,0,0,0,0,0,0,0, 
             MIB, 200, 10,  3, 6, 9,12,15,18,21,24,30000,65533,  0,0,
"pos34", 34, MSL,   0,  0,  0,0,0,0,0,0,0,0,0,0,  
             MIB, 200, 10,  3, 6, 9,12,15,18,21,24,30000,65533,  0,0,
"pos35", 35, MSML,  0,  0,  0,0,0,0,0,0,0,0,0,0, 
             MIB, 200, 10,  3, 6, 9,12,15,18,21,24,30000,65533,  0,0,

"pos36", 36, MST,  200, 10,  2, 4, 6, 8,10,12,14,16,60000,65534,  
             MIB,  200, 10,  3, 6, 9,12,15,18,21,24,30000,65533,  0,0,
"pos37", 37, MST,    1,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
             MIB,  200, 10,  3, 6, 9,12,15,18,21,24,30000,65533,  0,0,
"pos38", 38, MSM,  200, 10,  2, 4, 6, 8,10,12,14,16,60000,65534, 
             MIB,  200, 10,  3, 6, 9,12,15,18,21,24,30000,65533,  0,0,
"pos39", 39, MSM,    1,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
             MIB,  200, 10,  3, 6, 9,12,15,18,21,24,30000,65533,  0,0,

"pos40", 40, MSA, 0,  0,  0,0,0,0,0,0,0,0,0,0,  
             MIB, 1,  0,  0,0,0,0,0,0,0,0,0,0,  0,0,
"pos41", 41, MSE, 0,  0,  0,0,0,0,0,0,0,0,0,0,  
             MIB, 1,  0,  0,0,0,0,0,0,0,0,0,0,  0,0,
"pos42", 42, MSH, 0,  0,  0,0,0,0,0,0,0,0,0,0,  
             MIB, 1,  0,  0,0,0,0,0,0,0,0,0,0,  0,0,
"pos43", 43, MSMH,0,  0,  0,0,0,0,0,0,0,0,0,0, 
             MIB, 1,  0,  0,0,0,0,0,0,0,0,0,0,  0,0,
"pos44", 44, MSL, 0,  0,  0,0,0,0,0,0,0,0,0,0,  
             MIB, 1,  0,  0,0,0,0,0,0,0,0,0,0,  0,0,
"pos45", 45, MSML,0,  0,  0,0,0,0,0,0,0,0,0,0, 
             MIB, 1,  0,  0,0,0,0,0,0,0,0,0,0,  0,0,

"pos46", 46, MST,  200, 10,  2, 4, 6, 8,10,12,14,16,60000,65534, 
             MIB,    1,  0,  0,0,0,0,0,0,0,0,0,0,  0,0,
"pos47", 47, MST,    1,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
             MIB,    1,  0,  0,0,0,0,0,0,0,0,0,0,  0,0,
"pos48", 48, MSM,  200, 10,  2, 4, 6, 8,10,12,14,16,60000,65534, 
             MIB,    1,  0,  0,0,0,0,0,0,0,0,0,0,  0,0,
"pos49", 49, MSM,    1,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
             MIB,    1,  0,  0,0,0,0,0,0,0,0,0,0,  0,0,
};

static char *getlabel_ldesc0[] = { 
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
};

int
getlabel_lbl(void)
{
    char str[MSGLEN];        /* used to write to logfiles */
    char testname[SLEN];     /* used to write to logfiles */
    char *name;              /* holds casename from struct */
    short ncases0  = 50;     /* number of label-matching test cases */
    short num;               /* holds casenum from struct */
    int retval;              /* value returned on function call */
    int fail = 0;            /* set when a test case fails */
    int incomplete = 0;      /* set when a test case is incomplete */
    short i = 0;             /* test case loop counter */

    strcpy(testname,"getlabel_lbl");
    /*
     * Write formatted info to raw log.
     */   
    RAWLOG_SETUP( testname, ncases0 );
    /*
     * Call function getlabel_lbl0() for each label comparison test case.
     */    
    for (i = 0; i < ncases0; i++) {
	/*
	 * Flush output streams. 
	 */    
	flush_raw_log();   
	retval = getlabel_lbl0(&getlabel_linfo0[i]);
	num = getlabel_linfo0[i].casenum;
	name = getlabel_linfo0[i].casename;

	switch (retval) {
	/*
	 * Write formatted result to raw log.
	 */    
	case PASS:      /* Passed */
	    RAWPASS(num, name);
	    break;
	case FAIL:      /* Failed */
	    RAWFAIL(num, name, getlabel_ldesc0[i]);
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

static int getlabel_lbl0(struct label_info *parmsptr)
{
    register short i;            /* Loop counter for cats & divs */
    char testname[SLEN];         /* used to write to logfiles */
    char namestring[MSGLEN];     /* Used in error messages. */
    char obj[NLEN];              /* Obj name passed to mac_get_file(). */
    int retval = 0;              /* Return value of test call. */
    pid_t forkval;               /* Returned value of fork call. */
    int status,waitval;          /* For wait. */   
    int loc_errno;               /* Saves test call errno. */
    mac_t lptr;                  /* MAC label pointer. */
    mac_t testptr;               /* MAC label pointer for test call. */

    strcpy(testname,"getlabel_lbl");
    sprintf(namestring, "%s, case %d, %s:\n   ", testname,
	     parmsptr->casenum, parmsptr->casename);
    /*
     * Create name for temp obj.
     */
    sprintf(obj, "./%s%d", testname, parmsptr->casenum);

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
	waitval = wait(&status);
	if (waitval == -1 ) {
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
     * If param label is moldy, set process label moldy 
     * high so we can deal with a moldy directory.
     * Otherwise, set it msenhigh/mintequal.
     */
    lptr = mac_get_proc();
    
    if ( (parmsptr->obj_msen == MSM) || (parmsptr->obj_msen == MSMH) ||
	(parmsptr->obj_msen == MSML) ) {
	lptr->ml_msen_type = MSMH;
    }
    else {
	  lptr->ml_msen_type = MSH;
      }
    lptr->ml_mint_type = MIE;
    if ( cap_setplabel(lptr) ) {
	w_error(SYSCALL, namestring, err[F_SETPLABEL], errno);
	cap_rmdir(obj);
	exit(INCOMPLETE);
    }
    mac_free(lptr);
    lptr = (mac_label*)0;

    /*
     * Delete left-over object, if any, create a 
     * directory (object) and set its label.
     */
    cap_rmdir(obj);
    if ( cap_mkdir(obj, 0707) ) {
	w_error(SYSCALL, namestring, err[F_MKDIR], errno);
	exit(INCOMPLETE);
    }

    lptr = mac_get_proc();
    lptr->ml_msen_type = parmsptr->obj_msen;
    lptr->ml_level = parmsptr->obj_level;
    lptr->ml_catcount = parmsptr->obj_catcount;
    for (i = 0; i < parmsptr->obj_catcount; i++) { 
	lptr->ml_list[i] = parmsptr->obj_catlist[i];
    }
    lptr->ml_mint_type = parmsptr->obj_mint;
    lptr->ml_grade = parmsptr->obj_grade;
    lptr->ml_divcount = parmsptr->obj_divcount;
    for (i = 0; i < parmsptr->obj_divcount; i++) {
	lptr->ml_list[i + parmsptr->obj_catcount] =
	    parmsptr->obj_divlist[i];
    }
    if ( cap_setlabel(obj, lptr) ) {
	w_error(SYSCALL,namestring,err[SETLABEL_DIR],errno);
	cap_rmdir(obj);
	exit(INCOMPLETE);
    }
    mac_free(lptr);
    
    /*
     * Here's the test: Call mac_get_file(obj)
     * Save errno because it could be reset.
     */
    testptr = mac_get_file(obj);
    loc_errno = errno;
    retval = (testptr == NULL ? -1 : 0);
    
    cap_rmdir(obj);
    /*
     * All cases are positive: retval should be 0, label
     * fields should match what was passed to mac_set_file().
     */
    if ( retval == -1 ) {
	w_error(SYSCALL, namestring, err[TESTCALL], loc_errno);
	exit(FAIL);
    } 
    if (retval != 0) {
	w_error(PRINTNUM, namestring, err[TEST_RETVAL],retval);
	exit(FAIL);
    }
    if ( (testptr->ml_msen_type != parmsptr->obj_msen) ||
	(testptr->ml_mint_type != parmsptr->obj_mint) ||
	(testptr->ml_level != parmsptr->obj_level) ||
	(testptr->ml_grade != parmsptr->obj_grade) ||
	(testptr->ml_catcount != parmsptr->obj_catcount) ||
	(testptr->ml_divcount != parmsptr->obj_divcount) ) {
	w_error(GENERAL, namestring, err[BADLABEL], 0);
	exit(FAIL);
    }
    for (i = 0; i < parmsptr->obj_catcount; i++) {
	if (testptr->ml_list[i] != parmsptr->obj_catlist[i]) {
	    w_error(GENERAL, namestring, err[BADLABEL], 0);
	    exit(FAIL);
	}
    }
    for (i = 0; i < parmsptr->obj_divcount; i++) {
	if (testptr->ml_list[i + parmsptr->obj_catcount] !=
	    parmsptr->obj_divlist[i]) {
	    w_error(GENERAL, namestring, err[BADLABEL], 0);
	    exit(FAIL);
	}
    }
    exit(PASS);
    return(PASS);
}
