/*
 * Copyright 1990, 1991, Silicon Graphics, Inc. 
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

/*
 * satd	-	reliably preserve the system audit trail
 */

#ident  "$Revision: 1.30 $"

#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sat.h>
#include <sys/syssgi.h>
#include <sys/time.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <syslog.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <getopt.h>
#include <pwd.h>
#include <sys/capability.h>

#ifndef	TRUE
#define	TRUE	1
#define FALSE	0
#endif  /* TRUE */

#define	BUFLEN			  0x2000	/* 8 K */
#define	SATD_MAX_DIRF_SZ	0x400000	/* 4 Megabytes */
#define	SATD_MIN_DIRF_SZ	 0x80000	/* 512 K */

#define	SATD_MAX_INTR_PER_IO	8

#define EMERGENCY_RESERVE_SIZE	250000		/* 250 K of reserve space */
#define EMERGENCY_RESERVE_FILE	"/sat/satd.reserve"
#define EMERGENCY_FILE_MASK	"/sat/satd.emergency-%d"

int	emergency_fd = -1;

typedef struct  list_t {
	struct  list_t *        next;
	struct  list_t *        prev;
	int                     datum;
}       list_t;

typedef	struct	path_t {
	enum	{
		path_err,		/* output path spec is in error      */
		path_dir,		/* output path spec is a directory   */
		path_dev,		/* output path spec is a device      */
		path_fil,		/* output path spec is a reg. file   */
		path_nul		/* output path spec is /dev/null     */
	}	path_dscr;	/* describes output - dir,dev,fil,nul*/
	int		path_fd;	/* file descriptor for path          */
	char *		path_name;	/* name of path from command line    */
	char 		path_ext [16];	/* extension to path name for dirs   */
	int		path_max_size;	/* maximum bytes path can hold       */
	off64_t		path_size;	/* number of bytes currently in path */
	off64_t		path_room;	/* fstatfs again after room consumed */
	int		path_warn;	/* boolean, 1 if full warning issued */
	struct stat64   *path_stat;	/* stat info on path inode	     */
	struct statvfs64 path_statfs;	/* file system stat info for path    */
}	path_t;

typedef	enum	opt_e	{
	opt_path,			/* option specifies an output path   */
	opt_input,			/* option to use standard input	     */
	opt_output,			/* option to copy to std output	     */
	opt_firstpath,			/* prior to replacement use 1st path */
	opt_onepass,			/* optional replacement algorithm    */
	opt_rotation,			/* optional replacement algorithm    */
	opt_preference,			/* optional replacement algorithm    */
	opt_verbose,			/* option to print annoying chatter  */
	opt_valuecontext,		/* option to add context to stream   */
	opt_oneshot			/* option to sample trail only once  */
}	opt_e;			/* command line options		     */

#define	OPT_PATH		(1 << opt_path)
#define	OPT_INPUT		(1 << opt_input)
#define	OPT_OUTPUT		(1 << opt_output)
#define	OPT_FIRSTPATH		(1 << opt_firstpath)
#define	OPT_ONEPASS		(1 << opt_onepass)
#define	OPT_ROTATION		(1 << opt_rotation)
#define	OPT_PREFERENCE		(1 << opt_preference)
#define	OPT_VERBOSE		(1 << opt_verbose)
#define	OPT_ONESHOT		(1 << opt_oneshot)

/* function prototype definitions */
void	 satd_usage (void);
void	 satd_exit (char *, double);
path_t * satd_parse_pathname (char *);
void	 satd_pick_pathname (list_t *, char *);
void	 satd_show_paths (list_t *);
void	 satd_show_parms (list_t *, int);
int	 satd_send (list_t *, char *, int, int);
list_t * satd_replace_path (list_t *, list_t *, int);
int	 satd_open_path (path_t *);
void	 satd_close_path (path_t *);
int	 satd_path_full (path_t *);
int	 satd_open_dirf (path_t *);
int	 satd_dirf_full (path_t *);
void	 satd_close_dirf (path_t *);
void	 satd_sgnl (int);

/* globals */
int	satd_signalled = 0;
int	satd_signo = 0;
int	select_opts = 0;
int	cap_enabled;
uid_t	ruid;
uid_t	auditor_uid;

