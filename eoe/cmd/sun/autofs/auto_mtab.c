/*
 *	auto_mtab.c
 *
 *	Copyright (c) 1988-1993 Sun Microsystems Inc
 *	All Rights Reserved.
 */

#ident	"@(#)auto_mtab.c	1.5	93/11/03 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <errno.h>
#include <string.h>
#include <dirent.h>
#include <mntent.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/signal.h>
#include <sys/fs/autofs.h>
#include <sys/capability.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include "autofsd.h"

struct mntlist *mkmntlist(FILE *);
dev_t get_devid(struct mntent *);
dev_t get_lofsid(struct mntent *);
struct mntent *dupmntent(struct mntent *);
void freemntent(struct mntent *);
int  putmntlist(FILE *, struct mntlist *);
extern void trim(char *);

/*
 * Delete a single entry from the mtab
 * given its mountpoint.
 * Make sure the last instance in the
 * file is deleted.
 */
void
del_mtab(mntpnt)
	char *mntpnt;
{
	FILE *mtab;
	struct mntlist *delete;
	struct mntlist *mntl_head, *mntl;
	char *p, c;

	AUTOFS_LOCK(mtab_lock);
	mtab = setmntent(MOUNTED, "r+");
	if (mtab == NULL) {
		pr_msg("%s: %m", MOUNTED);
		AUTOFS_UNLOCK(mtab_lock);
		return;
	}

	/*
	 * Read the list of mounts
	 */
	mntl_head = mkmntlist(mtab);
	if (mntl_head == NULL)
		goto done;

	/*
	 * Remove appended space
	 */
	p = &mntpnt[strlen(mntpnt) - 1];
	c = *p;
	if (c == ' ')
		*p = '\0';

	/*
	 * Mark the last entry that matches the
	 * mountpoint for deletion.
	 */
	for (mntl = mntl_head; mntl; mntl = mntl->mntl_next) {
		if (strcmp(mntl->mntl_mnt->mnt_dir, mntpnt) == 0)
			delete = mntl;
	}
	*p = c;
	if (delete)
		delete->mntl_flags |= MNTL_UNMOUNT;

	/*
	 * Write the mount list back
	 */
	putmntlist(mtab, mntl_head);

done:
	freemntlist(mntl_head);
	(void) endmntent(mtab);
	AUTOFS_UNLOCK(mtab_lock);
}


#define	TIME_MAX 16
#define	RET_ERR  1

/*
 * Add a new entry to the /etc/mtab file.
 * Include the device id with the mount options.
 */

int
add_mtab(mnt)
	struct mntent *mnt;
{
	FILE *fd;
	char *opts;
	struct stat st;
	char obuff[256], *pb = obuff;
	int len;
	char real_mntpnt[MAXPATHLEN + 1];
	cap_t ocap;
	cap_value_t cap_mac_read = CAP_MAC_READ;

	/*
	 * the passed in mountpoint may have a space
	 * at the end of it. If so, it is important that
	 * the stat is done without changing it.
	 */
	ocap = cap_acquire(_CAP_NUM(cap_mac_read), &cap_mac_read);
	if (stat(mnt->mnt_dir, &st) < 0) {
		cap_surrender(ocap);
		pr_msg("%s: %m", MOUNTED);
		return (ENOENT);
	}
	cap_surrender(ocap);

	len = strlen(mnt->mnt_dir);
	if (mnt->mnt_dir[len - 1] == ' ') {
		(void) strncpy(real_mntpnt, mnt->mnt_dir, len - 1);
		real_mntpnt[len - 1] = '\0';
	} else
		strcpy(real_mntpnt, mnt->mnt_dir);
	mnt->mnt_dir = real_mntpnt;

	AUTOFS_LOCK(mtab_lock);
	fd = setmntent(MOUNTED, "r+");
	if (fd == NULL) {
		pr_msg("%s: %m", MOUNTED);
		AUTOFS_UNLOCK(mtab_lock);
		return (RET_ERR);
	}

	opts = mnt->mnt_opts;

	/*
	 * Normalize NFS fstype and mount options to look like
	 * blah:blah on /blah type nfs (vers=n,..,dev=x) in mtab.
	 * At this point, MNTTYPE_NFS means NFS V2 and MNTTYPE_NFS3 
	 * means V3.
	 * For LoFS mounts, include their ids in the options.
	 */
	if (strcmp(mnt->mnt_type, MNTTYPE_NFS3) == 0) {
		/* Note: printmtab assumes dev is last */
		if (hasmntopt(mnt, MNTOPT_VERS) == NULL) {
			len = sprintf(pb, "vers=3");
			pb += len;
			*pb++ = ',';
		}
		mnt->mnt_type = MNTTYPE_NFS;
	} else if (strcmp(mnt->mnt_type, MNTTYPE_NFS) == 0) {
		/* Note: printmtab assumes dev is last */
		if (hasmntopt(mnt, MNTOPT_VERS) == NULL) {
			len = sprintf(pb, "vers=2");
			pb += len;
			*pb++ = ',';
		}
	}
	if (opts && *opts) {
		len = sprintf(pb, opts);
		pb += len;
		*pb++ = ',';
	}
	if (strcmp(mnt->mnt_type, MNTTYPE_LOFS) == 0) {
		len = sprintf(pb, "%s=%d", MNTINFO_LOFSID, st.st_rdev);
		pb += len;
		*pb++ = ',';
	}

