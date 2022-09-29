

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>
#include <bstring.h>

#include "fpck.h"
#include "dosfs.h"
#include "macLibrary.h"

#include "part.h"

extern	int	debug_flag;

/* get volume header */
int
fatGetvh(FPINFO * fpinfop, u_int partition, dfs_t * dfs)
{
    void 	part_exist_clr(void);
    void 	part_changed_clr(void);
    void 	part_read_logic(u_long ext_offset);
    pte_t	*part_pte_find(int partn, u_long *ptbl_offset,
					    u_long *part_offset);

    extern int		fd;				
    extern int		part_count;				
    extern int		ext_index;
    extern u_long	ext_size;
    extern u_long	ext_offset;
    extern u_long	offsets[MAX_PARTNS];	
    extern char		exists[MAX_PARTNS];	
    extern char		buffer[SECTOR_SIZE];		

    int	      partn;
    u_int     bp_buf[SECTOR_SIZE / sizeof(u_int)];
    u_char  * bs = (u_char *)bp_buf;
    u_int     sectors;
    u_long    i, cluster;
    u_long    fat_entry_size;
    u_long    part_offset;
    u_long    ptbl_offset;
    bp_t    * bpp;
    pte_t   * p;


    if (partition > MAX_PARTNS || partition == 4)
        return(E_NOTSUPPORTED);

    lseek(fpinfop->devfd, 0, 0);

    if (partition == 0) {        /* floppy disk */
        part_offset = 0;
        fat_entry_size = 12;

    } else {                /* SoftPC hard disk */
        fpblockread(fpinfop->devfd, bs, sizeof(bp_buf)); 

	/*
	 * check format for non-partitioned diskette
	 *	Position 0 == 0xe9
	 *    -or-
	 *  Position 0 == DOS_MAGIC1 -and- Position 2 == DOS_MAGIC3
	 */
	if (    bs[VH_MAGIC0] == 0xe9
	    || (bs[VH_MAGIC0] == 0xEB && bs[VH_MAGIC2] == 0x90)) {

	    return(E_RANGE);
	}

	/*
	 * check format for partitioned diskette
	 *  Position 510 == 0x55 -and- Position 511 == 0xaa
	 */
	if ( ! (bs[510] == 0x55 && bs[511] == 0xaa)) {

	    return(E_RANGE);
	}

	fd = fpinfop->devfd;		/* Global fd used by part.c */

	lseek(fpinfop->devfd, 0, 0);

	fpblockread(fpinfop->devfd, (unsigned char *)buffer, SECTOR_SIZE); 

	part_exist_clr();
	part_changed_clr();

	for (partn = 0; partn < 4; partn++){

		p = pte_offset(buffer, partn);

		if (p->type == 0)
			continue;

		part_count++;

		if (p->type == PTE_EXTENDED) {
			ext_index  = partn;
			ext_offset = p_start_sect(p);
			ext_size   = p_numbr_sect(p);

			part_read_logic(ext_offset);
		}

		offsets[partn] = p_start_sect(p);
		exists[partn]  = 1;
	}

	/*
	 * Check to see if the desired partition is there.
	 */
	if ( ! exists[partition-1])
		return E_RANGE;

        /*
         * Locate the partition descriptor within the master boot
         * record. we support only type 1 and 4/6 partitions.
	 *
         * Find out the offset, from the begining of the disk, of the
         * partition in bytes
         */
	p = part_pte_find(partition-1, &ptbl_offset, &part_offset);

	if (debug_flag)
		fpMessage("dosutil.c:fatGetvh: part_offset=%d, type=%d",
						    part_offset, p->type);
        switch (p->type) {
          case 1:
            fat_entry_size = 12;
            break;
          case 4:
          case 6:
            fat_entry_size = 16;
            break;
          default:
            return(E_RANGE);
        }

        lseek(fpinfop->devfd, part_offset, 0);
    }

    /* initialize the file system structure */
    bzero(dfs, sizeof(dfs_t));
    bpp = &dfs->dfs_bp;
    dfs->dfs_fd = fpinfop->devfd;

    /* read disk boot parameter block */
    if (rfpblockread(fpinfop->devfd, bs, sizeof(bp_buf)) != E_NONE)
        return(E_READ);

    /*
     * check format
     *	Position 0 == 0xe9
     *    -or-
     *  Position 0 == DOS_MAGIC1 -and- Position 2 == DOS_MAGIC3
     */
    if ( ! (    bs[VH_MAGIC0] == 0xe9
            || (bs[VH_MAGIC0] == 0xEB && bs[VH_MAGIC2] == 0x90))) {

    if (debug_flag)
	fpError("dosutil.c:fatGetvh: Unknown magic #'s, 0=0x%x, 1=0x%x, 2=0x%x",
				bs[VH_MAGIC0], bs[VH_MAGIC1], bs[VH_MAGIC2]);

        return(E_UNKNOWN);
    }

    if (debug_flag)
	fpMessage("dosutil.c:fatGetvh: magic #'s, 0=0x%x, 1=0x%x 2=0x%x",
				bs[VH_MAGIC0], bs[VH_MAGIC1], bs[VH_MAGIC2]);

    bpp->bp_sectorsize = (bs[VH_SSHI] << 8) | bs[VH_SSLO];
    bpp->bp_sectorspercluster = bs[VH_CS];
    bpp->bp_reservecount = (bs[VH_RCHI] << 8) | bs[VH_RCLO];
    bpp->bp_fatcount = bs[VH_FC];
    bpp->bp_rootentries = (bs[VH_REHI] << 8) | bs[VH_RELO];
    bpp->bp_sectorcount = (bs[VH_SCHI] << 8) | bs[VH_SCLO];
    bpp->bp_mediatype = bs[VH_MT];
    bpp->bp_fatsize = (bs[VH_FSHI] << 8) | bs[VH_FSLO];
    bpp->bp_sectorspertrack = (bs[VH_STHI] << 8) | bs[VH_STLO];
    bpp->bp_headcount = (bs[VH_HCHI] << 8) | bs[VH_HCLO];
    bpp->bp_hiddensectors = (bs[VH_HSHHI] << 24) |
        (bs[VH_HSHLO] << 16) | (bs[VH_HSLHI] << 8) | bs[VH_HSLLO];

    /* total number of addressable sectors */
    sectors = bpp->bp_sectorcount - bpp->bp_reservecount -
        bpp->bp_rootentries / (bpp->bp_sectorsize / DIR_ENTRY_SIZE) -
        bpp->bp_fatsize * bpp->bp_fatcount;

    dfs->dfs_fataddr  = part_offset + bpp->bp_reservecount * bpp->bp_sectorsize;
    dfs->dfs_fatsize  = bpp->bp_fatsize * bpp->bp_sectorsize;
    dfs->dfs_rootaddr = dfs->dfs_fataddr + bpp->bp_fatcount *
                        dfs->dfs_fatsize;
    dfs->dfs_rootsize = bpp->bp_rootentries * DIR_ENTRY_SIZE;
    dfs->dfs_fileaddr = dfs->dfs_rootaddr + dfs->dfs_rootsize;
    dfs->dfs_clustersize = bpp->bp_sectorspercluster * bpp->bp_sectorsize;

    /*
     * allocate space for and copy in FAT, use the secondary copies if
     * necessary
     */
    dfs->dfs_fat = safemalloc((u_int)dfs->dfs_fatsize);
    lseek(fpinfop->devfd, dfs->dfs_fataddr, 0);
    for (i = 0; i < bpp->bp_fatcount; i++) {

	/* if fat table is broken, then try the spare one */
        if (rfpblockread(fpinfop->devfd, dfs->dfs_fat, dfs->dfs_fatsize)
							      == E_NONE)
            break;

        if (i == bpp->bp_fatcount - 1) 
            return(E_READ);

        lseek(fpinfop->devfd,
              dfs->dfs_fataddr + dfs->dfs_fatsize * (i + 1), 0);
    }

    /* Floptical standard committee defined two types of
       FATs and BPBs for the 20.8M VHD Floptical diskette,
       which are supporting by all the committee member
       companies. we need to figure out the current media
       type in the drive, and its formats */

	if (debug_flag)
	    fpMessage(
	      "dosutil.c:fatGetvh: mediatype=0x%x, fatsz=0x%x secperclstr=0x%x",
					bpp->bp_mediatype, bpp->bp_fatsize,
					bpp->bp_sectorspercluster);

    if (bpp->bp_mediatype == 0xF9 &&
        bpp->bp_fatsize  == 0x28       &&
        bpp->bp_sectorspercluster == 0x04) {
        /* it's 16bit FAT layout floptical diskette */
        fat_entry_size = 16;
    }

    if (fat_entry_size == 12) {
        dfs->dfs_readfat = readfat12;
        dfs->dfs_writefat = writefat12;
        dfs->dfs_endclustermark    = 0xfff;
        dfs->dfs_badclustermark    = 0xff7;
        dfs->dfs_unusedclustermark = 0xffe;
    } else {
        dfs->dfs_readfat = readfat16;
        dfs->dfs_writefat = writefat16;
        dfs->dfs_endclustermark    = 0xffff;
        dfs->dfs_badclustermark    = 0xfff7;
        dfs->dfs_unusedclustermark = 0xfffe;
    }

    /*
     * find out number of usable entries in FAT, the first 2 entries in
     * the FAT are reserved
     */
    dfs->dfs_clustercount = dfs->dfs_fatsize * 8 / fat_entry_size - 2;
    if (dfs->dfs_clustercount > sectors / bpp->bp_sectorspercluster)
        dfs->dfs_clustercount = sectors / bpp->bp_sectorspercluster;

    /* allocate the bitmap for fat block mapping */
    dfs->fatbitmapsz = (dfs->dfs_clustercount+BITMAPUNITSZ-1) / BITMAPUNITSZ;
    dfs->fatbitmapp  = safemalloc(dfs->fatbitmapsz);
    bzero(dfs->fatbitmapp, dfs->fatbitmapsz);

    if (debug_flag)
	fpMessage(
	      "dosutil.c:fatGetvh: clustcnt=%d, fatsz=%d fatentsz=%d",
					dfs->dfs_clustercount,
					dfs->dfs_fatsize,
					fat_entry_size);
    /* find out number of free clusters on disk,
       and set the fatbimap to map the used block */
    dfs->dfs_freeclustercount = 0;
    for (i = 2; i < dfs->dfs_clustercount + 2; i++) {
        if ((cluster = (*dfs->dfs_readfat)(dfs, i)) == 0) {
            dfs->dfs_freeclustercount++;
        } else if ((cluster != dfs->dfs_badclustermark) &&
                   (cluster != dfs->dfs_unusedclustermark))
            setfatbitmap(dfs, i);  
    }

    /* read the root directory information */
    dfs->dfs_root = (dfile_t *) safemalloc(dfs->dfs_rootsize);
    lseek(dfs->dfs_fd, dfs->dfs_rootaddr, 0);
    if (rfpblockread(dfs->dfs_fd, (u_char *) dfs->dfs_root,
                                            dfs->dfs_rootsize) != E_NONE)
        return(E_READ);

    return(E_NONE);
}


