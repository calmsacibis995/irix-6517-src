/*
 *	autod_mount.c
 *
 *	Copyright (c) 1988-1993 Sun Microsystems Inc
 *	All Rights Reserved.
 */

#ident	"@(#)autod_mount.c	1.13	94/03/18 SMI"

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <rpc/rpc.h>
#include <sys/mount.h>
#include <errno.h>
#include <pwd.h>
#include <netdb.h>
#include <netinet/in.h>
#include <locale.h>
#include <unistd.h>
#include <mntent.h>
#include <sys/wait.h>
#include "autofsd.h"
#include <assert.h>
#include <fcntl.h>
#include <sys/fs/export.h>
#include "autofs_prot.h"
#include <sat.h>
#include <sys/mac.h>
#include <sys/capability.h>
#include <signal.h>
#include <sys/fs/autofs.h>

extern int delmntent(const char *, struct mntent *);
extern struct mapent *do_mapent_hosts(char *, char *);
extern dev_t get_lofsid(struct mntent *);
extern int verbose;

static int unmount_mntpnt(struct mntlist *);
static int unmount_hierarchy(struct mntlist *,struct mntlist *);
static int fork_exec(char *, char *, char **, int);
static int parse_special(char *, struct mapent *, char *, char *, char **,
			 char **);
static struct mapent *parse_entry(char *, char *, char *, struct mapline *);

void
prunenohide(struct mapent *me)
{
	struct mapent *n, **nextme;
	char *slash;
	int mfslen;

	mfslen = strlen(me->map_fs->mfs_dir);
	nextme = &me->map_next;
	while (n = *nextme) {
		if (n->map_exflags == EX_NOHIDE) {
			slash = strrchr(n->map_fs->mfs_dir, '/');
			if ((mfslen == 1 && slash == n->map_fs->mfs_dir)
			    || (slash == &n->map_fs->mfs_dir[mfslen]
				&& !strncmp(me->map_fs->mfs_dir,
					    n->map_fs->mfs_dir, 
					    mfslen))) {
				/* prune this guy's kids */
				prunenohide(n);
				/* remove from chain */
				*nextme = n->map_next;
				n->map_next = NULL;
				free_mapent(n);
				continue;
			}
		}
		nextme = &n->map_next;
	}
}

int
cap_dostat(int (*stfunc) (const char *, struct stat *), const char *name,
	   struct stat *stbuf)
{
	int r;
	cap_t ocap;
	const cap_value_t cap_mac_read = CAP_MAC_READ;

	ocap = cap_acquire(_CAP_NUM(cap_mac_read), &cap_mac_read);
	r = (*stfunc)(name, stbuf);
	cap_surrender(ocap);
	return(r);
}

/* 
 * Add the autofs "-remount" entry (direct, no offset) mount point.
 * The options for this in the mtab entry have "ignore,nest" concatenated
 * with the option the nfs mount point had. This is where the nfs mount 
 * options are stored and retrieved when needed to do the actual nfs mount.
 * The nfs mount options are also stored in the options field of the
 * autoinfo structure by passing them to the kernel in the mount system call.
 * The "nest" option is needed so autofs doesn't unmount this mount point.
 */
int autofs_remount(whole_list, mntpnt, mapname, options)
     struct mntlist *whole_list;
     char *mntpnt, *mapname, *options;
{
	struct autofs_args ai;
	struct mntent mnt;
	int error;
	int len;
	const cap_value_t cap_mount_mgt[] = {CAP_MOUNT_MGT, CAP_PRIV_PORT};
	cap_value_t cap_audit_write = CAP_AUDIT_WRITE;
	cap_t ocap;

	ai.path = strdup(mntpnt);
	/* 
	 * Remove any trailing spaces.
	 */
	if (ai.path) {
		while ((len = strlen(ai.path)) && ai.path[len-1] == ' ')
			ai.path[len-1] = '\0';
	}
	ai.opts     = options;
	ai.map      = mapname;
	ai.direct   = 1;
	ai.mount_to = 300;
	ai.rpc_to   = 10;

	ocap = cap_acquire(_CAP_NUM(cap_mount_mgt), cap_mount_mgt);
	error = mount("",mntpnt,MS_DATA,MNTTYPE_AUTOFS,&ai,sizeof(ai));
	cap_surrender(ocap);

	if (error < 0) {
		if (trace > VERBOSE_REMOUNT)
			syslog(LOG_ERR,
			       "Couldn't add autofs remount point:%s,%s",
			       ai.map, ai.path);
		free(ai.path);
		return error;
	}

	mnt.mnt_fsname = mapname;
	mnt.mnt_dir    = mntpnt;
	mnt.mnt_type   = MNTTYPE_AUTOFS;
	/* 
	 * Concatenate the nfs mount options with REMOUNTOPTS.
	 */
	if (options)
		mnt.mnt_opts = strdup(options);
	else
		 mnt.mnt_opts = strdup("");
	mnt.mnt_freq = mnt.mnt_passno = 0;
	
	add_mtab(&mnt);
	
	free(mnt.mnt_opts);
	free(ai.path);

	return(0);
}

/* 
 * This function tries to locate mapent entries in mapme which are submounts of path
 * and tries to attach those mapent entries to the end of the list me after 
 * removing it from the list mapme.
 */
static void 
find_submounts(path, root, me, mapme)
     char *path, *root;	
     struct mapent *me, **mapme;
{
	struct mapent *tme     = *mapme;
	struct mapent *prevme  = NULL;
	struct mapfs *mfs;
	char mntpnt[MAXPATHLEN];
	struct mapent *printme = me;

	while (tme) {
		mfs = tme->map_fs;
		if (!mfs) {
			prevme = tme;
			tme = tme->map_next;
			continue;
		}
		strcpy(mntpnt,root);
		if (tme->map_root)
			strcat(mntpnt, tme->map_root);
		if (tme->map_mntpnt)
			strcat(mntpnt, tme->map_mntpnt);
		if (strlen(mntpnt) > strlen(path) && 
		   strncmp(mntpnt, path, strlen(path)) == 0) {
			if (trace > VERBOSE_REMOUNT)
				fprintf(stderr, "Found mntpnt=%s path=%s\n",
					mntpnt, path);
			if (tme->map_root)
				strcpy(tme->map_root, "");
			else
				tme->map_root = strdup("");
			if (tme->map_mntpnt)
				strcpy(tme->map_mntpnt, &mntpnt[strlen(path)]);
			else
				tme->map_mntpnt = 
					strdup(&mntpnt[strlen(path)]);

			/* 
			 * Whew!.
			 *
			 * It's time now to get tme. Attach it first to me.
			 */
			me->map_next = tme;
			me = me->map_next;

			/* 
			 * Remove it from mapme.
			 */
			if (tme == *mapme)
				*mapme = tme->map_next;
			else
				prevme->map_next = tme->map_next;

			tme = tme->map_next;
			me->map_next = NULL;
		} else {
			prevme = tme;
			tme = tme->map_next;
		}
	}
}

