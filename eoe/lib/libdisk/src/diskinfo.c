/**************************************************************************
 *									  *
 * 		 Copyright (C) 1988, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident	"libdisk/diskinfo.c $Revision: 1.27 $ of $Date: 1996/06/29 01:08:35 $"

/*	{set,get,put,end}diskinfo() routines.
	interface modeled on the *mntent routines for
	the fstab file.
	First version created by Dave Olson @ SGI, 10/88
*/


#include <sys/types.h>
#include <sys/param.h>
#include <sys/sema.h>
#include <sys/fs/efs.h>
#include <sys/dvh.h>
#include <sys/dkio.h>
#include <sys/stat.h>
#include <stdio.h>
#include <mntent.h>
#include <diskinfo.h>
#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
#include <unistd.h>
#include <bstring.h>
#include <string.h>

/*
 * XFS include files.
 */
#include <sys/buf.h>
#include <sys/uuid.h>
#include <sys/fs/xfs_types.h>
#include <sys/fs/xfs_inum.h>
#include <sys/fs/xfs_log.h>
#include <sys/fs/xfs_trans.h>
#include <sys/fs/xfs_sb.h>



#define MAXOPEN 128	/* max of 128 disks open at a time */


extern int	getdiskheader(int, struct volume_header *);
extern int	readsuperblk_efs_relative(int, struct efs *);
extern int	readsuperblk_xfs_relative(int, xfs_sb_t *);
extern int	valid_efs(struct efs *, int);
extern int	valid_xfs(xfs_sb_t *, long long);


/* this structure is local to this module! */
struct disktoken {
	struct diskinfo dinfo;	/* per disk info */
	struct ptinfo ptinfo[NPARTS];	/* per partition info */
	int curpart;	/* current partition */
	int dfd;
	int dmode;
};

static int filldiskinfo(unsigned, char *, char *);

struct disktoken *disks[MAXOPEN];


/*	Do setup necessary to return disk information for the given disk.
	mode is O_RDONLY, O_RDWR, etc
	returns 0 on error.
	Name must be the type of name returned by getdiskname();
	i.e, {dks,xyl,ips}0d?{s?,vol,vh}, as the full pathname.
	This is not actually checked for, but partition information
	may be returned incorrectly if it is not, and the open may
	also fail.
	The name passed in is converted to the equivalent disk, but
	the "vol" partition.  So any valid suffix, or no suffix at
	all may be passed (i.e., /dev/rdsk/dks0d1 will work).

	fstab is used to search for mount info, and should normally
	be /etc/fstab, the mtab directory prefix is also derived from it.
*/
unsigned
setdiskinfo(char *dskname, char *fstab, int mode)
{
	register unsigned token;
	register struct disktoken *d;

	/* start from 1, 0 indicates error */
	for(token=1; token<MAXOPEN; token++) {
		if(!disks[token])
			break;
	}
	if(token == MAXOPEN)
		return 0;

	/* never creat as a file, in case the node doesn't exist */
	mode &= O_CREAT;

	if((d = (struct disktoken *)calloc(sizeof(*d), 1)) == NULL)
		return 0;
	
	disks[token] = d;
	if((d->dfd = open(partname(dskname, PT_VOLUME), mode)) == -1) {
		enddiskinfo(token);
		return 0;
	}

	d->dmode = mode;
	d->curpart = 0;

	if(filldiskinfo(token, fstab, dskname) == -1) {
		enddiskinfo(token);
		return 0;
	}
	return token;
}


/* we're done with this disk. free the buffer and close the file if open */
void
enddiskinfo(unsigned token)
{
	if(token < MAXOPEN && disks[token]) {
		if(disks[token]->dfd != -1)
			(void)close(disks[token]->dfd);
		free(disks[token]);
		disks[token] = 0;
	}
}


