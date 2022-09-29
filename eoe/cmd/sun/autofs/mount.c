/*
 *	autofs mount.c
 *
 *	Copyright (c) 1988-1993 Sun Microsystems Inc
 *	All Rights Reserved.
 */

#ident	"@(#)mount.c	1.3	93/11/08 SMI"

#include <stdio.h>
#include <unistd.h>
#include <mntent.h>
#include <syslog.h>
#include <getopt.h>
#include <stdlib.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/utsname.h>
#include <sys/fs/autofs.h>
#include <sys/capability.h>
#include <sys/mac.h>
#include <sys/eag.h>
#include <sys/sat.h>

static int mac_enabled;

void addtomtab(struct mntent *);

void usage(void);

main(argc, argv)
	int argc;
	char **argv;
{
	int error;
	int c;
	int mntflags = MS_DATA;
	char *mntpnt, *mapname;
	struct autofs_args ai;
	char *optstring = "ignore,indirect,nest";
	char *options = "";
	int mount_timeout = 300;
	struct mntent mnt;
	char pathbuff[MAXPATHLEN + 1];
	char *p;
	cap_t ocap;
	const cap_value_t cap_mount_mgt[] = {CAP_MOUNT_MGT, CAP_PRIV_PORT};
	cap_value_t cap_audit_write = CAP_AUDIT_WRITE;
        char *eagopt;

	/* see if MAC is enabled on this system */
	mac_enabled = (sysconf(_SC_MAC) > 0);

	while ((c = getopt (argc, argv, "o:")) != EOF) {
		switch (c) {
		case '?':
			usage();
			exit(1);

		case 'o':
			options = optarg;
			break;

		default:
			usage();
		}
	}
	fprintf(stderr, "mount_autofs\n");
	if (argc - optind != 2)
		usage();

	if (getuid() != 0) {
		(void) fprintf(stderr, "must be root\n");
		exit(1);
	}

	mapname = argv[optind];
	mntpnt  = argv[optind + 1];

	if (strcmp(mntpnt, "/-") == 0) {
		(void) fprintf(stderr, "invalid mountpoint: /-\n");
		exit(1);
	}

	/*
	 * If we're being mounted by autofsd
	 * then path will be space-terminated.
	 * Need to trim the space from path
	 * in autofs mount arg.
	 */
	(void) strcpy(pathbuff, mntpnt);
	p = pathbuff + (strlen(pathbuff) - 1);
	if (*p == ' ')
		*p = '\0';

	ai.path		= pathbuff;
	ai.opts		= options;
	ai.map		= mapname;
	ai.direct	= 0;		/* can't do direct mounts */
	ai.mount_to	= mount_timeout;
	ai.rpc_to	= 10; /* for calls to autofsd (sec) */

	mnt.mnt_opts = options;
        if (eagopt = hasmntopt(&mnt, MNTOPT_EAG)) {
                char *cp;
                eagopt = strdup(eagopt);
                if (cp = strchr(eagopt, ','))
                        *cp = '\0';
                if (cp = strchr(eagopt, ' '))
                        *cp = '\0';
                if (cp = strchr(eagopt, '\t'))
                        *cp = '\0';
        }

	ocap = cap_acquire(_CAP_NUM(cap_mount_mgt), cap_mount_mgt);
        if (eagopt || mac_enabled)
	  error = sgi_eag_mount("", mntpnt, mntflags, MNTTYPE_AUTOFS,
				(char *) &ai, sizeof (ai), eagopt);
	else
	  error = mount("", mntpnt, mntflags, MNTTYPE_AUTOFS, &ai, sizeof (ai));
	cap_surrender(ocap);
	free(eagopt);
	if (error < 0) {
		ocap = cap_acquire(_CAP_NUM(cap_audit_write), &cap_audit_write);
		satvwrite(SAT_AE_MOUNT, SAT_FAILURE,
			  "autofs3: failed mount of %s on %s: %s", ai.path, mntpnt,
			  strerror(error));
		cap_surrender(ocap);		
		perror("autofs mount");
		exit(1);
	}
	else {

	  ocap = cap_acquire(_CAP_NUM(cap_audit_write), &cap_audit_write);
	  satvwrite(SAT_AE_MOUNT, SAT_SUCCESS, "autofs2: mounted %s on %s",
		  ai.path, mntpnt);
	  cap_surrender(ocap);
	}

	mnt.mnt_fsname = ai.map;
	mnt.mnt_dir = mntpnt;
	mnt.mnt_type = MNTTYPE_AUTOFS;
	mnt.mnt_opts = optstring;
	mnt.mnt_freq = mnt.mnt_passno = 0;

	addtomtab(&mnt);

	return(0);
}

#define	TIME_MAX 16

/*
 * Add a new entry to the /etc/mtab file.
 * Include the device id with the mount options.
 */
void
addtomtab(mntp)
	struct mntent *mntp;
{
	FILE *fd;
	char *opts;
	struct stat st;
	char obuff[256], *pb = obuff;

	if (stat(mntp->mnt_dir, &st) < 0) {
		perror(mntp->mnt_dir);
		exit(1);
	}
	fd = setmntent(MOUNTED, "a");
	if (fd == NULL) {
		syslog(LOG_ERR, "%s: %m", MOUNTED);
		exit(1);
	}

	opts = mntp->mnt_opts;
	(void) strcpy(pb, opts);
	pb += strlen(pb);
	(void) sprintf(pb, ",%s=%x", MNTINFO_DEV, st.st_dev);
	mntp->mnt_opts = obuff;

	addmntent(fd, mntp);

	mntp->mnt_opts = opts;
	endmntent(fd);
}

void
usage(void)
{
	(void) fprintf(stderr,
	    "Usage: autofs mount [-o opts]  map  dir\n");
	exit(1);
}
