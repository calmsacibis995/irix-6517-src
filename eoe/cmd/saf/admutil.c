/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

# ident	"@(#)saf:admutil.c	1.8.6.2"

# include <stdio.h>
# include <signal.h>
# include <sac.h>
# include <sys/types.h>
# include <sys/stat.h>
# include <unistd.h>
# include <priv.h>
# include <mac.h>
# include <errno.h>
# include <pwd.h>
# include <grp.h>
# include "misc.h"
# include "structs.h"
# include "extern.h"

/*
 * Procedure: error - print out an error message and die
 *
 * Args: msg - message to be printed, Saferrno previously set
 */

void
error(msg)
char *msg;
{
	(void) fprintf(stderr, "%s\n", msg);
	quit();
}


/*
 * Procedure: quit - exit the program with the status in Saferrno
 */

void
quit()
{
	exit(Saferrno);
}


/*
 * Procedure: make_tempname - generate a temp name to be used for
 *			      updating files.
 *
 * Notes: Names will be of the form HOME/xxx/.name, where HOME is from misc.h
 *
 * Args: bname - the basename of the file.  For example foo/_config
 *		 will generate a tempname of HOME/foo/._config
 */


char *
make_tempname(bname)
char *bname;
{
	static char buf[SIZE];	/* this is where we put the new name */
	char *p;			/* work pointer */

	p = strrchr(bname, '/');
	if (p == NULL)
		(void) sprintf(buf, "%s/.%s", HOME, bname);
	else {
		(void) strcpy(buf, HOME);
		/* this zaps the trailing slash so the '.' can be stuck in */
		*p = '\0';
		(void) strcat(buf, "/");
		(void) strcat(buf, bname);
		(void) strcat(buf, "/.");
		(void) strcat(buf, (p + 1));
		*p = '/';
	}
	return(buf);
}


/*
 * Procedure: open_temp - open up a temp file
 * Restrictions:
 *               access(2): None
 *               fopen: None
 *
 * Args: tname - temp file name
 */

FILE *
open_temp(tname)
char *tname;
{
	FILE *fp;			/* fp associated with tname */
	struct sigaction sigact;	/* for signal handling */

	sigact.sa_flags = 0;
	sigact.sa_handler = SIG_IGN;
	(void) sigemptyset(&sigact.sa_mask);
	(void) sigaddset(&sigact.sa_mask, SIGHUP);
	(void) sigaddset(&sigact.sa_mask, SIGINT);
	(void) sigaddset(&sigact.sa_mask, SIGQUIT);
	(void) sigaction(SIGHUP, &sigact, NULL);
	(void) sigaction(SIGINT, &sigact, NULL);
	(void) sigaction(SIGQUIT, &sigact, NULL);
	(void) umask(0022);
	if (access(tname, 0) != -1) {
		Saferrno = E_SAFERR;
		error("tempfile busy; try again later");
	}
	fp = fopen(tname, "w");
	if (fp == NULL) {
		Saferrno = E_SYSERR;
		error("cannot create tempfile");
	}
	return(fp);
}


/*
 * Procedure: replace - replace one file with another
 * Restrictions:
 *               stat(2): None
 *               lvlfile(2): None
 *               chmod(2): None
 *               chown(2): None
 *               rename(2): None
 *               unlink(2): None
 *
 * Notes: routine only returns on success.  replace() sets the original
 *	  file's attributes after the rename if the original file
 *	  exists.
 *
 * Args:  fname - full path name of target file
 *	  tname - full path name of source file
 */


