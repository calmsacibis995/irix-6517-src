/*
 *	autod_nfs.c
 *
 *	Copyright (c) 1988-1993 Sun Microsystems Inc
 *	All Rights Reserved.
 */

#ident	"@(#)autod_nfs.c	1.11	94/03/09 SMI"

#include <stdio.h>
#include <unistd.h>
#include <ctype.h>
#include <syslog.h>
#include <string.h>
#include <sat.h>
#include <arpa/inet.h>
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/signal.h>
#include <rpc/rpc.h>
#include <rpc/pmap_clnt.h>
#include <sys/mount.h>
#include <sys/fs/export.h>
#include <exportent.h>
#include <mntent.h>
#include <sys/fstyp.h>
#include <sys/fsid.h>
#include <netdb.h>
#include <netconfig.h>
#include <netdir.h>
#include <errno.h>
#include "nfs_prot.h"
typedef nfs_fh fhandle_t;
#include <rpcsvc/mount.h>
#include <rpc/nettype.h>
#define	NFSCLIENT
#include <rpcsvc/mount.h>
#include <sys/fs/nfs_clnt.h>
#include <sys/fs/bds.h>
#include <locale.h>
#include <setjmp.h>
#include <sys/socket.h>
#include "autofsd.h"
#include <sys/utsname.h>
#include <sys/capability.h>
#include <sys/mac.h>
#include <sys/eag.h>
#include <sys/syssgi.h>

extern int mac_enabled;

#define	MAXHOSTS 	512
#define	MAXSUBNETS 	20

#define	NFS3PREFVERS 	0

struct mapfs *find_server(struct mapent *, struct in_addr);
nfsstat nfsmount(char *, struct in_addr, char *, char *, char *, int, int);
void freeex_ent(struct exports *);
void freeex(struct exports *);
int	add_mtab(struct mntent *);
int	nopt(struct mntent *, char *);
int	cap_dostat(int (*) (const char *, struct stat *), const char *,
		   struct stat *);
static char *ismntopt(struct mntent *, char *);

extern char self[64];
extern int ping_cache_time;
extern struct in_addr myaddrs[];

static int rpc_timeout = 20;

static const cap_value_t mount_caps[] = {CAP_MOUNT_MGT, CAP_PRIV_PORT,
					 CAP_MAC_READ};
static const cap_value_t umount_caps[] = {CAP_MOUNT_MGT, CAP_PRIV_PORT,
					  CAP_MAC_READ};


static int
cap_mount_nfs (const char *path, const char *mntpnt, int flags,  char *type,
	       struct nfs_args *args, size_t size, char *eagopt)
{
	cap_t ocap;
	int r;

	ocap = cap_acquire(_CAP_NUM(mount_caps), mount_caps);
	if (eagopt || mac_enabled)
	     r = sgi_eag_mount(path, mntpnt, flags, type, args, size, eagopt);
	else
	     r = mount(path, mntpnt, flags, type, args, size);
	cap_surrender(ocap);
	return(r);
}

static int
cap_mount_lofs (const char *path, const char *mntpnt, int flags, int type,
		struct nfs_args *args, size_t size, char *eagopt)
{
	cap_t ocap;
	int r;

	ocap = cap_acquire(_CAP_NUM(mount_caps), mount_caps);
	if (eagopt || mac_enabled)
	     r = sgi_eag_mount(path, mntpnt, flags, type, args, size, eagopt);
	else
	     r = mount(path, mntpnt, flags, type, args, size);
	cap_surrender(ocap);
	return(r);
}

int cap_umount (mnt)
     struct mntlist *mnt;
{
	cap_t ocap;
	int r,serrno;
	cap_value_t cap_audit_write = CAP_AUDIT_WRITE;

	ocap = cap_acquire(_CAP_NUM(umount_caps), umount_caps);
	r=umount(mnt->mntl_mnt->mnt_dir);
	/* 
	 * Use the syssgi call in case of ESTALE only for now.
	 */
	if (r < 0 && errno == ESTALE) {
		r = syssgi(SGI_NFS_UNMNT, mnt->mntl_dev);
	}
	serrno = errno;
	cap_surrender(ocap);

	ocap = cap_acquire(_CAP_NUM(cap_audit_write), &cap_audit_write);
	if (r < 0) {
		satvwrite(SAT_AE_MOUNT, SAT_FAILURE,
			  "autofs: failed umount of %s : %s", 
			  mnt->mntl_mnt->mnt_dir,
			  strerror(serrno));
	} else {
		satvwrite(SAT_AE_MOUNT, SAT_SUCCESS,
			  "autofs: umounted %s", mnt->mntl_mnt->mnt_dir);
	}
	cap_surrender(ocap);
	return(r);
}

mount_nfs(me, mntpnt, overlay)
	struct mapent *me;
	char *mntpnt;
	int overlay;
{
	struct in_addr prevhost;
	struct mapfs *mfs;
	int err;
	int cached;

	prevhost.s_addr = (u_long)0;
	do {
		mfs = find_server(me, prevhost);
		if (mfs == NULL)
			return (ENOENT);

		if (self_check(mfs->mfs_host) ||
		    mfs->mfs_addr.s_addr == htonl(INADDR_LOOPBACK)) {
			err = loopbackmount(mfs->mfs_dir,
				mntpnt, me->map_mntopts, overlay);
		} else {
			cached = strcmp(me->map_mounter, MNTTYPE_CACHEFS) == 0;
			if (trace > 0)
				syslog(LOG_ERR, "mount_nfs: host %s mntpnt '%s'", 
				       mfs->mfs_host, mntpnt);
			err = nfsmount(mfs->mfs_host, mfs->mfs_addr, 
					mfs->mfs_dir, mntpnt, me->map_mntopts, 
					cached, overlay);
			if (err && trace > 0) {
				(void) fprintf(stderr,
					"Couldn't mount %s:%s\n",
					mfs->mfs_host,
					mfs->mfs_dir);
			}
		}
		if (err) {
			mfs->mfs_ignore = 1;
		}

	} while (err);

	return (0);
}

