/*
 *=============================================================================
 * 		File:		dos_node.c
 *		Purpose:	DOS filesystem file operations.
 *=============================================================================
 */
#include "dos_fs.h"
#include <errno.h>
#include <string.h>
#include <rpc/rpc.h>
#include <sys/param.h>
#include <sys/stat.h>

extern  uid_t	uid;
extern  gid_t	gid;
extern	int	debug;

static char	dot[] 		= ".";
static char	dotdot[] 	= "..";
static char	dotname[] 	= ".       ";
static char	dotdotname[] 	= "..      ";
char		nullext[] 	= "   ";


/* 
 *-----------------------------------------------------------------------------
 * dos_readnode()
 * This routine is used to find information about the specified 
 * file from disk directory entry.
 *-----------------------------------------------------------------------------
 */
int 	dos_readnode(dnode_t *dp)
{
	file_t	f;
	dfile_t	*df;
	dfs_t	*dfs;
	int	error;
	u_short	offset;
	u_short	cluster;

	dfs = dp->d_dfs;
	if (dp->d_dir != NULL)
		vfat_dir_write(dp);
	dp->d_uid = UID;
	dp->d_gid = GID;

	/* 	
	 * Fake up a dnode for the root 
	 */
	if (dp->d_fno == ROOTFNO) {
		dp->d_ftype = NFDIR;
		dp->d_dir = (dfile_t *) safe_malloc(dfs->dfs_rootsize);
		lseek(dfs->dfs_fd, dfs->dfs_rootaddr, 0);
		if (read(dfs->dfs_fd, &dp->d_dir[2],
			 dfs->dfs_rootsize-2*DIR_ENTRY_SIZE) < 0)
			return (errno);
		/* 
		 * Make up entries for "." and ".." 
		 */
		df = dp->d_dir;
		strncpy(df->df_name, dotname, 8);
		strncpy(df->df_ext, nullext, 3);
		df++;
		strncpy(df->df_name, dotdotname, 8);
		strncpy(df->df_ext, nullext, 3);

		dp->d_mode = READ_MODE | WRITE_MODE | SEARCH_MODE;
		dp->d_pfno = ROOTFNO;
		dp->d_size = dfs->dfs_rootsize;
		dp->d_time = time(NULL);
		return (0);
	}
	f.f_offset  = offset  = OFFSET(dp->d_fno);
	f.f_cluster = cluster = CLUSTER(dp->d_fno);
	if (cluster == ROOT_CLUSTER){
                xtract_fp_from_root(dfs, (dfile_t *) dfs->dfs_root->d_dir,
                                    offset, &f);
	}
	else {				
		error = vfat_dir_lookup(dp, &f);
		if (error == -1){
			error = vfat_dirent_read_disk(dfs, &f);
			if (error){
				/* Unable to read file entry */
				return (error);
			}
		}
	}
	df = &f.f_entries[0];
	dp->d_start = DF_START(df);
	to_unixtime(df->df_date, df->df_time, &dp->d_time);
	if (df->df_attribute & ATTRIB_DIR) {
		dp->d_ftype = NFDIR;
		dp->d_mode = READ_MODE | WRITE_MODE | SEARCH_MODE;
		dp->d_size = subdir_size(dp);
	} else {
		dp->d_ftype = NFREG;
		if (df->df_attribute & ATTRIB_RDONLY)
			dp->d_mode = READ_MODE | SEARCH_MODE;
		else 	dp->d_mode = READ_MODE | WRITE_MODE | SEARCH_MODE;
		dp->d_size = DF_SIZE(df);
	}
	return (0);
}

/* 
 *-----------------------------------------------------------------------------
 * dos_access()
 * This routine is used to check the access mode 
 *-----------------------------------------------------------------------------
 */
int 	dos_access(dnode_t *dp, struct authunix_parms *aup, int amode)
{
	if (aup->aup_uid == 0)
		return 0;
	if ((dp->d_mode & amode) == amode)
		return 0;
	return (EACCES);
}

