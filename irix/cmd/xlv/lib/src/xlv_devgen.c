/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1993, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#ident "$Revision: 1.31 $"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <bstring.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/debug.h>
#include <sys/major.h>
#include <sys/syssgi.h>
#include <sys/uuid.h>
#include <sys/xlv_base.h>
#include <sys/xlv_tab.h>
#include <sys/xlv_attr.h>
#include "xlv_utils.h"
#include <sys/dkio.h>
#include <stdio.h>
#include <xlv_cap.h>


#define ROOT_DEV	"/dev/rroot"
#define TMPVDEV		"/tmp/"

/*
 * A table of subvolume uuids is used to track XLV device
 * minor number. The "index" into this table corresponds to
 * a XLV device minor number. This table is used for keeping
 * state across multiple xlv_assemble calls. A subvolume must
 * always be assigned the same device number for the life-time
 * of the current system session.
 */

typedef struct tab_uuid_s {
	__uint32_t	size;			/* maximum entries */
	uuid_t		entry[1];		/* table of uuid's */
} tab_uuid_t;

static tab_uuid_t	*tab_uuid = NULL;
static int		_xlv_grow_tab_uuid(tab_uuid_t **old_p, int min_size);
static int		_xlv_get_tab_uuid(void);


static char *_rootname = NULL;

char *
xlv_getrootname(void)
{
	return _rootname;
}

/*
 * Set "name" as the xlv root directory. XLV data files are references
 * relative to that root directory.
 *
 * On failure return NULL. Caller can look into "errno" for error reason.
 */
char *
xlv_setrootname(char *name)
{
	char		*new_root;
	struct stat	buf;

	new_root = (name == NULL) ? NULL : strdup(name);

	if (new_root) {
		size_t	len;

		/*
		 * Remove all trailing "/"'s.
		 */
		len = strlen (new_root);
		while (new_root[--len] == '/') {
			new_root[len] = NULL;
		}

		/*
		 * Check that the name specifies a valid directory.
		 */
		if (0 > stat(new_root, &buf))
			return(0);

		if (0 == (S_IFDIR & buf.st_mode)) {
			setoserror(ENOTDIR);
			return(0);
		}

		if (_rootname)
			free(_rootname);

		_rootname = new_root;
	}

	return _rootname;
}

/*
 * Generate a pathname rooted at the user set root directory.
 * The pathname passed in must be a fully qualified pathname.
 */
char *
xlv_root_pathname(char *rooted_pn, char *fqpn)
{
	/*
	 * XXXjleong Do error checking of string length.
	 */
	if (!fqpn || !rooted_pn)
		return NULL;

	if (_rootname) 
		sprintf(rooted_pn, "%s%s", _rootname, fqpn);
	else
		strcpy (rooted_pn, fqpn);

	return rooted_pn;
}


#if 0 /* this code is no longer used! */
/*
 * Given an xlv_tab_vol entry, generate device nodes for each subvolume
 * for the specified volume.
 * The generated device numbers are stored in the xlv_tab directly.
 *
 * XXXjleong This may not be called since device numbers are generated
 * at the time of xlv_assemble.
 */
int
xlv_create_nodes (xlv_tab_vol_entry_t *tab_vol_entry)
{
	char			*volname = tab_vol_entry->name;
	xlv_tab_subvol_t	*sv;

	if (tab_vol_entry->log_subvol != NULL) {
		sv = tab_vol_entry->log_subvol;
		sv->dev = xlv_uuid_to_dev(&sv->uuid);
	}

	if (tab_vol_entry->data_subvol != NULL) {
		sv = tab_vol_entry->data_subvol;
		sv->dev = xlv_uuid_to_dev(&sv->uuid);
		if (xlv_create_node_pair(volname, sv->dev) < 0) {
			return (-1);
		}
	}

	if (tab_vol_entry->rt_subvol != NULL) {
		sv = tab_vol_entry->rt_subvol;
		sv->dev = xlv_uuid_to_dev(&sv->uuid);
	}

	return (0);
}
#endif /* dead code */


/*
 * Given a filename (x) and a device number, create a pair of nodes
 * /dev/xlv/x and /dev/rxlv/x. If a device exists with a different
 * number, check that this is a real device (not left over stuff): if so,
 * print an error (two volumes with the same name).  Otherwise, delete it
 * and create a new device with the correct device number.
 */