cap_t
acquire_audit_control (void)
{
	cap_t ocap = NULL;
	const cap_value_t cap_audit_control = CAP_AUDIT_CONTROL;

	if (cap_enabled)
		ocap = cap_acquire (1, &cap_audit_control);
	else
		(void) setreuid(auditor_uid, ruid);
	return (ocap);
}

void
relinquish_audit_control (cap_t ocap)
{
	if (cap_enabled)
		cap_surrender (ocap);
	else
		(void) setreuid(ruid, auditor_uid);
}

void
satd_sgnl (int sig)
{
	if (sig == SIGHUP || sig == SIGTERM) {
		satd_signalled = 1;
		satd_signo = sig;
		(void) signal (sig, satd_sgnl);
		return;
	}
	/* SIGBUS or SIGSEGV */
	if (sig == SIGBUS)
		syslog(LOG_ALERT, "satd died with bus error!");
	if (sig == SIGSEGV)
		syslog(LOG_ALERT, "satd died with segmentation violation!");

	chdir("/");
	kill(0, SIGQUIT); /* dump core */
}

/*
 * satd_usage -	display usage string, invoked when options are used improperly
 */
void
satd_usage (void) {
	fprintf (stderr, "usage: satd [-f path ...] [-i] [-o] ");
	fprintf (stderr, "[-r replacement-mode] [-v] [-1]\n");
	fprintf (stderr, "        -f and/or -o option must be specified\n");
	syslog(LOG_ERR, "usage error: exiting.");
	exit (1);
}


/*
 * satd_exit -	give some information about the reason for a failure exit
 */
void
satd_exit (char * errstring, double location)
{
	static int already_called_exit = 0;
	FILE *fd;
	int amt_read;
	char buffer[BUFLEN];
	int i;

	if (location)
		syslog (LOG_ALERT, errstring, location);
	else
		syslog (LOG_ALERT, errstring);

	if (already_called_exit) {
		syslog (LOG_EMERG,
		    "Satd recovery failure!  System will probably hang soon.");
		exit(1);
	}

	already_called_exit = 1;

	syslog (LOG_EMERG,
	    "all output paths full -- system shutdown in 10 seconds!");

	/* write all users */
	if ((fd = popen("/etc/wall","w")) != NULL) {
		fprintf(fd, "Warning! All audit output paths full -- %s",
			"system shutdown in 10 seconds!\n");
		pclose(fd);
	}

	/* shut down system to single-user mode */

	switch(fork()) {
	case 0:
		/* Child: give users a whole ten seconds */
		sleep (10);
		/*
		 * init s requires stdin be a terminal, so give it
		 * the console.
		 */
		(void) close(0);
		(void) open("/dev/console",O_RDONLY);
		(void) execl("/etc/init", "init", "s", 0);
		satd_exit("/etc/init exec failure: %m", 0.0);
		break;
	case -1:
		satd_exit("fork failure: %m", 0.0);
		break;
	}

	/*
	 * Parent: switch to emergency file
	 */

	/* free pre-allocated space */
	(void) unlink(EMERGENCY_RESERVE_FILE);

	for (i=0; i < 10; i++) {
		char emergency_file[MAXPATHLEN];

		sprintf(emergency_file, EMERGENCY_FILE_MASK, i);
		emergency_fd = open(emergency_file,
				    O_WRONLY | O_CREAT | O_EXCL | O_SYNC, 0600);
		if (emergency_fd >= 0 || errno != EEXIST)
			break;
	}

	if (emergency_fd < 0 || sat_write_filehdr(emergency_fd) < 0)
		satd_exit("Can't open reserve file: %m", 0.0);

	for (;;) {
		if (OPT_INPUT & select_opts)
			amt_read = read (0, buffer, sizeof(buffer));
		else {
			cap_t ocap = acquire_audit_control();
			amt_read = satread (buffer, sizeof(buffer));
			relinquish_audit_control(ocap);
		}
		if (amt_read < 0)
			satd_exit("Can't read records: %m", 0.0);

		if (write(emergency_fd, buffer, amt_read) < 0)
			satd_exit("Can't write records: %m", 0.0);
	}

	/* NOTREACHED */
}


/*
 * satd_parse_pathname - test pathname for validity, determine (dir|dev|file)
 */
