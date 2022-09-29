/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1994, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#ident "$Revision: 1.42 $"

#include <sys/types.h>
#include <sys/stat.h>
#include <bstring.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <sys/debug.h>
#include <sys/uuid.h>
#include <sys/xlv_base.h>
#include <sys/xlv_tab.h>
#include <diskinvent.h>
#include <sys/syssgi.h>
#include <sys/sysmacros.h>
#include <sys/xlv_attr.h>
#include <sys/dkio.h>
#include <sys/errno.h>
#include <xlv_utils.h>
#include <pathnames.h>
#include <xlv_cap.h>

static void _remove_vol_from_tab (xlv_tab_vol_t *tab_vol, int vol_idx);

/*
 * Miscellaneous xlv utility routines.
 */


/*
 * Open a subvolume given the device number
 */
int
xlv_subvol_open_by_dev (
        dev_t           dev,
        char            *subvol_name,
	char		*tmpdir,	/* NULL if using standard dir */
        int             flags)
{
	char            tmp_name[100];
	int		fd;

	sprintf(tmp_name, "%s/.xlv_%s_%dXXXXXX",
		(tmpdir == NULL) ? "/tmp" : tmpdir, subvol_name, dev);

        if (mktemp (tmp_name) == NULL)
		return (-1);

        if (cap_mknod (tmp_name, S_IFCHR, dev))
                return (-1);

        fd = open (tmp_name, flags);	/* This may fail if dev isnt valid */

        if (unlink (tmp_name))
                return (-1);

        return fd;
}


#if  (0)

/* The following function is unused since it cannot handle remote volumes
   of the form <machine.vol.log>. However, it is left undeleted since
   it can be a useful example */
/*
 * Open a subvolume given a name of the form "vol_foo.log".
 */
int
xlv_subvol_open (
	char		*subvol_name,
	int		flags)
{
	char		tmp_name[100], vol_name[50], c;
	unsigned	subvol_type, len;
	dev_t		dev;
	int		fd;
	xlv_getdev_t	getdev;


	/*
	 * Construct the volume's block device path,
	 * /dev/dsk/xlv/...
	 */
	strcpy (vol_name, XLV_DEV_RAWPATH);
	strcat (vol_name, subvol_name);
	len = strlen(vol_name);

	/*
	 * Strip off the suffix and record the type.
	 *
	 * The suffix could be one of ".log", ".data", and ".rt".
	 */
	c = vol_name [len-3];
	if (c == 'l') {
		len -= 4;
		subvol_type = XLV_SUBVOL_TYPE_LOG;
	}
	else if (c == 'a') {
		len -= 5;
		subvol_type = XLV_SUBVOL_TYPE_DATA;
	}
	else if (c == '_') {
		len -= 3;
		subvol_type = XLV_SUBVOL_TYPE_RT;
	}
	else
		return -1;

	vol_name[len] = '\0';

	/*
	 * Open the volume with the specified flags.
	 */
	if ((fd = open (vol_name, O_RDONLY)) == -1)
		return -1;

	getdev.version = 1;
	if (ioctl (fd, DIOCGETXLVDEV, &getdev) == -1) {
		close (fd);
		return -1;
	}
	close (fd);

	if (subvol_type == XLV_SUBVOL_TYPE_LOG)
		dev = getdev.log_subvol_dev;
	else if (subvol_type == XLV_SUBVOL_TYPE_DATA)
		dev = getdev.data_subvol_dev;
	else
		dev = getdev.rt_subvol_dev;

	strcpy (tmp_name, "/tmp/");
	strcat (tmp_name, subvol_name);
	strcat (tmp_name, "XXXXXX");
	mktemp (tmp_name);
	if (cap_mknod (tmp_name, S_IFCHR, dev))
		return -1;
	fd = open (tmp_name, flags);

	if (unlink (tmp_name))
		return -1;

	return fd;
}

#endif

