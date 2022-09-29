#include "iso.h"
#include "cdrom.h"
#include "iso9660.h"
#include "cye_mdebug.h"

static int _iso9660_strat(FSBLOCK *);
static int _iso9660_checkfs(FSBLOCK *);
static int _iso9660_open(FSBLOCK *);
static int _iso9660_close(FSBLOCK *);
static int _iso9660_read(FSBLOCK *);
static int _iso9660_getdirent(FSBLOCK *);
static int _iso9660_grs(FSBLOCK *);

static char dot[] = ".";
static char dotdot[] = "..";




int
iso9660_install(void)
{

	/*
	** Register the ISO9660 filesystem to the standalone kernel and
	** initialize any variables.
	**
	** HACK:
	**      The ISO9660 file system is registered as type DTFS_EFS so
	** that the prom routine fs_search() will check for ISO9660 style
	** file systems. The correct fix is to change the prom code so
	** that we can do something like:
	**
	** fs_register(_iso9660_strat, "iso9660", DTFS_ISO9660)
	*/

	int ret = fs_register(_iso9660_strat, "iso9660", DTFS_EFS);

	if( ret )
		printf("iso9660_install() ERROR : fs_register()\n");

	return(ret);
}





static int
_iso9660_strat(FSBLOCK *fsb)
{

	switch (fsb->FunctionCode) {
		case FS_OPEN:
			return _iso9660_open(fsb);
		case FS_CLOSE:
			return _iso9660_close(fsb);
		case FS_READ:
			return _iso9660_read(fsb);
		case FS_WRITE:
			return fsb->IO->ErrorNumber = EROFS;
		case FS_CHECK:
			return _iso9660_checkfs(fsb);
		case FS_GETDIRENT:
			return _iso9660_getdirent(fsb);
		case FS_GETREADSTATUS:
			return _iso9660_grs(fsb) ;
		default:
			return fsb->IO->ErrorNumber = ENXIO;
	}
}





static iso9660_info_t *
_get_iso9660info(void)
{

	/*
	** Allocate memory to contain the ISO9660 data structures as well
	** as memory to contain the iso9660_info_t structure itself.
	**
	** Return NULL if allocation fails.
	*/

	int len;
	iso9660_info_t *iso9660_infop;


	len = sizeof(iso9660_info_t);
	if((iso9660_infop = (iso9660_info_t *) cye_malloc(len)) == 0) {
		return (iso9660_info_t *)0;
	}

	/* allocate structures within iso9660_infop -- return NULL if fail */
	len = sizeof(struct p_vol_desc);
	if ( (iso9660_infop->iso9660_pvd = (struct p_vol_desc *)
	cye_malloc(len)) == 0) {
		cye_free(iso9660_infop);
		return (iso9660_info_t *)0;
        }

	len = sizeof(dir_t);
        if ( (iso9660_infop->iso9660_dinode = (dir_t *) cye_malloc(len))
	== 0) {
		cye_free(iso9660_infop->iso9660_pvd);
		cye_free(iso9660_infop);
		return (iso9660_info_t *)0;
	}

	return(iso9660_infop);
}





static void
_free_iso9660info(iso9660_info_t *iso9660_infop)
{
	if(iso9660_infop != NULL) {
		if(iso9660_infop->iso9660_pvd != NULL)
			cye_free(iso9660_infop->iso9660_pvd);
		if(iso9660_infop->iso9660_dinode != NULL)
			cye_free(iso9660_infop->iso9660_dinode);
        	cye_free(iso9660_infop);
	}
}





/*
** Returns:  1 on success
**           0 on failure
*/