/*
 * check to see if sector 0 is a volume header */
int
fatChkvh(FPINFO * fpinfop)
{
    u_int     bp_buf[SECTOR_SIZE / sizeof(u_int)];
    u_char  * bs = (u_char *)bp_buf;
    bp_t    * bpp;
    int       error;
    u_short   i, cluster;
    u_int     p_offset;
    u_int     pd_offset;
    u_int     sectors;
    u_short   fat_entry_size;

    lseek(fpinfop->devfd, 0, 0);

    /*
     * read disk boot parameter block
     */
    fpblockread(fpinfop->devfd, bs, sizeof(bp_buf));

    /*
     * check format
     *	Position 0 == 0xe9
     *    -or-
     *  Position 0 == DOS_MAGIC1 -and- Position 2 == DOS_MAGIC3
     */
    if ( ! (    bs[VH_MAGIC0] == 0xe9
            || (bs[VH_MAGIC0] == 0xEB && bs[VH_MAGIC2] == 0x90))) {

	if (debug_flag > 2)
	    fpError(
	      "dosutil.c:fatChkvh: Unknown magic #'s, 0=0x%x, 1=0x%x, 2=0x%x",
				bs[VH_MAGIC0], bs[VH_MAGIC1], bs[VH_MAGIC2]);

        return(E_UNKNOWN);
    }

    if (debug_flag)
	fpMessage("dosutil.c:ChkGetvh: magic #'s, 0=0x%x, 1=0x%x 2=0x%x",
				bs[VH_MAGIC0], bs[VH_MAGIC1], bs[VH_MAGIC2]);
    return(E_NONE);
}

