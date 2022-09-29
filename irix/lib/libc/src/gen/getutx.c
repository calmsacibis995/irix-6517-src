/*
 *
 * Copyright 1997, 1998, Silicon Graphics, Inc.
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

/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:gen/getutx.c	1.11"

/*******************************************************************

		PROPRIETARY NOTICE (Combined)

This source code is unpublished proprietary information
constituting, or derived under license from AT&T's UNIX(r) System V.
In addition, portions of such source code were derived from Berkeley
4.3 BSD under license from the Regents of the University of
California.



		Copyright Notice 

Notice of copyright on this source code product does not indicate 
publication.

	(c) 1986,1987,1988,1989  Sun Microsystems, Inc
	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
	          All rights reserved.
********************************************************************/ 

#ident  "$Header: /proj/irix6.5.7m/isms/irix/lib/libc/src/gen/RCS/getutx.c,v 1.17 1998/07/14 13:32:33 bbowen Exp $"

/*	Routines to read and write the /etc/utmpx file.			*/

#ifdef __STDC__
	#pragma weak getutxent = _getutxent
	#pragma weak getutxid = _getutxid
	#pragma weak getutxline = _getutxline
	#pragma weak makeutx = _makeutx
	#pragma weak modutx = _modutx
	#pragma weak pututxline = _pututxline
	#pragma weak setutxent = _setutxent
	#pragma weak endutxent = _endutxent
	#pragma weak utmpxname = _utmpxname
	#pragma weak updutmp = _updutmp
	#pragma weak updwtmpx = _updwtmpx
#endif

#include	"synonyms.h"
#include	<stdio.h>
#include	<sys/types.h>
#include	<sys/stat.h>
#include	<utmpx.h>
#include	<errno.h>
#include	<fcntl.h>
#include	<string.h>
#include	<unistd.h>
#include	<stdlib.h>
#include	<ctype.h>
#include 	<signal.h>	/* for prototyping */
#include	"gen_extern.h"
#define GETUT_EXTENDED 1
#include	"getut_internal.h"

static int fd = -1;	/* File descriptor for the utmpx file. */
static int fd_u = -1;	/* File descriptor for the utmp file. */
static char *utmpxfile = UTMPX_FILE;	/* Name of the current */
static char *utmpfile = UTMP_FILE;	/* "utmpx" and "utmp"  */
					/* like file.          */
static last_entry_t last_entry _INITBSSS;
int __keeputmp_open _INITBSS;		/* keep utmp file open */

#ifdef ERRDEBUG
static long loc_utmp;	/* Where in "utmpx" the current "ubuf" was found.*/
#endif

static buffer_t *u_buffer = NULL;

/*
 * utmpx primitives
 */

/*
 * _utmpx_buffer_read assumes:
 *  -any file locking has been performed before calling.
 *  -the file descriptor passed as an argument is valid.
 *  -any utmp/utmpx file synching has been performed.
 */
static
struct utmpx *_utmpx_buffer_read(int my_fd, int read_entries_num)
{
  ssize_t bytes_read;
  int entries_read;

  if (u_buffer == NULL)
    /* calloc the buffer to ensure any current and future flags are zero */
    if ((u_buffer = (buffer_t *)calloc(1, sizeof(buffer_t)))
	== NULL)
      return NULL;

  if (!u_buffer->f_valid ||
      u_buffer->cur_entry == u_buffer->num_entries) {
    bytes_read =
      read(my_fd, &(u_buffer->entries), read_entries_num*sizeof(struct utmpx));
    if (bytes_read <= 0)
      return NULL;
    entries_read = (int)(bytes_read/sizeof(struct utmpx));
    if (entries_read <= 0) {
      lseek(my_fd, -bytes_read, SEEK_CUR);
      return NULL;
    } else {
      /* rewind the file pointer if more than one entry was read
	 and a fractional entry was read. */
      if (bytes_read%(ssize_t)sizeof(struct utmpx) != 0)
	lseek(my_fd, -bytes_read%(ssize_t)sizeof(struct utmpx), SEEK_CUR);
      u_buffer->num_entries = entries_read;
      u_buffer->f_valid = 1;
      u_buffer->cur_entry = 0;
    }
  }

  return &(u_buffer->entries[u_buffer->cur_entry++]);
}

