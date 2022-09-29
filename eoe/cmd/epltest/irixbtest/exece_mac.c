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
 * exece_mac.c -- MAC read tests for exece() system call.
 */
static int exece_mac1(struct macinfo *parmsptr);


#define EXECL   0    /* Flag values */
#define EXECLE  1
#define EXECLP  2


static struct macinfo
exece_minfo[] = 
{/*
 case  case  c_flag expect  errno   type    directory   file       process
 name number        success         flag    msen/mint   msen/mint  msen/mint
*/

"pos00",  0,     0,  PASS,    0,    EXECL,   MSA,MIE,   MSL,MIE,   MSA,MIE,
"neg00",  1,     0,  FAIL,  EACCES, EXECL,   MSL,MIE,   MSH,MIE,   MSA,MIE,
"neg01",  2,     0,  FAIL,  EACCES, EXECL,   MSH,MIE,   MSL,MIE,   MSA,MIE,
"neg02",  3,     0,  FAIL,  EACCES, EXECL,   MSH,MIE,   MSH,MIE,   MSA,MIE,
"neg03",  4,     0,  FAIL,  EACCES, EXECL,   MSA,MIH,   MSA,MIL,   MSA,MIH,
"neg04",  5,     0,  FAIL,  EACCES, EXECL,   MSA,MIL,   MSA,MIH,   MSA,MIH,
"neg05",  6,     0,  FAIL,  EACCES, EXECL,   MSA,MIL,   MSA,MIL,   MSA,MIH,

"pos01",  7,     0,  PASS,    0,    EXECLE,  MSA,MIE,   MSL,MIE,   MSA,MIE,
"neg06",  8,     0,  FAIL,  EACCES, EXECLE,  MSL,MIE,   MSH,MIE,   MSA,MIE,
"neg07",  9,     0,  FAIL,  EACCES, EXECLE,  MSH,MIE,   MSL,MIE,   MSA,MIE,
"neg08", 10,     0,  FAIL,  EACCES, EXECLE,  MSH,MIE,   MSH,MIE,   MSA,MIE,
"neg09", 11,     0,  FAIL,  EACCES, EXECLE,  MSA,MIH,   MSA,MIL,   MSA,MIH,
"neg10", 12,     0,  FAIL,  EACCES, EXECLE,  MSA,MIL,   MSA,MIH,   MSA,MIH,
"neg11", 13,     0,  FAIL,  EACCES, EXECLE,  MSA,MIL,   MSA,MIL,   MSA,MIH,

"pos02", 14,     0,  PASS,    0,    EXECLP,  MSA,MIE,   MSL,MIE,   MSA,MIE,
"neg12", 15,     0,  FAIL,  EACCES, EXECLP,  MSL,MIE,   MSH,MIE,   MSA,MIE,
"neg13", 16,     0,  FAIL,  EACCES, EXECLP,  MSH,MIE,   MSL,MIE,   MSA,MIE,
"neg14", 17,     0,  FAIL,  EACCES, EXECLP,  MSH,MIE,   MSH,MIE,   MSA,MIE,
"neg15", 18,     0,  FAIL,  EACCES, EXECLP,  MSA,MIH,   MSA,MIL,   MSA,MIH,
"neg16", 19,     0,  FAIL,  EACCES, EXECLP,  MSA,MIL,   MSA,MIH,   MSA,MIH,
"neg17", 20,     0,  FAIL,  EACCES, EXECLP,  MSA,MIL,   MSA,MIL,   MSA,MIH,

};