path_t *
satd_parse_pathname (char * name_p)
{
	path_t *	path_p;
	struct stat64	dev_null_stat;
	static ino64_t	dev_null_inode_num = 0;
	static dev_t	dev_null_dev_num = 0;

	/* get inode number for /dev/null, if it isn't already known	     */
	/* /dev/null is treated as a special case, unlike other char devices */
	if (!dev_null_inode_num) {
		if (-1 == stat64("/dev/null", &dev_null_stat)) {
			syslog (LOG_ERR, "can't stat /dev/null");
		}
		else {
			dev_null_inode_num = dev_null_stat.st_ino;
			dev_null_dev_num = dev_null_stat.st_dev;
		}
	}

	/* obtain file system stats for the path */
	path_p = malloc (sizeof (*path_p));
	path_p->path_name = name_p;
	path_p->path_stat = malloc (sizeof (*path_p->path_stat));
	if (-1 == stat64 (name_p, path_p->path_stat)) {
		int fd;
		fd = creat(path_p->path_name, 0600);
		if (fd < 0) {
			syslog (LOG_ERR, "attempt to create %s failed: %m",
				path_p->path_name);
			path_p->path_dscr = path_err;
		} else {
			syslog (LOG_INFO, "creating file %s",
				path_p->path_name);
			(void) stat64(path_p->path_name, path_p->path_stat);
			close(fd);
			path_p->path_dscr = path_fil;
		}
		return (path_p);
	}

	/* determine the type of path */
	switch (S_IFMT & (path_p->path_stat->st_mode)) {
	case S_IFDIR:
		path_p->path_dscr = path_dir;
		break;
	case S_IFCHR:
		if (path_p->path_stat->st_ino == dev_null_inode_num &&
		    path_p->path_stat->st_dev == dev_null_dev_num) {
			path_p->path_dscr = path_nul;
			break;
		}
		path_p->path_dscr = path_dev;
		break;
	case S_IFREG:
		path_p->path_dscr = path_fil;
		break;
	default:
		path_p->path_dscr = path_err;
		syslog(LOG_ERR, "ignoring path: %s", path_p->path_name);
		fprintf (stderr, "\tpath is neither directory, ");
		fprintf (stderr, "character special device, nor disk file\n");
	}

	return (path_p);
}


/*
 * parse paths from an ascii list of path names
 */
void
satd_pick_pathname (list_t * head_p, char * argu_p)
{
	path_t *	path_p;
	char *          name_p;
	list_t *        new_item_p;
	const char *	pattern = ", \t";

	if (!head_p->next) {    /* initialize list if not yet done           */
		head_p->next  = head_p;
		head_p->prev  = head_p;
		head_p->datum = NULL;
	}
	name_p = strtok (argu_p, pattern);

	while (name_p) {
		path_p = satd_parse_pathname (name_p);
		if (path_err == path_p->path_dscr) {
			/* fprintf (stderr, "satd: bad path: %s\n", name_p); */
			name_p = strtok (0, pattern);
			continue;
		}

		name_p = strtok (0, pattern);
		new_item_p = malloc (sizeof (*new_item_p));
		new_item_p->next = head_p;
		new_item_p->prev = head_p->prev;
		head_p->prev->next = new_item_p;
		head_p->prev = new_item_p;
		new_item_p->datum = (int) path_p;
	}
}


/*
 * satd_show_paths - display output paths for verbose user
 */
void
satd_show_paths (list_t * path_list_p)
{
	list_t *	curs;
	path_t *	path;

	fprintf (stderr, "Path list:\n");
	curs = path_list_p -> next;
	if (curs == NULL) {
		fprintf(stderr, "\t(none)\n");
		return;
	}
	while (NULL != (path = (path_t *) curs->datum)) {
		fprintf (stderr, "\tPath = %s\n", path->path_name);
		switch (path->path_dscr) {
		case path_err:
			fprintf (stderr, "\t\t(Error in specification)\n");
			continue;
		case path_dir:
			fprintf (stderr, "\t\t(directory)\n");
			break;
		case path_dev:
			fprintf (stderr, "\t\t(device)\n");
			break;
		case path_fil:
			fprintf (stderr, "\t\t(regular file)\n");
			break;
		case path_nul:
			fprintf (stderr, "\t\t(bit bucket)\n");
			break;
		default:
			satd_exit ("satd_show_paths internal error", 0.0);
		}
		curs = curs -> next;
	}
}


/*
 * satd_show_parms - display command line parameters for verbose user
 */
