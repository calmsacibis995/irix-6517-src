/*
 *-----------------------------------------------------------------------------
 * DOS filesystem-level routines.
 *-----------------------------------------------------------------------------
 */
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/smfd.h>
#include <sys/stat.h>
#include "dos_fs.h"
#include <sys/major.h>
#include <sys/dvh.h>
#include <sys/dkio.h>
#include <sys/conf.h>
#include <invent.h>
#include <sys/iograph.h>
#include <string.h>
#define	SOFTPC_DISK	0xff00

static 	dfs_t *dfs_list;
dbufp_t trkcache;        

/*
 * NOTE: bp_sectorcount is a u_short. Type 6 partitions have sector
 * counts that are 4 bytes long and have to be accomodated elsewhere.
 * I'm storing them in "sectorcount", which is a global.
 */
u_int	sectorcount	= 0;	/* sector count for partition */
u_long	partsize 	= 0;	/* Size of partition */
u_long	partbgnaddr	= 0;	/* Beginning address of partition */
u_long	partendaddr	= 0;	/* Ending address of partition */
u_short	fat_entry_size	= 0;	/* Fat entry size: 12/16 */

extern int remmedinfo( int fd, int *status ); /* declared in libdisk */

dfs_t *
dos_getfs(dev_t dev)
{
	dfs_t *dfs;
	int status;
	char            devname[MAXDEVNAME];
	int             devnmlen = MAXDEVNAME;


	for (dfs = dfs_list; dfs != NULL; dfs = dfs->dfs_next) {
		if (dfs->dfs_dev == dev) {
			if (dfs->dfs_dev == SOFTPC_DISK)
				return dfs;
			if(dev_to_drivername(dfs->dfs_dev,
					     devname,
					     &devnmlen)) 
				if(strncmp(devname, "dksc", 4) == 0)
					return dfs;

			if ((ioctl(dfs->dfs_fd, SMFDMEDIA, &status) && 
                            remmedinfo( dfs->dfs_fd, &status))
			    || status == 0) {		/* no media */
				dfs->dfs_flags |= DFS_STALE;
				return NULL;
			}
			if (status & SMFDMEDIA_CHANGED 
			    || dfs->dfs_flags & DFS_STALE) {
			    /* New media -- kill this filesystem off. */
				dos_removefs(dfs);
				return NULL;
			}
			return dfs;
		}
	}

	return NULL;
}

static void
dos_destroyfs(dfs_t *dfs)
{
	close(dfs->dfs_fd);
	if (dfs->dfs_fat)
		free(dfs->dfs_fat);
	if (dfs->dfs_root)
		dos_putnode(dfs->dfs_root);
	if (dfs->dfs_buf)
		free(dfs->dfs_buf);
	if (dfs->dfs_zerobuf)
		free(dfs->dfs_zerobuf);
	free(dfs);
}

int
dos_addfs(char *pathname, u_int flags, u_int partition, dfs_t **dfsp)
{
	dfs_t 		*dfs;
	int 		error;
	int 		fd;
	int		part;
	struct stat 	sb;
	u_int		status;
	u_int		capacity;
	struct		volume_header vh;
	struct		partition_table *pt;

	if ((fd = open(pathname, O_RDWR | O_EXCL)) < 0)
		return errno;
	if (fstat(fd, &sb) < 0) {
		close(fd);
		return errno;
	}
	if (ioctl(fd, DIOCGETVH, &vh) == 0 && valid_vh(&vh)){
	    char	devname[MAXDEVNAME];
	    char	*pattern = EDGE_LBL_VOLUME"/"EDGE_LBL_CHAR;
	    int		len1 = strlen(pattern),len2 = MAXDEVNAME;
	    char	*str;

	    /* get the canonical name of this device */
	    dev_to_devname(sb.st_rdev,devname,&len2);
	    len2 = strlen(devname);
	    str = devname + (len2 - len1);
	    /* check if the volume partition is being used */
	    if (strcmp(pattern,str) == 0) {
		part = 10;	/* We have a volume partition */
		pt = &vh.vh_pt[part];
	    }
	    else
		part = -1;

            /* 
	     * only allow access to the whole-volume device. 
	     */
            if (part != 10) {
                close(fd);
                return ENODEV;
            }
            if (ioctl(fd, DIOCREADCAPACITY, &capacity) != 0) {
                perror("Error reading capacity");
                close(fd);
                return errno;
            }
            bzero(&vh, sizeof vh);
            vh.vh_magic 	 = VHMAGIC;
	    vh.vh_dp.dp_drivecap = capacity;
	    vh.vh_dp.dp_secbytes = SECTOR_SIZE;
            pt->pt_nblks 	 = vh.vh_dp.dp_drivecap;
            pt->pt_firstlbn 	 = 0;
            pt->pt_type 	 = PTYPE_RAW;
            vh.vh_csum 		 = -vhchksum(&vh);
            (void) ioctl(fd, DIOCSETVH, &vh); 
        }
	dfs = (dfs_t *)safe_malloc(sizeof *dfs);
	bzero(dfs, sizeof *dfs);
	dfs->dfs_pathname = pathname;
	dfs->dfs_fd = fd;
	dfs->dfs_dev = S_ISREG(sb.st_mode) ? SOFTPC_DISK : sb.st_rdev;
	dfs->dfs_flags = flags;
	if (error = dos_getvh(dfs, partition)) {
		dos_destroyfs(dfs);
		return error;
	}
	dfs->dfs_next = dfs_list;
	dfs_list = dfs;
	*dfsp = dfs;
	return 0;
}