/*
 *---------------------------------------------------------------------------
 * fpPart_pte_print()
 * This routine prints out the information in a partition table entry.
 *---------------------------------------------------------------------------
 */
void
fpPart_pte_print(int partn, pte_t *p, u_long ptbl_offset, u_long part_offset)
{
	u_short	get_bcyl(pte_t *p);
	u_short	get_ecyl(pte_t *p);
	 
        u_long   part_size;
        u_long   part_endaddr;

        part_size    = p_numbr_sect(p);
        part_endaddr = part_offset + part_size;

	if (p->type != PTE_EXTENDED) {
		fpMessage("\n");
		fpMessage(" fpck: partition index = %d", partn+1);
		fpMessage(" fpck: partition type  = %d", p->type);
		fpMessage(" fpck: partition begin = %d cyl", get_bcyl(p));
		fpMessage(" fpck: partition end   = %d cyl", get_ecyl(p));
		fpMessage(" fpck: partition size  = %s", ground(part_size));

		fpMessage(" fpck: ptbl offset     = %d", ptbl_offset);
		fpMessage(" fpck: part offset     = %d", part_offset);
		fpMessage(" fpck: end  offset     = %d", part_endaddr);
	}
}

/*
 *----------------------------------------------------------------------------
 * fpPart_read_disk()
 * This routine reads the master boot record of the hard-drive and then
 * invokes other functions to read extended partitions.
 *----------------------------------------------------------------------------
 */
