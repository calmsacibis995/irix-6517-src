/*
 *=============================================================================
 *                      File:           dos.c
 *                      Purpose:        This file has code for creating the
 *                                      actual filing system within a partn.
 *=============================================================================
 */
#include        <stdio.h>
#include        <time.h>
#include	<ctype.h>
#include        "dos.h"
#include	"misc.h"

/*
 *-----------------------------------------------------------------------------
 * Extern variables
 *-----------------------------------------------------------------------------
 */
extern  int     fd;             /* Device descr. */
extern  u_int   heads;          /* Disk Geometry */
extern  u_int   sectors;        /* Disk Geometry */

/*
 *-----------------------------------------------------------------------------
 * Global variables
 *-----------------------------------------------------------------------------
 */
static	dfs_t	*dos_dfs;

/*
 *-----------------------------------------------------------------------------
 * Function Prototypes
 *-----------------------------------------------------------------------------
 */
void    dos_mkfs(u_long daddr, u_long dsize, char *dlabel);
void	dos_create_incore(dfs_t **dfs, u_long daddr, u_long dsize);
void	dos_write_incore(dfs_t *dfs, char *dlabel);

void	dos_pack_boot(dfs_t *dfs, bp_t *bootp);
void	dos_init_fat(dfs_t *dfs);
void	dos_init_root(dfs_t *dfs);
void    dos_print_bootp(dfs_t *dfs, bp_t *bootp);

void    dos_time(u_char *d_date, u_char *d_time);
void    dos_check_label(char *outlabel, char *inlabel);
void    dos_write_label(char *label, u_long label_addr);

u_int   compute_sectorspercluster(u_int size);
u_int   compute_fatsize(u_int sect_count, u_int sect_reserved,
                        u_int sect_percluster, u_int dos_rootsize);

/*
 *-----------------------------------------------------------------------------
 * dos_mkfs()
 * This routine is the external interface to the rest of the program.
 *-----------------------------------------------------------------------------
 */
void	dos_mkfs(u_long daddr, u_long dsize, char *dlabel)
{
	dos_create_incore(&dos_dfs, daddr, dsize);
	dos_write_incore(dos_dfs, dlabel);
	return;
}

/*
 *-----------------------------------------------------------------------------
 * dos_create_incore()
 * This routine is used to create the incore data structures for the new
 * file system that is being created.
 *-----------------------------------------------------------------------------
 */
void	dos_create_incore(dfs_t **dfs, u_long daddr, u_long dsize)
{
	bp_t	dos_bootp;
	u_char	*dos_boot;
	u_char	*dos_fat;
	u_char	*dos_root;
	u_char	dos_sectorspercluster;
	u_short	dos_fatsize;	
	u_int	dos_numsectors;
	u_long	dos_rootsize;
	
	*dfs = gmalloc(sizeof(dfs_t));
	dos_rootsize   = DOS_ROOTSIZE(DOS_ROOTENTRIES);
	dos_numsectors = dsize/SECTOR_SIZE;
	dos_sectorspercluster = compute_sectorspercluster(dsize);
	dos_fatsize    = compute_fatsize(dos_numsectors, DOS_RESERVECOUNT,
			                 dos_sectorspercluster, dos_rootsize);
        strncpy((char *) dos_bootp.bp_oem, "IBM PNCI", OEM_LEN);
        dos_bootp.bp_magic[VH_MAGIC0] = DOS_MAGIC1;
        dos_bootp.bp_magic[VH_MAGIC1] = DOS_MAGIC2;
        dos_bootp.bp_magic[VH_MAGIC2] = DOS_MAGIC3;
        dos_bootp.bp_sectorsize       = SECTOR_SIZE;
        dos_bootp.bp_reservecount     = DOS_RESERVECOUNT;
        dos_bootp.bp_fatcount         = DOS_FATCOUNT;
        dos_bootp.bp_rootentries      = DOS_ROOTENTRIES;
        dos_bootp.bp_sectorcount      = 0;
        dos_bootp.bp_mediatype        = DOS_MEDIA_TYPE;
        dos_bootp.bp_sectorspertrack  = sectors;
        dos_bootp.bp_headcount        = heads;
        dos_bootp.bp_hiddensectors    = sectors;
        dos_bootp.bp_numsectors       = dos_numsectors;
        dos_bootp.bp_fatsize          = dos_fatsize;
        dos_bootp.bp_sectorspercluster= dos_sectorspercluster;

	(*dfs)->dfs_fd 	 = fd;
	(*dfs)->dfs_boot = gmalloc(SECTOR_SIZE);
	(*dfs)->dfs_fat  = gmalloc(dos_fatsize*SECTOR_SIZE);
	(*dfs)->dfs_root = gmalloc(dos_rootsize*SECTOR_SIZE);	
	(*dfs)->dfs_partaddr = daddr;
	(*dfs)->dfs_fataddr1 = (*dfs)->dfs_partaddr+SECTOR_SIZE;
	(*dfs)->dfs_fataddr2 = (*dfs)->dfs_fataddr1+dos_fatsize*SECTOR_SIZE;
	(*dfs)->dfs_rootaddr = (*dfs)->dfs_fataddr2+dos_fatsize*SECTOR_SIZE;
	(*dfs)->dfs_fileaddr = (*dfs)->dfs_rootaddr+dos_rootsize*SECTOR_SIZE;
	(*dfs)->dfs_fatsize  = dos_fatsize;
	(*dfs)->dfs_rootsize = dos_rootsize;

	dos_pack_boot(*dfs, &dos_bootp);
#ifdef	DOS_PRINT
	dos_print_bootp(*dfs,&dos_bootp);
#endif
	dos_init_fat(*dfs);
	dos_init_root(*dfs);
	return;
}