/*	check for EFS partitions with same size and starting block, and remove
	the 'incorrect' ones. Only one of them should have a mount pt
	associated with it.  Algorithm is to prefer PT_FSROOT, PT_FSUSR,
	and PT_FSALL when all are listed as having the same mount pt.
	This should only happen when we have used the name from the super block.
	If it is due to the pathalogical case of someone putting both
	partition names in fstab with different mount points, we don't
	change anything; who knows which actually is/was/will be mounted.
*/
static void
chkoverlaps(struct disktoken *d, register struct ptinfo *ptinfo,
	    unchar mtfrom[])
{
	unchar matches[NPARTS];
	register int j;
	int any = 0;
	register struct ptinfo *tmp;
	struct ptinfo **laps;
	int good = NPARTS;	/* an 'illegal' partition # */
	unsigned pnum;

	pnum = ptinfo->ptnum;
	good = mtfrom[pnum] ? NPARTS : pnum;
	bzero(matches, sizeof(matches));

	for(laps=ptinfo->overlaps; *laps; laps++) {
		tmp = *laps;
		if(ptinfo->pstartblk == tmp->pstartblk &&
			ptinfo->psize == tmp->psize &&
			ptinfo->ptype == tmp->ptype &&
			tmp->mountdir[0] &&
			strcmp(tmp->fsname, tmp->mountdir) == 0) {
			any++;
			matches[tmp->ptnum] = 1;
			if(!mtfrom[tmp->ptnum]) {
				if(good == NPARTS)
					good = tmp->ptnum;
				/* else	more than one found in fstab/mtab! */
			}
		}
	}
	if(any) {
		if(good == NPARTS) {
			/*	none or more than one found in fstab/mtab, so
				apply our 'favored parition rule if any of
				the dups are favored partions */
			if(pnum == PT_FSROOT || matches[PT_FSROOT])
				good = PT_FSROOT;
			else if(pnum == PT_FSUSR || matches[PT_FSUSR])
				good = PT_FSUSR;
			else if(pnum == PT_FSALL || matches[PT_FSALL])
				good = PT_FSALL;
			else /* don't know which to favor... */
				return;
		}
		for(j=pnum; j<NPARTS && any; j++) {
			if(j != good && (j==pnum || matches[j])) {
				d->ptinfo[j].mountdir[0] = '\0';
				any--;
			}
		}
	}
}


/*	set list of pointers to overlapping partitions, if any.  Note that the
	memory calloc'ed here will never be freed.  This is because the pointer
	could be changed by the caller;  The amount of memory is so small, that
	even with many disks it won't amount to very much.
	NOTE: we don't consider an overlap with PT_VOLUME; if a user ever
	changes that, they'll break all kinds of things!
*/
void
setoverlaps(struct disktoken *d)
{
	struct ptinfo *laps[NPARTS];
	register int i, j;
	register struct ptinfo *p;
	struct ptinfo *tmp;
	int overlaps;

	/* O(2), but few enough partitions that not worth worrying about... */
	for(p=d->ptinfo,i=0; i<NPARTS; i++, p++) {
		if(p->psize && p->ptnum != PT_VOLUME) {
			register unsigned first = p->pstartblk;
			register unsigned last = first + p->psize;
			overlaps = 0;
			for(tmp=d->ptinfo,j=0; j<NPARTS; j++, tmp++) {
				register unsigned ts;
				if(tmp == p || !tmp->psize || tmp->ptnum == PT_VOLUME)
					continue;
				ts = tmp->pstartblk;
				if((ts >= first && ts < last) ||
					(ts < first && (ts + tmp->psize) > first)) {
					laps[overlaps++] = tmp;
				}
			}
			if(overlaps) {
				p->overlaps = (struct ptinfo **)calloc(sizeof(p), overlaps+1);
				if(p->overlaps != NULL) {
					while(overlaps--)
						p->overlaps[overlaps] = laps[overlaps];
				}
			}
		}
	}
}