void
satd_show_parms (list_t * path_list_p, int select_opts)
{
	fprintf (stderr, "\n");
	satd_show_paths (path_list_p);
	fprintf (stderr, "\n");

	if (OPT_INPUT		& select_opts)
		fprintf (stderr, "Input comes from std input.\n");
	else
		fprintf (stderr, "Input comes from sat subsys.\n");

	if (OPT_OUTPUT		& select_opts)
		fprintf (stderr, "Output goes to std output.\n");
	else
		fprintf (stderr, "Output goes to archive only.\n");

	fprintf (stderr, "Path replacement algorithm = ");

	if (OPT_ONEPASS		& select_opts)
		fprintf (stderr, "onepass.\n");

	if (OPT_PREFERENCE	& select_opts)
		fprintf (stderr, "preference.\n");

	if (OPT_ROTATION		& select_opts)
		fprintf (stderr, "rotation.\n");

	if (OPT_VERBOSE		& select_opts)
		fprintf (stderr, "Messages are verbose.\n");
	else
		fprintf (stderr, "Messages are terse.\n");

	fprintf (stderr, "\n");
}


/*
 * satd_replace_path -	find next openable output path using user specified
 * 			replacement policy
 */
list_t *
satd_replace_path (list_t * path_list_p, list_t * curr_path_p, int replacement)
{
	list_t *	test_path_p;

	if (OPT_FIRSTPATH & replacement) {
		test_path_p = path_list_p->next;
		if (satd_open_path ((path_t *) test_path_p->datum))
			return (test_path_p);
		/* else find next path with user chosen replacement policy   */
	}

	if (OPT_ONEPASS & replacement) {
		test_path_p = curr_path_p;
		for (;;) {
			test_path_p = test_path_p->next;
			if (NULL == test_path_p->datum)
				satd_exit("Onepass path search complete", 0.0);
			if (satd_open_path ((path_t *) test_path_p->datum))
				return (test_path_p);
		}
	}

	if (OPT_PREFERENCE & replacement) {
		test_path_p = path_list_p;
		for (;;) {
			test_path_p = test_path_p->next;
			if (NULL == test_path_p->datum)
				satd_exit("Preference path search fails", 0.0);
			if (satd_open_path ((path_t *) test_path_p->datum))
				return (test_path_p);
		}
	}

	if (OPT_ROTATION & replacement) {
		test_path_p = curr_path_p;
		for (;;) {
			test_path_p = test_path_p->next;
			if (NULL == test_path_p->datum)
				test_path_p = test_path_p->next;
			if (test_path_p->datum == curr_path_p->datum)
				satd_exit ("Rotation path search fails", 0.0);
			if (satd_open_path ((path_t *) test_path_p->datum))
				return (test_path_p);
		}
	}

	satd_exit ("Internal error", 0.0);
	/* NOTREACHED */
}


/*
 * satd_path_full
 * Return 1 if no more data can be written to path, else 0
 */
int
satd_path_full (path_t *path_p)
{
	struct statvfs64 *path_fs_p;
	off64_t		 free_space;	/* Calculated bytes free */
	int		 fullness;	/* percent full */

	/*
	 * path_room tells how much room is left to fill before free space
	 * must be checked again
         */

	if (path_nul != path_p->path_dscr && 0 <= path_p->path_room) {

		path_fs_p = &(path_p->path_statfs);

		/* obtain space availability info on path */
		if (0 > fstatvfs64(path_p->path_fd, path_fs_p)){
			syslog (LOG_ERR,
			   "can't fstatvfs64: %s",path_p->path_name);
			return (TRUE);
		}

		free_space = path_fs_p->f_bsize * path_fs_p->f_bfree;

		/* path completely full */
		if (BUFLEN >= free_space)
			return (TRUE);

		/* determine how soon free space must be checked again       */
		path_p->path_room =
			(MIN(SATD_MAX_DIRF_SZ, free_space/2)) / 16 - BUFLEN;
		if (path_p->path_room < 0)
			path_p->path_room = free_space/4 - BUFLEN;

		/* path approaching fullness and never warned before */
		fullness = 100-((100*path_fs_p->f_bfree)/path_fs_p->f_blocks);
		if ((!path_p->path_warn) && (90 <= fullness)) {
			path_p->path_warn = TRUE;
			syslog (LOG_WARNING, "path %3d percent full: %s",
			   fullness, path_p->path_name);
		}
	}

	return (FALSE);
}