/*
 *-----------------------------------------------------------------------------
 * dos_write_incore()
 * This routine is used to write out the in-core data structures to disk.
 *-----------------------------------------------------------------------------
 */
void	dos_write_incore(dfs_t *dfs, char *dlabel)
{

#ifdef	DOS_PRINT
	printf(" PART_ADDR	= %d\n", dfs->dfs_partaddr);
        printf(" FAT1_ADDR      = %d\n", dfs->dfs_fataddr1);
        printf(" FAT1_SIZE      = %d\n", dfs->dfs_fatsize*SECTOR_SIZE);
        printf(" FAT2_ADDR      = %d\n", dfs->dfs_fataddr2);
        printf(" FAT2_SIZE      = %d\n", dfs->dfs_fatsize*SECTOR_SIZE);
        printf(" ROOT_ADDR      = %d\n", dfs->dfs_rootaddr);
        printf(" ROOT_SIZE      = %d\n", dfs->dfs_rootsize*SECTOR_SIZE);
        printf(" FILE_ADDR      = %d\n", dfs->dfs_fileaddr);
#endif
	gseek(dfs->dfs_partaddr);
	gwrite(dfs->dfs_boot, SECTOR_SIZE);

	gseek(dfs->dfs_fataddr1);
	gwrite(dfs->dfs_fat, SECTOR_SIZE*dfs->dfs_fatsize);

	gseek(dfs->dfs_fataddr2);
	gwrite(dfs->dfs_fat, SECTOR_SIZE*dfs->dfs_fatsize);

	gseek(dfs->dfs_rootaddr);
	gwrite(dfs->dfs_root, SECTOR_SIZE*dfs->dfs_rootsize);

	dos_write_label(dlabel, dfs->dfs_rootaddr);
	return;
}

/*
 *-----------------------------------------------------------------------------
 * dos_pack_boot()
 * This routine is used to align the boot sector.
 *-----------------------------------------------------------------------------
 */
void	dos_pack_boot(dfs_t *dfs, bp_t *bootp)
{
	u_int	tmpint;
	u_short	tmpshort;
	u_char	*ptr    = dfs->dfs_boot;
	u_char	*cbpptr = dfs->dfs_boot;

	memcpy(cbpptr, bootp->bp_magic, MAGIC_LEN);
	cbpptr += sizeof(u_char) * 3;

	memcpy(cbpptr, bootp->bp_oem, OEM_LEN);
	cbpptr += sizeof(u_char) * 8;

	tmpshort = align_short(bootp->bp_sectorsize);
	memcpy(cbpptr, &tmpshort, sizeof(u_short));
	cbpptr += sizeof(u_short);

	memcpy(cbpptr, &bootp->bp_sectorspercluster, sizeof(u_char));
	cbpptr += sizeof(u_char);

	tmpshort = align_short(bootp->bp_reservecount);
	memcpy(cbpptr, &tmpshort, sizeof(u_short));
	cbpptr += sizeof(u_short);

	memcpy(cbpptr, &bootp->bp_fatcount, sizeof(u_char));
	cbpptr += sizeof(u_char);

	tmpshort = align_short(bootp->bp_rootentries);
	memcpy(cbpptr, &tmpshort, sizeof(u_short));
	cbpptr += sizeof(u_short);

	tmpshort = align_short(bootp->bp_sectorcount);
	memcpy(cbpptr, &tmpshort, sizeof(u_short));
	cbpptr += sizeof(u_short);

	memcpy(cbpptr, &bootp->bp_mediatype, sizeof(u_char));
	cbpptr += sizeof(u_char);
	
	tmpshort = align_short(bootp->bp_fatsize);
	memcpy(cbpptr, &tmpshort, sizeof(u_short));
	cbpptr += sizeof(u_short);

	tmpshort = align_short(bootp->bp_sectorspertrack);
	memcpy(cbpptr, &tmpshort, sizeof(u_short));
	cbpptr += sizeof(u_short);

	tmpshort = align_short(bootp->bp_headcount);
	memcpy(cbpptr, &tmpshort, sizeof(u_short));
	cbpptr += sizeof(u_short);

	tmpint = bootp->bp_hiddensectors;
	ptr[VH_HSLLO] = (tmpint & 0x000000FF);
	ptr[VH_HSLHI] = (tmpint & 0x0000FF00) >> 8;
	ptr[VH_HSHLO] = (tmpint & 0x00FF0000) >> 16;
	ptr[VH_HSHHI] = (tmpint & 0xFF000000) >> 24;
	cbpptr += sizeof(u_int);

	tmpint = bootp->bp_numsectors;
	ptr[VH_SCLLO] = (tmpint & 0x000000FF);
	ptr[VH_SCLHI] = (tmpint & 0x0000FF00) >> 8;
	ptr[VH_SCHLO] = (tmpint & 0x00FF0000) >> 16;
	ptr[VH_SCHHI] = (tmpint & 0xFF000000) >> 24;
	cbpptr += sizeof(u_int);

	return;	
}

