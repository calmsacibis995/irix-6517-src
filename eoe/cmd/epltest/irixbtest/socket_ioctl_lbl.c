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
#include "soc.h"

/*
 * socket_ioctl_lbl.c -- Tests ability of process to set new label on
 * an AF_INET socket using ioctl(descriptor, SIOCGETLABEL, &label).
 */

static int socket_ioctl_lbl1(int n, char *casename, int type, uid_t uid,
			    struct msen_l* msenptr, struct mint_l* mintptr,
			    struct msen_l* smsenptr, struct mint_l* smintptr);

/*
 * Tests whether a process can set the label on a newly-created
 * socket by calling ioctl(sd, SIOCSETLABEL, &slbl).  This should
 * succeed if and only if the effective uid is superuser.
 */
int
socket_ioctl_lbl(void)
{
    short ncases = (NTYPES * NUIDS * NMSEN * NMINT * NSMSEN * NMINT);
                                   /* Number of test cases */
    register short n = 0;          /* Test case counter */
    register short npos = 0;       /* Count of positive cases */
    register short j,k,l,m,o,p;    /* Array indices */
    int fail = 0;                  /* Set on failed test case */
    int incomplete = 0;            /* Set on incomplete test case */
    char str[MSGLEN];              /* used to write to logfiles */
    char desc[MSGLEN];             /* used to write to logfiles */
    char *casename;                /* used to write to logfiles */
    char testname[SLEN];           /* used to write to logfiles */

    strcpy(testname,"socket_ioctl_lbl");
    /*
     * Write formatted info to raw log.
     */   
    RAWLOG_SETUP(testname, ncases);
    /*
     * Call function for each test case
     */   
    for (j = 0; j < NTYPES; j++) {
	for (k = 0; k < NUIDS; k++) {
	    for (l = 0; l < NMSEN; l++) {
		for (m = 0; m < NMINT; m++) {
		    for (o = 0; o < (NSMSEN); o++) {
			for (p = 0; p < NMINT; p++) {
			    flush_raw_log(); 
			    casename =  calloc(1, (size_t)SLEN);
			    /*
			     * Superuser cases are positive
			     */
			    if (uid[k] == SUPER) {
				sprintf(casename, "pos%3.3d\0",
					npos++);
			    }
			    else {
				sprintf(casename, "neg%3.3d\0",
					(n-npos));
			    }
			    switch ( socket_ioctl_lbl1(n, casename,
						      type[j], uid[k],
						      &msenlist[l], 
						      &mintlist[m], 
						      &smsenlist[o], 
						      &mintlist[p]) ) {
				/*
				 * Write formatted result to raw log.
				 */    
				
			    case PASS:      /* Passed */
				RAWPASS(n, casename);
				break;
			    case FAIL:      /* Failed */
				sprintf(desc, 
					"%s, %s, %s, %s, %s, %s", 
					typedesc[j], uiddesc[k],
					msendesc[l], mintdesc[m],
					smsendesc[o], mintdesc[p] );
				RAWFAIL(n, casename, desc);
				fail = 1;
				break;
			    default:
				RAWINC(n, casename);
				incomplete = 1;
				break;  /* Incomplete */
			    }
			    n++;
			    flush_raw_log();
			    free(casename);
			}
		    }
		}
	    }
	}
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
 * socket_ioctl_lbl1 sets process label and uid according to parameters,
 * creates an INET socket, and attempts to reset the socket
 * label. If successful, it compares the label of the socket to the
 * socket label parameter values.  All superuser cases are positive,
 * that is, the superuser should be able to set the socket label to 
 * any legal value.  All non-superuser cases are negative; a normal 
 * user should never be able to set the label of a socket.  
 */

static int 
socket_ioctl_lbl1(int n, char *casename, int type, uid_t uid,
		 struct msen_l* msenptr, struct mint_l* mintptr,
		 struct msen_l* smsenptr, struct mint_l* smintptr)
{
    register char i = 0;          /* cat/dev loop counter */
    char testname[SLEN];          /* used to write to logfiles */
    char namestring[MSGLEN];      /* Used in error messages. */
    int retval = 0;               /* Return value of test call. */
    pid_t forkval = 0;            /* Child pid returned to parent */ 
    mac_t      plptr;             /* Pointer to MAC label of process. */
    mac_t      slptr;             /* Pointer to MAC label set. */
    mac_t      glptr;             /* Pointer to MAC label gotten. */
    int status;                   /* For wait. */   
    int sd;                       /* Socket descriptor */
    int loc_errno;                /* Local copy of test call errno */

    strcpy(testname,"socket_ioctl_lbl");
    sprintf(namestring, "%s, case %d, %s:\n   ", testname, n, casename);

    /* 
     * Fork a child and wait for it.
     */

    if ( ( forkval = fork() ) == -1 ) {
	w_error(SYSCALL, namestring, err[F_FORK], errno);
	return(INCOMPLETE);
    }

    /* 
     * This is the parent.  Wait for child and return
     * 2 on unexpected error. Otherwise, return child's 
     * exit code.
     */
    if ( forkval ) {
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
     * This is the child.  Calloc label,
     * set process label and free pointer.
     */
    plptr = mac_get_proc();
    plptr->ml_msen_type = msenptr->msen;
    plptr->ml_level = msenptr->level;
    plptr->ml_catcount = msenptr->catcount;
    for ( i = 0; i < msenptr->catcount; i++ ) { 
	plptr->ml_list[i] = msenptr->catlist[i];
    }
    plptr->ml_mint_type = mintptr->mint;
    plptr->ml_grade = mintptr->grade;
    plptr->ml_divcount = mintptr->divcount;
    for ( i = 0; i < mintptr->divcount; i++ ) {
	plptr->ml_list[i + msenptr->catcount] =
	    mintptr->divlist[i];
    }
    if ( cap_setplabel(plptr) ) {
	w_error(SYSCALL,namestring,err[F_SETPLABEL],errno);
	exit(INCOMPLETE);
    }
    mac_free(plptr);
    /*
     * Set effective uid and verify it.
     */
    if ( ( cap_setreuid(SUPER, uid) ) < 0 ) {
	w_error(SYSCALL, namestring, err[SETREUID_S], errno);
	exit(INCOMPLETE);
    }
    if ( geteuid() != uid ) {
	w_error(GENERAL, namestring, err[BADUIDS_S], 0);
	exit(INCOMPLETE);
    }	

    /*
     * Create a socket.
     */    
    if ( ( sd = socket(AF_INET , type, 0) ) < 0 ) {
	w_error(SYSCALL, namestring, err[F_SOCKET], errno);
	exit(INCOMPLETE);
    }

    /*
     * Prepare socket label.
     */    

    slptr = mac_get_proc();
    slptr->ml_msen_type = smsenptr->msen;
    slptr->ml_level = smsenptr->level;
    slptr->ml_catcount = smsenptr->catcount;
    for ( i = 0; i < smsenptr->catcount; i++ ) { 
	slptr->ml_list[i] = smsenptr->catlist[i];
    }
    slptr->ml_mint_type = smintptr->mint;
    slptr->ml_grade = smintptr->grade;
    slptr->ml_divcount = smintptr->divcount;
    for ( i = 0; i < smintptr->divcount; i++ ) {
	slptr->ml_list[i + smsenptr->catcount] =
	    smintptr->divlist[i];
    }

    /*
     * Test call: Call ioctl(sd, SIOCSETLABEL, slptr).
     */    
    retval = ( ioctl(sd, SIOCSETLABEL, slptr) );
    loc_errno = errno;
    mac_free(slptr);

    /*
     * Set effective uid back to SUPER and verify it.
     */
    if ( ( cap_setreuid(-1, SUPER) ) < 0 ) {
	w_error(SYSCALL, namestring, err[SETREUID_S], errno);
	exit(INCOMPLETE);
    }
    if ( geteuid() != SUPER ) {
	w_error(GENERAL, namestring, err[BADUIDS_S], 0);
	exit(INCOMPLETE);
    }	

    /*
     * Set process label.
     */
    plptr = mac_get_proc();
    plptr->ml_msen_type = MSH;
    plptr->ml_mint_type = MIE;
    if ( cap_setplabel(plptr) ) {
	w_error(SYSCALL,namestring,err[F_SETPLABEL],errno);
	exit(INCOMPLETE);
    }
    mac_free(plptr);

    /*
     * Get current label on socket.
     */
    glptr = mac_get_proc();
    if ( ioctl(sd, SIOCGETLABEL, glptr) < 0 ) {
	w_error(SYSCALL, namestring, err[F_IOCTLGETLBL], errno);
	close(sd);
	exit(INCOMPLETE);
    }
    close(sd); 

    /*
     * For positive cases (all superuser cases)
     * retval should be 0 and socket label retreived
     * by ioctl should match socket label parameters.
     */
    if ( (uid == SUPER) && (smsenptr->msen != MSM) ) {
	if ( retval < 0 ) {
	    w_error(SYSCALL, namestring, err[TESTCALL], loc_errno);
	    exit(FAIL);
	} 
	if ( retval > 0 ) {
	    w_error(PRINTNUM, namestring, err[TEST_RETVAL],retval);
	    exit(FAIL);
	}
	if ( ( glptr->ml_msen_type != smsenptr->msen ) ||
	    ( glptr->ml_mint_type != smintptr->mint ) ||
	    ( glptr->ml_level != smsenptr->level ) ||
	    ( glptr->ml_grade != smintptr->grade ) ||
	    ( glptr->ml_catcount != smsenptr->catcount ) ||
	    ( glptr->ml_divcount != smintptr->divcount ) ) {
	    w_error(GENERAL, namestring, err[BADLABEL], 0);
	    exit(FAIL);
	}
	for ( i = 0; i < smsenptr->catcount; i++ ) {
	    if ( glptr->ml_list[i] != smsenptr->catlist[i] ) {
		w_error(GENERAL, namestring, err[BADLABEL], 0);
		exit(FAIL);
	    }
	}
	for ( i = 0; i < smintptr->divcount; i++ ) {
	    if ( glptr->ml_list[i + smsenptr->catcount] != 
	    	smintptr->divlist[i] ) {
		w_error(GENERAL, namestring, err[BADLABEL], 0);
		exit(FAIL);
	    }
	}
	exit(PASS);
    }

    /*
     * For negative test cases (all non-superuser cases), 
     * retval should be -1 and errno should equal EPERM.  If
     * not, write error message. Exit 0 for PASS, 1 for FAIL.	 
     */
    if ( retval != -1 ) {
	w_error(PRINTNUM, namestring, err[TEST_RETVAL],retval);
	exit(FAIL);
    }
    if ( loc_errno != EPERM ) {
	w_error(SYSCALL, namestring, err[TEST_ERRNO],loc_errno);
	exit(FAIL);
    }
    exit(PASS);
    return(PASS);
}
