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

#ident	"@(#)libc-port:gen/getut.c	1.19.3.4"

/*	Routines to read and write the /etc/utmp file.			*/
/*									*/
#ifdef __STDC__
	#pragma weak endutent = _endutent
	#pragma weak getutent = _getutent
	#pragma weak getutid = _getutid
	#pragma weak getutline = _getutline
	#pragma weak getutmp = _getutmp
	#pragma weak getutmpx = _getutmpx
	#pragma weak makeut = _makeut
	#pragma weak modut = _modut
	#pragma weak pututline = _pututline
	#pragma weak setutent = _setutent
	#pragma weak synchutmp = _synchutmp
        #pragma weak _synchutmp_internal
	#pragma weak updwtmp = _updwtmp
	#pragma weak utmpname = _utmpname
#endif
#include	"synonyms.h"
#include 	"shlib.h"
#include	<stdio.h>
#include	<sys/types.h>
#include	<sys/stat.h>
#include	<utmpx.h>
#include	<errno.h>
#include	<fcntl.h>
#include	<string.h>
#include	<stdlib.h>
#include	<unistd.h>
#include	<ctype.h>
#include	<signal.h>	/* for prototyping */
#include	<utime.h>
#include 	"getut_internal.h"

#ifdef ut_time
#undef ut_time
#endif

static int updutmpx(const struct utmp *);
static int updutxfile(int, int);
static int updutfile(int, int);

static int fd = -1;	/* File descriptor for the utmp file. */
static int fd_u = -1;	/* File descriptor for the utmpx file. */
static char *utmpfile = UTMP_FILE;	/* Name of the current */
static char *utmpxfile = UTMPX_FILE;	/* "utmp" like file.   */
static last_entry_t last_entry _INITBSSS; /* last entry read. used to detect
					     whether the calling function
					     wants getutline or getutid
					     to skip an entry */


#ifdef ERRDEBUG
static long loc_utmp;	/* Where in "utmp" the current "ubuf" was found.*/
#endif

static buffer_t *u_buffer = NULL;

/*
 * utmp primitives
 */

/*
 * _utmp_buffer_read assumes:
 *  -any file locking has been performed before calling.
 *  -the file descriptor passed as an argument is valid.
 *  -any utmp/utmpx file synching has been performed.
 */
static
struct utmp *_utmp_buffer_read(int my_fd, int read_entries_num)
{
  ssize_t bytes_read;
  int entries_read;

  if (u_buffer == NULL)
    /* calloc the buffer to ensure any current and future flags are zero */
    if ((u_buffer = (buffer_t *)calloc(1, sizeof(buffer_t))) == NULL)
      return NULL;


  if (!u_buffer->f_valid ||
      u_buffer->cur_entry == u_buffer->num_entries) {
    bytes_read =
      read(my_fd, &(u_buffer->entries), read_entries_num*sizeof(struct utmp));
    if (bytes_read <= 0)
      return NULL;
    entries_read = (int)(bytes_read/sizeof(struct utmp));
    if (entries_read <= 0) {
      lseek(my_fd, -bytes_read, SEEK_CUR);
      return NULL;
    } else {
      /* rewind the file pointer if more than one entry was read
	 and a fractional entry was read. */
      if (bytes_read%sizeof(struct utmp) != 0)
	lseek(my_fd, -bytes_read%(ssize_t)sizeof(struct utmp), SEEK_CUR);
      u_buffer->num_entries = entries_read;
      u_buffer->f_valid = 1;
      u_buffer->cur_entry = 0;
    }
  }

  return &(u_buffer->entries[u_buffer->cur_entry++]);
}

#define _utmp_buffer_one_read(my_fd) \
    _utmp_buffer_read(my_fd, 1)

/* pos - used to reposition the file pointer relative to the file location
   of the first entry in the buffer. The offset value is in whole entries. */
/* XXX - getut.c should really have a _utmp_buffer_write() to handle writes
         as expected. */