/*
 * Add a volume element to a plex. The plex structure is grown if
 * it cannot accommodate another volume element. The new volume
 * element is initialize to the "empty" state so it can be updated.
 *
 * If "ve_idx" is < 0 then the caller wants this function to calculate
 * the insertion point. This would be the case if there is only one
 * and there is no need to check for volume element size across rows.  *
 * Note: This assumes that the given volume element has been check
 * that it is the same size as other volume elements in the same
 * row in different plexes.
 *
 * Returns non-zero upon failure.
 */
#define	_E_VE_INVALID_IDX	-1	/* "ve_idx" is a invalid */
#define	_E_VE_OVERLAP		-2	/* overlaps with neighboring ve's */
#define	_E_VE_NO_MEMORY		-3	/* couldn't grow plex */
int
xlv_add_ve_to_plex(
	xlv_tab_plex_t		**plexpp,
	xlv_tab_vol_elmnt_t	*vep,
	int			ve_idx)		/* where to insert */
{
	xlv_tab_plex_t		*pp;
	xlv_tab_vol_elmnt_t	*pvep;
	int			count;
	int			last;

	pp = *plexpp;
	last = pp->num_vol_elmnts - 1;

	if (ve_idx < 0) {
		/* Calculate insertion point */
		for (ve_idx = 0; ve_idx < pp->num_vol_elmnts; ve_idx++) {
			pvep = &pp->vol_elmnts[ve_idx];
			if (vep->start_block_no < pvep->start_block_no)
				break;		/* found correct index */
		}

	} else {
		if (ve_idx > pp->num_vol_elmnts) {
			ASSERT(0);
			return(_E_VE_INVALID_IDX);
		}
		pvep = &pp->vol_elmnts[ve_idx];		/* insertion point */
	}

	/*
	 * Check for overlaps with the surrounding volume elements.
	 */
	if (((ve_idx > 0) && ((pvep-1)->end_block_no > vep->start_block_no)) ||
	    ((ve_idx <= last) && (vep->end_block_no > pvep->start_block_no))) {
		/*
		 * overlapping ve's
		 */
		return(_E_VE_OVERLAP);
	}

	if (pp->max_vol_elmnts == pp->num_vol_elmnts) {
		/*
		 * Grow the plex structure
		 */
		xlv_tab_plex_t	*new_pp;
		size_t		old_size, new_size;

		old_size = sizeof(xlv_tab_plex_t) +
			((pp->max_vol_elmnts-1) * sizeof(xlv_tab_vol_elmnt_t)); 
		new_size = old_size + (5 * sizeof(xlv_tab_vol_elmnt_t));
		new_pp = (xlv_tab_plex_t *)malloc(new_size);
		if (new_pp == 0)
			return(_E_VE_NO_MEMORY);
		bcopy(pp, new_pp, (int)old_size); 
		new_pp->max_vol_elmnts = pp->max_vol_elmnts + 5;
		free(pp);
		*plexpp = pp = new_pp;
	}

	/*
	 * insert the volume element
	 */
	count = (last - ve_idx + 1) * (int)sizeof(xlv_tab_vol_elmnt_t);
	if (count) {
		/*
		 * Move all trailing volume elements down the array.
		 */
		bcopy ( &pp->vol_elmnts[ve_idx],
			&pp->vol_elmnts[ve_idx+1],
			count);
	}
	bcopy(vep, &pp->vol_elmnts[ve_idx], sizeof(xlv_tab_vol_elmnt_t));
	if (vep->state != XLV_VE_STATE_INCOMPLETE) {
		/*
		 * Never change the state of an incomplete volume element.
		 */
		pp->vol_elmnts[ve_idx].state = XLV_VE_STATE_EMPTY;
	}
	pp->vol_elmnts[ve_idx].veu_name[0] = '\0';
	pp->num_vol_elmnts++;		/* one more volume element */

	return(0);

} /* end of xlv_add_ve_to_plex() */


/*
 * Find the given volume element in the plex and extract it.
 * Shrink the plex by decrementing the volume element count.
 *
 * Note: Watch out for removing the only volume element of a plex.
 */
