/*
 * Copyright 1993, Silicon Graphics, Inc. 
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
#include "cap.h"

/*
 * set_fxid.c -- Capability tests for chown() system call.
 */
static int set_fxid1(struct fxid_cparm1 *parmsptr);

#define	S_UGXID	(S_ISUID | S_ISGID)	/* execution bits for user & group */
#define S_AUGPS	(S_IRWXU | S_IRWXG)	/* read, write & execute user & grp*/

/*
 * Each structure contains information for one test case.
 * Variables are uids of Subject and Object processes,
 * whether the test call is expected to succeed (positive case) or fail
 * (negative case), and errno expected in each negative case. 
 */
static struct fxid_cparm1
fxid_cparms[] = {
/*
 *name    #  Suid   Sgid    Ouid   Ogid    xflag    flag     success  errno
 */
"pos00",  0, SUPER, SUPER,  SUPER, SUPER,  S_UGXID, S_AUGPS, PASS ,      0,
	"CAP_FSETID+e",
"pos01",  1, SUPER, SUPER,  SUPER, SUPER,  S_UGXID, S_AUGPS, PASS ,      0,
	"CAP_FSETID+p",
"pos02",  2, SUPER, SUPER,  TEST2, TESTG2, S_UGXID, S_AUGPS, PASS ,      0,
	"CAP_FSETID+e",
"pos03",  3, SUPER, SUPER,  TEST2, TESTG2, S_UGXID, S_AUGPS, PASS ,      0,
	"CAP_FSETID+p",
"pos04",  4, TEST2, TESTG2, TEST2, TESTG2, S_UGXID, S_AUGPS, PASS ,      0,
	"CAP_FSETID+e",
"pos05",  5, TEST2, TESTG2, TEST2, TESTG2, S_UGXID, S_AUGPS, PASS ,      0,
	"CAP_FSETID+p",
"pos06",  6, TEST0, TESTG0, TEST2, TESTG2, S_UGXID, S_AUGPS, PASS ,      0,
	"CAP_FSETID+e",
"neg00",  7, TEST0, TESTG0, TEST2, TESTG2, S_UGXID, S_AUGPS, FAIL,   EPERM,
	"CAP_FSETID+p",
	
"pos07",  8, SUPER, SUPER,  SUPER, SUPER,  S_ISUID, S_IRWXU, PASS ,      0,
	"CAP_FSETID+e",
"pos08",  9, SUPER, SUPER,  SUPER, SUPER,  S_ISUID, S_IRWXU, PASS ,      0,
	"CAP_FSETID+p",
"pos09", 10, SUPER, SUPER,  TEST2, TESTG2, S_ISUID, S_IRWXU, PASS ,      0,
	"CAP_FSETID+e",
"pos10", 11, SUPER, SUPER,  TEST2, TESTG2, S_ISUID, S_IRWXU, PASS ,      0,
	"CAP_FSETID+p",
"pos11", 12, TEST2, TESTG2, TEST2, TESTG2, S_ISUID, S_IRWXU, PASS ,      0,
	"CAP_FSETID+e",
"pos12", 13, TEST2, TESTG2, TEST2, TESTG2, S_ISUID, S_IRWXU, PASS ,      0,
	"CAP_FSETID+p",
"pos13", 14, TEST0, TESTG0, TEST2, TESTG2, S_ISUID, S_IRWXU, PASS ,      0,
	"CAP_FSETID+e",
"neg01", 15, TEST0, TESTG0, TEST2, TESTG2, S_ISUID, S_IRWXU, FAIL,   EPERM,
	"CAP_FSETID+p",
	
"pos14", 16, SUPER, SUPER,  SUPER, SUPER,  S_ISGID, S_ISGID, PASS ,      0,
	"CAP_FSETID+e",
"pos15", 17, SUPER, SUPER,  SUPER, SUPER,  S_ISGID, S_ISGID, PASS ,      0,
	"CAP_FSETID+p",
"pos16", 18, SUPER, SUPER,  TEST2, TESTG2, S_ISGID, S_ISGID, PASS ,      0,
	"CAP_FSETID+e",
"pos17", 19, SUPER, SUPER,  TEST2, TESTG2, S_ISGID, S_ISGID, PASS ,      0,
	"CAP_FSETID+p",
"pos18", 20, TEST2, TESTG2, TEST2, TESTG2, S_ISGID, S_ISGID, PASS ,      0,
	"CAP_FSETID+e",
"pos19", 21, TEST2, TESTG2, TEST2, TESTG2, S_ISGID, S_ISGID, PASS ,      0,
	"CAP_FSETID+p",
"pos20", 22, TEST0, TESTG0, TEST2, TESTG2, S_ISGID, S_ISGID, PASS ,      0,
	"CAP_FSETID+e",
"neg03", 23, TEST0, TESTG0, TEST2, TESTG2, S_ISGID, S_ISGID, FAIL,   EPERM,
	"CAP_FSETID+p"
};