void
replace(fname, tname)
char *fname;
char *tname;
{
	char buf[SIZE];		/* scratch buffer */
	struct stat attr;	/* buffer for attributes */
	int attr_ret = 0;	/* found atttributes */
	level_t level;		/* MAC level identifier */
	
	/* get attributes if original file exists */ 
	if ( stat(fname, &attr) == 0 ) 
		attr_ret = 1;
	
	/*
	 * set to original mode & ownership
	 * set level only if attr.st_level has a valid value
	 */
	if ( attr_ret ) {
                if ( attr.st_level != 0 ) {
			level = attr.st_level;
			if ( lvlfile(tname, MAC_SET, &level) != 0) {
				Saferrno = E_NOPRIV;
				(void) sprintf(buf, "could not set level on %s", fname);
				error(buf);
			}
		}
		if (chmod(tname, attr.st_mode) != 0 ||
		    chown(tname, attr.st_uid, attr.st_gid) != 0) {
			Saferrno = E_SYSERR;
			(void) sprintf(buf, "could not set attributes on %s", fname);
			error(buf);
		}
		
	}

	if ( rename(tname, fname) < 0) {
		Saferrno = E_SYSERR;
		(void) unlink(tname);
		(void) sprintf(buf, "unable to rename temp file %s", tname);
		error(buf);
	}
}


/*
 * Procedure: copy_file - copy information from one file to another
 *
 * Notes: return 0 on success, -1 on failure
 *
 * Args:  fp - source file's file pointer
 * 	  tfp - destination file's file pointer
 *	  start - starting line number
 *	  finish - ending line number (-1 indicates entire file)
 */

copy_file(fp, tfp, start, finish)
FILE *fp;
FILE *tfp;
register int start;
register int finish;
{
	register int i;		/* loop variable */
	char dummy[SIZE];	/* scratch buffer */

/*
 * always start from the beginning because line numbers are absolute
 */

	rewind(fp);

/*
 * get to the starting point of interest
 */

	if (start != 1) {
		for (i = 1; i < start; i++)
			if (!fgets(dummy, SIZE, fp))
				return(-1);
	}

/*
 * copy as much as was requested
 */

	if (finish != -1) {
		for (i = start; i <= finish; i++) {
			if (!fgets(dummy, SIZE, fp))
				return(-1);
			if (fputs(dummy, tfp) == EOF)
				return(-1);
		}
	}
	else {
		for (;;) {
			if (fgets(dummy, SIZE, fp) == NULL) {
				if (feof(fp))
					break;
				else
					return(-1);
			}
			if (fputs(dummy, tfp) == EOF)
				return(-1);
		}
	}
	return(0);
}

/*
 * Procedure: open_sactab - open _sactab with specified mode, returns
 *			    file pointer
 * Args:	mode: mode requested
 * 
 * Restrictions: fopen: P_MACREAD
 */

FILE *
open_sactab(mode)
char *mode;
{
	FILE *fp;	/* file pointer for _sactab */
	
	fp = fopen(SACTAB, mode);
	
	return(fp);
}

/*
 * Procedure: find_pm - find an entry in _sactab for a particular port
 *			monitor 
 *
 * Args:	fp - file pointer for _sactab
 *		pmtag - tag of port monitor we're looking for
 */

find_pm(fp, pmtag)
FILE *fp;
char *pmtag;
{
	register char *p;	/* working pointer */
	int line = 0;		/* line number we found entry on */
	struct sactab stab;	/* place to hold parsed info */
	char buf[SIZE];		/* scratch buffer */

	while (fgets(buf, SIZE, fp)) {
		line++;
		p = trim(buf);
		if (*p == '\0')
			continue;
		parse(p, &stab);
		if (!(strcmp(stab.sc_tag, pmtag)))
			return(line);
	}
	if (!feof(fp)) {
		Saferrno = E_SYSERR;
		error("error reading _sactab");
		/* NOTREACHED */
	}
	else
		return(0);
}


/*
 * Procedure: do_config - take a config script and put it where it
 * 			  belongs or output an existing one.
 * Restrictions:
 *               access(2): None
 *               stat(2): None
 *               fopen: None
 *               unlink(2): None
 *               chmod(2): None
 *               chown(2): None
 *               lvlin: None
 *               lvlfile(2): None
 *
 * Notes: Saferrno is set if any errors are encountered.  Calling
 * 	  routine may choose to quit or continue, in which case Saferrno
 *	  will stay set, but may change value if another error is
 *	  encountered. 
 *
 * Args:	script - name of file containing script (if NULL, means output
 *			 existing one instead)
 *		basename - name of script (relative to HOME (from misc.h))
 */