void
dos_removefs(dfs_t *dfs)
{
	dfs_t **dfsp;

	for (dfsp = &dfs_list; *dfsp != NULL; dfsp = &(*dfsp)->dfs_next) {
		if (*dfsp == dfs) {
			*dfsp = dfs->dfs_next;
			dos_putnode(dfs->dfs_root);
			dfs->dfs_root = NULL;
			dos_purgenodes(dfs);
			dos_destroyfs(dfs);
			return;
		}
	}
	panic("dos_removefs: fs %s not found", dfs->dfs_pathname);
}

int
dos_statfs(dfs_t *dfs, statfsokres *sfr)
{
	sfr->tsize = NFS_MAXDATA;		/* max transfer size */
	sfr->bsize = dfs->dfs_clustersize;	/* block size */
	sfr->blocks = dfs->dfs_clustercount;	/* blocks/disk */
	sfr->bfree = dfs->dfs_freeclustercount;	/* blocks free */
	sfr->bavail = sfr->bfree;		/* blocks available */
	return 0;
}

int dos_getvh(dfs_t *dfs, u_int partition)
{
	int		i, error;
	u_int		pd_offset;
	u_int		filesarea_sectors;
	u_char		partition_type;
	bp_t		*bpp;
	u_int 		bp_buf[SECTOR_SIZE / sizeof(u_int)];
	u_char 		*bs = (u_char *)bp_buf;
	u_int		msdos_boot_version;
	int		rootdir_sectors;
	int		numclusters_by_volspace;

	lseek(dfs->dfs_fd, 0, 0);

	if (partition) {
		u_long	parttbladdr = 0;

		if (partition > 4){
                        error = dos_find_extended(dfs, partition);
                        if (error){
                                fprintf(stderr, " Invalid logical partn\n");
                                return (ENXIO);
                        }
                        parttbladdr = lseek(dfs->dfs_fd, 0, SEEK_CUR);
		}
		if (read(dfs->dfs_fd, bs, sizeof(bp_buf)) < 0){
			fprintf(stderr, " read() fail!\n");
			return errno;
		}
		if (!MAGIC_PART(bs)){
			fprintf(stderr, " Bad Magic: Partition table\n");
                        return (EINVAL);
                }
                if (partition > 4)
                        pd_offset = PD_OFFSET;
                else    pd_offset = PD_OFFSET+(partition-1)*PD_SIZE;
		partition_type = bs[pd_offset + 4];
		switch (partition_type){
		  case PARTITION_NOTUSED:
			fprintf(stderr, " Partition %d is unused\n", partition);
			return (ENXIO);
		  case PARTITION_FAT12:
			fat_entry_size = 12;
			break;
		  case PARTITION_FAT16:
		  case PARTITION_HUGE: /* well, since we only support FAT16 */
			fat_entry_size = 16;
			break;
		  case PARTITION_EXTENDED:
			fprintf(stderr, " Cannot mount extended partn\n");
			fprintf(stderr, " Please mount logical  partn\n");
			return (ENXIO);
		  default:
			fprintf(stderr, " Unknown partition type\n");
			return (ENXIO);
		}
                if (partition > 4)
                        partbgnaddr = parttbladdr+PARTN1_OFFSET(bs);
                else    partbgnaddr = PARTN_OFFSET(bs, partition);
                lseek(dfs->dfs_fd, partbgnaddr, SEEK_SET);
	}
	else {
		partbgnaddr = 0;
		partition_type = PARTITION_NOTUSED;
	}

	/*
	 * OK, now read the boot sector parameter block, from wherever
	 * we found it, ie, in the master location or at the start of
	 * one of the partitions if one was specified.
	*/

	if (read(dfs->dfs_fd, bs, sizeof(bp_buf)) < 0){
		fprintf(stderr, " read of boot sector() fail!\n");
		return errno;
	}
	if (!MAGIC_DOS(bs)){
		fprintf(stderr, " Bad Magic: DOS Boot Sector\n");
		return (EINVAL);
	}
	bpp = &dfs->dfs_bp;
	bpp->bp_magic[0] = bs[VH_MAGIC0];
	bpp->bp_magic[1] = bs[VH_MAGIC1];
	bpp->bp_magic[2] = bs[VH_MAGIC2];
	bpp->bp_oem[0]   = bs[VH_OEM0];
	bpp->bp_oem[1]   = bs[VH_OEM1];
	bpp->bp_oem[2]   = bs[VH_OEM2];
	bpp->bp_oem[3]   = bs[VH_OEM3];
	bpp->bp_oem[4]   = bs[VH_OEM4];
	bpp->bp_oem[5]   = bs[VH_OEM5];
	bpp->bp_oem[6]   = bs[VH_OEM6];
	bpp->bp_oem[7]   = bs[VH_OEM7];

	/*
	 * Determine MSDOS boot sector version number right now.
	 * Availability of certain fields are dependent.
	*/
	for (i = 7; i && bpp->bp_oem[i] != '.'; i--)
	  ;
	msdos_boot_version = (isdigit(bpp->bp_oem[--i])) ? bpp->bp_oem[i] - '0' : 0;

	sectorcount = SECTOR_COUNT(bs); /* good in helping determine version */
	if (!msdos_boot_version) {
		/*
		 * Well, not too unexpected, let's look for other things
		 * that might help us determine what the heck it is.
		*/
		if (!sectorcount && SECTOR_COUNT_EXTD(bs)) {
			msdos_boot_version = 4;
		}
		if (!msdos_boot_version && MAGIC_EXTENDED_BOOT(bs)) {
			msdos_boot_version = 4;
		}
	}

	bpp->bp_sectorsize        = (bs[VH_SSHI] << 8) | bs[VH_SSLO];
	if (bpp->bp_sectorsize && bpp->bp_sectorsize != SECTOR_SIZE) {
		fprintf(stderr, " Unsupported sector size %d\n", bpp->bp_sectorsize);
		return (EINVAL);
	}
	else {
		bpp->bp_sectorsize = SECTOR_SIZE;
	}

	bpp->bp_sectorspercluster = (bs[VH_CS]);
	bpp->bp_reservecount      = (bs[VH_RCHI] << 8) | bs[VH_RCLO];
	bpp->bp_fatcount          = (bs[VH_FC]);
	bpp->bp_rootentries       = (bs[VH_REHI] << 8) | bs[VH_RELO];

        if (partition_type == 6 || (msdos_boot_version >= 4 && !sectorcount)) {
                sectorcount = SECTOR_COUNT_EXTD(bs);
	}

	partsize    = sectorcount * bpp->bp_sectorsize;
	partendaddr = partbgnaddr + partsize;
	bpp->bp_mediatype 	= (bs[VH_MT]);
	bpp->bp_fatsize 	= (bs[VH_FSHI] << 8) | bs[VH_FSLO];
	bpp->bp_sectorspertrack = (bs[VH_STHI] << 8) | bs[VH_STLO];
	bpp->bp_headcount 	= (bs[VH_HCHI] << 8) | bs[VH_HCLO];

	bpp->bp_hiddensectors 	= (bs[VH_HSLHI] << 8) | (bs[VH_HSLLO]);
	if (msdos_boot_version >= 4) {
		bpp->bp_hiddensectors |= (bs[VH_HSHHI] << 24) | (bs[VH_HSHLO] << 16);
	}

	if (partition) {
		/*
		 * If we've found this boot sector as a result of an indirect through
		 * the partition table/MBR, make sure that the media type code represents
		 * a fixed disk, which seems to be a requirement in the DOS world.
		 *
		 * Ie, a partition boot sector can't contain a boot sector ordinarily
		 * found as the boot sector on a floppy.  Just a sanity check.
		*/
		if (bpp->bp_mediatype != DOS_MEDIA_FIXED_DISK) {
			fprintf(stderr, " Invalid DOS Boot Sector in partition %d\n",
				partition);
			return (EINVAL);
		}
	}
	rootdir_sectors = (bpp->bp_rootentries * DIR_ENTRY_SIZE + bpp->bp_sectorsize - 1)
			 / bpp->bp_sectorsize;
	filesarea_sectors = sectorcount - bpp->bp_reservecount - rootdir_sectors -
		  bpp->bp_fatsize * bpp->bp_fatcount;
	/*
	 * Fill in values into the dfs
	 * structure about boot/fat/root
	 */
	dfs->dfs_fataddr     = partbgnaddr + bpp->bp_reservecount * bpp->bp_sectorsize;
	dfs->dfs_fatsize     = bpp->bp_fatsize * bpp->bp_sectorsize;
	dfs->dfs_rootaddr    = dfs->dfs_fataddr + bpp->bp_fatcount * dfs->dfs_fatsize;
	dfs->dfs_rootsize    = bpp->bp_rootentries * DIR_ENTRY_SIZE;
	dfs->dfs_fileaddr    = dfs->dfs_rootaddr + dfs->dfs_rootsize;
	dfs->dfs_clustersize = bpp->bp_sectorspercluster * bpp->bp_sectorsize;
#if 0
	/*
	 * Debug printout
	 */
	printf(" mount_dos: dfs_fataddr    = %d\n", dfs->dfs_fataddr);
	printf(" mount_dos: dfs_rootaddr   = %d\n", dfs->dfs_rootaddr);
	printf(" mount_dos: dfs_fileaddr   = %d\n", dfs->dfs_fileaddr);
	printf(" mount_dos: dfs_clustersize= %d\n", dfs->dfs_clustersize);
#endif
	/*
	 * Allocate space for and copy in FAT, use the 
	 * secondary copies if necessary
	 */
	dfs->dfs_fat = safe_malloc(dfs->dfs_fatsize);
	lseek(dfs->dfs_fd, dfs->dfs_fataddr, 0);
	for (i = 0; i < bpp->bp_fatcount; i++) {
		if (read(dfs->dfs_fd, dfs->dfs_fat, dfs->dfs_fatsize) > 0)
			break;
		if (i == bpp->bp_fatcount - 1) 
			return errno;
		lseek(dfs->dfs_fd,
		      dfs->dfs_fataddr + dfs->dfs_fatsize * (i + 1), 0);
	}
	/* Add space for . and .. and get root dnode */
	dfs->dfs_rootsize   += DIR_ENTRY_SIZE * 2;
	bpp->bp_rootentries += 2;
	if (error = dos_getnode(dfs, ROOTFNO, &dfs->dfs_root))
		return error;
	/* Allocate space for scratch work */
	dfs->dfs_buf = safe_malloc(dfs->dfs_clustersize);
	dfs->dfs_zerobuf = safe_malloc(dfs->dfs_clustersize);
	bzero(dfs->dfs_zerobuf, dfs->dfs_clustersize);

	numclusters_by_volspace = filesarea_sectors / bpp->bp_sectorspercluster;
	if (!fat_entry_size) {
		fat_entry_size = (numclusters_by_volspace > 4087) ? 16 : 12;
	}

	/*
	 * Find out number of usable entries in FAT, the first 2 entries in
	 * the FAT are reserved.  Determine the cluster count as well by looking
	 * at the FAT.  Then compare it against the cluster count as determined by
	 * looking at the number of sectors in the files area in conjunction with
	 * the sectors per cluster.  Take the best fit.
	 */
	dfs->dfs_clustercount = dfs->dfs_fatsize * 8 / fat_entry_size - 2;
	if (dfs->dfs_clustercount > numclusters_by_volspace) {
		dfs->dfs_clustercount = numclusters_by_volspace;
	}

	if (fat_entry_size == 12) {
		dfs->dfs_readfat  = readfat12;
		dfs->dfs_writefat = writefat12;
		dfs->dfs_endclustermark = 0xfff;
	} else {
		dfs->dfs_readfat = readfat16;
		dfs->dfs_writefat = writefat16;
		dfs->dfs_endclustermark = 0xffff;
	}

	/* Find out number of free clusters on disk */
	dfs->dfs_freeclustercount = 0;
	for (i = 2; i < dfs->dfs_clustercount + 2; i++) {
		if ((*dfs->dfs_readfat)(dfs, i) == 0)
			dfs->dfs_freeclustercount++;
	}
	/* Initialize the cache buffer parameters */
	trkcache.clustersize = bpp->bp_sectorspercluster * bpp->bp_sectorsize;
	if (fat_entry_size == 12){
		trkcache.trksize = bpp->bp_headcount*bpp->bp_sectorsize*
				   bpp->bp_sectorspertrack;
	}
	else {
		trkcache.trksize = bpp->bp_sectorspertrack * bpp->bp_sectorsize;
                trkcache.trksize = (trkcache.trksize/dfs->dfs_clustersize)*
                                   (dfs->dfs_clustersize);
		if (!trkcache.trksize)
			trkcache.trksize = 5*dfs->dfs_clustersize;
	}
	trkcache.clusterspertrk = trkcache.trksize/trkcache.clustersize;
	trkcache.trkbgnaddr  = 0;
	trkcache.trkendaddr  = 0;
	trkcache.bufp = safe_malloc(trkcache.trksize);
	trkcache.dirtymaskcnts = (trkcache.clusterspertrk+(MASKBYTESZ-1)) /
         							MASKBYTESZ;
	trkcache.dirty = (u_long *)
         			safe_malloc(trkcache.dirtymaskcnts*MASKBYTES);
	for (i = 0; i < trkcache.dirtymaskcnts; i++)
		trkcache.dirty[i] = 0;
	dfs->dirtyfat = FALSE;
	dfs->dirtyrootdir = FALSE;
   	return 0;
}