struct mapent *
do_mapent_hosts(mapopts, host)
	char *mapopts, *host;
{
	struct mapent *me, *ms, *mp;
	struct mapfs *mfs;
	struct exports *ex = NULL;
	struct exports *exlist, *texlist, **texp, *exnext;
	struct exportlist *el = 0;
	enum clnt_stat clnt_stat;
	char name[MAXPATHLEN];
	char entryopts[B_SIZE];
	char fstype[32], mounter[32];
	int elen;
	int exlen, duplicate;
	struct hostent h;
	CLIENT *cl;
	int herr;
	char buf[4096];

	if (! gethostbyname_r(host, &h, buf, sizeof(buf), &herr))
		return ((struct mapent *)NULL);

	/* check for special case: host is me */

	if (self_check(host)) {
		ms = (struct mapent *)malloc(sizeof (*ms));
		if (ms == NULL)
			goto alloc_failed;
		memset((void *) ms, '\0', sizeof (*ms));
		(void) strcpy(fstype, MNTTYPE_NFS);
		get_opts(mapopts, entryopts, fstype);
		ms->map_mntopts = strdup(entryopts);
		if (ms->map_mntopts == NULL)
			goto alloc_failed;
		ms->map_mounter = strdup(fstype);
		if (ms->map_mounter == NULL)
			goto alloc_failed;
		ms->map_fstype = strdup(MNTTYPE_NFS);
		if (ms->map_fstype == NULL)
			goto alloc_failed;

		(void) strcpy(name, "/");
		(void) strcat(name, host);
		ms->map_root = strdup(name);
		if (ms->map_root == NULL)
			goto alloc_failed;
		ms->map_mntpnt = strdup("");
		if (ms->map_mntpnt == NULL)
			goto alloc_failed;
		mfs = (struct mapfs *)malloc(sizeof (*mfs));
		if (mfs == NULL)
			goto alloc_failed;
		memset((void *) mfs, '\0', sizeof (*mfs));
		ms->map_fs = mfs;
		mfs->mfs_host = strdup(host);
		if (mfs->mfs_host == NULL)
			goto alloc_failed;
		mfs->mfs_addr = myaddrs[0];
		mfs->mfs_dir  = strdup("/");
		if (mfs->mfs_dir == NULL)
			goto alloc_failed;
		return (ms);
	}

	if (pingnfs(*(struct in_addr *)h.h_addr) != RPC_SUCCESS)
		return ((struct mapent *) NULL);


	/* First try to use the SGI protocol to get our export options
	 * and check for "nohide" filesystems.
	 * Try to use TCP rather than UDP.
	 */
	if ((cl=clnt_create(host, MOUNTPROG_SGI, MOUNTVERS_SGI_ORIG, "tcp")) ||
	    (cl=clnt_create(host, MOUNTPROG_SGI, MOUNTVERS_SGI_ORIG, "udp"))) {
		struct timeval timeout;

		timeout.tv_usec = 0;
		timeout.tv_sec = 5;
		clnt_stat = clnt_call(cl, MOUNTPROC_EXPORTLIST, xdr_void, 0,
					xdr_exportlist, &el, timeout);
		clnt_destroy(cl); /* free it, but do not null it */
	}

	if (cl && !clnt_stat) {
		struct exportlist *sortel, **telp, *ele, *elenext;

		/* nothing exported */
		if (el == 0) {
			if (trace > 1)
				(void) fprintf(stderr,
			    	"do_mapent_hosts: null export list\n");
			return ((struct mapent *)NULL);
		}

		/* now sort by length of names - to get mount order right */
		sortel = 0;
		telp = &sortel;
		for (ele = el; ele; ele = elenext) {
			elenext = ele->el_next;
			elen = strlen(ele->el_entry.ee_dirname);
			for (telp = &sortel; *telp;telp = &((*telp)->el_next))
				if (elen<strlen((*telp)->el_entry.ee_dirname))
					break;
			ele->el_next = *telp;
			*telp = ele;
		}

		texlist = 0;
		texp = &texlist;
		for (ele = sortel; ele; ele = elenext) {
			ex = (struct exports *)malloc(sizeof *ex);
			if (!ex)    /* (ignore sortel mem leak) */
				goto alloc_failed;
			memset((void *) ex, '\0', sizeof (*ex));
			ex->ex_name=strdup(ele->el_entry.ee_dirname);
			if (!ex->ex_name) /* ignore sortel mem leak */
				goto alloc_failed;
			/* 
			 * Just reuse the pingnfs lock. It gets used only in 
			 * the pingnfs function above anyway. getexportopt is 
			 * not thread safe.
			 */
			AUTOFS_LOCK(pingnfs_lock);
			if (getexportopt((struct exportent*)&ele->el_entry,
					  NOHIDE_OPT)) {
				ex->ex_flags = EX_NOHIDE;
			}
			AUTOFS_UNLOCK(pingnfs_lock);
			*texp = ex;
			texp = &ex->ex_next;
			elenext = ele->el_next;
			free(ele->el_entry.ee_dirname);
			free(ele->el_entry.ee_options);
			free(ele);
		}

	} else {

		/* otherwise, use the old protocol
		 */
		if (trace > 1)
			(void)fprintf(stderr, "%s: SGI exports: %s", host,
				      clnt_sperrno(clnt_stat));
		/* get export list of host */
		if (clnt_stat = callrpc(host, MOUNTPROG,
					MOUNTVERS, MOUNTPROC_EXPORT,
					xdr_void, 0, xdr_exports, &ex)) {
			syslog(LOG_ERR, "%s: exports: %s", host,
			       clnt_sperrno(clnt_stat));
			return ((struct mapent *) NULL);
		}
		if (ex == NULL) {
			if (trace > 1)
				(void) fprintf(stderr,
			    	"do_mapent_hosts: null export list\n");
			return ((struct mapent *)NULL);
		}

		/* now sort by length of names - to get mount order right */
		exlist = ex;
		texlist = NULL;
		for (; ex; ex = exnext) {
			exnext = ex->ex_next;
			exlen = strlen(ex->ex_name);
			duplicate = 0;
			for (texp = &texlist; *texp; texp = &((*texp)->ex_next)) {
				if (exlen < (int) strlen((*texp)->ex_name))
					break;
				if (duplicate = (strcmp(ex->ex_name,
					(*texp)->ex_name) == 0)) {

					/* disregard duplicate entry */
					freeex_ent(ex);
					break;
				}
			}
			if (!duplicate) {
				ex->ex_next = *texp;
				*texp = ex;
			}
		}
	}
	exlist = texlist;

	/*
	 * The following ugly chunk of code crept in as
	 * a result of cachefs.  If it's a cachefs mount
	 * of an nfs filesystem, then have it handled as
	 * an nfs mount but have cachefs do the mount.
	 */
	(void) strcpy(fstype, MNTTYPE_NFS);
	get_opts(mapopts, entryopts, fstype);
	(void) strcpy(mounter, fstype);
	if (strcmp(fstype, MNTTYPE_CACHEFS) == 0) {
		struct mntent m;
		char *p;

		m.mnt_opts = entryopts;
		if ((p = hasmntopt(&m, "backfstype")) != NULL) {
			int len = strlen(MNTTYPE_NFS3);

			p += 11;
			/*
			 * Cached nfs mount?
			 */
			if (strncmp(p, MNTTYPE_NFS3, len) == 0 && 
			    (p[len] == '\0' || p[len] == ',')) {
				strncpy(fstype, p, len);
				fstype[len] = '\0';
			} else if ((strncmp(p, MNTTYPE_NFS, len-1) == 0 &&
				  (p[len-1] == '\0' || p[len-1] == ',')) ||
				(strncmp(p, MNTTYPE_NFS2, len) == 0 &&
				(p[len] == '\0' || p[len] == ','))) {
				strcpy(fstype, MNTTYPE_NFS);
			}
		}
	}

	/* Now create a mapent from the export list */
	ms = NULL;
	me = NULL;

	for (ex = exlist; ex; ex = ex->ex_next) {
		mp = me;
		me = (struct mapent *)malloc(sizeof (*me));
		if (me == NULL)
			goto alloc_failed;
		memset((void *) me, '\0', sizeof (*me));

		if (ms == NULL)
			ms = me;
		else
			mp->map_next = me;

		(void) strcpy(name, "/");
		(void) strcat(name, host);
		me->map_root = strdup(name);
		if (me->map_root == NULL)
			goto alloc_failed;
		if (strcmp(ex->ex_name, "/") == 0)
			me->map_mntpnt = strdup("");
		else
			me->map_mntpnt = strdup(ex->ex_name);
		if (me->map_mntpnt == NULL)
			goto alloc_failed;

		me->map_fstype = strdup(fstype);
		if (me->map_fstype == NULL)
			goto alloc_failed;
		me->map_mounter = strdup(mounter);
		if (me->map_mounter == NULL)
			goto alloc_failed;
		me->map_mntopts = strdup(entryopts);
		if (me->map_mntopts == NULL)
			goto alloc_failed;
		me->map_exflags = ex->ex_flags;

		mfs = (struct mapfs *)malloc(sizeof (*mfs));
		if (mfs == NULL)
			goto alloc_failed;
		memset((void *) mfs, '\0', sizeof(*mfs));
		me->map_fs = mfs;
		mfs->mfs_host = strdup(host);
		if (mfs->mfs_host == NULL)
			goto alloc_failed;
		mfs->mfs_addr = *(struct in_addr *)h.h_addr;
		mfs->mfs_dir  = strdup(ex->ex_name);
		if (mfs->mfs_dir == NULL)
			goto alloc_failed;
	}
	freeex(exlist);
	return (ms);

alloc_failed:
	syslog(LOG_ERR, "Memory allocation failed: %m");
	free_mapent(ms);
	freeex(exlist);
	return ((struct mapent *) NULL);
}

