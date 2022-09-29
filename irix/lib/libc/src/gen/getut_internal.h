/*
 *
 * Copyright 1998, Silicon Graphics, Inc.
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

#ident  "$Header: /proj/irix6.5.7m/isms/irix/lib/libc/src/gen/RCS/getut_internal.h,v 1.3 1998/07/14 13:32:33 bbowen Exp $"

#include <stdio.h>
#include <utmpx.h>
#include <string.h>

/*
 * Macro constants
 */
/* SGI */
 /* _utmp_lock() and _utmp_unlock()'s whence argument */
 #define LOCK_BEGIN 0    /* lock file with offset from beginning of file */
 #define LOCK_CURRENT 1  /* lock file with offset from current file position */
 #define LOCK_END 2      /* lock file with offset from end of file */

 /* Buffering */
 #define BUFFER_MAX_ENTRIES 20
 #define READ_MAX BUFFER_MAX_ENTRIES

 /* _utmp_buffer_invalidate() and _utmpx_buffer_invalidate()'s pos argument */
 #define LSEEK_POS_EXPECTED 1 /* expected position for getut{,x}{id,line} */

 #if defined(GETUT_EXTENDED)
 #define ENTRY_STRUCT struct utmpx
 #else
 #define ENTRY_STRUCT struct utmp
 #endif

/* AT&T */
 #define IDLEN	4	/* length of id field in utmp */
 #define SC_WILDC	0xff	/* wild char for utmp ids */
 #define	MAXFILE	79	/* Maximum pathname length for "utmpx" file */

 # define MAXVAL 255	/* max value for an id `character' */
 # define IPIPE	"/etc/.initpipe"	/* FIFO to send pids to init */

 /* pd_type's */
 # define ADDPID 1	/* add a pid to "godchild" list */
 # define REMPID 2	/* remove a pid to "godchild" list */

 #ifdef	DEBUG
 #undef	UTMPX_FILE
 #define	UTMPX_FILE "utmpx"
 #undef	UTMP_FILE
 #define	UTMP_FILE "utmp"
 #endif

/*
 * Macros
 */
/* SGI */
 /* Macros to save and test the last entry read. See the notes section of
  * getut(3c) and getutx(3c) man pages. */
 #define save_entry_M(_src_, _dest_) \
        if (_src_ != NULL) { \
	  _dest_.entry = *_src_; \
	  _dest_.valid = 1; \
	} else \
	  _dest_.valid = 0;
 #define has_last_entry_changed_M(_last_) \
        ((u_buffer != NULL) && \
	 (_last_.valid == 1) && \
         memcmp(&(_last_.entry), \
		&(u_buffer->entries[u_buffer->cur_entry-1]), \
		sizeof(ENTRY_STRUCT)))


/*
 * Structure definitions
 */
/* SGI */
 /* Used by getutxline() and getutline() to determine if the last entry read
  * has changed*/
 typedef struct last_entry_s {
	int valid;
 #if defined(GETUT_EXTENDED)
	struct utmpx entry;
 #else
	struct utmp entry;
 #endif
 } last_entry_t;

 /* User database entry buffer */
 typedef struct buffer_s {
 #if defined(GETUT_EXTENDED)
	struct utmpx entries[BUFFER_MAX_ENTRIES];
 #else
	struct utmp entries[BUFFER_MAX_ENTRIES];
 #endif
	int num_entries;
	int cur_entry;
	struct flags_s {
		unsigned int valid:1;
		unsigned int write_occurred;
	} flags;
 } buffer_t;
 #define f_valid flags.valid
 #define f_write_occurred flags.write_occurred


/* AT&T */
/*
 * format of message sent to init
 */

struct	pidrec {
	int	pd_type;	/* command type */
	pid_t	pd_pid;		/* pid */
};



/*
 * Function prototypes
 */
/* SGI */
int _utmp_lock(int, int, short, off_t, off_t);
int _utmp_unlock(int, short, off_t, off_t);
int _synchutmp_internal(int, int);

/* AT&T */
static int idcmp(char *, char*);
static int allocid(char *, unsigned char *);
#if defined(GETUT_EXTENDED)
static int lockutx(void);
static void unlockutx(void);
#else
static int lockut(void);
static void unlockut(void); 
#endif
static void sendpid(int,pid_t);