#define _utmpx_buffer_one_read(my_fd) \
    _utmpx_buffer_read(my_fd, 1);

/* pos - used to reposition the file pointer relative to the file location
   of the first entry in the buffer. The offset value is in whole entries. */
/* XXX - getutx.c should really have a _utmpx_buffer_write() to handle writes
         as expected. */
static
void _utmpx_buffer_invalidate(int my_fd,
			      int pos)
{
  off_t offset;

  if (u_buffer != NULL &&
      (u_buffer->f_valid || u_buffer->f_write_occurred)) {
    /* keep the file pointer coherent with the buffer location */
    if (my_fd != -1) {

      /* Compute offset accordingly */
      if (u_buffer->f_write_occurred)
	  if (pos == LSEEK_POS_EXPECTED)
	      offset = -(off_t)sizeof(struct utmpx);
	  else
	      offset = 0;
      else /* u_buffer->f_valid == 1 */
	  offset = -(off_t)(u_buffer->num_entries - u_buffer->cur_entry + pos)*
	    (off_t)sizeof(struct utmpx);

      /* Reset the file pointer. if this fails because a negative, absolute
       * file position would occur, set the file position pointer to the
       * begining of the file.
       */
      if (offset != 0 &&
	  lseek(my_fd, offset, SEEK_CUR) == -1 &&
	  errno == EINVAL)
	lseek(my_fd, (off_t)0, SEEK_SET);
    }
    u_buffer->f_valid = 0;
    u_buffer->f_write_occurred = 0;
  } 
}

#define _utmpx_lock(fd, type, whence, offset, len) \
    _utmp_lock(fd, type, whence, offset, len)

#define _utmpx_unlock(fd, whence, offset, len) \
    _utmp_unlock(fd, whence, offset, len)

static
int _utmpx_open(
		char *utmpx_filename,
		char *utmp_filename,
		int *utmpx_fd,
		int *utmp_fd)
{
  /*
   * The utmpx_filename file must be opened. At a minimum, it must be
   * opened with read-only access. If the utmpx_filename file cannot
   * be opened, indicate to the calling function that an error occured
   * as calling functions expect the utmpx_filename file to be open.
   */
  if (*utmpx_fd < 0 &&
      ((*utmpx_fd = open(utmpx_filename, O_RDWR)) == -1) &&
      ((*utmpx_fd = open(utmpx_filename, O_RDONLY)) == -1))
    return -1;

  /*
   * The utmp_filename file should ideally be opened. Yet,
   * non-privileged users do not typically have read-write access, so
   * don't fail.
   */
  if (*utmp_fd < 0)
      *utmp_fd = open(utmp_filename, O_RDWR);

  /* Make sure files are in synch. If an error occurs
   * because the caller doesn't have write perms to 
   * update the out of sync file, synchutmp returns 2
   * and we continue here. Tis better to return partial
   * info than no info.
   */
  if (*utmp_fd >=0  &&
      _synchutmp_internal(*utmp_fd, *utmpx_fd) == 1)
    return -1;

  return 0;
}

static
void _utmpx_close(
		  int *utmpx_fd,
		  int *utmp_fd)
{
  if (u_buffer != NULL) {
    free(u_buffer);
    u_buffer = NULL;
  }
  /* Unlock both files in case an operation was interrupted by
   * a signal and the signal handler needs to unlock the utmp
   * and utmpx files. */
  _utmpx_unlock(*utmp_fd, LOCK_BEGIN, 0, 0);
  _utmpx_unlock(*utmpx_fd, LOCK_BEGIN, 0, 0);

  close(*utmpx_fd);
  close(*utmp_fd);
  *utmp_fd = -1;
  *utmpx_fd = -1;

  if (u_buffer)
     free (u_buffer);
}

/* "getutxent" gets the next entry in the utmpx file.
 */

struct utmpx *getutxent(void)
{
	extern int fd;
	struct utmpx *tmp_ptr;

/* If the "utmpx" file is not open, attempt to open it for
 * reading.  If there is no file, attempt to create one.  If
 * both attempts fail, return NULL.  If the file exists, but
 * isn't readable and writeable, do not attempt to create.
 */