int
xlv_rem_ve_from_plex(xlv_tab_plex_t *plexp, int ve_idx)
{
	int last, count;

	ASSERT(plexp->num_vol_elmnts > 1);

	if (ve_idx >= plexp->num_vol_elmnts) {
		ASSERT(0);
		return(-1);
	}

	last = plexp->num_vol_elmnts - 1;
	count = (last - ve_idx) * (int)sizeof(xlv_tab_vol_elmnt_t);

	if (count) {
		/*
		 * Move all trailing volume elements up the array.
		 */
		bcopy ( &plexp->vol_elmnts[ve_idx + 1],
			&plexp->vol_elmnts[ve_idx],
			count);
		/*
		 * zero out last ve
		 */
		bzero (&plexp->vol_elmnts[last], sizeof(xlv_tab_vol_elmnt_t));
	}
	plexp->num_vol_elmnts--;                /* one less volume element */

	return (0);

} /* end of xlv_rem_ve_from_plex() */


/*
 * Fills in the subvolume table of ve's. Each entry (row) corresponds
 * to an address. The index into each plex's ve array that covers that
 * address is stored in the entry. If the plex does not have a ve entry
 * for the address, an index of -1 is used.
 *
 * Return the number of entries (depth of filled in table).
 */
int
xlv_fill_subvol_ve_table(
	xlv_tab_subvol_t *svp,
	ve_table_entry_t *table,
	int		 max_entries)
{
	xlv_tab_plex_t	*plex;
	int		p;

	int		rows;
	int		ve_idx;
	int		tab_idx;
	__int64_t	ve_start_blkno;
	__int64_t	tab_start_blkno;

	/*
	 * First initialize the table.
	 */
	for (tab_idx = 0; tab_idx < max_entries; tab_idx++) {
		table[tab_idx].start_blkno = -1;
		table[tab_idx].end_blkno = -1;
		for (p = 0; p < XLV_MAX_PLEXES; p++)
			table[tab_idx].plex_ve_idx[p] = -1;
	}

	/*
	 * Go through all the plexes. Insert an entry for every
	 * volume address range that's not already in table.
	 * Set the plex's ve index which corresponds to that address.
	 * If the plex does not cover that address range insert -1 as
	 * the ve index.
	 */
	rows = 0;
	for (p = 0; p < XLV_MAX_PLEXES; p++) {
		plex = svp->plex[p];
		if (plex == NULL)
			continue;		/* missing plex */
		
		/*
		 * for each volume element in this plex...
		 */
		tab_idx = 0;
		for (ve_idx = 0; ve_idx < plex->num_vol_elmnts; ve_idx++) {
			ve_start_blkno =
				plex->vol_elmnts[ve_idx].start_block_no;

			if (rows == 0) {
				/*
				 * First valid volume element that we see.
				 * For the rest of this plex, we'll be
				 * in append mode (rows == tab_idx).
				 */
				table[0].start_blkno = ve_start_blkno;
				table[0].end_blkno =
					plex->vol_elmnts[ve_idx].end_block_no;
				table[0].plex_ve_idx[p] = ve_idx;
				rows = tab_idx = 1;
				continue;	/* next ve */
			}

			/*
			 * Find ve's corresponding table entry.
			 * Start the search from where the previous
			 * ve in this plex left off.
			 */
			for ( ; tab_idx < rows; tab_idx++) {
				tab_start_blkno = table[tab_idx].start_blkno;
				if (ve_start_blkno == tab_start_blkno) {
					table[tab_idx].plex_ve_idx[p] = ve_idx;
					goto next_ve_iteration;
				}
				if (ve_start_blkno < tab_start_blkno)
					break;	/* insert this ve */
			}

			if (tab_idx == rows) {
				/*
				 * Append mode. Add this entry to the end
				 * of stack. And stay in append mode since
				 * all following ve's in this plex should
				 * be after this stack entry.
				 */
				table[tab_idx].start_blkno = ve_start_blkno;
				table[tab_idx].end_blkno =
					plex->vol_elmnts[ve_idx].end_block_no;
				table[tab_idx].plex_ve_idx[p] = ve_idx;
				tab_idx++;	/* update rows later */

			} else {
				/*
				 * Insert mode. Move all entries over
				 * one slot and re-initialize the slot
				 * begin inserted.
				 */
				int	i;
				for (i = rows; i > tab_idx; i--) {
					table[i] = table[i-1];
				}
				for (i = 0; i < XLV_MAX_PLEXES; i++) {
					table[tab_idx].plex_ve_idx[i] = -1;
				}

				/* insert the entry */
				table[tab_idx].start_blkno = ve_start_blkno;
				table[tab_idx].end_blkno =
					plex->vol_elmnts[ve_idx].end_block_no;
				table[tab_idx].plex_ve_idx[p] = ve_idx;
			}
			rows++;

			if (rows == max_entries)
				break;
next_ve_iteration:
			/* NULL - a NOP */ ;

		} /* for each ve */

	} /* for each plex */

	return (rows);

} /* end of xlv_fill_subvol_ve_table() */