/* 
 *-----------------------------------------------------------------------------
 * dos_getattr()
 * This routine is used to get file attributes 
 *-----------------------------------------------------------------------------
 */
int 	dos_getattr(dnode_t *dp, fattr *fa)
{
	fa->type = dp->d_ftype;
	fa->mode = dp->d_mode;
	switch (fa->type){
	case NFREG:
	  fa->mode |= NFSMODE_REG;
	  break;
	case NFDIR:
	  fa->mode |= NFSMODE_DIR;
	  break;
	case NFBLK:
	  fa->mode |= NFSMODE_BLK;
	  break;
	case NFCHR:
	  fa->mode |= NFSMODE_CHR;
	  break;
	case NFLNK:
	  fa->mode |= NFSMODE_LNK;
	  break;
	case NFSOCK:
	  fa->mode |= NFSMODE_SOCK;
	  break;
	}
	fa->nlink = 1;
	fa->uid = dp->d_uid;
	fa->gid = dp->d_gid;
	fa->size = dp->d_size;
	fa->blocksize = 512;
	fa->rdev = 0;
	fa->blocks = (dp->d_size + 511) / 512;
	fa->fsid = dp->d_dfs->dfs_dev;
	fa->fileid = dp->d_fno;
	fa->atime.seconds = dp->d_time;		
	fa->atime.useconds = 0;
	fa->mtime.seconds = dp->d_time;
	fa->mtime.useconds = 0;
	fa->ctime.seconds = dp->d_time;
	fa->ctime.useconds = 0;
	return (0);
}


/* 
 *-----------------------------------------------------------------------------
 * dos_setattr()
 * This routine is used to set file attributes 
 *-----------------------------------------------------------------------------
 */
int 	dos_setattr(dnode_t *dp, sattr *sa)
{
	int	archive_attrib = 0;

	if (sa->uid != -1)			
		dp->d_uid = (uid_t) UID;
	if (sa->gid != -1)		
		dp->d_gid = (gid_t) GID;

        if (sa->mode != -1 && !(sa->mode & WRITE_MODE))
                dp->d_mode = DEFAULTMODE;
        else    dp->d_mode = DEFAULTMODE;

	if (sa->mtime.seconds != -1)
		dp->d_time = sa->mtime.seconds;
	if (sa->size != -1 && dp->d_ftype != NFDIR && dp->d_size != sa->size) {
		archive_attrib = ATTRIB_ARCHIVE;
		dp->d_size = sa->size;
	}
	timeoutp = &timeout; 
	return (set_attribute(dp, archive_attrib));
}

/*
 *-----------------------------------------------------------------------------
 * dos_lookup()
 * This routine is used to find whether the specified file is one of the 
 * entries in the given directory. if so, allocate a dnode for it
 *-----------------------------------------------------------------------------
 */
int 	dos_lookup(dnode_t *dp, char *name, dnode_t **cdp)
{
	dfs_t	*dfs = dp->d_dfs;
	int	error;
	file_t	f;

	printf(" dos_lookup(): file = %s\n", name);

	if (strcmp(name, dot) == 0)
		return dos_getnode(dfs, dp->d_fno, cdp);
	if (strcmp(name, dotdot) == 0)
		return dos_getnode(dfs, dp->d_pfno, cdp);
	error = vfat_dir_getent_lname(dp, name, &f);
	if (error)
		return (error);
	error = dos_getnode(dfs, FNO(f.f_cluster, f.f_offset), cdp);
	if (error)
		return (error);
	(*cdp)->d_pfno = dp->d_fno;
	return (0);
}

/*
 *-----------------------------------------------------------------------------
 * dos_create()
 * This routine is used to create a file. A directory entry as well as
 * at least one cluster is allocated for the new file.
 *-----------------------------------------------------------------------------
 */
