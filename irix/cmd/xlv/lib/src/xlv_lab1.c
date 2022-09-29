/**************************************************************************
 *                                                                        *
 *             Copyright (C) 1993-1994, Silicon Graphics, Inc.            *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#ident "$Revision: 1.44 $"

/* 
 * The xlv_lab1 routines presents the abstraction of complete
 * labels.  The write routine will update the master block
 * after writing the label (synchronously) and will read the
 * master block to determine which label to read.
 *
 * These routines own the sequence numbers contained in the
 * master block and the header of each label.
 */

#include <errno.h>
#include <sys/syssgi.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/debug.h>
#include <fcntl.h>
#include <unistd.h>
#include <diskinvent.h>
#include <sys/dkio.h>
#include <sys/uuid.h>
#include <stdlib.h>
#include <malloc.h>
#include <bstring.h>
#include <string.h>
#include <sys/xlv_base.h>
#include <sys/xlv_tab.h>
#include <xlv_oref.h>
#include <sys/xlv_vh.h>
#include <xlv_lab.h>
#include <xlv_utils.h>
#include <pathnames.h>
#include <diskinfo.h>
#include <sys/conf.h>
#include <xlv_cap.h>

void	vd_delete(char *dvh_file, struct volume_header *vh);
void	sortdir(struct volume_directory *v);
int	vh_contig(xlv_vh_entry_t *vh_entry, int size);
int	bytoblks(int cnt, int dev_bsize);
static void    xlv_lab1_dev_fixup(char *, xlv_vh_entry_t *);

struct partition_table *getvhpart(struct volume_header *vh);

int xlv_do_labsync;	/* Not Used but leave since xlv_labd(1m) uses */


#define XLV_LAB_NAME "xlvlab"

/* This is a hack to keep the xlv dev_t's consistent with the hardwaregraph.
 * Pre-hardwaregraph/lego dev_t's never changed, so xlv used them as
 * 'anchors' and fed them from the disk label directly into the kernel.  
 *
 * With the hardware graph, the current dev_t for a given partition is
 * unknown until kernel boot time, so this scheme doesn't work.
 *
 * Here we fill in the dev_t's for each partition entry directly so that
 * XLV doesn't try to send bogus device handles to the kernel.
 */

static void
xlv_lab1_dev_fixup(char *vhpath, xlv_vh_entry_t *vh_entry)
{
	int		   which_part;
	struct stat64	   sbuf;
	char		   devpath[MAXDEVNAME];
	char		   partpath[MAXDEVNAME];
	int		   len;
	xlv_vh_disk_part_t *dp0;
	xlv_vh_disk_part_t *dp1;
	int		   null_dp0;
	int		   null_dp1;
	unsigned	   st;

	/* guaranteed to be a long style hwgraph name */
	strcpy(devpath, vhpath);

	/* stat each partition and store the dev_t in the xlv labels */
	for (which_part = 0; which_part < NPARTAB; which_part++) {

		dp0 = &(vh_entry->label0_p->label_entry[which_part].disk_part);
		dp1 = &(vh_entry->label1_p->label_entry[which_part].disk_part);

		null_dp0 = uuid_is_nil(&(dp0->uuid), &st);
		null_dp1 = uuid_is_nil(&(dp1->uuid), &st);
		if (null_dp0 && null_dp1) {
			continue;
		}

		/* get the block device dev_t for this partition */
		len = MAXDEVNAME;
		if (path_to_partpath(devpath, which_part, S_IFBLK,
				      partpath, &len, &sbuf) == NULL) {
			/* failure, zap the dev_t's - make entry invalid */
#ifdef DEBUG
			fprintf(stderr,
		"DBG: %s: Cannot map label entry %d to partition path.\n",
				pathtoalias(vhpath), which_part);
#endif
			dp0->dev = 0;
			dp1->dev = 0;
			continue;
		}

		/* update the labels with entries */
		if (!null_dp0)
			 dp0->dev = sbuf.st_rdev;
		if (!null_dp1)
			 dp1->dev = sbuf.st_rdev;
	}
}