/*
 * Check a disk partition to make sure its ptype is consistent with
 * the type of the subvolume to which it belongs.
 * 
 * subvol_type can be LOG, DATA, or RT if the partition belongs to a
 * volume. If the partition is part of a standalone plex or ve, then
 * subvol_type will be XLV_SUBVOL_TYPE_UNKNOWN.
 *
 * When we are creating an XLV volume, we will accept XFS, EFS, XLV.
 *
 * NOTE: 12/13/94  We accept XFSLOG for backward compatiblilty.
 *
 * This function returns -1 if the ptype is not valid for this subvol
 * type or standalone object. It returns the subvol type if the ptype
 * is valid. For a standalone object, the return subvol type is
 * XLV_SUBVOL_TYPE_DATA.
 */
int
xlv_check_partition_type(
	int	subvol_type,
	int	partition_type,
	int	xlv_make_flag)
{
	switch (subvol_type) {

		case	XLV_SUBVOL_TYPE_UNKNOWN:
			/*
			 * Both LOG and DATA are valid for
			 * standalone plex and ve.
			 */
			if (partition_type == PTYPE_XLV ||
			    partition_type == PTYPE_XFSLOG) {
				return XLV_SUBVOL_TYPE_DATA;

			} else if (xlv_make_flag == YES) {
				/* See comment below */
				switch (partition_type) {
				case PTYPE_EFS:
				case PTYPE_XFS:
				case PTYPE_XFSLOG:
					return XLV_SUBVOL_TYPE_DATA;
				}
			}
			break;

		case	XLV_SUBVOL_TYPE_LOG:
			if (partition_type == PTYPE_XLV ||
			    partition_type == PTYPE_XFSLOG) {
				return XLV_SUBVOL_TYPE_LOG;
			}
			break;

		case	XLV_SUBVOL_TYPE_DATA:
		case	XLV_SUBVOL_TYPE_RT:
			if (partition_type == PTYPE_XLV)
				return XLV_SUBVOL_TYPE_DATA;

			if (xlv_make_flag == YES) {
				/*
				 * Differentiate between xlv_make and all
				 * other cases. When xlv_make create a
				 * new object it accepts partition types
				 * "efs", "xfs", & "xfslog" in addition
				 * to "xlv". xlv_make does convert all
				 * the above partition types to "xlv".
				 */
				switch (partition_type) {
				case PTYPE_EFS:
				case PTYPE_XFS:
				case PTYPE_XFSLOG:
					return XLV_SUBVOL_TYPE_DATA;
				}
			}
			break;

		default:
			ASSERT (0);
			break;
		}

	return -1;

} /* end of xlv_check_partition_type() */


/*
 * Print bad label warning given volume header pathname,
 * partition number, and reason. Assumes standard SGI
 * disk#d#s# convention.
 */
