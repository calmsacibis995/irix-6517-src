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
#include "irixbtest.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <net/if.h>

/* 
 * Timeout values
 */
#define TV_SEC   1
#define TV_USEC  200000

struct so_obrinfo {
	char *casename;          /* Test case name. */
	short casenum;           /* Test case number. */
	int type;                /* STREAM or DGRAM. */
};

/*
 * Each structure contains information for one test case.
 */
static struct so_obrinfo 
	soc_info[] = 
{
	/* 
	 * case  case   type
	 * name  num 
	 */
	"pos00",  0,  SOCK_STREAM,
	"pos01",  1,  SOCK_DGRAM,
	"pos02",  2,  SOCK_STREAM,
	"pos03",  3,  SOCK_DGRAM,
};

/* 
 * Description strings, corresponding with test cases, 
 * to be printed to the raw log if that case fails.
 */
static char *socket_desc[] = { 
	"INET STREAM",
	"INET DGRAM",
	"UNIX STREAM",
	"UNIX DGRAM",
};

/*
 * socket_obr1() tests INET sockets.
 */
static int socket_obr1(struct so_obrinfo *);

/*
 * socket_obr2() tests UNIX sockets.
 */
static int socket_obr2(struct so_obrinfo *);

int
socket_obr(void)
{
	char str[MSGLEN];        /* used to write to logfiles */
	char testname[SLEN];     /* used to write to logfiles */
	char *name;              /* holds casename from struct */
	short num;               /* holds casenum from struct */
	short ncases1 = 2;       /* number of unix domain test cases */
	short ncases2 = 2;       /* number of inet domain test cases */
	short i = 0;             /* test case loop counter */
	int fail = 0;            /* set when a test case fails */
	int incomplete = 0;      /* set when a test case is incomplete */
	int retval;              /* return value of test func */
	
	strcpy(testname,"socket_obr");
	/*
	 * Write formatted info to raw log.
	 */   
	RAWLOG_SETUP(testname, (ncases1 + ncases2));
	/*
	 * Call function for each test case.
	 */    
	for (i = 0; i < (ncases2 + ncases1); i++) {
		/*
		 * Flush output.
		 */    
		flush_raw_log();   
		/*
		 * Call socket_obr1() for each INET test case, passing a ptr to
		 * the structure containing the parameters for that case.
		 * A return value of 0 indicates PASS, 1 indicates FAIL, 2
		 * indicates the test was aborted due to an unexpected error.
		 */
		if (i < ncases1)    /* inet domain */
			retval = socket_obr1(&soc_info[i]);
		else               /* unix domain */
			retval = socket_obr2(&soc_info[i]);
		num = soc_info[i].casenum;
		name = soc_info[i].casename;
		switch (retval) {
			/*
			 * Write formatted result to raw log.
			 */    
		case PASS:      /* Passed */
			RAWPASS(num, name);
			break;
		case FAIL:      /* Failed */
			RAWFAIL(num, name, socket_desc[i]);
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
socket_obr1(struct so_obrinfo *parmsptr)
{
	char testname[SLEN];                /* used to write to logfiles */
	char namestring[MSGLEN];            /* Used in error messages. */
	int retval = 0;                     /* Return value of select call. */
	pid_t forkval1 = 0;                 /* Child (server) pid  */ 
	int status;                         /* For wait. */   
	struct sockaddr_in sad;             /* Socket address */
	int sd;                             /* Socket descriptor */
	fd_set r_set;                       /* For select */
	struct timeval timeout;             /* For select */
	char rbuf[SLEN];

	strcpy(testname,"socket_obr");
	/* 
	 * Fork a child and wait for it.
	 */
	if ( ( forkval1 = fork() ) == -1 ) {
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
		if ( !WIFEXITED(status)  ) {
			w_error(GENERAL, namestring, err[C_NOTEXIT], 0);
			return(INCOMPLETE);
		}
		return(WEXITSTATUS(status) );
	}
	
	/*
	 * Create a server socket.
	 */
	if ( ( sd = socket(AF_INET, parmsptr->type, 0) ) < 0 ) {
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
	 * If parmsptr->type is stream, call listen.
	 */
	if ( parmsptr->type == SOCK_STREAM ) {
		if ( ( listen(sd, 1) ) < 0 ) {
			w_error(SYSCALL, namestring, err[F_LISTEN], errno);
			close(sd);
			exit(INCOMPLETE);
		}
	}
	
	/*
	 * Call select.  Positive (all) cases should time out. 
         */
	timeout.tv_sec = TV_SEC;
	timeout.tv_usec = TV_USEC;
	FD_ZERO(&r_set);
	FD_SET(sd, &r_set);
	if ( ( retval = select(sd+1, &r_set, (fd_set *)0, (fd_set *)0,
			      &timeout) ) < 0 ) {
		w_error(SYSCALL, namestring, err[F_SELECT], errno);
		close(sd);
		exit(INCOMPLETE);
	}
	if ( retval != 0 ) {
		w_error(GENERAL, namestring, err[U_SELECT], 0);
		close(sd);
		exit(FAIL);
	}
	/*
	 * See what read returns.
	 */
	if ( fcntl(sd, F_SETFL, FNDELAY) < 0 ) {
		w_error(SYSCALL, namestring, err[F_FCNTL], errno);
		exit(FAIL);
	} 
	if ( read(sd, rbuf, sizeof(rbuf)) > 0 ) {
		w_error(GENERAL, namestring, err[BADBYTE_EMPTY],0);
		exit(FAIL);
	}
	/*
	 * Clean up
	 */
	close(sd);
	exit(PASS);
	return(PASS);
}

static int 
socket_obr2(struct so_obrinfo *parmsptr)
{
	char testname[SLEN];                /* used to write to logfiles */
	char namestring[MSGLEN];            /* Used in error messages. */
	int retval = 0;                     /* Return value of select call. */
	pid_t forkval1 = 0;                 /* Child (server) pid  */ 
	int status;                         /* For wait. */   
	struct sockaddr sad;                /* Socket address */
	int sd;                             /* Socket descriptor */
	fd_set r_set;                       /* For select */
	struct timeval timeout;             /* For select */
	char rbuf[SLEN];
	
	strcpy(testname,"socket_obr");
	/* 
	 * Fork a child and wait for it.
	 */
	if ( ( forkval1 = fork() ) == -1 ) {
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
		if ( !WIFEXITED(status)  ) {
			w_error(GENERAL, namestring, err[C_NOTEXIT], 0);
			return(INCOMPLETE);
		}
		return(WEXITSTATUS(status) );
	}
	/*
	 * Create a server socket.
	 */    
	if ( ( sd = socket(AF_UNIX, parmsptr->type, 0) )  < 0 ) {
		w_error(SYSCALL, namestring, err[F_SOCKET], errno);
		exit(INCOMPLETE);
	}
	/*
	 * Initialize socket address appropriately 
	 * for AF_UNIX and bind socket.
	 */
	memset((void *) &sad, '\0', sizeof(sad));
	sad.sa_family = AF_UNIX;
	sprintf(sad.sa_data, "s%d", parmsptr->casenum);
	cap_unlink(sad.sa_data);
	
	if ( bind(sd, &sad, sizeof sad) < 0 ) {
		w_error(SYSCALL, namestring, err[F_BIND], errno);
		close(sd);
		exit(INCOMPLETE);
	}
	cap_fchmod(sd, 777);
	
	/*
	 * If parmsptr->type is stream, call listen.
	 */
	if ( parmsptr->type == SOCK_STREAM ) {
		if ((listen(sd, 1)) < 0) {
			w_error(SYSCALL, namestring, err[F_LISTEN], errno);
			close(sd);
			cap_unlink(sad.sa_data);
			exit(INCOMPLETE);
		}
	}
	
	/*
	 * Call select.  Positive (all) cases should time out. 
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
	if ( retval != 0 ) {
		w_error(GENERAL, namestring, err[U_SELECT], 0);
		cap_unlink(sad.sa_data);
		close(sd);
		exit(FAIL);
	}
	
	/*
	 * See what read returns.
	 */
	if ( fcntl(sd, F_SETFL, FNDELAY) < 0 ) {
		w_error(SYSCALL, namestring, err[F_FCNTL], errno);
		exit(FAIL);
	} 
	if ( read(sd, rbuf, sizeof(rbuf)) > 0 ) {
		w_error(GENERAL, namestring, err[BADBYTE_EMPTY],0);
		exit(FAIL);
	}
	/*
	 * Clean up
	 */
	close(sd);
	cap_unlink(sad.sa_data);
	exit(PASS);
	return(PASS);
}
