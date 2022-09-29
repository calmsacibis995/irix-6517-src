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
#include "dac.h"
#include "mac.h"

/*
 * setpgid_dac.c -- DAC tests for setpgid() system call.
 */
static int setpgid_dac1(struct dacparms_4 *parmsptr);
static int setpgid_dac2(struct dacparms_5 *parmsptr);

/* 
 * Description strings, corresponding with test cases, 
 * to be printed to the raw log if that case fails.
 */
static char *setpgid_ddesc[] = 
{ "O is child, S & O are suser",
  "O is child, S & O uids differ",
  "O is child, S & O uids differ, pgid is not object's",
  "O not child, S & O are suser",
  "O not child, S & O uids differ"
};

/*
 * Each structure contains information for one test case.
 * Variables are uids of Subject and Object processes,
 * whether pgid argument is that of object, another process
 * in the same session (the parent) or a process in a 
 * different session.
 */
static struct dacparms_4
setpgid_dparms1[] = 
{
/*                                       (flag1) (flag2)
 * name    #  c_flag  Oruid  Oeuid  Sruid  Seuid  ppid?  badpid?  success?  errno
 */
  "pos00", 0,     0,  SUPER, SUPER, SUPER, SUPER, FAIL, FAIL,    PASS ,      0,
  "pos01", 1,     0,  TEST0, TEST0, TEST1, TEST1, FAIL, FAIL,    PASS ,      0,
  "neg00", 3,     0,  TEST0, TEST0, TEST1, TEST1, FAIL, PASS,    FAIL,   EPERM
};

static struct dacparms_5
setpgid_dparms2[] = 
{
/*
 * name    #  Oruid  Oeuid  Sruid  Seuid  c_flag  success?  errno
 */
  "neg01", 4, SUPER, SUPER, SUPER, SUPER,  0,     FAIL,   ESRCH,
  "neg02", 5, TEST0, TEST0, TEST1, TEST1,  0,     FAIL,   ESRCH
};