/* ARGSUSED */
void
xlv_print_type_check_msg(
	const char	*template,
	char		*pathname,
	const char	*reason,
	dev_t		devnum,
	dev_t		part,
	const char	*force)
{
	char    devname[MAXDEVNAME];
	int     len = sizeof(devname);

	char	alias[MAXDEVNAME];
	int	alias_len = sizeof(alias);

	sprintf(devname,
		"Unable to obtain device pathname for partition %d\n",
		(int)part);
	if (pathname == NULL) {
		dev_to_devname(devnum, devname, &len);
	} else {
		path_to_partpath(pathname, (int)part, S_IFCHR, devname, &len, NULL);
	}

	diskpath_to_alias(devname, alias, &alias_len);

	if (force) {
		/*
		 * XXX The <force> string does not contain a
		 * carriage return so "\n" needs to be appended.
		 */
		fprintf(stderr, template, alias, "", reason, force);
		fprintf(stderr, "\n");
	} else {
		fprintf(stderr, template, alias, "", reason, "\n");
	} 

} /* end of xlv_print_type_check_msg() */


/*
 * Remove all volumes that are not "complete". A volume is not complete
 * when it is missing a volume element which cannot be replaced by
 * a volume element from another plex.
 */
int
xlv_filter_complete_vol(
	xlv_tab_vol_t	*xlv_tab_vol,
	int		quiet)
{
	xlv_tab_vol_entry_t	*vol_p;
	int			v;
	int			remove_count;
	int			bad;

	remove_count = v = 0;

	while (v < xlv_tab_vol->num_vols) {

		bad = 0;

		vol_p = &xlv_tab_vol->vol[v];

		if ((vol_p->state == XLV_VOL_STATE_NONE) ||
		    (vol_p->state == XLV_VOL_STATE_ALL_PIECES_BUT_HOLEY) ||
		    (vol_p->state == XLV_VOL_STATE_MISS_UNIQUE_PIECE)) {
			bad = 1;

		}
		
		if (vol_p->data_subvol == NULL) {
			/*
			 * Volume must have a data subvolume
			 *
			 * XXXjleong Why is the state being set
			 * if this vol entry gets removed? Is this
			 * state used?
			 */
			vol_p->state = XLV_VOL_STATE_MISS_REQUIRED_SUBVOLUME;
			bad = 1;
		}

		if (bad) {
			if (!quiet) {
				fprintf (stderr,
					"Removing XLV Volume \"%.*s\" [%s]\n",
					sizeof(xlv_name_t),
					vol_p->name,
					xlv_vol_state_str(vol_p->state));
			}
			_remove_vol_from_tab (xlv_tab_vol, v);
			remove_count++;
			/* don't increment and do this slot again */ 
		} else {
			v++;
		}
	}

	return(remove_count);
} /* end of xlv_filter_complete_vol() */


/*
 * Remove all volumes that are "invalid".
 * A volume is not valid:
 *	- a subvolume has a size of 0.
 *	- a subvolume size exceeds the kernel maximum size.
 */
int
xlv_filter_valid_vol(
	xlv_tab_vol_t	*xlv_tab_vol,
	int		quiet)
{
	xlv_tab_vol_entry_t	*vol_p;
	xlv_tab_subvol_t	*svp;
	int			v;
	int			remove_count;
	int			bad;
	__uint64_t		max, subvol_size;

	remove_count = v = 0;

	max = get_kernel_sv_maxsize();

	while (v < xlv_tab_vol->num_vols) {

		bad = 0;
		vol_p = &xlv_tab_vol->vol[v];

		/*
		 * XXX Should use svp->subvol_size instead of
		 * calling xlv_tab_subvol_size(). Make the change
		 * when svp->subvol_size is a 64 bits field.
		 */
		if (!bad && (svp = vol_p->data_subvol)) {
			subvol_size =  xlv_tab_subvol_size(svp);
			if (svp->subvol_size==0 || subvol_size > max)
				bad = 1;
		}
		if (!bad && (svp = vol_p->log_subvol)) {
			subvol_size =  xlv_tab_subvol_size(svp);
			if (svp->subvol_size==0 || subvol_size > max)
				bad = 1;
		}
		if (!bad && (svp = vol_p->rt_subvol)) {
			subvol_size =  xlv_tab_subvol_size(svp);
			if (svp->subvol_size==0 || subvol_size > max)
				bad = 1;
		}

		if (bad) {
			if (!quiet) {
				fprintf (stderr,
"Removing XLV Volume \"%.*s\" : volume exceeds maximum size %lld blks\n",
					sizeof(xlv_name_t),
					vol_p->name,  max);
			}
			_remove_vol_from_tab (xlv_tab_vol, v);
			remove_count++;
			/* don't increment and do this slot again */ 
		} else {
			v++;
		}
	}

	return(remove_count);
} /* end of xlv_filter_valid_vol() */

