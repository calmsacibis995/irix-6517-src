/*
 * Filesystem space usage statistics
 */

#ident "$Id: filesys.c,v 1.20 1998/11/04 23:24:34 tes Exp $"

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>
#include "pmapi.h"
#include "impl.h"
#include "cluster.h"
#include "filesys.h"

static struct statfs	statfsx;

#if (_MIPS_SZLONG == 64) || (_MIPS_SIM == _ABIN32)
/* 64-bit counters */
#define PM_TYPE PM_TYPE_64
#else
/* 32-bit counters */
#define PM_TYPE PM_TYPE_32
#endif

static pmMeta		meta[] = {
/* irix.filesys.capacity */
  { (char *)&statfsx.f_blocks, { PMID(1,27,1), PM_TYPE, PM_INDOM_FILESYS, PM_SEM_INSTANT, {1,0,0,PM_SPACE_KBYTE,0,0} } },
/* irix.filesys.used */
  { (char *)&statfsx.f_blocks, { PMID(1,27,2), PM_TYPE, PM_INDOM_FILESYS, PM_SEM_INSTANT, {1,0,0,PM_SPACE_KBYTE,0,0} } },
/* irix.filesys.free */
  { (char *)&statfsx.f_bfree,  { PMID(1,27,3), PM_TYPE, PM_INDOM_FILESYS, PM_SEM_INSTANT, {1,0,0,PM_SPACE_KBYTE,0,0} } },
/* irix.filesys.maxfiles */
  { (char *)&statfsx.f_files,  { PMID(1,27,4), PM_TYPE, PM_INDOM_FILESYS, PM_SEM_INSTANT, {0,0,0,0,0,0} } },
/* irix.filesys.usedfiles */
  { (char *)&statfsx.f_files,  { PMID(1,27,5), PM_TYPE, PM_INDOM_FILESYS, PM_SEM_INSTANT, {0,0,0,0,0,0} } },
/* irix.filesys.freefiles */
  { (char *)&statfsx.f_ffree,  { PMID(1,27,6), PM_TYPE, PM_INDOM_FILESYS, PM_SEM_INSTANT, {0,0,0,0,0,0} } },
/* irix.filesys.mountdir */
  { (char *)0,  { PMID(1,27,7), PM_TYPE_STRING, PM_INDOM_FILESYS, PM_SEM_DISCRETE, {0,0,0,0,0,0} } },
/* irix.filesys.full */
  { (char *)0,  { PMID(1,27,8), PM_TYPE_DOUBLE, PM_INDOM_FILESYS, PM_SEM_DISCRETE, {0,0,0,0,0,0} } },
/* hinv.nfilesys */
  { (char *)0, { PMID(1,27,9), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE, {0,0,1,0,0,PM_COUNT_ONE} } },
};

static int		nmeta = (sizeof(meta)/sizeof(meta[0]));

fsinst_t	*fsilist = (fsinst_t *)0; /* Array of instances */
int		nfsi = 0;		/* Number of instances in fsilist */
int		nmfs = 0;		/* Number of mounted filesystems */
static int	newfsinst = 1;		/* Instance id for next new instance */
static int	fsisize = 0;		/* Size of fsilist */
static time_t	mtime = (time_t) 0;
static FILE	*mtab = NULL;
static int	direct_map = 1;

void
filesys_init(int reset)
{
    int	i;
    int	indomtag;		/* Constant from descr in form */

    if (reset)
	return;

    for (i = 0; i < nmeta; i++) {
	indomtag = meta[i].m_desc.indom;
	if (direct_map && meta[i].m_desc.pmid != PMID(1,27,i+1)) {
	    direct_map = 0;
	    __pmNotifyErr(LOG_WARNING, 
			 "filesys_init: direct map disabled @ meta[%d]", i);
	}
	if (indomtag == PM_INDOM_NULL)
	    continue;
	if (indomtag < 0 || indomtag >= PM_INDOM_NEXT) {
	    __pmNotifyErr(LOG_ERR, "filesys_init: bad instance domain (%d) for metric %s\n",
			 indomtag, pmIDStr(meta[i].m_desc.pmid));
	    continue;
	}
	/* Replace tag with it's indom */
	meta[i].m_desc.indom = indomtab[indomtag].i_indom;
	meta[i].m_offset = (char *)((ptrdiff_t)(meta[i].m_offset - (char *)&statfsx));
    }
    /* Defer initialising instances until last possible moment */
}