/*
 * Open and read in the volume header given a device pathname
 * to the volume header - e.g., /dev/rdsk/dks0d6vh. Returns 0
 * on success.
 *
 * The volume header is read into vh_entry. 
 *
 * If the caller has only a device name to the partition, then
 * pathtovhpath() can be used to generate the path to the vh.
 * Keep the file descriptor for the volume header open.
 */
int
xlv_lab1_open (char		   *vh_pathname,
	       xlv_tab_disk_part_t *dp,
	       xlv_vh_entry_t	   *vh_entry)
{
	int		i, label_size, offset;
	char		*label;
	int		dev_bsize;
	struct volume_header *vh;
	int		vfd;

	if ((vh_entry->vfd = open(vh_pathname, O_RDWR | O_SYNC)) == -1) {
		vertex_hdl_t	p_vhdl, s_vhdl;
		char		dev_name[MAXDEVNAME];
		int		dev_len;
		int		o_errno;

	    	if (!dp) { return(-1); }	/* no failover paths */
		p_vhdl = dp->dev[0];		/* dp->dev[dp->active_path] */
		o_errno = oserror();
#ifdef DEBUG
		fprintf(stderr, "DBG: Cannot open %s(%d): %s\n",
			vh_pathname, p_vhdl, strerror(o_errno));
#endif
		while ((vh_entry->vfd < 0)
		    && (cap_vht_syssgi(SGI_FO_SWITCH, p_vhdl, &s_vhdl) != -1)
		    && (s_vhdl != dp->dev[0]) /* wrapping */ ) {
			/*
			 * An alternate path exists.
			 */
			dev_len = MAXDEVNAME;
			dev_to_devname(s_vhdl, dev_name, &dev_len);
			vh_pathname = dev_name;
			p_vhdl = s_vhdl;
#ifdef DEBUG
			fprintf(stderr, "DBG:    switch --> %s(%d)\n",
				dev_name, s_vhdl);
#endif
			vh_entry->vfd = open(vh_pathname, O_RDWR|O_SYNC);
		}

		if (vh_entry->vfd < 0) {
			setoserror(o_errno);
			return(-1);
		} else {
	    		/* found a good path -- update active path */
			dp->dev[0] = p_vhdl;
			vh_pathname = dev_name;
		}

	}  /* opened volume header */

	strncpy (vh_entry->vh_pathname, vh_pathname, MAXDEVPATHLEN);

	if (dp) {
		/*XXXjleong  Why save the disk part dev_t as the vh's? */
		vh_entry->dev = DP_DEV(*dp);
	} else {
		struct stat sbuf;
		if (-1 == stat(vh_pathname, &sbuf)) {
			return -1; /* couldn't get dev_t */
		} else {
			vh_entry->dev = sbuf.st_rdev;
		}
	}

	vfd = vh_entry->vfd;
	vh = &(vh_entry->vh);

	if (getdiskheader (vfd, vh) == -1) {
		close(vfd);
		return (-1);
	}

	dev_bsize = vh_entry->vh.vh_dp.dp_secbytes;
	if (dev_bsize == 0)     /* a lot of older disks had their
	                           secbytes set to 0 */
		dev_bsize = DEV_BSIZE;
	vh_entry->dev_bsize = dev_bsize;

	/*
	 * Check the directory to see if the disk has an XLV label.
	 *
	 * XXX Check the version number of the XLV volume header label.
	 * When the version number, thus label, changes, this is the
	 * place to do label conversion.
	 */

	for (i = 0; i < NVDIR; i++) {
		/*
		 * Search the volume directory for the xlv label file.
		 */
	        if (strcmp (vh->vh_vd[i].vd_name, XLV_LAB_NAME) == 0) {
			/*
			 * Found the xlv label file.
			 */
	                vh_entry->label_lbn = vh->vh_vd[i].vd_lbn;
			label_size = roundblk(sizeof(xlv_vh_master_t),
				dev_bsize) + 2*roundblk(
				sizeof(xlv_vh_disk_label_t), dev_bsize);

			label = (char *) malloc (label_size);
			offset = vh_entry->label_lbn * dev_bsize;

			if (lseek (vfd, offset, SEEK_SET) == -1) {
				close (vfd);
				free (label);
				return (-1);
			}

		        if (read (vfd, label, label_size) < label_size) {
	            	    close (vfd);
	    		    free (label);
			    return (-1);
			}

		        vh_entry->xlv_label = (xlv_vh_master_t *)label;
		        label += roundblk(sizeof(xlv_vh_master_t), dev_bsize);
		        vh_entry->label0_p = (xlv_vh_disk_label_t *)label;
	                label += roundblk(sizeof(xlv_vh_disk_label_t),
				          dev_bsize);
		        vh_entry->label1_p = (xlv_vh_disk_label_t *)label;

			/* get the real dev_t's for each partition */
			xlv_lab1_dev_fixup(vh_pathname, vh_entry);

			break;
		} /* search for label file */
	} /* search of volume directory */

	return (0);
} /* end of xlv_lab1_open() */