/*
 * External interface to remove a particular volume
 */
void
xlv_remove_vol(
	xlv_tab_vol_t *tab_vol, 
	int vol_idx)
{

	_remove_vol_from_tab (tab_vol, vol_idx);
}

static void
_remove_vol_from_tab (xlv_tab_vol_t *tab_vol, int vol_idx)
{
	xlv_tab_vol_entry_t	*vol_p;
	xlv_tab_subvol_t	*svp;
	int			last;

	vol_p = &tab_vol->vol[vol_idx];

	/*
	 * Mark subvolumes "free" by setting their volume pointer to NULL.
	 */
	if (svp = vol_p->log_subvol) {
		svp->vol_p = NULL;
	}
	if (svp = vol_p->data_subvol) {
		svp->vol_p = NULL;
	}
	if (svp = vol_p->rt_subvol) {
		svp->vol_p = NULL;
	}

	/*
	 * Remove this volume from the volume table.
	 */
	 last = tab_vol->num_vols - 1;
	 if (vol_idx < last) {
		/*
		 * Move last volume entry here. Must update the
		 * corresponding subvolumes' volume back pointer.
		 */
		bcopy (&tab_vol->vol[last], vol_p, sizeof(*vol_p));

		if (svp = vol_p->log_subvol) {
			svp->vol_p = vol_p;
		}
		if (svp = vol_p->data_subvol) {
			svp->vol_p = vol_p;
		}
		if (svp = vol_p->rt_subvol) {
			svp->vol_p = vol_p;
		}
	}

	tab_vol->num_vols--;
} /* end of _remove_vol_from_tab() */


/*
 * Memory allocation routines
 */

xlv_tab_subvol_t *
get_subvol_space( void )
{
        int                     i, plexsize;
        xlv_tab_plex_t          *plex;
        xlv_tab_subvol_t        *subvol;

        subvol = malloc(sizeof(xlv_tab_subvol_t));
        bzero(subvol, sizeof(xlv_tab_subvol_t));
        plexsize = sizeof(xlv_tab_plex_t) +
                        (sizeof(xlv_tab_vol_elmnt_t) * (XLV_MAX_VE_PER_PLEX-1));
        for ( i = 0; i < XLV_MAX_PLEXES; i ++ ) {
                plex = malloc( plexsize );
                bzero(plex, plexsize );
		plex->max_vol_elmnts = XLV_MAX_VE_PER_PLEX;
                subvol->plex[i] = plex;
        }
        return( subvol );
}


xlv_tab_vol_entry_t *
get_vol_space(void)
{
	xlv_tab_vol_entry_t *vol;

	vol = malloc(sizeof(*vol));

	if (vol == NULL)
		return (0);

        bzero(vol, sizeof(*vol));

	if ((NULL == (vol->log_subvol = get_subvol_space())) ||
	    (NULL == (vol->data_subvol = get_subvol_space())) ||
	    (NULL == (vol->rt_subvol = get_subvol_space()))) {
		free_vol_space(vol);
		return (0);
	}
	
	return (vol);
}


int
free_subvol_space(xlv_tab_subvol_t *subvol) 
{
	int i;

	for ( i = 0; i < XLV_MAX_PLEXES; i++ ) {
		if (subvol->plex[i])
			free(subvol->plex[i]);
	}
	free(subvol);
	return (0);
}