int
filesys_desc(pmID pmid, pmDesc *desc)
{
    int		i;

    for (i = 0; i < nmeta; i++) {
	if (pmid == meta[i].m_desc.pmid) {
	    *desc = meta[i].m_desc;	/* struct assignment */
	    return 0;
	}
    }
    return PM_ERR_PMID;
}

static int
initinsts(void)
{
    if ((mtab = setmntent(MOUNTED, "r")) == NULL) {
	__pmNotifyErr(LOG_ERR, "initinsts: setmntent: %s\n", strerror(errno));
	return -1;
    }
    fsisize = 4;
    if ((fsilist = (fsinst_t *)
		   malloc(fsisize * sizeof(fsinst_t))) == (fsinst_t *)0) {
	__pmNotifyErr(LOG_ERR, "initinsts: malloc: %s\n", strerror(errno));
	fsisize = 0;
	return -1;
    }

    return 0;
}

static int
newinst(int old_nfsi)
{
    int i;

    for (i = 0; i < old_nfsi; i++)
	if (fsilist[i].status.used == 0) {
	    if (fsilist[i].fsname != NULL)
	    	free(fsilist[i].fsname);
	    fsilist[i].fsname = NULL;
	    if (fsilist[i].mntdir != NULL)
	    	free(fsilist[i].mntdir);
	    fsilist[i].mntdir = NULL;
	    break;
	}

    if (i < old_nfsi)
	return i;

    if (nfsi < fsisize)
	return nfsi++;
    fsisize *= 2;
    if ((fsilist = (fsinst_t *)
	 realloc(fsilist, fsisize * sizeof(fsinst_t))) == (fsinst_t *)0)
	return -1;
    return nfsi++;
}