/*	fill in the disktoken struct as much as we can.  If disk is not
	initialized (i.e., no (or bad) volume header, get what we
	can from DIOCREADCAPACITY, etc).

	Daveh note 12/12/89: getdiskheader now always uses DIOCGETVH,
	because some disks have turned up where the on-disk vh is slightly
	wrong & we rely on the driver to sanity-check it for us on open.
	So there is no longer a "from_disk" parameter to it: & all callers
	better be sure they opened the RAW device!!
*/
static int
filldiskinfo(unsigned token, char *fstab, char *dname)
{
	static struct volume_header vhdr;
	struct efs efs_superb;
	xfs_sb_t xfs_superb;
	register struct disktoken *d = disks[token];
	struct diskinfo *dinfo;
	struct ptinfo *ptinfo;
	struct partition_table *vhptinfo;
	int fd = d->dfd;
	register unsigned i;
	unchar mtfromsb[NPARTS];
	extern int fs_readonly;
	off64_t		diskoffset;

	if (getdiskheader(fd, &vhdr) == -1)
	{
		return -1;
	}

	/* fill in volume stuff from volume header */
	dinfo = &d->dinfo;

	/*	this happened with the a39 fx release which didn't initialize the
		secbytes field correctly if a 'create' was done before anything
		else, as was done in the disk test setup.  Nothing else uses
		this field, so it wasn't noticed in time... Olson, 11/88 */
	if ((dinfo->sectorsize = vhdr.vh_dp.dp_secbytes) == 0)
		dinfo->sectorsize = NBPSCTR;
	dinfo->capacity = vhdr.vh_dp.dp_drivecap;

	bzero(mtfromsb, sizeof(mtfromsb));	/* zero mount pt info */

	/* fill in partition stuff from volume header */
	ptinfo = d->ptinfo;
	vhptinfo = vhdr.vh_pt;

	for (i = 0; i < NPARTS; i++, ptinfo++, vhptinfo++) {
		ptinfo->pstartblk = vhptinfo->pt_firstlbn;
		ptinfo->ptype =  vhptinfo->pt_type;
		ptinfo->fsstate = NOFS;
		ptinfo->overlaps = NULL;
		ptinfo->ptnum = i;
		ptinfo->psize = vhptinfo->pt_nblks;

		/* check because some early systems went out that way even
		 * though really EFS. */

		if (ptinfo->psize
		   && (ptinfo->ptype == PTYPE_EFS
		       || ptinfo->ptype == 5 /* was SYSV */
		       || ptinfo->ptype == PTYPE_XLV
		       || ptinfo->ptype == PTYPE_XFS)) {
			
			diskoffset = (off64_t)(ptinfo->pstartblk*dinfo->sectorsize);

			if (lseek64(fd, diskoffset, SEEK_SET) >= 0
			    && readsuperblk_efs_relative(fd, &efs_superb) == 0
			    && valid_efs(&efs_superb, ptinfo->psize) == 1) {
				/*
				 * this will require mods later, since 
				 * the state could be left from a crash;
				 * also, the fs_dirty isn't always
				 * marked ACTIVE when mounted, sometimes
				 * it just stays CLEAN.  In addition, 
				 * the fs_readonly check is pointless, 
				 * since if it is mounted readonly, the
				 * superblock will never be written 	
				 * back to disk. I have left the check 
				 * in just to be thorough...
				 */
				switch(efs_superb.fs_dirty) {
				case EFS_CLEAN:
					ptinfo->fsstate = CLEAN;
					break;
				case EFS_ACTIVEDIRT:
				case EFS_ACTIVE:
					ptinfo->fsstate = 
				efs_superb.fs_readonly ? MTRDONLY : MOUNTEDFS;
					break;
				case EFS_DIRTY:
					ptinfo->fsstate = DIRTY;
					break;
				}
				strncpy(ptinfo->fsname, efs_superb.fs_fname,
					sizeof(efs_superb.fs_fname));
				strncpy(ptinfo->packname, efs_superb.fs_fpack,
					sizeof(efs_superb.fs_fpack));
				ptinfo->fssize = efs_superb.fs_size;

				/*	
				 * same calculations used by efs_statfs; 
				 * change to do a statfs later ? 
				 */
				ptinfo->ftotal = efs_superb.fs_size -
				    efs_superb.fs_ncg * efs_superb.fs_cgisize - 
				    efs_superb.fs_firstcg - 2;
				ptinfo->itotal = 
				    (efs_superb.fs_cgisize * efs_superb.fs_ncg *
				    EFS_INOPBB) - 2;

				ptinfo->fsfree = efs_superb.fs_tfree;
				ptinfo->ifree  = efs_superb.fs_tinode;
				strncpy(ptinfo->mountdir, 
					getmountpt(fstab, partname(dname, i)),
					sizeof(ptinfo->mountdir));

		
				/*
				 * mount pt from superblk.
				 */
				if (mtpt_fromwhere == MTPT_FROM_SBLK)
					mtfromsb[i] = 1;

				/* 
				 * fs_readonly is set as a side 
				 * effect by getmountpt 
				 */
				if (fs_readonly && ptinfo->fsstate == CLEAN)
					ptinfo->fsstate = MTRDONLY;

			} else if (lseek64(fd, diskoffset, SEEK_SET) >= 0
				   && readsuperblk_xfs_relative(fd, &xfs_superb) == 0
				   && valid_xfs(&xfs_superb, ptinfo->psize) == 1) {

				/*
				 * Always mark the file system as
				 * clean.
				 */
				ptinfo->fsstate = CLEAN;

				strncpy(ptinfo->fsname, xfs_superb.sb_fname,
					sizeof(xfs_superb.sb_fname));
				strncpy(ptinfo->packname, xfs_superb.sb_fpack,
					sizeof(xfs_superb.sb_fpack));

				/*
				 * Size of file system in blocks.
				 */
				ptinfo->fssize = (unsigned)xfs_superb.sb_dblocks;

				/*	
				 * Total blocks include data and real time.
				 */
				ptinfo->ftotal = (unsigned)(xfs_superb.sb_dblocks + 
					(xfs_superb.sb_rextents*
					xfs_superb.sb_rextsize));

				ptinfo->itotal = (unsigned)xfs_superb.sb_icount;

				ptinfo->fsfree = (unsigned)(xfs_superb.sb_fdblocks +
					(xfs_superb.sb_frextents * 
					xfs_superb.sb_rextsize));

				ptinfo->ifree  = (unsigned)xfs_superb.sb_ifree;

				strncpy(ptinfo->mountdir, 
					getmountpt(fstab, partname(dname, i)),
					sizeof(ptinfo->mountdir));
		
				/*
				 * mount pt from superblk.
				 */
				if (mtpt_fromwhere == MTPT_FROM_SBLK)
					mtfromsb[i] = 1;

				/* 
				 * fs_readonly is set as a side 
				 * effect by getmountpt 
				 */
				if (fs_readonly && ptinfo->fsstate == CLEAN)
					ptinfo->fsstate = MTRDONLY;

			}
		}
	}

	/* Now overlap check stuff. */

	/* set pointers to any overlapping partitions */
	setoverlaps(d);

	/* if the partition claims to be a filesystem, and it has a mountpt
		associated with it, check for multiple partitions with same
		size, offset, and type.  Only one 'should' be the real filesystem. */

	for(ptinfo=d->ptinfo,i=0; i<NPARTS; i++, ptinfo++)
		if(ptinfo->fsstate != NOFS &&
			ptinfo->overlaps && ptinfo->mountdir[0] &&
			strcmp(ptinfo->fsname, ptinfo->mountdir) == 0)
			chkoverlaps(d, ptinfo, mtfromsb);
	return 0;
}