/*
 * 0 on success
 * 1 on failure
 */
int do_mapent_lofsremount(me, key, opts, root)
     struct mapent *me;
     char *key, *root, *opts;
{
	char buf[256];
	extern struct in_addr myaddrs[];
	char *p;

	if (gethostname(buf,256))
		return 1;

	me->map_fs->mfs_host = strdup(buf);
	me->map_fs->mfs_dir  = strdup(root);
	me->map_fs->mfs_addr = myaddrs[0];

	/* 
	 * Remove lofsid=xxx
	 */
	p=strstr(opts, "lofsid=");
	if (p) {
		char *q;

		for (q = p + 7; isalnum(*q); q++)
			;
		
		if (*q == ',')
			q++;
		(void) strcpy(p, q);
	}
		
	/* 
	 * Re-use the lofs space -- ugh.
	 */
	strcpy(me->map_fstype,MNTTYPE_NFS3);
	strcpy(me->map_mounter,MNTTYPE_NFS3);
	return 0;
}

/* 
 * 0 on success
 * 1 on failure
 */
int do_mapent_nfsremount(me, key, opts, root)
     struct mapent *me;
     char *key, *root, *opts;
{
	struct mapfs  *mfs;
	char *p, *q, *r;
	struct hostent *hp, h;
	char buf[LINESZ];
	int err;

	p  = strchr(root,':');
	if (p) {
		*p = '\0';
		
		hp = gethostbyname_r(root, &h, buf, sizeof(buf), &err);
		if (hp == NULL)	{
			*p = ':';
			return 1;
		}
	}

	if (*(p+1) != '/') {
		*p = ':';
		return 1;
	}

	me->map_fs->mfs_host = strdup(root);
	me->map_fs->mfs_dir  = strdup(p+1);
	memcpy((void *) &me->map_fs->mfs_addr,
	       (void *) hp->h_addr, hp->h_length);

	return 0;
}

struct mapent *do_mapent_remount(key, mapopts, root)
     char *key, *mapopts, *root;
{
	struct mapent *me = NULL, *tme = NULL;
	struct mapfs  *mfs;
	char *p, *q, *r;
	struct hostent *hp;
	extern struct mntlist *getmntlist();
	struct mntlist *whole_list = NULL, *mntl = NULL;
	struct mapline ml;
	int err;
	char *stack[STACKSIZ];
	char **stkptr = stack;

	mapopts += strlen(REMOUNTOPTS) + 1;

	me = (struct mapent *)calloc(sizeof(struct mapent), 1);
	if (me == NULL)
		goto done;

	me->map_fs = (struct mapfs *)calloc(sizeof(struct mapfs), 1);
	if (me->map_fs == NULL)
		goto freemem;

	/* 
	 * Remove "rfstype=xxx" from the opts;
	 */
	q = strstr(mapopts, "rfstype=");
	if (q) {
		char c;

		for (r = q + 8; isalnum(*r); r++)
			;
		c = *r;
		*r = '\0';

		me->map_fstype  = strdup(q + 8);
                me->map_mounter = strdup(q + 8);
		
		*r = c;
		if (*r == ',')
			r++;
		
		(void) strcpy(q, r);
		q = mapopts + (strlen(mapopts) - 1);
		if (*q == ',')
			*q = '\0';

		if (strncmp(me->map_fstype, MNTTYPE_NFS, strlen(MNTTYPE_NFS)) 
		    == 0) {
			if (do_mapent_nfsremount(me, key, mapopts, root))
				goto freemem;
		} else if (strcmp(me->map_fstype, MNTTYPE_LOFS) == 0) {
			if (do_mapent_lofsremount(me, key, mapopts, root))
				goto freemem;
		} else 
			goto freemem;
			
	} else {		
		goto freemem;
	}
	
	me->map_mntopts = strdup(mapopts);
	me->map_root    = strdup("");
	me->map_mntpnt  = strdup("");

	/* 
	 * This whole piece of code here is necessary to identify any 
	 * submounts of the remount point.
	 */
	mntl = whole_list = getmntlist();

	for (mntl = whole_list; mntl; mntl = mntl->mntl_next) {
		int len;
		
		if (!mntl->mntl_mnt || !mntl->mntl_mnt->mnt_type || 
		    !mntl->mntl_mnt->mnt_fsname || !mntl->mntl_mnt->mnt_dir)
			continue;
		if (strcmp(mntl->mntl_mnt->mnt_type, MNTTYPE_AUTOFS))
			continue;
		/* 
		 * We will go into an infinite recursion if we don't check 
		 * this.
		 * This is because parse_entry below will call 
		 * do_mapent_remount again.
		 */
		if (strncmp(mntl->mntl_mnt->mnt_fsname, "-remount", 
			    strlen("-remount")) == 0)
			continue;

		len = strlen(mntl->mntl_mnt->mnt_dir);
		if (len > strlen(key))
			continue;

		if (strncmp(mntl->mntl_mnt->mnt_dir, key, len))
			continue;

		if (key[len] != '/' && key[len] != '\0')
			continue;

		err = getmapent(mntl->mntl_mnt->mnt_dir,
				mntl->mntl_mnt->mnt_fsname, &ml, 
				stack, &stkptr);
		if (err == 0) {
			tme = parse_entry(mntl->mntl_mnt->mnt_dir,
					  mntl->mntl_mnt->mnt_fsname,
					  mapopts, &ml);
			if (tme != NULL) {
				/* 
				 * Now we have to find submounts of key and
				 * free up the rest of the mapent entries 
				 * returned by parse_entry.
				 */
				find_submounts(key, mntl->mntl_mnt->mnt_dir, 
					       me, &tme);
				free_mapent(tme);
			}
		}
	}

	tme = me;
	me  = NULL;
freemem:
	if (me)
		free_mapent(me);
	if (whole_list)
		freemntlist(whole_list);
done:
	return tme;
}
	