/*
 * satd_dirf_full -	check whether the directory file is large enough
 */
int
satd_dirf_full (path_t *path_p)
{
	return ((path_p->path_size >= path_p->path_max_size) ? TRUE : FALSE);
}


/*
 * satd_open_dirf -	invent a unique name for a file in this directory;
 *			attempt to create the newly named file; return success
 */
int
satd_open_dirf (path_t * path_p)
{
	time_t		 now;
	char		 dirf_name[MAXPATHLEN];	/* name of file in this dir  */
        struct statvfs64 *path_fs_p;
        struct stat64	 statbuf;
	off64_t		 free_space;

	/*
	 * Form a unique file name out of year, month, day, hour, & minute
	 * There's a (small) chance the file already exists, in which case
	 * The minute is incremented.
	 */
	now = time(0);
	cftime(path_p->path_ext, "sat_%y%m%d%H%M", &now);
	sprintf(dirf_name, "%s/%s", path_p->path_name, path_p->path_ext);

	while (stat64(dirf_name, &statbuf) >= 0) {
		/*
		 * Since the file exists, use the next minute.
		 * XXX: There's no guarantee this'll ever finish.
		 */
		now += 60;
		cftime(path_p->path_ext, "sat_%y%m%d%H%M", &now);
		sprintf(dirf_name,"%s/%s", path_p->path_name, path_p->path_ext);
	}
	/*
	 * Attempt to open the newly named unique file
	 */
	path_p->path_fd = open(dirf_name, O_EXCL|O_CREAT|O_RDWR, 0600);

	if (path_p->path_fd < 0) {
		syslog(LOG_WARNING, "can't open %s", dirf_name);
		return (FALSE);
	}

	if (sat_write_filehdr(path_p->path_fd) < 0) {
		syslog(LOG_WARNING, "can't write header to %s", dirf_name);
		return (FALSE);
	}

        /*
	 * Obtain space availability info on path
	 */
        path_fs_p = & (path_p->path_statfs);
        if (0 > fstatvfs64(path_p->path_fd, path_fs_p)){
                syslog (LOG_ERR, "can't fstatfs: %s", path_p->path_name);
		path_p->path_max_size = SATD_MAX_DIRF_SZ;
	} else {
        	free_space = path_fs_p->f_bsize * path_fs_p->f_bfree;
		path_p->path_max_size = MIN (SATD_MAX_DIRF_SZ, free_space/2);
		if (path_p->path_max_size < SATD_MIN_DIRF_SZ)
			path_p->path_max_size = SATD_MIN_DIRF_SZ;
	}

	path_p->path_size = 0;
	path_p->path_warn = FALSE;
	return (TRUE);
}


/*
 * satd_close_dirf -	close directory file because it's full
 */
void
satd_close_dirf (path_t * path_p)
{
	(void) sat_close_filehdr(path_p->path_fd);
	(void) close (path_p->path_fd);

	syslog (LOG_NOTICE, "closing directory file %s/%s",
		path_p->path_name, path_p->path_ext);
}


/*
 * satd_open_path -	attempt to open the indicated path; return success
 */
int
satd_open_path (path_t * path_p)
{
	if (!path_p) {
		syslog (LOG_DEBUG, "null path pointer");
		return (FALSE);
	}

	switch (path_p->path_dscr) {
	case path_dir:
		if (satd_open_dirf (path_p) == FALSE)
			return FALSE;
		break;
	case path_dev:
		/* not supported 'cause I can't figure out yet how to	     */
		/* tell how big a tape is in advance of filling it up	     */
		syslog (LOG_ERR, "satd device output not supported yet");
		return (FALSE);
	case path_fil:
	case path_nul:
		path_p->path_fd = open (path_p->path_name,
					O_RDWR|O_CREAT|O_TRUNC, 0600);
		if (path_p->path_fd < 0
		    || sat_write_filehdr(path_p->path_fd) < 0) {
			syslog (LOG_WARNING, "can't open %s",
				path_p->path_name);
			return (FALSE);
		}
		if (path_p->path_dscr == path_nul)
			syslog (LOG_WARNING, "discarding audit records");
		break;
	default:
		return (FALSE);
	}

	path_p->path_warn = FALSE;
	syslog (LOG_INFO, "opening path %s", path_p->path_name);

	/* checking path fullness has the side effect of setting path_room   */
	path_p->path_room = 0;
	if (satd_path_full (path_p))
		syslog (LOG_DEBUG, "opened full path %s", path_p->path_name);

	return (TRUE);
}