/*
 * This function parses the map entry for a nfs type file system
 * The input is the string lp (and lq) which can be one of the
 * following forms:
 *
 * a) host,host... :/directory
 * b) host:/directory[,host:/directory]...
 * This routine constructs a mapfs link-list for each of
 * the hosts and the corresponding file system. The list
 * is then attatched to the mapent struct passed in.
 */
parse_nfs(mapname, me, w, wq, lp, lq)
	struct mapent *me;
	char *mapname, *w, *wq, **lp, **lq;
{
	struct mapfs *mfs, **mfsp;
	char *wlp, *wlq;
	char *hl, hostlist[B_SIZE], *hlq, hostlistq[B_SIZE];
	char hostname[MXHOSTNAMELEN+1];
	char dirname[MAXPATHLEN+1], subdir[MAXPATHLEN+1];
	char qbuff[MAXPATHLEN+1];

	mfsp = &me->map_fs;
	*mfsp = NULL;

	while (*w && *w != '/') {
		wlp = w; 
		wlq = wq;
		getword(hostlist, hostlistq, &wlp, &wlq, ':', sizeof(hostlist));
		if (!*hostlist)
			goto bad_entry;
		getword(dirname, qbuff, &wlp, &wlq, ':', sizeof(dirname));
		if (*dirname != '/')
			goto bad_entry;
		*subdir = '/'; 
		*qbuff = ' ';
		getword(subdir+1, qbuff+1, &wlp, &wlq, ':', sizeof(subdir)-1);
		if (*(subdir+1))
			(void) strcat(dirname, subdir);

		hl = hostlist; 
		hlq = hostlistq;
		for (;;) {
			getword(hostname, qbuff, &hl, &hlq, ',', sizeof(hostlist));
			if (!*hostname)
				break;
			mfs = (struct mapfs *)malloc(sizeof (*mfs));
			if (mfs == NULL)
				return(-1);
			memset((void *) mfs, '\0', sizeof(*mfs));
			*mfsp = mfs;
			mfsp = &mfs->mfs_next;

			mfs->mfs_host = strdup(hostname);
			if (mfs->mfs_host == NULL) {
				free(mfs);
				return(-1);
			}
			mfs->mfs_dir = strdup(dirname);
			if (mfs->mfs_dir == NULL) {
				free(mfs->mfs_host);
				free(mfs);
				return(-1);
			}
		}
		getword(w, wq, lp, lq, ' ', B_SIZE);
	}
	return (0);

bad_entry:
	syslog(LOG_ERR, "bad entry in map %s \"%s\"", mapname, w);
	return (1);
}

static int
getsubnet_byaddr(struct in_addr *ptr, u_int *subnet)
{
	int  j;
	u_long i, netmask;
	u_char *bytes;
	u_int u[4];
	struct in_addr net;
	char key[128], *mask;

	i = ntohl(ptr->s_addr);
	bytes = (u_char *)(&net);
	if (IN_CLASSA(i)) {
		net.s_addr = htonl(i & IN_CLASSA_NET);
		sprintf(key, "%d.0.0.0", bytes[0]);
	} else 	if (IN_CLASSB(i)) {
		net.s_addr = htonl(i & IN_CLASSB_NET);
		sprintf(key, "%d.%d.0.0", bytes[0], bytes[1]);
	} else 	if (IN_CLASSC(i)) {
		net.s_addr = htonl(i & IN_CLASSC_NET);
		sprintf(key, "%d.%d.%d.0", bytes[0], bytes[1], bytes[2]);
	}
	if (getnetmask_byaddr(key, &mask) != 0)
		return (-1);

	bytes = (u_char *) (&netmask);
	sscanf(mask, "%d.%d.%d.%d", &u[0], &u[1], &u[2], &u[3]);
	free(mask);
	for (j = 0; j < 4; j++)
		bytes[j] = u[j];
	netmask = ntohl(netmask);
	if (IN_CLASSA(i))
		*subnet = IN_CLASSA_HOST & netmask & i;
	else if (IN_CLASSB(i)) {
		*subnet = IN_CLASSB_HOST & netmask & i;
	} else if (IN_CLASSC(i))
		*subnet = IN_CLASSC_HOST & netmask & i;

	return (0);
}

/*
 * This function is called to get the subnets to which the
 * host is connected to. Since the subnets to which the hosts
 * is attached is not likely to change while the automounter
 * is running, this computation is done only once. So only
 * my_subnet_cnt is protected by a semaphore.
 */
static u_int *
get_myhosts_subnets(void)
{
	static int my_subnet_cnt = 0;
	static u_int my_subnets[MAXSUBNETS + 1];
	struct hostent myhost;
	struct in_addr *ptr;
	int herr;
	char buf[4096];

	AUTOFS_LOCK(mysubnet_hosts);
	if (my_subnet_cnt)
		goto done;
	if (! gethostbyname_r(self, &myhost, buf, sizeof(buf), &herr))
		goto done;
	while ((ptr = (struct in_addr *) *myhost.h_addr_list++) != NULL) {
		if (my_subnet_cnt < MAXSUBNETS) {
			if (getsubnet_byaddr(ptr,
				&my_subnets[my_subnet_cnt]) == 0)
				my_subnet_cnt++;
		}
	}
	my_subnets[my_subnet_cnt] = (u_int) NULL;
done:
	AUTOFS_UNLOCK(mysubnet_hosts);
	return (my_subnets);

}

/*
 * Given a host-entry, check if it matches any of the subnets
 * to which the localhost is connected to
 */