do_mount1(mapname, key, mapopts, path, cred)
	char *mapname;
	char *key;
	char *mapopts;
	char *path;
	struct authunix_parms *cred;
{
	struct mapline ml;
	struct mapent *me, *mapents = NULL, *mapnext = NULL;
	char mntpnt[MAXPATHLEN];
	char spec_mntpnt[MAXPATHLEN];
	int err, imadeit, depth;
	struct stat stbuf;
			/*
			 * data to be remembered by fs specific
			 * mounts. eg prevhost in case of nfs
			 */
	int overlay = 0;
	int isdirect = 0;
	int mount_ok = 0;
	int len;
	char *stack[STACKSIZ];
	char **stkptr = stack;

	/* initialize the stack of open files for this thread */
	stack_op(INIT, NULL, stack, &stkptr);

	err = getmapent(key, mapname, &ml, stack, &stkptr);
	if (err == 0)
		mapents = parse_entry(key, mapname, mapopts, &ml);
	/*
	 * Now we indulge in a bit of hanky-panky.
	 * If the entry isn't found in the map and the
	 * name begins with an "=" then we assume that
	 * the name is an undocumented control message
	 * for the daemon.  This is accessible only
	 * to superusers.
	 */
	if (mapents == NULL) {
		if (*key == '=' && cred->aup_uid == 0) {
			if (isdigit(*(key+1))) {
				/*
				 * If next character is a digit
				 * then set the trace level.
				 */
				trace = atoi(key+1);
				(void) fprintf(stderr,
					"autofsd: trace level = %d\n",
					trace);
			} else if (*(key+1) == 'v') {
				/*
				 * If it's a "v" then
				 * toggle verbose mode.
				 */
				verbose = !verbose;
				(void) fprintf(stderr,
					"autofsd: verbose %s\n",
						verbose ? "on" : "off");
			}
		}

		err = ENOENT;
		goto done;
	}

	if (trace > 0) {
		struct mapfs  *mfs;

		(void) syslog(LOG_ERR, "  mapname = %s, key = %s",
			mapname, key);
		for (me = mapents; me; me = me->map_next) {
			(void) syslog(LOG_ERR, "     (%s,%s) %s \t-%s\t",
				me->map_fstype,
				me->map_mounter,
				*me->map_mntpnt ? me->map_mntpnt : "/",
				me->map_mntopts);
			for (mfs = me->map_fs; mfs; mfs = mfs->mfs_next)
				(void) syslog(LOG_ERR, "%s:%s ",
					mfs->mfs_host ? mfs->mfs_host: "",
					mfs->mfs_dir);
		}
		(void) fprintf(stderr, "  mapname = %s, key = %s\n",
			mapname, key);
		for (me = mapents; me; me = me->map_next) {
			(void) fprintf(stderr, "     (%s,%s) %s \t-%s\t",
				me->map_fstype,
				me->map_mounter,
				*me->map_mntpnt ? me->map_mntpnt : "/",
				me->map_mntopts);
			for (mfs = me->map_fs; mfs; mfs = mfs->mfs_next)
				(void) fprintf(stderr, "%s:%s ",
					mfs->mfs_host ? mfs->mfs_host: "",
					mfs->mfs_dir);
			(void) fprintf(stderr, "\n");
		}
	}

	/*
	 * Each mapent in the list describes a mount to be done.
	 * Normally there's just a single entry, though in the
	 * case of /hosts mounts there may be many entries, that
	 * must be mounted as a hierarchy.  For each mount the
	 * autofsd must make sure the required mountpoint
	 * exists and invoke the appropriate mount command for
	 * the fstype.
	 */
	for (me = mapents; me; me = mapnext) {
		(void) sprintf(mntpnt, "%s%s%s", path, mapents->map_root,
				me->map_mntpnt);
		mapnext = me->map_next;

		/*
		 * remove trailing /'s from mountpoint to avoid problems
		 * stating a directory with two or more trailing slashes.
		 * This will let us mount directories from machines which
		 * export with two or more slashes (apollo for instance).
		 */
		len = strlen(mntpnt) - 1;
		while (mntpnt[len] == '/')
			mntpnt[len--] = '\0';

		imadeit = 0;
		overlay = strcmp(mntpnt, key) == 0;
		if (overlay || isdirect) {
			/*
			 * direct mount and no offset
			 * the isdirect flag takes care of hierarchies
			 * mounted under a direct mount
			 */
			isdirect = 1;
			(void) strcpy(spec_mntpnt, mntpnt);
		} else
			(void) sprintf(spec_mntpnt, "%s%s", mntpnt, " ");

	no_dir_try_again:
		if (cap_dostat(lstat, spec_mntpnt, &stbuf) != 0) {
			depth = 0;
			if (autofs_mkdir_r(mntpnt, &depth) == 0) {
				imadeit = 1;
				if (cap_dostat(stat, spec_mntpnt, &stbuf) < 0) {
					syslog(LOG_ERR,
					"Couldn't stat created mntpoint %s: %m",
						spec_mntpnt);
					continue;
				}
			} else {
				if (verbose)
					syslog(LOG_ERR,
					"Couldn't create mntpoint %s: %m",
						spec_mntpnt);
				continue;
			}
		} else if ((stbuf.st_mode & S_IFMT) == S_IFLNK) {
			if (verbose)
				syslog(LOG_ERR, 
				"%s symbolic link: not a valid auto-mountpoint",
				spec_mntpnt);
			continue;
		}
		if (trace > 0)
			syslog(LOG_ERR, "mounting '%s' on '%s'\n", 
				me->map_fs->mfs_dir, spec_mntpnt);
		if (strcmp(me->map_fstype, MNTTYPE_NFS3) == 0 ||
		    strcmp(me->map_fstype, MNTTYPE_NFS) == 0)
			err = mount_nfs(me, spec_mntpnt, overlay);
		else
			err = mount_generic(me->map_fs->mfs_dir,
					    me->map_fstype, me->map_mntopts,
					    spec_mntpnt, overlay);

		if (!err) {
			mount_ok++;
			prunenohide(me);
			mapnext = me->map_next;
		} else {
			struct stat stbuf1;
			/*
			 * If the first mount of a no-offset
			 * direct mount fails, further sub-mounts
			 * will block since they aren't using
			 * special paths.  Avoid hang and quit here.
			 */
			if (isdirect) {
				err = ENOENT;
				goto done;
			}

			/* 
			 * This code is necessary because in the case of the
			 * multi-threaded autofsd, the kernel can go ahead and
			 * remove the mount-point (because of the user hitting
			 * ^C) just when autofsd is about to do the mount 
			 * causing bogus mount failures. 
			 *
			 * Stat to see if the file is still there. If it is
			 * there verify if the inode has changed. If the inode
			 * has changed this means someone else is racing with
			 * us. This works only because autofs has a 
			 * sequentially increasing inode number (an_nodeid) for
			 * every new autonode and does not reuse the inode 
			 * number.
			 */
			if (cap_dostat(lstat, spec_mntpnt, &stbuf1) != 0 && 
			    errno == ENOENT) {
				struct mapfs *mfs;

				/* 
				 * Clear state set by previous mount_nfs.
				 */
				for (mfs = me->map_fs; mfs; 
				     mfs = mfs->mfs_next) 
					mfs->mfs_ignore = 0;
				if (trace > 0)
					fprintf(stderr,
						"Making directory %s and "
						"trying mount one more "
						"time.\n",
						spec_mntpnt);
				goto no_dir_try_again;
			}

			if (errno == 0 && stbuf1.st_dev == stbuf.st_dev && 
			    stbuf1.st_ino != stbuf.st_ino ) {
				/* 
				 * Another thread is racing with us.
				 */
				err = ENOENT;
                                goto done;
                        }

			if (imadeit)
				if (rmdir_r(spec_mntpnt, depth) < 0)
					(void) fprintf(stderr,
						"  Rmdir: error=%d, mnt='%s'\n",
						errno, spec_mntpnt);
		}
	}
	err = mount_ok ? 0 : ENOENT;
done:
	if (mapents)
		free_mapent(mapents);

