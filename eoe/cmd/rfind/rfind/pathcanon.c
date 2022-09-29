#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <mntent.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <limits.h>
#include <sys/param.h>
#include <sys/fsid.h>
#include <sys/fstyp.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/buf.h>
#include <netdb.h>
#include <rpc/rpc.h>
#include <sys/fs/nfs.h>
#include <sys/socket.h>
#include "externs.h"

char	MTAB[] = "/etc/mtab";

static char *pathslinkeval(char *nm);

 struct mntent *
getmntbydev(dev_t dev, char *file)
{
	FILE *mtabp;			/* /etc/mtab */
	struct mntent *mntp;		/* current mtab entry */
	static struct mntent nfsmnt;	/* closest NFS ancestor mntp */
	struct mntent *nfsmntp = NULL;	/* set to &nfsmnt when nfsmnt valid */
	char *nfsdir = NULL;		/* dup mnt_dir for nfsmntp */

	if ((mtabp = setmntent(MTAB, "r")) == NULL)
		return NULL;
	while ((mntp = getmntent(mtabp)) != NULL) {
		struct stat64 statbuf;

		if ( ! isancestor (mntp->mnt_dir, file, strlen(mntp->mnt_dir)) )
			continue;
		if (!ping_nfs_server(mntp))
			continue;
		if (stat64(mntp->mnt_dir, &statbuf) < 0)
			continue;
		if (statbuf.st_dev == dev)
			return mntp;
		if (strncmp(mntp->mnt_type, MNTTYPE_NFS, strlen(MNTTYPE_NFS)) == 0) {
			if (nfsmntp == NULL
			 || isancestor (nfsdir, mntp->mnt_dir, strlen(nfsdir))
			) {
			    /*
			     * getmntent() calls rewrite not only the static
			     * struct mntent, but also the memory holding the
			     * strings pointed to from mntent.
			     */
			    if (nfsmntp != NULL) {
				free ((void *) nfsmnt.mnt_fsname);
				free ((void *) nfsmnt.mnt_dir);
				free ((void *) nfsmnt.mnt_type);
				free ((void *) nfsmnt.mnt_opts);
				nfsmntp = NULL;
			    }
			    nfsmnt.mnt_fsname = strdup (mntp->mnt_fsname);
			    nfsmnt.mnt_dir = strdup (mntp->mnt_dir);
			    nfsmnt.mnt_type = strdup (mntp->mnt_type);
			    nfsmnt.mnt_opts = strdup (mntp->mnt_opts);
			    nfsmntp = &nfsmnt;
			    if (nfsdir) free (nfsdir);
			    nfsdir = strdup (mntp->mnt_dir);
			}
		}
	}
	if (nfsdir) free (nfsdir);
	endmntent(mtabp);
	return nfsmntp;
}

/*
 * pathcanon - Convert path to canonical form "host:fullpath" -- where:
 *
 *	host is the network host serving the referenced file
 *	(host is "localhost" for EFS files, and a node name
 *	for NFS mounted files).
 *
 *	fullpath is root based, with no symbolic links.
 *
 *	Quietly return NULL if unable for any reason to do this.
 *	(P.S. - now perror if stat fails - tell user why "rfind foo ..." fails.)
 *	(Though it invokes subroutines which may exit noisely on error.)
 *	Returns pointer to static buffer, overwritten each call.
 */

#define CANON_LEN MAXHOSTNAMELEN+2+PATH_MAX

 char *
