/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1994, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ident  "xfsutils.c: $Revision: 1.2 $"

/*
 * xfsutils.c -- library of xfs functions to access /etc/fstab system
 * file and validate mount points.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <dirent.h>
#include <libgen.h>
#include <sys/file.h>
#include <sys/stat.h>

#include <mntent.h>
#include <netinet/in.h>
#include <netdb.h>

#include <xfs_fs_defs.h>

#define LINESIZE 1024
static char TMPFILE[] = "/etc/mntXXXX";

/*
 *	isvalidmntpt()
 *	The mount directory path is checked to make sure it is a
 *	absolute path, doesn't contain dangling "..", isn't a symbolic
 *	link etc. If it is a invalid mount point, 1 is returned.
 *	On successful vaildation of the mount point, a 0 is returned.
 */
int
isvalidmntpt(char *path)
{
	DIR *dirfd;
	struct stat dcheck;
	int returnValue=0;

	if(path == NULL || *path == '\0' || strchr(path, ' ') != NULL) {
		returnValue = 1;
	}
	/* Return error if it's not an absolute path */
	else if ( path[0] != '/') {
		returnValue = 1;
	}
	/* Return error if it has /./ or /../ included in the path */
	else if(strstr(path, "/./") || strstr(path, "/../") ) {
		returnValue = 1;
	}
	/* Now check to see if path begins with /tmp */
	else if(strncmp(path, "/tmp/", 5) == 0) {
		returnValue = 1;
	}
	/* Make sure the path is not a symbolic link to never, never land */
	else if ((stat(path,&dcheck) == 0) &&
		((dcheck.st_mode & S_IFMT) != S_IFDIR)) {
		returnValue = 1;
	}

	return(returnValue);
}


/*
 * remmntent()
 * Removes an entry identified by fsname from the /etc/fstab system file
 * by traversing the list of entries.
 * On success, 0 is returned. Otherwise -1 is returned.
 */
 
int
remmntent(mnttabp, mnttabname, fsname)
FILE *mnttabp;
char *mnttabname;
char *fsname;
{
    char buf[LINESIZE];
    FILE *ftmp;
    int len;
    char tmpname[sizeof(TMPFILE)];
    char *val;
    int entryRemoved = 0;

    (void)strcpy(tmpname, TMPFILE);
    mktemp(tmpname);
    ftmp = fopen(tmpname, "w");
    if(!ftmp)
	return -1;
		
    len = strlen(fsname);
    while ((val = fgets(buf, sizeof(buf), mnttabp)) != 0) {
	if (strncmp(buf, fsname, len) != 0 || ! isspace(buf[len])) {
	    if (fputs(buf, ftmp) <= 0) {
		fclose(ftmp);	
		return -1;
	    }
	}
	else
	    entryRemoved++;
    }
    (void)fclose(ftmp);
	
    if(entryRemoved)
    	return rename(tmpname, mnttabname);
    else
	return unlink(tmpname);
}

int
xfs_getmntent_bydir(char *tabfile, char *dirname, struct mntent *ent)
{
	FILE		*fp;
	struct mntent	*tab_ent;
	int		found = 1;

	/* Check the parameters, tabfile and fsname */
	if ((tabfile == NULL) || (dirname == NULL)) {
		return(-1);
	}

	if(ent != NULL) {
		ent->mnt_fsname = NULL;
		ent->mnt_dir = NULL;
		ent->mnt_type = NULL;
		ent->mnt_opts = NULL;
	}

	if ((fp = setmntent(tabfile, "r")) != NULL) {
		while((tab_ent = getmntent(fp)) != NULL) {
			if(strcmp(tab_ent->mnt_dir, dirname) == 0) {
				found = 0;
				break;
			}
		}
	}

	if(found == 0 && ent != NULL) {
		ent->mnt_fsname	= strdup(tab_ent->mnt_fsname);
		ent->mnt_dir	= strdup(tab_ent->mnt_dir);
		ent->mnt_type	= strdup(tab_ent->mnt_type);
		ent->mnt_opts	= strdup(tab_ent->mnt_opts);
		ent->mnt_freq	= tab_ent->mnt_freq;
		ent->mnt_passno	= tab_ent->mnt_passno;
	}

	endmntent(fp);

	return(found);
}

