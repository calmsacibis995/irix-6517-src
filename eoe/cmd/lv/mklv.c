
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

#ident "$Revision: 1.18 $"

#include <sys/param.h>
#include <sys/types.h>
#include <sys/dvh.h>
#include <sys/dkio.h>
#include <sys/fcntl.h>
#include <stdio.h>
#include <ctype.h>
#include <sys/sema.h>
#include <sys/uuid.h>
#include <sys/fs/efs.h>
#include <sys/fs/xfs_types.h>
#include <sys/fs/xfs_inum.h>
#include <sys/fs/xfs_sb.h>
#include <sys/utsname.h>
#include <time.h>
#include <ustat.h>
#include <sys/lv.h>
#include "lvutils.h"

extern int errno;
struct volmember memhead = {0};
struct logvol    *loghead = NULL;
int readonly = 0;
char *progname;
int force = 0;
int raw = 0;

struct logvol *devnametovol();
char *makevolid(), *pathtolabname();
char *fname = "/etc/lvtab";

main(ac, av)
int ac;
char **av;
{
register struct logvol *vol;
register int i, j, c;
char namebuf[MAXDEVPATHLEN];
int		lvfd;
struct 		lvioarg arg;
int 		extending = 0;
int 		writerr = 0;
int 		setuperr = 0;
extern int	optind;
extern char	*optarg;

	progname = av[0];
	if (getuid() != 0)
	{
		fprintf(stderr,"must be superuser to run %s\n", progname);
		exit(-1);
	}

	/* Check for correct usage */
	if( ac < 2 ) 
	{
ucrap:
		fprintf(stderr,"usage: mklv [-f] [-l lvtabname] lvname \n");
		exit(-1);
	}

	/*
	 * Parse the arguments.
	 */

	while ((c = getopt(ac, av, "fl:")) != EOF) 
	{
		switch (c) {
		case 'f':
			force = 1;
			break;
		case 'l':
			fname = optarg;
			break;
		default: goto ucrap;
		}
	}
	ac -= optind;
	if (ac <= 0) goto ucrap;
	av = &av[optind];

	importlvtab(fname);

	vol = devnametovol(*av);
	if (!vol)
	{
	    	fprintf(stderr, "mklv: %s not found in lvtab.\n", *av);
		exit(-1);
	}

	if (!(vol->status & L_GOODTABENT))
	{
		fprintf(stderr,"mklv: bad lvtab entry for %s.\n", *av);
		exit(-1);
	}

	volmemsetup(vol);
	if (checkvolpaths(vol)) setuperr = 1;
	if (getvolheads(vol)) setuperr = 1;
	if (partcheck(vol)) setuperr = 1;
	if (setuperr)
	{
		(void)printbadvol(vol, stderr, NEW_VOLUME);
		exit(-1);
	}

	/* Now we must check to see if this is a brand-new logical volume,
	 * or extension of an existing one. We call getlvlabs to obtain
	 * any already-existing labels. 
	 *
	 * If all specified member devices already have logical volume labels
	 * we just notify the user & quit.
	 *
	 * If there are labels on a contiguous range of devices starting
	 * at the first one specified, we assume we are extending; in this
	 * case we must do additional checks to verify that the existing
	 * volume is a proper subset of the extended one now specified.
	 *
	 * If there is a mixture of missing/present labels in some less
	 * orderly pattern, something is wrong; again we notify & quit.
	 *
	 * Note: if the 'force' flag is set, these checks are skipped.
	 */

	if (force) goto writeit;

	 getlvlabs(vol);

	check_dev_connections(1);

	if (vol->status & L_GOTALL_LABELS)
	{
		fprintf(stderr,"mklv: all devices specified for %s \n",vol->tabent->devname);
		fprintf(stderr,"are already part of Logical Volume(s).\n");
		exit(-1);
	}
	if (vol->status & L_BADPARTIAL)
	{
		fprintf(stderr,"mklv: one or more devices specified for %s \n",vol->tabent->devname);
		fprintf(stderr,"are already part of a Logical Volume\n");
		exit(-1);
	}
	if ((vol->status & L_GOODPARTIAL))
	{
		extending = 1;
		vol_labels_check(vol);
		vol_devs_check(vol);

		if (vol->status !=
		    (L_GOODPARTIAL | L_GOODTABENT | L_GOODLABELS | L_DEVSCHEKD))
		{
			(void)printbadvol(vol, stderr, EXISTING_VOLUME);
			exit(-1);
		}
		if (vol_to_tab_check(vol, PARTIAL_VOL))
			exit(-1);
	}


writeit:
	if (extending)
		vol->volid = vol->vmem[0]->lvlab->volid;
	else
		vol->volid = makevolid();

	if (buildlv(vol, vol->tabent->stripe, vol->tabent->gran)) 
	{
		(void)printbadvol(vol, stderr, NEW_VOLUME);
		exit(-1);
	}
	if (extending && comparelv(vol->lv, 
				   &vol->vmem[0]->lvlab->lvdescrip, 
				   PARTIAL_VOL))
	{
		fprintf(stderr,"mklv: devices specified for %s already contain a Logical Volume\n",vol->tabent->devname);
		fprintf(stderr,"which is inconsistent with the volume specified.\n");
		exit(-1);
	}
	makelabels(vol);

	for (i = 0; i < vol->ndevs; i++)
	{
		if (writelabel(vol->vmem[i], raw))
		{
			writerr = 1;
			break;
		}
	}

	/* If a write of a label failed, we have left the overall volume in
	 * an inconsistent state. We will attempt to restore the previous
	 * state in this case.
	 */

	if (writerr)
	{
		fprintf(stderr,"mklv: attempting to restore initial state\n");
		for (j = 0; j < i; j++) restorelabel(vol->vmem[j]);
		exit(-1);
	}

	exit(lvdev_init(vol, vol->lv, 1));
}