	return (err);
}

/*
 * Check the option string for an "fstype"
 * option.  If found, return the fstype
 * and the option string with the fstype
 * option removed, e.g.
 *
 *  input:  "fstype=cachefs,ro,nosuid"
 *  opts:   "ro,nosuid"
 *  fstype: "cachefs"
 */
void
get_opts(input, opts, fstype)
	char *input;
	char *opts; 	/* output */
	char *fstype;   /* output */
{
	char *p, *pb , *lasts;
	char buf[B_SIZE];

	*opts = '\0';
	(void) strcpy(buf, input);
	pb = buf;
	while (p = strtok_r(pb, ",", &lasts)) {
		pb = NULL;
		if (strncmp(p, "fstype=", 7) == 0) {
			(void) strcpy(fstype, p + 7);
		} else {
			if (*opts)
				(void) strcat(opts, ",");
			(void) strcat(opts, p);
		}
	}
}

static struct mapent *
parse_entry(key, mapname, mapopts, ml)
	char *key;
	char *mapname;
	char *mapopts;
	struct mapline *ml;
{
	struct mapent *me, *ms, *mp;
	int err, implied;
	char *lp = ml->linebuf;
	char *lq = ml->lineqbuf;
	char w[B_SIZE], wq[B_SIZE];
	char entryopts[B_SIZE];
	char fstype[32], mounter[32];
	char *p;

	/*
	 * Assure the key is only one token long.
	 * This prevents options from sneaking in through the
	 * command line or corruption of /etc/mtab.
	 */
	for (p = key; *p != '\0'; p++) {
		if (isspace(*p)) {
			syslog(LOG_ERR, "bad key in map %s: %s", mapname, key);
			return ((struct mapent *) NULL);
		}
	}

	if (strcmp(lp, "-hosts") == 0)
		return (do_mapent_hosts(mapopts, key));

	/* 
	 * The -remount map type is something we ourselves created. It is
	 * a special map type and it needs to be handled differently.
	 */
	if (strncmp(lp, "-remount", strlen("-remount")) == 0) {
		return do_mapent_remount(key, mapopts, 
					 lp+strlen("-remount"));
	}

	if (macro_expand(key, lp, lq, sizeof (ml->linebuf)) == -1) {
		syslog(LOG_ERR, "overflow key in map %s: %s", mapname, key);
		return ((struct mapent *) NULL);
	}
	getword(w, wq, &lp, &lq, ' ', sizeof(w));
	(void) strcpy(fstype, MNTTYPE_NFS3);

	get_opts(mapopts, entryopts, fstype);

	if (w[0] == '-') {	/* default mount options for entry */
		/*
		 * Since options may not apply to previous fstype,
		 * we want to set it to the default value again
		 * before we read the default mount options for entry.
		 */
		(void) strcpy(fstype, MNTTYPE_NFS3);

		(void) get_opts(w+1, entryopts, fstype);
		getword(w, wq, &lp, &lq, ' ', sizeof(w));
	}

	implied = *w != '/';

