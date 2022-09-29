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

symlink_obr1(void);

static char file[] = {
	"symlink_obr_testfile"
};

static char symname[] = {
	"symlink_obr_testlink"
};

static char rbuf[RBUFLEN];
static int  symlink_obr1(void);

int
symlink_obr(void)
{
	int fail = 0;            /* set when a test case fails */
	int incomplete = 0;      /* set when a test case is incomplete */
	char str[MSGLEN];        /* used to write to logfiles */
	char testname[SLEN];     /* used to write to logfiles */
	char name[SLEN];            
	short num = 1;             
	
	strcpy(testname,"symlink_obr");
	strcpy(name, "pos01");
	
	/*
	 * Write formatted info to raw log.
	 */   
	RAWLOG_SETUP(testname, 1);
	flush_raw_log();   
	/*
	 * Call function 
	 */    
	switch ( symlink_obr1() ) {
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
	cap_unlink(symname);
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
symlink_obr1(void)
{
	int fd;                  /* File descriptor.*/
	int bytes = 0;
	struct stat statbuf;
	int rval = 0;
	char namestring[MSGLEN];     /* Used in error messages. */

	/* 
	 * Open a new file and close it.
	 */
	cap_unlink(file);
	if ( (fd = cap_open( file, O_CREAT|O_EXCL|O_RDWR, 0666) ) < 0 ) {
		w_error(SYSCALL, namestring, err[FILEOPEN], errno);
		return(INCOMPLETE);
	}
	/* 
	 * Make a symlink to the file.
	 */
	if ( symlink(file, symname) < 0 ) {
		w_error(SYSCALL, namestring, err[F_SYMLINK], errno);
		return(INCOMPLETE);
	}
	/*
	 * Check symlink size.
	 */
	if ( lstat(symname, &statbuf) < 0 ) {
		w_error(SYSCALL, namestring, err[F_STAT], errno);
		return(INCOMPLETE);
	}
	if ( statbuf.st_size > sizeof(file) ) {
		w_error(GENERAL, namestring, err[BADLINKSIZE], 0);
		return(FAIL);
	}

	/*
	 * Read contents of symlink.
	 */
	if ( rval = readlink(symname, rbuf, sizeof(rbuf)) < 0 ) {
		w_error(SYSCALL, namestring, err[F_READLINK], errno);
		return(INCOMPLETE);
	}
	if ( strncmp(rbuf, symname, rval ) != 0 )  {
		w_error(GENERAL, namestring, err[BADSYM], 0);
		return(FAIL);
	}
	/* 
	 * Open the symlink and try to read beyond end.
	 */
	if ( ( fd = cap_open(file, O_RDWR) ) < 0 )  {
		w_error(SYSCALL, namestring, err[FILEOPEN], errno);
		return(INCOMPLETE);
	}
	if ( lseek(fd, rval, SEEK_SET) < 0 ) {
		w_error(SYSCALL, namestring, err[F_LSEEK], errno);
		return(INCOMPLETE);
	}
	/*
	 * Try to read past the end.
	 */
	do {
		if ( ( rval = read(fd, rbuf, sizeof(rbuf)) ) < 0 ) {
			w_error(SYSCALL, namestring, err[F_READ], errno);
			return(INCOMPLETE);
		}
		bytes += rval;
	} while ( rval > 0 );
	
	if ( bytes > 0 ) {
		w_error(GENERAL, namestring, err[BADBYTE_BEYOND],0);
		return(FAIL);
	}
	close(fd);
	return(PASS);
}