int
xfs_getmntent(char *tabfile, char *fsname, struct mntent *ent)
{
	FILE		*fp;
	struct mntent	*tab_ent;
	struct hostent	*in_hostent;
	struct stat	stat_buf;

	boolean_t	found = B_FALSE;
	char		*dup_fsname = NULL;
	char		*in_host = NULL,
			*in_fsname = NULL;
	char		*dup_tabfsname = NULL;
	char		*tab_host = NULL,
			*tab_fsname = NULL;
	int		i;

	/* Check the parameters, tabfile and fsname */
	if ((tabfile == NULL) || (fsname == NULL)) {
		return(-1);
	}

	if(ent != NULL) {
		ent->mnt_fsname = NULL;
		ent->mnt_dir = NULL;
		ent->mnt_type = NULL;
		ent->mnt_opts = NULL;
	}

	if ((stat(fsname, &stat_buf) == 0) &&
	  (((stat_buf.st_mode & S_IFMT) == S_IFREG) ||
	   ((stat_buf.st_mode & S_IFMT) == S_IFDIR))) {
		return(xfs_getmntent_bydir(tabfile, fsname, ent));
	} 

	if ((fp = setmntent(tabfile, "r")) != NULL) {
	    if(strchr(fsname, ':') != NULL) {
		/*
		 *	split up the fsname into discreet parts
		 */
		dup_fsname = strdup(fsname);
		in_host = strtok(dup_fsname, ":");
		in_fsname = dup_fsname + strlen(in_host) + 1;

		/*
		 *	Get aliases for the host
		 */
		if((in_hostent = gethostbyname(in_host)) == NULL) {
			free(dup_fsname);
			dup_fsname = NULL;
			in_host = NULL;
			in_fsname = NULL;
		}
	    }

	    /*
	     *	Read the entries in the file, comparing to fsname
	     */
	    while(found == B_FALSE && (tab_ent = getmntent(fp)) != NULL) {
		if((tab_ent->mnt_fsname != NULL) &&
		    (strcmp(tab_ent->mnt_fsname, fsname) == 0)) {
			found = B_TRUE;
		} else if(dup_fsname != NULL &&
			  strcmp(tab_ent->mnt_type, "nfs") == 0) {

		    /*
		     *	Check to see if we've been passed an alias
		     */
		    dup_tabfsname = strdup(tab_ent->mnt_fsname);
		    tab_host = strtok(dup_tabfsname, ":");
		    tab_fsname = dup_tabfsname + strlen(tab_host) + 1;
		    if(strcmp(in_fsname, tab_fsname) == 0) {

			/*
			 *	Compare against the full hostname
			 */
			if(strcmp(in_hostent->h_name, tab_host) == 0) {
				found = B_TRUE;
			}

			/*
			 *	Look at the aliases
			 */
			for(i = 0;
			    found==B_FALSE && in_hostent->h_aliases[i] != NULL;
			    ++i) {
			    if(strcmp(in_hostent->h_aliases[i], tab_host)==0) {
				found = B_TRUE;
				break;
			    }
			}
			free(dup_tabfsname);
		    }
		}
	    }

	    /* Close the file */
	    endmntent(fp);
	    free(dup_fsname);
	}

	if(found == B_TRUE) {
		if(ent != NULL) {
			ent->mnt_fsname	= strdup(tab_ent->mnt_fsname);
			ent->mnt_dir	= strdup(tab_ent->mnt_dir);
			ent->mnt_type	= strdup(tab_ent->mnt_type);
			ent->mnt_opts	= strdup(tab_ent->mnt_opts);
			ent->mnt_freq	= tab_ent->mnt_freq;
			ent->mnt_passno	= tab_ent->mnt_passno;
		}
		return(0);
	}
	else {
		return(1);
	}
}