int
free_vol_space(xlv_tab_vol_entry_t *vol)
{
	if (vol == NULL)
		return (0);

	if (vol->log_subvol)
		free_subvol_space(vol->log_subvol);
	if (vol->data_subvol)
		free_subvol_space(vol->data_subvol);
	if (vol->rt_subvol)
		free_subvol_space(vol->rt_subvol);
	if (vol->nodename)
		free(vol->nodename);
	free(vol);
	return (0);
}


/*
 * Free the subvolume table.
 */
void
free_tab_subvol(xlv_tab_t **tab_sv_p)
{
	xlv_tab_subvol_t *subvol;
	xlv_tab_t	 *tab_sv;
	int		 i, j;

	if (tab_sv_p == NULL || *tab_sv_p == NULL)
		return;

	tab_sv = *tab_sv_p;
	for (i = 0; i < tab_sv->max_subvols; i++) {
		subvol = &tab_sv->subvolume[i];
		if (! XLV_SUBVOL_EXISTS(subvol)) {
			continue;
		}
		for ( j = 0; j < XLV_MAX_PLEXES; j++ ) {
			if (subvol->plex[j])
				free(subvol->plex[j]);
		}
	}

	free(tab_sv);
	*tab_sv_p = NULL;
	return;
}


/*
 * Free the volume table.
 */
void
free_tab_vol(xlv_tab_vol_t **tab_vol_p)
{
	xlv_tab_vol_t	*tab_vol;
	char		*nodename;
	int		i;

	if (tab_vol_p == NULL || *tab_vol_p == NULL)
		return;

	tab_vol = *tab_vol_p;
	for (i=0; i < tab_vol->num_vols; i++) {
		/*
		 * Note that the subvolumes {log,data,rt}_subvol
		 * are not being freed.
		 */
		if (nodename = tab_vol->vol[i].nodename)
			free(nodename);
	}
	free(tab_vol);

	*tab_vol_p = NULL;
	return;
}


/*
 * Query the kernel for the number of locks configured for XLV.
 */
int
get_max_kernel_locks (void)
{
	xlv_attr_req_t		req;
	xlv_attr_cursor_t	cursor;

	/*
	 * First we need to get a XLV cursor.
	 */
	if (0 > syssgi(SGI_XLV_ATTR_CURSOR, &cursor)) {
		return (-1);
	}

	req.attr = XLV_ATTR_LOCKS;
	if (0 > syssgi(SGI_XLV_ATTR_GET, &cursor, &req)) {
		req.ar_max_locks = -1;
	}

	return (req.ar_max_locks);
}


/*
 * Query the kernel for the maximum subvolume size supported.
 */
__uint64_t
get_kernel_sv_maxsize (void)
{
	xlv_attr_req_t		req;
	xlv_attr_cursor_t	cursor;

	/*
	 * First we need to get a XLV cursor.
	 */
	if (0 > syssgi(SGI_XLV_ATTR_CURSOR, &cursor)) {
		return (-1);
	}

	req.attr = XLV_ATTR_SV_MAXSIZE;
	if (0 > syssgi(SGI_XLV_ATTR_GET, &cursor, &req)) {
		/*
		 * Cannot get size from the kernel, so default
		 * to 32 bits of daddr_t.
		 */
		req.ar_sv_maxsize = 0x7FFFFFFF;
	}

	return (req.ar_sv_maxsize);
}


/*
 * Query the kernel for the root disk device. This is the
 * device that had the xlv label in its volume header which
 * flag this system as having a root volume.
 */
dev_t
xlv_get_rootdevice (void)
{
	xlv_attr_req_t		req;
	xlv_attr_cursor_t	cursor;

	/*
	 * First we need to get a XLV cursor.
	 */
	if (0 > syssgi(SGI_XLV_ATTR_CURSOR, &cursor)) {
		return (-1);
	}

	req.attr = XLV_ATTR_ROOTDEV;
	if (0 > syssgi(SGI_XLV_ATTR_GET, &cursor, &req)) {
		req.ar_rootdev = makedev(0,0);
	}

	return (req.ar_rootdev);
}
