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
#include <sys/ipc.h>
#include <sys/msg.h>
#include "irixbtest.h"

static int msgget_obr1(void);

int
msgget_obr(void)
{
	char str[MSGLEN];        /* used to write to logfiles */
	int fail = 0;            /* set when a test case fails */
	int incomplete = 0;      /* set when a test case is incomplete */
	char testname[SLEN];     /* used to write to logfiles */
	char name[SLEN];            
	short num = 1;             
	
	strcpy(testname,"msgget_obr");
	strcpy(name, "pos01");
	
	/*
	 * Write formatted info to raw log.
	 */   
	RAWLOG_SETUP(testname, 1);
	flush_raw_log();   
	/*
	 * Call function 
	 */    
	switch ( msgget_obr1() ) {
		/*
		 * Write formatted result to raw log.
		 */    
	case PASS:      /* Passed */
		RAWPASS( num, name );
		break;
	case FAIL:      /* Failed */
		RAWFAIL(  num, name, "Unaccessed bytes should be null" );
		fail = 1;
		break;
	default:
		RAWINC( num, name );
		incomplete = 1;
		break;  /* Incomplete */
	}
	flush_raw_log();
	
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
msgget_obr1(void)
{
	struct msgbuf msg;
	int msqid;                   /* Returned by msgget create call. */
	key_t key;		     /* Key value. */
	char namestring[MSGLEN];     /* Used in error messages. */

	srand(getpid());
	key = rand();
	if ( (msqid = msgget(key,(00666|IPC_EXCL|IPC_CREAT))) == -1 ) {
	    w_error (SYSCALL, namestring, err[F_MSGGET], errno);
	    return(INCOMPLETE);
	}
	/*
	 * See what msgrcv returns. With IPC_NOWAIT, if there's
	 * no message, it should return -1, error ENOMSG.
	 */
	if (msgrcv(msqid, &msg, 512, 0, IPC_NOWAIT) < 0) {
		if (errno != ENOMSG) {
			w_error(SYSCALL, namestring, err[F_MSGRCV], errno);
			return(INCOMPLETE);
		}
	}
	else {
		w_error(GENERAL, namestring, err[BADMSGRCV], 0);
		return(FAIL);
	}
	return(PASS);
}