	ms = NULL;
	while (*w == '/' || implied) {
		mp = me;
		me = (struct mapent *) malloc(sizeof (*me));
		if (me == NULL)
			goto alloc_failed;
		memset((void *) me, '\0', sizeof (*me));
		if (ms == NULL)
			ms = me;
		else
			mp->map_next = me;

		if (strcmp(w, "/") == 0 || implied)
			me->map_mntpnt = strdup("");
		else
			me->map_mntpnt = strdup(w);
		if (me->map_mntpnt == NULL)
			goto alloc_failed;

		if (implied)
			implied = 0;
		else
			getword(w, wq, &lp, &lq, ' ', sizeof(w));

		if (w[0] == '-') {	/* mount options */
			/*
			 * Since options may not apply to previous fstype,
			 * we want to set it to the default value again
			 * before we read the default mount options for entry.
			 */
			(void) strcpy(fstype, MNTTYPE_NFS3);

			get_opts(w+1, entryopts, fstype);
			getword(w, wq, &lp, &lq, ' ', sizeof(w));
		}

		if (w[0] == '\0') {
			syslog(LOG_ERR, "bad key in map %s: %s", mapname, key);
			goto bad_entry;
		}

		(void) strcpy(mounter, fstype);

		me->map_mntopts = strdup(entryopts);
		if (me->map_mntopts == NULL)
			goto alloc_failed;

		/*
		 * The following ugly chunk of code crept in as
		 * a result of cachefs.  If it's a cachefs mount
		 * of an nfs filesystem, then it's important to
		 * parse the nfs fsname field.  Otherwise, just
		 * hand the fsname field to the fs-specific mount
		 * program with no special parsing.
		 */
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
				} 
				else if ((strncmp(p, MNTTYPE_NFS, len-1) == 0 
				    && (p[len-1] == '\0' || p[len-1] == ',')) 
				    || (strncmp(p, MNTTYPE_NFS2, len) == 0 
				    && (p[len] == '\0' || p[len] == ','))) {
					strcpy(fstype, MNTTYPE_NFS);
				}
			}
		}

		me->map_fstype = strdup(fstype);
		if (me->map_fstype == NULL)
			goto alloc_failed;
		me->map_mounter = strdup(mounter);
		if (me->map_mounter == NULL)
			goto alloc_failed;

		if (strcmp(fstype, MNTTYPE_NFS3) == 0 ||
		    strcmp(fstype, MNTTYPE_NFS) == 0)
			err = parse_nfs(mapname, me, w, wq, &lp, &lq);
		else
			err = parse_special(mapname, me, w, wq, &lp, &lq);

		if (err < 0)
			goto alloc_failed;
		if (err > 0)
			goto bad_entry;
		me->map_next = NULL;
	}

	if (*key == '/') {
		*w = '\0';	/* a hack for direct maps */
	} else {
		(void) strcpy(w, "/");
		(void) strcat(w, key);
	}
	ms->map_root = strdup(w);
	if (ms->map_root == NULL)
		goto alloc_failed;

	return (ms);

alloc_failed:
	syslog(LOG_ERR, "Memory allocation failed: %m");
bad_entry:
	free_mapent(ms);
	return ((struct mapent *) NULL);
}


void
free_mapent(me)
	struct mapent *me;
{
	struct mapfs *mfs;
	struct mapent *m;

	while (me) {
		while (me->map_fs) {
			mfs = me->map_fs;
			if (mfs->mfs_host)
				free(mfs->mfs_host);
			if (mfs->mfs_dir)
				free(mfs->mfs_dir);
			me->map_fs = mfs->mfs_next;
			free((char *) mfs);
		}

		if (me->map_root)
			free(me->map_root);
		if (me->map_mntpnt)
			free(me->map_mntpnt);
		if (me->map_mntopts)
			free(me->map_mntopts);
		if (me->map_fstype)
			free(me->map_fstype);
		if (me->map_mounter)
			free(me->map_mounter);

		m = me;
		me = me->map_next;
		free((char *) m);
	}
}

static int
parse_special(mapname, me, w, wq, lp, lq)
	struct mapent *me;
	char *mapname, *w, *wq, **lp, **lq;
{
	char devname[MAXPATHLEN + 1], qbuf[MAXPATHLEN + 1];
	char *wlp, *wlq;
	struct mapfs *mfs;

	wlp = w;
	wlq = wq;
	getword(devname, qbuf, &wlp, &wlq, ' ', sizeof(devname));
	mfs = (struct mapfs *) malloc(sizeof (struct mapfs));
	if (mfs == NULL)
		return (-1);
	memset((void *) mfs, '\0', sizeof (*mfs));

	/*
	 * A device name that begins with a slash could
	 * be confused with a mountpoint path, hence use
	 * a colon to escape a device string that begins
	 * with a slash, e.g.
	 *
	 *	foo  -ro  /bar  foo:/bar
	 * and
	 *	foo  -ro  /dev/sr0
	 *
	 * would confuse the parser.  The second instance
	 * must use a colon:
	 *
	 *	foo  -ro  :/dev/sr0
	 */
	mfs->mfs_dir = strdup(&devname[devname[0] == ':']);
	if (mfs->mfs_dir == NULL)
		return (-1);
	me->map_fs = mfs;
	getword(w, wq, lp, lq, ' ', B_SIZE);
	return (0);
}

#define	ARGV_MAX	16
#define	VFS_PATH	"/sbin"

int
mount_generic(fsname, fstype, opts, mntpnt, overlay)
	char *fsname, *fstype, *opts, *mntpnt;
	int overlay;
{
	struct mntent m;
	struct stat stbuf;
	int i;
	char *newargv[ARGV_MAX];

	if (trace > 0) {
		(void) fprintf(stderr, "  mount: %s %s %s %s\n",
			fsname, mntpnt, fstype, opts);
	}

	if (cap_dostat(stat, mntpnt, &stbuf) < 0) {
		syslog(LOG_ERR, "Couldn't stat %s: %m", mntpnt);
		return (ENOENT);
	}

	i = 2;

	/*
	 * tell the mount command this request is from autofsd and not to
	 * use realpath
	 */
	newargv[i++] = "-A";

	if (opts && *opts) {
		m.mnt_opts = opts;
		if (hasmntopt(&m, MNTOPT_RO) != NULL)
			newargv[i++] = "-r";
		newargv[i++] = "-o";
		newargv[i++] = opts;
	}

	/*
	 * Fill in the mount file system type.
	 */
	newargv[i++] = "-t";
	newargv[i++] = fstype;

	newargv[i++] = fsname;
	newargv[i++] = mntpnt;
	newargv[i] = NULL;
	return (fork_exec(fstype, "mount", newargv, verbose));
}