static int
subnet_matches(u_int *mysubnets, struct hostent *hs)
{
	struct in_addr *ptr;
	u_int subnet;

	while ((ptr = (struct in_addr *) *hs->h_addr_list++) != NULL) {
		if (getsubnet_byaddr(ptr, &subnet) == 0)
			while (*mysubnets != (u_int) NULL)
				if (*mysubnets++ == subnet)
					return (1);
	}

	return (0);
}

/*
 * Given a list of servers find all the servers who are
 * on the same subnet(s)  as the local host
 */
get_mysubnet_servers(mfs_head, hosts_ptr)
struct mapfs *mfs_head;
struct host_names *hosts_ptr;
{
	u_int *mysubnets = (u_int *) NULL;
	struct mapfs *mfs;
	struct hostent hs;
	int cnt;
	int herr;
	char buf[4096];

	cnt = 0;
	mysubnets = get_myhosts_subnets();
	for (mfs = mfs_head; mfs && (cnt < MAXHOSTS); mfs = mfs->mfs_next) {
		if (mfs->mfs_ignore)
			continue;
		if (! gethostbyname_r(mfs->mfs_host, &hs, buf, sizeof(buf), &herr))
			continue;
		if (subnet_matches(mysubnets, &hs)) {
			hosts_ptr->host = mfs->mfs_host;
			hosts_ptr++;
			cnt++;
		}
	}
	hosts_ptr->host = (char *) NULL; /* terminate list */
	return (cnt);

}


get_mynet_servers(mfs_head, hosts_ptr)
	struct mapfs *mfs_head;
	struct host_names *hosts_ptr;
{
	struct mapfs *mfs;
	struct hostent hs;
	int mynet = 0;
	int cnt;
	int herr;
	char buf[4096];

	cnt = 0;
	if (! gethostbyname_r(self, &hs, buf, sizeof(buf), &herr))
		return(cnt);
	mynet = inet_netof(*((struct in_addr *)*hs.h_addr_list));
	for (mfs = mfs_head; mfs && (cnt < MAXHOSTS); mfs = mfs->mfs_next) {
		if (mfs->mfs_ignore)
			continue;
		if (! gethostbyname_r(mfs->mfs_host, &hs, buf, sizeof(buf), &herr))
			continue;
		if (mynet == inet_netof(*((struct in_addr *)
					  *hs.h_addr_list))) {
			hosts_ptr->host = mfs->mfs_host;
			hosts_ptr++;
			cnt++;
		}
	}
	hosts_ptr->host = (char *) NULL; /* terminate lilst */
	return (cnt);
}

struct in_addr gotaddr;

/* ARGSUSED */
catchfirst(raddr)
	struct sockaddr_in *raddr;
{
	gotaddr = raddr->sin_addr;
	return (1);	/* first one ends search */
}

/*
 * ping a bunch of hosts at once and find out who
 * responds first
 */
static struct in_addr
trymany(struct in_addr *addrs, int timeout)
{
	enum clnt_stat clnt_stat;
	extern enum clnt_stat nfs_cast();

	if (trace > 1) {
		register struct in_addr *a;

		(void) fprintf(stderr, "  nfs_cast: ");
		for (a = addrs; a->s_addr; a++)
			(void) fprintf(stderr, "%s ", inet_ntoa(*a));
		(void) fprintf(stderr, "\n");
	}

	gotaddr.s_addr = 0;
	clnt_stat = nfs_cast(addrs, catchfirst, timeout);
	if (trace > 1) {
		(void) fprintf(stderr, "  nfs_cast: got %s\n",
			(int) clnt_stat ? "no response" :
					inet_ntoa(gotaddr));
	}
	if (clnt_stat != RPC_SUCCESS) {
		char buff[2048];
		struct in_addr *a;

		for (a = addrs; a->s_addr; a++) {
			(void) strcat(buff, inet_ntoa(*a));
			if ((a + 1)->s_addr)
				(void) strcat(buff, ",");
		}

		syslog(LOG_ERR, "servers %s not responding: %s",
			buff, clnt_sperrno(clnt_stat));
	}

	return (gotaddr);
}

#ifdef _Sun
/*
 * This function is added to detect compatibility problem with SunOS4.x.
 * The compatibility problem exits when fshost cannot decode the request
 * arguments for NLM_GRANTED procedure.
 * Only in this case  we use local locking.
 * In any other case we use fshost's lockd for remote file locking.
 */
remote_lock(fshost, fh)
	char *fshost;
	caddr_t fh;
{
	nlm_testargs rlm_args;
	nlm_res rlm_res;
	struct timeval timeout = { 5, 0};
	CLIENT *cl;
	enum clnt_stat rpc_stat;
	struct utsname myid;

	memset((void *) &rlm_args, '\0', sizeof(rlm_args));
	/*
	 * Assign the hostname and the file handle for the
	 * NLM_GRANTED request below.  If for some reason the uname call fails,
	 * list the server as the caller so that caller_name has some
	 * reasonable value.
	 */
	if (uname(&myid) == -1)  {
		rlm_args.alock.caller_name = fshost;
	} else {
		rlm_args.alock.caller_name = myid.nodename;
	}
	rlm_args.alock.fh.n_len = sizeof (fhandle_t);
	rlm_args.alock.fh.n_bytes = fh;

	cl = clnt_create(fshost, NLM_PROG, NLM_VERS, "datagram_v");
	if (cl == NULL)
		return (1);

	rpc_stat = clnt_call(cl, NLM_GRANTED,
			xdr_nlm_testargs, (caddr_t)&rlm_args,
			xdr_nlm_res, (caddr_t)&rlm_res, timeout);
	clnt_destroy(cl);

	return (rpc_stat == RPC_CANTDECODEARGS);
}
#endif

struct mapfs *
find_server(me, preferred)
	struct mapent *me;
	struct in_addr preferred;
{
	int entrycount;
	struct mapfs *mfs, *mfs_one;
	struct hostent h;
	struct in_addr addrs[MAXHOSTS], addr, *addrp, trymany();
	int herr;
	char buf[4096];

	/*
	 * last entry reserved for terminating list
	 * in case there are MAXHOST servers
	 */

	/*
	 * get addresses & see if any are myself
	 * or were mounted from previously in a
	 * hierarchical mount.
	 */
	entrycount = 0;
	mfs_one = NULL;
	for (mfs = me->map_fs; mfs; mfs = mfs->mfs_next) {
		if (mfs->mfs_addr.s_addr == 0) {
			if (trace > 0)
				syslog(LOG_ERR, "find_server: gethost %s", 
				    mfs->mfs_host);
			if (gethostbyname_r(mfs->mfs_host, &h, buf, sizeof(buf), &herr))
				memcpy((void *) &mfs->mfs_addr,
				    (void *) h.h_addr, h.h_length);
		}
		if (mfs->mfs_addr.s_addr && !mfs->mfs_ignore) {
			mfs_one = mfs;
			if (self_check(mfs->mfs_host) ||
			    mfs->mfs_addr.s_addr == htonl(INADDR_LOOPBACK) ||
			    mfs->mfs_addr.s_addr == preferred.s_addr) {
				if (trace > 0)
					syslog(LOG_ERR, "find_server: return host %s", 
					    mfs->mfs_host);
				return (mfs);
			}
			entrycount++;
		}
	}
	if (entrycount == 0) 
		return (NULL);
	if (entrycount == 1)
		return (mfs_one);