static int
_iso9660_lookup_name(FSBLOCK *fsb, char *filename)
{
	int ret;
	int namelen, error;
	IOBLOCK *iob = fsb->IO;
	fhandle_t *fh_dir, *fh_ret;
	char *sp, *basename, name[MAXNAMELEN + 1];

#ifdef DEBUG
	printf("_iso9660_lookup_name()\n");
#endif
#ifdef MDEBUG
	printf("START of _iso9660_lookup_name()  ");
	cye_outstanding_nbytes();
#endif
	if(strlen(filename) == 1 && strncmp(filename, "/", 1) == 0) {
		basename = dot;
		sp = basename;
	} else {

		/*
		** skip over leading blanks, tabs, and slashes -- all paths are
		** grounded at root
		*/
		sp = filename;
		while (*sp && (*sp == ' ' || *sp == '\t' || *sp == '/')) sp++;

		/* find our basename */
		basename = filename;
		while(*(basename++) != '\0');
		while(*(--basename - 1) != '/'); 
	}

        if(NULL == (fh_ret = (fhandle_t *) cye_malloc(sizeof(fhandle_t)))) {
                printf("_iso9660_lookup_name() ERROR : malloc()\n");
                return(1);
        }

	/* do our lookup in CDROM_BLKSIZE byte block mode */
	cd_set_blksize(fsb, CDROM_BLKSIZE);

	/* set up our starting point -- root directory */
	fh_dir = &rootfh;

	/* traverse the directories until the basename is found */
	while(sp != basename) {

		for(namelen = 0; namelen < MAXNAMELEN && *sp && *sp != '/';
		name[namelen++] = *sp++);
		name[namelen] = '\0';

#ifdef DEBUG
		printf("_iso9660_lookup_name() lookup on [%s]\n", name);
#endif
		if ( ! iso_lookup(fsb, fh_dir, name, fh_ret)  ) {
			if(fh_dir != &rootfh && fh_dir != fh_ret)
				cye_free(fh_dir);
			fh_dir = fh_ret;
			fh_ret = (fhandle_t *) cye_malloc(sizeof(fhandle_t));
		} else {
			if(fh_dir != &rootfh) cye_free(fh_dir);
			if(fh_ret != &rootfh) cye_free(fh_ret);
			cd_set_blksize(fsb, 512);
			return(1);
		}

		/* skip over any additional slashes */
		while(*sp == '/') sp++;
	}


	/* do our final lookup */
#ifdef DEBUG
	printf("_iso9660_lookup_name() final lookup on %s (parent_dir_bn=%d)\n",
	basename, fh_dir->fh_fno/CDROM_BLKSIZE);
#endif

	if ( !iso_lookup(fsb, fh_dir, basename, fh_ret) ) {

		fsb->IO->StartBlock =
		CHARSTOLONG(fsb_to_inode(fsb)->ext_loc_msb);
#ifdef DEBUG
		printf("_iso9660_lookup_name() : %s at block %d len=%d\n",
		fsb->Filename, fsb->IO->StartBlock,
		CHARSTOLONG(fsb_to_inode(fsb)->data_len_msb));
#endif

		ret = 0;
	} else {
#ifdef DEBUG
		printf("_iso9660_lookup_name() : failed on final lookup\n");
#endif
		ret = 1;
	}

	if(fh_dir != &rootfh) cye_free(fh_dir);
	if(fh_ret != &rootfh) cye_free(fh_ret);

	/* return to 512 byte block mode */
	cd_set_blksize(fsb, 512);

#ifdef MDEBUG
	printf("END of _iso9660_lookup_name()  ");
	cye_outstanding_nbytes();
#endif
	return(ret);
}





static int
_iso9660_get_pvd(FSBLOCK *fsb)
{

	/*
	** read the PVD from the device pointed to by the fsb and store
	** the data in the iso9660_pvd structure (via fsb->FsPtr)
	*/

	int error;
	struct p_vol_desc *vol = fsb_to_mount(fsb);

	error = cd_read(fsb, cd_voldesc(), vol, sizeof(struct p_vol_desc));
	if (error) {
		printf("_iso9660_get_pvd() ERROR : cd_read()\n");
		return(error);
	}
	if(strncmp(vol->id, "CD001", 5) == 0) {
		return(0);
	} else {
		return(1);
	}

}