/*
 *-----------------------------------------------------------------------------
 * dos_init_fat()
 * This routine is used to initialize the in-core fat.
 *-----------------------------------------------------------------------------
 */
void	dos_init_fat(dfs_t *dfs)
{
	dfs->dfs_fat[0] = DOS_FATID;
	dfs->dfs_fat[1] = 0xFF;
	dfs->dfs_fat[2] = 0xFF;
	dfs->dfs_fat[3] = 0xFF;
	return;
}

/*
 *-----------------------------------------------------------------------------
 * dos_init_root()
 * This routine is used to initialize the in-core root.
 *-----------------------------------------------------------------------------
 */
void	dos_init_root(dfs_t *dfs)
{
	/* Duh, nothing to be done ?! */
	return;
}

/*
 *-----------------------------------------------------------------------------
 * dos_print_bootp()
 * This routine is used to print out the boot sector parameters.
 *-----------------------------------------------------------------------------
 */
void    dos_print_bootp(dfs_t *dfs, bp_t *bootp)
{
        printf(" ----------------------------------------------------------\n");
        printf("                     BOOT SECTOR                           \n");
        printf(" ----------------------------------------------------------\n");
	printf(" dos_partaddr	      = %d\n", dfs->dfs_partaddr);
        printf(" dos_fataddr1         = %d\n", dfs->dfs_fataddr1);
        printf(" dos_fataddr2         = %d\n", dfs->dfs_fataddr2);
        printf(" dos_fatsize          = %d\n", dfs->dfs_fatsize);
        printf(" dos_rootaddr         = %d\n", dfs->dfs_rootaddr);
        printf(" dos_rootsize         = %d\n", dfs->dfs_rootsize);
        printf(" dos_fileaddr         = %d\n", dfs->dfs_fileaddr);
        printf(" bp_magic[1]          = %x\n", bootp->bp_magic[0]);
        printf(" bp_magic[2]          = %x\n", bootp->bp_magic[1]);
        printf(" bp_magic[3]          = %x\n", bootp->bp_magic[2]);
        printf(" bp_oem               = %s\n", bootp->bp_oem);
        printf(" bp_sectorsize        = %d\n", bootp->bp_sectorsize);
        printf(" bp_sectorspercluster = %d\n", bootp->bp_sectorspercluster);
        printf(" bp_reservecount      = %d\n", bootp->bp_reservecount);
        printf(" bp_fatcount          = %d\n", bootp->bp_fatcount);
        printf(" bp_rootentries       = %d\n", bootp->bp_rootentries);
        printf(" bp_sectorcount       = %d\n", bootp->bp_sectorcount);
        printf(" bp_mediatype         = %d\n", bootp->bp_mediatype);
        printf(" bp_fatsize           = %d\n", bootp->bp_fatsize);
        printf(" bp_sectorspertrack   = %d\n", bootp->bp_sectorspertrack);
        printf(" bp_headcount         = %d\n", bootp->bp_headcount);
        printf(" bp_hiddensectors     = %d\n", bootp->bp_hiddensectors);
        printf(" bp_numbrsectors      = %d\n", bootp->bp_numsectors);
        printf(" ----------------------------------------------------------\n");
        return;
}