int 	dos_create(dnode_t *dp, char *name, sattr *sa, 
		   dnode_t **cdp, struct authunix_parms *aup)
{
        file_t  f;
	dfile_t	*df;
        dfs_t   *dfs;
        time_t  mtime;
        u_int   size;
	u_short	ccount, cluster;
	int	error, sflag;
	char	sname[MSDOS_NAME];

	printf(" dos_create(): file = %s\n", name);

	dfs = dp->d_dfs;
        if (strcmp(name, dot) == 0 || strcmp(name, dotdot) == 0)
                return (EACCES);
	/*
	 * Search parent directory to see if
	 * a file of this name already exists.
	 */
	error = vfat_dir_getent_lname(dp, name, &f);
	if (error == 0){
		/* 
		 * If file already exists, merely change attr 
		 * If file's a directory, error.
		 * If file's a read only file, error.
		 */
		if (IS_DIR(f))
			return (EACCES);
		if (IS_RDONLY(f))
			return (EACCES);

		error = dos_getnode(dfs, FNO(f.f_cluster, f.f_offset), cdp);
		if (error)
			return (error);
		(*cdp)->d_pfno = dp->d_fno;
		return (dos_setattr(*cdp, sa));
	}

	if (error != ENOENT){
		/* A file of this name exists */
		return (EEXIST);	
	}

	error = vfat_create_sname(dp, name, sname, &sflag);
	if (error == ERROR){
		/* Unable to map this long name to a short one */
		return (EINVAL);
	}	

	vfat_dirent_packname(&f, name, sname, sflag);
        vfat_dirent_print(&f);

	error = vfat_dir_freent(dp, f.f_nentries, &f.f_cluster, &f.f_offset);
	if (error){
		/* Unable to dig up enough space in directory file */
		return (ENOSPC);
	}

	if (sa->size == -1)
		size = 0;
	else	size = sa->size;
	ccount = (size + dfs->dfs_clustersize - 1) / dfs->dfs_clustersize;
	if (ccount == 0)
		ccount = 1;
	if (ccount > dfs->dfs_freeclustercount){
		/* Unable to accomodate this file */	
		return (ENOSPC);
	}
	cluster = vfat_free_fat(dfs);
	if (cluster == 0){
		/* Unable to allocate free cluster */
		return (ENOSPC);
	}

	error   = vfat_update_fat(dfs);
	if (error){
		/* Unable to update fat */
		return (EINVAL);
	}

	df = &f.f_entries[0];
	df->df_attribute = ATTRIB_ARCHIVE;
	if (sa->mode != -1 && !(sa->mode & WRITE_MODE))
		df->df_attribute |= ATTRIB_RDONLY;

	df->df_size[0]  = (size & 0xFF);
	df->df_size[1]  = (size >> 8)  & 0xFF;
	df->df_size[2]  = (size >> 16) & 0xFF;	
	df->df_size[3]  = (size >> 24) & 0xFF;
	
	df->df_start[0] = (cluster & 0xFF);
	df->df_start[1] = (cluster >> 8);

	mtime = time(NULL);
	to_dostime(df->df_date, df->df_time, &mtime);
	bzero(df->df_reserved, 10);

	error = vfat_dir_writent(dp, &f);
	if (error){
		/* Unable to write directory entry */
		return (EINVAL);
	}

	error = dos_getnode(dfs, FNO(f.f_cluster, f.f_offset), cdp);
	if (error){
		/* Unable to obtain a node from node cache */
		return (error);
	}

        (*cdp)->d_pfno = dp->d_fno;
        (*cdp)->d_uid  = UID;
        (*cdp)->d_gid  = GID;

	if (sa->mode != -1 && !(sa->mode & WRITE_MODE))
		(*cdp)->d_mode = DEFAULTMODE;
	else	(*cdp)->d_mode = DEFAULTMODE;

        dp->d_time = mtime;
        timeoutp = &timeout; 
        return (0);
}

/* 
 *-----------------------------------------------------------------------------
 * dos_remove()
 * This routine is used to delete a file. The directory entry associated with
 * this file is wiped out.
 *-----------------------------------------------------------------------------
 */
