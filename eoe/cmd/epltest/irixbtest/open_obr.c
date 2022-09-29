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
#include "irixbtest.h"
#define OFFSET_BYTES 16384

static char file[] = "open_obr_testfile";      /* File name. */
static char wbuf[] = "This is a test.\n";
static char bytebuf[1];
static char rbuf[SLEN];
static int  open_obr1(void);

int
open_obr(void)
{
	int fail = 0;            /* set when a test case fails */
	int incomplete = 0;      /* set when a test case is incomplete */
	char str[MSGLEN];        /* used to write to logfiles */
	char testname[SLEN];     /* used to write to logfiles */
	char name[SLEN];            
	short num = 1;             
	
	strcpy(testname,"open_obr");
	strcpy(name, "pos01");
	
	/*
	 * Write formatted info to raw log.
	 */   
	RAWLOG_SETUP(testname, 1);
	flush_raw_log();   
	/*
	 * Call function 
	 */    
	switch ( open_obr1() ) {
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
	cap_unlink(file);
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
open_obr1(void)
{
	int fd;                  /* File descriptor.*/
	int bytes;
	struct stat statbuf;
	char namestring[MSGLEN];     /* Used in error messages. */
	
	/* 
	 * Open a new file with O_CREAT|O_EXCL|O_RDWR.
	 */
	cap_unlink(file);
	if ( (fd = cap_open(file, O_CREAT|O_EXCL|O_RDWR, 0666)) < 0 ) {
		w_error(SYSCALL, namestring, err[FILEOPEN], errno);
		return(INCOMPLETE);
	}
	/*
	 * Check file size.
	 */
	if (stat(file, &statbuf) < 0) {
		w_error(SYSCALL, namestring, err[F_STAT], errno);
		return(INCOMPLETE);
	}
	if (statbuf.st_size != 0) {
		w_error(GENERAL, namestring, err[BADFSIZE], 0);
		return(FAIL);
	}
	/*
	 * Seek beyond end and write contents of wbuf.
	 */
	if (lseek(fd, OFFSET_BYTES, SEEK_SET) < 0) {
		w_error(SYSCALL, namestring, err[F_LSEEK], errno);
		return(INCOMPLETE);
	}
	if (write(fd, wbuf, sizeof(wbuf)) < 0)  {
		w_error(SYSCALL, namestring, err[F_WRITE], errno);
		return(INCOMPLETE);
	}
	/*
	 * Space before OFFSET_BYTES should be zero-filled.
	 */
	if (lseek(fd, 0, SEEK_SET) < 0) {
		w_error(SYSCALL, namestring, err[F_LSEEK], errno);
		return(INCOMPLETE);
	}
	for (bytes = 0; bytes < OFFSET_BYTES; bytes++) {
		if (read(fd, bytebuf, sizeof(bytebuf)) < 0) {
			w_error(SYSCALL, namestring, err[F_READ], errno);
			return(INCOMPLETE);
		}
		if (strcmp(bytebuf, "\0") != 0) {
			w_error(GENERAL, namestring, err[BADBYTE_BEFORE],0);
			return(FAIL);
		}
	}
	/*
	 * We should be at OFFSET_BYTES. Read what we wrote before.
	 */
	if (read(fd, rbuf, sizeof(rbuf)) < 0) {
		w_error(SYSCALL, namestring, err[F_READ], errno);
		return(INCOMPLETE);
	}
	if (strcmp(rbuf, wbuf) != 0)  {
		w_error(GENERAL, namestring, err[BADSTRING], 0);
		return(FAIL);
	}
	/*
	 * Reading beyond our last write should return zeros.
	 */
	for (bytes = 0; bytes < OFFSET_BYTES; bytes++) {
		if (read(fd, bytebuf, sizeof(bytebuf)) < 0) {
			w_error(SYSCALL, namestring, err[F_READ], errno);
			return(INCOMPLETE);
		}
		if (strcmp(bytebuf, "\0") != 0) {
			w_error(GENERAL, namestring, err[BADBYTE_BEYOND],0);
			return(FAIL);
		}
	}
	/* 
	 * Close and re-open O_TRUNC
	 */
	close(fd);
	if ( (fd = cap_open(file, O_TRUNC | O_WRONLY)) < 0 ) {
		w_error(SYSCALL, namestring, err[FILEOPEN], errno);
		return(INCOMPLETE);
	}
	/*
	 * Check that file size is zero again.
	 */
	if (stat(file, &statbuf) < 0 ) {
		w_error(SYSCALL, namestring, err[F_STAT], errno);
		return(INCOMPLETE);
	}
	if (statbuf.st_size != 0) {
		w_error(GENERAL, namestring, err[BADFSIZE], 0);
		return(FAIL);
	}
	/*
	 * Close and open for reading again.
	 */
	close(fd);
	if ( (fd = cap_open(file, O_RDWR)) < 0 )  {
		w_error(SYSCALL, namestring, err[FILEOPEN], errno);
		return(INCOMPLETE);
	}
	/*
	 * Stuff we wrote should be gone and
	 * read should return all zeros.
	 */
	for (bytes = 0; bytes < (2 * OFFSET_BYTES); bytes++) {
		if (read(fd, bytebuf, sizeof(bytebuf)) < 0) {
			w_error(SYSCALL, namestring, err[F_READ], errno);
			return(INCOMPLETE);
		}
		if (strcmp(bytebuf, "\0") != 0) {
			w_error(GENERAL, namestring, err[BADBYTE_EMPTY],0);
			return(FAIL);
		}
	}
	close(fd);
	cap_unlink(file);
	return(PASS);
}