struct diskinfo *
getdiskinfo(unsigned token)
{
	register struct disktoken *d = disks[token];

	if(!d)
		return NULL;
	return &d->dinfo;
}


/* return info for next partition of non-zero size */
struct ptinfo *
getpartition(unsigned token)
{
	register struct disktoken *d = disks[token];
	register struct ptinfo *pt;

	if(!d || d->curpart >= NPARTS)
		return NULL;

	pt = &d->ptinfo[d->curpart];
	while(pt->psize == 0 && ++d->curpart < NPARTS)
		pt++;
	if(d->curpart == NPARTS)
		return NULL;
	d->curpart++;
	return pt;
}



/*	write out new info on the partition.  Does some basic sanity
	checks to be sure partition isn't larger than the disk.
	It doesn't check for overlapping partitions since they
	make sense for some things, like the whole disk partition,
	and partition PT_VOLUME and PT_FSALL.
*/
/*ARGSUSED*/
int
putpartition(unsigned token, register struct ptinfo *pt)
{
	register struct disktoken *d = disks[token];

	if(!(d->dmode & O_WRONLY))
		return -1;	/* didn't open for writing */
	return 1;	/* failed; not implemented yet */
}



/* Stuff to return partition type by name rather than just numeric tag. 
   Note that there are no real sysv filesystems out there on 4Ds; but for
   some historical reason filesystem partitions used to be typed
   PTYPE_SYSV even though they contained efs filesystems. So to avoid
   user confusion, we'll just return 'em as efs. */

static struct sn_table {
	char *string;
	int num;
} pt_types[] = {
	{ "volhdr",	PTYPE_VOLHDR },
	{ "",	-1 },
	{ "",	-1 },
	{ "raw",	PTYPE_RAW },
	{ "",	-1 },
	{ "efs",	5 /* was SYSV */ },
	{ "efs",	PTYPE_EFS },
	{ "volume",	PTYPE_VOLUME },
	{ "",	-1 },
	{ "",	-1 },
	{ "xfs",	PTYPE_XFS },
	{ "xfslog",	PTYPE_XFSLOG },
	{ "xlv",	PTYPE_XLV },
	{ 0,		0 }
};



char *
pttype_to_name(register int num)
{
	register struct sn_table *sn = &pt_types[0];

	for (; sn->string; sn++)
	    if (sn->num == num)
			return(sn->string);
	return((char *)0);
}