/* 
 * Description strings, corresponding with test cases, 
 * to be printed to the raw log if that case fails.
 */
static char *chown_cdesc[] = { 
"Seuid super = Ouid, S_UGXID, S_AUGPS, Effe",
"Seuid super = Ouid, S_UGXID, S_AUGPS, Perm",
"Seuid super != Ouid, S_UGXID, S_AUGPS, Effe",
"Seuid super != Ouid, S_UGXID, S_AUGPS, Perm",
"Seuid = Ouid, S_UGXID, S_AUGPS, Effe",
"Seuid = Ouid, S_UGXID, S_AUGPS, Perm",
"Seuid != Oeuid, S_UGXID, S_AUGPS, Effe",
"Seuid != Oeuid, S_UGXID, S_AUGPS, Perm",

"Seuid super = Ouid, S_ISUID, S_IRWXU, Effe",
"Seuid super = Ouid, S_ISUID, S_IRWXU, Perm",
"Seuid super != Ouid, S_ISUID, S_IRWXU, Effe",
"Seuid super != Ouid, S_ISUID, S_IRWXU, Perm",
"Seuid = Ouid, S_ISUID, S_IRWXU, Effe",
"Seuid = Ouid, S_ISUID, S_IRWXU, Perm",
"Seuid != Oeuid, S_ISUID, S_IRWXU, Effe",
"Seuid != Oeuid, S_ISUID, S_IRWXU, Perm",

"Seuid super = Ouid, S_ISGID, S_ISGID, Effe",
"Seuid super = Ouid, S_ISGID, S_ISGID, Perm",
"Seuid super != Ouid, S_ISGID, S_ISGID, Effe",
"Seuid super != Ouid, S_ISGID, S_ISGID, Perm",
"Seuid = Ouid, S_ISGID, S_ISGID, Effe",
"Seuid = Ouid, S_ISGID, S_ISGID, Perm",
"Seuid != Oeuid, S_ISGID, S_ISGID, Effe",
"Seuid != Oeuid, S_ISGID, S_ISGID, Perm"
};

int
set_fxid()
{
	char str[MSGLEN];        /* used to write to logfiles */
	char testname[SLEN];     /* used to write to logfiles */
	short ncases1 = 24;       /* number of test cases */
	int fail = 0;            /* set when test case fails */
	int incomplete = 0;      /* set when test case incomplete */
	int retval;              /* value returned on function call */
	short i = 0;             /* test case loop counter */
	
	strcpy(testname,"set_fxid");
	/*
	 * Write formatted info to raw log.
	 */    
	RAWLOG_SETUP(testname, (ncases1) );
	
	for (i = 0; i < (ncases1); i++) {
		/*
		 * Flush output streams
		 */    
		flush_raw_log(); 
		/*
		 * Call set_fxid1()
		 */    
		retval = set_fxid1(&fxid_cparms[i]);
		/*
		 * Write formatted result to raw log.
		 */    
		switch ( retval ) {
			case PASS:      /* Passed */
			RAWPASS(fxid_cparms[i].casenum, fxid_cparms[i].casename);
			break;
			case FAIL:      /* Failed */
			RAWFAIL(fxid_cparms[i].casenum, fxid_cparms[i].casename,
				chown_cdesc[i]);
			fail = 1;
			break;
			default:
			RAWINC(fxid_cparms[i].casenum, fxid_cparms[i].casename);
			incomplete = 1;
			break;  /* Incomplete */
		}
	}
	/*
	 * Return 0, 1, or 2 to calling function, which records
	 * result in summary log.  If ANY case failed or was
	 * incomplete, the whole test is recorded as failed or
	 * incomplete.
	 */    
	if (incomplete)
		return(INCOMPLETE);
	if (fail)
		return(FAIL);
	return(PASS);
}


/* 
 * Test function.
 */
