/*
 *  remote.c
 *
 *  Description:
 *      Remote mount a file system via NFS
 *
 *  History:
 *      rogerc      01/29/91    Created
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <mntent.h>
#include <netdb.h>
#include <errno.h>
#include <rpc/rpc.h>
#include <netinet/in.h>
#include "nfs_prot.h"
#include "iso_types.h"
#include <sys/fs/nfs_clnt.h>
#include <sys/mount.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/fstyp.h>
#include "main.h"
#include "remote.h"

#define TIMEOUTINT  10

static int
nopt( struct mntent *mnt, char *opt );

void
remote_mount( struct mntent *mntp )
{
	struct sockaddr_in  sin;
	struct hostent      *hp;
	char                *cp, host[MNTMAXSTR], *hostp = host, *path;
	struct nfs_args     args;
	int                 s, mflags, type;
	enum clnt_stat      rpc_stat;
	struct timeval      timeout;
	CLIENT              *client;
	rootres             rr;

	cp = mntp->mnt_fsname;
	while ((*hostp = *cp) != ':') {
		if (*cp == '\0') {
			fprintf( stderr,
			 "%s: remote file system; use host:path\n", progname );
			exit (1);
		}
		hostp++;
		cp++;
	}
	*hostp = '\0';
	path = ++cp;

	if ((hp = gethostbyname( host )) == NULL) {
		if ((hp = gethostbyname( host )) == NULL) {
			fprintf( stderr, "%s: %s not in hosts database\n",
			 progname, host );
			exit (1);
		}
	}

	args.flags = 0;
	/*
	 *  Make mounts soft by default; it's too easy to eject a disc
	 */
	if (hasmntopt( mntp, MNTOPT_SOFT ) || !hasmntopt( mntp, MNTOPT_HARD )) {
		args.flags |= NFSMNT_SOFT;
	}
	if (hasmntopt( mntp, MNTOPT_NOINTR )) {
		args.flags |= NFSMNT_NOINT;
	}
	if (hasmntopt( mntp, MNTOPT_NOAC )) {
		args.flags |= NFSMNT_NOAC;
	}
	if (hasmntopt( mntp, MNTOPT_PRIVATE )) {
		args.flags |= NFSMNT_PRIVATE;
	}

	/*
	 *  Get fhandle of remote path from servers mount_gfs, using
	 *  sgi_nfs program number
	 */
	bzero( &sin, sizeof (sin) );
	bcopy( hp->h_addr, &sin.sin_addr, hp->h_length );
	sin.sin_family = AF_INET;
	timeout.tv_usec = 0;
	timeout.tv_sec = TIMEOUTINT;
	s = RPC_ANYSOCK;

	if ((client = clntudp_create( &sin, (u_long)SGI_NFS_PROGRAM,
	 (u_long)SGI_NFS_VERSION, timeout, &s )) == NULL) {
		fprintf( stderr, "%s: %s server not responding\n",
		 progname, mntp->mnt_fsname );
		clnt_pcreateerror( "" );
		exit (1);
	}
	client->cl_auth = authunix_create_default( );
	timeout.tv_usec = 0;
	timeout.tv_sec = TIMEOUTINT;

	rpc_stat = clnt_call( client, SGI_NFSPROC_ROOT, xdr_nfspath, &path,
	 xdr_rootres, &rr, timeout );

	if (rpc_stat != RPC_SUCCESS) {
		fprintf( stderr, "%s: %s server not responding\n", progname,
		 mntp->mnt_fsname );
		exit (1);
	}
	clnt_destroy( client );

	errno = rr.status;
	if (errno) {
		if (errno == EACCES) {
			fprintf( stderr, "%s: access denied for %s:%s\n",
			 progname, host, path );
		}
		else {
			fprintf( stderr, "%s: ", progname );
			perror( mntp->mnt_fsname );
		}
		exit (1);
	}

	mflags = MS_FSS|MS_DATA|MS_RDONLY;
	type = sysfs(GETFSIND, FSID_NFS);
	if (type < 0) {
		fprintf(stderr, "%s: NFS is not installed.\n", progname);
		exit(1);
	}

	/*
	 *  Set mount args
	 */
	args.fh = (fhandle_t *)&rr.rootres_u.file;
	args.hostname = host;
	args.flags |= NFSMNT_HOSTNAME;
	if (args.rsize = nopt(mntp, "rsize")) {
		args.flags |= NFSMNT_RSIZE;
	}
	if (args.wsize = nopt(mntp, "wsize")) {
		args.flags |= NFSMNT_WSIZE;
	}
	if (args.timeo = nopt(mntp, "timeo")) {
		args.flags |= NFSMNT_TIMEO;
	}
	if (args.retrans = nopt(mntp, "retrans")) {
		args.flags |= NFSMNT_RETRANS;
	}
	if (args.acregmin = nopt(mntp, "actimeo")) {
		args.acregmax = args.acregmin;
		args.acdirmin = args.acregmin;
		args.acdirmax = args.acregmin;
		args.flags |= NFSMNT_ACREGMIN;
		args.flags |= NFSMNT_ACREGMAX;
		args.flags |= NFSMNT_ACDIRMIN;
		args.flags |= NFSMNT_ACDIRMAX;
	}
	else {
		if (args.acregmin = nopt(mntp, "acregmin")) {
			args.flags |= NFSMNT_ACREGMIN;
		}
		if (args.acregmax = nopt(mntp, "acregmax")) {
			args.flags |= NFSMNT_ACREGMAX;
		}
		if (args.acdirmin = nopt(mntp, "acdirmin")) {
			args.flags |= NFSMNT_ACDIRMIN;
		}
		if (args.acdirmax = nopt(mntp, "acdirmax")) {
			args.flags |= NFSMNT_ACDIRMAX;
		}
	}
#ifdef FSID_ISO9660
	args.flags |= NFSMNT_BASETYPE;
	strcpy(args.base, FSID_ISO9660);
#endif

	args.addr = &sin;

	if (mount( mntp->mnt_dir, mntp->mnt_dir, mflags, type, &args,
	 sizeof (args) ) < 0) {
		fprintf( stderr, "%s: %s on ", progname, mntp->mnt_dir );
		perror( mntp->mnt_fsname );
		exit (1);
	}
}

/*
 * Return the value of a numeric option of the form foo=x, if
 * option is not found or is malformed, return 0.
 */
static int
nopt( struct mntent *mnt, char *opt )
{
	int val = 0;
	char *equal, *index();
	char *str;

	if (str = hasmntopt(mnt, opt)) {
		if (equal = index(str, '=')) {
			val = atoi(&equal[1]);
		} else {
			(void) fprintf(stderr, "%s: bad numeric option '%s'\n",
			 progname, str );
		}
	}
	return (val);
}