do_config(script, basename)
char *script;
char *basename;
{
	FILE *ifp;		/* file pointer for source file */
	FILE *ofp;		/* file pointer for target file */
	struct stat statbuf;	/* file status info */
	char *tname;		/* pathname of temp file */
	char origname[SIZE];	/* pathname of original file */	
	char buf[SIZE];		/* scratch buffer */
	short new = 0;		/* check original config file exists */
	level_t level;		/* MAC level identifier */
	
	if (script) {
		/* we're installing a new configuration script */
		if (access(script, 0) == 0) {
			if (stat(script, &statbuf) < 0) {
				Saferrno = E_SYSERR;
				(void) fprintf(stderr, "Could not stat <%s>\n", script);
				return(1);
			}
			if ((statbuf.st_mode & S_IFMT) != S_IFREG) {
				(void) fprintf(stderr, "warning - %s not a regular file - ignored\n", script);
				return(1);
			}
		}
		else {
			Saferrno = E_NOEXIST;
			(void) fprintf(stderr, "Invalid request, cannot access %s\n", script);
			return(1);
		}
		ifp = fopen(script, "r");
		if (ifp == NULL) {
			(void) fprintf(stderr, "Invalid request, can not open %s\n", script);
			Saferrno = E_SYSERR;
			return(1);
		}
		tname = make_tempname(basename);
		/* note - open_temp only returns if successful */
		ofp = open_temp(tname);
		while(fgets(buf, SIZE, ifp)) {
			if (fputs(buf, ofp) == EOF) {
				(void) unlink(tname);
				Saferrno = E_SYSERR;
				error("error in writing tempfile");
			}
		}
		(void) fclose(ifp);
		if (fclose(ofp) == EOF) {
			(void) unlink(tname);
			Saferrno = E_SYSERR;
			error("error closing tempfile");
		}

		(void) sprintf(origname, "%s/%s", HOME, basename);
		if (access(origname, F_OK) < 0)
			new = 1;	/* no original config file */

		/* note - replace only returns if successful */
		replace(origname, tname);
		
		/* if new, set owner, group, mode, & level */
		if ( new ) {
			if (chmod(origname, (mode_t) 0644) != 0 ||
			    chown(origname, (uid_t) SAF_OWNER, (gid_t) SAF_GROUP) != 0) {
				Saferrno = E_SYSERR;
				(void) sprintf(buf, "error setting attributes on %s", origname);
				error(buf);
			}
			
			if (lvlin("SYS_PRIVATE", &level) == 0) {
				if (lvlfile(origname, MAC_SET, &level) != 0) {
					Saferrno = E_NOPRIV;
					(void) sprintf(buf, "error setting level on %s", origname);
					error(buf);
				}
			} else {
				if (mac_check()) {
					Saferrno = E_SYSERR;
					error("unable to find lid of SYS_PRIVATE");
				}
			}
		}

		return(0);
	}
	else {
		/* we're outputting a configuration script */
		(void) sprintf(buf, "%s/%s", HOME, basename);
		if (access(buf, 0) < 0) {
			(void) fprintf(stderr, "Invalid request, script does not exist\n");
			Saferrno = E_NOEXIST;
			return(1);
		}
		ifp = fopen(buf, "r");
		if (ifp == NULL) {
			(void) fprintf(stderr, "Invalid request, can not open script\n");
			Saferrno = E_SYSERR;
			return(1);
		}
		while (fgets(buf, SIZE, ifp))
			(void) fputs(buf, stdout);
		(void) fclose(ifp);
		return(0);
	}
}

/*
 * Procedure: mac_check - checks if MAC is installed
 *
 * Notes:	return	1 if yes 
 *			0 if no
 */

mac_check()
{
	level_t level_lid;	/* scratch MAC level identifier */
	
	if ( lvlproc(MAC_GET, &level_lid) != 0 ) {
		if ( errno == ENOPKG )
			return(0);
		else {
			Saferrno = E_SYSERR;
			error("could not determine if MAC is installed");
		}
	}
	else
		return(1);
	/* NOTREACHED */
}