static int
set_fxid1(struct fxid_cparm1 *parmsptr)
{
	char testname[SLEN];         /* used to write to logfiles */
	char namestring[MSGLEN];     /* Used in error messages. */
	char file[NLEN + NLEN];      /* File name passed to chmod(). */
 	int fildes;                  /* File descriptor.*/
	pid_t forkval = 0;           /* pid returned to parent */ 
	pid_t retval = 0;            /* Return value of test call. */
	pid_t errval = 0;            /* Error value of test call. */
	int status;		     /* For wait. */
	cap_t cptr;		     /* Pointer to cap label */
	struct stat stat_buf;
	
	strcpy(testname,"set_fxid");
	sprintf(namestring, "%s, case %d, %s:\n   ", testname,
		parmsptr->casenum, parmsptr->casename);

	/*
	 * Create names for temp file.
	 */
	sprintf(file,  "%s%d", testname, parmsptr->casenum);

	/*
	 * Fork a child to do some testing.
	 */
	if (( forkval = fork() ) == -1  ) {
		w_error(SYSCALL, namestring, err[F_FORK], errno);
		return(INCOMPLETE);
	}		
	/* 
	 * This is the parent after forking.
	 */
	if ( forkval )  { 
		/* 
		 * This is the parent.	Wait for child and return
		 * 2 on unexpected error. Otherwise, return child's 
		 * exit code.
		 */
		if ( wait(&status) == -1 ) {
			w_error(SYSCALL, namestring, err[F_WAIT], errno);
			return(INCOMPLETE);
    		}
		if (!WIFEXITED(status)) {
			w_error(GENERAL, namestring, err[C_NOTEXIT], 0);
			return(INCOMPLETE);
		}
		if ( WEXITSTATUS(status) ) {
			w_error(GENERAL, namestring, err[C_BADEXIT], 0);
			return(INCOMPLETE);
    		}

		/* 
		 * If we got here, child exited with status 0
		 */
		cap_unlink(file);
		return(PASS);
	} /*end parent*/
	

	/* 
	 * This is the child process.
	 * Create capability label and set Subject.
	 */
	if ( ( cptr = cap_from_text(parmsptr->cflag) ) == (cap_t) NULL  ) {
		w_error(SYSCALL, namestring, err[F_CREPLABEL], errno);
		exit(INCOMPLETE);
	}	
	if ( cap_set_proc(cptr) == -1 ) {
		w_error(SYSCALL, namestring, err[F_SETPLABEL], errno);
		exit(INCOMPLETE);
	}	
	cap_free(cptr);

	/*
	 * Set uid and gid to test value and verify.
	 */
	if ( ( cap_setregid(parmsptr->Sgid, parmsptr->Sgid) ) == -1  ) {
		w_error(SYSCALL, namestring, err[SETREGID_S], errno);
		exit(INCOMPLETE);
	}	
	if ( ( getgid() != parmsptr->Sgid) ||
		(getegid() != parmsptr->Sgid) ) { 
		w_error(GENERAL, namestring, err[BADGIDS_S], 0);
		exit(INCOMPLETE);
	}
	if ( ( cap_setreuid(parmsptr->Suid, parmsptr->Suid) ) == -1  ) {
		w_error(SYSCALL, namestring, err[SETUID_S], errno);
		exit(INCOMPLETE);
	}	
	if ( ( getuid() != parmsptr->Suid) ||
		(geteuid() != parmsptr->Suid) ) { 
		w_error(GENERAL, namestring, err[BADUIDS_S], 0);
		exit(INCOMPLETE);
	}
 	/*
	 * Create a file, close it.
	 */
	cap_unlink(file);
	if ( (fildes = creat(file, (parmsptr->xflag | parmsptr->flag))) == -1 ) {
		w_error(SYSCALL, namestring, err[F_CREAT], errno);
		exit(INCOMPLETE);
	}	
	close(fildes);
	/*
	 * Here's the test: Call chown( uid, gid)
	 */
	retval = chown(file, parmsptr->Ouid, parmsptr->Ogid);
	errval = errno;
	/*
	 * Positive test cases
	 */
	if (parmsptr->expect_success == PASS ) {
		if ( retval == -1 ) {
			w_error(SYSCALL, namestring, err[TESTCALL], errval);
			exit(FAIL);
		}
		if ( retval ) {
			w_error(PRINTNUM, namestring, err[TEST_RETVAL], retval);
			exit(FAIL);
		}
		if ( stat(file, &stat_buf) < 0 ) {
			w_error(SYSCALL, namestring, err[F_STAT], errno);
			exit(INCOMPLETE);
		}
		if ( !(stat_buf.st_mode & parmsptr->xflag)) {
			w_error(GENERAL, namestring, err[BADSTATMO], 0);
			exit(FAIL);
		}
		exit(PASS);
	}
	/*
	 * For negative test cases, retval should be -1 and
	 * errno should equal parmsptr->expect_errno.
	 */
	if ( retval != -1 ) {
		w_error(PRINTNUM, namestring, err[TEST_RETVAL], retval);
		exit(FAIL);
	}
	if ( errval != parmsptr->expect_errno ) {
		w_error(SYSCALL, namestring, err[TEST_ERRNO], errval);
		exit(FAIL);
	}
	if ( stat(file, &stat_buf) < 0 ) {
		w_error(SYSCALL, namestring, err[F_STAT], errno);
		exit(INCOMPLETE);
	}
	if ( stat_buf.st_mode & parmsptr->xflag ) {
		w_error(GENERAL, namestring, err[BADSTATMO], 0);
		exit(FAIL);
	}
	exit(PASS);
	return(PASS);
}