	if (fd < 0 && _utmpx_open(utmpxfile, utmpfile, &fd, &fd_u) == -1)
	  return NULL;
	_utmpx_buffer_invalidate(fd, 0);
	tmp_ptr = _utmpx_buffer_one_read(fd);
	save_entry_M(tmp_ptr, last_entry)
	return tmp_ptr;
}


/* __getutxid() does the work of getutxid() but allows the caller to
 * specify whether it is pututxline(). pututxline()'s caller may have
 * changed the last entry accessed in u_buffer. When pututxline() calls
 * getutxid(), getutxid() thinks it should skip over the current entry
 * when it really shouldn't.
 */
static
struct utmpx *__getutxid(const struct utmpx *entry, int pututxline_calling)
{
        register short type;
	struct utmpx *cur_entry, *ret_val = NULL;

	if (fd < 0 && _utmpx_open(utmpxfile, utmpfile, &fd,  &fd_u) == -1)
	  return NULL; /* cannot open file */
	else {
          /* Invalidate buffer so the first call to _utmpx_buffer_read() will
           * load the buffer with fresh information.
           *
           * Some commands and pututxline() rely on getutxid() checking
           * the entry just read, so reposition the file pointer to the
	   * first expected entry.
           */
          _utmpx_buffer_invalidate(fd, LSEEK_POS_EXPECTED);

	  _utmpx_lock(fd, F_RDLCK, LOCK_CURRENT, 0, 0); /* read lock file from
							   current location
							   to end of file */
	}

	/* Do *not* reread the current entry since the caller has modified
	 * the last entry, indicating we should not reread the entry.
	 *
	 * XXX - The man page does not explicitly say getutxid() needs to
	 *       do this, but one could infer this from the man page. Hence,
	 *       this code may not be correct, but this behavior is likely.
	 */
	if (!pututxline_calling && has_last_entry_changed_M(last_entry))
	  lseek(fd, sizeof(struct utmpx), SEEK_CUR);

	while ((cur_entry = _utmpx_buffer_read(fd, READ_MAX)) != NULL) {
			switch(entry->ut_type) {

/* Do not look for an entry if the user sent us an EMPTY entry. */
			case EMPTY:
			        /* ret_val = NULL; (implicit) */
			        goto done;

/* For RUN_LVL, BOOT_TIME, OLD_TIME, and NEW_TIME entries, only */
/* the types have to match.  If they do, return the address of */
/* internal buffer. */
			case RUN_LVL:
			case BOOT_TIME:
			case OLD_TIME:
			case NEW_TIME:
				if (entry->ut_type == cur_entry->ut_type) {
				  ret_val = cur_entry;
				  goto done;
				}
				break;

/* For INIT_PROCESS, LOGIN_PROCESS, USER_PROCESS, and DEAD_PROCESS */
/* the type of the entry in "ubuf", must be one of the above and */
/* id's must match. */
			case INIT_PROCESS:
			case LOGIN_PROCESS:
			case USER_PROCESS:
			case DEAD_PROCESS:
			        type = cur_entry->ut_type;
				if ((type  == INIT_PROCESS
					|| type == LOGIN_PROCESS
					|| type == USER_PROCESS
					|| type == DEAD_PROCESS)
				    && cur_entry->ut_id[0] == entry->ut_id[0]
				    && cur_entry->ut_id[1] == entry->ut_id[1]
				    && cur_entry->ut_id[2] == entry->ut_id[2]
				    && cur_entry->ut_id[3] == entry->ut_id[3]) {
					ret_val = cur_entry;
					goto done;
				}
				break;

/* Do not search for illegal types of entry. */
			default:
			        ret_val = NULL;
				goto done;
			}
	}

done:
	_utmpx_unlock(fd, LOCK_BEGIN, 0, 0); /* unlock the entire file */

	save_entry_M(ret_val, last_entry)

	return ret_val;
}

/*	"getutxid" finds the specified entry in the utmpx file.  If	*/
/*	it can't find it, it returns NULL.				*/
struct utmpx *getutxid(const struct utmpx *entry)
{
	return __getutxid(entry, 0);
}


/* "getutxline" searches the "utmpx" file for a LOGIN_PROCESS or
 * USER_PROCESS with the same "line" as the specified "entry".
 */

struct utmpx *getutxline(const struct utmpx *entry)
{
	struct utmpx *cur_entry;