static
void _utmp_buffer_invalidate(int my_fd,
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
	      offset = -(off_t)sizeof(struct utmp);
	  else
	      offset = 0;
      else /* u_buffer->f_valid == 1 */
	  offset = -(off_t)(u_buffer->num_entries - u_buffer->cur_entry + pos)*
		  (off_t)sizeof(struct utmp);

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

static
int _utmp_open(
	       char *utmp_filename,
	       char *utmpx_filename,
	       int *utmp_fd,
	       int *utmpx_fd)
{
  /*
   * The utmp_filename file must be opened. At a minimum, it must be
   * opened with read-only access. If the utmp_filename file cannot
   * be opened, indicate to the calling function that an error occured
   * as calling functions expect the utmp_filename file to be open.
   */
  if (*utmp_fd < 0 &&
      ((*utmp_fd = open(utmp_filename, O_RDWR)) == -1) &&
      ((*utmp_fd = open(utmp_filename, O_RDONLY)) == -1))
    return -1;

  /*
   * The utmpx_filename file should ideally be opened. Yet,
   * non-privileged users do not typically have read-write access, so
   * don't fail.
   */
  if (*utmpx_fd < 0)
      *utmpx_fd = open(utmpx_filename, O_RDWR);

  /* Make sure files are in synch. If an error occurs
   * because the caller doesn't have write perms to 
   * update the out of sync file, synchutmp returns 2
   * and we continue here. Tis better to return partial
   * info than no info.
   */
  if (*utmpx_fd >= 0 &&
      _synchutmp_internal(*utmp_fd, *utmpx_fd) == 1)
    return -1;

  return 0;
}

static
void _utmp_close(
		 int *utmp_fd,
		 int *utmpx_fd)
{
  if (u_buffer != NULL) {
    free(u_buffer);
    u_buffer = NULL;
  }
  /* unlock both files in case an operation was interrupted by a signal */
  _utmp_unlock(*utmp_fd, LOCK_BEGIN, 0, 0);
  _utmp_unlock(*utmpx_fd, LOCK_BEGIN, 0, 0);

  close(*utmp_fd);
  close(*utmpx_fd);
  *utmp_fd = -1;
  *utmpx_fd = -1;

  if (u_buffer)
     free (u_buffer);
}

/*
 * Functions to lock and unlock the utmp and utmpx files.
 * Only used by gen/getut.c and gen/getutx.c.
 *
 * Read and write locks may be applied to a file descriptor.
 * Each lock may be for a set of entries or the entire file.
 *
 * This fixes the problem reported in incident #338261.
 */

/*
 * _utmp_lock()
 *
 * Locks all or part of a file using a file descriptor.
 *
 * arguments:
 *  fd - file descriptor of file to lock
 *  type - type of lock read (F_RDLCK) or write (F_WRLCK)
 *  whence - basis for offset (LOCK_BEGIN, LOCK_CURRENT, LOCK_END)
 *  offset - file offset where the lock should begin
 *  len - length of lock. If zero (0), then until the end of file.
 */
int
_utmp_lock(int fd, int type, short whence, off_t offset, off_t len)
{
  struct flock lock;
  int rv;

  lock.l_type = type;
  lock.l_whence = whence;
  lock.l_start = offset;
  lock.l_len = len;

  /* Try to acquire lock. Retry if an interrupt occurs. */
  while (((rv = fcntl(fd, F_SETLKW, &lock)) < 0) && (errno == EINTR));
  if (rv == -1)
    return errno;
  else
    return 0;
}


/*
 * _utmp_unlock()
 *
 * Unlocks all or part of a file using a file descriptor.
 *
 * arguments
 *  fd - file descriptor of file to unlock
 *  offset - file offset where the unlock should begin
 *  len - length of the file section to unlock. If zero (0), then
 *        until the end of file.
 */
int
_utmp_unlock(int fd, short whence, off_t offset, off_t len)
{
  struct flock lock;

  lock.l_type = F_UNLCK;
  lock.l_whence = whence;
  lock.l_start = offset;
  lock.l_len = len;

  return fcntl(fd, F_SETLK, &lock);
}





/* "getutent" gets the next entry in the utmp file.
 */

struct utmp *getutent(void)
{
	extern int fd;
	struct utmp *tmp_ptr;

/* If the "utmp" file is not open, attempt to open it for
 * reading.  If there is no file, attempt to create one.  If
 * both attempts fail, return NULL.  If the file exists, but
 * isn't readable and writeable, do not attempt to create.
 */

	if (fd < 0 && _utmp_open(utmpfile, utmpxfile, &fd, &fd_u) == -1)
	  return NULL;
	_utmp_buffer_invalidate(fd, 0);
	tmp_ptr = _utmp_buffer_one_read(fd);
	save_entry_M(tmp_ptr, last_entry);
	return tmp_ptr;
}


/* __getutid() does the work of getutid() but allows the caller to
 * specify whether it is pututline(). pututline()'s caller may have
 * changed the last entry accessed in u_buffer. When pututline() calls
 * getutid(), getutid() thinks it should skip over the current entry
 * when it really shouldn't.
 */
static
struct utmp *__getutid(register const struct utmp *entry,
		       int pututline_calling)
{
        struct utmp *cur_entry;
        struct utmp *ret_val = NULL;
	register short type;

	if (fd < 0 && _utmp_open(utmpfile, utmpxfile, &fd,  &fd_u) == -1)
	  return NULL; /* cannot open file */
	else {
	  /* Invalidate buffer so the first call to _utmp_buffer_read() will
	   * load the buffer with fresh information.
	   *
	   * Some commands and pututline() rely on getutid() checking
           * the entry just read, so reposition the file pointer to the
	   * first expected entry.
	   */
	  _utmp_buffer_invalidate(fd, LSEEK_POS_EXPECTED);

	  _utmp_lock(fd, F_RDLCK, LOCK_CURRENT, 0, 0); /* read lock file from
							  current location
							  to end of file */
	}

	/* Do *not* reread the current entry since the caller has modified
	 * the last entry, indicating we should not reread the entry.
	 *
	 * XXX - The man page does not explicitly say getutid() needs to
	 *       do this, but one could infer this from the man page. Hence,
	 *       this code may not be correct, but this behavior is likely.
	 */
	if (!pututline_calling && has_last_entry_changed_M(last_entry))
	  lseek(fd, sizeof(struct utmp), SEEK_CUR);

	while ((cur_entry = _utmp_buffer_read(fd, READ_MAX)) != NULL) {
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
			        if (((type = cur_entry->ut_type) == INIT_PROCESS
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
			        /* ret_val = NULL; (implicit) */
			        goto done;  
			}
	}

done:
	_utmp_unlock(fd, LOCK_BEGIN, 0, 0); /* unlock the entire file */

	save_entry_M(ret_val, last_entry)

	return ret_val;
}

/*	"getutid" finds the specified entry in the utmp file.  If	*/
/*	it can't find it, it returns NULL.				*/
struct utmp *getutid(const struct utmp *entry)
{
	return __getutid(entry, 0);
}


/* "getutline" searches the "utmp" file for a LOGIN_PROCESS or
 * USER_PROCESS with the same "line" as the specified "entry".
 */

struct utmp *getutline(register const struct utmp *entry)
{
	register struct utmp *cur;

	if (fd < 0 && _utmp_open(utmpfile, utmpxfile, &fd,  &fd_u) == -1)
	  return NULL; /* cannot open file */
	else {
          /* Invalidate buffer so the first call to _utmp_buffer_read() will
           * load the buffer with fresh information.
           *
           * Some commands rely on getutline() checking the entry just read,
	   * so reposition the file pointer to the first expected entry.
           */
          _utmp_buffer_invalidate(fd, LSEEK_POS_EXPECTED);

	  _utmp_lock(fd, F_RDLCK, LOCK_CURRENT, 0, 0); /* read lock file from
							  current location
							  to end of file */
	}

	/* Do *not* reread the current entry since the caller has modified
	 * the last entry, indicating we should not reread the entry.
	 */
	if (has_last_entry_changed_M(last_entry))
	  lseek(fd, sizeof(struct utmp), SEEK_CUR);

	while ((cur = _utmp_buffer_read(fd, READ_MAX)) != NULL) {
/* If the current entry is the one we are interested in, return */
/* a pointer to it. */
		if (cur->ut_type != EMPTY && (cur->ut_type == LOGIN_PROCESS
		    || cur->ut_type == USER_PROCESS) && strncmp(&entry->ut_line[0],
		    cur->ut_line, sizeof(cur->ut_line)) == 0) break;
	}

	_utmp_unlock(fd, LOCK_BEGIN, 0, 0);

	save_entry_M(cur, last_entry)

	return cur;
}


/*	"pututline" writes the structure sent into the utmp file.	*/
/*	If there is already an entry with the same id, then it is	*/
/*	overwritten, otherwise a new entry is made at the end of the	*/
/*	utmp file.							*/

struct utmp *pututline(const struct utmp *entry)
{
	int fc;
	struct utmp *answer;
	extern int fd;
	struct utmp tmpbuf, *savbuf = NULL;
	static struct utmp ret_buf;

	if (entry == NULL) return NULL;

        /*
         * Save the entry in case the user has passed in a pointer to
         * an internal buffer like u_buffer or ret_buf.
         */
	tmpbuf = *entry;

	if (fd < 0 && _utmp_open(utmpfile, utmpxfile, &fd,  &fd_u) == -1) {
#ifdef	ERRDEBUG
		gdebug("pututline: Unable to create utmp file.\n");
#endif
		return((struct utmp *)NULL);
	}
/* Make sure file is writable */
	if ((fc=fcntl(fd, F_GETFL, NULL)) == -1
	    || (fc & O_RDWR) != O_RDWR) {
		return((struct utmp *)NULL);
	}

/* Find the proper entry in the utmp file.  Start at the current */
/* location.  If it isn't found from here to the end of the */
/* file, then reset to the beginning of the file and try again. */
/* If it still isn't found, then write a new entry at the end of */
/* the file.  (Making sure the location is an integral number of */
/* utmp structures into the file incase the file is scribbled.) */

	_utmp_lock(fd, F_WRLCK, LOCK_CURRENT, 0, 0);
	if ((savbuf = __getutid(entry, 1)) == NULL) {
#ifdef	ERRDEBUG
		gdebug("First getutid() failed.  fd: %d",fd);
#endif
		setutent();
		if ((savbuf = __getutid(entry, 1)) == NULL) {
#ifdef	ERRDEBUG
			loc_utmp = lseek(fd, 0L, 1);
			gdebug("Second getutid() failed.  fd: %d loc_utmp: %ld\n",fd,loc_utmp);
#endif
			_utmp_lock(fd, F_WRLCK, LOCK_END, 0,
				   sizeof(struct utmp));
			fcntl(fd, F_SETFL, fc | O_APPEND);
		} else {
		        _utmp_buffer_invalidate(fd, LSEEK_POS_EXPECTED);
		}
	} else {
	        _utmp_buffer_invalidate(fd, LSEEK_POS_EXPECTED);
	}

/* Write out the user supplied structure.  If the write fails, */
/* then the user probably doesn't have permission to write the */
/* utmp file. */
	if (write(fd, &tmpbuf, sizeof(struct utmp)) !=
	    (int)sizeof(struct utmp)) {
#ifdef	ERRDEBUG
		gdebug("pututline failed: write-%d\n",errno);
#endif
		answer = (struct utmp *)NULL;
	} else {
		ret_buf = tmpbuf;
		answer = &ret_buf;	


		/* Tell _utmpx_buffer_invalidate() a write occurred,
		 * thus changing the file position.
		 *
		 * This is kludgy. Maybe the buffering strategy should
		 * change. */
		u_buffer->f_write_occurred= 1;

#ifdef	ERRDEBUG
		gdebug("id: %c%c%c%c loc: %x\n",ubuf.ut_id[0],ubuf.ut_id[1],
		    ubuf.ut_id[2],ubuf.ut_id[3],loc_utmp);
#endif
	}
/* update the parallel utmpx file */
	if (updutmpx(&tmpbuf)) {
		lseek(fd, -(long)sizeof(struct utmp), 1);
		write(fd, savbuf, sizeof(savbuf));
		u_buffer->f_write_occurred = 1; /* set this since updutmp() can
						 * be called even when a write
						 * to utmpx fails. */
		answer = (struct utmp *)NULL;
	}

	_utmp_unlock(fd, LOCK_BEGIN, 0, 0);
	fcntl(fd, F_SETFL, fc);
	return(answer);
}

/*	"setutent" just resets the utmp file back to the beginning.	*/

void
setutent(void)
{
	extern int fd;

	if (fd != -1) lseek(fd, 0, SEEK_SET);

	_utmp_buffer_invalidate(-1, 0);
}

/*	"endutent" closes the utmp file.				*/

void
endutent(void)
{
	extern int fd;

	_utmp_close(&fd, &fd_u);
}

/*	"utmpname" allows the user to read a file other than the	*/
/*	normal "utmp" file.						*/
/* move out of function scope so we get a global symbol for use with data cording */
static int alloclen _INITBSS;

int
utmpname(const char *newfile)
{
	char *ptr;
	int len;

/* Determine if the new filename will fit.  If not, return 0. */
	if ((len = (int)strlen(newfile)) >= MAXFILE) return (0);

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
	(void)strcat(utmpxfile, "x");

/* Make sure everything is reset to the beginning state. */
	endutent();
	return(1);
}

/* "updutmpx" updates the utmpx file. Uses the same
 * search algorithm as pututline to make sure records
 * end up in the same place. 
 */
static int updutmpx(const struct utmp *entry)
{
	int fc, type;
	struct utmpx uxbuf, *uxptr = NULL;
	extern int fd_u;

	/* pututline is the only function calling updumptx(), so
	   it can be assumed the utmpx file is already open */

	if ((fc = fcntl(fd_u, F_GETFL, NULL)) == -1) {
		close(fd_u);
		fd_u = -1;
		return(1);
	}

	_utmp_lock(fd_u, F_RDLCK, LOCK_CURRENT, 0, 0);
	while (read(fd_u, &uxbuf, sizeof(uxbuf)) == (int)sizeof(uxbuf)) {
		if (uxbuf.ut_type != EMPTY) {
			switch (entry->ut_type) {
				case EMPTY:
				    goto done;	
				case RUN_LVL:
				case BOOT_TIME:
				case OLD_TIME:
				case NEW_TIME:
				    if (entry->ut_type == uxbuf.ut_type) {
					uxptr = &uxbuf;
				        goto done;
				    }
				/*FALLTHROUGH*/
				case INIT_PROCESS:
				case LOGIN_PROCESS:
				case USER_PROCESS:
				case DEAD_PROCESS:
				    if (((type = uxbuf.ut_type) == INIT_PROCESS
					|| type == LOGIN_PROCESS
					|| type == USER_PROCESS
					|| type == DEAD_PROCESS)
				      && uxbuf.ut_id[0] == entry->ut_id[0]
				      && uxbuf.ut_id[1] == entry->ut_id[1]
				      && uxbuf.ut_id[2] == entry->ut_id[2]
				      && uxbuf.ut_id[3] == entry->ut_id[3]) {
					uxptr = &uxbuf;
				        goto done;
				    }
			}
		}
	}

done:	
	if (uxptr)
		lseek(fd_u, -(long)sizeof(uxbuf), 1);
	else {
		fcntl(fd_u, F_SETFL, fc|O_APPEND);
		_utmp_lock(fd_u, F_WRLCK, LOCK_END, 0, sizeof(uxbuf));
	}

	/* Determine whether a full or partial update is needed. This is done to
	 * preserve the pid field which differs in size between the utmp and utmpx.
	 */
	if (uxptr && (entry->ut_pid == (short)uxbuf.ut_pid) &&
	    !strncmp(uxptr->ut_user, entry->ut_user, sizeof(entry->ut_user)) &&
	    !strncmp(uxptr->ut_line, entry->ut_line, sizeof(entry->ut_line))) {
	  /* perform partial update */
	  uxbuf.ut_type = entry->ut_type;
	  uxbuf.ut_exit = entry->ut_exit;
	  uxbuf.ut_tv.tv_sec = entry->ut_time;
	  uxbuf.ut_tv.tv_usec = 0;
	} else
	  getutmpx(entry, &uxbuf);
	
	if (write(fd_u, &uxbuf, sizeof(uxbuf)) != (int)sizeof(uxbuf)) {
	        /* Unlock the whole file since the write may have written
		   out part of the data. */
	        _utmp_unlock(fd_u, LOCK_BEGIN, 0, 0);
#ifdef ERRDEBUG
		gdebug("updutmpx failed: write-%d\n", errno);
#endif
		return(1);
	}
	_utmp_unlock(fd_u, LOCK_BEGIN, 0, 0);
	
	fcntl(fd_u, F_SETFL, fc);

	return(0);
}

/*
 * If one of wtmp and wtmpx files exist, create the other, and the record.
 * If they both exist add the record.
 */
void
updwtmp(const char *file, struct utmp *ut)
{
	char filex[MAXFILE+2];
	struct utmpx utx;

	if (strlen(file) > MAXFILE)
		return;
	(void) strcpy(filex, file);
	(void) strcat(filex, "x");
	getutmpx(ut, &utx);
	updwtmpx(filex, &utx);
}


/*
 * "getutmp" - convert a utmpx record to a utmp record.
 */
void
getutmp(const struct utmpx *utx, struct utmp *ut)
{
        (void) strncpy(ut->ut_user, utx->ut_user, sizeof(ut->ut_user));
        (void) strncpy(ut->ut_line, utx->ut_line, sizeof(ut->ut_line));
	(void) memcpy(ut->ut_id, utx->ut_id, sizeof(utx->ut_id));
        ut->ut_pid = (short) utx->ut_pid;
        ut->ut_type = utx->ut_type;
        ut->ut_exit = utx->ut_exit;
        ut->ut_time = utx->ut_tv.tv_sec;
}


/*
 * "getutmpx" - convert a utmp record to a utmpx record.
 */
void
getutmpx(const struct utmp *ut, struct utmpx *utx)
{
        (void) strncpy(utx->ut_user, ut->ut_user, sizeof(ut->ut_user));
	(void) memset(&utx->ut_user[sizeof(ut->ut_user)], '\0',
	    sizeof(utx->ut_user) - sizeof(ut->ut_user));
        (void) strncpy(utx->ut_line, ut->ut_line, sizeof(ut->ut_line));
	(void) memset(&utx->ut_line[sizeof(ut->ut_line)], '\0',
	    sizeof(utx->ut_line) - sizeof(ut->ut_line));
	(void) memcpy(utx->ut_id, ut->ut_id, sizeof(ut->ut_id));
        utx->ut_pid = ut->ut_pid;
        utx->ut_type = ut->ut_type;
        utx->ut_exit = ut->ut_exit;
        utx->ut_tv.tv_sec = ut->ut_time;
        utx->ut_tv.tv_usec = 0;
	utx->ut_session = 0;
	(void) memset(utx->ut_pad, 0, sizeof(utx->ut_pad));
	(void) memset(utx->ut_host, '\0', sizeof(utx->ut_host));
}




/* "synchutmp" make sure utmp and utmpx files are in synch.
 * Returns an error code if the files are not multiples
 * of their respective struct size. Updates the out of 
 * date file.
*/
int
synchutmp(const char *utf, const char *utxf)
{
  int ut_fd, utx_fd;
  int ret_val;

  if ((ut_fd = open(utf, O_RDWR)) < 0)
    if (errno == EACCES)
      return 2;
    else
      return 1;

  if ((utx_fd = open(utxf, O_RDWR)) < 0) {
    close (ut_fd);
    if (errno == EACCES)
      return 2;
    else
      return 1;
  }

  ret_val = _synchutmp_internal(ut_fd, utx_fd);

  close(ut_fd);
  close(utx_fd);

  return ret_val;
}


#define LOCK_TRIES 5

int
/* Most of the code in gen/getut.c and gen/getutx.c calling this
   function already has opened one or both of these files.
   _synchutmp_internal avoids additional system calls and calls faster
   ones like fstat() (vs stat()) and ftruncate() (vs. truncate()). */
_synchutmp_internal(int utf, int utxf)
{
	struct stat stbuf, stxbuf;
	int ret_val;

        /*
         * Acquire write locks for utmp and utmpx. This is pretty coarse
         * grained, but the process calling _synchutmp_internal must ensure no
	 * writers (or readers) for the utmp and utmpx currently exist.
	 * (The code could acquire read locks on both files, but acquiring
	 * write locks appears to speed up this code for particular
	 * situations.) This code ensures both locks are acquired while
	 * avoiding a deadlock.
         *
         * If a non-EDEADLK error occurs, or deadlock occurs more than
	 * LOCK_TRIES times, return 1.
         */
        int errval, i;
        for (i = 0; i < LOCK_TRIES; i++) {
	  /* acquire lock for utmp */
	  if ((errval = _utmp_lock(utf, F_WRLCK, LOCK_BEGIN, 0, 0)) != 0)
	    if (errval != EDEADLK) /* a non-deadlock error occurred */
	      return 1;
	    else
	      sginap(1);
	  /* acquire lock for utmpx */
	  else if ((errval = _utmp_lock(utxf, F_WRLCK, LOCK_BEGIN, 0, 0))
	      != 0) {
	    _utmp_unlock(utf, LOCK_BEGIN, 0, 0);
	    if (errval != EDEADLK) /* a non-deadlock error occurred */
	      return 1;
	    else
	      sginap(1);
	  } else
	    break; /* success! */
        }
	if (i == LOCK_TRIES)
	  return 1;

	if (fstat(utf, &stbuf) == 0 &&
	    fstat(utxf, &stxbuf) == 0) {
		/*
		 * potentially racy, but at least gets us
		 * back to a good utmp file
		 */
		if((stbuf.st_size % (off_t)(sizeof(struct utmp))) != 0) {
			if (ftruncate(utf, (long)(stbuf.st_size -
				(stbuf.st_size % (off_t)(sizeof(struct utmp))))) != 0) {
#if _ABIAPI
					errno = EINVAL;
#else
					setoserror(EINVAL);
#endif
					_utmp_unlock(utf, LOCK_BEGIN, 0, 0);
					_utmp_unlock(utxf, LOCK_BEGIN, 0, 0);
					return(1);
			}
		}
		if((stxbuf.st_size % (off_t)(sizeof(struct utmpx))) != 0) {
			if (ftruncate(utxf, (long)(stxbuf.st_size -
				(stxbuf.st_size % (off_t)(sizeof(struct utmpx))))) != 0) {
#if _ABIAPI
					errno = EINVAL;
#else
					setoserror(EINVAL);
#endif
					_utmp_unlock(utf, LOCK_BEGIN, 0, 0);
					_utmp_unlock(utxf, LOCK_BEGIN, 0, 0);
					return(1);
			}
		}

		ret_val = 0;
		if (stbuf.st_size) {
		  if (!stxbuf.st_size)
		    ret_val = updutxfile(utf, utxf);
		} else if (stxbuf.st_size)
		    ret_val = updutfile(utf, utxf);
				
		if (abs(stxbuf.st_mtime-stbuf.st_mtime) >= MOD_WIN) {
			/* files are out of sync */
			if (stxbuf.st_mtime > stbuf.st_mtime) 
				ret_val = updutfile(utf, utxf);
			else 
				ret_val = updutxfile(utf, utxf);
		}
		_utmp_unlock(utf, LOCK_BEGIN, 0, 0);
		_utmp_unlock(utxf, LOCK_BEGIN, 0, 0);
		return ret_val;
	}
	_utmp_unlock(utf, LOCK_BEGIN, 0, 0);
	_utmp_unlock(utxf, LOCK_BEGIN, 0, 0);
	return(1);
}



/* "updutfile" updates the utmp file using the contents of the
 * umptx file.
 */
static int
updutfile(int fd1, int fd2)
{
	struct utmpx utx;
	struct utmp  ut;

	if (ftruncate(fd1, 0) != 0)
	  return 1;

	while (read(fd2, &utx, sizeof(utx)) == (int)sizeof(utx)) {
		getutmp(&utx, &ut);
		if (write(fd1, &ut, sizeof(ut)) != (int)sizeof(ut)) {
			close(fd1);
			close(fd2);
			return(1);
		}
	}
	return(0);
}

#define compare_key_utmp_utmpx(ut, utx) \
         (ut.ut_pid == (short)utx.ut_pid && \
	  !strncmp(ut.ut_user, utx.ut_user, sizeof(ut.ut_user)) && \
	  !strncmp(ut.ut_id, utx.ut_id, sizeof(ut.ut_id)) && \
	  !strncmp(ut.ut_line, utx.ut_line, sizeof(ut.ut_id)))

/* "updutxfile" updates the utmpx file using the contents of the 
 * utmp file. Tries to preserve the host information as much
 * as possible.
 */
static int
updutxfile(int fd_utmp, int fd_utmpx)
{
	struct utmp  ut;
	struct utmpx utx, utx_tmp;
	int n1, cnt=0, done;
	off_t utmpx_pos_save;
  
	/* Don't truncate the utmpx file.
	   Later code handles this correctly. */
  
	/* As long as the entries match, copy the records from the
	 * utmpx file to keep the host information.
	 */
	while ((n1 = (int)read(fd_utmp, &ut, sizeof(ut))) == (int)sizeof(ut)) {
	        if (read(fd_utmpx, &utx, sizeof(utx)) != (int)sizeof(utx)) 
			break;
		/*
		 * The original AT&T code has several problems:
		 *
		 *   -doesn't handle comparisons between utmp pids
		 *    and utmpx pids gracefully. (The utmp and utmpx
		 *    define a pid differently. The utmp uses a short,
		 *    while the utmpx uses a pid_t, which is a long.)
		 *
		 *   -memcmp() comparisons are incorrect. The original
		 *    comparisons appear to assume the if statement's
		 *    body will be executed if the compare succeeds.
		 *    This is contradictory to the integer compares
		 *    previous to the memcmp()'s. This leads to
		 *    updates which are unnecessary. (The body code
		 *    appears to be executed for almost every entry
		 *    consequently the cnt += sizeof(struct utmpx)
		 *    works properly.)
		 *
		 *   -does not distinguish between partial and full
		 *    updates. It is possible that both of these are
		 *    are needed. The original code will trash fields
		 *    special to the utmpx when only a partial update
		 *    is needed.
		 *
		 * This fixes incident #501830, but the utmp and utmpx
		 * file and record locking probably also solves #501830.
		 */
		cnt += sizeof(struct utmpx);

		if (compare_key_utmp_utmpx(ut, utx))
		  /* case 1: similar utmp and utmpx entries are found in the
		   *         same order in their respective files.
		   */
		  if (ut.ut_type != utx.ut_type ||
		      ut.ut_time != utx.ut_xtime) {
		    /* perform partial update retaining umtpx
		       specific fields */
		    utx.ut_type = ut.ut_type;
		    utx.ut_exit = ut.ut_exit;
		    utx.ut_tv.tv_sec = ut.ut_time;
		    utx.ut_tv.tv_usec = 0;
		  } else /* key entries match so no need to update */
		    continue;
		
		else {
		  /* case 2: similar utmp and utmpx entries are found in
		   *         different order in their respective files, i.e.,
		   *         searching is needed.
		   * case 3: the entry only exists in the utmp.
		   */
		  done = 0;
		  utmpx_pos_save = lseek(fd_utmpx, 0, SEEK_CUR);
		  lseek(fd_utmpx, 0, SEEK_SET); /* rewind utmpx file pointer */
		  
		  while (!done &&
			 read(fd_utmpx, &utx_tmp, sizeof(utx_tmp)) ==
			 (int)sizeof(utx_tmp))
		    if (compare_key_utmp_utmpx(ut, utx_tmp))
		      /* update needed? */
		      if (ut.ut_type != utx_tmp.ut_type ||
			  ut.ut_time != utx_tmp.ut_xtime) {
			lseek(fd_utmpx, -(long)sizeof(struct utmpx), SEEK_CUR);
			if (write(fd_utmpx, &utx_tmp, sizeof(utx))
			    != (int)sizeof(utx_tmp))
			  return(1);
			/* sucessful update so restore file pointer and
			   continue */
			lseek(fd_utmpx, utmpx_pos_save, SEEK_SET);
			done = 1;
		      }
		  
		  if (done)
		    /* finished with this record so continue */
		    continue;
		  else {
		    /* new entry */
		    lseek(fd_utmpx, utmpx_pos_save, SEEK_SET); /* restore fp */
		    getutmpx(&ut, &utx);
		  }
		}

		/* handle writes for in order and new entry updates */
		lseek(fd_utmpx, -(long)sizeof(struct utmpx), 1);
		if (write(fd_utmpx, &utx, sizeof(utx)) !=
		    (int)sizeof(utx))
		  return(1);
	}

	/* out of date file is shorter, copy from the up to date file
	 * to the new file.
	 */
	if (n1 > 0) {
		do {
			getutmpx(&ut, &utx);
			if (write(fd_utmpx, &utx, sizeof(utx)) != (int)sizeof(utx))
				return(1);
		} while ((n1 = (int)read(fd_utmp, &ut, sizeof(ut))) == (int)sizeof(ut));
	} else {
	        /* out of date file was longer, truncate it */
	        ftruncate(fd_utmpx, cnt);
	}
  
  return(0);
}

/*
 * makeut - create a utmp entry, recycling an id if a wild card is
 *	specified.  Also notify init about the new pid
 *
 *	args:	utmp - point to utmp structure to be created
 */


struct utmp *makeut(register struct utmp *utmp)
{
	register int i;			/* scratch variable */
	register struct utmp *utp;	/* "current" utmp entry being examined */
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
 * try to lock the utmp file, only needed if we're doing wildcard matching
 */

                if (lockut())
                        return((struct utmp *) NULL);

                setutent();
		/* find the first alphanumeric character */
		for (i = 0; i < MAXVAL; ++i) {
                        if (isalnum(i))
                                break;
		}
                (void) memset(saveid, i, IDLEN);
		while (utp = getutent()) {
                        if (idcmp(utmp->ut_id, utp->ut_id)) {
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

 			(void) memcpy(utmp->ut_id, utp->ut_id, IDLEN);
			utp = pututline(utmp);
			if (utp)
                                updwtmp(WTMP_FILE, utp);
			endutent();
			unlockut();
			sendpid(ADDPID, (pid_t)utmp->ut_pid);
			return(utp);
		}
		else {

/*
 * nothing available, try to allocate an id
 */

			if (allocid(utmp->ut_id, saveid)) {
                                endutent();
                                unlockut();
                                return((struct utmp *) NULL);
			}
			else {
                                utp = pututline(utmp);
                                if (utp)
                                        updwtmp(WTMP_FILE, utp);
                                endutent();
                                unlockut();
                                sendpid(ADDPID, (pid_t)utmp->ut_pid);
                                return(utp);
			}
		}
	}
	else {
		utp = pututline(utmp);
		if (utp)
			updwtmp(WTMP_FILE, utp);
		endutent();
		sendpid(ADDPID, (pid_t)utmp->ut_pid);
		return(utp);
	}
}


/*
 * modut - modify a utmp entry.	 Also notify init about new pids or
 *	old pids that it no longer needs to care about
 *
 *	args:	utmp - point to utmp structure to be created
 */


struct utmp *modut(register struct utmp *utp)
{
	register int i;				/* scratch variable
*/
	struct utmp utmp;			/* holding area */
	register struct utmp *ucp = &utmp;	/* and a pointer to
it */
	struct utmp *up;			/* "current" utmp entry being examined */

	for (i = 0; i < IDLEN; ++i) {
		if ((unsigned char) utp->ut_id[i] == SC_WILDC)
			return((struct utmp *) NULL);
	}
	/* copy the supplied utmp structure someplace safe */
	utmp = *utp;
	setutent();
	while (up = getutent()) {
		if (idcmp(ucp->ut_id, up->ut_id))
                        continue;
		/* only get here if ids are the same, i.e. found right entry */
           	if (ucp->ut_pid != up->ut_pid) {
                        sendpid(REMPID, (pid_t)up->ut_pid);
                        sendpid(ADDPID, (pid_t)ucp->ut_pid);
		}
                break;
	}
        up = pututline(ucp);
	if (ucp->ut_type == DEAD_PROCESS)
		sendpid(REMPID, (pid_t)ucp->ut_pid);
	if (up)
                updwtmp(WTMP_FILE, up);
	endutent();
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
 * lockut - lock utmp and utmpx files
 */


static int
lockut(void)
{
 	if ((fd = open(UTMP_FILE, O_RDWR)) < 0)
		return(-1);
	if ((fd_u = open(UTMPX_FILE, O_RDWR)) < 0) {
		close(fd);
		fd = -1;
		return(-1);
	}
	if (_synchutmp_internal(fd, fd_u)) {
		close(fd); fd = -1;
		close(fd_u); fd_u = -1;
		return(-1);
	}
		
	if ((lockf(fd, F_LOCK, 0) < 0) || (lockf(fd_u, F_LOCK, 0) < 0)) {
		close(fd); close(fd_u);
		fd = fd_u = -1;
		return(-1);
	}
	return(0);
}


/*
 * unlockut - unlock utmp and utmpx files
 */


static void
unlockut(void)
{
	(void) lockf(fd, F_ULOCK, 0);
	(void) lockf(fd_u, F_ULOCK, 0);
	(void) close(fd);
	(void) close(fd_u);
	fd = -1; fd_u = -1;
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

gdebug(char *format, int arg1, int arg2, int arg3, int arg4, int arg5, int arg6)
{
        register FILE *fp;
        register int errnum;

        if ((fp = fopen("/etc/dbg.getut","a+")) == NULL) return;
        fprintf(fp,format,arg1,arg2,arg3,arg4,arg5,arg6);
        fclose(fp);
}
#endif