/*
 * satd_close_path -	close file, device, or directory because it's full
 */
void
satd_close_path (path_t * path_p)
{
	(void) sat_close_filehdr(path_p->path_fd);
	(void) close (path_p->path_fd);

	syslog (LOG_NOTICE, "closing path %s", path_p->path_name);
}


/*
 * satd_send - send the audit buffer to the best available user specified path
 */
int
satd_send (list_t * path_list_p, char * buffer, int bufsize, int opts)
{
	static list_t * current	= NULL;		/* pt to ptr to current path */

	path_t *	path_p;			/* path currently in use     */
	int		amt_writ;		/* number of bytes output    */

	/* if no path has ever been opened, open the first one               */
	if (!current)
		current = path_list_p;
	path_p = (path_t *) current->datum;
	if (!path_p) {
		current = satd_replace_path (path_list_p, current,
					     (OPT_FIRSTPATH | opts));
		path_p = (path_t *) current->datum;
	}

	/* on receipt of a sighup, try to switch output paths		     */
	if (satd_signalled) {
		switch (satd_signo) {
		case SIGTERM:
			satd_close_path (path_p);
			syslog(LOG_ALERT, "received SIGTERM -- exiting.");
			exit(1);
			break;
		case SIGHUP:
		default:
			if (OPT_VERBOSE & opts)
				fprintf (stderr, "satd - sighup received\n");
			satd_close_path (path_p);
			if (opts & OPT_ONEPASS)
				current = satd_replace_path (path_list_p, path_list_p, opts | OPT_FIRSTPATH);
			else
				current = satd_replace_path (path_list_p, current, opts);
			path_p = (path_t *) current->datum;
			break;
		}
		satd_signalled = 0;
	}

	/* if we're just here to serve the interrupt, return */
	if (bufsize == 0)
		return 0;

	/* if the current path is full, replace it with a new path           */
	if (satd_path_full (path_p)) {
		if (OPT_VERBOSE & opts)
			fprintf (stderr, "satd - current path full\n");
		satd_close_path (path_p);
		current = satd_replace_path (path_list_p,current,opts);
		path_p = (path_t *) current->datum;
	}

	/* if the current path is a directory, and if the directory file     */
	/* is full, replace with a new directory file                        */
	if ((path_dir == path_p->path_dscr) && (satd_dirf_full (path_p))) {
		if (OPT_VERBOSE & opts)
			fprintf (stderr, "satd - replacing directory file\n");
		satd_close_dirf (path_p);
		if (!satd_open_dirf  (path_p)) {
			if (OPT_VERBOSE & opts)
				fprintf (stderr,
					"satd - can't replace dir. file\n");
			satd_close_path (path_p);
			current = satd_replace_path (path_list_p,current,opts);
			path_p = (path_t *) current->datum;
		}
	}

	amt_writ = write (path_p->path_fd, buffer, bufsize);
	if (amt_writ == -1) {
		amt_writ = 0;
		path_p->path_room = 0;
	}
	else {
		path_p->path_size += amt_writ;
		path_p->path_room -= amt_writ;

		if (amt_writ < bufsize) {
			syslog (LOG_ALERT,
				"satd short write - %d asked but %d written",
				bufsize, amt_writ);
			current = satd_replace_path (path_list_p,current,opts);
		}
	}

	return amt_writ;
}