int
xfs_replace_mntent(char *tabfile,
		   struct mntent *old_ent,
		   struct mntent *new_ent)
{
	boolean_t	entry_replaced = B_FALSE;
	char		tmpname[sizeof(TMPFILE) + 1];
	struct mntent	*tab_ent;
	FILE		*fp_tab;
	FILE		*fp_tmp;
	int		rval;

	if((fp_tab = setmntent(FSTAB, "r+")) == NULL) {
		return(-1);
	}

	/*
	 *	If old_ent is NULL, then this is simply an add
	 */
	if(old_ent == NULL) {
		rval = addmntent(fp_tab, new_ent);
		endmntent(fp_tab);
		return(rval);
	}

	/*
	 *	This is an add or replace, so open up the tmpfile
	 *	so that we can do the job.
	 */
	strcpy(tmpname, TMPFILE);
	mktemp(tmpname);
	if((fp_tmp = setmntent(tmpname, "w")) == NULL) {
		endmntent(fp_tab);
		return(-1);
	}

	while ((tab_ent = getmntent(fp_tab)) != NULL) {
		if(strcmp(tab_ent->mnt_fsname, old_ent->mnt_fsname) == 0 &&
		   strcmp(tab_ent->mnt_dir, old_ent->mnt_dir) == 0 &&
		   strcmp(tab_ent->mnt_type, old_ent->mnt_type) == 0 &&
		   strcmp(tab_ent->mnt_opts, old_ent->mnt_opts) == 0) {
			/*
			 *	If new_ent is NULL, then this is a remove
			 */
			if(new_ent != NULL) {
				rval = addmntent(fp_tmp, new_ent);
			}
			entry_replaced = B_TRUE;
		} else {
			rval = addmntent(fp_tmp, tab_ent);
		}

		if(rval != 0) {
			endmntent(fp_tab);
			endmntent(fp_tmp);
			unlink(tmpname);
			return(-1);
		}
	}
	endmntent(fp_tab);
	endmntent(fp_tmp);

	if(entry_replaced) {
		return(rename(tmpname, tabfile));
	}
	else {
		return(unlink(tmpname));
	}
}

/***************************************************
 ****						****
 ****		Export Utilities		****
 ****						****
 ***************************************************/
int
xfs_get_exportent(char *tabfile, char *dirname, struct exportent *ent)
{
	struct exportent	*export_ent;
	FILE			*fp_tab;
	int			returnValue = 1;

	/* Check the parameters, tabfile and fsname */
	if (dirname == NULL) {
		return(-1);
	}

	if(ent != NULL) {
		ent->xent_dirname = NULL;
		ent->xent_options = NULL;
	}

	if(tabfile != NULL) {
		if((fp_tab = fopen(tabfile, "r")) == NULL) {
			return(-1);
		}
	}
	else {
		if((fp_tab = setexportent()) == NULL) {
			return(-1);
		}
	}

	while((export_ent = getexportent(fp_tab)) != NULL) {
	    if(strcmp(dirname, export_ent->xent_dirname) == 0) {
		if(ent != NULL) {
		    ent->xent_dirname = strdup(export_ent->xent_dirname);
		    if(export_ent->xent_options) {
			ent->xent_options = strdup(export_ent->xent_options);
		    }
		}
		returnValue = 0;
		break;
	    }
	}

	endexportent(fp_tab);

	return(returnValue);
}

int
xfs_replace_exportent(struct exportent *old_ent, struct exportent *new_ent)
{
	FILE		*fp_tab;
	int		rval = 0;

	if((fp_tab = fopen(EXTAB, "r+")) == NULL) {
		return(-1);
	}

	if(old_ent != NULL) {
		rval = remexportent(fp_tab, old_ent->xent_dirname);
	}
	if(new_ent != NULL && rval == 0) {
		rval = addexportent(fp_tab, new_ent->xent_dirname,
					    new_ent->xent_options);
	}

	endexportent(fp_tab);
	return(rval);
}