void
fpPart_read_disk(FPINFO * fpinfop)
{
	void 	part_exist_clr(void);
	void 	part_changed_clr(void);
	void 	part_read_logic(u_long ext_offset);
	pte_t  *part_pte_find(int partn, u_long *ptbl_offset,
						u_long *part_offset);

	extern int	fd;				
	extern int	part_count;				
	extern int	ext_index;
	extern	int	verbose_flag;		/* Flag for verbosity */
	extern u_long	ext_size;
	extern u_long	ext_offset;
	extern u_long	offsets[MAX_PARTNS];	
	extern char	exists[MAX_PARTNS];	
	extern char	buffer[SECTOR_SIZE];		

	int	partn;
	char	part_message[256];
	pte_t	*p;
        u_long   part_offset;
        u_long   ptbl_offset;


	fd = fpinfop->devfd;		/* Global fd used by part.c */

	lseek(fpinfop->devfd, 0, 0);

	fpblockread(fpinfop->devfd, (unsigned char *)buffer, SECTOR_SIZE); 

	/*
	 * check format for partitioned diskette
	 *  Position 510 == 0x55 -and- Position 511 == 0xaa
	 */
	if ( ! (buffer[510] == 0x55 && buffer[511] == 0xaa)) {

		fpWarn(
"Partition sector magic # mismatch! exp./rec. 510(0x55/0x%x) 511(0xaa/0x%x)",
						buffer[510], buffer[511]);
	    return;
	}

	part_exist_clr();
	part_changed_clr();

	for (partn = 0; partn < 4; partn++){

		p = pte_offset(buffer, partn);

		if (p->type == 0)
			continue;

		part_count++;

		if (p->type == PTE_EXTENDED) {
			ext_index  = partn;
			ext_offset = p_start_sect(p);
			ext_size   = p_numbr_sect(p);

			part_read_logic(ext_offset);
		}

		offsets[partn] = p_start_sect(p);
		exists[partn]  = 1;
	}

	if (verbose_flag > 1) {
		for (partn = 0; partn < part_count; partn++){

			if (exists[partn]){
				p = part_pte_find(partn, &ptbl_offset,
							 &part_offset);
				fpPart_pte_print(partn, p, ptbl_offset,
							   part_offset);
			}
		}
	} else {
		int i    = 0;
		int pcnt = 0;

		for (partn = 0; partn < part_count; partn++) {

			if (exists[partn]) {

				if (partn == 3)
					continue;

				if (i) {
					part_message[i++] = ',';
					part_message[i++] = ' ';
				}

				part_message[i++] = '1' + partn;

				pcnt++;
			}
		}

		part_message[i] = '\0';

		if (i)
			fpMessage("fpck: This disk has %spartition%s %s",
						(pcnt == 1) ? "only " : "",
						(pcnt > 1) ? "s" : "",
						part_message);
		else
			fpMessage("fpck: This disk has no active partitions");
	}
}





