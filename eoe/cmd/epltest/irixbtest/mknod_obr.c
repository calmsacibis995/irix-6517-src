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

static char fifo[] = "mknod_obr_testfifo";      /* Fifo name. */
static int mknod_obr1(void);

int
mknod_obr(void)
{
	char str[MSGLEN];        /* used to write to logfiles */
	int fail = 0;            /* set when a test case fails */
	int incomplete = 0;      /* set when a test case is incomplete */
	char testname[SLEN];     /* used to write to logfiles */
	char name[SLEN];            
	short num = 1;             
	
	strcpy(testname,"mknod_obr");
	strcpy(name, "pos01");
	
	/*
	 * Write formatted info to raw log.
	 */   
	RAWLOG_SETUP(testname, 1);
	flush_raw_log();   
	/*
	 * Call function 
	 */    
	switch ( mknod_obr1() ) {
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
	cap_chmod(fifo, 0666);
	cap_unlink(fifo);
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
mknod_obr1(void)
{
	int fd;                  /* File descriptor.*/
	int bytes;
	struct stat statbuf;
	char rbuf[16];
	char namestring[MSGLEN];     /* Used in error messages. */

	cap_chmod(fifo, 0666);
	cap_unlink(fifo);
	/* 
	 * Mknod a fifo
	 */
	if (mknod(fifo, S_IFIFO | 0666, 0) < 0) {
		w_error(SYSCALL, namestring, err[F_MKNOD], errno);
		return(INCOMPLETE);
	}
	/*
	 * Check fifo size.
	 */
	if (stat(fifo, &statbuf) <0 ) {
		w_error(SYSCALL, namestring, err[F_STAT], errno);
		return(INCOMPLETE);
	}
	if (statbuf.st_size != 0) {
		w_error(GENERAL, namestring, err[BADFSIZE], 0);
		return(FAIL);
	}
	/*
	 * Open the fifo for reading, no delay.
	 */
	if ( (fd = cap_open(fifo, O_RDONLY | O_NDELAY)) < 0 )  {
		w_error(SYSCALL, namestring, err[FILEOPEN], errno);
		return(INCOMPLETE);
	}
	/*
	 * See what read returns.
	 */
	if ((bytes = read(fd, rbuf, sizeof(rbuf))) < 0) {
		w_error(SYSCALL, namestring, err[F_READ], errno);
		return(INCOMPLETE);
	}
	if (bytes > 0) {
		w_error(GENERAL, namestring, err[BADBYTE_EMPTY],0);
		return(FAIL);
	}
	close(fd);
	return(PASS);
}
