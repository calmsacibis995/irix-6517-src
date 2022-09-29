/*
** This is a simple daemon to monitor NFS mount points.  We sit in
** a loop around a system call asking for the next timeout, it will
** only return the first time a filesystem has a timeout.  When we
** wake up we fork a child to umount the filesystem.
*/
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <mntent.h>
#include "nfs.h"
#define NFSCLIENT
#include <sys/mount.h>
#include <sys/fs/nfs_clnt.h>
#include <sys/fstyp.h>
#include <sys/fsid.h>
#include <sys/capability.h>
#include <syslog.h>

#define NSD_MTAB
#ifdef NSD_MTAB
/*
 * update /etc/mtab
 */
void
addtomtab(struct mntent *mnt)
{
	FILE *mnted;
	struct mntent mget;
	struct mntent mref;
	int i;
	cap_t ocap;
	cap_value_t cap_mac_write = CAP_MAC_WRITE;

	ocap = cap_acquire (1, &cap_mac_write);
	mnted = setmntent(MOUNTED, "r+");
	cap_surrender (ocap);
	if (mnted == NULL) {
		syslog(LOG_ERR, "cant read %s: %m\n",MOUNTED);
		exit(1);
	}
	/*
	 * add the mount table entry if it is not already there
	 * use getmntany to check for the presence of a mount table entry
	 * check using only the mount point name and the FS type
	 */
	memset( &mref, 0, sizeof( struct mntent ) );
	mref.mnt_dir = mnt->mnt_dir;
	/*
	 * eliminate trailing blanks
	 */
	for (i = strlen(mnt->mnt_dir) - 1; (i >= 0) && (mnt->mnt_dir[i] == ' ');
		i--) {
			mnt->mnt_dir[i] = 0;
	}
	mref.mnt_type = mnt->mnt_type;
	if ((getmntany(mnted, &mget, &mref) == -1) && addmntent(mnted, mnt)) {
		syslog(LOG_ERR, "Cant add %s to %s: %m\n",mnt->mnt_fsname, MOUNTED);
		exit(1);
	}
	(void) endmntent(mnted);
}
#endif

extern int sgi_eag_mount(const char *, const char *, int, int,
			 struct nfs_args *, size_t, const char *);
static int
cap_mount(const char *spec, const char *dir, int mflag, int fstype,
	  struct nfs_args *args, size_t arglen)
{
	const char *eagopt = "eag:mac-ip=dblow:mac-default=dblow";
	const cap_value_t caps[] = {CAP_MOUNT_MGT, CAP_PRIV_PORT};
	cap_t ocap;
	int r;

	ocap = cap_acquire(2, caps);
	r = sgi_eag_mount(spec, dir, mflag, fstype, args, arglen, (sysconf(_SC_MAC) > 0) ? eagopt : (char *) NULL);
	cap_surrender(ocap);
	if (r) {
		syslog(LOG_ERR, "mount failed: %m\n");
	}
	return(r);
}

static int verbose=0;

void
main(int argc, char **argv)
{
	char *program;
	int opt;
	uint64_t fh[8];
	char *dir="/ns";
	int flags, err;
	int usage=0, doxattr=0;
	int nfstype, port=0;
	pid_t ppid;
	struct nfs_args args;
	struct sockaddr_in sin;
#ifdef NSD_MTAB
	struct mntent mntent;
#endif

	if (program = strrchr(argv[0], '/')) {
		program++;
	} else {
		program = argv[0];
	}

	if (getuid() != 0) {
		fprintf(stderr, "%s: must be root to execute\n", program);
		exit(1);
	}

	openlog(program, LOG_PID|LOG_ODELAY|LOG_NOWAIT,LOG_DAEMON);

	memset(fh, 0, sizeof(fh));

	ppid = getppid();

	while ((opt = getopt(argc, argv, "vd:p:i:x")) != EOF) {
		switch (opt) {
		    case 'd':
			dir = optarg;
			break;
		    case 'i':
			ppid = atoi(optarg);
			break;
		    case 'v':
			verbose++;
			break;
		    case 'p':
			port = atoi(optarg);
			break;
		    case 'x':
			doxattr = 1;
			break;
		    default:
			usage++;
		}
	}
	if (usage || !port) {
		fprintf(stderr,
		  "usage: %s [-vx][-d mountpoint][-i pid] -p port\n", program);
		exit(1);
	}

	mkdir(dir, 0755);

	flags = MS_DATA;
	
	if ((nfstype = sysfs(GETFSIND, FSID_NFS3)) < 1) 
	    if ((nfstype = sysfs(GETFSIND, FSID_NFS2)) < 1) 
		if ((nfstype = sysfs(GETFSIND, FSID_NFS)) < 1) {
			err=errno;
			syslog(LOG_ERR, "Can't determine nfs mount type: %m\n");
			exit(err);
		}
	if (nfstype < 0) {
		flags |= MS_FSS;
	} 
		
	if (doxattr) {
		flags |= MS_DOXATTR;
	} else {
		flags |= MS_DEFXATTR;
	}
	sin.sin_family = AF_INET;
	sin.sin_port = htons(port);
	sin.sin_addr.s_addr = INADDR_LOOPBACK;
	args.addr = &sin;
	args.flags = NFSMNT_TIMEO | NFSMNT_HOSTNAME | NFSMNT_RETRANS |
	    NFSMNT_SOFT | NFSMNT_SILENT;
	args.timeo = 100;
	args.retrans = 2;
	args.fh_len = 32;
	args.fh = (fhandle_t *)fh;
	args.hostname = "localhost (nsd)";
	args.flags |= NFSMNT_PID;
	args.pid = ppid;
	if (cap_mount(dir, dir, flags, nfstype, &args, sizeof(args))) {
		err=errno;
		syslog(LOG_ERR,"mount of %s failed: %m\n",dir);
		exit(err);
	}

#ifdef NSD_MTAB
	mntent.mnt_fsname=dir;
	mntent.mnt_dir=dir;
	mntent.mnt_type=malloc(FSTYPSZ);
	if (mntent.mnt_type == NULL) {
		syslog(LOG_ERR, "malloc failed: %m");
		exit(errno);
	}
	mntent.mnt_opts="ignore,rw";
	mntent.mnt_freq=0;
	mntent.mnt_passno=0;
	sysfs(GETFSTYP,nfstype, mntent.mnt_type);
	addtomtab(&mntent);
	free(mntent.mnt_type);
#endif
	exit(0);
}