static int
fork_exec(fstype, cmd, newargv, console)
	char *fstype;
	char *cmd;
	char **newargv;
	int console;
{
	char path[MAXPATHLEN];
	int pid;
	int i;
	int stat_loc;
	int fd = 0;
	struct stat stbuf;
	int res;
	struct sigaction nact, oact;
	int sigfail;

	/* build the full path name of the fstype dependent command */
	(void) sprintf(path, "%s/%s", VFS_PATH, cmd);

	if (cap_dostat(stat, path, &stbuf) != 0) {
		res = errno;
		if (trace > 0)
			(void)
			fprintf(stderr, "  fork_exec: stat %s returned %d\n",
				path, res);
		return (res);
	}

	if (trace > 0) {
		(void) fprintf(stderr, "  fork_exec: %s ", path);
		for (i = 2; newargv[i]; i++)
			(void) fprintf(stderr, "%s ", newargv[i]);
		(void) fprintf(stderr, "\n");
	}

	/* Make sure SNOWAIT is not set so our waitpid() works. */
	nact.sa_flags = 0;
	nact.sa_handler = SIG_DFL;
	sigemptyset(&nact.sa_mask);
	if ((sigfail = sigaction(SIGCLD, &nact, &oact)) == -1)
		syslog(LOG_ERR, "sigaction failed: %m");

	newargv[1] = cmd;
	switch (pid = fork()) {
	case -1:
		syslog(LOG_ERR, "Cannot fork: %m");
		if (!sigfail)
			sigaction(SIGCLD, &oact, NULL);
		return (errno);
	case 0:
		/*
		 * Child
		 */
		(void) setsid();
		fd = open(console ? "/dev/console" : "/dev/null", O_WRONLY);
		if (fd != -1) {
			(void) dup2(fd, 1);
			(void) dup2(fd, 2);
			if (fd > 2)
				(void) close(fd);
		}

		(void) execv(path, &newargv[1]);
		if (errno == EACCES)
			syslog(LOG_ERR, "exec %s: %m", path);

		exit(errno);
	default:
		/*
		 * Parent
		 */
		(void) waitpid(pid, &stat_loc, 0);

		if (!sigfail)
			sigaction(SIGCLD, &oact, NULL);

		if (WIFEXITED(stat_loc)) {
			if (trace > 0) {
				(void) fprintf(stderr,
				    "  fork_exec: returns exit status %d\n",
				    WEXITSTATUS(stat_loc));
			}

			return (WEXITSTATUS(stat_loc));
		} else
		if (WIFSIGNALED(stat_loc)) {
			if (trace > 0) {
				(void) fprintf(stderr,
				    "  fork_exec: returns signal status %d\n",
				    WTERMSIG(stat_loc));
			}
		} else {
			if (trace > 0)
				(void) fprintf(stderr,
				    "  fork_exec: returns unknown status\n");
		}

		return (1);
	}
}


do_unmount1(ul)
	umntrequest *ul;
{

	struct mntlist *mntl_head, *mntl;
	extern struct mntlist *getmntlist();
	struct mntlist *match;
	struct umntrequest *ur;
	struct hostent h;
	int res = 0;
	int herr;
	char buf[4096];
	struct mntlist *whole_list;

	whole_list = mntl_head = getmntlist();
	if (mntl_head == NULL)
		return (1);

	for (ur = ul; ur; ur = ur->next) {
		/*
		 * Find the last entry with a matching
		 * device id.
		 * Loopback mounts have the same device
		 * id as the real filesystem.  Loopback
		 * mount will always come after the real
		 * mount in the mtab.
		 *
		 * XXX In the case of multiple lofs mounts
		 * we can screw up here.  If all lofs mounts
		 * of a filesystem belong to a single
		 * hierarchy then there's no problem.  If they
		 * belong to different hierarchies, then the
		 * wrong set of lofs mounts may get unmounted.
		 * We need to find a way to fix this...
		 */
		match = NULL;
		for (mntl = mntl_head; mntl; mntl = mntl->mntl_next) {
			/*
			 * Look for dev match.
			 * If LOFS, check rdevid as well.
			 * The NFS checks are total paranoia, to
			 * protect us from ever trying to unmount 
			 * the real '/'
			 */

			/* 
			 * Also look for autofs mount points. These are 
			 * mount points which can happen with the -remount
			 * map type only at present.
			 */
			if (ur->devid == mntl->mntl_dev &&
	  		    ! (mntl->mntl_flags & MNTL_UNMOUNT) &&
			    ((strcmp(mntl->mntl_mnt->mnt_type,
				    MNTTYPE_NFS3) == 0 ||
			    strcmp(mntl->mntl_mnt->mnt_type,
				    MNTTYPE_NFS) == 0 ||
			    strcmp(mntl->mntl_mnt->mnt_type,
				    MNTTYPE_AUTOFS) == 0 ||
			    strcmp(mntl->mntl_mnt->mnt_type,
				    MNTTYPE_CACHEFS) == 0) ||
			    (strcmp(mntl->mntl_mnt->mnt_type, 
				    MNTTYPE_LOFS) == 0 &&
			    get_lofsid(mntl->mntl_mnt) == ur->rdevid))) {
				match = mntl;
				if (trace > 0)
					fprintf(stderr, "do_unmount 0x%x %s\n", 
						ur->devid, 
						mntl->mntl_mnt->mnt_type);
			}
		}

		if (match == NULL) {
			if (trace > 0) {
				(void) fprintf(stderr,
					"  do_unmount: %x = ? "
					"<----- mntpnt not found\n",
					ur->devid);
			}
			continue;
		}

		/*
		 * Special case for NFS mounts.
		 * Don't want to attempt unmounts from
		 * a dead server.  If any member of a
		 * hierarchy belongs to a dead server
		 * give up (try later).
		 */
		if (strcmp(match->mntl_mnt->mnt_type, MNTTYPE_NFS3) == 0 ||
		    strcmp(match->mntl_mnt->mnt_type, MNTTYPE_NFS) == 0) {
			char *server, *p;

			server = match->mntl_mnt->mnt_fsname;
			p = strchr(server, ':');
			if (p) {
				*p = '\0';
				if (! gethostbyname_r(server, &h, buf, sizeof(buf), &herr)) {
					if (verbose)
						syslog(LOG_ERR, 
						    "do_unmount no address for %s",
						    server);
					goto done;
				}
				if (pingnfs(*(struct in_addr *)h.h_addr) != 
					    RPC_SUCCESS) {
					res = 1;
					if (verbose)
						syslog(LOG_ERR, 
					    	    "do_unmount cannot contact %s",
						    server);
					goto done;
				}
				*p = ':';
			}
		}

		match->mntl_flags |= MNTL_UNMOUNT;
		if (!ur->isdirect)
			(void) strcat(
				match->mntl_mnt->mnt_dir, " ");
	}

	res = unmount_hierarchy(whole_list,mntl_head);

done:
	freemntlist(mntl_head);
	return (res);
}


/*
 * Mount every filesystem in the mount list
 * that has its unmount flag set.
 */
