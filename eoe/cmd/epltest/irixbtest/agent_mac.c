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
#define TV_SEC  1
#define TV_USEC 200000

/*
 * agent_mac.c -- Sets various labels on sending and receiving
 * sockets and tests whether the message is received.  It should
 * be received if and only if the labels are equal.
 */


/*
 * agent_mac1() tests INET sockets.
 */
static int agent_mac1(int n, int type,
		      struct msen_l* sendmsenptr, struct mint_l* sendmintptr,
		      struct msen_l* recvmsenptr, struct mint_l* recvmintptr);

/*
 * agent_mac2() tests UNIX sockets.
 */
static int agent_mac2(int n, int type,
		       struct msen_l* sendmsenptr, struct mint_l* sendmintptr,
		       struct msen_l* recvmsenptr, struct mint_l* recvmintptr);

static char smsg[] = "Hello";           /* Message sent by client */
static short npos = 0;                  /* Count of positive cases */
static char casename[SLEN];             /* used to write to logfiles */

int
agent_mac(void)
{

    short ncases = (NDOMAINS * NTYPES * NMSEN * NMINT *	NMSEN * NMINT);
                                /* Number of test cases */

    register short n = 0;       /* Test case counter */
    register short i,j,l,m,o,p; /* Array indices */
    int fail = 0;               /* set when a test case fails */
    int incomplete = 0;         /* set when a test case is incomplete */
    short retval = 0;           /* return value of test subroutine */
    char str[MSGLEN];           /* used to write to logfiles */
    char desc[MSGLEN];          /* used to write to logfiles */
    char testname[SLEN];        /* used to write to logfiles */
    char domainstr[SLEN];       /* domain description string */
    char namestring[MSGLEN];    /* Used in error messages. */
    
    strcpy(testname,"agent_mac");
    /*
     * Write formatted info to raw log.
     */   
    RAWLOG_SETUP(testname, ncases);
    /*
     * Set lo0 interface to SGIPSO2 with full label range.
     */
    /* if (system("iflabel lo0 SGIPSO2 msenhigh/mintlow msenlow/minthigh 3")) { */
    if (system("iflabel lo0 SGIPSO2 attributes=msen,mint min_msen=msenlow max_msen=msenhigh min_mint=mintlow max_mint=minthigh 3")) {
	w_error(GENERAL, namestring, err[F_SYSIFLABEL], 0);
	return(INCOMPLETE);
    }

    /*
     * Call function for each test case
     */   
    for (i = 0; i < NDOMAINS; i++) {
	for (j = 0; j < NTYPES; j++) {
	    for (l = 0; l < NMSEN; l++) {
		for (m = 0; m < NMINT; m++) {
		    for (o = 0; o < NMSEN; o++) {
			for (p = 0; p < NMINT; p++) {
			    flush_raw_log(); 
			    if (domain[i] == AF_INET) { 
				/*
				 * Call agent_mac1 to test
				 * INET domain sockets.
				 */
				retval =  agent_mac1(n, type[j],
						      &msenlist[l],
						      &mintlist[m],
						      &msenlist[o],
						      &mintlist[p]);
				strcpy(domainstr, "INET");
			    }
			    else {
				/*
				 * Call agent_mac2 to test
				 * UNIX domain sockets.
				 */
				retval =  agent_mac2(n, type[j],
						      &msenlist[l],
						      &mintlist[m],
						      &msenlist[o],
						      &mintlist[p]);
				strcpy(domainstr, "UNIX");
			    }
			    switch ( retval ) {
				/*
				 * Write formatted result to raw log.
				 */    
			    case PASS:      /* Passed */
				RAWPASS(n, casename);
				break;
			    case FAIL:      /* Failed */
				sprintf(desc, "%s, %s, %s/%s, %s/%s",
					domainstr, typedesc[j],
					msendesc[l], mintdesc[m], 
					msendesc[o], mintdesc[p]);
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
 * agent_mac1 first compares send and receive labels using mac_equal
 * to determine whether this is a positive or negative test case. Then it
 * forks a server process that sets its process label, creates and binds
 * a socket, then calls getsockname to get the port number.  Once the
 * port number is known, it forks a client process which sets its process
 * label, creates a socket, and attempts to connect or send.  The server
 * then calls select. Negative cases should time out and positive cases
 * should detect 1 fd.  
 */

static int 
agent_mac1(int n, int type, struct msen_l* sendmsenptr,
	    struct mint_l* sendmintptr, struct msen_l* recvmsenptr,
	    struct mint_l* recvmintptr)
{
    register char i = 0;                /* cat/div loop counter */
    char testname[SLEN];                /* used to write to logfiles */
    char namestring[MSGLEN];            /* Used in error messages. */
    int retval = 0;                     /* Return value of select call. */
    pid_t forkval1 = 0;                 /* Child (server) pid  */ 
    pid_t forkval2 = 0;                 /* Grandchild (client) pid */ 
    mac_t slptr;                        /* Pointer to client MAC label. */
    mac_t rlptr;                        /* Pointer to server MAC label. */
    int status;                         /* For wait. */   
    struct sockaddr_in sad, sad2;       /* Socket addresses */
    int sd, sd2;                        /* Socket descriptors */
    int sadsize;                        /* Address sizes */
    fd_set r_set;                       /* For select */
    struct timeval timeout;             /* For select */
    short expect_success;               /* set PASS  if send and recv */
    char pipebuf[4];                    /* Used in pipe read/writes. */
    int pipefd1[2];                     /* For synchronization. */
    int pipefd2[2];                     /* For synchronization. */
					/* labels are equal */

    strcpy(testname,"agent_mac");
    strcpy (pipebuf, "abc");      /* Stuff to write to pipe. */

    /*
     * Determine whether this is a positive or negative test case, 
     * that is, whether the send and receive labels are evaluated 
     * as equal by the function mac_equal.
     */

    /* 
     * Set up slptr.
     */    
    slptr = mac_get_proc();
    slptr->ml_msen_type = sendmsenptr->msen;
    slptr->ml_level = sendmsenptr->level;
    slptr->ml_catcount = sendmsenptr->catcount;
    for (i = 0; i < sendmsenptr->catcount; i++) { 
	slptr->ml_list[i] = sendmsenptr->catlist[i];
    }
    slptr->ml_mint_type = sendmintptr->mint;
    slptr->ml_grade = sendmintptr->grade;
    slptr->ml_divcount = sendmintptr->divcount;
    for (i = 0; i < sendmintptr->divcount; i++) {
	slptr->ml_list[i + sendmsenptr->catcount] =
	    sendmintptr->divlist[i];
    }
    /* 
     * Set rlptr.
     */    
    rlptr = mac_get_proc();
    rlptr->ml_msen_type = recvmsenptr->msen;
    rlptr->ml_level = recvmsenptr->level;
    rlptr->ml_catcount = recvmsenptr->catcount;
    for (i = 0; i < recvmsenptr->catcount; i++) { 
	rlptr->ml_list[i] = recvmsenptr->catlist[i];
    }
    rlptr->ml_mint_type = recvmintptr->mint;
    rlptr->ml_grade = recvmintptr->grade;
    rlptr->ml_divcount = recvmintptr->divcount;
    for (i = 0; i < recvmintptr->divcount; i++) {
	rlptr->ml_list[i + recvmsenptr->catcount] =
	    recvmintptr->divlist[i];
    }

    /*
     * If equal, set expect_success true, create positive 
     * casename, and increment static variable npos.  Otherwise, 
     * set expect_success FAIL and create negative casename.
     */

    if ( mac_equal(slptr, rlptr) ) {
	expect_success = PASS ;
	sprintf(namestring, "%s, case %d, pos%3.3d:\n   ", testname, n, npos);
	sprintf(casename, "pos%d",npos++);
    }
    else {
	expect_success = FAIL;
	sprintf(namestring, "%s, case %d, neg%3.3d:\n   ", testname, n,
		(n - npos));
	sprintf(casename, "neg%d", (n - npos));
    }

    /* 
     * Fork a child and wait for it.
     */
    if (  ( forkval1 = fork() ) == -1  ) {
	w_error(SYSCALL, namestring, err[F_FORK], errno);
	return(INCOMPLETE);
    }
    
    /* 
     * This is the parent.  Wait for child and return
     * 2 on unexpected error. Otherwise, return child's 
     * exit code.
     */
    if (forkval1) {
	if ( wait(&status) < 0 ) {
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
     * This is the server process.  Make 2 pipes.
     */
    if ( pipe(pipefd1) || pipe(pipefd2) )  {
	w_error(SYSCALL, namestring, err[F_PIPE], errno);
	return(INCOMPLETE);
    }

    /* 
     * Set process label to rlptr.
     */    
    if ( cap_setplabel(rlptr) ) {
	w_error(SYSCALL,namestring,err[F_SETPLABEL],errno);
	exit(INCOMPLETE);
    }
    mac_free(rlptr);

    /*
     * Create a server socket.
     */
    if ( (sd = socket(AF_INET, type, 0))  < 0 ) {
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
	close(sd);
	exit(INCOMPLETE);
    }
    
    /*
     * Get port so client process can use it.
     */
    sadsize = sizeof(sad);
    if (getsockname(sd, &sad, &sadsize)) {
	w_error(SYSCALL, namestring, err[F_GETSOCKNAME], errno);
	close(sd);
	exit(INCOMPLETE);
    }
    
    /*
     * Now that we know the port, fork the client process.
     */
    if (  ( forkval2 = fork() ) < 0 ) {
	w_error(SYSCALL, namestring, err[F_FORK], errno);
	close(sd);
	exit(INCOMPLETE);
    }
    if (forkval2) {
	/*
	 * This is still the server.  Close pipe1's read fd and
	 * pipe2's write fd.
	 */
	close(pipefd1[0]);
	close(pipefd2[1]);

	/* 
	 * If type is stream, call listen.
	 */
  	if (type == SOCK_STREAM) {
	    if ((listen(sd, 1)) < 0) {
		w_error(SYSCALL, namestring, err[F_LISTEN], errno);
		close(sd);
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
	if ( (write(pipefd1[1], pipebuf, sizeof(pipebuf)))
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
	if ( wait(&status) == -1 ) {
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
	if (expect_success == PASS ) {  
	    /* 
	     * positive case 
	     */
	    if (retval == 1) {
		 exit( WEXITSTATUS(status) );
	     }
	    w_error(GENERAL, namestring, err[T_SELECT], 0);
	    exit(FAIL);
	}
	/*
	 * negative case
	 */
	if (retval == 0) {
	    exit( WEXITSTATUS(status) );
	}
	w_error(GENERAL, namestring, err[U_SELECT], 0);
	exit(FAIL);
    }  

  
    /* 
     * This is the client process. Close server socket.
     */
    close(sd);
#if 0
    close(pipefd1[1]);
    close(pipefd2[0]);
#endif
    close(pipefd1[1]);
    close(pipefd2[0]);
    
    /*
     * Set process label to slptr->
     */
    if ( cap_setplabel(slptr) ) {
	w_error(SYSCALL,namestring,err[F_SETPLABEL],errno);
	exit(INCOMPLETE);
    }
    mac_free(slptr);
    
    /*
     * Create a client socket
     */
    if ( (sd2 = socket(AF_INET, type, 0))  < 0 ) {
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
    if ( (write(pipefd2[1], pipebuf, sizeof(pipebuf)))
	!= (sizeof(pipebuf)) ) {
	w_error(SYSCALL, namestring, err[PIPEWRITE_S], errno);
	exit(INCOMPLETE);
    }
    
    /*
     * Read from pipe to ensure that server socket is ready.
     */
    if ( (read(pipefd1[0], pipebuf, sizeof(pipebuf))) != sizeof(pipebuf) ) {
	w_error(SYSCALL, namestring, err[PIPEREAD_O], errno);
	exit(INCOMPLETE);
    }

    /*
     * If type is stream, call connect.  If this is a
     * negative case and connect fails with ECONNREFUSED, exit(0).
     * If errno != ECONNREFUSED, this is an unexpected error, exit
     * 2. If this is a positive case and connect failed with 
     * errno == ECONNREFUSED, the case failed, exit 1.  If connect 
     * succeeds, continue.
     */
    if ( type == SOCK_STREAM ) {
	if (connect(sd2, &sad2, sizeof(sad2)) < 0) {
	    if (errno != ECONNREFUSED) {
		w_error(SYSCALL, namestring, err[F_CONNECT], errno);
		close(sd2);
		exit(INCOMPLETE);
	    }
	    if (expect_success == PASS ) {
		w_error(SYSCALL, namestring, err[F_CONNECT], errno);
		close(sd2);
		exit(FAIL);
	    }
	    else {
		close(sd2);
		exit(PASS);
	    }
	}
#if 0
	if ( (write(sd2, smsg, sizeof(smsg))) != sizeof(smsg)) {
	    w_error(SYSCALL, namestring, err[F_WRITE], errno);
	    close(sd2);
	    exit(INCOMPLETE);
	}
#endif
    }
    /*
     * If type is dgram, call sendto.  If this is a
     * negative case and sendto fails with ECONNREFUSED, exit(0).
     * If errno != ECONNREFUSED, this is an unexpected error, exit
     * 2. If this is a positive case and connect failed with
     * errno == ECONREFUSED, the case failed, exit 1.  If sendto 
     * succeeds, exit(0).
     */
    else { /* SOCK_DGRAM */
	if (sendto(sd2, smsg, sizeof(smsg), 0, &sad2, sizeof(sad2)) < 0 ) {
	    if (expect_success == PASS ) {
		w_error(SYSCALL, namestring, err[F_CONNECT], errno);
		close(sd2);
		exit(FAIL);
	    }
	    if (errno != ECONNREFUSED) {
		w_error(SYSCALL, namestring, err[F_CONNECT], errno);
		close(sd2);
		exit(INCOMPLETE);
	    }
	}
    }
    close(sd2);
    exit(PASS);
    return(PASS);
}



/*
 * agent_mac2 first compares send and receive labels using mac_equal
 * to determine whether this is a positive or negative test case. Then 
 * it forks a server process which forks a client process.  Each 
 * process sets its process label and creates a socket.  The client 
 * attempts to connect or send.  The server binds a name, then calls 
 * select.  Negative cases should time out and positive cases should 
 * detect 1 fd.   
 */

static int 
agent_mac2(int n, int type, struct msen_l* sendmsenptr,
	    struct mint_l* sendmintptr, struct msen_l* recvmsenptr, struct
	    mint_l* recvmintptr)
{
    register char i = 0;                /* cat/div loop counter */
    char testname[SLEN];                /* used to write to logfiles */
    char namestring[MSGLEN];            /* Used in error messages. */
    int retval = 0;                     /* Return value of select call. */
    pid_t forkval1 = 0;                 /* Child (server) pid  */ 
    pid_t forkval2 = 0;                 /* Grandchild (client) pid */ 
    mac_t slptr;                        /* Pointer to client MAC label. */
    mac_t rlptr;                        /* Pointer to server MAC label. */
    mac_t wlptr;                        /* Pointer to wildcard MAC label. */
    int status;                         /* For wait. */   
    struct sockaddr sad;                /* Socket addresses */
    int sd;                             /* Socket descriptors */
    fd_set r_set;                       /* For select */
    struct timeval timeout;             /* For select */
    short expect_success;               /* set PASS  if send and recv */
    char pipebuf[4];                        /* Used in pipe read/writes. */
    int pipefd1[2];                      /* For synchronization. */
    int pipefd2[2];                     /* For synchronization. */
					/* labels are equal */

    strcpy(testname,"agent_mac");
    strcpy (pipebuf, "abc");      /* Stuff to write to pipe. */
    /*
     * Determine whether this is a positive or negative test case, 
     * that is, whether the send and receive labels are evaluated 
     * as equal by the function mac_equal.
     */
    /* 
     * Set up slptr.
     */    
    slptr = mac_get_proc();
    slptr->ml_msen_type = sendmsenptr->msen;
    slptr->ml_level = sendmsenptr->level;
    slptr->ml_catcount = sendmsenptr->catcount;
    for (i = 0; i < sendmsenptr->catcount; i++) { 
	slptr->ml_list[i] = sendmsenptr->catlist[i];
    }
    slptr->ml_mint_type = sendmintptr->mint;
    slptr->ml_grade = sendmintptr->grade;
    slptr->ml_divcount = sendmintptr->divcount;
    for (i = 0; i < sendmintptr->divcount; i++) {
	slptr->ml_list[i + sendmsenptr->catcount] =
	    sendmintptr->divlist[i];
    }
    /* 
     * Set rlptr.
     */    
    rlptr = mac_get_proc();
    rlptr->ml_msen_type = recvmsenptr->msen;
    rlptr->ml_level = recvmsenptr->level;
    rlptr->ml_catcount = recvmsenptr->catcount;
    for (i = 0; i < recvmsenptr->catcount; i++) { 
	rlptr->ml_list[i] = recvmsenptr->catlist[i];
    }
    rlptr->ml_mint_type = recvmintptr->mint;
    rlptr->ml_grade = recvmintptr->grade;
    rlptr->ml_divcount = recvmintptr->divcount;
    for (i = 0; i < recvmintptr->divcount; i++) {
	rlptr->ml_list[i + recvmsenptr->catcount] =
	    recvmintptr->divlist[i];
    }

    /*
     * If equal, set expect_success true, create positive casename,
     * and increment static variable npos.  Otherwise, set
     * expect_success FAIL and create negative casename.
     */
    if ( mac_equal(slptr, rlptr) ) {
	expect_success = PASS ;
	sprintf(namestring, "%s, case %d, pos%3.3d:\n   ", testname, n, npos);
	sprintf(casename, "pos%d",npos++);
    }
    else {
	expect_success = FAIL;
	sprintf(namestring, "%s, case %d, neg%3.3d:\n   ", testname, n,
		(n - npos));
	sprintf(casename, "neg%d", (n - npos));
    }

    /* 
     * Fork a child and wait for it.
     */
    if (  ( forkval1 = fork() ) == -1  ) {
	w_error(SYSCALL, namestring, err[F_FORK], errno);
	return(INCOMPLETE);
    }
    
    /* 
     * This is the parent.  Wait for child and return
     * 2 on unexpected error. Otherwise, return child's 
     * exit code.
     */
    if (forkval1) {
	if ( wait(&status) < 0 ) {
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
     * This is the server process.       
     * Make 2 pipes.
     */
    if ( pipe(pipefd1) || pipe(pipefd2) )  {
	w_error(SYSCALL, namestring, err[F_PIPE], errno);
	return(INCOMPLETE);
    }

    /* 
     * Server forks client process
     */
    if (  ( forkval2 = fork() ) < 0 ) {
	w_error(SYSCALL, namestring, err[F_FORK], errno);
	exit(INCOMPLETE);
    }
    if (forkval2) {
	/*
	 * This is still the server. Close pipe1's read 
	 * fd and pipe2's write fd.
	 */
	close(pipefd1[0]);
	close(pipefd2[1]);

	/* 
	 * Set process label to recvlbl. 
	 */    
	if ( cap_setplabel(rlptr) ) {
	    w_error(SYSCALL,namestring,err[F_SETPLABEL],errno);
	    exit(INCOMPLETE);
	}
	mac_free(rlptr);
	
	/*
	 * Create a server socket.
	 */    
	if ( (sd = socket(AF_UNIX, type, 0))  < 0 ) {
	    w_error(SYSCALL, namestring, err[F_SOCKET], errno);
	    exit(INCOMPLETE);
	}
	/*
	 * Initialize socket address appropriately 
	 * for AF_UNIX and bind socket.
	 */
	memset((void *) &sad, '\0', sizeof(sad));
	sad.sa_family = AF_UNIX;
	sprintf(sad.sa_data, "s%d",n);
	cap_unlink(sad.sa_data);
	
	if ( bind(sd, &sad, sizeof sad) < 0 ) {
	    w_error(SYSCALL, namestring, err[F_BIND], errno);
	    close(sd);
	    exit(INCOMPLETE);
	}
	
	/*
	 * Set label of rendezvous point to wildcard.  
	 * We want to test socket MAC, not file system MAC.
	 */
	wlptr = mac_get_proc();
	wlptr->ml_msen_type = MSE;
	wlptr->ml_mint_type = MIE;
	if ( cap_setlabel(sad.sa_data, wlptr) < 0 ) {
	    close(sd);
	    cap_unlink(sad.sa_data);
	    w_error(SYSCALL, namestring, err[SETLABEL_FILE], errno);
	    exit(INCOMPLETE);
	}
	mac_free(wlptr);
	/*
	 * If type is stream, call listen.
	 */
	if (type == SOCK_STREAM) {
	    if ((listen(sd, 1)) < 0) {
		w_error(SYSCALL, namestring, err[F_LISTEN], errno);
		close(sd);
		cap_unlink(sad.sa_data);
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
	if ( (write(pipefd1[1], pipebuf, sizeof(pipebuf)))
	    != (sizeof(pipebuf)) ) {
	    w_error(SYSCALL, namestring, err[PIPEWRITE_S], errno);
	    exit(INCOMPLETE);
	}

	/*
	 * Select's return value determines whether test
	 * case passes or fails.  Negative cases should time
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
	    cap_unlink(sad.sa_data);
	    exit(INCOMPLETE);
	}
	
	/*
	 * Clean up and wait for client process.
	 */
	close(sd);
	cap_unlink(sad.sa_data);

	if ( wait(&status) == -1 ) {
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
	if (expect_success == PASS ) {
	    /* 
	     * positive case 
	     */
	    if (retval == 1) {
		 exit( WEXITSTATUS(status) );
	     }
	    w_error(GENERAL, namestring, err[T_SELECT], 0);
	    exit(FAIL);
	}
	/*
	 * negative case
	 */
	if (retval == 0) {
	    exit( WEXITSTATUS(status) );
	}
	w_error(GENERAL, namestring, err[U_SELECT], 0);
	exit(FAIL);
    }


    /* 
     * This is the client process. Close pipe1's write fd and 
     * pipe2's read fd.
     */
    close(sd);
    close(pipefd1[1]);
    close(pipefd2[0]);

    /* 
     * Set process label to slptr.     
     */
    if ( cap_setplabel(slptr) ) {
	w_error(SYSCALL,namestring,err[F_SETPLABEL],errno);
	exit(INCOMPLETE);
    }
    mac_free(slptr);
    
    /*
     * Create a client socket
     */
    if ( (sd = socket(AF_UNIX, type, 0))  < 0 ) {
	w_error(SYSCALL, namestring, err[F_SOCKET], errno);
	exit(INCOMPLETE);
    }
    
    /*
     * Initialize socket address appropriately 
     * for AF_UNIX. 
     */
    memset((void *) &sad, '\0', sizeof(sad));
    sad.sa_family = AF_UNIX;
    sprintf(sad.sa_data, "s%d",n);

    /*
     * Write to pipe so server knows we're ready.
     */
    if ( (write(pipefd2[1], pipebuf, sizeof(pipebuf)))
	!= (sizeof(pipebuf)) ) {
	w_error(SYSCALL, namestring, err[PIPEWRITE_S], errno);
	exit(INCOMPLETE);
    }
    
    /*
     * Read from pipe to ensure that server socket is ready.
     */
    if ( (read(pipefd1[0], pipebuf, sizeof(pipebuf))) != sizeof(pipebuf) ) {
	w_error(SYSCALL, namestring, err[PIPEREAD_O], errno);
	exit(INCOMPLETE);
    }

    /*
     * If type is stream, call connect and write.  If this is a
     * negative case and connect fails with EACCES, exit(0).  
     * If errno is not EACCES, this is an unexpected error, exit 2. 
     * If this is a positive case and errno ==  EACCES, then
     * the case failed, exit 1.  If connect succeeds, continue.
     */
    if ( type == SOCK_STREAM ) {
	if (connect(sd, &sad, sizeof(sad)) < 0) {
	    if  (errno != EACCES) {
		w_error(SYSCALL, namestring, err[F_CONNECT], errno);
		close(sd);
		exit(INCOMPLETE);
	    }
	    if (expect_success == PASS ) {
		w_error(SYSCALL, namestring, err[F_CONNECT], errno);
		close(sd);
		exit(FAIL);
	    }
	    else {
		close(sd);
		exit(PASS);
	    }
	}
/*	if ( (write(sd, smsg, sizeof(smsg))) != sizeof(smsg)) {
	    w_error(SYSCALL, namestring, err[F_WRITE], errno);
	    close(sd);
	    exit(INCOMPLETE);
	}
*/
    }
    /*
     * If type is dgram, call sendto.  If this is a
     * negative case and connect fails with an error indicative
     * of unequal labels, exit(0).  If errno is something else, 
     * this is an unexpected error, exit 2. If this is a positive 
     * case and errno == ECONNREFUSED, EACCES, or ENOENT,
     * the case failed, exit 1.  If connect succeeds, continue.
     */
    else { /* SOCK_DGRAM */
	if (sendto(sd, smsg, sizeof(smsg), 0, &sad, sizeof(sad)) < 0 ) {
	    if (expect_success == PASS ) {
		w_error(SYSCALL, namestring, err[F_CONNECT], errno);
		close(sd);
		exit(FAIL);
	    }
	    if ( (errno != ECONNREFUSED) && (errno != EACCES) &&
		(errno != ENOENT) ) {
		w_error(SYSCALL, namestring, err[F_CONNECT], errno);
		close(sd);
		exit(INCOMPLETE);
	    }
	}
    }
    close(sd);
    exit(PASS);
    return(PASS);
}