/*
 * Close the file descriptor associated with this
 * volume header and frees the vh_entry (and the
 * incore xlv label, if any).
 */

void
xlv_lab1_close (xlv_vh_entry_t **vh_entry_p)
{
	xlv_vh_entry_t	*vh_entry = *vh_entry_p;

	if (vh_entry->xlv_label != NULL) {
		free (vh_entry->xlv_label);
		vh_entry->xlv_label = NULL;
	}
	if (vh_entry->vfd >= 0)
		close (vh_entry->vfd);
	free (vh_entry);
	*vh_entry_p = NULL;
}



/*
 * The following routines are designed to allow us to cache file
 * descriptors.  In general, the xlv_lab1 routines assume that the 
 * file descriptors are kept open between xlv_lab1_open() and 
 * xlv_lab1_close().  However, there are cases where we need to
 * open multiple logical volumes and hence physical disks. 
 * Since the max number of open fd's is limited, the caller may
 * close some file descriptors in the interim. The following two
 * routines are used to close and reopen file descriptors.
 */

/*
 * Reclaim a file descriptor leaving the vh_entry intact.
 */
void
xlv_lab1_reclaim_fd (xlv_vh_entry_t *vh_entry)
{
	ASSERT (vh_entry->vfd != -1);

	close (vh_entry->vfd);
	vh_entry->vfd = -1;
}

/*
 * Reopen a file descriptor that has been reclaimed.
 */
int
xlv_lab1_restore_fd (xlv_vh_entry_t   *vh_entry)
{
	ASSERT (vh_entry->vfd == -1);

        if ((vh_entry->vfd = open(vh_entry->vh_pathname, O_RDWR | O_SYNC)) < 0) 
                return (-1);

	return (0);
}

/*
 * Allocates xlv_lab0, xlv_lab1, xlv_master in the header
 * partition.
 * Initializes the xlv master block with sequence number 0.
 *
 * Assumes all contiguously laid out:
 * Directory entry: xlv_lab -> one file containing 3 parts.
 * Assumes that header already read in.
 */
