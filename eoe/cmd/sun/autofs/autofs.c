/*
 *	autofs.c
 *
 *	Copyright (c) 1988-1993 Sun Microsystems Inc
 *	All Rights Reserved.
 */

#ident	"@(#)autofs.c	1.7	93/11/03 SMI"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <varargs.h>
#include <errno.h>
#include <string.h>
#include <dirent.h>
#include <mntent.h>
#include <getopt.h>
#include <sys/socket.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/signal.h>
#include <sys/utsname.h>
#include <sys/fs/autofs.h>
#include <sys/capability.h>
#include <sys/mac.h>
#include <sys/eag.h>
#include <sys/sat.h>
#include "autofsd.h"

static int compare_opts(char *, char *);
static struct mntent *find_mount(char *, int);
static void do_unmounts(void);
static void usage(void);

struct autodir *dir_head;
struct autodir *dir_tail;
extern struct mntlist *current_mounts;
int verbose;

static int mac_enabled;

int mount_timeo = 5 * 60;	/* 5 minutes */
int rpc_timeo = 60;
char *user_to_execute_maps_as = NULL;

int
main(argc, argv)
	int argc;
	char *argv[];
{
	int c;
	struct autofs_args ai;
	struct utsname utsname;
	struct autodir *dir, *d;
	struct stat stbuf;
	char *master_map = "/etc/auto_master";
	int null;
	struct mntent mnt, *mntp;
	char mntopts[1000];
	char *non_num;
	int mntflgs;
	int count = 0;
	cap_t ocap;
	cap_value_t mount_caps[] = {CAP_MOUNT_MGT, CAP_PRIV_PORT};
	cap_value_t cap_audit_write = CAP_AUDIT_WRITE;
	int mnterr = 0;
	char *eagopt;
	char *stack[STACKSIZ];
	char **stkptr;

	multithread = 0;

	/* see if MAC is enabled on this system */
	mac_enabled = (sysconf(_SC_MAC) > 0);

	/*
	 * NOTE: Both autofs and autofsd can be started from the
	 *	 /etc/init.d/network script, which will pass the contents
	 *	 of /etc/config/autofs.options to BOTH autofs and autofsd.
	 *	 This means that autofs must gracefully handle (and ignore)
	 *	 any autofsd options (e.g., -tp).
	 */

	while ((c = getopt(argc, argv, "m:M:D:f:p:t:TvE:")) != EOF) {
		switch (c) {
		case 'm':
			break;
		case 'M':
			pr_msg("Warning: -M option not supported");
			break;
		case 'D':
			pr_msg("Warning: -D option not supported");
			break;
		case 'f':
			pr_msg("Error: -f option no longer supported");
			usage();
			break;
		case 't':
			if (argv[optind-1][2] == 'p') {
				/* We have encountered the autofsd -tp option.
				 * Because this is a non-standard two character
				 * option, we may need to do some manual
				 * manipulation of getopt's 'optind' variable
				 * to ignore this option. In the case where
				 * the -tp argument is separated by a space
				 * from the option name (-tp n), getopt()
				 * assumes that 'p' is the argument and that
				 * the next option is in the next argv entry.
				 * For -tp, this is not the case, the -tp
				 * argument is in the next entry, and the next
				 * option is in the entry after that. So, in
				 * order to ignore -tp and it's argument, we
				 * increment 'optind'.
				 */
				if (argv[optind-1][3] == '\0')
					optind++;
				break;

			} else if ((strtol(optarg, &non_num, 10) == 0) ||
			    (*non_num != '\0')) {
				pr_msg("Error: -t argument is not numeric");
				usage();
			}
			mount_timeo = atoi(optarg);
			break;
		case 'v':
			verbose++;
			break;
		case 'p':		/* ignore autofsd's ops */
		case 'T':
		case 'E':
			break;
		default:
			usage();
			break;
		}
	}

	if (optind < argc) {
		pr_msg("%s: command line mountpoints/maps "
			"no longer supported",
			argv[optind]);
		usage();
	}

	if (geteuid() != 0) {
		pr_msg("must be root");
		exit(1);
	}

	current_mounts = getmntlist();
	if (current_mounts == NULL) {
		pr_msg("Couldn't establish current mounts");
		exit(1);
	}

	(void) init_names();
	(void) umask(0); /* XXX */

	stack_op(INIT, NULL, stack, &stkptr);
	(void) loadmaster_map(master_map, "rw", stack, &stkptr);

	if (uname(&utsname) < 0) {
		pr_msg("uname: %m");
		exit(1);
	}

	ai.mount_to	= mount_timeo;
	ai.rpc_to	= rpc_timeo;

	/*
	 * Mount the daemon at its mount points.
	 */
	for (dir = dir_head; dir; dir = dir->dir_next) {

		/*
		 * Skip null entries
		 */
		if (strcmp(dir->dir_map, "-null") == 0)
			continue;

		/*
		 * Skip null'ed entries
		 */
		null = 0;
		for (d = dir->dir_prev; d; d = d->dir_prev) {
			if (strcmp(dir->dir_name, d->dir_name) == 0)
				null = 1;
		}
		if (null)
			continue;

		/*
		 * Check whether there's already an entry
		 * in the mtab for this mountpoint.
		 */
		if (mntp = find_mount(dir->dir_name, 1)) {
			/*
			 * If it's not an autofs mount - don't
			 * mount over it.
			 */
			if (strcmp(mntp->mnt_type, MNTTYPE_AUTOFS) != 0) {
				pr_msg("%s: already mounted",
					mntp->mnt_dir);
				continue;
			}

			/*
			 * Compare the mtab entry with the master map
			 * entry.  If the map or mount options are
			 * different, then update this information
			 * with a remount.
			 */
			if (strcmp(mntp->mnt_fsname, dir->dir_map) == 0 &&
				compare_opts(dir->dir_opts,
					mntp->mnt_opts) == 0) {
				continue;	/* no change */
			}

			/*
			 * Check for an overlaid direct autofs mount.
			 * Cannot remount since it's inaccessible.
			 */
			if (hasmntopt(mntp, "direct") != NULL) {
				mntp = find_mount(dir->dir_name, 0);
				if (hasmntopt(mntp, "direct") == NULL) {
					if (verbose)
						pr_msg("%s: cannot remount",
							dir->dir_name);
					continue;
				}
			}
			if (eagopt = hasmntopt(mntp, MNTOPT_EAG)) {
			  char *cp;
			  eagopt = strdup(eagopt);
			  if (cp = strchr(eagopt, ','))
			    *cp = '\0';
			  if (cp = strchr(eagopt, ' '))
			    *cp = '\0';
			  if (cp = strchr(eagopt, '\t'))
			    *cp = '\0';
			}
			dir->dir_remount = 1;
		}

		/*
		 * Create a mount point if necessary
		 * If the path refers to an existing symbolic
		 * link, refuse to mount on it.  This avoids
		 * future problems.
		 */
		if (lstat(dir->dir_name, &stbuf) == 0) {
			if ((stbuf.st_mode & S_IFMT) != S_IFDIR) {
				pr_msg("%s: Not a directory", dir->dir_name);
				continue;
			}
		} else {
			if (mkdir_r(dir->dir_name)) {
				pr_msg("%s: %m", dir->dir_name);
				continue;
			}
		}

		ai.path 	= dir->dir_name;
		ai.opts		= dir->dir_opts;
		ai.map		= dir->dir_map;
		ai.direct 	= dir->dir_direct;
		mntflgs = dir->dir_remount ? MS_REMOUNT : 0;


		ocap = cap_acquire(_CAP_NUM(mount_caps), mount_caps);

		if (eagopt || mac_enabled)
		    mnterr=sgi_eag_mount("", dir->dir_name, MS_DATA | mntflgs, 
			 MNTTYPE_AUTOFS, (char *) &ai, sizeof (ai), eagopt);
		else
		    mnterr=mount("", dir->dir_name, MS_DATA | mntflgs,
			 MNTTYPE_AUTOFS,&ai, sizeof (ai));
		cap_surrender(ocap);
		free(eagopt);
		if (mnterr < 0) {
			pr_msg("mount %s: %m", dir->dir_name);
			ocap = cap_acquire(_CAP_NUM(cap_audit_write), &cap_audit_write);
			satvwrite(SAT_AE_MOUNT, SAT_FAILURE,
				  "autofs: failed umount of %s : %s",  dir->dir_name, 
				  strerror(mnterr));
			cap_surrender(ocap);
			continue;
		}
	
		/*
		 * Add autofs entry to /etc/mtab
		 */
		mnt.mnt_fsname = dir->dir_map;
		mnt.mnt_dir  = dir->dir_name;
		mnt.mnt_type  = MNTTYPE_AUTOFS;
		(void) sprintf(mntopts, "ignore,%s",
			dir->dir_direct  ? "direct" : "indirect");
		if (dir->dir_opts && *dir->dir_opts) {
			(void) strcat(mntopts, ",");
			(void) strcat(mntopts, dir->dir_opts);
		}
		mnt.mnt_opts = mntopts;
		if (dir->dir_remount)
			fix_mtab(&mnt);
		else
			add_mtab(&mnt);

		count++;

		if (verbose) {
			if (dir->dir_remount)
				pr_msg("%s remounted", dir->dir_name);
			else
				pr_msg("%s mounted", dir->dir_name);
		}
		ocap = cap_acquire(_CAP_NUM(cap_audit_write), &cap_audit_write);
		if (dir->dir_remount)
		  satvwrite(SAT_AE_MOUNT, SAT_SUCCESS,
			    "autofs: remount of %s : %s",  dir->dir_name, strerror(mnterr));
		else
		  satvwrite(SAT_AE_MOUNT, SAT_SUCCESS,
			    "autofs: mount of %s : %s",  dir->dir_name, strerror(mnterr));
		cap_surrender(ocap);
	}

	if (verbose && count == 0)
		pr_msg("no mounts");

	/*
	 * Now compare the /etc/mtab with the master
	 * map.  Any autofs mounts in the /etc/mtab
	 * that are not in the master map must be
	 * unmounted
	 */
	do_unmounts();

	return(0);
}