	(void) sprintf(pb, "%s=%x", MNTINFO_DEV, st.st_dev);
	mnt->mnt_opts = obuff;
	(void) fseek(fd, 0L, 2); /* guarantee at EOF */

	addmntent(fd, mnt);

	mnt->mnt_opts = opts;
	endmntent(fd);
	AUTOFS_UNLOCK(mtab_lock);

	return (0);
}

/*
 * Replace an existing entry with
 * a new one - a remount.
 */
void
fix_mtab(mnt)
	struct mntent *mnt;
{
	FILE *mtab;
	struct mntlist *mntl_head, *mntl;
	struct mntlist *found;
	char *opts;
	char *newopts, *pn;

	AUTOFS_LOCK(mtab_lock);
	mtab = setmntent(MOUNTED, "r+");
	if (mtab == NULL) {
		pr_msg("%s: %m", MOUNTED);
		AUTOFS_UNLOCK(mtab_lock);
		return;
	}

	/*
	 * Read the list of mounts
	 */
	mntl_head = mkmntlist(mtab);
	if (mntl_head == NULL)
		goto done;

	/*
	 * Find the last entry that matches the
	 * mountpoint.
	 */
	found = NULL;
	for (mntl = mntl_head; mntl; mntl = mntl->mntl_next) {
		if (strcmp(mntl->mntl_mnt->mnt_dir, mnt->mnt_dir) == 0)
			found = mntl;
	}
	if (found == NULL) {
		pr_msg("Cannot find mntpnt for %s", mnt->mnt_dir);
		goto done;
	}

	newopts = (char *) malloc(256);
	if (newopts == NULL) {
		pr_msg("fix_mtab: no memory");
		goto done;
	}

	pn = newopts;
	opts = mnt->mnt_opts;
	if (opts && *opts) {
		(void) strcpy(pn, opts);
		trim(pn);
		pn += strlen(pn);
		*pn++ = ',';
	}

	(void) sprintf(pn, "%s=%x", MNTINFO_DEV, get_devid(found->mntl_mnt));
	mnt->mnt_opts = newopts;

	freemntent(found->mntl_mnt);
	found->mntl_mnt = dupmntent(mnt);

	/*
	 * Write the mount list back
	 */
	if (putmntlist(mtab, mntl_head))
		goto done;

done:
	freemntlist(mntl_head);
	endmntent(mtab);
	AUTOFS_UNLOCK(mtab_lock);
}


/*
 * Free a single mntent structure
 */
void
freemntent(mnt)
	struct mntent *mnt;
{
	if (mnt) {
		if (mnt->mnt_fsname)
			free(mnt->mnt_fsname);
		if (mnt->mnt_dir)
			free(mnt->mnt_dir);
		if (mnt->mnt_type)
			free(mnt->mnt_type);
		if (mnt->mnt_opts)
			free(mnt->mnt_opts);
		free(mnt);
	}
}

/*
 * Free a list of mntent structures
 */
void
freemntlist(mntl)
	struct mntlist *mntl;
{
	struct mntlist *mntl_tmp;

	while (mntl) {
		freemntent(mntl->mntl_mnt);
		mntl_tmp = mntl;
		mntl = mntl->mntl_next;
		free(mntl_tmp);
	}
}

/*
 * Duplicate a mntent structure
 */
struct mntent *
dupmntent(mnt)
	struct mntent *mnt;
{
	struct mntent *new;
	void freemntent();

	new = (struct mntent *)malloc(sizeof(*new));
	if (new == NULL)
		goto alloc_failed;
	memset((void *) new, '\0', sizeof(*new));
	new->mnt_fsname = strdup(mnt->mnt_fsname);
	if (new->mnt_fsname == NULL)
		goto alloc_failed;
	/*
	 * Allocate an extra byte for the mountpoint
	 * name in case a space needs to be added.
	 */
	new->mnt_dir = (char *) malloc(strlen(mnt->mnt_dir) + 2);
	if (new->mnt_dir == NULL)
		goto alloc_failed;
	(void) strcpy(new->mnt_dir, mnt->mnt_dir);
	new->mnt_type = strdup(mnt->mnt_type);
	if (new->mnt_type == NULL)
		goto alloc_failed;
	if (mnt->mnt_opts) {
		new->mnt_opts = strdup(mnt->mnt_opts);
		if (new->mnt_opts == NULL)
			goto alloc_failed;
	}
	new->mnt_freq   = mnt->mnt_freq;
	new->mnt_passno = mnt->mnt_passno;