int
xlv_lab1_init (xlv_vh_entry_t *vh_entry)
{
	int			label_size;
	int			i, empty;
	char			*label;
	struct volume_header    *vh;
	int                 	dev_bsize;
	int			lbn;
	off_t			offset;

	vh = &(vh_entry->vh);
	dev_bsize = vh_entry->dev_bsize;

	label_size = roundblk(sizeof(xlv_vh_master_t), dev_bsize) +
		2*roundblk(sizeof(xlv_vh_disk_label_t), dev_bsize);

	/* Delete first in case we are replacing. */
	(void) vd_delete(XLV_LAB_NAME, vh);

	/* Some older versions of dvhtool appear to leave bogus 
	 * zero-length directory entries when asked to delete an entry. 
	 * We'll do a cleaning pass here to force any such semi-allocated 
	 * entries to be properly clear!
	 */

	for (i = 0; i < NVDIR; i++)
	{
	        if (vh->vh_vd[i].vd_nbytes == 0)
	        {
			vh->vh_vd[i].vd_lbn = -1;
			vh->vh_vd[i].vd_name[0] = '\0';

	        }
	}
	sortdir(vh->vh_vd); /* keep in sorted order because vh_contig
			       counts on it. */

	/* Now allocate a directory slot. */

	for (i = 0, empty = -1; i < NVDIR; i++)
	{
	        if (vh->vh_vd[i].vd_nbytes == 0)
	        {
			empty = i;
			break;
	        }
	}
	if (empty == -1) {
		fprintf(stderr,
		"Error: Can't write label, disk header directory full\n");
		return (-1);
	}

	/* Since XLV labels are small, we should not have any problem
	 * finding contiguous space for them; we don't need to go through
	 * all the excruciating copy-off/copy-on stuff that dvhtool has to
	 * do in order to handle large objects...
	 */

	if ((lbn = vh_contig(vh_entry, label_size)) == -1)
	{
		fprintf(stderr,
			"Can't write label, disk header partition full\n");
		return(-1);
	}

	offset = lbn * dev_bsize;
	lseek(vh_entry->vfd, offset, SEEK_SET);

	label = (char *) malloc (label_size);
	bzero (label, label_size);

	if (write (vh_entry->vfd, label, label_size) != label_size)
	{
	        free (label);
	        fprintf(stderr,"Error: write of label failed\n");
	        return (-1);
	}

	strcpy (vh->vh_vd[empty].vd_name, XLV_LAB_NAME);
	vh->vh_vd[empty].vd_nbytes = label_size;
	vh->vh_vd[empty].vd_lbn = lbn;
	sortdir (vh->vh_vd); /* keep in sorted order just for appearances,
	        and in case any older programs counted on it. */

	if (putdiskheader (vh_entry->vfd, vh, 0) < 0) {
		free (label);
		return (-1);
	}

	/* Save the label and set up pointers to the primary and secondary
	   copies in memory. */
	vh_entry->label_lbn = lbn;
	vh_entry->xlv_label = (xlv_vh_master_t *)label;
	label += roundblk (sizeof(xlv_vh_master_t), dev_bsize);
	vh_entry->label1_p = vh_entry->label0_p = (xlv_vh_disk_label_t *)label;
	vh_entry->label1_p++;

	return(0);
} /* end of xlv_lab1_init() */


/*
 * Write the in-core master block to disk.
 */
int
xlv_lab1_write_master_blk (xlv_vh_entry_t *vh_entry)
{
	int 	offset, dev_bsize;

	offset = vh_entry->label_lbn * vh_entry->dev_bsize;
	dev_bsize = vh_entry->dev_bsize;

	if (lseek(vh_entry->vfd, offset, SEEK_SET) == -1)
		return (-1);

	/*
	 * Writes must be multiple of device block size.  We know that
	 * the master block is <= 1 sector
	 */
	ASSERT (sizeof(xlv_vh_master_t) <= dev_bsize);

	if (write(vh_entry->vfd, vh_entry->xlv_label, dev_bsize) < dev_bsize) {
		errno = EIO;
		return (-1);
	}
	return (0);
}

/*
 * Writes a label. This will write the label into the space
 * taken by the secondary label.
 * This does not update the master block.  That needs to be
 * done as a separate operation.
 */