int     dos_remove(dnode_t *dp, char *name)
{
	int	error;
	file_t	f;
	dnode_t	*cdp;

	/*
	 * Don't trash "." and ".."
	 * Check to see that it's not a directory remove.
	 * Get node associated with this file entry.
	 * Delete this file entry from the parent directory.
	 */
	if (!strcmp(name, dot) || !strcmp(name, dotdot))
		return (EACCES);
	error = vfat_dir_getent_lname(dp, name, &f);
	if (error)
		return (ENOENT);

	if (IS_DIR(f))
		return (EISDIR);

	error = dos_getnode(dp->d_dfs, FNO(f.f_cluster, f.f_offset), &cdp);
	if (error)
		return (error);

	error = vfat_dir_delent(dp, &f);
	if (error){
		dos_putnode(cdp);
		return (error);
	}

	dos_purgenode(cdp);
	dp->d_time = time(NULL);
	timeoutp   = &timeout;
        return (0);
}

/*
 *-----------------------------------------------------------------------------
 * dos_mkdir()
 * This routine is used to create a directory.
 *-----------------------------------------------------------------------------
 */
int     dos_mkdir(dnode_t *dp, char *name, sattr *sa,
                  dnode_t **cdp, struct authunix_parms *aup)
{
	dfs_t	*dfs;
	file_t	f;
	dfile_t	*df;
	time_t	mtime;
	u_short	cluster;
	int	error, sflag;
        char    sname[MSDOS_NAME];

	printf(" dos_mkdir(): directory = %s\n", name);

	dfs = dp->d_dfs;
	if (!strcmp(name, dot) || !strcmp(name, dotdot))
		return (EACCES);

	error = vfat_dir_getent_lname(dp, name, &f);
	if (error != ENOENT){
		/* A file/directory of this name already exists */
		return (EEXIST);
	}

	error = vfat_create_sname(dp, name, sname, &sflag);
	if (error == ERROR){
		/* Unable to map this long name to a short one */
		return (EINVAL);
	}
        vfat_dirent_packname(&f, name, sname, sflag);

        error = vfat_dir_freent(dp, f.f_nentries, &f.f_cluster, &f.f_offset);
        if (error){
                /* Unable to dig up enough space in directory file */
                return (ENOSPC);
        }

        vfat_dirent_print(&f);

        cluster = vfat_free_fat(dfs);
	if (cluster == 0){
		/* Unable to allocate new cluster in fat */
		return (ENOSPC);
	}

	error = vfat_update_fat(dfs);
	if (error){
		/* Unable to update fat */
		return (EINVAL);
	}

        df = &f.f_entries[0];
	df->df_attribute = ATTRIB_DIR;
	df->df_size[0]   = 0;
	df->df_size[1]   = 0;
	df->df_size[2]   = 0;
	df->df_size[3]   = 0;
	df->df_start[0]  = (cluster & 0xFF);
	df->df_start[1]  = (cluster >> 8);
	
	mtime = time(NULL);
	to_dostime(df->df_date, df->df_time, &mtime);
	bzero(df->df_reserved, 10);
	
	error = vfat_dir_writent(dp, &f);
	if (error){
		/* Unable to write directory entry */
		return (EINVAL);
	}

	/* Create a "." and ".." in subdirectory */
	bzero(dfs->dfs_buf, dfs->dfs_clustersize);
	df  = (dfile_t *) dfs->dfs_buf;
	*df = f.f_entries[0];
	strncpy(df->df_name, dotname, 8);
	strncpy(df->df_ext,  nullext, 3);

	df++;
	*df = f.f_entries[0];
	strncpy(df->df_name, dotdotname, 8);
	strncpy(df->df_ext,  nullext, 3);
	
	if (dp->d_fno == ROOTFNO){
		df->df_start[0] = 0;
		df->df_start[1] = 0;
	}
	else {
		df->df_start[0] = (dp->d_start & 0xFF);
		df->df_start[1] = (dp->d_start >> 8);
	}

	error = vfat_clust_writ(dfs, DISK_ADDR(dfs, cluster), dfs->dfs_buf);
	if (error){
		/* Unable to write directory cluster */
		return (EINVAL);
	}

	error = dos_getnode(dfs, FNO(f.f_cluster, f.f_offset), cdp);
	if (error){
		/* Unable to obtain a node from node cache */
		return (error);
	}

        (*cdp)->d_pfno = dp->d_fno;
        (*cdp)->d_uid = UID;
        (*cdp)->d_gid = GID;
        (*cdp)->d_mode = (sa->mode != (u_int)-1) ? sa->mode : DEFAULTMODE;

        dp->d_time = mtime;
        timeoutp   = &timeout;  
        return (0);
}

