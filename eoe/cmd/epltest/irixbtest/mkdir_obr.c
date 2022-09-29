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
#include <dirent.h>

static dirent_t rbuf[RBUFLEN];
static char dirname[] = "mkdir_obr_testdir";      /* Dir name. */
static int  mkdir_obr1(void);

int
mkdir_obr(void)
{
	char str[MSGLEN];        /* used to write to logfiles */
	int fail = 0;            /* set when a test case fails */
	int incomplete = 0;      /* set when a test case is incomplete */
	char testname[SLEN];     /* used to write to logfiles */
	char name[SLEN];            
	short num = 1;             
	
	strcpy(testname,"mkdir_obr");
	strcpy(name, "pos01");
	
	/*
	 * Write formatted info to raw log.
	 */   
	RAWLOG_SETUP(testname, 1);
	flush_raw_log();   
	/*
	 * Call function 
	 */    
	switch ( mkdir_obr1() ) {
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
	cap_rmdir(dirname);
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
mkdir_obr1(void)
{
	int fd;                  /* File descriptor.*/
	size_t bytes = 0;
	ssize_t rval = 0;
	off_t telval;
	struct stat statbuf;
	DIR *dirp;
	struct dirent *ent;
	char namestring[MSGLEN];     /* Used in error messages. */
	/* 
	 * Make a new directory.
	 */
	cap_rmdir(dirname);
	if (cap_mkdir(dirname, 0777) < 0 )  {
		w_error(SYSCALL, namestring, err[F_MKDIR], errno);
		return(INCOMPLETE);
	}
	/*
	 * Check dir size.
	 */
	
	if (stat(dirname, &statbuf) < 0 ){
		w_error(SYSCALL, namestring, err[F_STAT], errno);
		return(INCOMPLETE);
	}
	if (statbuf.st_size > 512) {
		w_error(GENERAL, namestring, err[BADDIRSIZE], 0);
		return(FAIL);
	}
	
	/*
	 * Open the dir with opendir.
	 */
	
	if ((dirp = opendir(dirname)) <= (DIR *)0) {
		w_error(SYSCALL, namestring, err[F_OPENDIR], errno);
		cap_rmdir(dirname);
		return(INCOMPLETE);
	}
	while ((ent = readdir(dirp)) != 0) {
		if (ent < (struct dirent *)0){
			w_error(SYSCALL, namestring, err[F_OPENDIR], errno);
			return(INCOMPLETE);
		}
		if ((strcmp(ent->d_name, ".") != 0) && 
		    (strcmp(ent->d_name, "..") != 0)) {
			w_error(GENERAL, namestring, err[BADDIRSIZE], 0);
			return(FAIL);
		}
	}
	telval = telldir(dirp);
	/*
	 * Open the dir with open
	 */
	
	if ((fd = cap_open(dirname, O_RDONLY)) < 0)  {
		w_error(SYSCALL, namestring, err[FILEOPEN], errno);
		cap_rmdir(dirname);
		return(INCOMPLETE);
	}
	/*
	 * Try to read past the end.
	 */
	do {
		if ((rval = getdents(fd, rbuf, sizeof(rbuf))) < 0) {
			w_error(SYSCALL, namestring, err[F_READ], errno);
			cap_rmdir(dirname);
			return(INCOMPLETE);
		}
		else
			bytes += rval;
	} while (rval > 0);
	if (bytes > telval || bytes > statbuf.st_size){
		w_error(GENERAL, namestring, err[BADBYTE_BEYOND],0);
		return(FAIL);
	}
	close(fd);
	closedir(dirp);
	return(PASS);
}