	if (fd < 0 && _utmpx_open(utmpxfile, utmpfile, &fd,  &fd_u) == -1)
	  return NULL; /* cannot open file */
	else {
	  /* Invalidate buffer so the first call to _utmpx_buffer_read() will
           * load the buffer with fresh information.
           *
           * Some commands rely on getutxline() checking the entry just read,
	   * so reposition the file pointer to the first expected entry.
           */
          _utmpx_buffer_invalidate(fd, LSEEK_POS_EXPECTED);

	  _utmpx_lock(fd, F_RDLCK, LOCK_CURRENT, 0, 0); /* read lock file from
							   current location
							   to end of file */
	}

	/* Do *not* reread the current entry since the caller has modified
	 * the last entry, indicating we should not reread the entry.
	 */
	if (has_last_entry_changed_M(last_entry))
	  lseek(fd, sizeof(struct utmpx), SEEK_CUR);
	
	while ((cur_entry = _utmpx_buffer_read(fd, READ_MAX)) != NULL) {

/* If the current entry is the one we are interested in, return */
/* a pointer to it. */
		if (cur_entry->ut_type != EMPTY &&
		    (cur_entry->ut_type == LOGIN_PROCESS ||
		     cur_entry->ut_type == USER_PROCESS) &&
		    strncmp(&entry->ut_line[0], &cur_entry->ut_line[0],
			    sizeof(cur_entry->ut_line)) == 0)
		  break;
	}

	_utmpx_unlock(fd, LOCK_BEGIN, 0, 0);

	save_entry_M(cur_entry, last_entry)

	return cur_entry;
}

/*	"pututxline" writes the structure sent into the utmpx file.	*/
/*	If there is already an entry with the same id, then it is	*/
/*	overwritten, otherwise a new entry is made at the end of the	*/
/*	utmpx file.							*/

struct utmpx *pututxline(const struct utmpx *entry)
{
	int fc;
	struct utmpx *answer;
	extern int fd;
	struct utmpx tmpxbuf, *savbuf = NULL;
	static struct utmpx ret_buf;

	if (entry == (struct utmpx *) NULL)
		return((struct utmpx *)NULL);

	/*
	 * Save the entry in case the user has passed in a pointer to
	 * an internal buffer like u_buffer or ret_buf.
	 */
	tmpxbuf = *entry;

	if (fd < 0 && _utmpx_open(utmpxfile, utmpfile, &fd,  &fd_u) == -1) {
#ifdef	ERRDEBUG
		gxdebug("pututxline: Unable to create utmpx file.\n");
#endif
		return((struct utmpx *)NULL);
	}

/* Make sure utmpx file is writable */
	if ((fc=fcntl(fd, F_GETFL, NULL)) == -1 ||
	    (fc & O_RDWR) != O_RDWR) {
		return((struct utmpx *)NULL);
	}


/* Find the proper entry in the utmpx file.  Start at the current */
/* location.  If it isn't found from here to the end of the */
/* file, then reset to the beginning of the file and try again. */
/* If it still isn't found, then write a new entry at the end of */
/* the file.  (Making sure the location is an integral number of */
/* utmp structures into the file incase the file is scribbled.) */