/*
 *-----------------------------------------------------------------------------
 * dos_rmdir()
 * This routine is used to remove a directory.
 *-----------------------------------------------------------------------------
 */
int     dos_rmdir(dnode_t *dp, char *name)
{
        file_t  f;
        dfile_t *df;
	dnode_t	*cdp;
        dfs_t   *dfs;
        time_t  mtime;
        u_int   size;
        u_short ccount, cluster;
        int     i, entry_count, error;

	dfs = dp->d_dfs;
	cdp = NULL;
	printf("dos_rmdir()");
	if (!strcmp(name, dot) || !strcmp(name, dotdot))
		return (EACCES);

	error = vfat_dir_getent_lname(dp, name, &f);
	if (error == ENOENT){
		/* Unable to find specified directory */
		return (ENOENT);
	}	

	if (!IS_DIR(f)){
		/* Unable to find specified directory */
		return (ENOTDIR);
	}

	error = dos_getnode(dp->d_dfs, FNO(f.f_cluster, f.f_offset), &cdp);
	if (error){
		/* Unable to find node in node cache */
		return (error);
	}

	cdp->d_pfno = dp->d_fno;
	if (cdp->d_dir == NULL){
		error = vfat_dir_read(cdp);
		if (error)
			goto rmdir_done;
	}
	/*
	 * See if directory is empy or not.
	 * Entries "." and ".." are allowed.
	 */
	for (i = cdp->d_size / DIR_ENTRY_SIZE, df = cdp->d_dir, entry_count = 0;
             i > 0; i--, df++) {
		if (df->df_name[0] == LAST_ENTRY)
                        break;
                if (df->df_name[0] == REUSABLE_ENTRY)
                        continue;
                if (++entry_count > 2) {    
                        error = ENOTEMPTY;
                        goto rmdir_done;
                }
	}

	error = vfat_dir_delent(dp, &f);
	if (error){
		/* Unable to delete directory entry */
		goto rmdir_done;
	}

	dos_purgenode(cdp);
	cdp = NULL;

	dp->d_time = time(NULL);

rmdir_done:
	if (cdp)
		dos_putnode(cdp);
	timeoutp = &timeout;
	return (error);
}

/* 
 *-----------------------------------------------------------------------------
 * dos_readdir()
 * This routine is used to read a directory 
 *-----------------------------------------------------------------------------
 */
