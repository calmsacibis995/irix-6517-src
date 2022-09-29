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
 * Timeout values
 */
#define TV_SEC  2
#define TV_USEC 0

/*
 * transport_mac.c -- Sets a specific MONO, BSO, CIPSO2, or 
 * SGIPSO2 label on the loopback interface, then attempts to send
 * a message between sockets at a specific process label.  The 
 * message should be received if and only if (1) the socket label 
 * is within the range of the interface label and (2) the number 
 * and value of the process label's categories and divisions can  
 * be represented in socket label.
 * The iflabel command lines and process labels are defined in
 * soc.h.
 */

static char *smsg = "Hello";           /* Message sent by client */

static int transport_mac1(int n, char *casename, struct so_minfo *parmsptr);

int
transport_mac(void)
{
    short ncases = 48;        /* Number of test cases */
    int fail = 0;            /* set when a test case fails */
    int incomplete = 0;      /* set when a test case is incomplete */
    short i = 0;             /* test case loop counter */
    short npos = 0;          /* Count of positive cases */
    char casename[SLEN];     /* used to write to logfiles */
    char str[MSGLEN];        /* used to write to logfiles */
    char namestring[MSGLEN]; /* Used in error messages. */
    char testname[SLEN];     /* used to write to logfiles */
    
    strcpy(testname,"transport_mac");
    /*
     * Write formatted info to raw log.
     */   
    RAWLOG_SETUP(testname, ncases);

    /*
     * Call function for each test case
     */   
    for (i = 0; i < ncases; i++) {
	/*
	 * Flush output streams
	 */    
	flush_raw_log(); 
	/*
	 * Look at the expect_success field in the
	 * structure to determine whether this case
	 * is positive or negative.  Construct
	 * the case name for this case.
	 */
	if(so_trans_minfo[i].expect_success == PASS )
	    sprintf(casename, "pos%3.3d\0", npos++ );
	else
	    sprintf(casename, "neg%3.3d\0",(i - npos) );
	/*
	 * Pass the case number i, casename, and the address
	 * of the ith structure to the test function.  Write
	 * formatted output to the raw log based on the 
	 * return value.
	 */
	switch ( transport_mac1(i, casename, &so_trans_minfo[i]) ) {
	    /*
	     * Write formatted result to raw log.
	     */    
	case PASS:      /* Passed */
	    RAWPASS(i, casename);
	    break;
	case FAIL:      /* Failed */
	    RAWFAIL(i, casename, so_trans_mdesc[i]);
	    fail = 1;
	    break;
	default:
	    RAWINC(i, casename);
	    incomplete = 1;
	    break;  /* Incomplete */
	}
	flush_raw_log();
    }
    /*
     * Restore lo0 interface to SGIPSO2 with full label range.
     */
    if (system("iflabel lo0 SGIPSO2 msenhigh/mintlow msenlow/minthigh 3")) {
	w_error(GENERAL, namestring, err[F_SYSIFLABEL], 0);
	return(INCOMPLETE);
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
transport_mac1(int n, char *casename, struct so_minfo *parmsptr)
{
    register char i = 0;                /* cat/div loop counter */
    char testname[SLEN];                /* used to write to logfiles */
    char namestring[MSGLEN];            /* Used in error messages. */
    int retval = 0;                     /* Return value of select call. */
    pid_t forkval1 = 0;                 /* Child (server) pid  */ 
    pid_t forkval2 = 0;                 /* Grandchild (client) pid */ 
    mac_t lptr;                         /* Pointer to MAC label. */
    int status;                         /* For wait. */   
    struct sockaddr_in sad, sad2;       /* Socket addresses */
    int sd, sd2;                        /* Socket descriptors */
    int sadsize;                        /* Address sizes */
    fd_set r_set;                       /* For select */
    struct timeval timeout;             /* For select */
    char pipebuf[4];                    /* Used in pipe read/writes. */
    int pipefd1[2];                     /* For synchronization. */
    int pipefd2[2];                     /* For synchronization. */

    strcpy(testname,"transport_mac");
    strcpy (pipebuf, "abc");      /* Stuff to write to pipe. */
    sprintf(namestring, "%s, case %d, %s:\n   ", testname, n, casename);
    /*
     * Set lo0 interface
     */
    if ( system(iflabelcmd[parmsptr->cmdindex]) ) {
	w_error(GENERAL, namestring, err[F_SYSIFLABEL], 0);
	return(INCOMPLETE);
    }
    /* 
     * Fork a child and wait for it.
     */
    if ( ( forkval1 = fork() ) < 0 ) {
	w_error(SYSCALL, namestring, err[F_FORK], errno);
	return(INCOMPLETE);
    }
    
    /* 
     * This is the parent.  Wait for child and return
     * 2 on unexpected error. Otherwise, return child's 
     * exit code.
     */
    if ( forkval1 ) {
	if ( wait(&status) < 0 ) {
	    w_error(SYSCALL, namestring, err[F_WAIT], errno);
	    return(INCOMPLETE);
    	}
	if ( !WIFEXITED(status) ) {
	    w_error(GENERAL, namestring, err[C_NOTEXIT], 0);
	    return(INCOMPLETE);
    	}
	return(WEXITSTATUS(status));
    }
        
    /* 
     * This is the server process.  
     * Make 2 pipes.
     */
    if ( pipe(pipefd1) || pipe(pipefd2) )  {
	w_error(SYSCALL, namestring, err[F_PIPE], errno);
	return(INCOMPLETE);
    }

    /* 
     * Set process label.
     */    
    lptr = mac_get_proc();
    lptr->ml_msen_type = msen_tlist[parmsptr->msenindex].msen;
    lptr->ml_level = msen_tlist[parmsptr->msenindex].level;
    lptr->ml_catcount = msen_tlist[parmsptr->msenindex].catcount;
    for (i = 0; i < msen_tlist[parmsptr->msenindex].catcount; i++) { 
	lptr->ml_list[i] = msen_tlist[parmsptr->msenindex].catlist[i];
    }
    lptr->ml_mint_type = mint_tlist[parmsptr->mintindex].mint;
    lptr->ml_grade = mint_tlist[parmsptr->mintindex].grade;
    lptr->ml_divcount = mint_tlist[parmsptr->mintindex].divcount;
    for (i = 0; i < mint_tlist[parmsptr->mintindex].divcount; i++) {
	lptr->ml_list[i + msen_tlist[parmsptr->msenindex].catcount] =
	    mint_tlist[parmsptr->mintindex].divlist[i];
    }
    if ( cap_setplabel(lptr) ) {
	w_error(SYSCALL,namestring,err[F_SETPLABEL],errno);
	exit(INCOMPLETE);
    }
    mac_free(lptr);
    /*
     * Create a server socket.
     */
    if ( ( sd = socket(AF_INET, parmsptr->type, 0)) < 0 ) {
	w_error(SYSCALL, namestring, err[F_SOCKET], errno);
	exit(INCOMPLETE);
    }
    
    /*
     * Initialize socket address appropriately 
     * for AF_INET and bind socket.
     */
    memset((void *) &sad, '\0', sizeof(sad));
    sad.sin_family = AF_INET;
    sad.sin_port = 0;
    sad.sin_addr.s_addr = INADDR_ANY;
    
    if ( bind(sd, &sad, sizeof sad) < 0 ) {
	w_error(SYSCALL, namestring, err[F_BIND], errno);
	exit(INCOMPLETE);
    }
    
    /*
     * Get port so client process can use it.
     */
    sadsize = sizeof(sad);
    if ( getsockname(sd, &sad, &sadsize) ) {
	w_error(SYSCALL, namestring, err[F_GETSOCKNAME], errno);
	exit(INCOMPLETE);
    }
    
    /*
     * Now that we know the port, fork the client process.
     */
    if ( ( forkval2 = fork() ) < 0 ) {
	w_error(SYSCALL, namestring, err[F_FORK], errno);
	exit(INCOMPLETE);
    }
    if ( forkval2 ) {
	/*
	 * This is still the server. Close pipe1's read fd and
	 * pipe2's write fd.
	 */
	close(pipefd1[0]);
	close(pipefd2[1]);
	/* 
	 * If type is stream, call listen.
	 */
  	if ( parmsptr->type == SOCK_STREAM ) {
	    if ( listen(sd, 1) < 0) {
		w_error(SYSCALL, namestring, err[F_LISTEN], errno);
		exit(INCOMPLETE);
	    }
	}
	
	/*
	 * Read from pipe to ensure client is ready.
	 */
	if ((read(pipefd2[0], pipebuf, sizeof(pipebuf))) != sizeof(pipebuf) ) {
	    w_error(SYSCALL, namestring, err[PIPEREAD_O], errno);
	    exit(INCOMPLETE);
	}

	/*
	 * Write to pipe so client knows to connect/send.
	 */
	if ( ( write(pipefd1[1], pipebuf, sizeof(pipebuf)) ) 
		!= (sizeof(pipebuf)) ) {
	    w_error(SYSCALL, namestring, err[PIPEWRITE_S], errno);
	    exit(INCOMPLETE);
	}

	/*
	 * Call select to determine whether test case
	 * passes or fails.  Negative cases should time
	 * out.  Positive cases should detect 1 fd to read.
         */
	timeout.tv_sec = TV_SEC;
	timeout.tv_usec = TV_USEC;
	FD_ZERO(&r_set);
	FD_SET(sd, &r_set);
	if ( (retval = select(sd+1, &r_set, (fd_set *)0, (fd_set *)0,
			      &timeout)) < 0 ) {
	    w_error(SYSCALL, namestring, err[F_SELECT], errno);
	    close(sd);
	    exit(INCOMPLETE);
	}

	/*
	 * Clean up and wait for client process.
	 */
	close(sd);
	if ( wait(&status) < 0 ) {
	    w_error(SYSCALL, namestring, err[F_WAIT], errno);
	    exit(INCOMPLETE);
    	}
	if ( !WIFEXITED(status)  ) {
	    w_error(GENERAL, namestring, err[C_NOTEXIT], 0);
	    exit(INCOMPLETE);
    	}

	/*
	 * Evaluate retval.  It should be 1 for positive 
	 * cases and 0 for negative cases. If so, exit 
	 * with child's exit status.  If not, write error
	 * message and exit 1.
	 */
	if ( parmsptr->expect_success == PASS  ) {  
	    /* 
	     * positive case 
	     */
	    if ( retval == 1 ) {
		 exit( WEXITSTATUS(status) );
	     }
	    w_error(GENERAL, namestring, err[T_SELECT], 0);
	    exit(FAIL);
	}
	/*
	 * negative case
	 */
	if ( retval == 0 ) {
	    exit( WEXITSTATUS(status) );
	}
	w_error(GENERAL, namestring, err[U_SELECT], 0);
	exit(FAIL);
    }  

  
    /* 
     * This is the client process. Close server socket.
     */
    close(sd);
    close(pipefd1[1]);
    close(pipefd2[0]);
    /*
     * Create a client socket
     */
    if ( ( sd2 = socket(AF_INET, parmsptr->type, 0))  < 0 ) {
	w_error(SYSCALL, namestring, err[F_SOCKET], errno);
	exit(INCOMPLETE);
    }
    
    /*
     * Initialize socket address appropriately 
     * for AF_INET. 
     */
    memset((void *) &sad2, '\0', sizeof(sad2));
    sad2.sin_family = AF_INET;
    sad2.sin_port = sad.sin_port;
    
    /*
     * Write to pipe so server knows we're ready.
     */
    if ( ( write(pipefd2[1], pipebuf, sizeof(pipebuf)) )
	!= (sizeof(pipebuf)) ) {
	w_error(SYSCALL, namestring, err[PIPEWRITE_S], errno);
	exit(INCOMPLETE);
    }
    
    /*
     * Read from pipe to ensure that server socket is ready.
     */
    if ( ( read(pipefd1[0], pipebuf, sizeof(pipebuf)) ) != sizeof(pipebuf) ) {
	w_error(SYSCALL, namestring, err[PIPEREAD_O], errno);
	exit(INCOMPLETE);
    }

    /*
     * If type is stream, call connect and write.  If this is a
     * positive case and connect failed, the case failed, exit 1. If
     * this is a negative case and errno != expect_errno, this is an
     * unexpected error, exit 2.  If this is a negative case and errno
     * == expect_errno, exit(0).  
     * If connect succeeds, write to the socket and exit(0).
     */
    if ( parmsptr->type == SOCK_STREAM ) {
	sleep(1);
	if ( connect(sd2, &sad2, sizeof(sad2)) < 0 ) {
	    if ( parmsptr->expect_success == PASS  ) {
		w_error(SYSCALL, namestring, err[F_CONNECT], errno);
		close(sd2);
		exit(FAIL);
	    }
	    if ( errno != parmsptr->expect_errno ) {
		w_error(SYSCALL, namestring, err[F_CONNECT], errno);
		close(sd2);
		exit(INCOMPLETE);
	    }
	    else {
		close(sd2);
		exit(PASS);
	    }
	}
	if ( ( write(sd2, smsg, sizeof(smsg)) ) != sizeof(smsg) ) {
	    w_error(SYSCALL, namestring, err[F_WRITE], errno);
	    close(sd2);
	    exit(INCOMPLETE);
	}
	exit(PASS);
    }
    /*
     * If type is dgram, call sendto.  If this is a positive case
     * and connect failed, then this case failed, exit 1. If this
     * is a negative case and errno != expect_errno, then this is an
     * unexpected error, exit 2.  If this is a negative case and errno
     * == expect_errno, exit(0).  If this is a positive case and
     * sendto succeeded, exit(0).
     */
    /* SOCK_DGRAM */
    if ( sendto(sd2, smsg, sizeof(smsg), 0, &sad2, sizeof(sad2)) < 0 ) {
	if ( parmsptr->expect_success == PASS  ) {
	    w_error(SYSCALL, namestring, err[F_CONNECT], errno);
	    close(sd2);
	    exit(FAIL);
	}
	if ( errno != parmsptr->expect_errno ) {
	    w_error(SYSCALL, namestring, err[F_CONNECT], errno);
	    close(sd2);
	    exit(INCOMPLETE);
	}
    }
    close(sd2);
    exit(PASS);
    return(PASS);
}