int
xlv_create_node_pair (xlv_name_t base_name, char *node_name, dev_t devno,
		      xlv_tab_t *xlv_tab)
{
	char		dev_path[XLV_MAXDEVPATHLEN];
	struct stat	buf;
	mode_t		mode;
	int		count = 0;

#ifdef DEBUG
#define DEBUG_PERROR(s)		perror(s);
#else
#define DEBUG_PERROR(s)
#endif

	/* See if the block device exists. */
	xlv_root_pathname(dev_path, XLV_DEV_BLKPATH);
	mode = S_IFBLK|S_IRUSR|S_IWUSR;		/* block special file */

again:
	if (node_name != NULL) {
		strcat(dev_path, node_name);
		strcat(dev_path, ".");
	}
	strncat (dev_path, base_name, sizeof(xlv_name_t));

	buf.st_rdev = makedev(0,0);
	if ((0 == stat (dev_path, &buf)) && (devno != buf.st_rdev)) {
		int	  sv_num = minor(buf.st_rdev);

		/*
		 * If the stat'ed device is a valid volume (i.e., valid dev_t,
		 * and volume exists), and the names match, then we have a
		 * name conflict: print an error message and return.
		 * node names match if both are NULL or both are non-NULL
		 * and the contents match.
		 */
		if (major(buf.st_rdev) == XLV_MAJOR
		    && sv_num < xlv_tab->max_subvols
		    && XLV_SUBVOL_EXISTS(&xlv_tab->subvolume[sv_num])
		    && !strcmp(xlv_tab->subvolume[sv_num].vol_p->name,
			       base_name)
		    && ((xlv_tab->subvolume[sv_num].vol_p->nodename == NULL
			 && node_name == NULL)
			||
			(xlv_tab->subvolume[sv_num].vol_p->nodename != NULL
			 && node_name != NULL
			 && !strcmp(xlv_tab->subvolume[sv_num].vol_p->nodename,
				    node_name)))) {
			/* Oh, oh! Two volumes have identical names! */
			fprintf(stderr, "Cannot create device node %s "
				"at (%d, %d) [already in use].\n",
				dev_path, XLV_MAJOR, minor(devno));
			return -1;
		}

		/*
		 * Device exists but has the wrong dev numbers.
		 * Delete the original and recreate it.
		 */
		if (unlink (dev_path)) {
			DEBUG_PERROR(dev_path);
			return (-1);
		}
	}
	if (devno != buf.st_rdev) {
		/*
		 * Create the special file
		 */
		if (cap_mknod(dev_path, mode, devno) < 0) {
			DEBUG_PERROR(dev_path);
			return (-1);
		}
	}

	if (count == 0) {
		/*
		 * Go back and create (if necessary) the raw device.
		 */
		xlv_root_pathname(dev_path, XLV_DEV_RAWPATH);
		mode = S_IFCHR|S_IRUSR|S_IWUSR;
		count++;
		goto again;
	}

	/* Both block and raw devices are created. */
	return (0);
#undef DEBUG_PERROR
} /* end of xlv_create_node_pair() */


#ifdef STAT_FOR_BOOTINFO 
int
xlv_get_uuid_for_dev(dev_t vdev, uuid_t *uuidp)
{
	int			fd;
	int			mypid;
	char			dev_name[100];
	xlv_tab_subvol_t	*subvol;

	mypid = getpid();

        /*
         * Make virtual disk dev node.
         */
        sprintf(dev_name, "%s.pid%d.dev0x%x", TMPVDEV, mypid, vdev);
        if (cap_mknod( dev_name, S_IFCHR, vdev)) {
		fprintf(stderr, "Cannot mknod(%s): %s\n",
			dev_name, strerror(oserror()));
                return( 0 );
        }

        if ((fd = open( dev_name, O_RDWR )) == -1 ) {
		/*
		 * Don't need to display error message for open
		 * because the device may not exist.
		 */
                unlink( dev_name );
                return( 0 );
        }

	subvol = get_subvol_space();
	
	if ( ioctl(fd, DIOCGETSUBVOLINFO, subvol) < 0 ) {
		fprintf(stderr, "Cannot ioctl(%s, DIOCGETSUBVOLINFO): %s\n",
			dev_name, strerror(oserror()));
		close(fd);
                unlink( dev_name );
		return( 0 );
	}

	COPY_UUID(subvol->uuid, (*uuidp));
	free_subvol_space( subvol );
	
	close(fd);
	unlink(dev_name);
	return ( 1 );
} /* end of xlv_get_uuid_for_dev() */
#endif /* STAT_FOR_BOOTINFO */