int
refresh_filesys(void)
{
    int			i;
    int			end;
    struct mntent	*mntent;
    struct stat		mstat;

#ifdef PCP_DEBUG
    if (pmIrixDebug & DBG_IRIX_FILESYS)
	__pmNotifyErr(LOG_DEBUG, "refresh_filesys: entering\n");
#endif

    if (mtab == NULL)
	if (initinsts() < 0)
	    return 0;

    /* No need to refresh unless mtab (MOUNTED) has been modified.
     * When an entry is added to MTAB by mount(1), MTAB is extended, but when
     * an entry is removed, a new file is created and the old one removed.
     * Thus the modification time of the file and the link count are checked.
     */
    if (fstat(fileno(mtab), &mstat) < 0) {
	__pmNotifyErr(LOG_ERR, "refresh_filesys: fstat: %s\n", strerror(errno));
	return nmfs;
    }

    if (mtime == mstat.st_mtime && mstat.st_nlink) {
#ifdef PCP_DEBUG
	if (pmIrixDebug & DBG_IRIX_FILESYS)
	    __pmNotifyErr(LOG_DEBUG, "refresh_filesys: mtab unchanged\n");
#endif
	return nmfs;
    }

    /* Mark all instances as tenuous */
    for (i = 0; i < nfsi; i++)
	fsilist[i].status.mounted = 0;

    end = nfsi;
    nmfs = 0;

    mtime = mstat.st_mtime;
    if (mstat.st_nlink)			/* Still the same MTAB file */
	rewind(mtab);
    else {				/* Open the new MTAB file */
	endmntent(mtab);
	if ((mtab = setmntent(MOUNTED, "r")) == NULL) {
	    __pmNotifyErr(LOG_ERR, "refresh_filesys: setmntent: %s\n",
			 strerror(errno));
	    return nmfs;
	}
    }

    while ((mntent = getmntent(mtab)) != (struct mntent *)0) {
	fsinst_t *new;

	/* 
	 * Ignore unwanted filesystems (type == nfs, ignore, debug,
	 * proc (==DBG, see sys/fsid.h) and fd, etc) -- only interested
	 * in efs and xfs
	 */
	if (strcmp(mntent->mnt_type, MNTTYPE_EFS) != 0 &&
	    strcmp(mntent->mnt_type, MNTTYPE_XFS) != 0)
	    continue;

	/* See if we already had the filesystem */
	for (i = 0; i < end; i++)
	    if (strcmp(fsilist[i].fsname, mntent->mnt_fsname) == 0 &&
		strcmp(fsilist[i].mntdir, mntent->mnt_dir) == 0)
		break;

	if (i < end) {			/* Only new entries from end onwards */
#ifdef PCP_DEBUG
	    if (pmIrixDebug & DBG_IRIX_FILESYS)
		__pmNotifyErr(LOG_DEBUG, 
			      "refresh_filesys: found %s (%d) again\n",
			      fsilist[i].fsname, i);
#endif

	    /* Mark this file system as mounted and in use */
	    fsilist[i].status.mounted = 1;
	    fsilist[i].status.used = 1;
	    nmfs++;
	    continue;
	}

	if ((i = newinst(end)) < 0) {
	    __pmNotifyErr(LOG_ERR, "refresh_filesys: newinst: %s\n", strerror(errno));
	    return nmfs;		/* No memory, work with what we have */
	}
	new = &fsilist[i];
	new->fsname = strdup(mntent->mnt_fsname);
	new->mntdir = strdup(mntent->mnt_dir);
	/* Don't need any other mntent fields */

	if (new->fsname == (char *)0 ||
	    new->mntdir == (char *)0) {
	    __pmNotifyErr(LOG_ERR, "refresh_filesys: mntent: %s\n",
			 strerror(errno));
	    return nmfs;		/* No memory, work with what we have */
	}

	/* We'll run out of memory long before this overflows! */
	new->inst = newfsinst++;

	/* Mark filesystem as mounted and in use */
	new->status.mounted = 1;
	new->status.fetched = 0;
	new->status.used = 1;
	nmfs++;
    }

    /*
     * Harvest those filesystems that were mounted but are not in the
     * current mtab. Do not remove the entries in case they will be
     * mounted again soon, just mark them as not in use so they may
     * be replaced by another file system later.
     */

    for (i = 0; i < end; i++) {
	if (fsilist[i].status.mounted == 0 && fsilist[i].status.used) {
	    fsilist[i].status.used = 0;
#ifdef PCP_DEBUG
	    if (pmIrixDebug & DBG_IRIX_FILESYS)
		__pmNotifyErr(LOG_DEBUG,
			      "refresh_filesys: Deactivating %d: id = %d, mntdir = %s, fsname = %s\n",
			      i, fsilist[i].inst, fsilist[i].mntdir,
			      fsilist[i].fsname);
#endif
	}
    }

#ifdef PCP_DEBUG
    if (pmIrixDebug & DBG_IRIX_FILESYS)
	__pmNotifyErr(LOG_DEBUG,
		      "refresh_filesys: normal exit, nmfs = %d\n", nmfs);
#endif

    return nmfs;
}

void
filesys_fetch_setup(void)
{
    int i;

    for (i = 0; i < nfsi; i++)
	fsilist[i].status.fetched = 0;
}