/*
 * Find a mount entry given
 * the mountpoint path.
 * Optionally return the first
 * or last entry.
 */
static struct mntent *
find_mount(mntpnt, first)
	char *mntpnt;
	int first;
{
	struct mntlist *mntl;
	struct mntent *found = NULL;

	for (mntl = current_mounts; mntl; mntl = mntl->mntl_next) {

		if (strcmp(mntpnt, mntl->mntl_mnt->mnt_dir) == 0) {
			found = mntl->mntl_mnt;
			if (first)
				break;
		}
	}

	return (found);
}

char *ignore_opts[] = {"ignore", "direct", "indirect", "dev", NULL};

/*
 * Compare mount options
 * ignoring "ignore", "direct", "indirect"
 * and "dev=".
 */
static int
compare_opts(opts, mntopts)
	char *opts, *mntopts;
{
	char optbuff[1000], *bp = optbuff;
	char *p, *q;
	int c, i, found;

	(void) strcpy(bp, mntopts);

	while (*bp) {

		if ((p = strchr(bp, ',')) == NULL)
			p = bp + strlen(bp);
		else
			p++;

		q = bp + strcspn(bp, ",=");
		c = *q;
		*q = '\0';
		found = 0;
		for (i = 0; ignore_opts[i]; i++) {
			if (strcmp(ignore_opts[i], bp) == 0) {
				found++;
				break;
			}
		}
		if (found) {
			(void) strcpy(bp, p);
		} else {
			*q = c;
			bp = p;
		}
	}

	p = optbuff + (strlen(optbuff) - 1);
	if (*p == ',')
		*p = '\0';

	return (strcmp(opts, optbuff));
}