static char *exece_mdesc[] = { 

"execl; MSEN:P > D, P > F; MINT:P < D, P < F",   
"execl; MSEN:P > D, P !> F; MINT:P < D, P < F",   
"execl; MSEN:P !> D, P > F; MINT:P < D, P < F",   
"execl; MSEN:P !> D, P !> F; MINT:P < D, P < F",   
"execl; MSEN:P > D, P > F; MINT:P < D, P !< F",   
"execl; MSEN:P > D, P > F; MINT:P !< D, P < F",   
"execl; MSEN:P > D, P > F; MINT:P !< D, P !< F", 
  
"execle; MSEN:P > D, P > F; MINT:P < D, P < F",   
"execle; MSEN:P > D, P !> F; MINT:P < D, P < F",   
"execle; MSEN:P !> D, P > F; MINT:P < D, P < F",   
"execle; MSEN:P !> D, P !> F; MINT:P < D, P < F",   
"execle; MSEN:P > D, P > F; MINT:P < D, P !< F",   
"execle; MSEN:P > D, P > F; MINT:P !< D, P < F",   
"execle; MSEN:P > D, P > F; MINT:P !< D, P !< F", 
  
"execlp; MSEN:P > D, P > F; MINT:P < D, P < F",   
"execlp; MSEN:P > D, P !> F; MINT:P < D, P < F",   
"execlp; MSEN:P !> D, P > F; MINT:P < D, P < F",   
"execlp; MSEN:P !> D, P !> F; MINT:P < D, P < F",   
"execlp; MSEN:P > D, P > F; MINT:P < D, P !< F",   
"execlp; MSEN:P > D, P > F; MINT:P !< D, P < F",   
"execlp; MSEN:P > D, P > F; MINT:P !< D, P !< F", 
  
};
	