#ifdef STAT_FOR_BOOTINFO
int
xlv_get_bootinfo(uuid_t *datauuid, uuid_t *loguuid, uuid_t *rtuuid)
{
	dev_t		vdev;
	uint_t		st;
	struct stat	statbuf;

	/*
	 * Stat the /dev/rroot device.
	 */
	if ( stat(ROOT_DEV, &statbuf) ) {
		return ( 0 );
	}

	/*
	 * Check if root is an xlv device.
 	 */
	if ( major(statbuf.st_rdev) != XLV_MAJOR ) {
		return ( 0 );
	}

	/*
	 * If it is a xlv device, get the subvolume information.
 	 */

	/*
	 * Check for a data subvolume.
	 */
	vdev = makedev(XLV_MAJOR, XLV_TAB_UUID_BOOT_DATA);
	if (!xlv_get_uuid_for_dev( vdev, datauuid) ) {
		uuid_create_nil(datauuid, &st);
	} 

	/*
	 * Check for a log subvolume.
	 */
	vdev = makedev(XLV_MAJOR, XLV_TAB_UUID_BOOT_LOG);
	if (!xlv_get_uuid_for_dev( vdev, loguuid)) {
		uuid_create_nil(loguuid, &st);
	}

	/*
	 * Check for a rt subvolume.
	 */
	vdev = makedev(XLV_MAJOR, XLV_TAB_UUID_BOOT_RT);
	if (!xlv_get_uuid_for_dev( vdev, rtuuid) ) {
		uuid_create_nil(rtuuid, &st);
	}

	return( 1 );

} /* end of xlv_get_bootinfo() */
#endif /* STAT_FOR_BOOTINFO */


/*
 * Lookup the subvolume uuid in a table. If an entry exists for that
 * uuid then return the table index (which corresponds to the XLV device
*  minor number). If the uuid is not found in the table, add it and
 * return the index.
 *
 * NOTE:
 *	The table can be sparse.
 */
dev_t
xlv_uuid_to_dev(uuid_t *uuid)
{
	int		index;
	int		found = 0;
	int		firstfree = -1;
#ifdef STAT_FOR_BOOTINFO
	int		xlv_root_dev = 1;
	uuid_t		datauuid, loguuid, rtuuid;
#endif
	unsigned	st;


	if (uuid_is_nil(uuid, &st)) {
		ASSERT(0);	/* shouldn't be called without a uuid */
		return (makedev(0,0));
	}

#ifdef STAT_FOR_BOOTINFO
	if ( !xlv_get_bootinfo(&datauuid,&loguuid, &rtuuid) ) {
		xlv_root_dev = 0;
	}
#endif

	if (tab_uuid == 0) {
		(void) _xlv_get_tab_uuid();	/* generate the table */
	}

	/*
	 * The first XLV_TAB_UUID_RSVD number slots are reserved.
	 *
	 * Determine if the disk partition with this uuid is
	 * already part of the root device. If so, the index number
	 * cannot be changed.
 	 */
#ifdef STAT_FOR_BOOTINFO
	if (xlv_root_dev) {
		if ( !uuid_is_nil(&datauuid, &st) && 
				uuid_equal(uuid, &datauuid, &st)) {
			return (makedev (XLV_MAJOR, XLV_TAB_UUID_BOOT_DATA));
		}

		if ( !uuid_is_nil(&loguuid, &st) && 
				uuid_equal(uuid, &loguuid, &st)) {
			return (makedev (XLV_MAJOR, XLV_TAB_UUID_BOOT_LOG));
		}

		if ( !uuid_is_nil(&rtuuid, &st) && 
				uuid_equal(uuid, &rtuuid, &st)) {
			return (makedev (XLV_MAJOR, XLV_TAB_UUID_BOOT_RT));
		}
	}
#endif	/* STAT_FOR_BOOTINFO */

	for (index=0; index < XLV_TAB_UUID_RSVD; index++) {
		if (!uuid_is_nil(&tab_uuid->entry[index], &st)
		    && uuid_equal(uuid, &tab_uuid->entry[index], &st))
			return(makedev(XLV_MAJOR, index));
	}


	for (index=XLV_TAB_UUID_RSVD; index < tab_uuid->size; index++)
	{
		if ( uuid_is_nil(&tab_uuid->entry[index], &st) ) {
			if (firstfree == -1)
				firstfree = index; /* save for future use */
			continue;
		}
		if (uuid_equal(uuid, &tab_uuid->entry[index], &st)) {
			found++;
			break;
		}
	}

	if (!found) {
		if (firstfree != -1)
			index = firstfree;

		if (index >= tab_uuid->size)
			_xlv_grow_tab_uuid(&tab_uuid, index);

		COPY_UUID (*uuid, tab_uuid->entry[index]);
	}

	return (makedev(XLV_MAJOR, index));

} /* end of xlv_uuid_to_dev() */