/*
 *-----------------------------------------------------------------------------
 * dos_time()
 *-----------------------------------------------------------------------------
 */
void    dos_time(u_char *d_date, u_char *d_time)
{
        struct tm  tm;
        time_t     mtime;

        mtime = time(NULL);
        tm = *localtime(&mtime);
        d_date[0] = tm.tm_mday | (((tm.tm_mon + 1) & 0x7) << 5);
        d_date[1] = ((tm.tm_mon + 1) >> 3) | ((tm.tm_year - 80) << 1);
        d_time[0] = (tm.tm_sec / 2) | ((tm.tm_min & 0x7) << 5);
        d_time[1] = (tm.tm_min >> 3) | (tm.tm_hour << 3);
        return;
}

/*
 *-----------------------------------------------------------------------------
 * dos_check_label()
 *-----------------------------------------------------------------------------
 */
void     dos_check_label(char *outlabel, char *inlabel)
{
        int     retval = 0;
        int     count;
        char    vlabel[MAX_DOS_VOL_NAME];
        char    *strp;

        /* blank the label with space character */
        memset (vlabel, ' ', MAX_DOS_VOL_NAME);
        /* check  the illegal printable chars in the volume label */
        if (strpbrk(inlabel, "*?/\\|.,;:+=<>[]()&^")){
		strcpy(outlabel, "");
                return;
	}
        count = 0;
        while (*inlabel && count < MAX_DOS_VOL_NAME) {
                if (!isprint(*inlabel)){
			if (retval == 0){
				strcpy(outlabel, "");
                        	return;
			}
		}
                else if (isspace(*inlabel) || *inlabel == '\t') {
                        vlabel[count++] = ' ';
                        inlabel = spaceskip(inlabel);
                }
                else    vlabel[count++] = *inlabel++;
        }
        strncpy(outlabel, vlabel, MAX_DOS_VOL_NAME);
	for (count = 0; count < MAX_DOS_VOL_NAME; count++)
		outlabel[count] = toupper(outlabel[count]);
        return;
}

/*
 *-----------------------------------------------------------------------------
 * dos_write_label()
 * This routine is used to write out the volume label to disk.
 *-----------------------------------------------------------------------------
 */
void    dos_write_label(char *label, u_long label_addr)
{
        u_char  *buff;
        dfile_t fentry;
        char    vlabel[MAX_DOS_VOL_NAME+1];


        dos_check_label(vlabel, label);
	if (!strcmp(vlabel, ""))
                return;

        buff = (u_char *) gmalloc(SECTOR_SIZE);
        bzero(buff, SECTOR_SIZE);
        bzero(&fentry, sizeof(fentry));

        strncpy(fentry.df_name, vlabel, FILE_LEN);
        if (strlen(vlabel) > FILE_LEN)
                strncpy(fentry.df_ext, vlabel+FILE_LEN, EXTN_LEN);

        fentry.df_attribute = ATTRIB_ARCHIVE | ATTRIB_LABEL;
        dos_time(fentry.df_date, fentry.df_time);
        memcpy(buff, &fentry, sizeof(fentry));

	gseek(label_addr);
	gwrite(buff, SECTOR_SIZE);

        return;
}

/*
 *-----------------------------------------------------------------------------
 * compute_fatsize()
 * This routine is used to compute the number of sectors that are to be
 * reserved for the FAT.
 *-----------------------------------------------------------------------------
 */
u_int   compute_fatsize(u_int sect_count, u_int sect_reserved,
                        u_int sect_percluster, u_int dos_rootsize)
{
        u_int   dos_fatsize;
        u_int   num, den;

        num = (sect_count-sect_reserved-dos_rootsize+2*sect_percluster);
        den = (256*sect_percluster+2);
        dos_fatsize = ceiling(num, den);
        return (dos_fatsize);
}

/*
 *-----------------------------------------------------------------------------
 * compute_sectorspercluster()
 * This routine is used to compute the number of sectors per cluster.
 *-----------------------------------------------------------------------------
 */
u_int   compute_sectorspercluster(u_int size)
{
        u_int   sect_count;
        u_int   sect_percluster;

        sect_count = size/SECTOR_SIZE;
        if (sect_count <= 64*K*4)
                sect_percluster = 4;
        else if (sect_count <= 64*K*8)
                sect_percluster = 8;
        else if (sect_count <= 64*K*16)
                sect_percluster = 16;
        else if (sect_count <= 64*K*32)
                sect_percluster = 32;
        else if (sect_count <= 64*K*64)
                sect_percluster = 64;
        else    sect_percluster = 128;
        return (sect_percluster);
}