int
exece_mac(void)
{
    short ncases =  21;      /* number of total test cases */
    int fail = 0;            /* set when a test case fails */
    int incomplete = 0;      /* set when a test case is incomplete */
    short i = 0;             /* test case loop counter */
    char str[MSGLEN];        /* used to write to logfiles */
    char testname[SLEN];     /* used to write to logfiles */

    strcpy(testname,"exece_mac");
    /*
     * Write formatted info to raw log.
     */   
    RAWLOG_SETUP(testname, ncases);
    /*
     * Call function for each test case.
     */    
    for (i = 0; i < ncases; i++) {
	/*
	 * Flush output streams.
	 */    
	flush_raw_log();   
	switch (exece_mac1(&exece_minfo[i]) ) {
	/*
	 * Write formatted result to raw log.
	 */    
	case PASS:      /* Passed */
	    RAWPASS(exece_minfo[i].casenum, exece_minfo[i].casename);
	    break;
	case FAIL:      /* Failed */
	    RAWFAIL(exece_minfo[i].casenum, exece_minfo[i].casename,
		    exece_mdesc[i]);
	    fail = 1;
	    break;
	default:
	    RAWINC(exece_minfo[i].casenum, exece_minfo[i].casename);
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
exece_mac1(struct macinfo *parmsptr)
{
    char **enp;                  /* Pointer returned by get_envp() */
    char buf[MSGLEN];            /* Used to write contents of file. */
    char testname[SLEN];         /* used to write to logfiles */
    char namestring[MSGLEN];     /* Used in error messages. */
    char file[NLEN + NLEN];      /* File name passed to exece(). */
    char dir[NLEN];              /* Directory containing file. */
    int retval = 0;              /* Return value of test call. */    
    int exitcode = 0;            /* Child's exit code. */
    int fildes;                  /* File descriptor.*/
    pid_t forkval;               /* Returned value of fork call. */
    int status;                  /* For wait. */   
    int loc_errno;               /* Saves test call errno. */
    mac_t lptr;                  /* MAC label pointer. */

    strcpy(testname,"exece_mac");
    strcpy (buf, "abc");      /* Stuff to write to pipe. */
    sprintf(namestring, "%s, case %d, %s:\n   ", testname,
	     parmsptr->casenum, parmsptr->casename);
    /*
     * Create names for temp file and directory.
     */
    sprintf(dir,  "%s%d", testname, parmsptr->casenum);
    sprintf(file, "%s/tmp%d", dir, parmsptr->casenum);

    /* 
     * Fork a child to do setplabel and test call.
     */
    if (  ( forkval = fork() ) == -1  ) {
	w_error(SYSCALL, namestring, err[F_FORK], errno);
	return(INCOMPLETE);
    }

    if ( forkval  ) { 
	/* 
	 * This is the parent.	 Wait for child (Subject).
	 * Exit 2 on unexpected error.
	 */
	if ( wait(&status) == -1 ) {
	    w_error(SYSCALL, namestring, err[F_WAIT], errno);
	    return(INCOMPLETE);
    	}
	if ( !WIFEXITED(status)  ) {
	    w_error(GENERAL, namestring, err[C_NOTEXIT], 0);
	    return(INCOMPLETE);
    	}
	exitcode = (WEXITSTATUS(status));


	/*
	 * If child execed successfully, it did not clean up,
	 * so parent must unlink the dir.
	 */
	cap_unlink(file);
	cap_rmdir(dir);

	/*
	 * Positive test cases: child's exitcode should be 5.
	 */
	if (parmsptr->expect_success == PASS ) {
	    switch (exitcode) {
	    case FAIL:       /* exec call unexpectedly failed */
		return(FAIL);
	    case 5:       /* exec call succeeded, as expected */
		return(PASS);
	    default:      /* unexpected error */
		return(INCOMPLETE);
	    }
	}
	/*
	 * Negative test cases: child's exitcode should be 0.
	 */
	else {
	    switch (exitcode) {
	    case PASS:        /* call failed with expected errno */
		return(PASS);
	    case FAIL:        /* call failed with bad errno */
		return(FAIL);
	    case 5:        /* exec call unexpectedly succeded */
	        w_error(PRINTNUM,namestring, err[TEST_UNEXSUCC],retval);
		return(FAIL);
	    default:       /* unexpected error */
		return(INCOMPLETE);
	    }
	}
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
     * Malloc a mac_label pointer.
     */
    lptr = mac_get_proc();
    
    /* 
     * Set directory label
     */	
    lptr->ml_msen_type = parmsptr->dir_msen;
    lptr->ml_mint_type = parmsptr->dir_mint;
    if ( cap_setlabel(dir, lptr) ) {
	w_error(SYSCALL, namestring, err[SETLABEL_DIR], errno);
	exit(INCOMPLETE);
    }
    
    /*
     * Create a file, mode 700
     */
    if ( (fildes = cap_open(file, O_CREAT|O_RDWR, 0700)) < 0 ) {
	w_error(SYSCALL, namestring, err[F_CREAT], errno);
	exit(INCOMPLETE);
    }

    /*
     * Write the file's contents, a sh script that exits with
     * status = 5.
     */
    sprintf(buf, "#!/bin/sh \nexit 5\n");
    if ( write(fildes, buf, sizeof(buf)) != sizeof(buf) ) {
	w_error(SYSCALL, namestring, err[F_WRITE], errno);
	exit(INCOMPLETE);
    }
    /*
     *  Close the file and set its label.
     */
    close(fildes);
    lptr->ml_msen_type = parmsptr->file_msen;
    lptr->ml_mint_type = parmsptr->file_mint;
    if ( cap_setlabel(file, lptr) ) {
	w_error(SYSCALL, namestring, err[SETLABEL_FILE], errno);
	exit(INCOMPLETE);
    }
    
    
    /* 
     * Set process label.
     */
    lptr->ml_msen_type = parmsptr->proc_msen;
    lptr->ml_mint_type = parmsptr->proc_mint;
    if ( cap_setplabel(lptr) ) {
	w_error(SYSCALL, namestring, err[F_SETPLABEL], errno);
	exit(INCOMPLETE);
    }
    
    /*
     * Call the appropriate flavor of exec and save 
     * return value and errno.
     */
    switch (parmsptr->flag) {
    case EXECL:
	retval = execl(file, file, (char *)0);
	break;
    case EXECLE:
	enp = get_envp();
	retval = execle(file, file, (char *)0, enp);
	break;
    case EXECLP:
	retval = execlp(file, file, (char *)0);
	break;
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
     * Positive test cases: Expected result is for exec to succeed.
     * Execed program's exit status is 5.  If exec fails, current
     * child exits with status 1.
     */
    if (parmsptr->expect_success == PASS ) {
	if (retval == -1 ) {
	    w_error(SYSCALL, namestring, err[TESTCALL], loc_errno);
	    exit(FAIL);
	} 
	exit(PASS);
    }
    
    /*
     * Negative test cases: Expected result is for exec to fail
     * with errno=expect_errno. Current child exits with status 0.
     * If errno is an unexpected value, exit status is 1.  If
     * exec unexpectedly succeeds, exit status is 5.
     */
    if (retval != -1 ) {
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