	return (new);

alloc_failed:
	syslog(LOG_ERR, "dupmntent: memory allocation failed");
	freemntent(new);
	return NULL;
}

/*
 * Scan the mount option string and extract
 * the hex device id from the "dev=" string.
 * If the string isn't found get it from the
 * filesystem stats.
 */
dev_t
get_devid(mnt)
	struct mntent *mnt;
{
	dev_t val = 0;
	char *equal;
	char *str;
	struct stat st;
	cap_t ocap;
	cap_value_t cap_mac_read = CAP_MAC_READ;

	if (str = hasmntopt(mnt, MNTINFO_DEV)) {
		if (equal = strchr(str, '='))
			val = strtol(equal + 1, (char **) NULL, 16);
		else
			syslog(LOG_ERR, "Bad device option '%s'", str);
	}

	if (val == 0) {		/* have to stat the mountpoint */
		ocap = cap_acquire(_CAP_NUM(cap_mac_read), &cap_mac_read);
		if (stat(mnt->mnt_dir, &st) < 0)
			syslog(LOG_ERR, "stat %s: %m", mnt->mnt_dir);
		else
			val = st.st_dev;
		cap_surrender(ocap);
	}

	return (val);
}

/*
 * Scan the mount option string and extract
 * the decimal lofs id from the "lofsid=" string.
 * If the string isn't found, get it from the
 * filesystem stats.
 */
dev_t
get_lofsid(mnt)
	struct mntent *mnt;
{
	dev_t val = 0;
	char *equal;
	char *str;
	struct stat st;
	cap_t ocap;
	cap_value_t cap_mac_read = CAP_MAC_READ;

	if (str = hasmntopt(mnt, MNTINFO_LOFSID)) {
		if (equal = strchr(str, '='))
			val = strtoul(equal + 1, (char **) NULL, 10);
		else
			syslog(LOG_ERR, "Bad lofs id '%s'", str);
	}

	if (val == 0) {		/* have to stat the mountpoint */
		ocap = cap_acquire(_CAP_NUM(cap_mac_read), &cap_mac_read);
		if (stat(mnt->mnt_dir, &st) < 0)
			syslog(LOG_ERR, "stat %s: %m", mnt->mnt_dir);
		else
			val = st.st_rdev;
		cap_surrender(ocap);
	}
	return (val);
}

/*
 * Read the mtab file and return it
 * as a list of mntent structs.
 */
struct mntlist *
mkmntlist(mtab)
	FILE *mtab;
{
	struct mntent *mnt = NULL;
	struct mntlist *mntl_head = NULL;
	struct mntlist *mntl_prev, *mntl;

	while((mnt = getmntent(mtab)) != NULL) {
		mntl = (struct mntlist *) malloc(sizeof (*mntl));
		if (mntl == NULL)
			goto alloc_failed;
		if (mntl_head == NULL)
			mntl_head = mntl;
		else
			mntl_prev->mntl_next = mntl;
		mntl_prev = mntl;
		mntl->mntl_next = NULL;
		mntl->mntl_dev = get_devid(mnt);
		mntl->mntl_flags = 0;
		mntl->mntl_mnt = dupmntent(mnt);
		if (mntl->mntl_mnt == NULL)
			goto alloc_failed;
	}
	return (mntl_head);

alloc_failed:
	freemntlist(mntl_head);
	return (NULL);
}

struct mntlist *
getmntlist(void)
{
	FILE *mtab;
	struct mntlist *mntl;

	AUTOFS_LOCK(mtab_lock);
	mtab = setmntent(MOUNTED, "r+");
	if (mtab == NULL) {
		syslog(LOG_ERR, "%s: %m", MOUNTED);
		AUTOFS_UNLOCK(mtab_lock);
		return(NULL);
	}

	mntl = mkmntlist(mtab);

	(void) endmntent(mtab);
	AUTOFS_UNLOCK(mtab_lock);
	return (mntl);
}

int
putmntlist(mtab, mntl_head)
	FILE *mtab;
	struct mntlist *mntl_head;
{
	struct mntlist *mntl;
	int error = 0;

	rewind(mtab);
	if (ftruncate(fileno(mtab), 0) < 0) {
		pr_msg("truncate %s: %m", MOUNTED);
		return (1);
	}

	for (mntl = mntl_head; mntl; mntl = mntl->mntl_next) {
		if (mntl->mntl_flags & MNTL_UNMOUNT)
			continue;

		if (addmntent(mtab, mntl->mntl_mnt)) {
			pr_msg("addmntent: %m");
			error = 1;
		}
	}
	return (error);
}