	_utmpx_lock(fd, F_WRLCK, LOCK_CURRENT, 0, 0);
	if ((savbuf = __getutxid(&tmpxbuf, 1)) == NULL) {
#ifdef	ERRDEBUG
		gxdebug("First getutxid() failed.  fd: %d",fd);
#endif
		setutxent();
		_utmpx_lock(fd, F_WRLCK, LOCK_CURRENT, 0, 0);
		if ((savbuf = __getutxid(&tmpxbuf, 1)) == NULL) {
#ifdef	ERRDEBUG
			loc_utmp = lseek(fd, 0L, 1);
			gxdebug("Second getutxid() failed.  fd: %d loc_utmp: %ld\n",fd,loc_utmp);
#endif
			_utmpx_lock(fd, F_WRLCK, LOCK_END, 0,
				    sizeof(struct utmpx));
			fcntl(fd, F_SETFL, fc | O_APPEND);
		} else {
		        _utmpx_buffer_invalidate(fd, LSEEK_POS_EXPECTED);
		}
	} else {
	        _utmpx_buffer_invalidate(fd, LSEEK_POS_EXPECTED);
	}


/* Write out the user supplied structure.  If the write fails, */
/* then the user probably doesn't have permission to write the */
/* utmpx file. */
	if (write(fd, &tmpxbuf, sizeof(struct utmpx)) !=
	    (int)sizeof(struct utmpx)) {
#ifdef	ERRDEBUG
		gxdebug("pututxline failed: write-%d\n",errno);
#endif
		answer = (struct utmpx *)NULL;
	} else {
		ret_buf = tmpxbuf;
		answer = &ret_buf;

		/* Tell _utmpx_buffer_invalidate() a write occurred,
		 * thus changing the file position.
		 *
		 * This is kludgy. Maybe the buffering strategy should
		 * change. */
		u_buffer->f_write_occurred= 1;

#ifdef	ERRDEBUG
		gxdebug("id: %c%c loc: %x\n",ubuf->ut_id[0],ubuf->ut_id[1],
		    ubuf->ut_id[2],ubuf->ut_id[3],loc_utmp);
#endif
	}
	if (updutmp(&tmpxbuf)) {
		lseek(fd, -(long)sizeof(struct utmpx), 1);
		write(fd, savbuf, sizeof(savbuf));
		u_buffer->f_write_occurred = 1; /* set this since updutmp() can
						 * be called even when a write
						 * to utmpx fails. */
		answer = (struct utmpx *)NULL;
	}

	_utmpx_unlock(fd, LOCK_BEGIN, 0, 0);
	fcntl(fd, F_SETFL, fc);
	return(answer);
}


/*	"setutxent" just resets the utmpx file back to the beginning.	*/

void
setutxent(void)
{
	extern int fd;

	if (fd != -1) lseek(fd, 0, SEEK_SET);

/* Zero the stored copy of the last entry read, since we are */
/* resetting to the beginning of the file. */

	_utmpx_buffer_invalidate(-1, 0);
}

/*	"endutxent" closes the utmpx file.				*/

void
endutxent(void)
{
	extern int fd;

	_utmpx_close(&fd, &fd_u);
}

/*	"utmpxname" allows the user to read a file other than the	*/
/*	normal "utmpx" file.						*/

/* move out of function scope so we get a global symbol for use with data cording */
static int alloclen _INITBSS;

int
utmpxname(const char *newfile)
{
	char *ptr;
	int len;

	/* Determine if the new filename will fit.  If not, return 0. */
	if ((len = (int)strlen(newfile)) >= MAXFILE) return (0);

	/* The name of the utmpx file has to end with 'x' */
	if (newfile[len-1] != 'x') return(0);

	/* malloc enough space for utmp, utmpx, and null bytes */
	if (len > alloclen)
	{
		if (alloclen) {
			free(utmpfile);
			utmpfile = utmpxfile = 0;
		}
		if ((ptr = (char *)calloc(1, 2 * len + 3)) == 0) {
			alloclen = 0;
			return (0);
		}
		alloclen = len;
		utmpfile = (char *)ptr;
		utmpxfile = (char *)ptr + len + 1;
	}

	/* copy in the new file name. */
	(void)strcpy(utmpfile, newfile);
	(void)strcpy(utmpxfile, newfile);

	/* strip the 'x' */
	utmpfile[len-1] = '\0';

	/* Make sure everything is reset to the beginning state. */
	endutxent();
	return(1);
}

/* "updutmp" updates the utmp file, uses same algorithm as 
 * pututxline so that the records end up in the same spot.
 */