/* read an entry from the FAT. each entry is 12 bits wide */
u_short
readfat12(dfs_t *idfs, u_short cluster)
{
    int     i;

    i = cluster * 12 / 8;

    if (cluster % 2)
        return (idfs->dfs_fat[i] >> 4) | (idfs->dfs_fat[i + 1] << 4);

    return idfs->dfs_fat[i] | ((idfs->dfs_fat[i + 1] & 0xf) << 8);
}



/* read an entry from the FAT. each entry is 16 bits wide */
u_short
readfat16(dfs_t * idfs, u_short cluster)
{
    int     i;

    i = cluster * 2;

    return idfs->dfs_fat[i] | (idfs->dfs_fat[i + 1] << 8);
}



/* write an entry into the FAT. each entry is 12 bits wide */
void
writefat12(dfs_t * idfs, u_short cluster, u_short value)
{
    int  i;

    i = cluster * 12 / 8;

    if (cluster % 2) {
        idfs->dfs_fat[i] =
                     (idfs->dfs_fat[i] & 0xf) | ((value & 0xf) << 4);
        idfs->dfs_fat[i + 1] = value >> 4;
    } else {
        idfs->dfs_fat[i] = value & 0xff;
        idfs->dfs_fat[i + 1] =
        (idfs->dfs_fat[i + 1] & 0xf0) | (value >> 8);
    }
}


/* write an entry into the FAT. each entry is 16 bits wide */
void
writefat16(dfs_t * idfs, u_short cluster, u_short value)
{
    int     i;

    i = cluster * 2;

    idfs->dfs_fat[i] = value & 0xff;
    idfs->dfs_fat[i + 1] = value >> 8;
}



int
getfilesz(dfile_t * df)
{
    return(df->df_size[0] | (df->df_size[1] << 8) |
           (df->df_size[2] << 16) | (df->df_size[3] << 24));
}


int
getstartcluster(dfile_t * df)
{
    return(df->df_start[0] | (df->df_start[1] << 8));
}




void
to_dostime(u_char *d_date, u_char *d_time, time_t *u_time)
{
    struct tm       tm;

    tm = *localtime(u_time);
    d_date[0] = tm.tm_mday | (((tm.tm_mon + 1) & 0x7) << 5);
    d_date[1] = ((tm.tm_mon + 1) >> 3) | ((tm.tm_year - 80) << 1);
    d_time[0] = (tm.tm_sec / 2) | ((tm.tm_min & 0x7) << 5);
    d_time[1] = (tm.tm_min >> 3) | (tm.tm_hour << 3);
}


/*-------- getdosname ------------------------------------------

   return the file name string by duplicating it out 
   from the dos file entry structure.
   
   the user should take resposibility of releasing the memory

  ---------------------------------------------------------------*/

char *
getdosname(dfile_t * df)
{
    char * dname;
    char   namebuf[14];
    int    dnamesz;
    int    namesz = 0, extsz = 0, ip = 0;

    namebuf[0] = '\0';
    while (namesz < 8) {
        if (df->df_name[namesz] == ' ')
            break;

        namebuf[ip++] = df->df_name[namesz++];
    }

    /* if there is extention */
    if (df->df_ext[0] != ' ')
        namebuf[ip++] = '.';

    while (extsz < 3) {
        if (df->df_ext[extsz] == ' ')
            break;

        namebuf[ip++] = df->df_ext[extsz++];
    }
    namebuf[ip] = '\0';
    if (dnamesz = strlen(namebuf)) {
        dname = safemalloc(dnamesz);
        strcpy(dname, namebuf);
        return (dname);
    } else
        return ((char *) NULL);
}