int 	dos_readdir(dnode_t *dp, nfscookie cookie, u_int count, 
			entry *entries, u_int *eof, u_int *nread)
{
	dfs_t	*dfs;
	file_t	f;
	dfile_t	*df;
	u_short	cluster;
	entry	*ent, *o_entries;	
	int	entsize, error;
	u_int	i, limit, offset;
	char	sname[MSDOS_NAME];
	char	lname[NFS_MAXNAMLEN+1];

	printf("dos_readdir()");
	ent = NULL;
	dfs = dp->d_dfs;
	o_entries = entries;
	offset = *(u_int *) cookie;
	limit  = dp->d_size/DIR_ENTRY_SIZE;
	*eof = 0;
	
	if (offset >= limit){
		/* No entries could be read */
		*nread = 0;
		*eof = 1;
		return (0);
	}
	if (dp->d_dir == NULL){
		error = vfat_dir_read(dp);
		if (error)
			return (error);
	}
	df = &dp->d_dir[offset];
	/*
	 * For root cluster, we only look at this cluster.
	 * For other clusters, we look at all clusters in dir.
	 */
	if (dp->d_fno == ROOTFNO)
		cluster = ROOT_CLUSTER;
	else	cluster = to_cluster(dp, offset);
	/*
	 * Start scanning all the directory entries in dir.
	 */
	while (count > 0 && offset < limit){
		if (df->df_name[0] == LAST_ENTRY) {
			*eof = 1;
			break;
		}
		if (df->df_name[0] == REUSABLE_ENTRY || IS_LABEL(df)){
			/* We've encountered either a blank */
			/* directory entry or volume label */
			df++;
			offset++;
			if (cluster != ROOT_CLUSTER){
				if (offset % DIR_ENTRY_PER_CLUSTER(dfs) == 0)
				    cluster = (*dfs->dfs_readfat)(dfs, cluster);
			}
			continue;
		}	
		f.f_offset  = offset;
		f.f_cluster = cluster;
		copy_dir_to_fp(dp, &f);
		/*
		 * We've just read in one directory entry into "f".
		 * Position df correctly to point to next entry.
		 * Position offset correctly to point to next entry.
		 * Unpack the lname/sname from "f". Convert (name, ext)
		 * to unix format sname.
		 */
		df     = f.f_nentries+df+1;
		offset = f.f_nentries+offset+1;
		vfat_dirent_upackname(&f, lname, sname);
		vfat_dirent_print(&f);
		to_unixname(f.f_entries[0].df_name, 
			    f.f_entries[0].df_ext, sname);
		entsize = sizeof(entry)+RNDUP(strlen(lname)+4);
		if (count < entsize)	
			break;
		count = count-entsize;
		ent   = entries;
		entries = (entry *) ((char *) ent+entsize);
		if (!strcmp(lname, dot))
			ent->fileid = dp->d_fno;
		else if (!strcmp(lname, dotdot))
			ent->fileid = dp->d_pfno;
		else	ent->fileid = FNO(cluster, offset)-1;
		ent->name = strcpy((char *) ent+sizeof(entry), lname);
		ent->nextentry = entries;
		*(u_int *) ent->cookie = offset;
		/*
		 * Proceed to the next cluster if
		 * we're not looking at the root dir.
		 */
		if (cluster != ROOT_CLUSTER){
			if (offset % DIR_ENTRY_PER_CLUSTER(dfs) == 0)
				cluster = (*dfs->dfs_readfat)(dfs, cluster);
		}
	}
	/* Calculate num. bytes read */
	*nread = (char *) entries - (char *) o_entries; 
	if (ent)
		ent->nextentry = 0;
        return (0);
}




/* symbolic link not implemented by DOS */
int 	dos_readlink(dnode_t *dp, u_int count, char *data)
{
	return ENXIO;
}



/*
 *-----------------------------------------------------------------------------
 * dos_read()
 *-----------------------------------------------------------------------------
 */
int 	dos_read(dnode_t *dp, u_int offset, u_int count, 
		 char *data, u_int *nread)
{
        u_int   bcount;
        u_short cluster;
        dfs_t   *dfs;
        int     head;
        int     i, j;
        u_short ncluster;
        int 	clusteraddr, rcode;

	dfs = dp->d_dfs;
        /* 
	 * Make sure there is something to read 
	 */
        if (offset >= dp->d_size) {
                *nread = 0;
                return 0;
        }
        /* 
	 * Don't try to read pass end of file 
	 */
        if (offset + count > dp->d_size)
                count = dp->d_size - offset;
        /* 
	 * Skip to the right cluster 
	 */
        cluster = dp->d_start;
        for (i = offset / dfs->dfs_clustersize; i > 0; i--)
                cluster = (*dfs->dfs_readfat)(dfs, cluster);
	*nread = 0;
        /* 
	 * Take care of non-cluster-aligned read 
	 */
        if ((head = offset % dfs->dfs_clustersize) != 0) {
                clusteraddr = DISK_ADDR(dfs, cluster);
                if ((rcode = verify_cache(dfs, clusteraddr, CACHE_READ)) != 0)
                        return (rcode);
                bcount = dfs->dfs_clustersize - head;
                if (count < bcount)
                        bcount = count;
                bcopy(&trkcache.bufp[clusteraddr-trkcache.trkbgnaddr+head],
                      &data[*nread], bcount);
                count -= bcount;
                *nread += bcount;
                cluster = (*dfs->dfs_readfat)(dfs, cluster);
        }
        while (count != 0) {
                clusteraddr = DISK_ADDR(dfs, cluster);
                if ((rcode = verify_cache(dfs, clusteraddr, CACHE_READ)) != 0)
                        return (rcode);
                if (count < dfs->dfs_clustersize) {
                        bcopy(&trkcache.bufp[clusteraddr-trkcache.trkbgnaddr],
                              &data[*nread], count);
                        *nread += count;
                        break;
                }
                bcopy(&trkcache.bufp[clusteraddr-trkcache.trkbgnaddr],
                      &data[*nread], dfs->dfs_clustersize);
                count  -= dfs->dfs_clustersize;
                *nread += dfs->dfs_clustersize;
                cluster = (*dfs->dfs_readfat)(dfs, cluster);
        }
	return (0);
}