int updutmp(const struct utmpx *entry)
{
	int fc, type;
	struct utmp ubuf, *uptr = NULL;
	extern int fd_u;

	if (fd_u < 0) {
		if ((fd_u = open(utmpfile, O_RDWR)) < 0) {
#ifdef ERRDEBUG
		gxdebug("Could not open utmpfile\n");
#endif
			return(1);
		}
	} else {
		lseek(fd_u,0L,0);
	}

	if ((fc = fcntl(fd_u, F_GETFL, NULL)) == -1) {
		close(fd_u);
		fd_u = -1;
		return(1);
	}

	_utmpx_lock(fd_u, F_WRLCK, LOCK_CURRENT, 0, 0);
	while (read(fd_u, &ubuf, sizeof(ubuf)) == (int)sizeof(ubuf)) {
		if (ubuf.ut_type != EMPTY) {
			switch (entry->ut_type) {
				case EMPTY:
				    goto done;	
				case RUN_LVL:
				case BOOT_TIME:
				case OLD_TIME:
				case NEW_TIME:
				    if (entry->ut_type == ubuf.ut_type) {
					uptr = &ubuf;
				        goto done;
				    }
				/*FALLTHROUGH*/
				case INIT_PROCESS:
				case LOGIN_PROCESS:
				case USER_PROCESS:
				case DEAD_PROCESS:
				    if (((type = ubuf.ut_type) == INIT_PROCESS
					|| type == LOGIN_PROCESS
					|| type == USER_PROCESS
					|| type == DEAD_PROCESS)
				      && ubuf.ut_id[0] == entry->ut_id[0]
				      && ubuf.ut_id[1] == entry->ut_id[1]
				      && ubuf.ut_id[2] == entry->ut_id[2]
				      && ubuf.ut_id[3] == entry->ut_id[3]) {
					uptr = &ubuf;
				        goto done;
				    }
			}
		}
	}

done:
	if (uptr) 
		lseek(fd_u, -(long)sizeof(ubuf), 1);
	else {
		fcntl(fd_u, F_SETFL, fc|O_APPEND);
		_utmpx_lock(fd_u, F_WRLCK, LOCK_END, 0, sizeof(ubuf));
	}

	getutmp(entry, &ubuf);
	
	if (write(fd_u, &ubuf, sizeof(ubuf)) != (int)sizeof(ubuf)) {
	        /* Unlock the whole file since the write may have written
		   out part of the data. */
		_utmpx_unlock(fd_u, LOCK_BEGIN, 0, 0);
		return(1);
	}
	_utmpx_unlock(fd_u, LOCK_BEGIN, 0, 0);
	
	fcntl(fd_u, F_SETFL, fc);

	/*
	 * programs like ftpd need to keep fd's open so that even if
	 * they chroot they can perform utmp accounting
	 */
	if (!__keeputmp_open) {
		close(fd_u);
		fd_u = -1;
	}

	return(0);
}


/*
 * If one of wtmp and wtmpx files exist, create the other, and the record.
 * If they both exist add the record.
 * Provide a mechanism for programs like ftp to keep wtmp files open -
 * they need it to be able to chroot and still update the wtmp files
 */
int __keepwtmps_open = 0;
/* move out of function scope so we get a global symbol for use with data cording */
static int wfd = -1, wfdx = -1;

void
updwtmpx(const char *filex, struct utmpx *utx)
{
	char file[MAXFILE+1];
	struct utmp ut;
	int didopenfd = 0;
	mode_t omask;

	if (strlen(filex) > MAXFILE)
		return;
	(void) strcpy(file, filex);
	file[strlen(filex) - 1] = '\0';	/* remove 'x' */

	if (wfd < 0) {
		didopenfd = 1;
		/* Since O_CREAT is ineffectual when the file already
		   exists, the code can be collapsed into one open call
		   for each file */
		omask = umask(0);
		if ((wfd = open(file, O_WRONLY | O_APPEND | O_CREAT)) < 0) {
		  umask(omask);
		  return;
		}
		if ((wfdx = open(filex, O_WRONLY | O_APPEND | O_CREAT)) < 0) {
		  umask(omask);
		  return;
		}
		umask(omask);
	}

	/*
	 * Both files exist, synch them
	 * Don't worry too much about synch-ing - its possible that we
	 * can't open the files (like ftp when it chroot's)
	 */
	(void)_synchutmp_internal(wfd, wfdx);

	getutmp(utx, &ut);
	_utmpx_lock(wfd, F_WRLCK, LOCK_END, 0, sizeof(struct utmp));
	write(wfd, &ut, sizeof(struct utmp));
	_utmpx_unlock(wfd, LOCK_BEGIN, 0, 0);
	_utmpx_lock(wfdx, F_WRLCK, LOCK_END, 0, sizeof(struct utmpx));
	write(wfdx, utx, sizeof(struct utmpx));
	_utmpx_unlock(wfdx, LOCK_BEGIN, 0, 0);

	if (didopenfd && !__keepwtmps_open) {
		close(wfd);
		close(wfdx);
		wfd = wfdx = -1;
	}
}