/*
 *----------------------------------------------------------------------------
 * dos_find_extended()
 * This routine scans a hard-drive and searches for a required logical
 * partition and positions the file descriptor at the very beginning.
 *----------------------------------------------------------------------------
 */
int     dos_find_extended(dfs_t *dfs, u_int partition)
{
        int     partn;
        u_char  p_type;
        u_long  extaddr, nextaddr;
	u_long	parttbladdr;
        u_char  bs[SECTOR_SIZE];

        lseek(dfs->dfs_fd, 0, SEEK_SET);
        read(dfs->dfs_fd, bs, SECTOR_SIZE);
        for (partn = 1; partn <= 4; partn++){
                p_type = PARTN_TYPE(bs, partn);
                if (p_type == EXTENDED)
                        break;
        }
        if (partn == 5)
                return (1);
        extaddr = PARTN_OFFSET(bs, partn);
        partn = 5;
        parttbladdr = extaddr;
        lseek(dfs->dfs_fd, parttbladdr, SEEK_SET);
        read(dfs->dfs_fd, bs, SECTOR_SIZE);
        while (partn != partition){
                nextaddr = PARTN2_OFFSET(bs);
                if (nextaddr == 0)
                        goto error;
                partn++;
                parttbladdr = extaddr+nextaddr;
                lseek(dfs->dfs_fd, parttbladdr, SEEK_SET);
                read(dfs->dfs_fd, bs, SECTOR_SIZE);
        }
        if (partn == partition){
                lseek(dfs->dfs_fd, parttbladdr, SEEK_SET);
                return (0);
        }
error:
        return (1);
}