static int
_xlv_grow_tab_uuid (tab_uuid_t **old_p, int min_size)
{
	tab_uuid_t	*new, *old;
	size_t		new_len;		/* byte count */
	int		new_size;		/* max entries */

	ASSERT(old_p && *old_p);

	old = *old_p;

	/*
	 * Grow the table enough ...
	 */
	new_size = old->size + XLV_TAB_UUID_GROW;
	if (new_size <= min_size) {
		new_size = min_size + XLV_TAB_UUID_GROW;
	}

	new_len = sizeof(tab_uuid_t) + (sizeof(uuid_t) * (new_size - 1));
	new = (tab_uuid_t *) malloc(new_len);
	bzero (new, (int)new_len);
	new->size = new_size;

	/* move old data */
	bcopy ( &old->entry[0],
		&new->entry[0],
		(int)(sizeof(uuid_t) * old->size));

	free(old);
	*old_p = new;		/* make the new table the real thing */

	return (new_size);

} /* end of _xlv_grow_tab_uuid() */


/*
 * Generate the uuid to device number table by enumerating the
 * subvolumes in the kernel and getting the subvolume uuid and
 * device number.
 *
 * By getting the uuid-to-device table from the kernel, context
 * is perserved. One can do multiple xlv_assemble's and be assure
 * that a subvolume is always assigned the same dev_t for as long
 * as the system stays up. Device numbers are also recycled when
 * they are not used.
 *
 * Return the number of the subvolumes in the kernel.
 */
static int
_xlv_get_tab_uuid(void)
{
	xlv_attr_cursor_t	cursor;
	xlv_tab_subvol_t	subvol;
	xlv_attr_req_t		req;
	int			status;
	int			count;
	int			index;
	size_t			len;
	struct full_plex_s {
		xlv_tab_plex_t		plex;
		xlv_tab_vol_elmnt_t	ve[XLV_MAX_VE_PER_PLEX-1];	
	} fullplex;

	xlv_tab_plex_t	*scratchplex = &fullplex.plex;

	if (tab_uuid != NULL)
		return (-1);		/* table already exists */

	bzero(&subvol, sizeof(xlv_tab_subvol_t));
	/*
	 * Don't really care about the plex info so just point to
	 * a scratch area.
	 */
	subvol.plex[0] = subvol.plex[1] = scratchplex;
	subvol.plex[2] = subvol.plex[3] = scratchplex;

	/*
	 * Create the table.
	 */

	len = sizeof(tab_uuid_t) + (sizeof(uuid_t) * XLV_TAB_UUID_SIZE-1);
	tab_uuid = (tab_uuid_t *) malloc(len);
	/*FALLTHROUGH*/
again:
	bzero(tab_uuid, (int)len);
	tab_uuid->size = XLV_TAB_UUID_SIZE;

	/*
	 * Fill up the table.
	 */

	if (0 > syssgi(SGI_XLV_ATTR_CURSOR, &cursor)) {
#ifdef DEBUG
	perror("DBG: syssgi(SGI_XLV_ATTR_CURSOR) failed for tab_uuid build.");
#endif
		return 0;
	}

	req.attr = XLV_ATTR_SUBVOL;
	req.ar_svp = &subvol;
	status = 0;
	count = 0;

	while (status == NULL) {
		status = syssgi(SGI_XLV_ATTR_GET, &cursor, &req);
		if (0 > status) {
			switch(oserror()) {
			    case ENOENT:		/* xlv not assembled */
			    case ENFILE:		/* end of enumeration */
				goto done;

			    case ESTALE:
				/*
				 * Configuration changed while we were
				 * busy doing the enumeration so let's
				 * start from the beginning.
				 */
				goto again;
				
			    default:
#ifdef DEBUG
	perror("DBG: syssgi(SGI_XLV_ATTR_GET) failed for tab_uuid build.");
#endif
				goto done;
			}
		}

		/*
		 * Using this subvolume's minor number as the index,
		 * insert this subvolume into uuid-to-device table.
		 */
		index = minor(subvol.dev);
		if (index >= tab_uuid->size)
			_xlv_grow_tab_uuid(&tab_uuid, index);

		COPY_UUID (subvol.uuid, tab_uuid->entry[index]);
		count++;
	} /* while loop */

done:
	return (count);

} /* end of _xlv_get_tab_uuid() */



#ifdef TESTING
void
main (int argc, char *argv[])
{
    dev_t       dev;

    dev = makedev(XLV_MAJOR, 0xFF);

    if (xlv__create_node_pair (argv[1], &dev)) {
        printf ("create_nodes failed\n");
    }
    else {
        printf ("Major number: %d, minor number: %d\n",
                major(dev), minor(dev));
    }
}
#endif