/*
 * makeutx - create a utmpx entry, recycling an id if a wild card is *	specified.  Also notify init about the new pid
 *
 *	args:	utmpx - point to utmpx structure to be created
 */


struct utmpx *makeutx(const struct utmpx *utmp)
{
 	register int i;			/* scratch variable */
	register struct utmpx *utp;	/* "current" utmpx entry being examined */
	int wild;			/* flag, true iff wild card
char seen */
	unsigned char saveid[IDLEN];	/* the last id we matched that was
                                           NOT a dead proc */

	wild = 0;
	for (i = 0; i < IDLEN; i++) {
		if ((unsigned char) utmp->ut_id[i] == SC_WILDC) {
			wild = 1;
			break;
		}
	}

	if (wild) {

/*
 * try to lock the utmpx and utmp files, only needed if we're doing
 * wildcard matching
 */

		if (lockutx()) {
			return((struct utmpx *) NULL);
		}

		setutxent();
		/* find the first alphanumeric character */
		for (i = 0; i < MAXVAL; ++i) {
			if (isalnum(i))
                                break;
		}
		(void) memset(saveid, i, IDLEN);
		while (utp = getutxent()) {
			if (idcmp((char *)utmp->ut_id, utp->ut_id)) {
                                continue;
			}
			else {
                                if (utp->ut_type == DEAD_PROCESS) {
                                        break;
                                }
                                else {
                                        (void) memcpy(saveid, utp->ut_id, IDLEN);
                                }
			}
		}
		if (utp) {

/*
 * found an unused entry, reuse it
 */

			(void) memcpy((char *)(utmp->ut_id), utp->ut_id, IDLEN);
			utp = pututxline(utmp);
			if (utp) 
                                updwtmpx(WTMPX_FILE, utp);
			endutxent();
			unlockutx();
			sendpid(ADDPID, (pid_t)utmp->ut_pid);
			return(utp);
		}
		else {

/*
 * nothing available, try to allocate an id
 */

                        if (allocid((char *)utmp->ut_id, saveid)) {
                                endutxent();
                                unlockutx();
                                return((struct utmpx *) NULL);
                        }
                        else {
                              	utp = pututxline(utmp);
                                if (utp) 
                                        updwtmpx(WTMPX_FILE, utp);
                                endutxent();
                                unlockutx();
                                sendpid(ADDPID, (pid_t)utmp->ut_pid);
                                return(utp);
                        }
		}
	}
        else {
              	utp = pututxline(utmp);
		if (utp) 
                        updwtmpx(WTMPX_FILE, utp);
		endutxent();
		sendpid(ADDPID, (pid_t)utmp->ut_pid);
		return(utp);
	}
}


/*
 * modutx - modify a utmpx entry.  Also notify init about new pids or
 *	old pids that it no longer needs to care about
 *
 *	args:	utp- point to utmpx structure to be created
 */

struct utmpx *modutx(const struct utmpx *utp)
{
	register int i;				/* scratch variable
*/
	struct utmpx utmp;			/* holding area */
	register struct utmpx *ucp = &utmp;	/* and a pointer to
it */
	struct utmpx *up;			/* "current" utmpx entry being examined */

	for (i = 0; i < IDLEN; ++i) {
		if ((unsigned char) utp->ut_id[i] == SC_WILDC)
			return((struct utmpx *) NULL);
	}
	/* copy the supplied utmpx structure someplace safe */
	utmp = *utp;
	setutxent();
	while (up = getutxent()) {
		if (idcmp(ucp->ut_id, up->ut_id))
			continue;
		/* only get here if ids are the same, i.e. found right entry */
		if (ucp->ut_pid != up->ut_pid) {
			sendpid(REMPID, (pid_t)up->ut_pid);
			sendpid(ADDPID, (pid_t)ucp->ut_pid);
		}
		break;
	}
	up = pututxline(ucp);
	if (ucp->ut_type == DEAD_PROCESS)
		sendpid(REMPID, (pid_t)ucp->ut_pid);
	if (up)
		updwtmpx(WTMPX_FILE, up);
	endutxent();
	return(up);
}


/*
 * idcmp - compare two id strings, return 0 if same, non-zero if not *
 *	args:	s1 - first id string
 *		s2 - second id string
 */