void
main (int argc, char *argv[])
{
	int		c;		/* option character		     */
	struct passwd * pw;		/* passwd struct for auditor	     */

	static	list_t	path_list;	/* dirs, devs, files for output	     */

	int	num_bufs_read = 0;	/* count of times through main loop  */
	char	buffer	 [ BUFLEN ];	/* read buffer                       */
	int	amt_reqd = BUFLEN;	/* requested  read length            */
	int	amt_read = 0;		/* the actual read length            */
	int	i;			/* counts repeated reads & writes    */
	int	io_done;		/* boolean, true if io succeeds	     */

	cap_t	ocap;
	cap_value_t cap_setuid = CAP_SETUID;

	ruid = getuid();

	/*
	 * Error messages go to both stderr and syslog until the fork().
	 */
	openlog("satd", LOG_CONS | LOG_PERROR, LOG_AUDIT);

	/* must be superuser */

	if (geteuid() != (uid_t) 0) {
		syslog(LOG_ERR, "Only root can run satd.");
		exit(1);
	}

	if (sysconf (_SC_AUDIT) <= 0) {
		syslog(LOG_ERR, "Audit not configured -- exiting.");
		exit(1);
	}

	cap_enabled = (sysconf (_SC_CAP) == CAP_SYS_NO_SUPERUSER);

	/* find out who auditor is on this system */

	pw = getpwnam("auditor");
	auditor_uid = (pw == NULL ? 0 : pw->pw_uid);

	ocap = cap_acquire (1, &cap_setuid);
	(void) setreuid (ruid, auditor_uid);
	cap_surrender (ocap);

	emergency_fd = open(EMERGENCY_RESERVE_FILE, O_RDWR | O_CREAT, 0600);
	if (emergency_fd < 0
	 || lseek(emergency_fd, EMERGENCY_RESERVE_SIZE-3, SEEK_SET) < 0
	 || write(emergency_fd, "xyz", 3) < 0) {
		(void) unlink(EMERGENCY_RESERVE_FILE);
		syslog(LOG_WARNING, "can't make reserve file: %m");
	}
	(void) close(emergency_fd);
	emergency_fd = -1;

	/* parse command line options	*/
	 while ((c = getopt(argc, argv, "1f:ior:v")) != EOF)
		switch (c) {
		case '1':
			select_opts |= OPT_ONESHOT;
			amt_reqd--;
			break;
		case 'f':
			select_opts |= OPT_PATH;
			satd_pick_pathname (&path_list, optarg);
			break;
		case 'i':
			select_opts |= OPT_INPUT;
			break;
		case 'o':
			select_opts |= OPT_OUTPUT;
			break;
		case 'r':
			if ((OPT_ROTATION | OPT_PREFERENCE | OPT_ONEPASS)
					& select_opts) {
				fprintf (stderr,
				"Only use one replacement strategy at a time");
				satd_usage();
			}
			if (!strcmp (optarg, "rotation"))
				select_opts |= OPT_ROTATION;
			if (!strcmp (optarg, "preference"))
				select_opts |= OPT_PREFERENCE;
			if (!strcmp (optarg, "onepass"))
				select_opts |= OPT_ONEPASS;
			break;
		case 'v':
			select_opts |= OPT_VERBOSE;
			break;
		default:
			satd_usage();
	}
	
	/* assign defaults if the user hasn't fully specified the command */
	if ((!(OPT_ONEPASS	& select_opts))
	&&  (!(OPT_PREFERENCE	& select_opts))
	&&  (!(OPT_ROTATION	& select_opts))) {
		select_opts |= OPT_PREFERENCE;
		if (OPT_VERBOSE & select_opts)
			fprintf (stderr,
				"Path replacement algorithm defaulted.\n");
	}

	/* confirm parameters - a debug option	*/
	if (OPT_VERBOSE & select_opts)
		satd_show_parms (&path_list, select_opts);

	/*
	 * Turn off all events for this process except fork, exec
	 * and exit so we can see what satd (and any children) are
	 * up to.  For these, we retain the default system setting.
	 */
	ocap = acquire_audit_control();
	for (i = 0; i < SAT_NTYPES; i++) {
		if (i == SAT_EXIT || i == SAT_EXEC || i == SAT_FORK)
			continue;
		(void) syssgi(SGI_SATCTL, SATCTL_LOCALAUDIT_OFF, i, 0);
	}
	relinquish_audit_control(ocap);

	/* put ourselves in the background */

#ifdef DEBUG
	/* catch signals	*/
	(void) signal(SIGHUP, satd_sgnl);
	(void) signal(SIGTERM, satd_sgnl);
	(void) signal(SIGBUS, satd_sgnl);
	(void) signal(SIGSEGV, satd_sgnl);
	/* reopen the log so msgs just go to syslog */
	openlog("satd", LOG_CONS, LOG_AUDIT);
#else /* NODEBUG */
	switch (fork()) {
	case -1:
		syslog(LOG_ERR, "Cannot fork: %m");
		exit(1);
	case 0:
		/* child */
		{ int fd = open("/dev/tty", O_RDWR);
		  if (fd > 0)
			(void) ioctl(fd, TIOCNOTTY, (char *)0);
		  closelog();
		  for (fd = getdtablesize(); --fd >= 0;) {
			if (fd == emergency_fd ||
			    (OPT_INPUT & select_opts) && fd == 0 ||
			    (OPT_OUTPUT & select_opts) && fd == 1 ||
			    (OPT_VERBOSE & select_opts) && fd == 2)
				continue;
			(void) close(fd);
		  }
		}
		/* reopen the log so msgs just go to syslog */
		openlog("satd", LOG_CONS, LOG_AUDIT);

		/* catch signals	*/
		(void) signal(SIGHUP, satd_sgnl);
		(void) signal(SIGTERM, satd_sgnl);
		(void) signal(SIGBUS, satd_sgnl);
		(void) signal(SIGSEGV, satd_sgnl);

		/* make sure we're the only sat daemon (unless we are */
		/* reading from stdin)                                */
		ocap = acquire_audit_control();
		if (!(OPT_INPUT & select_opts) &&
		    syssgi(SGI_SATCTL, SATCTL_REGISTER_SATD) < 0) {
			if (errno == EBUSY) {
				relinquish_audit_control(ocap);
				syslog(LOG_ERR,"Already a sat daemon running.");
				if (OPT_VERBOSE & select_opts)
				    	fprintf(stderr, 
					"Already a sat daemon running.\n");
				exit(1);
			} else
				syslog(LOG_WARNING,
				       "Warning, can't register satd: %m.");
		}
		relinquish_audit_control(ocap);

		break;
	default:
		/* parent */
		exit(0);
	}
#endif
	/*
	 * -f or -o option must be specified .
	 * Otherwise, open emergency file and take system down
	 * to single user mode.
	 */

	if ( !((OPT_OUTPUT | OPT_PATH) & select_opts)) {
		satd_exit("must specify -f and/or -o option", 0.0);
	}

        /* Write file header to stdout if needed */
	if (OPT_OUTPUT & select_opts) {
		if (sat_write_filehdr(1) < 0) {
			satd_exit("Can't write sat file header to"
				"stdout: %m",0);
		}
		fprintf(stderr,"Wrote file header to stdout\n");
	}

	/* gather records	*/
	for (;;) {
		num_bufs_read++;

		for (i = SATD_MAX_INTR_PER_IO, io_done = 0;
				(i > 0) && !io_done; i--) {

			if (OPT_INPUT & select_opts)
				amt_read = read (0, buffer, amt_reqd);
			else {
				ocap = acquire_audit_control();
				amt_read = satread (buffer, amt_reqd);
				relinquish_audit_control(ocap);
			}

			if (amt_read < 0) {
				if (errno == EINTR) {
					if (satd_signalled) {
						amt_read = 0;
						break;
					}
					errno = 0;
					continue;
				}
				else if (errno == EPERM)
					satd_exit("No permission match on read",
						0.0);
				else
					satd_exit (
					"satd read error (near byte %.0f): %m",
					    (double)(num_bufs_read * amt_reqd));
			}
			else
				io_done = 1;
		}

		if ((OPT_OUTPUT & select_opts) &&
		    write(1, buffer, amt_read) < 0)
			satd_exit(
			"Can't write sat buffer to stdout (near byte %.0f): %m",
			    (double) (num_bufs_read * amt_reqd));

		if ((OPT_PATH & select_opts) &&
		    satd_send(&path_list, buffer, amt_read, select_opts) < 0)
			satd_exit("Can't write sat buffer (near byte %.0f): %m",
			    (double) (num_bufs_read * amt_reqd));

		/*
		 * option 'oneshot' reads all the records currently queued,
		 * then exits.  This can be used to flush the audit record
		 * queue.  If '-1' isn't used, satd blocks on read and loops
		 * forever.  Since all records are an even multiple of four
		 * bytes long, amt_reqd is an odd number, ensuring that
		 * satd won't stop prematurely when it encounters a string
		 * of records exactly BUFLEN long.
		 */
		if ((OPT_ONESHOT & select_opts)
				&& (amt_read != amt_reqd))
			break;
	}

	closelog ();

	exit(0);
}