int
filesys_fetch(pmID pmid, __pmProfile *profp, pmVPCB *vpcb)
{
    int			i, j;
    int			sts;
    int			nval = 0;
    pmAtomValue		av;
    void		*vp;

    if (direct_map) {
	__pmID_int       *pmidp = (__pmID_int *)&pmid;
        i = pmidp->item - 1;
	if (i < nmeta && pmid == meta[i].m_desc.pmid)
	    goto doit;

	__pmNotifyErr(LOG_WARNING, 
		     "filesys_fetch: direct mapping failed for %s (!= %s)\n",
		     pmIDStr(pmid), pmIDStr(meta[i].m_desc.pmid));
	direct_map = 0;
    }

    for (i = 0; i < nmeta; i++)
	if (pmid == meta[i].m_desc.pmid)
	    break;

    if (i >= nmeta) {
	vpcb->p_nval = 0;
	return PM_ERR_PMID;
    }

 doit:

    refresh_filesys();

    if (pmid == PMID(1,27,9)) {		/* hinv.nfilesys */
	vpcb->p_nval = 1;
	vpcb->p_vp[0].inst = PM_IN_NULL;
	av.ul = nmfs;
	if ((sts = __pmStuffValue(&av, 0, &vpcb->p_vp[0],
				  meta[i].m_desc.type)) < 0)
	    return sts;
	vpcb->p_valfmt = sts;
	return sts;
    }

    /* Call statfs for mounted filesystems in instance profile that haven't
     * been done already.
     */
    for (j = 0; j < nfsi; j++) {
	if (!__pmInProfile(indomtab[PM_INDOM_FILESYS].i_indom, profp, fsilist[j].inst))
	    continue;
	if (!fsilist[j].status.mounted)
	    continue;
	if (!fsilist[j].status.fetched)
	    if (statfs(fsilist[j].mntdir, &fsilist[j].statfs,
		       sizeof(fsilist[j].statfs), 0) < 0) {
		__pmNotifyErr(LOG_ERR, "_filesys_fetch: statfs: %s\n",
			     strerror(errno));
		continue;
	    }
	    else
		fsilist[j].status.fetched = 1;

	vpcb->p_nval = nval+1;
	sizepool(vpcb);
	if (pmid == PMID(1,27,7))
	    av.cp = fsilist[j].mntdir;
	else if (pmid != PMID(1,27,8)) {
	    vp = (void *)&((char *)&fsilist[j].statfs)[(ptrdiff_t)meta[i].m_offset];
	    avcpy(&av, vp, meta[i].m_desc.type);
	}

	switch (pmid) {
	    case PMID(1,27,2):		/* Currently, value == capacity */
#if PM_TYPE == PM_TYPE_64
		av.ll -= fsilist[j].statfs.f_bfree;
#else
		av.l -= fsilist[j].statfs.f_bfree;
#endif
		break;

	    case PMID(1,27,5):		/* Currently, value == capacity */
#if PM_TYPE == PM_TYPE_64
		av.ll -= fsilist[j].statfs.f_ffree;
#else
		av.l -= fsilist[j].statfs.f_ffree;
#endif
		break;
	    case PMID(1,27,8):
		if (fsilist[j].statfs.f_blocks > 0)
		    av.d = ((fsilist[j].statfs.f_blocks - 
			    fsilist[j].statfs.f_bfree) /
				(double)fsilist[j].statfs.f_blocks) * 100.0;
		else
		    av.d = 0.0;
		break;
	}

	/* Convert from blocks to Kbytes for space metrics
	 */
	if (pmid >= PMID(1,27,1) && pmid <= PMID(1,27,3))
#if PM_TYPE == PM_TYPE_64
	    av.ll = (__int64_t)((double)av.ll * (double)fsilist[j].statfs.f_bsize / 1024.0);
#else
	    av.l = (__int32_t)((float)av.l * (float)fsilist[j].statfs.f_bsize / 1024.0);
#endif

	if ((sts = __pmStuffValue(&av, 0, &vpcb->p_vp[nval],
			      meta[i].m_desc.type)) < 0)
	    return sts;
	vpcb->p_vp[nval].inst = fsilist[j].inst;
	nval++;
    }
    vpcb->p_valfmt = sts;
    vpcb->p_nval = nval;
    return 0;
}