	/* now try them all */
	addrp = addrs;
	for (mfs = me->map_fs; mfs; mfs = mfs->mfs_next)
		if (!mfs->mfs_ignore)
			*addrp++ = mfs->mfs_addr;
	(*addrp).s_addr = 0;	/* terminate list */
	addr = trymany(addrs, rpc_timeout / 2);
	if (addr.s_addr) {	/* got one */
		for (mfs = me->map_fs; mfs; mfs = mfs->mfs_next)
			if (addr.s_addr == mfs->mfs_addr.s_addr) {
				if (trace > 0)
					syslog(LOG_ERR,
						"find_server: returning %s", 
						mfs->mfs_host);
				return (mfs);
			}
	}
	return (NULL);
}

nfsstat
nfsmount(host, hostaddr, dir, mntpnt, opts, cached, overlay)
	char *host;
	struct in_addr hostaddr;
	char *dir, *mntpnt, *opts;
	int cached, overlay;
{

	char remname[MAXPATHLEN];
	struct mntent m;
	struct nfs_args args;
	int flags;
	struct sockaddr_in sin;
	struct mountres3 mountres3;
	struct fhstatus fhs;
	int s = -1;
	struct timeval timeout;
	CLIENT *cl = NULL;
	enum clnt_stat rpc_stat;
	u_short port;
	nfsstat status;
	struct stat stbuf;
	u_short vers;
	u_short prev_vers = 0;
	u_long prev_prog = 0;
	int cache_time = 60;	/* sec */
	int err;
	cap_t ocap;
	cap_value_t cap_audit_write = CAP_AUDIT_WRITE;
	char *eagopt;

	if (trace > 0) {
		(void) fprintf(stderr,
			"  nfsmount: %s:%s %s %s\n", host, dir, mntpnt, opts);
		syslog(LOG_ERR, "nfsmount: %s:%s '%s' %s", 
		       host, dir, mntpnt, opts);
	}

	/* Make sure mountpoint is safe to mount on */
	if (cap_dostat(lstat, mntpnt, &stbuf) < 0) {
		syslog(LOG_ERR, "Couldn't stat '%s': %m", mntpnt);
		return (NFSERR_NOENT);
	}

	if (pingnfs(hostaddr) != RPC_SUCCESS) {
		syslog(LOG_ERR, "server %s not responding to ping", host);
		return (NFSERR_NOENT);
	}

	(void) sprintf(remname, "%s:%s", host, dir);
	m.mnt_opts = opts;

	/*
	 * If it's cached then we need to get
	 * cachefs to mount it.
	 */
	if (cached) {
		err = mount_generic(remname, MNTTYPE_CACHEFS, opts,
			mntpnt, overlay);
		return (err ? NFSERR_NOENT : NFS_OK);
	}
	/*
	 * vers=2 & nfs2 map to MOUNTVERS (1)
	 * vers=3 & nfs3 map to MOUNTVERS3 (3)
	 * default is NFS3PREFVERS: try V3 and
	 * back-off to V2, if necessary
	 */
	vers = ((vers = nopt(&m, MNTOPT_VERS)) != 0 ? 
		(vers == 2 ? MOUNTVERS : MOUNTVERS3) :
		(ismntopt(&m, MNTTYPE_NFS2) ? MOUNTVERS : 
		(ismntopt(&m, MNTTYPE_NFS3) ? MOUNTVERS3 : 
		 NFS3PREFVERS)));

	memset((void *) &sin, '\0', sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = hostaddr.s_addr;
	timeout.tv_usec = 0;
	timeout.tv_sec = 2;  /* retry interval */
	s = RPC_ANYSOCK;

	if (vers == MOUNTVERS) {
		/*
		 * Try to get SGI private mount proc first so that
		 * statvfs will work.
		 */
		if ((cl = clntudp_create(&sin, 
				prev_prog = (u_long)MOUNTPROG_SGI,
				(u_long)MOUNTVERS_SGI_ORIG, timeout, 
				&s)) == NULL &&
			    (rpc_createerr.cf_stat == RPC_TIMEDOUT ||
 			    (cl = clntudp_create(&sin, 
				prev_prog = (u_long)MOUNTPROG, vers,
				timeout, &s)) == NULL)) {
			syslog(LOG_ERR, "%s: %s", remname,
			clnt_spcreateerror("server not responding"));
			return (NFSERR_NOENT);
		}
	} else {
		/*
		 * Try to get a V3 client handle. sgi_mountd doesn't 
		 * support V3, so stick to mountd. SGI's portmapper 
		 * doesn't check the version # (so we won't know if 
		 * we need to back-off to a V2 client handle until 
		 * after the rpc), but others do, so try the back-off 
		 * to V2 mountd, if necessary.
		 */
		if ((cl = clntudp_create(&sin,
				prev_prog = (u_long)MOUNTPROG, 
				MOUNTVERS3, timeout, &s)) == NULL &&
			    (rpc_createerr.cf_stat == RPC_TIMEDOUT ||
			    vers == MOUNTVERS3 ||
			    (cl = clntudp_create(&sin,
				prev_prog = (u_long)MOUNTPROG,
				vers = MOUNTVERS, timeout, 
				&s)) == NULL)) {
			syslog(LOG_ERR, "%s: %s", remname,
			clnt_spcreateerror("server not responding"));
			return (NFSERR_NOENT);
		}
	}
	cl->cl_auth = authunix_create_default();

	/*
	 * Get fhandle of remote path from server's mountd
	 */
callmount:
	/* 
	 * The xdr functions assume this.
	 */
	memset(&mountres3,'\0',sizeof(struct mountres3));
	memset(&fhs,'\0',sizeof(struct fhstatus));

	prev_vers = vers;
	timeout.tv_usec = 0;
	timeout.tv_sec = rpc_timeout;
	if (vers == MOUNTVERS) {
		rpc_stat = clnt_call(cl, MOUNTPROC_MNT, xdr_path, &dir, 
				xdr_fhstatus, (caddr_t)&fhs, timeout);
	} else {
		rpc_stat = clnt_call(cl, MOUNTPROC_MNT, xdr_path, &dir,
				xdr_mountres3, (caddr_t)&mountres3, timeout);
	}
	errno = 0;
	if (rpc_stat != RPC_SUCCESS) {
		/*
		 * For servers that don't check the program and/or
		 * version # until now...
		 */
		if (prev_prog == MOUNTPROG_SGI ||
		    (prev_vers != MOUNTVERS && 
		     (rpc_stat == RPC_VERSMISMATCH ||
		      rpc_stat == RPC_PROGVERSMISMATCH))) {
			CLIENT *nclient = NULL;

			memset((void *) &sin, '\0', sizeof(sin));
			sin.sin_addr.s_addr = hostaddr.s_addr;
			sin.sin_family = AF_INET;
			timeout.tv_usec = 0;
			timeout.tv_sec = 2;
			s = RPC_ANYSOCK;

			if (prev_prog == MOUNTPROG_SGI) {
				nclient = clntudp_create(&sin, 
						prev_prog = (u_long)MOUNTPROG,
						vers, timeout, &s);
			} else if (vers != MOUNTVERS3) {
				/* 
				 * V3 isn't available. Try V2, sgi-style first
				 */
				if ((nclient = 
				    clntudp_create(&sin,
					    prev_prog = (u_long)MOUNTPROG_SGI,
					    vers = MOUNTVERS,
					    timeout, &s)) == NULL)
					nclient = clntudp_create(&sin,
					    prev_prog = (u_long)MOUNTPROG,
					    vers = MOUNTVERS,
					    timeout, &s);
			}
			if (nclient) {		
				nclient->cl_auth = cl->cl_auth;
				cl->cl_auth = NULL;
				clnt_destroy(cl);
				cl = nclient;
				goto callmount;
			}
		}
		/*
		 * Given the way "clnt_sperror" works, the "%s" immediately
		 * following the "not responding" is correct.
		 */
		syslog(LOG_ERR, "%s server for %s not responding%s", remname, mntpnt,
		    clnt_sperror(cl, ""));
		if (cl->cl_auth)
			AUTH_DESTROY(cl->cl_auth);
		clnt_destroy(cl);
		return (NFSERR_NOENT);
	}
	errno = (vers == MOUNTVERS ? fhs.fhs_status : mountres3.fhs_status);
	if (errno)  {
		if (errno == EACCES) {
			if (verbose)
				syslog(LOG_ERR,
				"can't mount %s: no permission", remname);
			status = NFSERR_ACCES;
		} else {
			syslog(LOG_ERR, "%s: %m", remname);
			status = NFSERR_IO;
		}
		ocap = cap_acquire(_CAP_NUM(cap_audit_write), &cap_audit_write);
		satvwrite(SAT_AE_MOUNT, SAT_FAILURE,
			  "autofs: can't mount %s: %s", remname, 
			  strerror(status));
		cap_surrender(ocap);
		if (cl->cl_auth)
			AUTH_DESTROY(cl->cl_auth);
		clnt_destroy(cl);
		return (status);
	}
	/*
	 * set mount args
	 */
	if (vers == MOUNTVERS) {
		args.fh = (fhandle_t *)&fhs.fhs_fh;
		m.mnt_type = MNTTYPE_NFS;
	} else {
		args.fh =
		    (fhandle_t *)mountres3.mountres3_u.mountinfo.fhandle.fhandle3_val;
		args.fh_len = mountres3.mountres3_u.mountinfo.fhandle.fhandle3_len;
		m.mnt_type = MNTTYPE_NFS3;
	}
	args.hostname = host;
	args.flags = NFSMNT_HOSTNAME;
	args.addr = &sin;

	if (ismntopt(&m, MNTOPT_SOFT) != NULL) {
		args.flags |= NFSMNT_SOFT;
	}
	if (ismntopt(&m, MNTOPT_NOINTR) != NULL) {
		args.flags |= NFSMNT_NOINT;
	}
	if (ismntopt(&m, MNTOPT_NOAC) != NULL) {
		args.flags |= NFSMNT_NOAC;
	}
	if (ismntopt(&m, MNTOPT_PRIVATE)) {
		args.flags |= NFSMNT_PRIVATE;
	}
	if (ismntopt(&m, "bds")) {
		args.flags |= NFSMNT_BDS;
		if (args.bdsauto = nopt(&m, "bdsauto")) {
			args.flags |= NFSMNT_BDSAUTO;
		}
		if (args.bdswindow = nopt(&m, "bdswindow")) {
			args.flags |= NFSMNT_BDSWINDOW;
		}
		if (args.bdsbuflen = nopt(&m, "bdsbuffer")) {
			args.flags |= NFSMNT_BDSBUFLEN;
		}
	}

	if (port = nopt(&m, MNTOPT_PORT)) {
		sin.sin_port = htons(port);
	} else {
		sin.sin_port = htons(NFS_PORT);
	}
/* This next hack is for nfs over tcp */
#ifndef NFSMNT_TCP
#define NFSMNT_TCP   0x400000 /* use tcp if possible */
#endif

	if ( hasmntopt(&m, "proto=tcp") != NULL ||
	     hasmntopt(&m, "proto=TCP") != NULL )
		args.flags |= NFSMNT_TCP;
	if (args.rsize = nopt(&m, MNTOPT_RSIZE)) {
		args.flags |= NFSMNT_RSIZE;
	}
	if (args.wsize = nopt(&m, MNTOPT_WSIZE)) {
		args.flags |= NFSMNT_WSIZE;
	}
	if (args.timeo = nopt(&m, MNTOPT_TIMEO)) {
		args.flags |= NFSMNT_TIMEO;
	}
	if (args.retrans = nopt(&m, MNTOPT_RETRANS)) {
		args.flags |= NFSMNT_RETRANS;
	}
	if (args.acregmax = nopt(&m, MNTOPT_ACTIMEO)) {
		args.flags |= NFSMNT_ACREGMAX;
		args.flags |= NFSMNT_ACDIRMAX;
		args.flags |= NFSMNT_ACDIRMIN;
		args.flags |= NFSMNT_ACREGMIN;
		args.acdirmin = args.acregmin = args.acdirmax
			= args.acregmax;
	} else {
		if (args.acregmin = nopt(&m, MNTOPT_ACREGMIN)) {
			args.flags |= NFSMNT_ACREGMIN;
		}
		if (args.acregmax = nopt(&m, MNTOPT_ACREGMAX)) {
			args.flags |= NFSMNT_ACREGMAX;
		}
		if (args.acdirmin = nopt(&m, MNTOPT_ACDIRMIN)) {
			args.flags |= NFSMNT_ACDIRMIN;
		}
		if (args.acdirmax = nopt(&m, MNTOPT_ACDIRMAX)) {
			args.flags |= NFSMNT_ACDIRMAX;
		}
	}
	/*
	 * Get statvfs from remote mount point to set static values
	 * that aren't supported by the nfs protocol.
	 */
	if (prev_prog == MOUNTPROG_SGI) {
		struct mntrpc_statvfs mntstatvfs;

		memset(&mntstatvfs,'\0',sizeof(struct mntrpc_statvfs));
		rpc_stat = clnt_call(cl, MOUNTPROC_STATVFS, xdr_path, &dir,
					 xdr_statvfs, &mntstatvfs, timeout);
		if (rpc_stat == RPC_SUCCESS) {
			args.flags |= NFSMNT_BASETYPE|NFSMNT_NAMEMAX;
			strncpy(args.base, mntstatvfs.f_basetype,
							sizeof args.base);
			args.namemax = mntstatvfs.f_namemax;
		}
	}

	flags = MS_DATA;

	/* 
	 * only do xattrs when explicitly requested.
	 * get xattrs into eagopt.
	 */
	flags |= (hasmntopt(&m, MNTOPT_DOXATTR) == NULL) ? 0 : MS_DOXATTR;
	flags |= (hasmntopt(&m, MNTOPT_NODEFXATTR) == NULL) ? MS_DEFXATTR : 0;
	flags |= (hasmntopt(&m, MNTOPT_RO) == NULL) ? 0 : MS_RDONLY;
	flags |= (hasmntopt(&m, MNTOPT_NOSUID) == NULL) ? 0 : MS_NOSUID;
	flags |= (hasmntopt(&m, MNTOPT_GRPID) == NULL) ? 0 : MS_GRPID;
	flags |= (hasmntopt(&m, MNTOPT_NODEV) == NULL) ? 0 : MS_NODEV;
	flags |= overlay ? MS_OVERLAY : 0;
	if (eagopt = hasmntopt(&m, MNTOPT_EAG)) {
		char *cp;
		eagopt = strdup(eagopt);
		if (cp = strchr(eagopt, ','))
			*cp = '\0';
		if (cp = strchr(eagopt, ' '))
			*cp = '\0';
		if (cp = strchr(eagopt, '\t'))
			*cp = '\0';
	}

	if (mntpnt[strlen(mntpnt) - 1] != ' ')
		/* direct mount point without offsets */
		flags |= MS_OVERLAY;
#ifdef Sun
	/* decide whether to use remote host's lockd or do local locking */
	if (newhost) {
		if (remote_lock(host, (caddr_t)&fhs.fhs_fh)) {
			syslog(LOG_ERR, "No network locking on %s : "
				"contact admin to install server change",
				host);
			args.flags |= NFSMNT_LLOCK;
		}
	}
#endif

	if (trace > 0) {
		syslog(LOG_ERR, "mount %s %s %s (%s)", 
		       remname, mntpnt, m.mnt_type, opts);
	}
retry:
	if (cap_mount_nfs("", mntpnt, flags, m.mnt_type, &args, sizeof (args), eagopt)) {
		/*
		 * Hack for NFS over TCP
		 */
		if ((args.flags & NFSMNT_TCP) && (errno == ECONNREFUSED)) {
			args.flags &= ~NFSMNT_TCP;
			/*
			 * Fix up options; relies on the fact that tcp and udp
			 * are the same length
			 */
			{
				char *p = strstr(opts, "proto=");
				if (p) {
					while (*p != '=') {
						p++;
					}
					p++;
					memcpy((void *) p, (void *) "udp", 3);
				}
			}
			
			if (trace > 0) {
				syslog(LOG_ERR, 
					"mount of %s on %s, %s: TCP unavailable, trying UDP", 
					remname, mntpnt, m.mnt_type);
			}
			goto retry;
		}
		syslog(LOG_ERR, "mount of %s on %s, %s: %m", remname, 
		       mntpnt, m.mnt_type);
		ocap = cap_acquire(_CAP_NUM(cap_audit_write), &cap_audit_write);
		satvwrite(SAT_AE_MOUNT, SAT_FAILURE,
			  "autofs: failed mount of %s on %s, %s: %s", remname,
			  mntpnt, m.mnt_type, strerror(errno));
		cap_surrender(ocap);
		free (eagopt);
		if (cl->cl_auth)
			AUTH_DESTROY(cl->cl_auth);
		clnt_destroy(cl);
		return (NFSERR_IO);
	}
	free (eagopt);
	ocap = cap_acquire(_CAP_NUM(cap_audit_write), &cap_audit_write);
	satvwrite(SAT_AE_MOUNT, SAT_SUCCESS, "autofs: mounted %s on %s",
		  remname, mntpnt);
	cap_surrender(ocap);
	if (trace > 0) {
		(void) fprintf(stderr, "mount %s OK\n", remname);
	}

	m.mnt_fsname = remname;
	m.mnt_dir = mntpnt;

	if (add_mtab(&m) == ENOENT) {
		if (cl->cl_auth)
			AUTH_DESTROY(cl->cl_auth);
		clnt_destroy(cl);
		return (NFSERR_NOENT);
	}

	if (cl->cl_auth)
		AUTH_DESTROY(cl->cl_auth);
	clnt_destroy(cl);
	return (NFS_OK);
}

enum clnt_stat
pingnfs(hostaddr)
	struct in_addr hostaddr;
{
	struct timeval tottime, timeout;
	struct sockaddr_in server_addr;
	enum clnt_stat clnt_stat = RPC_TIMEDOUT;
	CLIENT *cl = NULL;
	static struct in_addr goodhost, deadhost;
	static time_t goodtime, deadtime;
	int cache_time = ping_cache_time;  /* sec */
	int sock = RPC_ANYSOCK;

	AUTOFS_LOCK(pingnfs_lock);
	if (goodtime > time_now && hostaddr.s_addr == goodhost.s_addr) {
		AUTOFS_UNLOCK(pingnfs_lock);
		return (RPC_SUCCESS);
	}
	if (deadtime > time_now && hostaddr.s_addr == deadhost.s_addr) {
		AUTOFS_UNLOCK(pingnfs_lock);
		return (RPC_TIMEDOUT);
	}
	AUTOFS_UNLOCK(pingnfs_lock);

	if (trace > 0)
		(void) syslog(LOG_ERR, "ping %s ", inet_ntoa(hostaddr));

	server_addr.sin_family = AF_INET;
	server_addr.sin_addr = hostaddr;
	server_addr.sin_port = htons(NFS_PORT);
	timeout.tv_sec = 2;     /* retry interval */
	timeout.tv_usec = 0;
	cl = clntudp_create(&server_addr, NFS_PROGRAM, NFS_VERSION,
			 timeout, &sock);
	if (cl == NULL) {
		if (verbose)
			syslog(LOG_ERR, "pingnfs: %s%s",
				inet_ntoa(hostaddr), clnt_spcreateerror(""));
	} else {
		tottime.tv_sec = 10;
		tottime.tv_usec = 0;
		clnt_stat = clnt_call(cl, NULLPROC,
			xdr_void, 0, xdr_void, 0, tottime);
		clnt_destroy(cl);
	}

	AUTOFS_LOCK(pingnfs_lock);
	if (clnt_stat == RPC_SUCCESS) {
		goodhost = hostaddr;
		goodtime = time_now + cache_time;
	} else {
		deadhost = hostaddr;
		deadtime = time_now + cache_time;
	}
	AUTOFS_UNLOCK(pingnfs_lock);

	if (trace > 0)
		(void) syslog(LOG_ERR, "%s",
			clnt_stat == RPC_SUCCESS ?
			"OK" : "NO RESPONSE");

	return (clnt_stat);
}

#define	RET_ERR		33

int
loopbackmount(fsname, dir, mntopts, overlay)
	char *fsname; 		/* Directory being mounted */
	char *dir;		/* Directory being mounted on */
	char *mntopts;
	int overlay;
{
	struct mntent mnt;
	int fs_ind;
	int flags = MS_FSS;
	char fstype[] = MNTTYPE_LOFS;
	char *eagopt;
	cap_t ocap;
	cap_value_t cap_audit_write = CAP_AUDIT_WRITE;

	if (trace > 0)
		syslog(LOG_ERR, "loopbackmount of %s on %s", fsname, dir);

	mnt.mnt_opts = mntopts;
	if (hasmntopt(&mnt, MNTOPT_RO) != NULL)
		flags |= MS_RDONLY;

	if (overlay)
		flags |= MS_OVERLAY;

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

	if ((fs_ind = sysfs(GETFSIND, MNTTYPE_LOFS)) < 0) {
		syslog(LOG_ERR, "LOFS Mount of %s on %s: %m", fsname, dir);
		ocap = cap_acquire(_CAP_NUM(cap_audit_write), &cap_audit_write);
		satvwrite(SAT_AE_MOUNT, SAT_FAILURE,
			  "lofs: failed mount of %s on %s: %s", fsname,
			  dir, strerror(errno));
		cap_surrender(ocap);
		return (RET_ERR);
	}
	if (trace > 0)
		(void) fprintf(stderr,
			"  loopbackmount: fsname=%s, dir=%s, flags=%d\n",
			fsname, dir, flags);

	if (cap_mount_lofs(fsname, dir, flags, fs_ind, 0, 0,eagopt) < 0) {
		ocap = cap_acquire(_CAP_NUM(cap_audit_write), &cap_audit_write);
		satvwrite(SAT_AE_MOUNT, SAT_FAILURE,
			  "lofs: failed mount of %s on %s: %s", fsname,
			  dir, strerror(errno));
		cap_surrender(ocap);
		free (eagopt);
		syslog(LOG_ERR, "LOFS Mount of %s on %s: %m", fsname, dir);
		return (RET_ERR);
	}

	mnt.mnt_fsname = fsname;
	mnt.mnt_dir  = dir;
	mnt.mnt_type  = fstype;
	mnt.mnt_opts = (flags & MS_RDONLY) ? MNTOPT_RO : MNTOPT_RW;
	mnt.mnt_freq = mnt.mnt_passno = 0;
	(void) add_mtab(&mnt);
	free (eagopt);
	ocap = cap_acquire(_CAP_NUM(cap_audit_write), &cap_audit_write);
	satvwrite(SAT_AE_MOUNT, SAT_SUCCESS, "lofs: %s mount of %s on %s",
		mnt.mnt_type,  fsname,dir);
	cap_surrender(ocap);
	if (trace > 0)
		syslog(LOG_ERR, "%s mount of %s on %s OK", 
			mnt.mnt_type, fsname, dir);
	return (0);
}

/*
 * ismntopt implements a more accurate version of hasmntopt.
 * hasmntopt has no concept of tokens.  hasmntopt(s,"noac")
 * could return pointing to "noac," or "noacl" or "noaction"
 * which causes problems because some joker decided to invent
 * an option called "noacl" which conflict with noac.
 * This routine makes sure that
 *      a) hasmntopt's return pointer is actually the beginning of
 *         a token.
 *         (if there is no previous char, or if it is a ',')
 *      b) hasmntopt's return pointer points to a token that is a
 *         exact match for opt.
           (the next char is a '=' or a ',')
 * If this is the case, return the pointer, otherwise return NULL.
 */
static char *
ismntopt(struct mntent *mnt, char *opt)
{
        char *cp, *retp = hasmntopt(mnt, opt);
        if (retp) {
                /* is retp the beginning of a token?*/
                if (retp == mnt->mnt_opts || *(retp-1)==',') {
                        /* is retp exactly equal to opt? */
                        cp = retp + strlen(opt);
                        if (*cp == '\0' || *cp == ',' || *cp == '=') {
                                return (retp);
                        }  else {
                                char c;
                                while (*cp != '\0' && *cp != ',' && *cp != '=')
                                        cp++;
                                c = *cp;
                                *cp = '\0';
                                (void) fprintf(stderr,
                                        "mount: invalid option %s ignored\n",
                                        retp);
                                *cp = c;
                        }
                }
        }
	
        return (NULL);
}

/*
 * Return the value of a numeric option of the form foo=x, if
 * option is not found or is malformed, return 0.
 */
nopt(mnt, opt)
	struct mntent *mnt;
	char *opt;
{
	int val = 0;
	char *equal;
	char *str;

	if (str = ismntopt(mnt, opt)) {
		if (equal = strchr(str, '=')) {
			val = atoi(&equal[1]);
		} else {
			syslog(LOG_ERR, "Bad numeric option '%s'", str);
		}
	}
	return (val);
}

void
freeex_ent(ex)
	struct exports *ex;
{
	struct groups *groups, *tmpgroups;

	free(ex->ex_name);
	groups = ex->ex_groups;
	while (groups) {
		free(groups->g_name);
		tmpgroups = groups->g_next;
		free((char *)groups);
		groups = tmpgroups;
	}
	free((char *)ex);
}

void
freeex(ex)
	struct exports *ex;
{
	struct exports *tmpex;

	while (ex) {
		tmpex = ex->ex_next;
		freeex_ent(ex);
		ex = tmpex;
	}
}

#define	TIME_MAX 16


#define	STARTPORT 600
#define	ENDPORT (IPPORT_RESERVED - 1)
#define	NPORTS	(ENDPORT - STARTPORT + 1)

int
nfsunmount(mnt)
	struct mntlist *mnt;
{
	struct timeval timeout;
	CLIENT *cl;
	enum clnt_stat rpc_stat;
	char *server, *path, *p;
	struct hostent h;
	struct sockaddr_in sin;
	int s;
	u_short prev_vers=0,vers;
	u_long prev_prog;
	int herr;
	char buf[4096];

	if (trace > 0)
		syslog(LOG_ERR, "umounting '%s'", mnt->mntl_mnt->mnt_dir);

	errno = 0;
	if (cap_umount(mnt) < 0) {
		/*
		 * EINVAL means that it is not a mount point
		 * ENOENT and EINVAL can happen if on a previous
		 * attempt the daemon successfully did its job
		 * but timed out. So this time around it should
		 * return all ok
		 */
		if (errno)
			return (errno);
	}

	/*
	 * The rest of this code is advisory to the server.
	 * If it fails return success anyway.
	 */

	server = mnt->mntl_mnt->mnt_fsname;
	p = strchr(server, ':');
	if (p == NULL)
		return (0);
	*p = '\0';
	path = p + 1;
	if (*path != '/')
		goto done;

	if (! gethostbyname_r(server, &h, buf, sizeof(buf), &herr)) {
		syslog(LOG_WARNING, "%s:%s %s", server, mnt->mntl_mnt->mnt_dir,
		       "server not found");
		goto done;
	}

	memset((void *) &sin, '\0', sizeof(sin));
	sin.sin_family = AF_INET;
	bcopy(h.h_addr, &sin.sin_addr, h.h_length);
	timeout.tv_usec = 0;
	timeout.tv_sec = 3;  /* retry interval */
	s = RPC_ANYSOCK;
	
	if ((cl = clntudp_create(&sin, MOUNTPROG, MOUNTVERS,
				     timeout, &s)) == NULL) {
		syslog(LOG_ERR, "%s: %s", server,
		       clnt_spcreateerror("server not responding"));
		goto done;
	}
	cl->cl_auth = authunix_create_default();
	timeout.tv_usec = 0;
	timeout.tv_sec = 2;
	rpc_stat = clnt_call(cl, MOUNTPROC_UMNT, xdr_path, (caddr_t)&path,
	    xdr_void, (char *)NULL, timeout);
	if (verbose && rpc_stat != RPC_SUCCESS)
		syslog(LOG_ERR, "%s: %s",
			server, clnt_sperror(cl, "unmount"));
	AUTH_DESTROY(cl->cl_auth);
	clnt_destroy(cl);
done:
	*p = ':';
	return (0);
}