pathcanon (char *inpath)
{
	struct statvfs64 statvfs64buf;	/* stat the file system into here */
	static char result[CANON_LEN];	/* return "host:fullpath" in here */
	struct stat64 statbuf;		/* stat the file "inpath" into here */
	char *fixedpath;		/* inpath fixed: root based, no symlinks */
	char *host;			/* network server for this file */
	char *localmnt;			/* local mount point for this fs */
	char *remotemnt;		/* remote mount point on host for this fs */

	if (stat64(inpath, &statbuf) < 0) {
		perror (inpath);
		return NULL;
	}

	if (statvfs64 (inpath, &statvfs64buf) < 0)
		return NULL;

	if ((fixedpath = pathslinkeval (inpath)) == NULL)
		return NULL;

	if (strncmp(statbuf.st_fstype, FSID_NFS, strlen(FSID_NFS)) == 0) {
		struct mntent *mntp;
		char *cp;

		if ((mntp = getmntbydev(statbuf.st_dev, fixedpath)) == NULL)
			return NULL;

		if ((cp = strchr (mntp->mnt_fsname, ':')) == NULL)
			return NULL;

		*cp++ = 0;

		host = mntp->mnt_fsname;
		localmnt = mntp->mnt_dir;
		remotemnt = cp;

	} else if (statvfs64buf.f_flag & ST_LOCAL) {
		host = "localhost";
		/*
		 * localmnt and remotemnt should both be the mount
		 * point of the file system containing the file "inpath",
		 * but so long as they are equal and both prefixes
		 * of fixedpath - the result is the same in the "efs" and "xfs"
		 * case.
		 * So we cheat and just set them both to "/".
		 */
		localmnt = "/";
		remotemnt = "/";

	} else {
		return NULL;
	}

	if ( ! isancestor (localmnt, fixedpath, strlen(localmnt)) )
		return NULL;

	if (strlen(remotemnt) + strlen(fixedpath) - strlen(localmnt) + 2 > sizeof result)
		return NULL;

	strcpy (result, host);
	strcat (result, ":");
	if (strcmp (remotemnt, "/") != 0) strcat (result, remotemnt);
	if (strcmp (localmnt, "/") != 0)
		strcat (result, fixedpath + strlen (localmnt));
	else
		strcat (result, fixedpath);

	return result;
}

/*
 * NFS-specific code.
 */

#define	WAIT	1	/* wait one second before first retransmission */
#define	TOTAL	3	/* wait no more than three seconds (two tries) */

int
ping_nfs_server(struct mntent *mntp)
{
	char *fsname, *cp;
	ulong_t len;
	int sock;
	static ulong_t hostlen;
	static char host[MAXHOSTNAMELEN+1];
	struct hostent *hp;
	struct sockaddr_in sin;
	struct timeval tv;
	CLIENT *client;
	static enum clnt_stat clnt_stat;

	if (strncmp(mntp->mnt_type, MNTTYPE_NFS, strlen(MNTTYPE_NFS)) != 0)
		return 1;

	fsname = mntp->mnt_fsname;
	cp = strchr(fsname, ':');
	if (cp == NULL)
		return 1;
	len = cp - fsname;
	if (len >= sizeof host)
		return 1;
	if (len == hostlen && strncmp(fsname, host, len) == 0)
		return clnt_stat != RPC_TIMEDOUT;
	hostlen = len;
	(void) strncpy(host, fsname, len);
	host[len] = '\0';

	hp = gethostbyname(host);
	if (hp == NULL || hp->h_addrtype != AF_INET)
		return 1;
	sin.sin_family = AF_INET;
	sin.sin_port = NFS_PORT;
	bcopy(hp->h_addr, &sin.sin_addr, sizeof sin.sin_addr);
	bzero(sin.sin_zero, sizeof sin.sin_zero);

	tv.tv_sec = WAIT;
	tv.tv_usec = 0;
	sock = RPC_ANYSOCK;
	client = clntudp_create(&sin, NFS_PROGRAM, NFS_VERSION, tv, &sock);
	if (client == NULL)
		return 1;

	tv.tv_sec = TOTAL;
	clnt_stat = clnt_call(client, RFS_NULL, xdr_void, 0, xdr_void, 0, tv);
	clnt_destroy(client);
	if (clnt_stat == RPC_TIMEDOUT) {
		fprintf(stderr, "rfind: %s: NFS server %s not responding\n",
			mntp->mnt_dir, host);
		return 0;
	}
	return 1;
}