static void
remount_hierarchy(struct mntlist *whole_list,struct mntlist *mntl_head)
{
	struct mntlist *mntl, *m, *paml = 0;
	char *fsname, *mntpnt, *fstype, *opts;
	char spec_mntpnt[MAXPATHLEN];
	char *server;
	char *p, *q;
	struct hostent h;
	int overlay;
	int herr;
	char buf[4096];
	int isdirect;
	int paml_len;
	int len;
	char buf1[B_SIZE];

	for (mntl = mntl_head; mntl; mntl = mntl->mntl_next) {
		if (!(mntl->mntl_flags & MNTL_UNMOUNT))
			continue;

		fsname = mntl->mntl_mnt->mnt_fsname;
		mntpnt = mntl->mntl_mnt->mnt_dir;
		fstype = mntl->mntl_mnt->mnt_type;
		opts   = mntl->mntl_mnt->mnt_opts;

		/*
		 * If the mountpoint path matches a previous
		 * mountpoint then it's an overlay mount.
		 * Make sure the overlay flag is set for the
		 * remount otherwise the mount will fail
		 * with EBUSY.
		 * Overlay mounts and submounts of overlay mounts
		 * do not use the space protocol.  All others do.
		 */
		overlay = 0; /* defaults for the rare case we do not find aml */
		isdirect = 0;
		if (paml && strncmp(paml->mntl_mnt->mnt_dir, mntpnt, paml_len)
		    && mntpnt[paml_len] == '/') {
			isdirect = paml->mntl_flags & MNTL_DIRECT;
			overlay = 0;
		} else for (m = whole_list; m != mntl; m = m->mntl_next) {
			if (strcmp(m->mntl_mnt->mnt_type, "autofs"))
				continue;
			len = strlen(m->mntl_mnt->mnt_dir);
			if (strncmp(m->mntl_mnt->mnt_dir, mntpnt, len))
				continue;
			if (mntpnt[len] != '\0' && mntpnt[len] != '/')
				continue;

			/*
			 * Found our autofs mountpoint.
			 * Now look for the first submount.
			 */
			paml = m; /* set previous autofs mntlist */
			paml_len = len;
			for (m = m->mntl_next; m != mntl; m = m->mntl_next) {
				if (strncmp(m->mntl_mnt->mnt_dir, mntpnt, len))
					continue;
				if (m->mntl_mnt->mnt_dir[len] == '\0' ||
				    m->mntl_mnt->mnt_dir[len] == '/')
					break;
			}

			/*
			 * Found the first autofs mounted filesystem.
			 * If it does not exactly match the autofs mountpoint,
			 * it is not a direct no-offset mount and we must
			 * use the space protocol.  If it matches exactly
			 * and this is mntl, mark as overlay.
			 */
			if (strcmp(paml->mntl_mnt->mnt_dir,
			    m->mntl_mnt->mnt_dir)) {
				overlay = 0;
				isdirect = 0;
				paml->mntl_flags &= ~MNTL_DIRECT;
			} else {
				overlay = (m == mntl);
				isdirect = 1;
				paml->mntl_flags |= MNTL_DIRECT;
			}
			/* 
			 * Need to break out of the loop here if m == mntl
			 * as the inner loop could have set m to that value.
			 * In this scenario, we will never break out of the
			 * loop otherwise.
			 */
			if (m == mntl)
				break;
		}
		/*
		 * Remove "dev=xxx" from the opts
		 */
		p = strstr(opts, "dev=");
		if (p) {
			for (q = p + 4; isalnum(*q); q++)
				;
			if (*q == ',')
				q++;
			(void) strcpy(p, q);
			p = opts + (strlen(opts) - 1);
			if (*p == ',')
				*p = '\0';
		}

		/*
		 * Use the space protocol unless this is a direct mount
		 * with no offsets (an overlay on the autofs mount) or
		 * a submount of a direct no-offset mount.
		 */
		if (isdirect) {
			(void) strcpy(spec_mntpnt, mntpnt);
		} else
			(void) sprintf(spec_mntpnt, "%s%s", mntpnt, " ");

		/* 
		 * This check should be before the nfsmount below because it 
                 * assumes that the autofs_remount that will happen if the 
                 * nfsmount fails is gonna' be done. 
		 *
		 * What this check does is to verify that nothing else is 
		 * going to be mounted on top of this autofs mount point.
		 * If there exists something else coming on top of us, assume
		 * it is the mount which had originally failed in the 
		 * remount.
		 */
		if (strcmp(fstype, MNTTYPE_AUTOFS) == 0 && fsname && 
		    strncmp(fsname, "-remount", strlen("-remount")) == 0) {
			int anothermount=0;
			/* 
			 * If there is another mount going to happen on top 
			 * of us. We just should ignore this autofs mount.
			 */
			for (m=mntl->mntl_next;m;m=m->mntl_next) {
				if (m->mntl_mnt && m->mntl_mnt->mnt_dir && 
				    strcmp(spec_mntpnt,m->mntl_mnt->mnt_dir) 
				    == 0) {
					anothermount=1;
					break;
				}
			}
			if (anothermount) {
				if (trace > VERBOSE_REMOUNT)
					fprintf(stderr,
						"Ignoring autofs remount.\n");
			} else {
				autofs_remount(whole_list,
					       spec_mntpnt, fsname, opts);
			}
			continue;
		}

		if (strcmp(fstype, MNTTYPE_NFS3) == 0 ||
		    strcmp(fstype, MNTTYPE_NFS) == 0) {
			server = fsname;
			p  = strchr(server, ':');
			*p = '\0';
			if (! gethostbyname_r(server, &h, buf, sizeof(buf), &herr)) {
				syslog(LOG_ERR, "No address for %s", server);
				*p = ':';
				continue;
			}
			if (nfsmount(server, *(struct in_addr *)h.h_addr, 
				p + 1, spec_mntpnt, opts, 0, overlay)) {

				syslog(LOG_ERR, 
				       "Could not remount '%s' from %s:%s",
				       spec_mntpnt,server,p + 1);
				/* 
				 * The nfs mount failed, so attempt to add
				 * the autofs direct mount point.
				 */
				snprintf(buf, 4096, "-remount%s:%s", 
					 server, p+1);
				snprintf(buf1, B_SIZE, "%s,%s,rfstype=%s", 
					 REMOUNTOPTS, opts, MNTTYPE_NFS);
				autofs_remount(whole_list, spec_mntpnt, buf,
					       buf1);
				/* 
				 * UnMark any submounts of this directory.
				 */
				for (m=mntl; m; m=m->mntl_next)	{
					len = strlen(mntpnt);
					if (strncmp(mntpnt, 
						    m->mntl_mnt->mnt_dir, len)
					    == 0) {
						if (m->mntl_mnt->mnt_dir[len] 
						    == '/' || 
						    m->mntl_mnt->mnt_dir[len] 
						    == '\0')
							m->mntl_flags &= 
								~MNTL_UNMOUNT;
					}
				}
			}
			*p = ':';
			continue;
		}

		/*
		 * For a cachefs mount, we need to determine the real
		 * "fsname", since it is maintained in the underlying
		 * NFS mount entry.  Look for a match of our current
		 * "mnt_fsname" against the "mnt_dir" field of some
		 * other entry.  The "mnt_fsname" field of that entry
		 * is the one we want to use.
		 */
		if (strcmp(fstype, MNTTYPE_CACHEFS) == 0) {
			for (m = mntl; m; m = m->mntl_next) {
				if (strcmp(m->mntl_mnt->mnt_dir, fsname) == 0) {
					fsname = m->mntl_mnt->mnt_fsname;
					break;
				}
			}
		}

		if (strcmp(fstype, MNTTYPE_LOFS) == 0) {
			if (loopbackmount(fsname, spec_mntpnt, opts, overlay)) {
				syslog(LOG_ERR, "Could not remount %s\n",
					mntpnt);
				/*
				 * The lofs mount failed, so attempt to add
				 * the autofs trigger direct mount point.
				 */
				snprintf(buf, 4096, "-remount%s", fsname);
				snprintf(buf1, B_SIZE, "%s,%s,rfstype=%s", 
					 REMOUNTOPTS, opts, MNTTYPE_LOFS);
				autofs_remount(whole_list, spec_mntpnt, buf,
					       buf1);
				/* 
				 * UnMark any submounts of this directory.
				 */
				for (m=mntl; m; m=m->mntl_next)	{
					len = strlen(mntpnt);
					if (strncmp(mntpnt, 
						    m->mntl_mnt->mnt_dir, len)
					    == 0) {
						if (m->mntl_mnt->mnt_dir[len] 
						    == '/' || 
						    m->mntl_mnt->mnt_dir[len] 
						    == '\0')
							m->mntl_flags &= 
								~MNTL_UNMOUNT;
					}
				}
			}
			continue;
		}

		if (mount_generic(fsname, fstype, opts, mntpnt, overlay)) {
			syslog(LOG_ERR, "Could not remount %s",
				mntpnt);
		}
	}
}