int
xlv_lab1_write (xlv_vh_entry_t *vh_entry)
{
	xlv_vh_disk_label_t	*label;
	int			lab_size, offset;
	int			retval;
	int                     which_part;
	xlv_vh_disk_part_t      *dp;
	unsigned		st;
	dev_t			realdev[XLV_MAX_DISK_PARTS];

	label = XLV__LAB1_SECONDARY (vh_entry);

	if (vh_entry->xlv_label->vh_seq_number & 0x1) {

		/* 
		 * Current primary label is at offset 1, hence secondary
		 * is offset 0
		 */
		offset = vh_entry->label_lbn * vh_entry->dev_bsize +
		roundblk(sizeof(xlv_vh_master_t), vh_entry->dev_bsize);
	}
	else {
		offset = vh_entry->label_lbn * vh_entry->dev_bsize +
	    	    roundblk(sizeof(xlv_vh_master_t), vh_entry->dev_bsize) +
	    	    roundblk(sizeof(xlv_vh_disk_label_t), vh_entry->dev_bsize);
	}
	if (lseek(vh_entry->vfd, offset, SEEK_SET) == -1)
		return (-1);

	/* XXXsbarr
	 * compatability hack for 5.3 and 6.2 labels
	 * force partition number in lower 4 bits, set an extra higher bit
         * so that partition 0 doesn't show up as dev_t 0.
         * 5.3 and 6.2 will assume the disk migrated and fill in the
         * correct high bits
	 *
	 * After updating the label on disk, the dev_t's in the
	 * in-core label must be restored to their orginal (real) values.
         */
	for (which_part = 0; which_part < XLV_MAX_DISK_PARTS; which_part++) {
		dp = &label->label_entry[which_part].disk_part;
		realdev[which_part] = dp->dev;
		if (!uuid_is_nil(&(dp->uuid), &st)) {
			dp->dev = (which_part | 0x10);
		}
	}

	/*
	 * Needs to be multiple of sector size. Note that we write out
	 * the entire label (containing 16 disk part entries). This is
	 * required because disk parts entries are sparse.
	 */
	lab_size = roundblk(sizeof(*label), vh_entry->dev_bsize);

	retval = (int)write(vh_entry->vfd, label, lab_size);

	/*
	 * Fix 461915:  Now that the label has been written to disk,
	 * restore the real dev_t's to the in-core label.
	 */
	for (which_part = 0; which_part < XLV_MAX_DISK_PARTS; which_part++) {
		label->label_entry[which_part].
		    disk_part.dev = realdev[which_part];
	}

	if (retval < lab_size) {
		/* XXX should write be in a loop? */
		errno = EIO;
		return (-1);
	}
	return (0);
}


/*
 * Delete the XLV label from the disk volume header.
 * 
 * Note: The file descriptor to the volume header must be valid.
 */
int
xlv_lab1_delete_label(xlv_vh_entry_t *vh_entry)
{
	struct volume_header	*header;

	ASSERT(vh_entry->vfd != -1);

	header = &(vh_entry->vh);

	(void) vd_delete(XLV_LAB_NAME, header);

	if (putdiskheader (vh_entry->vfd, header, 0) < 0) {
		return (-1);
	}

	return (0);
}

/*
 * Update partition type in volume header
 *
 * Note: <ptype_update_list> should be an array. The terminating
 * entry (the first invalid entry) must have its "num_partitions"
 * field set to zero.
 */
int
xlv_lab1_update_ptype (xlv_ptype_update_list_t *ptype_update_list)
{
	int	i, j, vh_fd;
	dev_t	partition;
	int	partition_type, num_partitions;
	struct volume_header	vh;
	char	*vh_pathname;

	/*
	 * For all the disks in the list
	 */
	for (i = 0;
	     i < XLV_MAX_DISKS && ptype_update_list[i].num_partitions > 0;
	     i++) {

		/*
		 * Open the volume header
		 */
		vh_pathname = ptype_update_list[i].vh_pathname;
		if ((vh_fd = open(vh_pathname, O_RDWR | O_SYNC)) < 0) {
			fprintf(stderr, "Failed to open %s: %s\n",
				pathtoalias(vh_pathname), strerror(oserror()));
			return (-1);
		}
		if (getdiskheader (vh_fd, &vh) == -1) {
			fprintf(stderr,
				"Failed to get disk header from %s: %s\n",
				pathtoalias(vh_pathname), strerror(oserror()));
			close(vh_fd);
			return (-1);
		}

		/*
		 * And update the partition type for 
		 * all the flagged partitions on that disk
		 */
		num_partitions = ptype_update_list[i].num_partitions;
		for (j=0; j < num_partitions; j++) {
			partition = ptype_update_list[i].partition[j];
			partition_type = ptype_update_list[i].partition_type[j];
			vh.vh_pt[partition].pt_type = partition_type;
		}
		if (putdiskheader (vh_fd, &vh, NO) == -1) {
			fprintf(stderr, "Failed to put disk header to %s: %s\n",
				pathtoalias(vh_pathname), strerror(oserror()));
			close(vh_fd);
			return (-1);
		}
		close(vh_fd);
	}

	return (0);
} /* end of xlv_lab1_update_ptype() */

/* Routines that should go into libdisk.a */

/*
 * delete a volume directory entry from the given header.
 */