/*
 * pathslinkeval - Path Symbolic Link Evaluate - translate a path into
 * the equivalent path which does not refer to any symbolic links.
 *
 *	Displays error message and exits if unable to do this.
 *	Returns pointer to static buffer, overwritten each call.
 *
 * Tutorial: this routine gets rid of embedded symlinks, as well
 *	as leaf node symlinks.  This is not the same as RCS uses
 *	internally (see rcsedit.c:resolve_symlink).  RCS just
 *	does repeated readlinks on the whole path, in order to
 *	resolve a path to a real file, so that it can use syscalls
 *	like rename, link, unlink, ... on the "real" file and place
 *	locks in the directory containing this file.  It is content
 *	to let the kernel resolve intermediate symbolic links in
 *	the path to the real file.  But in the case of rfind, when
 *	I need to perform textual comparisons against mount points
 *	in /etc/mtab and map local NFS mounted files across non
 *	isomorphic mount points to the corresponding file on the
 *	server, then I need to fully resolve any symlink anywhere
 *	in the path.  This same full path evaluation is also needed
 *	in the rfindd daemon, when resolving path arguments to
 *	-root and -[ac]newer, and when -follow flag is on.  See the
 *	routine rfindd.c:xstat() for code that does this.  Since
 *	rfindd is essentially emulating the entire file system in
 *	user code, it cannot make use of the kernel's builtin
 *	translation of such paths.
 */

static char	*
pathslinkeval(char *pathname)
{
	register int llen;		/* length from readlink */
	register int lcnt = 1000;	/* detect infinite symlink loops */
	char buf1[PATH_MAX];		/* mungable copy of passed in pathname */
	static char buf2[2*PATH_MAX];	/* build fixed path here - and return it */
	char linkpath[PATH_MAX];	/* readlink's into here */
	char *restofpath;		/* the still unresolved portion of pathname */
	char *nextcomponent;		/* next component of pathname */
	char *cp;			/* used to trim last component of path */

	if (strlen (pathname) >= sizeof(buf1)) {
		fprintf(stderr,"rfind: path <%s> too long\n", buf2);
		exit(1);
	}
	strcpy (buf1, pathname);
	pathcomp (buf1);
	if (*buf1 != '/') {
		my_getwd (buf2);
		strcat (buf2, "/");
		strcat (buf2, buf1);
		pathcomp (buf2);
		if (strlen (buf2) > sizeof buf1) {
			fprintf(stderr,"rfind: path <%s> too long\n", buf2);
			exit (1);
		}
		strcpy (buf1, buf2);
	}
	restofpath = buf1;
	strcpy (buf2, "/");
	while ((nextcomponent = strtok (restofpath, "/")) != NULL) {
		restofpath = NULL;

		if (--lcnt == 0) {
			fprintf(stderr,"Too many levels of symbolic links: %s\n", buf2);
			exit (1);
		}

		strcat (buf2, "/");
		strcat (buf2, nextcomponent);

		if ((llen = readlink(buf2, linkpath, sizeof(linkpath) - 1)) <= 0)
			continue;
		linkpath[llen] = '\0';

		/* oops - symlink encountered - trim last component back off */
		cp = strrchr (buf2, '/');
		*cp = '\0';

		if ((restofpath = strtok (NULL, "")) != NULL) {
			strcat (linkpath, "/");
			strcat (linkpath, restofpath);
		}
		pathcomp (linkpath);
		if (strlen (linkpath) > sizeof buf1) {
			fprintf(stderr,"symlink resolved path length >= PATH_MAX: %s\n", linkpath);
			exit (1);
		}
		strcpy(buf1, linkpath);
		restofpath = buf1;
		if (*buf1 == '/')
			*buf2 = '\0';
	}
	pathcomp (buf2);

	return buf2;
}