static void
usage(void)
{
	pr_msg("Usage: autofs  [ -v ]  [ -t duration ]");
	exit(1);
}

/*
 * Unmount any autofs mounts that
 * aren't in the master map
 */
static void
do_unmounts(void)
{
	struct mntlist *mntl;
	struct mntent *mnt;
	extern struct autodir *dir_head;
	struct autodir *dir;
	extern int verbose;
	int current;
	int count = 0;
	cap_t ocap;
	cap_value_t umount_caps[] = {CAP_MOUNT_MGT, CAP_PRIV_PORT, CAP_MAC_READ};
	cap_value_t cap_audit_write = CAP_AUDIT_WRITE;

	for (mntl = current_mounts; mntl; mntl = mntl->mntl_next) {
		mnt = mntl->mntl_mnt;
		if (strcmp(mnt->mnt_type, MNTTYPE_AUTOFS) != 0)
			continue;

		/*
		 * Don't unmount autofs mounts done
		 * from the autofs mount command.
		 * How do we tell them apart ?
		 * Autofs mounts not eligible for auto-unmount
		 * have the "nest" pseudo-option.
		 */
		if (hasmntopt(mnt, "nest") != NULL)
			continue;

		current = 0;
		for (dir = dir_head; dir; dir = dir->dir_next) {
			if (strcmp(dir->dir_name, mnt->mnt_dir) == 0) {
				current = strcmp(dir->dir_map, "-null");
				break;
			}
		}
		if (current)
			continue;


		ocap = cap_acquire(_CAP_NUM(umount_caps), umount_caps);
		if (umount(mnt->mnt_dir) == 0) {
			del_mtab(mnt->mnt_dir);
			if (verbose) {
				pr_msg("%s unmounted",
					mnt->mnt_dir);
			}
			count++;
			ocap = cap_acquire(_CAP_NUM(cap_audit_write), &cap_audit_write);
			satvwrite(SAT_AE_MOUNT, SAT_SUCCESS,
				  "autofs: umount of %s : %s", mnt->mnt_dir, strerror(errno));
			cap_surrender(ocap);
		}
	}
	if (verbose && count == 0)
		pr_msg("no unmounts");
}

/*
 * Print an error.
 * Works like printf (fmt string and variable args)
 * except that it will subsititute an error message
 * for a "%m" string (like syslog).
 */
void
pr_msg(fmt, va_alist)
	char *fmt;
	va_dcl
{
	va_list ap;
	char buf[BUFSIZ], *p2;
	char *p1;
	extern int errno, sys_nerr;
	extern char *sys_errlist[];

	strcpy(buf, "autofs: ");
	p2 = buf + strlen(buf);

	fmt = gettext(fmt);

	for (p1 = fmt; *p1; p1++) {
		if (*p1 == '%' && *(p1+1) == 'm') {
			if (errno < sys_nerr) {
				(void) strcpy(p2, sys_errlist[errno]);
				p2 += strlen(p2);
			}
			p1++;
		} else {
			*p2++ = *p1;
		}
	}
	if (p2 > buf && *(p2-1) != '\n')
		*p2++ = '\n';
	*p2 = '\0';

	va_start(ap);
	(void) vfprintf(stderr, buf, ap);
	va_end(ap);
}