/* makevolid(): this creates the 'system' id of the logical volume: 
 * (a concatenation of systemname & date).
 */

char *
makevolid()
{
static char buf[MAXVNAMELEN];
time_t tim;
struct utsname ut;

	uname(&ut);
	tim = time(0);
	sprintf(buf, "%s: %s", ut.sysname, ctime(&tim));
	return (buf);
}

/* partcheck: goes through all the pathnames for a volume checking for:
 * a) illegal partition type (volheader, track replacement etc).
 * b) Zero size (ie non-set-up partitions).
 * c) Mounted filesystems.
 * d) appearent superblocks on them. This check is skipped if the 'force' flag 
 *    is set. If a superblock does exist, and the user elects to go ahead, 
 *    it is wiped: this prevents later conflicts where disks could get
 *    mounted both as part of a filesystem spanning the whole volume, and,
 *    bogusly, as the old filesystem on the individual disk.
 *    BUT: with the exception: we don't wipe out any filesystem on the first 
 *    drive of a nonstriped volume, since we may be extending an fs which
 *    was formerly on a regular partition.
 */

partcheck(vol)
register struct logvol *vol;
{
register vmp vmem;
register int i, fd;
struct efs efs;
xfs_sb_t xfs;
int ptype;
struct ustat ust;

	for (i = 0; i < vol->ndevs; i++)
	{
		vmem = vol->vmem[i];
		if (partsize(vmem) == 0)
		{
			vmem->status |= WRONG_PSIZE;
			return (-1);
		}

		/* Now check for mounted filesystems */

		if (ustat(vmem->dev, &ust) == 0)
		{
			fprintf(stderr,"mklv: %s contains a mounted filesystem.\n",vmem->pathname);
			exit(-1);
		}
		ptype = partype(vmem);
		switch (ptype) 
		{
		case PTYPE_VOLHDR:
		case PTYPE_VOLUME:

			{
				vmem->status |= ILLEGAL_PTYPE;
				return (-1);
			}
		 
		case PTYPE_LVOL: 
		
		/* It is not unreasonable for the first member in a volume
		 * to already have a superblock on it: we may be extending
		 * an existing volume. So skip the check in this case.
		 */

			if (i == 0) continue;

		default:

			if (force) continue;

			if ((fd = open(vmem->pathname, O_RDWR)) < 0)
			{
				vmem->status |= CANT_ACCESS;
				return (-1);
			}
			(void) lseek(fd, EFS_SUPERBB * DEV_BSIZE, 0);  
			if (read(fd, &efs, sizeof (efs)) != sizeof (efs))
			{
				vmem->status |= CANT_ACCESS;
				close(fd);
				return (-1);
			}
			(void) lseek(fd, XFS_SB_DADDR * DEV_BSIZE, 0);
			if (read(fd, &xfs, sizeof (xfs)) != sizeof (xfs))
			{
				vmem->status |= CANT_ACCESS;
				close(fd);
				return (-1);
			}
			if (IS_EFS_MAGIC(efs.fs_magic) ||
			    xfs.sb_magicnum == XFS_SB_MAGIC)
			{
				printf("mklv: %s appears to contain a filesystem.\n",vmem->pathname);

		/* If the volume isn't striped, we should NOT wipe any
		 * filesystem on the FIRST disk since we may be extending
		 * a filesystem which was formerly on a regular partition. */

				if ((i == 0) && (vol->tabent->stripe == 1))
				{
					if (reply("OK"))
						exit(0);
					else
					{
						close (fd);
						continue;
					}
				}

				printf("This will be wiped out if we proceed. ");
				if (reply("OK")) 
				{
					exit(0);
				}
			}
			bzero((char *)&efs, sizeof(efs));
			(void) lseek(fd, EFS_SUPERBB * DEV_BSIZE, 0);  
			if (write(fd, &efs, sizeof (efs)) != sizeof (efs))
			{
				vmem->status |= CANT_ACCESS;
				close(fd);
				return (-1);
			}
			bzero((char *)&xfs, sizeof(xfs));
			(void) lseek(fd, XFS_SB_DADDR * DEV_BSIZE, 0);  
			if (write(fd, &xfs, sizeof (xfs)) != sizeof (xfs))
			{
				vmem->status |= CANT_ACCESS;
				close(fd);
				return (-1);
			}
			close(fd);
		}
	}
	return (0);
}