static int
_iso9660_open(FSBLOCK *fsb)
{

	int ret;
	char *filename = (char *)fsb->Filename;

#ifdef DEBUG
	printf("_iso9660_open(%d,%d,%d) on %s\n", fsb->IO->Controller,
	fsb->IO->Unit, fsb->IO->Partition, fsb->Filename);
#endif

#ifdef MDEBUG
	printf("at start of _iso9660_open()  ");
	cye_outstanding_nbytes();
#endif
	if(fsb->IO->Flags & F_WRITE) {
		printf("iso9660: cannot open file for writing.\n");
		return(fsb->IO->ErrorNumber = EIO);
	}

	if((fsb->FsPtr = _get_iso9660info()) == 0) {
		printf("iso9660: couldn't allocate memory for iso9660 info.\n");
		return(fsb->IO->ErrorNumber = ENOMEM);
	}

	/* read in primary volume descriptor */
	if (_iso9660_get_pvd(fsb)) {
		_free_iso9660info(fsb->FsPtr);
		printf("_iso9660_open() ERROR : _iso9660_get_pvd()\n");
		return( fsb->IO->ErrorNumber = ENXIO );
	}

	/* this only needs to be called once per CD actually */
	if( set_blocks(fsb) ) {
		printf("_iso9660_open() ERROR : set_blocks()\n");
		_free_iso9660info(fsb->FsPtr);
		return(fsb->IO->ErrorNumber = EIO);
	}
	iso_init(128);

	if(NULL == (fsb->Buffer = (char *) cye_malloc(CDROM_BLKSIZE))) {
		printf("_iso9660_open() ERROR : malloc()\n");
		_free_iso9660info(fsb->FsPtr);
		return(fsb->IO->ErrorNumber = EIO);
	}

	/*
	** find the file we're interested in (use lookup function --
	** passing filename string and FSBLOCK structure to fill)
	*/
	if (_iso9660_lookup_name(fsb, filename)) {
		ret = ENOENT;
		_free_iso9660info(fsb->FsPtr);
	} else {
		ret = ESUCCESS;
	}
	return(fsb->IO->ErrorNumber = ret);
}





static int
_iso9660_close(FSBLOCK *fsb)
{
#ifdef DEBUG
	printf("_iso9660_close() on (%d,%d,%d) [%s] ", fsb->IO->Controller,
	fsb->IO->Unit, fsb->IO->Partition, fsb->Filename);
#endif
	_free_iso9660info(fsb->FsPtr);
	cye_free(fsb->Buffer);
#ifdef DEBUG
	printf("OKAY\n");
#endif
#ifdef MDEBUG
	printf("at end of _iso9660_close()  ");
	cye_outstanding_nbytes();
#endif
	return(ESUCCESS);
}





static int
_iso9660_read(FSBLOCK *fsb)
{
	int error, offset;
	IOBLOCK *iob = fsb->IO;
	LONG length;
	int count = (int) iob->Count;

	length = CHARSTOLONG(fsb_to_inode(fsb)->data_len_msb);

	/* truncate the count value if it extents past end of file */
	if (iob->Offset.lo + count > length)
		count = (LONG) (length - iob->Offset.lo);

#ifdef DEBUG
	printf("_iso9660_read(len=%d count=%d StartBlock=%d, Offset.lo=%d)\n",
	length, count, iob->StartBlock, iob->Offset.lo);
#endif

	/* check that count is a positive value */
	if (count <= 0) {
		iob->Count = 0;
		return(ESUCCESS);
	}

	offset = CHARSTOLONG(fsb_to_inode(fsb)->ext_loc_msb) * CDROM_BLKSIZE +
	iob->Offset.lo;
	error = cd_set_blksize(fsb,512);
	if(error) {
		printf("_iso9660_read() ERROR with cd_set_blksize(512)\n");
		return(EIO);
	}

        /* single extent logic  */
        error = cd_read(fsb, offset, iob->Address, count);
	if(error) {
		printf("_iso9660_read() : ERROR with cd_read()\n");
		return(EIO);
	}

	iob->Offset.lo += count;
	iob->Count = count;
#ifdef DEBUG
	printf("_iso9660_read() iob->Offset.lo=%d count=%d\n",iob->Offset.lo,
	count);
	printf("_iso9660_read() OKAY\n");
#endif
	return(ESUCCESS);
}





/* I'll put something real in here later -- for now, this should be fine. */

static int
_iso9660_checkfs(FSBLOCK *fsb)
{

	struct p_vol_desc vol;

#ifdef DEBUG
	printf("_iso9660_checkfs()\n");
#endif

	if(cd_read(fsb, cd_voldesc(), &vol, sizeof(struct p_vol_desc))) {
		printf("_iso9660_checkfs() ERROR : cd_read()\n");
		return(EINVAL);
	}

	if(strncmp(vol.id, "CD001", 5) == 0) {
		return(ESUCCESS);
	} else {
#ifdef DEBUG
		printf("_iso9660_checkfs() NOT AN ISO9660 FILESYSTEM\n");
#endif
		return(EINVAL);
	}
}