/*
 * DOS filesystem node cache.
 */
typedef struct dlist {
	dnode_t	*d_forw;	/* hash table linkage */
	dnode_t	*d_back;
} dlist_t;

#define	INSERT_HASH(dp, hashlist) \
	((dp)->d_forw = (hashlist)->d_forw, \
	 (dp)->d_back = (dnode_t *)(hashlist), \
	 (dp)->d_forw->d_back = (dp)->d_back->d_forw = (dp))

#define	REMOVE_HASH(dp) \
	((dp)->d_forw->d_back = (dp)->d_back, \
	 (dp)->d_back->d_forw = (dp)->d_forw, \
	 NULL_HASH(dp))

#define	NULL_HASH(dp) \
	((dp)->d_forw = (dp)->d_back = (dp))

typedef struct dfreelist {
	dnode_t	*d_forw;	/* hash table linkage */
	dnode_t	*d_back;
	dnode_t	*d_avforw;	/* freelist linkage */
	dnode_t	*d_avback;
} dfreelist_t;

#define	INSERT_FREE(dp, freelist) \
	((dp)->d_avforw = (freelist)->d_avforw, \
	 (dp)->d_avback = (dnode_t *)(freelist), \
	 (dp)->d_avforw->d_avback = (dp)->d_avback->d_avforw = (dp))