/*
 * Restorelabel(): attempt to restore the previous state. If there was a
 * previous label, put that back.
 * If there wasn't, wipe out any label and restore the PTYPE to what it was.
 */

restorelabel(vmem)
register struct volmember *vmem;
{
	register char *labelname;
	register struct volume_header *vh;
	register int i;
	register int empty;
	register int bytes;
	register int vfd;
	register int dev_bsize;
	daddr_t lbn;
	unsigned offset;
	register int part;
	struct partition_table *pt;

	if (!vmem->vhinfo)
	{
		if (getdvh(vmem) < 0) return (-1);
	}
	else if ((vfd = membfd(vmem)) < 0) return (-1);
	
	vh = &vmem->vhinfo->vh;
	dev_bsize = vmem->vhinfo->dev_bsize;

	/* delete first */

	labelname = pathtolabname(vmem->pathname);
	(void)vd_delete(labelname, vh);
	sortdir(vh->vh_vd); /* keep in sorted order just for appearances */

	if (!vmem->lvlab)
	{
		vh->vh_pt[pathtopart(vmem->pathname)].pt_type = vmem->ptype;
		goto pushvh;
	}

	/* Reallocate a directory slot. */

	empty = -1;
	for (i = 0; i < NVDIR; i++) 
	{
		if (empty < 0 && vh->vh_vd[i].vd_nbytes == 0)
		{
			empty = i;
			break;
		}
	}
	if (empty == -1) {
		fprintf(stderr,"%s: volume header directory full\n",progname);
		return (-1);
	}

	bytes = roundblk(vmem->labsize, dev_bsize); /* round up to blks */

	if((lbn = vh_contig(vmem, bytes)) == -1) 
	{
		fprintf(stderr,"%s: can't restore label for %s\n", 
			progname, labelname);
		return(-1);	/* shouldn't ever happen here... */
	}

	offset = lbn * dev_bsize;
	lseek(vfd, offset, 0);
	if (write(vfd, (caddr_t)vmem->lvlab, bytes) != bytes)
	{
		fprintf(stderr,"%s: can't restore label for %s\n", 
			progname, vmem->pathname);
		return (-1);
	}

	strcpy(vh->vh_vd[empty].vd_name, labelname);
	vh->vh_vd[empty].vd_nbytes = bytes;
	vh->vh_vd[empty].vd_lbn = lbn;
	sortdir(vh->vh_vd); /* keep in sorted order just for appearances,
		and in case any older programs counted on it. */

pushvh:
	if (putdvh(vmem) < 0) return (-1);
	return(0);
}