/*
 *-----------------------------------------------------------------------------
 * dos_write()
 *-----------------------------------------------------------------------------
 */
int 	dos_write(dnode_t *dp, u_int offset, u_int count, char *data)
{
        u_int   bcount;
        u_short cluster, ncluster;
        dfs_t   *dfs;
	int	i, j;
        int     error, head;
        int   clusteraddr, rcode;

	
	dfs = dp->d_dfs;
        /* 
	 * Expand the file if necessary 
	 */
        if (offset + count > dp->d_size)
                dp->d_size = offset + count;
        dp->d_time = time(NULL);
        if (error = set_attribute(dp, ATTRIB_ARCHIVE))
                return error;
        /* 
	 * Skip to the right cluster 
	 */
        cluster = dp->d_start;
        for (i = offset / dfs->dfs_clustersize; i > 0; i--)
                cluster = (*dfs->dfs_readfat)(dfs, cluster);
        /* 
	 * Take care of non-cluster-aligned write 
	 */
        if ((head = offset % dfs->dfs_clustersize) != 0) {
                bcount = dfs->dfs_clustersize - head;
                if (count < bcount)
                        bcount = count;
                clusteraddr = DISK_ADDR(dfs, cluster);
                if ((rcode = verify_cache(dfs, clusteraddr, CACHE_WRITE)) != 0)
                        return rcode;
                bcopy(&data[0],
                        &trkcache.bufp[clusteraddr-trkcache.trkbgnaddr+head],
                        bcount);
                set_dirty(clusteraddr);
                count -= bcount;
                offset = bcount;
                cluster = (*dfs->dfs_readfat)(dfs, cluster);

        } else  offset = 0;
        bcount = dfs->dfs_clustersize;
        while (count != 0) {
                clusteraddr = DISK_ADDR(dfs, cluster);
                if ((rcode = verify_cache(dfs, clusteraddr, CACHE_WRITE)) != 0)
                        return rcode;
                if (count < dfs->dfs_clustersize) {
                        bcopy(&data[offset],
                                &trkcache.bufp[clusteraddr-trkcache.trkbgnaddr],                                count);
                        set_dirty(clusteraddr);
                        break;
                }
                bcopy(&data[offset],
                      &trkcache.bufp[clusteraddr-trkcache.trkbgnaddr],
                      bcount);
                set_dirty(clusteraddr);
                cluster = (*dfs->dfs_readfat)(dfs, cluster);
                count  -= bcount;
                offset += bcount;
        }
        timeoutp = &timeout; 
	return (0);
}

/* 
 *-----------------------------------------------------------------------------
 * dos_rename()
 * This routine is used to rename a file.
 *-----------------------------------------------------------------------------
 */