#define	REMOVE_FREE(dp) \
	((dp)->d_avforw->d_avback = (dp)->d_avback, \
	 (dp)->d_avback->d_avforw = (dp)->d_avforw, \
	 NULL_FREE(dp))

#define	NULL_FREE(dp) \
	((dp)->d_avforw = (dp)->d_avback = (dp))

#define	DHASHLOG2	7
#define	DHASHSIZE	(1<<DHASHLOG2)
#define	DHASH(fno)	(&dhash[(fno) & DHASHSIZE-1])

static dlist_t dhash[DHASHSIZE];

static dfreelist_t dfreelist = {
	(dnode_t *)&dfreelist, (dnode_t *)&dfreelist,
	(dnode_t *)&dfreelist, (dnode_t *)&dfreelist
};

#define	NUMDNODES	256

static dnode_t dnodes[NUMDNODES];

void
dos_initnodes()
{
	dnode_t	*dp;
	int	i;

	for (i = 0; i < DHASHSIZE; i++) {
		dp = (dnode_t *) &dhash[i];
		NULL_HASH(dp);
	}
	for (i = 0; i < NUMDNODES; i++) {
		dp = &dnodes[i];
		INSERT_FREE(dp, &dfreelist);
		NULL_HASH(dp);
	}
}

void
dos_purgenodes(dfs_t *dfs)
{
	dnode_t *dp;
	int	i;

	for (i = NUMDNODES, dp = &dnodes[0]; --i >= 0; dp++) {
		if (dp->d_dfs == dfs)
			dos_purgenode(dp);
	}
}