void
vd_delete(char *dvh_file, struct volume_header *vh)
{
	register int i;

	for (i = 0; i < NVDIR; i++) {
		if (vh->vh_vd[i].vd_nbytes &&
	            (strcmp(vh->vh_vd[i].vd_name, dvh_file) == 0)) {
	        	vh->vh_vd[i].vd_nbytes = 0;
	        	vh->vh_vd[i].vd_lbn = -1;
	        	*vh->vh_vd[i].vd_name = '\0';
	        	sortdir(vh->vh_vd); /* keep in sorted order just for
	                                     * appearances, and in case any
	                                     * older programs counted on it.
	                                     */
	        	break;
	        }
	}
}

/*
 * Sort the volume directory on lbn.  Zero and negative #'s sort to
 * highest position.  These are invalid lbn values, and indicate an 
 * unused slot or a partially completed entry.
 */

int
lbncmp (struct volume_directory *v0, struct volume_directory *v1)
{
        if(v0->vd_lbn <= 0)
                return 1;
        else if(v1->vd_lbn <= 0)
                return -1;
        return v0->vd_lbn - v1->vd_lbn;
}

void
sortdir(struct volume_directory *v)
{
        qsort((void *)v, (size_t)NVDIR, sizeof(v[0]), 
		(int (*)(const void *, const void *))lbncmp);
}


/*
 * Determine if there is a contiguous chunk big enough in the volume
 * header.
 * Return its lbn if there is, otherwise return -1.
 * Note it is assumed that:
 *      a) the volume header info is in core, and
 *      b) the volume directory is sorted by blockno.
 */

/*
 * The first block that we use to write files is 2.  This matches
 * the value in fx and dvhtool.
 */
#define FIRSTFILEBLK 2

int
vh_contig(xlv_vh_entry_t *vh_entry, int size)
{
        register struct volume_directory *v, *vd0, *vd1;
        register struct volume_header *vh;
        register struct partition_table *vhpt;
        register int reqblks, fileblks, dev_bsize, filestart;

        vh = &(vh_entry->vh);
        dev_bsize = vh_entry->dev_bsize;

        reqblks = bytoblks(size, dev_bsize);
        v = vh->vh_vd;
        if(v->vd_lbn <= 0)      /* empty directory: legal tho unlikely! */
                return (FIRSTFILEBLK);
        for(vd0=v, vd1=vd0+1; vd1->vd_lbn > 0 && vd1 < &v[NVDIR]; vd1++) {
                fileblks = bytoblks(vd0->vd_nbytes, dev_bsize);
                filestart = vd0->vd_lbn;
                if (filestart < FIRSTFILEBLK) filestart = FIRSTFILEBLK;
                if((filestart + fileblks + reqblks) <= vd1->vd_lbn)
                        return (filestart + fileblks);
                vd0 = vd1;
        }

        /* check for enough between space between last entry and end of
           partition */

        vhpt = getvhpart(vh);
        if (!vhpt)
                return (-1);
        fileblks = bytoblks(vd0->vd_nbytes, dev_bsize);
        if(vhpt->pt_nblks >= (vd0->vd_lbn + fileblks + reqblks))
                return (vd0->vd_lbn + fileblks);
        else
                return (-1);
}



/* round up to dev_bsize multiple */
int
roundblk (int	cnt,
	  int	dev_bsize)
{
        cnt = (cnt + dev_bsize -1) / dev_bsize;
        cnt *= dev_bsize;
        return cnt;
}


/* return # of dev_bsize blox needed to hold cnt */

int
bytoblks (int	cnt,
	  int	dev_bsize)
{
        return (roundblk(cnt, dev_bsize) / dev_bsize);
}

/*
 * Figure out which partition is the volume header.
 */

struct partition_table *
getvhpart(struct volume_header *vh)
{
        struct partition_table *vhpt = NULL, *pt;
        register int i;

        pt = vh->vh_pt;
        for (i = 0; i < NPARTAB; i++, pt++) {
                if(!vhpt && pt->pt_nblks && pt->pt_type == PTYPE_VOLHDR) {
                        vhpt = pt;
                        break;
                }
        }
        if(!vhpt)
        	fprintf(stderr,
			"No entry for volume header in partition table\n");
        return vhpt;
}