static int
unmount_hierarchy(struct mntlist *whole_list,struct mntlist *mntl_head)
{
	struct mntlist *mntl;
	int res = 0;
	for (mntl = mntl_head; mntl; mntl = mntl->mntl_next)
		if (mntl->mntl_flags & MNTL_UNMOUNT) {
			break;
		}
	if (mntl == NULL)
		return (0);

	res = unmount_hierarchy(whole_list,mntl->mntl_next);
	if (res) {
		return (res);
	}

	res = unmount_mntpnt(mntl);
	if (res)
		remount_hierarchy(whole_list,mntl->mntl_next);

	return (res);
}

static int
removeit(struct mntent *mnt)
{
	char *p, c;
	int res;

	/*
	 * Remove appended space
	 */
	p = &mnt->mnt_dir[strlen(mnt->mnt_dir) - 1];
	c = *p;
	if (c == ' ')
		*p = '\0';

	AUTOFS_LOCK(mtab_lock);	
	res = delmntent(MOUNTED, mnt);
	AUTOFS_UNLOCK(mtab_lock);

	if (res < 0) {
		syslog(LOG_ERR, "delmntent of %s failed", mnt->mnt_dir);
	}
	return(0);
}

/* This procedure should return errno */

static int
unmount_mntpnt(mntl)
	struct mntlist *mntl;
{
	char *fstype = mntl->mntl_mnt->mnt_type;
	char *mountp = mntl->mntl_mnt->mnt_dir;
	char *newargv[ARGV_MAX];
	int res, mntstat;
	cap_t ocap;
	const cap_value_t umount_caps[] = {CAP_MOUNT_MGT, CAP_PRIV_PORT, CAP_MAC_READ};
	cap_value_t cap_audit_write = CAP_AUDIT_WRITE;

	if (strcmp(fstype, MNTTYPE_NFS3) == 0 ||
	    strcmp(fstype, MNTTYPE_NFS) == 0) {
		res = nfsunmount(mntl);
		if (res == 0) {
			res = removeit(mntl->mntl_mnt);
		}
	} else if (strcmp(fstype, MNTTYPE_LOFS) == 0 || 
		   strcmp(fstype, MNTTYPE_AUTOFS) == 0) {
		extern int cap_umount (struct mntlist *);

		mntstat = cap_umount(mntl);
		res = ((mntstat < 0) ? errno : removeit(mntl->mntl_mnt));
	} else {
		/*
		 * tell the umount command this request is from autofsd and not to
		 * use realpath
		 */
		newargv[2] = "-A";
		newargv[3] = mountp;
		newargv[4] = NULL;

		res = fork_exec(fstype, "umount", newargv, verbose);

		if (res == ENOENT) {
			ocap = cap_acquire(_CAP_NUM(umount_caps), umount_caps);
			mntstat = umount(mountp);
			cap_surrender(ocap);
			res = ((mntstat < 0) ? errno :
			       removeit(mntl->mntl_mnt));
		}
	}
	ocap = cap_acquire(_CAP_NUM(cap_audit_write), &cap_audit_write);
	if (res < 0) 
	  satvwrite(SAT_AE_MOUNT, SAT_FAILURE,
		    "autofs: failed umount of %s : %s", mountp, strerror(errno));
	else 
	  satvwrite(SAT_AE_MOUNT, SAT_SUCCESS,
		    "autofs: umount of %s : %s", mountp, strerror(errno));
	cap_surrender(ocap);
	if (trace)
		(void) syslog(LOG_ERR, "unmount %s %s\n",
			mountp, res ? "failed" : "OK");
	return (res);
}