void
dos_purgenode(dnode_t *dp)
{
	if (dp->d_count > 1)
		panic("dos_purgenode: multiple reference");
	REMOVE_HASH(dp);
	if (dp->d_count == 1) {
		dp->d_count = 0;
		INSERT_FREE(dp, &dfreelist);
	}
	if (dp->d_dir != NULL)
	    vfat_dir_write(dp);
	dp->d_dfs = NULL;
	dp->d_fno = 0;
}

int
dos_getnode(dfs_t *dfs, u_long fno, dnode_t **dpp)
{
	dlist_t *dl;
	dnode_t *dp;
	int	error;

	dl = DHASH(fno);
	for (dp = dl->d_forw; dp != (dnode_t *) dl; dp = dp->d_forw) {
		if (dp->d_dfs == dfs && dp->d_fno == fno) {
			dp->d_count++;
			REMOVE_FREE(dp);
			*dpp = dp;
			return 0;
		}
	}

	dp = dfreelist.d_avforw;
	if (dp == (dnode_t *) &dfreelist)
		panic("dos_getnode: out of dnodes, stuck references");
	REMOVE_HASH(dp);
	REMOVE_FREE(dp);

	dp->d_dfs = dfs;
	dp->d_fno = fno;
	if (error = dos_readnode(dp)) {
		INSERT_FREE(dp, &dfreelist);
		return error;
	}
	INSERT_HASH(dp, dl);
	dp->d_count = 1;
	*dpp = dp;
	return 0;
}

void
dos_putnode(dnode_t *dp)
{
	--dp->d_count;
	if (dp->d_count < 0) {
		panic("dos_putnode: fs %s fno %lx reference underflow",
		      dp->d_dfs->dfs_pathname, dp->d_fno);
	}
	if (dp->d_count == 0) {
		INSERT_FREE(dp, dfreelist.d_avback);
	}
}