int     dos_rename(dnode_t *dp, char *name, dnode_t *tdp, char *tname)
{
        file_t  f;
        file_t  tf;
        dfile_t *df;
        dfs_t   *dfs;
        u_long  fno;
        u_long  tfno;
        time_t  mtime;
        dnode_t *cdp = NULL;
        dnode_t *ctdp = NULL;
        dfile_t dfile;
        int     error, sflag;
        int     target_file_exists;
        char    sname[MSDOS_NAME];

        printf(" dos_rename(): file1 = %s, file2 = %s\n", name, tname);

        dfs = dp->d_dfs;
        if (!strcmp(name, dot)  || !strcmp(name, dotdot) ||
            !strcmp(tname, dot) || !strcmp(tname, dotdot))
                return (EACCES);

        if (!strcmp(name, tname) && dp == tdp)
                return (ENOTUNIQ);

        error = vfat_dir_getent_lname(dp, name, &f);
        if (error == ENOENT){
                /* Unable to get file entry for "name" */
                return (ENOENT);
        }

        error = vfat_dir_getent_lname(tdp, tname, &tf);
        if (error == 0){
                /* 
                 * Target file exists 
                 */
                if (IS_DIR(tf)){
                        /* This target file is a directory */   
                        return (EACCES);
                }
                /* 
                 * Delete target file 
                 */
                tfno   = FNO(tf.f_cluster, tf.f_offset);                
                error = dos_getnode(dfs, tfno, &ctdp);
                if (error)
                        return (error);

                ctdp->d_pfno = tdp->d_fno;
                
                error = vfat_dir_delent(tdp, &tf);
                if (error)
                        goto rename_done;

                dos_purgenode(ctdp);
        }
	else if (error != ENOENT){
		/*
		 * We couldn't obtain the file entry associated
		 * with "tname", but we ran into some other error.
		 */
		return (error);
	}
        /*
         * Create short name for this new name.
         * Pack the old attributes into this file entry.
         */
        error = vfat_create_sname(tdp, tname, sname, &sflag);
        if (error){
                /* Unable to map this long name to a short one */
                return (EINVAL);
        }

        vfat_dirent_packname(&tf, tname, sname, sflag);
        vfat_copy_attributes(&f, &tf);
        /*
         * Obtain free entries in parent directory
         */
        error = vfat_dir_freent(tdp, tf.f_nentries, &tf.f_cluster,&tf.f_offset);
        if (error){
                /* Unable to obtain free entries in parent directory */
                return (ENOSPC);
        }
        
	vfat_dirent_print(&tf);

        df = tf.f_entries;
        bzero(df->df_reserved, 10);

        if (!IS_DIR(f))
                df->df_attribute |= ATTRIB_ARCHIVE;

        fno   = FNO(f.f_cluster, f.f_offset);
        error = dos_getnode(dfs, fno, &cdp);
        if (error){
                /* Unable to obtain node from node cache */
                return (error);
        }
        cdp->d_pfno = dp->d_fno;
        /*
         * Write file entry corresponding to new name
         */
        error = vfat_dir_writent(tdp, &tf);
        if (error){
                /* Unable to write directory entry */
                goto rename_done;
        }

        tfno  = FNO(tf.f_cluster, tf.f_offset);
        error = dos_getnode(dfs, tfno, &ctdp);
        if (error){
                /* Unable to obtain node from node cache */
                goto rename_done;
        }

        ctdp->d_pfno = tdp->d_fno;
        ctdp->d_uid  = cdp->d_uid;
        ctdp->d_gid  = cdp->d_gid;
        ctdp->d_mode = cdp->d_mode;
        /* 
         * Nullify file entry corresponding to old name 
         */
        error = vfat_dir_nulent(dp, &f);
        if (error){
                /* Unable to nullify old file entry */
                goto rename_done;
        }

        dos_purgenode(cdp);
        cdp = NULL;
        
        dp->d_time = time(NULL);
        if (dp != tdp)
                tdp->d_time = dp->d_time;
rename_done:
        if (cdp)        
                dos_putnode(cdp);
        if (ctdp)
                dos_putnode(ctdp);
        timeoutp = &timeout;
        return (error);
}

/* link not implemented by DOS */
int 	dos_link(dnode_t *dp, dnode_t *tdp, char *name)
{
	return ENXIO;
}

/* symbolic link not implemented by DOS */
int 	dos_symlink(dnode_t *dp, char *name, sattr *sa, char *path)
{
	return ENXIO;
}