static int
idcmp(register char *s1, register char *s2)
{
	register int i;		/* scratch variable */

	for (i = 0; i < IDLEN; ++i) {
		if ((unsigned char) *s1 != SC_WILDC && (*s1++ != *s2++))
			return(-1);
	}
	return(0);
}


/*
 * allocid - allocate an unused id for utmp, either by recycling a
 *	DEAD_PROCESS entry or creating a new one.  This routine only *	gets called if a wild card character was specified.
 *
 *	args:	srcid - pattern for new id
 *		saveid - last id matching pattern for a non-dead process
 */


static int
allocid(register char *srcid, register unsigned char *saveid)
{
	register int i;		/* scratch variable */
	int changed;		/* flag to indicate that a new id has been generated */
	char copyid[IDLEN];	/* work area */

	(void) memcpy(copyid, srcid, IDLEN);
	changed = 0;
	for (i = 0; i < IDLEN; ++i) {
		/* if this character isn't wild, it'll be part of the generated id */
		if ((unsigned char) copyid[i] != SC_WILDC)
			continue;
		/* it's a wild character, retrieve the character from the saved id */
		copyid[i] = saveid[i];
		/* if we haven't changed anything yet, try to find a new char to use */
		if (!changed && (saveid[i] < MAXVAL)) {

/*
 * Note: this algorithm is taking the "last matched" id and trying to make
 * a 1 character change to it to create a new one.  Rather than special-case
 * the first time (when no perturbation is really necessary), just don't
 * allocate the first valid id.
 */

			while (++saveid[i] <= MAXVAL) {
                                /* make sure new char is alphanumeric */
                                if (isalnum(saveid[i])) {
                                        copyid[i] = saveid[i];
                                        changed = 1;
                                        break;
                                }
			}
		}
	}
	/* changed is true if we were successful in allocating an id */
	if (changed) {
		(void) memcpy(srcid, copyid, IDLEN);
		return(0);
	}
	else {
		return(-1);
	}
}


/*
 * lockutx - lock utmpx and utmp files
 */

static int
lockutx(void)
{
        if ((fd = open(UTMPX_FILE, O_RDWR)) < 0)
                return(-1);
        if ((fd_u = open(UTMP_FILE, O_RDWR)) < 0) {
                close(fd);
                fd = -1;
                return(-1);
        }
	if (_synchutmp_internal(fd_u, fd)) {
                close(fd); fd = -1;
                close(fd_u); fd_u = -1;
                return(-1);
        } 
	if ((lockf(fd, F_LOCK, 0) < 0) || (lockf(fd_u, F_LOCK, 0) <0)) {
		close(fd); close(fd_u);
		fd = -1; fd_u = -1;
		return(-1);
	}
	return(0);
}


/*
 * unlockutx - unlock utmp and utmpx files
 */

static void
unlockutx(void)
{
	(void) lockf(fd, F_ULOCK, 0);
	(void) lockf(fd_u, F_ULOCK, 0);
	(void) close(fd);
	(void) close(fd_u);
	fd = fd_u = -1;
}


/*
 * sendpid - send message to init to add or remove a pid from the
 *	"godchild" list
 *
 *	args:	cmd - ADDPID or REMPID
 *		pid - pid of "godchild"
 */


static void
sendpid(int cmd, pid_t pid)
{
	int pfd;		/* file desc. for init pipe */
	struct pidrec prec;	/* place for message to be built */

/*
 * if for some reason init didn't open .initpipe, open it read/write
 * here to avoid sending SIGPIPE to the calling process
 */

	pfd = open(IPIPE, O_RDWR);
	if (pfd < 0)
		return;
	prec.pd_pid = pid;
	prec.pd_type = cmd;
	(void) write(pfd, &prec, sizeof(struct pidrec));
	(void) close(pfd);
}


#ifdef  ERRDEBUG
#include        <stdio.h>

gxdebug(char *format, int arg1, int arg2, int arg3, int arg4, int arg5, int arg6)
{
        register FILE *fp;
        register int errnum;

        if ((fp = fopen("/etc/dbg.getut","a+")) == NULL) return;
        fprintf(fp,format,arg1,arg2,arg3,arg4,arg5,arg6);
        fclose(fp);
}
#endif