int
setpgid_dac(void)
{
    char str[MSGLEN];        /* used to write to logfiles */
    char testname[SLEN];     /* used to write to logfiles */
    char *name;              /* holds casename from struct */
    short num;               /* holds casenum from struct */
    short ncases1 = 3;       /* number of test cases with child object */
    short ncases2 = 2;       /* number of test cases with non-child object */
    short i = 0;             /* test case loop counter */
    int fail = 0;            /* set when a test case fails */
    int incomplete = 0;      /* set when a test case is incomplete */
    int retval;              /* return value of test function */
    strcpy(testname,"setpgid_dac");
    /*
     * Write formatted info to raw log.
     */    
    RAWLOG_SETUP(testname, (ncases1 + ncases2) );
    for (i = 0; i < (ncases1+ncases2); i++) {
	/*
	 * Flush output streams
	 */    
	flush_raw_log(); 
	/*
	 * Call function setpgid_dac1() for each test case in 
	 * which object is a child of subject.
	 */    
	if (i < ncases1) {
	    retval = setpgid_dac1(&setpgid_dparms1[i]);
	    num = setpgid_dparms1[i].casenum;
	    name = setpgid_dparms1[i].casename;
	}
	/*
	 * Call function setpgid_dac2() for each test case in 
	 * which object is not a child of subject.
	 */    
	if (i >= ncases1) {
	    retval = setpgid_dac2(&setpgid_dparms2[i - ncases1]);
	    num = setpgid_dparms2[i - ncases1].casenum;
	    name =  setpgid_dparms2[i - ncases1].casename;
	}
	/*
	 * Write formatted result to raw log.
	 */    
	switch ( retval ) {
	case PASS:      /* Passed */
	    RAWPASS( num, name );
	    break;
	case FAIL:      /* Failed */
	    RAWFAIL(  num, name, setpgid_ddesc[i] );
	    fail = 1;
	    break;
	default:
	    RAWINC( num, name );
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
 * Test function for cases where Object is Child of Subject.
 */
static int
setpgid_dac1(struct dacparms_4 *parmsptr)
{
    char buf[4];                 /* Used in pipe read/writes. */
    char testname[SLEN];         /* used to write to logfiles */
    char namestring[MSGLEN];     /* Used in error messages. */
    int S_O_fd[2];               /* Subject writes, object reads. */
    int O_S_fd[2];               /* Subject writes, object reads. */
    pid_t Ppid = 0;              /* Parent's pid. */ 
    pid_t Spid = 0;              /* Subject's pid returned to parent. */ 
    pid_t Opid = 0;              /* Object's pid returned to subject */ 
    pid_t pgrp;                  /* Pid used as pgrp argument. */
    pid_t retval = 0;            /* Return value of test call. */
    int loc_errno;               /* Save errno on test call. */
    int status;                  /* For wait. */   

    strcpy(testname,"setpgid_dac");
    strcpy (buf, "abc");      /* Stuff to write to pipe. */
    sprintf(namestring, "%s, case %d, %s:\n   ", testname,
	     parmsptr->casenum, parmsptr->casename);

    /*
     * Make 2 pipes.
     */
    if ( pipe(S_O_fd) || pipe(O_S_fd) )  {
	w_error(SYSCALL, namestring, err[F_PIPE], errno);
	return(INCOMPLETE);
    }

    /*
     * Get Parent pid, in case we need it for this case.
     */
    Ppid = getpid();

    /*
     * Fork Subject.
     */
    if ( ( Spid = fork() ) == -1 ) {
	w_error(SYSCALL, namestring, err[F_FORK], errno);
	return(INCOMPLETE);
    }

    /* 
     * This is the parent after forking Subject.
     */
    if ( Spid ) { 
	/* 
	 * Close all pipe fds.
	 */
	close(S_O_fd[0]);
	close(S_O_fd[1]);
	close(O_S_fd[0]);
	close(O_S_fd[1]);
	/*
	 * Wait for child (Subject).
	 */
	if ( wait(&status) == -1 ) {
	    w_error(SYSCALL, namestring, err[F_WAIT], errno);
	    return(INCOMPLETE);
    	}
	if ( !WIFEXITED(status) ) {
	    w_error(GENERAL, namestring, err[C_NOTEXIT], 0);
	    return(INCOMPLETE);
    	}
	return(  WEXITSTATUS(status)  );
    }

    /*
     * This is the Subject before forking the Object.
     */
    /*
     * Fork Object.
     */
    if ( ( Opid = fork() ) == -1 ) {
	w_error(SYSCALL, namestring, err[F_FORK], errno);
	exit(INCOMPLETE);
    }
    
    /*
     * This is the Subject after forking the Object.
     */    
    if ( ( !Spid ) && ( Opid ) ) {
	close(S_O_fd[0]);
	close(O_S_fd[1]);
	/*
	 * Set Subject ruid and euid to test values and verify.
	 */
	if ( ( cap_setreuid(parmsptr->Sruid, parmsptr->Seuid) ) == -1 ) {
	    w_error(SYSCALL, namestring, err[SETREUID_S], errno);
	    exit(INCOMPLETE);
	}	
	if ( (getuid() != parmsptr->Sruid)||(geteuid() != parmsptr->Seuid) ) {
	    w_error(GENERAL, namestring, err[BADUIDS_S], 0);
	    exit(INCOMPLETE);
	}	
	/*
	 * Read what Object wrote to the pipe.
	 */
	if ( ( read(O_S_fd[0], buf, sizeof(buf) ) ) != sizeof(buf) ) {
	    w_error(SYSCALL, namestring, err[PIPEREAD_S], errno);
	    exit(INCOMPLETE);
	}
	/*
	 * Select appropriate value for test call pgrp arg.
	 */
	if ( parmsptr->flag1 == PASS  ) {
	    /*
	     * Use a pid that is not Object's, but that
	     * is in same session: Parent's pid.
	     */
	    pgrp = Ppid;

	}
	else {
	    if ( parmsptr->flag2 == PASS  ) {
	    /*
	     * Use a pid that exists, but that is not
	     * in the same session: init's pid.
	     */
		pgrp = 1;
	    }
	    else
		pgrp = Opid;
	}

	/*
	 * Here's the test call.  Call setpgid with Object's pid
	 * as arg1 and appropriate pgrp as arg2.  Save errno 
	 * because it could be reset by wait call, below.
	 */
	retval = setpgid(Opid, pgrp);
	loc_errno = errno;
	 /*
	  * Write to the pipe so Object can go away.
	  */
	if ( ( write(S_O_fd[1], buf, sizeof(buf) ) )
	    != (sizeof(buf)) ) {
	    w_error(SYSCALL, namestring, err[PIPEWRITE_S], errno);
	    exit(INCOMPLETE);
	}
	/*
	 * Wait for child (Object) to be sure it exits 
	 * normally.  If not, exit with value of 2.
	 */
	if ( wait(&status) == -1 ) {
	    w_error(SYSCALL, namestring, err[F_WAIT], loc_errno);
	    exit(INCOMPLETE);
    	}
	if ( !WIFEXITED(status) ) {
	    w_error(GENERAL, namestring, err[C_NOTEXIT], 0);
	    exit(INCOMPLETE);
    	}
	if ( WEXITSTATUS(status) ) {
	    w_error(GENERAL, namestring, err[C_BADEXIT], 0);
	    exit(INCOMPLETE);
    	}

	/*
	 * For positive cases, retval should be zero.
	 */
	if ( parmsptr->expect_success == PASS  ) {
	    if ( retval == -1 ) {
		w_error(SYSCALL, namestring, err[TESTCALL], loc_errno);
		exit(FAIL);
	    }
	    if ( retval ) {
		w_error(PRINTNUM, namestring, err[TEST_RETVAL],retval);
		exit(FAIL);
	    }
	    exit(PASS);
	}

	/*
	 * For negative test cases, retval should be -1 and
	 * errno should equal parmsptr->expect_errno.
	 */
	if ( retval != -1 ) {
	    w_error(PRINTNUM, namestring, err[TEST_RETVAL],retval);
	    exit(FAIL);
	}
	if ( errno != parmsptr->expect_errno ) {
	    w_error(SYSCALL, namestring, err[TEST_ERRNO], loc_errno);
	    exit(FAIL);
	}
	exit(PASS);
    }

    /*
     * This is the Object.
     */    
    close(S_O_fd[1]);
    close(O_S_fd[0]);
    /*
     * Set Object ruid and euid to test values and verify.
     */
    if ( ( cap_setreuid(parmsptr->ruid, parmsptr->euid) ) == -1 ) {
	w_error(SYSCALL, namestring, err[SETREUID_O], errno);
	exit(INCOMPLETE);
    }	
    if ( ( getuid() != parmsptr->ruid ) || ( geteuid() != parmsptr->euid ) ) {
	w_error(GENERAL, namestring, err[BADUIDS_O], errno);
	exit(INCOMPLETE);
    }	
    /*
     * Write to the pipe so Subject can proceed.
     */
    if ( ( write(O_S_fd[1], buf, sizeof(buf) ) ) != (sizeof(buf)) ) {
	w_error(SYSCALL, namestring, err[PIPEWRITE_S], errno);
	exit(INCOMPLETE);
    }
    
    /*
     * Read what Subject wrote to the pipe, then exit.
     */
    if ( ( read(S_O_fd[0], buf, sizeof(buf) ) ) != sizeof(buf) ) {
	w_error(SYSCALL, namestring, err[PIPEREAD_O], errno);
	exit(INCOMPLETE);
    }
    exit(PASS);
    return(PASS);
}

/* 
 * Test function for cases where Object is NOT Child of Subject.
 */
static int
setpgid_dac2(struct dacparms_5 *parmsptr)
{
    char buf[4];                 /* Used in pipe read/writes. */
    char testname[SLEN];         /* used to write to logfiles */
    char namestring[MSGLEN];     /* Used in error messages. */
    int OtoS_fd[2];              /* O writes, S reads. */
    int StoO_fd[2];              /* S writes, O reads. */
    pid_t forkval1 = 0;          /* O's pid returned to parent */ 
    pid_t forkval2 = 0;          /* S's pid returned to parent */ 
    pid_t Spid = 0;              /* S's pid returned by getpid in S */ 
    pid_t retval = 0;            /* Return value of test call. */
    int status0;                 /* For wait. */   
    int exitstatus0;             /* For wait. */   
    int status1;                 /* For wait. */   
    int exitstatus1;             /* For wait. */   
    

    strcpy(testname,"setpgid_dac");
    strcpy (buf, "abc");      /* Stuff to write to pipe. */
    sprintf(namestring, "%s, case %d, %s:\n   ", testname,
	     parmsptr->casenum, parmsptr->casename);
    /*
     * Make 2 pipes.
     */
    if ( pipe(OtoS_fd) || pipe(StoO_fd) )  {
	w_error(SYSCALL, namestring, err[F_PIPE], errno);
	return(INCOMPLETE);
    }
    /*
     * Fork O.
     */
    if ( ( forkval1 = fork() ) == -1 ) {
	w_error(SYSCALL, namestring, err[F_FORK], errno);
	return(INCOMPLETE);
    }

    /* 
     * This is the parent after forking O.
     */
    if ( forkval1 )  { 
	/*
	 * Fork S.
	 */
	if ( ( forkval2 = fork() ) == -1 ) {
	    w_error(SYSCALL, namestring, err[F_FORK], errno);
	    return(INCOMPLETE);
	}
    }

    /* 
     * This is the parent after forking S.
     */
    if ( ( forkval2 ) && ( forkval1 ) )  { 
	/* 
	 * Close all pipe fds.
	 */
	close(OtoS_fd[0]);
	close(OtoS_fd[1]);
	close(StoO_fd[0]);
	close(StoO_fd[1]);
 

	/*
	 * Wait for Children. Return 2 if either child
	 * has been killed or returns 2 or any other
	 * unexpected event occurs.  Otherwise, return 1 if one
	 * child returns 1, otherwise 0.
	 */
	if ( ( wait(&status0) == -1 ) || ( wait(&status1) == -1 ) ) {
	    w_error(SYSCALL, namestring, err[F_WAIT], errno);
	    wait(&status1);
	    return(INCOMPLETE);
    	}
	if ( ( !WIFEXITED(status0) ) || ( !WIFEXITED(status1) ) ) {
	    w_error(GENERAL, namestring, err[C_NOTEXIT], 0);
	    return(INCOMPLETE);
	}
	if ( ( ( exitstatus0 = WEXITSTATUS(status0) ) == 2) ||
	     ( ( exitstatus1 = WEXITSTATUS(status1) ) == 2) ) {
	    return(INCOMPLETE);
	}
	if ( ( exitstatus0 == 1 ) || ( exitstatus1 == 1 ) ) 
	    return(FAIL);
	if ( ( exitstatus0 == 0 ) || ( exitstatus1 == 0 ) ) 
	    return(PASS);
    }


   /* 
     * This is S, the subject process. 
     */
    if ( ( forkval1 ) && ( !forkval2 ) ) {  
	/*
	 * Close pipe descriptors that S does not use.
	 */
	close(OtoS_fd[1]);
	close(StoO_fd[0]);
	/*
	 * Set ruid and euid to test value and verify.
	 */
	if ( ( cap_setreuid(parmsptr->Sruid, parmsptr->Seuid) ) == -1 ) {
	    w_error(SYSCALL, namestring, err[SETREUID_S], errno);
	    exit(INCOMPLETE);
	}	
	if ( (getuid() != parmsptr->Sruid)||(geteuid() != parmsptr->Seuid) ) {
	    w_error(GENERAL, namestring, err[BADUIDS_S], 0);
	    exit(INCOMPLETE);
	}	
	/*
	 * Read what O wrote to the pipe.
	 */
	if ( ( read(OtoS_fd[0], buf, sizeof(buf))) != sizeof(buf) ) {
	    w_error(SYSCALL, namestring, err[PIPEREAD_S], errno);
	    exit(INCOMPLETE);
	}

	Spid = getpid();
	/*
	 * Here's the test: Call setpgid with O's pid
	 * as arg1 and S's pid as arg2.
	 */
	retval = setpgid(forkval1,Spid);


	 /*
	  * Write to the pipe so O can go away.
	  */
	if ( ( write(StoO_fd[1], buf, sizeof(buf)) ) != ( sizeof(buf) ) ) {
	    w_error(SYSCALL, namestring, err[PIPEWRITE_S], errno);
	    exit(INCOMPLETE);
	}

	/*
	 * For positive cases, retval should be zero.
	 */
	if ( parmsptr->expect_success == PASS  ) {
	    if ( retval== -1 ) {
		w_error(SYSCALL, namestring, err[TESTCALL], errno);
		exit(FAIL);
	    }
	    if ( retval ) {
		w_error(PRINTNUM, namestring, err[TEST_RETVAL],retval);
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
	if ( errno != parmsptr->expect_errno ) {
	    w_error(SYSCALL, namestring, err[TEST_ERRNO], errno);
	    exit(FAIL);
	}
	exit(PASS);		
    }

    /* 
     * This is O, the object process. 
     */
    /*
     * Begin O code. Close pipe descriptors that O does not use.
     */
    close(OtoS_fd[0]);
    close(StoO_fd[1]);
    
    /*
     * Set O's ruid and euid to test value and verify.
     */
    if ( ( cap_setreuid(parmsptr->ruid, parmsptr->euid) ) == -1 ) {
	w_error(SYSCALL, namestring, err[SETREUID_O], errno);
	exit(INCOMPLETE);
    }
    
    if ( ( getuid() != parmsptr->ruid ) || ( geteuid() != parmsptr->euid ) ) {
	w_error(GENERAL, namestring, err[BADUIDS_O], errno);
	exit(INCOMPLETE);
    }
    
    /*
     * Write to the pipe so S can proceed.
     */
    if ( ( write(OtoS_fd[1], buf, sizeof(buf) ) ) != ( sizeof(buf) ) ) {
	w_error(SYSCALL, namestring, err[PIPEWRITE_O], errno);
	exit(INCOMPLETE);
    }
    /*
     * Stay alive until S writes to the pipe.
     */
    if ( ( read(StoO_fd[0], buf, sizeof(buf))) != sizeof(buf) ) {
	w_error(SYSCALL, namestring, err[PIPEREAD_O], errno);
	exit(INCOMPLETE);
    }
    exit(PASS);
    return(PASS);
    /*
     * End O code.
     */
}
