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
 * socket_lbl.c -- Tests label set on socket at creation.
 */

static int socket_lbl1(int n, int domain, int type, uid_t uid,
			    struct msen_l* msenptr, struct mint_l* mintptr);

int
socket_lbl(void)
{
    short ncases = (NDOMAINS * NTYPES * NUIDS * NMSEN * NMINT);
                                /* Number of test cases. Note: Use */
				/* NMESEN-1 because MSEN_EQUAL */
                                /* is not valid for processes */

    register short n = 0;       /* Test case counter */
    register short i,j,k,l,m;   /* Array indices */
    int fail = 0;               /* set when a test case fails */
    int incomplete = 0;         /* set when a test case is incomplete */
    char str[MSGLEN];           /* used to write to logfiles */
    char desc[MSGLEN];          /* used to write to logfiles */
    char *casename;             /* used to write to logfiles */
    char testname[SLEN];        /* used to write to logfiles */

    strcpy(testname,"socket_lbl");
    /*
     * Write formatted info to raw log.
     */   
    RAWLOG_SETUP(testname, ncases);
    /*
     * Call function for each test case
     */   
    for (i = 0; i < NDOMAINS; i++) {
	for (j = 0; j < NTYPES; j++) {
	    for (k = 0; k < NUIDS; k++) {
		for (l = 0; l < NMSEN; l++) {
		    for (m = 0; m < NMINT; m++) {
			flush_raw_log(); 
			casename =  calloc(1, (size_t)SLEN);
			sprintf(casename, "pos%3.3d\0", n);
			switch ( socket_lbl1(n, domain[i],
						  type[j], uid[k],
						  &msenlist[l],
						  &mintlist[m]) ) {
			    /*
			     * Write formatted result to raw log.
			     */    
			    
			case PASS:      /* Passed */
			    RAWPASS(n, casename);
			    break;
			case FAIL:      /* Failed */
			    sprintf(desc, "%s, %s, %s, %s, %s",
				    domaindesc[i], typedesc[j],
				    uiddesc[k], msendesc[l], mintdesc[m]);
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
 * socket_lbl1 sets process label and uid according to
 * parameters, creates a socket, and compares the label of
 * the socket to the parameter values.  All cases are
 * positive, that is, the label of the socket should always
 * equal the label of the process that created it.
 */

static int 
socket_lbl1(int n, int domain, int type, uid_t uid,
		 struct msen_l* msenptr, struct mint_l* mintptr)
{
    register char i = 0;          /* cat/div loop counter */
    char testname[SLEN];          /* used to write to logfiles */
    char namestring[MSGLEN];      /* Used in error messages. */
    pid_t forkval = 0;            /* Child pid returned to parent */ 
    mac_t plptr;                  /* Pointer to MAC label of process. */
    mac_t slptr;                  /* Pointer to MAC label of socket. */
    int status;                   /* For wait. */   
    int sd;                       /* Socket descriptor */

    strcpy(testname,"socket_lbl");
    sprintf(namestring, "%s, case %d, pos%d:\n   ", testname, n, n);

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
    if (forkval) {
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
     * Set process label.
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

    /*
     * Set uid and verify it.
     */
    if (  ( cap_setuid(uid) ) < 0  ) {
	w_error(SYSCALL, namestring, err[SETUID_S], errno);
	exit(INCOMPLETE);
    }
    if ( getuid() != uid ) {
	w_error(GENERAL, namestring, err[BADUIDS_S], 0);
	exit(INCOMPLETE);
    }	

    /*
     * Create a socket.
     */    
    if ( ( sd = socket(domain , type, 0) ) < 0 ) {
	w_error(SYSCALL, namestring, err[F_SOCKET], errno);
	exit(INCOMPLETE);
    }

    /*
     * Get socket label.
     */    
    slptr = mac_get_proc();
    if ( ioctl(sd, SIOCGETLABEL, slptr) < 0 ) {
	w_error(SYSCALL, namestring, err[F_IOCTLGETLBL], errno);
	close(sd);
	exit(INCOMPLETE);
    }

    /*
     * Clean up and test whether socket label matches label set
     * on process.  All cases are positive, that is, all test
     * cases should succeed.
     */
    close(sd); 

    if ( ( slptr->ml_msen_type != msenptr->msen ) ||
	( slptr->ml_mint_type != mintptr->mint ) ||
	( slptr->ml_level != msenptr->level ) ||
	( slptr->ml_grade != mintptr->grade ) ||
	( slptr->ml_catcount != msenptr->catcount ) ||
	( slptr->ml_divcount != mintptr->divcount ) ) {
	w_error(GENERAL, namestring, err[BADLABEL], 0);
	exit(FAIL);
    }
    for ( i = 0; i < msenptr->catcount; i++ ) {
	if ( slptr->ml_list[i] != msenptr->catlist[i] ) {
	    w_error(GENERAL, namestring, err[BADLABEL], 0);
	    exit(FAIL);
	}
    }
    for ( i = 0; i < mintptr->divcount; i++ ) {
	if ( slptr->ml_list[i + msenptr->catcount] != mintptr->divlist[i] ) {
	    w_error(GENERAL, namestring, err[BADLABEL], 0);
	    exit(FAIL);
	}
    }
    exit(PASS);
    return(PASS);
}