static int
_iso9660_getdirent(FSBLOCK *fsb)
{

	/*
	** Get the directory entries for the directory described by the
	** fsb parameter.
	**
	** RETURNS:
	**        ESUCCESS on success
	**        ENOTDIR when there are no more entries.
	*/

	IOBLOCK *iob = fsb->IO;
	LONG count = fsb->IO->Count;
	DIRECTORYENTRY *dp = (DIRECTORYENTRY *)fsb->IO->Address;
	fhandle_t fh;
	entry *entries, *ep;
	int cnt;	/* maximum # of bytes to read into entries */
	LONG cookie;	/* offset into directory entry from which to start */
	bool_t eof = 0;	/* set to 1 if we've read the last entry */
	int nread;	/* set to the number of entries read */
	int i, j, ret;
	
#ifdef MDEBUG
	printf("START of _iso9660_getdirent()  ");
	cye_outstanding_nbytes();
#endif

	/* no directory entries requested */
	if (count <= 0) {
		return(fsb->IO->ErrorNumber = EINVAL);
	}

	cd_set_blksize(fsb, CDROM_BLKSIZE);
	cookie = fsb->IO->Offset.lo;

	fh.fh_fno = CHARSTOLONG(fsb_to_inode(fsb)->ext_loc_msb) * CDROM_BLKSIZE;

	/*
	** they asked for count DIRECTORYENTRYies so that's how many bytes
	** we'll ask for and allocate in entry equivalents
	*/
	cnt = (sizeof(entry) + 64) * count;
	entries = (entry *) cye_malloc(cnt);

#ifdef DEBUG
	printf("block=%d fh_fhno=%d count=%d cnt=%d entries=0x%x\n",
	fh.fh_fno/CDROM_BLKSIZE, fh.fh_fno, count, cnt, entries);
#endif

	/*
	** need to load dp array with information from entries buffer
	**
	** problem is that nread can exceed count after one pass, in
	** which case you won't have enough space to transfer the
	** entries
	**
	** if you stop/truncate, you must make sure cookie is correct
	*/
	i = 0;
	while(!eof &&
	(ret = iso_readdir(fsb, &fh, entries, cnt, cookie, &eof, &nread)) == 0
	&& i < count && nread > 0) {

		ep = entries;
		j = 0;
		while(i<count && j<nread && ep != NULL) {
			int flen = strlen(ep->name);
#ifdef DEBUG
printf("_iso9660_getdirent() : name=[%s] flen=%d dst=0x%x\n", ep->name, flen,
dp->FileName);
#endif
			/* for now, everything is a file */
			dp->FileAttribute = ReadOnlyFile;
			dp->FileNameLength = flen;
			bcopy(ep->name, dp->FileName, flen);
			dp->FileName[flen] = '\0';
			cookie = CHARSTOLONG(ep->cookie);
			ep = ep->nextentry;
			dp++;
			i++;
			j++;
		}
		if(i < nread) eof = 0;
#ifdef DEBUG
		printf("loaded %d entries, %d/%d, eof=%d, ret=%d cookie=%d\n",
		nread, i, count, eof, ret, cookie);
#endif
	}

	cye_free(entries);
	fsb->IO->Offset.lo = cookie;
	fsb->IO->Count = i;

	if(ret != 0)
		return(ret);

	/* ARCS specifies that ENOTDIR is returned when no more entries */
	if (fsb->IO->Count == 0) {
#ifdef MDEBUG
	printf("END of _iso9660_getdirent()  ");
	cye_outstanding_nbytes();
#endif
		return(fsb->IO->ErrorNumber = ENOTDIR);
	}

#ifdef MDEBUG
	printf("END of _iso9660_getdirent()  ");
	cye_outstanding_nbytes();
#endif
	return(ESUCCESS);
}





static int
_iso9660_grs(FSBLOCK *fsb)
{
	/* returns read status */

	LONG length;

	length = CHARSTOLONG(fsb_to_inode(fsb)->data_len_msb);

	/* make sure we didn't seek past the end of the file */
	if (fsb->IO->Offset.lo >= length)
		return(fsb->IO->ErrorNumber = EAGAIN);

	/* set the result (count) of our byte transfer */
	fsb->IO->Count = (LONG) (length - fsb->IO->Offset.lo);

	return(ESUCCESS);
}
