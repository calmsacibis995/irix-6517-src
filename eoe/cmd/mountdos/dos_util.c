/*
 *=============================================================================
 * 		File:		dos_util.c
 *		Purpose:	This file constitutes the "VFat" layer, which
 *				consists of remnants from "dos_utils.c" and a 
 *				whole bunch of functions that are required to
 *				support Windows 95 style long file names.
 *=============================================================================
 */
#include	<stdio.h>
#include	<sys/bsd_types.h>
#include 	<ctype.h>
#include 	<errno.h>
#include 	<stdlib.h>
#include 	<string.h>
#include 	<sys/syslog.h>

#include	"dos_fs.h"

extern	int	debug;		/* Debug flag */

struct unicode_value {
        unsigned char uni1;
        unsigned char uni2;
};

struct unicode_value a2uni[];    /* Ascii to Unicode conversion table */
unsigned char *uni2asc_pg[];	 /* Unicode to Ascii conversion table */

struct unicode_value a2uni[256] = {
/* 0x00 */
{0x00, 0x00}, {0x01, 0x00}, {0x02, 0x00}, {0x03, 0x00},
{0x04, 0x00}, {0x05, 0x00}, {0x06, 0x00}, {0x07, 0x00},
{0x08, 0x00}, {0x09, 0x00}, {0x0A, 0x00}, {0x0B, 0x00},
{0x0C, 0x00}, {0x0D, 0x00}, {0x0E, 0x00}, {0x0F, 0x00},
/* 0x10 */
{0x10, 0x00}, {0x11, 0x00}, {0x12, 0x00}, {0x13, 0x00},
{0x14, 0x00}, {0x15, 0x00}, {0x16, 0x00}, {0x17, 0x00},
{0x18, 0x00}, {0x19, 0x00}, {0x1A, 0x00}, {0x1B, 0x00},
{0x1C, 0x00}, {0x1D, 0x00}, {0x1E, 0x00}, {0x1F, 0x00},
/* 0x20 */
{0x20, 0x00}, {0x21, 0x00}, {0x22, 0x00}, {0x23, 0x00},
{0x24, 0x00}, {0x25, 0x00}, {0x26, 0x00}, {0x27, 0x00},
{0x28, 0x00}, {0x29, 0x00}, {0x2A, 0x00}, {0x2B, 0x00},
{0x2C, 0x00}, {0x2D, 0x00}, {0x2E, 0x00}, {0x2F, 0x00},
/* 0x30 */
{0x30, 0x00}, {0x31, 0x00}, {0x32, 0x00}, {0x33, 0x00},
{0x34, 0x00}, {0x35, 0x00}, {0x36, 0x00}, {0x37, 0x00},
{0x38, 0x00}, {0x39, 0x00}, {0x3A, 0x00}, {0x3B, 0x00},
{0x3C, 0x00}, {0x3D, 0x00}, {0x3E, 0x00}, {0x3F, 0x00},
/* 0x40 */
{0x40, 0x00}, {0x41, 0x00}, {0x42, 0x00}, {0x43, 0x00},
{0x44, 0x00}, {0x45, 0x00}, {0x46, 0x00}, {0x47, 0x00},
{0x48, 0x00}, {0x49, 0x00}, {0x4A, 0x00}, {0x4B, 0x00},
{0x4C, 0x00}, {0x4D, 0x00}, {0x4E, 0x00}, {0x4F, 0x00},
/* 0x50 */
{0x50, 0x00}, {0x51, 0x00}, {0x52, 0x00}, {0x53, 0x00},
{0x54, 0x00}, {0x55, 0x00}, {0x56, 0x00}, {0x57, 0x00},
{0x58, 0x00}, {0x59, 0x00}, {0x5A, 0x00}, {0x5B, 0x00},
{0x5C, 0x00}, {0x5D, 0x00}, {0x5E, 0x00}, {0x5F, 0x00},
/* 0x60 */
{0x60, 0x00}, {0x61, 0x00}, {0x62, 0x00}, {0x63, 0x00},
{0x64, 0x00}, {0x65, 0x00}, {0x66, 0x00}, {0x67, 0x00},
{0x68, 0x00}, {0x69, 0x00}, {0x6A, 0x00}, {0x6B, 0x00},
{0x6C, 0x00}, {0x6D, 0x00}, {0x6E, 0x00}, {0x6F, 0x00},
/* 0x70 */
{0x70, 0x00}, {0x71, 0x00}, {0x72, 0x00}, {0x73, 0x00},
{0x74, 0x00}, {0x75, 0x00}, {0x76, 0x00}, {0x77, 0x00},
{0x78, 0x00}, {0x79, 0x00}, {0x7A, 0x00}, {0x7B, 0x00},
{0x7C, 0x00}, {0x7D, 0x00}, {0x7E, 0x00}, {0x7F, 0x00},
/* 0x80 */
{0xC7, 0x00}, {0xFC, 0x00}, {0xE9, 0x00}, {0xE2, 0x00},
{0xE4, 0x00}, {0xE0, 0x00}, {0xE5, 0x00}, {0xE7, 0x00},
{0xEA, 0x00}, {0xEB, 0x00}, {0xE8, 0x00}, {0xEF, 0x00},
{0xEE, 0x00}, {0xEC, 0x00}, {0xC4, 0x00}, {0xC5, 0x00},
/* 0x90 */
{0xC9, 0x00}, {0xE6, 0x00}, {0xC6, 0x00}, {0xF4, 0x00},
{0xF6, 0x00}, {0xF2, 0x00}, {0xFB, 0x00}, {0xF9, 0x00},
{0xFF, 0x00}, {0xD6, 0x00}, {0xDC, 0x00}, {0xF8, 0x00},
{0xA3, 0x00}, {0xD8, 0x00}, {0xD7, 0x00}, {0x92, 0x00},
/* 0xA0 */
{0xE1, 0x00}, {0xE0, 0x00}, {0xF3, 0x00}, {0xFA, 0x00},
{0xF1, 0x00}, {0xD1, 0x00}, {0xAA, 0x00}, {0xBA, 0x00},
{0xBF, 0x00}, {0xAE, 0x00}, {0xAC, 0x00}, {0xBD, 0x00},
{0xBC, 0x00}, {0xA1, 0x00}, {0xAB, 0x00}, {0xBB, 0x00},
/* 0xB0 */
{0x91, 0x25}, {0x92, 0x25}, {0x93, 0x25}, {0x02, 0x25},
{0x24, 0x25}, {0xC1, 0x00}, {0xC2, 0x00}, {0xC0, 0x00},
{0xA9, 0x00}, {0x63, 0x25}, {0x51, 0x25}, {0x57, 0x25},
{0x5D, 0x25}, {0xA2, 0x00}, {0xA5, 0x00}, {0x10, 0x25},
/* 0xC0 */
{0x14, 0x25}, {0x34, 0x25}, {0x2C, 0x25}, {0x1C, 0x25},
{0x00, 0x25}, {0x3C, 0x25}, {0xE3, 0x00}, {0xC3, 0x00},
{0x5A, 0x25}, {0x54, 0x25}, {0x69, 0x25}, {0x66, 0x25},
{0x60, 0x25}, {0x50, 0x25}, {0x6C, 0x25}, {0xA4, 0x00},
/* 0xD0 */
{0xF0, 0x00}, {0xD0, 0x00}, {0xCA, 0x00}, {0xCB, 0x00},
{0xC8, 0x00}, {0x31, 0x01}, {0xCD, 0x00}, {0xCE, 0x00},
{0xCF, 0x00}, {0x18, 0x25}, {0x0C, 0x25}, {0x88, 0x25},
{0x84, 0x25}, {0xA6, 0x00}, {0xCC, 0x00}, {0x80, 0x25},
/* 0xE0 */
{0xD3, 0x00}, {0xDF, 0x00}, {0xD4, 0x00}, {0xD2, 0x00},
{0xF5, 0x00}, {0xD5, 0x00}, {0xB5, 0x00}, {0xFE, 0x00},
{0xDE, 0x00}, {0xDA, 0x00}, {0xDB, 0x00}, {0xD9, 0x00},
{0xFD, 0x00}, {0xDD, 0x00}, {0xAF, 0x00}, {0xB4, 0x00},
/* 0xF0 */
{0xAD, 0x00}, {0xB1, 0x00}, {0x17, 0x20}, {0xBE, 0x00},
{0xB6, 0x00}, {0xA7, 0x00}, {0xF7, 0x00}, {0xB8, 0x00},
{0xB0, 0x00}, {0xA8, 0x00}, {0xB7, 0x00}, {0xB9, 0x00},
{0xB3, 0x00}, {0xB2, 0x00}, {0xA0, 0x25}, {0xA0, 0x00}
};

unsigned char page00[256] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, /* 0x00-0x07 */
        0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, /* 0x08-0x0F */
        0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, /* 0x10-0x17 */
        0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, /* 0x18-0x1F */
        0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, /* 0x20-0x27 */
        0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, /* 0x28-0x2F */
        0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, /* 0x30-0x37 */
        0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F, /* 0x38-0x3F */
        0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, /* 0x40-0x47 */
        0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F, /* 0x48-0x4F */
        0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, /* 0x50-0x57 */
        0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F, /* 0x58-0x5F */
        0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, /* 0x60-0x67 */
        0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F, /* 0x68-0x6F */
        0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, /* 0x70-0x77 */
        0x78, 0x79, 0x7A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F, /* 0x78-0x7F */

        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x80-0x87 */
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x88-0x8F */
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x90-0x97 */
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x92, /* 0x98-0x9F */
        0xFF, 0xAD, 0xBD, 0x9C, 0xCF, 0xBE, 0xDD, 0xF5, /* 0xA0-0xA7 */
        0xF9, 0xB8, 0x00, 0xAE, 0xAA, 0xF0, 0x00, 0xEE, /* 0xA8-0xAF */
        0xF8, 0xF1, 0xFD, 0xFC, 0xEF, 0xE6, 0xF4, 0xFA, /* 0xB0-0xB7 */
        0xF7, 0xFB, 0x00, 0xAF, 0xAC, 0xAB, 0xF3, 0x00, /* 0xB8-0xBF */
        0xB7, 0xB5, 0xB6, 0xC7, 0x8E, 0x8F, 0x92, 0x80, /* 0xC0-0xC7 */
        0xD4, 0x90, 0xD2, 0xD3, 0xDE, 0xD6, 0xD7, 0xD8, /* 0xC8-0xCF */
        0x00, 0xA5, 0xE3, 0xE0, 0xE2, 0xE5, 0x99, 0x9E, /* 0xD0-0xD7 */
        0x9D, 0xEB, 0xE9, 0xEA, 0x9A, 0xED, 0xE8, 0xE1, /* 0xD8-0xDF */
        0xA1, 0xA0, 0x83, 0xC6, 0x84, 0x86, 0x91, 0x87, /* 0xE0-0xE7 */
        0x8A, 0x82, 0x88, 0x89, 0x8D, 0x00, 0x8C, 0x8B, /* 0xE8-0xEF */
        0xD0, 0xA4, 0x95, 0xA2, 0x93, 0xE4, 0x94, 0xF6, /* 0xF0-0xF7 */
        0x9B, 0x97, 0xA3, 0x96, 0x81, 0xEC, 0xE7, 0x98  /* 0xF8-0xFF */
};

unsigned char page25[256] = {
        0xC4, 0x00, 0xB3, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x00-0x07 */
        0x00, 0x00, 0x00, 0x00, 0xDA, 0x00, 0x00, 0x00, /* 0x08-0x0F */
        0xBF, 0x00, 0x00, 0x00, 0xC0, 0x00, 0x00, 0x00, /* 0x10-0x17 */
        0xD9, 0x00, 0x00, 0x00, 0xC3, 0x00, 0x00, 0x00, /* 0x18-0x1F */
        0x00, 0x00, 0x00, 0x00, 0xB4, 0x00, 0x00, 0x00, /* 0x20-0x27 */
        0x00, 0x00, 0x00, 0x00, 0xC2, 0x00, 0x00, 0x00, /* 0x28-0x2F */
        0x00, 0x00, 0x00, 0x00, 0xC1, 0x00, 0x00, 0x00, /* 0x30-0x37 */
        0x00, 0x00, 0x00, 0x00, 0xC5, 0x00, 0x00, 0x00, /* 0x38-0x3F */
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x40-0x47 */
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x48-0x4F */
        0xCD, 0xBA, 0x00, 0x00, 0xC9, 0x00, 0x00, 0xBB, /* 0x50-0x57 */
        0x00, 0x00, 0xC8, 0x00, 0x00, 0xBC, 0x00, 0x00, /* 0x58-0x5F */
        0xCC, 0x00, 0x00, 0xB9, 0x00, 0x00, 0xCB, 0x00, /* 0x60-0x67 */
        0x00, 0xCA, 0x00, 0x00, 0xCE, 0x00, 0x00, 0x00, /* 0x68-0x6F */
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x70-0x77 */
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x78-0x7F */

        0xDF, 0x00, 0x00, 0x00, 0xDC, 0x00, 0x00, 0x00, /* 0x80-0x87 */
        0xDB, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x88-0x8F */
        0x00, 0xB0, 0xB1, 0xB2, 0x00, 0x00, 0x00, 0x00, /* 0x90-0x97 */
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x98-0x9F */
        0xFE, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0xA0-0xA7 */
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0xA8-0xAF */
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0xB0-0xB7 */
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0xB8-0xBF */
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0xC0-0xC7 */
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0xC8-0xCF */
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0xD0-0xD7 */
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0xD8-0xDF */
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0xE0-0xE7 */
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0xE8-0xEF */
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0xF0-0xF7 */
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00  /* 0xF8-0xFF */
};

unsigned char *uni2asc_pg[256] = {
        page00, NULL, NULL, NULL, NULL, NULL, NULL, NULL, /* 0x00-0x07 */
        NULL,   NULL, NULL, NULL, NULL, NULL, NULL, NULL, /* 0x08-0x0F */
        NULL,   NULL, NULL, NULL, NULL, NULL, NULL, NULL, /* 0x10-0x17 */
        NULL,   NULL, NULL, NULL, NULL, NULL, NULL, NULL, /* 0x18-0x1F */
        NULL,   NULL, NULL, NULL, page25		  /* 0x20-0x27 */
};

extern u_short	fat_entry_size;
extern u_long   partsize;       /* Partition size      */
extern u_long   partbgnaddr;    /* Partition beginning */
extern u_long   partendaddr;    /* Partition ending    */
extern u_int    sectorcount;    /* Number of sectors in this partition */
extern char     nullext[];

int	dbg = 1;
static 	dirhash_t        *dirhash[DIRHASHSIZE];
static  char skip_chars[]       = ".:\"?<>| ";
static  char bad_chars[]        = "*?<>|\":/\\";
static  char replace_chars[]    = "[];,+=";

/*
 *-----------------------------------------------------------------------------
 * vfat_update_disk_fat()
 * This routine writes out the in-core fat to disk.
 *-----------------------------------------------------------------------------
 */
int	vfat_update_disk_fat(dfs_t *dfs)
{
        int     i;
        int     error;
        int     bad_fat_count = 0;

        if (!dfs->dirtyfat)
                return (0);
        lseek(dfs->dfs_fd, dfs->dfs_fataddr, 0);
        for (i = 0; i < dfs->dfs_bp.bp_fatcount; i++) {
                if (write(dfs->dfs_fd, dfs->dfs_fat, dfs->dfs_fatsize) < 0) {
                        error = errno;
                        bad_fat_count++;
                        lseek(dfs->dfs_fd,
                              dfs->dfs_fataddr+dfs->dfs_fatsize*(i + 1), 0);
                }
        }
        if (bad_fat_count == dfs->dfs_bp.bp_fatcount)
                return (error);
        dfs->dirtyfat = FALSE;
        return 0;
}

/*
 *-----------------------------------------------------------------------------
 * vfat_update_disk_root()
 * This routine writes out the in-core root to disk.
 *-----------------------------------------------------------------------------
 */
int	vfat_update_disk_root(dfs_t *dfs)
{
        if (!dfs->dirtyrootdir)
                return (0);
        lseek(dfs->dfs_fd, dfs->dfs_rootaddr, 0);
        if (write(dfs->dfs_fd, &dfs->dfs_root->d_dir[2],
                  (dfs->dfs_rootsize-2*DIR_ENTRY_SIZE)) < 0)
                return (errno);
        dfs->dirtyrootdir = FALSE;
        return (0);
}

/*
 *-----------------------------------------------------------------------------
 * vfat_create_sname()
 * This routine is used to convert a long name into a short name.
 * NOTE:
 * If the lname can   directly be used as sname, sflag = 1.
 * If the lname can't directly be used as sname, sflag = 0.
 *-----------------------------------------------------------------------------
 */
int     vfat_create_sname(dnode_t *dp, char *lname, char *sname, int *sflag)
{
	file_t	f;
        char    *ip, *op;
        char    *lname_ext;
        char    msdos_name[13];
        char    base[NAME_LEN+1], ext[EXTN_LEN+1];
        int     i, error, length, blen, elen;

	printf(" vfat_create_sname()\n");
	*sflag  = 0;
	length = strlen(lname);
        if (length && lname[length-1] == ' ')
                return (ERROR);
        if (length <= MSDOS_NAME+1){
                /*
                 * Attempt to directly use this as a shortname.
                 * Convert lowercase to uppercase and try it.
                 */
                for (i = 0, ip = lname, op = msdos_name; 
                     i < length; i++,ip++,op++){
                        if (*ip >= 'a' && *ip <= 'z')
                                *op = *ip-32;
                        else    *op = *ip;
                }
                error = vfat_format_sname(msdos_name, sname, length);
                if (!error){
			error = vfat_dir_getent_sname(dp, msdos_name, &f);
			if (error != ENOENT)
                                return (ERROR);
			*sflag = 1;
                        return (0);
                }
        }
        vfat_type_lname(lname, &lname_ext, &length);
        vfat_compose_base(lname, lname_ext, length, base, &blen);
        vfat_compose_ext( lname, lname_ext, length, ext, &elen);
        
        if (blen == 0)
                return (ERROR);

        error = vfat_compose_sname(dp, msdos_name, sname, 
				   base, blen, ext, elen);
        return (error);
}

/*
 *-----------------------------------------------------------------------------
 * vfat_type_lname()
 * This routine is used to determine type of long name.
 * For type A: extension need not be created.
 * For type B: extension need be created.
 * - Type A: (XXXXXX, XXXXX., ...XXX)
 * - Type B: (XXX.XX)
 * Output:
 * - Type A: (lname_ext == 0, lname_size = len(lname))
 * - Type B: (lname_ext != 0, lname_size = x)
 *-----------------------------------------------------------------------------
 */
int     vfat_type_lname(char *lname, char **lname_ext, int *lname_size)
{
        int     length;
        char    *t_bgn, *t_end, *t_ext;

        length = *lname_size;
        t_ext  = t_end = &lname[length];
        while (--t_ext >= lname){
                if (*t_ext == '.'){
                        if (t_ext == t_end-1){
                                *lname_size = length;
                                t_ext = 0;
                        }
                        break;
                }
        }
        if (t_ext == lname-1){
                *lname_size = length;
                t_ext = 0;
        }
        else if (t_ext){
                for (t_bgn = &lname[0]; t_bgn < t_ext; t_bgn++){
                        if (!strchr(skip_chars, *t_bgn))
                                break;
                }
                if (t_bgn != t_ext){
                        *lname_size = t_ext-lname;
                        t_ext++;
                }
                else {
                        *lname_size = length;
                        t_ext = 0;
                }
        }
        *lname_ext = t_ext;
        return (0);
}

/*
 *-----------------------------------------------------------------------------
 * vfat_compose_base()
 * This routine accepts a long name, a pointer to the start of the extension
 * as well as size (which indicates number of characters to be scanned to form
 * the base), and then composes a base, and calculates its base length.
 *-----------------------------------------------------------------------------
 */
int     vfat_compose_base(char *lname, char *lname_ext, int lname_size,
                       char *base,  int *baselen)
{
        int     blen;
        char    *ip, *op;

	printf(" vfat_compose_base() \n");
        for (blen = 0, ip = lname, op = base;
             blen < NAME_LEN && lname_size;
             ip++, lname_size--){
                if (!strchr(skip_chars, *ip)){
                        if (*ip >= 'a' && *ip <= 'z')
                                *op = *ip-32;
                        else    *op = *ip;
                        if (strchr(replace_chars, *ip))
                                *op = '_';
                        op++; blen++;
                }

        }
        *baselen = blen;
        base[blen] = '\0';
        return (0);
}

/*
 *-----------------------------------------------------------------------------
 * vfat_compose_ext()
 * This routine accepts a long name, a pointer to the start of the extension
 * and then composes an extension, and calculates its extension length.
 *-----------------------------------------------------------------------------
 */
int     vfat_compose_ext(char *lname, char *lname_ext, int lname_size,
                         char *ext,   int *extlen)
{
        int     elen;
        char    *ip, *op;

	printf(" vfat_compose_ext() \n");
        if (lname_ext == 0){
                *extlen = 0;
                return (0);
        }
        for (elen = 0, ip = lname_ext, op = ext;
             elen < EXTN_LEN && *ip != '\0'; ip++){
                if (!strchr(skip_chars, *ip)){
                        if (*ip >= 'a' && *ip <= 'z')
                                *op = *ip-32;
                        else    *op = *ip;
                        if (strchr(replace_chars, *ip))
                                *op = '_';
                        op++; elen++;
                }
        }
        *extlen = elen;
        ext[elen] = '\0';
        return (0);
}

/*
 *-----------------------------------------------------------------------------
 * vfat_compose_sname()
 * This routine accepts the extracted base, ext and then attempts to compose
 * a shortname, trying each time to see that there is no conflict.
 *-----------------------------------------------------------------------------
 */
int     vfat_compose_sname(dnode_t *dp, 
			   char *sname1, char *sname2, 
			   char *base, int blen,  
			   char *ext,  int elen)
{
	file_t	f;
        char    tilde[NAME_LEN];
        int     count, size;
        int     total, error, spaces;

	printf(" vfat_compose_sname()\n");
        count = 0;
        error = 0;
        sname1[blen] = '.';
        strcpy(sname1, base);
        strcpy(sname1+blen+1, ext);
        spaces = NAME_LEN-blen;
        total  = blen+elen+1;
        while (error != ENOENT){
                count++;
                if (count == 10000000)
                        return (ERROR);
                sprintf(tilde, "%d", count);
                size = strlen(tilde);
                if (spaces < size+1){
                        blen -= (size+1-spaces);
                        spaces   = (size+1);
                }
                strncpy(sname1, base, blen);
                sname1[blen] = '~';
                strcpy(sname1+blen+1, tilde);
                sname1[blen+size+1] = '.';
                strcpy(sname1+blen+size+2, ext);
                total = blen+elen+size+2;
                error = vfat_format_sname(sname1, sname2, total);
                if (!error){
                        printf(" ATTEMPT: %s\n", vfat_print11(sname2));
			error = vfat_dir_getent_sname(dp, sname2, &f);
                }
        }
	printf(" SUCCESS: %s\n", vfat_print11(sname2));
        return (0);
}


/*
 *-----------------------------------------------------------------------------
 * vfat_format_sname()
 * This routine accepts a short name and attempts to convert it into a MSDOS
 * name; If this conversion isn't possible, an error is returned. If it *is*
 * possible, the corresponding MSDOS name is returned in 'sname2'.
 *-----------------------------------------------------------------------------
 */
int     vfat_format_sname(char *sname1, char *sname2, int len)
{
        int     spc_flg;
        char    ch, *t1, *t2;

	printf(" vfat_format_sname() \n");
        ch  = 0;
        spc_flg = 1;
        for (t1 = sname1, t2 = sname2; len && t2-sname2 < NAME_LEN; t2++){
                ch = *t1++;
                len--;
                if (ch == '.')
                        break;
                if (strchr(bad_chars, ch))
                        return (ERROR);
                if (strchr(replace_chars, ch))
                        return (ERROR);
                if (ch < ' ' || ch == ':' || ch == '\\')
                        return (ERROR);
                spc_flg = (ch == ' ');
		/* Test Return */
		if (spc_flg)
			return (ERROR);
                *t2 = (ch >= 'a' && ch <= 'z') ? ch-32:ch;
        }
        if (spc_flg)
                return (ERROR);
        if (len && ch != '.'){
                ch = *t1++;
                len--;
                if (ch != '.')
                return (ERROR); 
        }
        while (ch != '.' && len--)
                ch = *t1++;
        if (ch == '.'){
                while (t2-sname2 < NAME_LEN)
                        *t2++ = ' ';
                while (len > 0 && t2-sname2 < MSDOS_NAME){
                        ch = *t1++;
                        len--;
                        if (strchr(bad_chars, ch))
                                return (ERROR);
                        if (strchr(replace_chars, ch))
                                return (ERROR);
                        if (ch < ' ' || ch == ':' || ch == '\\' || ch == '.')
                                return (ERROR);
                        spc_flg = (ch == ' ');
			/* Test Return */
			if (spc_flg)
				return (ERROR);
                        *t2++ = (ch >= 'a' && ch <= 'z') ? ch-32:ch;
                }
                if (spc_flg)
                        return (ERROR);
                if (len)
                        return (ERROR);
        }
        while (t2-sname2 < MSDOS_NAME)
                *t2++ = ' ';
        return (0);
}


/*
 *-----------------------------------------------------------------------------
 * vfat_isvalid_lname()
 * This routine is used to check if a long name is valid or not.
 * If lname is valid, returns 0.
 * If lname isn't valid, returns 1.
 *-----------------------------------------------------------------------------
 */
int     vfat_isvalid_lname(char *lname, int lname_len)
{
        char    ch;
        int     i;

        if (lname_len && lname[lname_len-1] == ' ')
                return (ERROR);
        if (lname_len >= 256)
                return (ERROR);
        for (i = 0; i < lname_len; i++){
                ch = lname[i];
                if (strchr(bad_chars, ch))
                        return (ERROR);
        }
        return (0);
}

/*
 *-----------------------------------------------------------------------------
 * vfat_dir_lookup()
 * This routine accepts a node pointer to a directory entry, and looks into
 * the directory cache hash-tbl and returns the directory entry associated
 * with this file.
 * MAPS: (file's dp) => (file's file_t)
 * RETURNS:
 * (0)  : If directory entry was successfully returned, and file_t.
 * (-1) : If directory entry wasn't successfully returned.
 *-----------------------------------------------------------------------------
 */
int	vfat_dir_lookup(dnode_t *dp, file_t *fp)
{
	u_short		offset;
	u_short		cluster;
	dnode_t		*dirp;
	dirhash_t	*dh;

	offset  = OFFSET(dp->d_fno);
	cluster = CLUSTER(dp->d_fno);
	for (dh = DIRHASH(cluster); dh != NULL; dh = dh->next){
		if (dh->cluster == cluster){
			dirp = dh->dnode;
			fp->f_offset = offset;
			copy_dir_to_fp(dirp, fp);
			return (0);
		}
	}
	return (-1);
}

/*
 *-----------------------------------------------------------------------------
 * vfat_dir_getent_sname()
 * This routine accepts a directory node pointer and searches for an entry
 * inside it that matches the given shortname.
 * MAPS: (file's parent dp, sname) => (file's file_t)
 *-----------------------------------------------------------------------------
 */
int	vfat_dir_getent_sname(dnode_t *dp, char *sname, file_t *fp)
{
        return (vfat_dir_generic(dp, MODE_BRK, sname, fp, dirent_cmp_sname));
}

/*
 *-----------------------------------------------------------------------------
 * vfat_dir_getent_lname()
 * This routine accepts a directory node pointer and searches for an entry
 * inside it that matches the given longname.
 * MAPS: (file's parent dp, lname) => (file's file_t)
 *-----------------------------------------------------------------------------
 */
int	vfat_dir_getent_lname(dnode_t *dp, char *lname, file_t *fp)
{
        return (vfat_dir_generic(dp, MODE_BRK, lname, fp, dirent_cmp_lname));
}

/*
 *-----------------------------------------------------------------------------
 * vfat_dir_generic()
 * This routine accepts a directory node pointer and a function that is to
 * be invoked on each directory entry that is scanned. On success, this routine
 * might or might not return to the user depending upon "mode".
 * MODE	= MODE_BRK	=> stop once func() returns 0, continue otherwise.
 * MODE = MODE_CNT	=> continue all through.
 *-----------------------------------------------------------------------------
 */
int	vfat_dir_generic(dnode_t *dp, int mode, char *aux,
			 file_t *fp, int (*func)(file_t *, char *))
{
	slot_t	*sp;
	dfile_t	*de, *de_bgn, *de_end;

        int     id, error;
        int     state, ret;
        int     de_type;
        int     expect_slotid, cluster;
        int     dents_perclust;

	if (dp->d_dir == NULL){
		error = vfat_dir_read(dp);
		if (error)
			return (error);
	}
	/*
	 * Set states.
	 */
	state = ST_INIT;
	fp->f_nentries = 0;
	expect_slotid  = -1;
	CLEAR_SLOTS(fp);
	/*
	 * Start scanning all the directory entries
	 * that are present in this parent directory.
	 */
	de_bgn = dp->d_dir;	
	de_end = de_bgn+dp->d_size/DE_SIZ-1;
	for (de = de_bgn; de < de_end+1; de++){
		if (de->df_name[0] == LAST_ENTRY)
			break;
		de_type = vfat_dirent_type(de);
                /*
                 * Whenever we're in ST_INIT and we see
                 * a directory entry, we store its cluster
                 * and offset, and place it in "fp".
                 */
                if (state == ST_INIT){
        	        fp->f_offset = de-de_bgn;
                        if (dp->d_fno == ROOTFNO)
                        	fp->f_cluster = ROOT_CLUSTER;
                        else   	fp->f_cluster = to_cluster(dp, fp->f_offset);
                }
		if (state == ST_INIT && de_type == DE_DIRENT){
			fp->f_nentries = 0;
			COPY_SLOT_TO_FP(fp, de, 0);
			/*
			 * Finished reading in one dir entry.	
			 * Execute generic function on it.
			 */
			ret = (*func)(fp, aux);
			if (mode == MODE_BRK && ret == 0)
				return (ret);
			goto reset;
		}
		else if (state == ST_INIT && de_type == DE_SLOT){
			/*
			 * This is the first slot that we're seeing
			 * that might indicate the beginning of a 
			 * long file name. The first slot id has to
			 * be extracted.
			 */
			sp = (slot_t *) de;
			id = sp->sl_id & 0xBF;
			fp->f_nentries = id;
			COPY_SLOT_TO_FP(fp, de, id);	
			if (id == 1){
				state = ST_EXPECT_DIRENT;
				continue;
			}
			else {
				state = ST_EXPECT_SLOT;
				expect_slotid = id-1;
				continue;
			}
		}
		else if (state == ST_EXPECT_SLOT && de_type == DE_SLOT){
			sp = (slot_t *) de;
			id = sp->sl_id;
			if (id != expect_slotid)
				goto reset;
			COPY_SLOT_TO_FP(fp, de, id);
			if (id == 1){
				state = ST_EXPECT_DIRENT;
				expect_slotid = -1;
				continue;
			}
			else {
				state = ST_EXPECT_SLOT;
				expect_slotid -= 1;
				continue;
			}
		}
		else if (state == ST_EXPECT_DIRENT && de_type == DE_DIRENT){
			COPY_SLOT_TO_FP(fp, de, 0);
                        /*
                         * Finished reading one directory entry.
                         * Execute function passed, on it.
                         */
			ret = (*func)(fp, aux);
			if (mode == MODE_BRK && ret == 0)
				return (ret);
			goto reset;
		}
		else 	goto reset;
reset:
		state  = ST_INIT;
		fp->f_nentries = 0;
		expect_slotid  = -1;
	}
	/*
	 * A return from this point implies a failure.
	 * or that the func passed hasn't executed with
	 * any success or "BRK"'ed out.
	 */
	return (ENOENT);
}

/*
 *-----------------------------------------------------------------------------
 * vfat_dirent_read_disk()
 * This routine is used to read a particular file entry from disk.
 * INPUT: (fp->f_offset, fp->f_cluster) are only filled.
 * NOTE:  A particular file entry could be spread across more than one cluster.
 *-----------------------------------------------------------------------------
 */
int     vfat_dirent_read_disk(dfs_t *dfs, file_t *fp)
{
        int     error;
        slot_t  *sp;
        dfile_t *de;
        dfile_t *de_bgn, *de_end;
        int     state;
        int     nread, id, expect_slotid;
        int     nentries, de_per_clust, de_type;
        u_short offset, noffset;
        u_short cluster, ncluster;
        u_char  *cbuf;

        de_per_clust = DIR_ENTRY_PER_CLUSTER(dfs);
        CLEAR_SLOTS(fp);

        offset   = fp->f_offset;
        cluster  = fp->f_cluster;

        cbuf  = safe_malloc(dfs->dfs_clustersize);
        error = vfat_clust_read(dfs, DISK_ADDR(dfs, cluster), cbuf);
        if (error)
                goto error;

        state = ST_INIT;
        expect_slotid  = -1;
        noffset = offset % de_per_clust;

        de_bgn = (dfile_t *) cbuf;
        de_end = de_bgn+de_per_clust-1;
        de     = de_bgn+noffset;
        de_type= vfat_dirent_type(de);

        /* Take a peek at the directory entry at this offset */
        /* We then decide whether or not its part of a lname */

        if (de_type == DE_DIRENT){
                /* We've been asked to retrive a short name */
                fp->f_nentries = 0;
                COPY_SLOT_TO_FP(fp, de, 0);
                goto found_dirent;
        }
        else if (de_type == DE_FREE){
                /* We're looking at a blank directory entry */
                goto error;
        }

        /* We see the beginning of a long directory entry */
        /* This might or might not be spread over two clusts */
        
        sp = (slot_t *) de;
        id = sp->sl_id & 0xBF;
        fp->f_nentries = nentries = id;
        
        if (nentries == 1){
                /* We now expect to see a directory entry */
                state = ST_EXPECT_DIRENT;
                expect_slotid = -1;
        }
        else {
                /* We now expect to see a slot */
                state = ST_EXPECT_SLOT;
                expect_slotid = nentries-1;
        }

        /* Read all the slots that're part of this entry */
        /* that are present inside this partic. cluster  */
        de++;   
        while (de <= de_end){
                de_type = vfat_dirent_type(de);
                if (state == ST_EXPECT_DIRENT && de_type == DE_DIRENT){
                        COPY_SLOT_TO_FP(fp, de, 0);
                        goto found_dirent;
                }
                else if (state == ST_EXPECT_SLOT && de_type == DE_SLOT){
                        sp = (slot_t *) de;
                        id = sp->sl_id;
                        if (id != expect_slotid)
                                goto error;
                        COPY_SLOT_TO_FP(fp, de, id);
                        if (id == 1){
                                state = ST_EXPECT_DIRENT;
                                expect_slotid = -1;
                        }
                        else {
                                state = ST_EXPECT_SLOT;
                                expect_slotid -= 1;
                        }

                }
                else goto error;
                de++;
        }

        /* Duh! This damned directory entry is part of a lname */
        /* and it spills over to the next cluster in this dir  */
        /* Retrieve this next cluster, and keep grabbing slots */
        
        ncluster = (*dfs->dfs_readfat)(dfs, cluster);
        if (end_cluster(dfs, ncluster)){
                /* End of directory reached */
                goto error;
        }

        error = vfat_clust_read(dfs, DISK_ADDR(dfs, ncluster), cbuf);
        if (error)
                goto error;

        de_bgn = (dfile_t *) cbuf;
        de_end = de_bgn+de_per_clust-1;
        de     = de_bgn;
        while (de <= de_end){
                de_type = vfat_dirent_type(de);
                if (state == ST_EXPECT_DIRENT && de_type == DE_DIRENT){
                        COPY_SLOT_TO_FP(fp, de, 0);
                        goto found_dirent;
                }
                else if (state == ST_EXPECT_SLOT && de_type == DE_SLOT){
                        sp = (slot_t *) de;
                        id = sp->sl_id;
                        if (id != expect_slotid)
                                goto error;
                        COPY_SLOT_TO_FP(fp, de, id);
                        if (id == 1){
                                state = ST_EXPECT_DIRENT;
                                expect_slotid = -1;
                        }
                        else {
                                state = ST_EXPECT_SLOT;
                                expect_slotid -= 1;
                        }

                }
                else goto error;
                de++;
        }

error:
        free (cbuf);
        CLEAR_SLOTS(fp);
        return (1);

found_dirent:
        free (cbuf);
        return (0);
}

/*
 *-----------------------------------------------------------------------------
 * vfat_dir_writent()
 * This routine is used to write a particular file entry into a directory.
 * NOTE: We have to handle the case wherein a file entry spills into next clust
 *-----------------------------------------------------------------------------
 */
int     vfat_dir_writent(dnode_t *dp, file_t *fp)
{
        int     error;
        dfs_t   *dfs;
        u_long  daddr1;
        u_long  daddr2;
        u_long  clustadd;
        u_char  *clustbuf;
        u_short offset, eoffset;
        u_short cluster, ncluster;
        int     de_per_clust;
        int     nextclusterflag;

        dfs = dp->d_dfs;
        if (dp->d_dir == NULL){
                error = vfat_dir_read(dp);
                if (error)
                        return (error);
        }
        /*
         * Copy file directory entry into dir.
         */
        copy_fp_to_dir(dp, fp);
        if (fp->f_cluster == ROOT_CLUSTER){
                error = vfat_update_root(dfs);
                return (error);
        }
        
        offset  = fp->f_offset;
        eoffset = fp->f_offset+fp->f_nentries;
        cluster = fp->f_cluster;
        daddr1  = fp->f_offset-fp->f_offset % DIR_ENTRY_PER_CLUSTER(dfs);
        daddr2  = daddr1+DIR_ENTRY_PER_CLUSTER(dfs);

        /* Check to see if this file entry */
        /* spills into next cluster or not */

        if (eoffset >= daddr2)
                nextclusterflag = 1;
        else    nextclusterflag = 0;

        if (!nextclusterflag){
                /* No, we merely have to write out this cluster */
                clustadd = DISK_ADDR(dfs, cluster);
                clustbuf = (u_char *) &dp->d_dir[daddr1];

                error = vfat_clust_writ(dfs, clustadd, clustbuf);
                if (error)
                        return (error);
        }
        else {
                /* Yes, we have to write out this and next cluster */
                clustadd = DISK_ADDR(dfs, cluster);
                clustbuf = (u_char *) &dp->d_dir[daddr1];

                error = vfat_clust_writ(dfs, clustadd, clustbuf);
                if (error)
                        return (error);

                ncluster = (*dfs->dfs_readfat)(dfs, cluster);
                if (end_cluster(dfs, ncluster))
                        return (error);

                clustadd = DISK_ADDR(dfs, ncluster);
                clustbuf = (u_char *) &dp->d_dir[daddr2];
        
                error = vfat_clust_writ(dfs, clustadd, clustbuf);
                if (error)
                        return (error);
        }
        return (0);
}

/*
 *-----------------------------------------------------------------------------
 * vfat_dir_delent()
 * This routine is used to delete a particular file entry from a directory.
 * The file corresponding to the directory entry is also deleted from the fat.
 *-----------------------------------------------------------------------------
 */
int     vfat_dir_delent(dnode_t *dp, file_t *fp)
{
        dfs_t   *dfs;
        int     error;
	int	nentries;
        u_short cluster;
        u_short ncluster;

        /* 
	 * Remove FAT chain corresponding to the file 
	 */
	dfs = dp->d_dfs;
	cluster = START_CLUSTER(fp);
        do {
                ncluster = (*dfs->dfs_readfat)(dfs, cluster);
                (*dfs->dfs_writefat)(dfs, cluster, 0);
                dfs->dfs_freeclustercount++;
                cluster = ncluster;
        } while (!LAST_CLUSTER(dfs, cluster));

	error = vfat_update_fat(dfs);
        if (error)
                return (ERR);

	fp->f_entries[0].df_name[0] = REUSABLE_ENTRY;
	for (nentries = fp->f_nentries; nentries; nentries--)
		fp->f_entries[nentries].df_name[0] = REUSABLE_ENTRY;

	error = vfat_dir_writent(dp, fp);
	if (error)
		return (ERR);

	fp->f_nentries = 0;
        return (0);
}

/*
 *-----------------------------------------------------------------------------
 * vfat_dir_nulent()
 * This routine is used to delete a particular file entry from a directory
 * without removing the file itself.
 *-----------------------------------------------------------------------------
 */
int     vfat_dir_nulent(dnode_t *dp, file_t *fp)
{
        dfs_t   *dfs;
        int     error;
        int     nentries;
        u_short cluster;
        u_short ncluster;

        fp->f_entries[0].df_name[0] = REUSABLE_ENTRY;
        for (nentries = fp->f_nentries; nentries; nentries--)
                fp->f_entries[nentries].df_name[0] = REUSABLE_ENTRY;

        error = vfat_dir_writent(dp, fp);
        if (error)
                return (ERR);

        fp->f_nentries = 0;
        return (0);
}


/*
 *-----------------------------------------------------------------------------
 * vfat_dir_freent()
 * This routine accepts a pointer to a directory node and scans it looking for
 * "nentries" that are vacant and contiguous; If found, the cluster and offset
 * are returned; If not found, the directory file is expanded by one cluster.
 *-----------------------------------------------------------------------------
 */
int     vfat_dir_freent(dnode_t *dp, int nentries, u_short *clst, u_short *off)
{
	u_short	lclst;
	u_short	nclst;
	u_short	tmpoff;
	dfs_t	*dfs;
        dfile_t *de;
	dfile_t	*de_bgn, *de_end;
        int     de_type, error;
        int     state, tmpoffset;
        int     nfree, noffset;

	dfs = dp->d_dfs;
	if (dp->d_dir == NULL){
		error = vfat_dir_read(dp);
		if (error)
			return (ERR);
	}
	/*
	 * Root directory has a limited capacity,
	 * The other directories don't have limits.
	 */
	if (dp->d_fno == ROOTFNO){
		state  = ST_INIT;
		nfree  = 0;
		noffset= 0;
        	de_bgn = dp->d_dir;
        	de_end = de_bgn+dp->d_size/DE_SIZ-1;
		for (de = de_bgn; de < de_end+1; de++){
			de_type = vfat_dirent_type(de);
			if (state == ST_INIT && de_type == DE_FREE){
				nfree   = 1;
				noffset = de-de_bgn;
				*clst   = ROOT_CLUSTER;
				if (nfree == nentries+1){
					*off = noffset;
					return (0);
				}
				state = ST_EXPECT_FREE;
			}
			else if (state == ST_EXPECT_FREE && de_type == DE_FREE){
				nfree++;
				if (nfree == nentries+1){
					*off = noffset;
					return (0);
				}
				state = ST_EXPECT_FREE;	
			}
			else if (state == ST_EXPECT_FREE && de_type != DE_FREE){
				nfree   = 0;
				noffset = 0;
				*clst   = 0;
				state   = ST_INIT;
			}
		}
		return (ENOSPC);
	}
	else {
                state  = ST_INIT;
                nfree  = 0;
                noffset= 0;
                de_bgn = dp->d_dir;
                de_end = (de_bgn+dp->d_size/DE_SIZ-1);
                for (de = de_bgn; de < de_end+1; de++){
                        de_type = vfat_dirent_type(de);
                        if (state == ST_INIT && de_type == DE_FREE){
                                nfree   = 1;
                                noffset = de-de_bgn;
				*clst   = to_cluster(dp, noffset);
                                if (nfree == nentries+1){
                                        *off = noffset;
                                        return (0);
                                }
                                state = ST_EXPECT_FREE;
                        }
                        else if (state == ST_EXPECT_FREE && de_type == DE_FREE){
                                nfree++;
                                if (nfree == nentries+1){
                                        *off = noffset;
                                        return (0);
                                }
                                state = ST_EXPECT_FREE; 
                        }
                        else if (state == ST_EXPECT_FREE && de_type != DE_FREE){
                                nfree   = 0;
                                noffset = 0;
                                *clst   = 0;
                                state   = ST_INIT;
                        }
			else if (state == ST_INIT && de_type != DE_FREE){
                                nfree   = 0;
                                noffset = 0;
                                *clst   = 0;
                                state   = ST_INIT;
			}
                }
		/* We've reached the end of the directory file */
		/* We have no vacant entries that are enough   */
		/* We have to expand the directry file by 1    */
		/* Chain this cluster with the last in FAT */
		/* Bzero the entire new cluster and write it */
		/* (1) We might have already seen the beginning of a hole */
		/* (2) We might not have seen the beginning of a hole */

		nclst = vfat_free_fat(dfs);
		if (nclst == 0)
			return (ENOSPC);

		tmpoff = de-dp->d_dir;
		lclst  = to_cluster(dp, tmpoff-1);
		(*dfs->dfs_writefat)(dfs, lclst, nclst);

		dp->d_size += dfs->dfs_clustersize;
		dp->d_dir  =  (dfile_t *) realloc(dp->d_dir, dp->d_size);
		if (!dp->d_dir){
                        syslog(LOG_ERR, "Out of memory");
                        exit(1);
		}
		bzero(&dp->d_dir[tmpoff], dfs->dfs_clustersize);

		vfat_clust_writ(dfs, DISK_ADDR(dfs, nclst), dfs->dfs_zerobuf);
		if (*clst == 0){
			/* We return a hole that's entirely in the new clust */
			*clst = nclst;
			*off  = tmpoff;
		}
		else {
			/* We return a hole that began in a previous cluster */
			*off = noffset;
		}
	}
	error = vfat_update_fat(dfs);
	return (0);
}

/*
 *-----------------------------------------------------------------------------
 * vfat_dirent_type
 * This routine sees a particular directory entry and returns:
 * - DE_FREE  = free entry.
 * - DE_SLOT  = slot entry associated with a directory entry.
 * - DE_DIRENT= directory entry.
 * - DE_LABEL = volume label.
 *-----------------------------------------------------------------------------
 */
int     vfat_dirent_type(dfile_t *df)
{
        if (df->df_name[0] == LAST_ENTRY ||
            df->df_name[0] == REUSABLE_ENTRY)
                return (DE_FREE);
	else if (IS_LABEL(df))
		return (DE_LABEL);
	else if (df->df_attribute == 0x0F &&
            	 df->df_start[0]  == 0x00 && df->df_start[1] == 0x00)
                return (DE_SLOT);
        else    return (DE_DIRENT);
}

/*
 *-----------------------------------------------------------------------------
 * vfat_dirent_print()
 * This routine is used to print out a "file_t", it's lname/sname.
 *-----------------------------------------------------------------------------
 */
int	vfat_dirent_print(file_t *fp)
{
	char	sname[12];
	char	lname[255];

        vfat_dirent_upackname(fp, lname, sname);
	sname[11] = '\0';

        printf(" Sname = %s nentries = %d ", sname, fp->f_nentries);
	printf(" clust = %d off = %d\n", fp->f_cluster, fp->f_offset);

	fflush(stdout);
        return (0);
}

/*
 *-----------------------------------------------------------------------------
 * vfat_dirent_packname()
 * This routine accepts a lname/sname and a "file_t" and proceeds to pack
 * these two names into it.
 * NOTE:
 * Input sflag = 1 => sname is to be used as real name, no slots needed.
 * Input sflag = 0 => lname is to be used as real name, slots are needed.
 *-----------------------------------------------------------------------------
 */
int	vfat_dirent_packname(file_t *fp, char *lname, char *sname, int sflag)
{
        slot_t  *sp;
        dfile_t *de;
        u_char  chksum;
        int     end_flg;
        int     i, num_slots;
        int     lname_len, slot_id;
        char    *s, *sbgn, *send, *send1, *send2;

        end_flg   = 0;
        chksum    = vfat_sname_chksum(sname);
        lname_len = strlen(lname);
        num_slots = (lname_len+12)/13;
        /*
         * Fill in directory entry.
         * This is the very first slot.
         */
	de = fp->f_entries;
	strncpy(de->df_name, sname, 8);
	strncpy(de->df_ext,  sname+8, 3);
	if (sflag)
		goto directly_use_sname;
        for (slot_id = num_slots; slot_id; slot_id--){
                sp   = (slot_t *) &fp->f_entries[slot_id];
                sbgn = &lname[(slot_id-1)*13];
                send1= &lname[slot_id*13-1];
                send2= &lname[lname_len-1];
                if (send1 < send2)
                        send = send1;
                else    send = send2;
                end_flg = 0;
                /*
                 * Initialize the slot entry.
		 * The last slot id is OR'd with 0x40.
                 */
                sp->sl_id     = slot_id;
		if (slot_id == num_slots)
			sp->sl_id |= 0x40;
                sp->sl_attr   = ATTRIB_EXTNDD;
                sp->sl_chksum = chksum;
                sp->sl_reserved = 0;
                sp->sl_start[0] = 0;
                sp->sl_start[1] = 0;
                /*
                 * Break longname segment up into three parts.
                 * Pack each part with its ucodes.
                 * part1 = 5 chars:  sbgn.....sbgn+4
                 * part2 = 6 chars:  sbgn+5...sbgn+10
                 * part3 = 2 chars:  sbgn+11..sbgn+12
                 */
                for (s = sbgn, i = 0; s < sbgn+5; s++, i+= 2){
                        if (end_flg){
                                /* Go ahead and scribble 0xFF ucodes */
                                sp->sl_name0_4[i]  = 0xFF;
                                sp->sl_name0_4[i+1]= 0xFF;
                        }
			else {
                                if (s == send+1){
                                        /* Set end flag */
                                        /* Scribble 0x00 ucode and continue */
                                        end_flg = 1;
                                        sp->sl_name0_4[i]   = 0x00;
                                        sp->sl_name0_4[i+1] = 0x00;
                                        continue;
                                }
                                else {
                                        sp->sl_name0_4[i]   = a2ucode1(*s);
                                        sp->sl_name0_4[i+1] = a2ucode2(*s);
                                }
                        }
                }
                for (s = sbgn+5, i = 0; s < sbgn+11; s++, i += 2){
                        if (end_flg){
                                /* Go ahead and scribble 0xFF ucodes */
                                sp->sl_name5_10[i]  = 0xFF;
                                sp->sl_name5_10[i+1]= 0xFF;
                        }
                        else {
                                if (s == send+1){
                                        /* Set end flag */
                                        /* Scribble 0x00 ucode and continue */
                                        end_flg = 1;
                                        sp->sl_name5_10[i]   = 0x00;
                                        sp->sl_name5_10[i+1] = 0x00;
                                        continue;
                                }
                                else {
                                        sp->sl_name5_10[i]   = a2ucode1(*s);
                                        sp->sl_name5_10[i+1] = a2ucode2(*s);
                                }
                        }
                }
                for (s = sbgn+11, i = 0; s < sbgn+13; s++, i += 2){
                        if (end_flg){
                                /* Go ahead and scribble 0xFF ucodes */
                                sp->sl_name11_12[i]  = 0xFF;
                                sp->sl_name11_12[i+1]= 0xFF;
                        }
                        else {
                                if (s == send+1){
                                        /* Set end flag */
                                        /* Scribble 0x00 ucode and continue */
                                        end_flg = 1;
                                        sp->sl_name11_12[i]   = 0x00;
                                        sp->sl_name11_12[i+1] = 0x00;
                                        continue;
                                }
                                else {
                                        sp->sl_name11_12[i]   = a2ucode1(*s);
                                        sp->sl_name11_12[i+1] = a2ucode2(*s);
                                }
                        }

                }
        }
        fp->f_nentries = num_slots;
        return (0);

directly_use_sname:
	/* It looks like this file's long name */
	/* is identical to the short name. We  */
	/* hence need no slots to pack the name*/
	fp->f_nentries = 0;
	return (0);
}

/*
 *-----------------------------------------------------------------------------
 * vfat_dirent_upackname()
 * This routine accepts a "file_t" and extracts the lname/sname from inside it.
 *-----------------------------------------------------------------------------
 */
int	vfat_dirent_upackname(file_t *fp, char *lname, char *sname)
{
        slot_t  *sp;
        dfile_t *de;
	int	end_flg;
	int	broken;
        int     i, slot_id;
        char    *t = lname;
        u_char  chksum, byte1, byte2, ascii;
        u_short word;

        de = fp->f_entries;

        strncpy(sname,  de->df_name,8);
        strncpy(sname+8,de->df_ext, 3);

        chksum = vfat_sname_chksum(sname);

	if (fp->f_nentries == 0){
		to_unixname(de->df_name, de->df_ext, lname);

		return (0);
	}

        for (slot_id = 1; slot_id <= fp->f_nentries; slot_id++){

                sp = (slot_t *) &fp->f_entries[slot_id];

                /* Look at: sl_name0_4 */
                for (i = 0; i < 10; i += 2){

                        byte1 = sp->sl_name0_4[i];
                        byte2 = sp->sl_name0_4[i+1];

                        if (byte1 == 0xFF || byte2 == 0xFF)
                                return ERR;

                        if (byte1 == 0x00 && byte2 == 0x00)
                                goto end;

                        word  = (byte2 << 8) | (byte1);

			ascii = ucode2a(word, &end_flg);

			if (end_flg)
				goto end;

                        *t++  = ascii;
                }

                /* Look at: sl_name5_10 */
                for (i = 0; i < 12; i += 2){

                        byte1 = sp->sl_name5_10[i];
                        byte2 = sp->sl_name5_10[i+1];

                        if (byte1 == 0xFF || byte2 == 0xFF)
                                return ERR;

                        if (byte1 == 0x00 && byte2 == 0x00)
                                goto end;

                        word  = (byte2 << 8) | (byte1);

			ascii = ucode2a(word, &end_flg);

			if (end_flg)
				goto end;

                        *t++ = ascii;
                }

                /* Look at: sl_name11_12 */
                for (i = 0; i < 4; i+= 2){

                        byte1 = sp->sl_name11_12[i];
                        byte2 = sp->sl_name11_12[i+1];

                        if (byte1 == 0xFF || byte2 == 0xFF)
                                return ERR;

                        if (byte1 == 0x00 && byte2 == 0x00)
                                goto end;

                        word  = (byte2 << 8) | (byte1);

			ascii = ucode2a(word, &end_flg);

			if (end_flg)
				goto end;

                        *t++ = ascii;
                }
        }

end:
	*t = '\0';

	/*
	 * We need to perform a consistency check to 
	 * ensure that the long name does indeed map
	 * to this short name.
	 * Also check for invalid unicode detected (end == 2).
	 */
	broken = vfat_dirent_broken(fp);

	if (broken || (end_flg == 2)) {
		/* Merely return the short name as long name */
		fp->f_nentries = 0;
		to_unixname(de->df_name, de->df_ext, lname);	
	}

        return (0);

error:
        return (ERR);
}

/*
 *-----------------------------------------------------------------------------
 * vfat_dirent_broken()
 * This routine takes a "file_t" and checks to see if the checksum holds good.
 * Returns (0)	=> checksum is intact.
 * Returns (1)  => checksum is broken.
 *-----------------------------------------------------------------------------
 */
int	vfat_dirent_broken(file_t *fp)
{
	slot_t	*sp;
	dfile_t	*de;
	int	id;
	u_char	chksum;
	char	sname[MSDOS_NAME];

	if (fp->f_nentries == 0)
		return (0);

	de = &fp->f_entries[0];
	strncpy(sname,   de->df_name, 8);
	strncpy(sname+8, de->df_ext,  3);
	chksum = vfat_sname_chksum(sname);
	
	id = fp->f_nentries;
	while (id){
		sp = (slot_t *) &fp->f_entries[id];	
		if (sp->sl_chksum != chksum)
			return (1);
		id--;
	}
	return (0);
}

/*
 *-----------------------------------------------------------------------------
 * dirent_cmp_sname()
 * This routine takes a file_t and compares it against a short name.
 * NOTE: sname has to be in dos format (11 chars).
 *-----------------------------------------------------------------------------
 */
int	dirent_cmp_sname(file_t *fp, char *sname)
{
        char    sname2[11];
        char    lname[255];

        vfat_dirent_upackname(fp, lname, sname2);
	if (dosname_cmp(sname, sname2))
		return (0);
	return (1);
}

/*
 *-----------------------------------------------------------------------------
 * dirent_cmp_lname()
 * This routine takes a file_t and compares it against a long name.
 *-----------------------------------------------------------------------------
 */
int	dirent_cmp_lname(file_t *fp, char *lname)
{
	int	ret;
        char    sname[11];
        char    lname2[255];

        vfat_dirent_upackname(fp, lname2, sname);
	if (fp->f_nentries == 0){
		to_unixname(fp->f_entries[0].df_name,
			    fp->f_entries[0].df_ext, sname);
		/*
		 * Perform a case insensitive comparison between sname/lname 
		 */
		ret = dosname_cmp_lname(sname, lname);
		if (ret) return (0);
	}
	else {
		/*
		 * Perform a case insensitive comparison between lname/lname2
		 */
		ret = dosname_cmp_lname(lname, lname2);
		if (ret) return (0);
	}
        return (1);
}

/*
 *-----------------------------------------------------------------------------
 * vfat_sname_chksum()
 * This routine computes the checksum of a sname.
 *-----------------------------------------------------------------------------
 */
u_char  vfat_sname_chksum(char *sname)
{
	int	i;
        u_char  sum;

        for (sum = i = 0; i < 11; i++) 
                sum = (((sum&1)<<7)|((sum&0xfe)>>1)) + sname[i];
        return (sum);
}

/*
 *-----------------------------------------------------------------------------
 * vfat_dir_read()
 * This routine reads a directory into the directory cache.
 *-----------------------------------------------------------------------------
 */
int	vfat_dir_read(dnode_t *dp)
{
        char            *bp;
        u_short         cluster;
        dfs_t           *dfs;
        dirhash_t       *dh, **dhp;

	dfs = dp->d_dfs;
        dp->d_dir = (dfile_t *)safe_malloc(dp->d_size);
        cluster = dp->d_start;
        bp = (char *) dp->d_dir;
        do {
                lseek(dfs->dfs_fd, DISK_ADDR(dfs, cluster), 0);
                if (read(dfs->dfs_fd, bp, dfs->dfs_clustersize) < 0)
                        return errno;
                dhp = &DIRHASH(cluster);
                dh  = (dirhash_t *)safe_malloc(sizeof *dh);
                dh->dnode  = dp;
                dh->cluster= cluster;
                dh->next   = *dhp;
                *dhp       = dh;
                bp += dfs->dfs_clustersize;
                cluster = (*dfs->dfs_readfat)(dfs, cluster);
        } while (!LAST_CLUSTER(dfs, cluster));
        return (0);
}

/*
 *-----------------------------------------------------------------------------
 * vfat_dir_write()
 * This routine purges a directory cache to disk.
 *-----------------------------------------------------------------------------
 */
int	vfat_dir_write(dnode_t *dp)
{
        int             i;
        dirhash_t       *dh, **dhp;

        if (dp->d_dir == NULL)
                return (0);
        for (i = 0; i < DIRHASHSIZE; i++) {
                dhp = &dirhash[i];
                while ((dh = *dhp) != NULL) {
                        if (dh->dnode == dp) {
                                *dhp = dh->next;
                                free(dh);
                                continue;
                        }
                        dhp = &dh->next;
                }
        }
        free(dp->d_dir);
        dp->d_dir = NULL;
	return (0);
}

/* 
 *-----------------------------------------------------------------------------
 * vfat_clust_writ()
 *-----------------------------------------------------------------------------
 */
int	vfat_clust_writ(dfs_t *dfs, u_long daddr, u_char *clustbuf)
{
	int	nwrite;

	lseek(dfs->dfs_fd, daddr, SEEK_SET);
	nwrite = write(dfs->dfs_fd, clustbuf, dfs->dfs_clustersize);
	if (nwrite != dfs->dfs_clustersize)
		return (1);
	return (0);
}

/*
 *-----------------------------------------------------------------------------
 * vfat_clust_read()
 *-----------------------------------------------------------------------------
 */
int	vfat_clust_read(dfs_t *dfs, u_long daddr, u_char *clustbuf)
{
	int	nread;

	lseek(dfs->dfs_fd, daddr, SEEK_SET);
	nread = read(dfs->dfs_fd, clustbuf, dfs->dfs_clustersize);
	if (nread != dfs->dfs_clustersize)
		return (1);
	return (0);
}

/*
 *-----------------------------------------------------------------------------
 * vfat_update_fat()
 *-----------------------------------------------------------------------------
 */
int 	vfat_update_fat(dfs_t *dfs)
{
        dfs->dirtyfat = TRUE;
        return (0);
}

/*
 *-----------------------------------------------------------------------------
 * vfat_update_root()
 *-----------------------------------------------------------------------------
 */
int	vfat_update_root(dfs_t *dfs)
{
	dfs->dirtyrootdir = TRUE;
	return (0);	
}

/*
 *-----------------------------------------------------------------------------
 * vfat_free_fat()
 *-----------------------------------------------------------------------------
 */
u_short vfat_free_fat(dfs_t *dfs)
{
        u_short         i;
        static u_short  lcluster_assigned = 1;

        if (dfs->dfs_freeclustercount == 0)
                return 0;

        for (i = lcluster_assigned + 1; i < dfs->dfs_clustercount + 2; i++) {
                if ((*dfs->dfs_readfat)(dfs, i) == 0) {
                        (*dfs->dfs_writefat)(dfs, i, dfs->dfs_endclustermark);
                        dfs->dfs_freeclustercount--;
                        lcluster_assigned = i;
                        return i;
                }
        }
        for (i = 2; i <= lcluster_assigned; i++) {
                if ((*dfs->dfs_readfat)(dfs, i) == 0) {
                        (*dfs->dfs_writefat)(dfs, i, dfs->dfs_endclustermark);
                        dfs->dfs_freeclustercount--;
                        lcluster_assigned = i;
                        return i;
                }
        }
        panic(" vfat_free_fat: cannot allocate a FAT entry");
	return (0);
}

/*
 *-----------------------------------------------------------------------------
 * copy_dir_to_fp()
 * NOTE: The structure fp's field fp->f_offset ought to be filled.
 *-----------------------------------------------------------------------------
 */
int	copy_dir_to_fp(dnode_t *dp, file_t *fp)
{
	slot_t	*sp;
	dfile_t	*de;
	int	nentries;
	int	de_type, id;

	de = &dp->d_dir[fp->f_offset];
	de_type = vfat_dirent_type(de);
	if (de_type == DE_SLOT){
		sp = (slot_t *) de;
		nentries = sp->sl_id & 0xBF;
		for (id = nentries; id >= 0; id--, de++){
			COPY_SLOT_TO_FP(fp, de, id);
		}
		fp->f_nentries = nentries;
	}
	else {
		COPY_SLOT_TO_FP(fp, de, 0);
		fp->f_nentries = 0;
	}
	return (0);

}

/*
 *-----------------------------------------------------------------------------
 * xtract_fp_from_root()
 * This routine accepts a chunk of memory constituting the root, and an offset
 * that indexes into it; It then starts at this offset and proceeds to piece
 * together a "file_t".
 * NOTE: offset = dfile_t index over entire directory.
 *-----------------------------------------------------------------------------
 */
int     xtract_fp_from_root(dfs_t *dfs, dfile_t *rootbuf,
                            u_short offset, file_t *fp)
{
        slot_t  *sp;
        dfile_t *de;
        int     nentries;
        int     de_type, id;

        de = &rootbuf[offset];
        de_type = vfat_dirent_type(de);
        if (de_type == DE_SLOT){
                sp = (slot_t *) de;
                nentries = sp->sl_id & 0xBF;
                for (id = nentries; id >= 0; id--, de++){
                        COPY_SLOT_TO_FP(fp, de, id);
                }
                fp->f_nentries = nentries;
        }
        else {
                COPY_SLOT_TO_FP(fp, de, 0);
                fp->f_nentries = 0;
        }
        return (0);
}

/*
 *-----------------------------------------------------------------------------
 * copy_fp_to_dir()
 * NOTE: The structure fp ought to be filled.
 *-----------------------------------------------------------------------------
 */
int	copy_fp_to_dir(dnode_t *dp, file_t *fp)
{
	dfile_t *de;
	int	id, nentries;

	de = &dp->d_dir[fp->f_offset];
	nentries = fp->f_nentries;
	for (id = nentries; id >= 0; id--, de++){
		COPY_SLOT_FROM_FP(fp, de, id);
	}
	return (0);
}

/*
 *-----------------------------------------------------------------------------
 * vfat_copy_attributes()
 * This routine is used to copy the attributes: (time, date, size, start).
 *-----------------------------------------------------------------------------
 */
int	vfat_copy_attributes(file_t *fp1, file_t *fp2)
{
	dfile_t	*df1 = fp1->f_entries;
	dfile_t *df2 = fp2->f_entries;

	df2->df_attribute = df1->df_attribute;

	df2->df_time[0] = df1->df_time[0];
	df2->df_time[1] = df1->df_time[1];

	df2->df_date[0] = df2->df_date[0];
	df2->df_date[1] = df2->df_date[1];

	df2->df_size[0] = df1->df_size[0];
	df2->df_size[1] = df1->df_size[1];
	df2->df_size[2] = df1->df_size[2];
	df2->df_size[3] = df1->df_size[3];

	df2->df_start[0] = df1->df_start[0];
	df2->df_start[1] = df1->df_start[1];
	return (0);
}

/*
 *-----------------------------------------------------------------------------
 * a2ucode1()
 * This routine is used to convert a char into its first ucode.
 *-----------------------------------------------------------------------------
 */
u_char  a2ucode1(char ch)
{
	return (a2uni[ch].uni1);
}

/*
 *-----------------------------------------------------------------------------
 * a2ucode()
 * This routine is used to convert a char into its second ucode.
 *-----------------------------------------------------------------------------
 */
u_char  a2ucode2(char ch)
{
        return (a2uni[ch].uni2);
}

/*
 *-----------------------------------------------------------------------------
 * ucode2a()
 * This routine is used to convert a unicode into its ascii char.
 *
 * (*end) set to:	0 - valid ascii character returned
 *			1 - 'end'   unicode detected
 *			2 - invalid unicode detected
 *-----------------------------------------------------------------------------
 */
u_char  ucode2a(u_short word, int *end)
{
        u_char  ret;
        u_char  byte1, byte2;
        u_char  *uni_page;
        u_char  page, pg_off, ascii;

        byte1  = (word & 0xFF00) >> 8;
        byte2  = (word & 0x00FF);       

        pg_off = byte2;
        page   = byte1;

	*end = 0;

        if (pg_off == 0 && page == 0) {
		*end = 1;

		return 0;
	}

        uni_page = uni2asc_pg[page];

	if (uni_page == NULL) {
		*end = 2;

		return 0;
	}

	ascii = uni_page[pg_off];

        ret  = ascii ? ascii : '?';      

        return (ret);
}

/*
 *-----------------------------------------------------------------------------
 * to_dostime()
 * This routine chances time in UNIX format to DOS format
 * The DOS time format is as follow:
 *      Bits            Contents
 *      0x0-0x4         second in 2 seconds increment (0-29)
 *      0x5-0xa         minutes (0-59)
 *      0xb-0xf         hours (0-23)
 *
 * the DOS date format is as follow:
 *      Bits            Contents
 *      0x0-0x4         days of month (1-31)
 *      0x5-0x8         month (1-12)
 *      0x9-0xf         year (relative to 1980)
 *-----------------------------------------------------------------------------
 */
void 	to_dostime(u_char *d_date, u_char *d_time, time_t *u_time)
{
        struct tm       tm;

        tm = *localtime(u_time);
        d_date[0] = tm.tm_mday | (((tm.tm_mon + 1) & 0x7) << 5);
        d_date[1] = ((tm.tm_mon + 1) >> 3) | ((tm.tm_year - 80) << 1);
        d_time[0] = (tm.tm_sec / 2) | ((tm.tm_min & 0x7) << 5);
        d_time[1] = (tm.tm_min >> 3) | (tm.tm_hour << 3);
	return;
}

/*
 *-----------------------------------------------------------------------------
 * to_unixtime()
 * This routine changes time in DOS format to UNIX format.
 *-----------------------------------------------------------------------------
 */
void 	to_unixtime(u_char *d_date, u_char *d_time, time_t *u_time)
{
        struct tm       tm;

        tm.tm_sec 	= (d_time[0] & 0x1f) * 2;
        tm.tm_min 	= ((d_time[1] & 0x7) << 3) | (d_time[0] >> 5);
        tm.tm_hour 	= d_time[1] >> 3;
        tm.tm_mday 	= d_date[0] & 0x1f;
        tm.tm_mon 	= ((d_date[0] >> 5) | ((d_date[1] & 0x1) << 3)) - 1;
        tm.tm_year 	= (d_date[1] >> 1) + 80;
        tm.tm_isdst 	= 1;
        *u_time = mktime(&tm);
	return;
}

/*
 *-----------------------------------------------------------------------------
 * set_attribute()
 *-----------------------------------------------------------------------------
 */
int     set_attribute(dnode_t *dp, int archive_attrib)
{
        int     error;
	dfs_t	*dfs;
	dnode_t	*pdp;
        file_t  f;
        dfile_t *df;
        u_int   size;
        u_short offset;
        u_short cluster;

	dfs = dp->d_dfs;
	if (dp->d_fno == ROOTFNO)
		return (0);
	error = dos_getnode(dfs, dp->d_pfno, &pdp);
	if (error)
		return (error);
	if (pdp->d_dir == NULL){
		error = vfat_dir_read(pdp);
		if (error)	
			goto set_attribute_done;	
	}
	f.f_offset  = offset  = OFFSET(dp->d_fno);
	f.f_cluster = cluster = CLUSTER(dp->d_fno);
	copy_dir_to_fp(pdp, &f);
	df = &(f.f_entries[0]);
	if (dp->d_ftype == NFREG){
		size = DF_SIZE(df);
		error = expand_file(dfs, dp->d_start, size, dp->d_size);
		if (error)	
			goto set_attribute_done;
		df->df_size[0]  = (dp->d_size & 0xff);
		df->df_size[1]  = (dp->d_size >> 8) & 0xff;
		df->df_size[2]  = (dp->d_size >> 16) & 0xff;
		df->df_size[3]  = (dp->d_size >> 24) & 0xff;
		/* 
		 * No readonly directory in DOS 
                 * archive attribute applies to regular files only 
		 */
		if (dp->d_mode & WRITE_MODE)
                        df->df_attribute &= ~ATTRIB_RDONLY;
                else 	df->df_attribute |= ATTRIB_RDONLY;
                df->df_attribute |= archive_attrib;
	}
	to_dostime(df->df_date, df->df_time, &dp->d_time);
	error = vfat_dir_writent(pdp, &f);

set_attribute_done:

	dos_putnode(pdp);
	return (error);
}

/*
 *-----------------------------------------------------------------------------
 * expand_file()
 *-----------------------------------------------------------------------------
 */
int     expand_file(dfs_t *dfs, u_short start, u_int o_size, u_int n_size)
{
        int             i;
        u_short         lcluster;
        u_short         ncluster;

        /*
         * Convert size in bytes to size in clusters. 
	 * In DOS, every file must has atleast one cluster
         */
        o_size = (o_size + dfs->dfs_clustersize - 1) / dfs->dfs_clustersize;
        if (o_size == 0)
                o_size = 1;
        n_size = (n_size + dfs->dfs_clustersize - 1) / dfs->dfs_clustersize;
        if (n_size == 0)
                n_size = 1;
        if (o_size == n_size)   /* no change */
                return 0;
        /* Truncate */
        if (o_size > n_size) {
                ncluster = start;
                for (i = n_size - 1; i > 0; i--)
                        ncluster = (*dfs->dfs_readfat)(dfs, ncluster);
                lcluster = ncluster;
                ncluster = (*dfs->dfs_readfat)(dfs, ncluster);
                (*dfs->dfs_writefat)(dfs, lcluster, dfs->dfs_endclustermark);

                do {
                        lcluster = ncluster;
                        ncluster = (*dfs->dfs_readfat)(dfs, ncluster);
                        (*dfs->dfs_writefat)(dfs, lcluster, 0);
			/* Increment free cluster count */
			dfs->dfs_freeclustercount++;
                } while (!LAST_CLUSTER(dfs, ncluster));
                return (vfat_update_fat(dfs));
        }
        /* Expand */
        if (n_size - o_size > dfs->dfs_freeclustercount)
                return (ENOSPC);
        lcluster = start;
        for (i = o_size - 1; i > 0; i--)
                lcluster = (*dfs->dfs_readfat)(dfs, lcluster);
        for (i = n_size - o_size; i > 0; i--) {
                ncluster = vfat_free_fat(dfs); 
                (*dfs->dfs_writefat)(dfs, lcluster, ncluster);
                lcluster = ncluster;
        }
        return (vfat_update_fat(dfs));
}

/*
 *-----------------------------------------------------------------------------
 * subdir_size()
 * This routine is used to find the size, in bytes, of a specified directory.
 *-----------------------------------------------------------------------------
 */
u_int	subdir_size(dnode_t *dp)
{
        u_int   count;
        u_short cluster;
        dfs_t   *dfs = dp->d_dfs;

        cluster = dp->d_start;
        count = 0;
        do {
                count++;
                cluster = (*dfs->dfs_readfat)(dfs, cluster);
        } while (!LAST_CLUSTER(dfs, cluster));
        return (dfs->dfs_clustersize*count);
}

/*
 *-----------------------------------------------------------------------------
 * to_cluster()
 * This routine is used to locate the cluster where a directory entry with
 * a given offset is.
 *-----------------------------------------------------------------------------
 */
u_short	to_cluster(dnode_t *dp, u_short offset)
{
        int     i;
        u_short cluster;
        dfs_t   *dfs = dp->d_dfs;

        cluster = dp->d_start;
        for (i = offset / DIR_ENTRY_PER_CLUSTER(dfs); i > 0; i--)
                cluster = (*dfs->dfs_readfat)(dfs, cluster);
        return (cluster);
}

/*
 *-----------------------------------------------------------------------------
 * readfat12()
 *-----------------------------------------------------------------------------
 */
u_short readfat12(dfs_t *dfs, u_short cluster)
{
        int     i;

        i = cluster * 12 / 8;

        if (cluster % 2)
                return (dfs->dfs_fat[i] >> 4) | (dfs->dfs_fat[i + 1] << 4);
        return dfs->dfs_fat[i] | ((dfs->dfs_fat[i + 1] & 0xf) << 8);
}

/*
 *-----------------------------------------------------------------------------
 * readfat16()
 *-----------------------------------------------------------------------------
 */
u_short readfat16(dfs_t *dfs, u_short cluster)
{
        int     i;

        i = cluster * 2;
        return (dfs->dfs_fat[i] | (dfs->dfs_fat[i + 1] << 8));
}

/*
 *-----------------------------------------------------------------------------
 * writefat12()
 *-----------------------------------------------------------------------------
 */
void    writefat12(dfs_t *dfs, u_short cluster, u_short value)
{
        int     i;

        i = cluster * 12 / 8;

        if (cluster % 2) {
                dfs->dfs_fat[i] =
                        (dfs->dfs_fat[i] & 0xf) | ((value & 0xf) << 4);
                dfs->dfs_fat[i + 1] = value >> 4;
        } else {
                dfs->dfs_fat[i] = value & 0xff;
                dfs->dfs_fat[i + 1] =
                        (dfs->dfs_fat[i + 1] & 0xf0) | (value >> 8);
        }
        return;
}

/*
 *-----------------------------------------------------------------------------
 * writefat16()
 *-----------------------------------------------------------------------------
 */
void    writefat16(dfs_t *dfs, u_short cluster, u_short value)
{
        int     i;

        i = cluster * 2;

        dfs->dfs_fat[i] = value & 0xff;
        dfs->dfs_fat[i + 1] = value >> 8;
        return;
}

/*
 *-----------------------------------------------------------------------------
 * cache_dirty()
 *-----------------------------------------------------------------------------
 */
int     cache_dirty()
{
    int i ;

    for (i = 0; i < trkcache.dirtymaskcnts; i++)
        if (trkcache.dirty[i])
            return (TRUE);
    return (FALSE);
}

/*
 *-----------------------------------------------------------------------------
 * set_dirty()
 *-----------------------------------------------------------------------------
 */
int     set_dirty(int clusteraddr)
{
   int clusterno;
   int index, offset;

   clusterno = (clusteraddr-trkcache.trkbgnaddr) /
               trkcache.clustersize;
   index  = clusterno / MASKBYTESZ;
   offset = clusterno % MASKBYTESZ;
   if (index < trkcache.dirtymaskcnts) {
      trkcache.dirty[index] |= (1 << offset);
      return 0;
   } else {
        syslog(LOG_ERR, "Don't know how to handle the cache index");
        exit(1);
   }
   return (0);
}

/*
 *-----------------------------------------------------------------------------
 * verify_cache()
 *-----------------------------------------------------------------------------
 */
int     verify_cache(dfs_t * dfs, int clusteraddr, int attrib)
{
   int  rcode = 0;
   int  trkstartaddr;
   int  trkendaddr;
   static       int lastop= -1;

   if (clusteraddr <  trkcache.trkbgnaddr ||
       clusteraddr >= trkcache.trkendaddr ||
      (lastop == CACHE_WRITE && attrib == CACHE_READ)) {
      if (cache_dirty())
         if (rcode = cache_writeback(dfs))
            return rcode;
      trkstartaddr = (clusteraddr-dfs->dfs_fileaddr)/trkcache.trksize*
                     trkcache.trksize+dfs->dfs_fileaddr;
      trkendaddr   = trkstartaddr + trkcache.trksize;
      if (trkendaddr > partendaddr)
          trkendaddr = partendaddr;
      if (attrib == CACHE_READ || attrib == CACHE_WRITE) {
         lseek(dfs->dfs_fd, trkstartaddr, 0);
         if (read(dfs->dfs_fd, trkcache.bufp, trkendaddr - trkstartaddr) < 0)
            return errno;
      }
      trkcache.trkbgnaddr = trkstartaddr;
      trkcache.trkendaddr = trkendaddr;
      lastop = attrib;
   }
   return (rcode);
}

/*
 *-----------------------------------------------------------------------------
 * cache_writeback()
 *-----------------------------------------------------------------------------
 */
int     cache_writeback(dfs_t * dfs)
{
    int remainclusters;
    int i, j, count, start, end;

    start = -1;
    remainclusters = trkcache.clusterspertrk;
    for (i = 0; i < trkcache.dirtymaskcnts; i++) {
        count = (remainclusters >= MASKBYTESZ) ?
                 MASKBYTESZ : remainclusters;
        for (j = 0; j < count; j++) {
            if (trkcache.dirty[i] & (1 << j)) {
                if (start == -1) {
                    start = i*MASKBYTESZ+j;
                    end = start;
                } else
                    end = i*MASKBYTESZ+j;
            } else {
                if (start != -1) {
                    if (disk_write(dfs, start, end))
                        return errno;
                    start = -1;
                }
            }
        }
        remainclusters -= count;
    }
    if (start != -1)
        if (disk_write(dfs, start, end))
                return (errno);
    for (i = 0; i < trkcache.dirtymaskcnts; i++)
        trkcache.dirty[i] = 0;
    return (0);
}

/*
 *-----------------------------------------------------------------------------
 * disk_write()
 *-----------------------------------------------------------------------------
 */
int     disk_write(dfs_t * dfs, int startcluster, int endcluster)
{
    int clusteraddroffset;
    int writesize;
    int wrotesize;

    clusteraddroffset = startcluster*trkcache.clustersize;
    writesize = trkcache.clustersize*(endcluster-startcluster+1);
    lseek(dfs->dfs_fd, trkcache.trkbgnaddr+clusteraddroffset, 0);
    wrotesize = write(dfs->dfs_fd, &trkcache.bufp[clusteraddroffset],writesize);    if (wrotesize < 0)
        return errno;
    if (wrotesize != writesize)
        return (1);
    return (0);
}

/*
 *-----------------------------------------------------------------------------
 * to_unixname()
 * This routine takes a (name, ext) tuple and converts it to a sname.
 *-----------------------------------------------------------------------------
 */
void	to_unixname(char *name, char *ext, char *sname)
{
        int     i;
        int     j;

        for (i = 0; i < 8 && name[i] != ' '; i++)
                sname[i] = tolower(name[i]);

        if (strncmp(ext, nullext, 3) == 0)
                j = 0;
        else {
                sname[i++] = '.';
                for (j = 0; j < 3 && ext[j] != ' '; j++)
                        sname[i + j] = tolower(ext[j]);
        }
        sname[i + j] = '\0';
	return;
}

/*
 *-----------------------------------------------------------------------------
 * to_dosname()
 * This routine takes a sname and converts it into a (name, ext) tuple.
 *-----------------------------------------------------------------------------
 */
void	to_dosname(char *name, char *ext, char *uname)
{
        u_int   ext_len;
        int     i;
        u_int   name_len;
        char    *p;

        p = strchr(uname, '.');
        if (p == NULL) {
                name_len = strlen(uname);
                if (name_len > 8)
                        name_len = 8;
                ext_len = 0;
        } else {
                name_len = p - uname;
                if (name_len > 8)
                        name_len = 8;
                ext_len = strlen(p + 1);
                if (ext_len > 3)
                        ext_len = 3;
        }
        for (i = 0; i < name_len; i++)
                name[i] = toupper(uname[i]);
        for (; i < 8; i++)
                name[i] = ' ';

        for (i = 0; i < ext_len; i++)
                ext[i] = toupper(p[i + 1]);
        for (; i < 3; i++)
                ext[i] = ' ';
	return;
}

/*
 *----------------------------------------------------------------------------
 * vfat_print11()
 *----------------------------------------------------------------------------
 */
char	*vfat_print11(char *sname)
{
	static	char	dupsname[12];
	
	strncpy(dupsname, sname, MSDOS_NAME);
	dupsname[11] = '\0';
	return (dupsname);
}

/*
 *----------------------------------------------------------------------------
 * dosname_cmp()
 * This routine performs a case-insensitive comparison of two 11 char names.
 * Returns (1): strings identical.
 * Returns (0): strings non-identical.
 *----------------------------------------------------------------------------
 */
int	dosname_cmp(char *s1, char *s2)
{
	int	i;
	char	*t1, *t2;
	char	ch1, ch2;

	for (i = 0, t1 = s1, t2 = s2; i < MSDOS_NAME; i++, t1++, t2++){
		if (*t1 >= 'A' && *t1 <= 'Z')
			ch1 = *t1+32;
		else	ch1 = *t1;
		if (*t2 >= 'A' && *t2 <= 'Z')
			ch2 = *t2+32;
		else 	ch2 = *t2;
		if (ch1 != ch2)
			return (0);
	}
	return (1);
}

/*
 *-----------------------------------------------------------------------------
 * dosname_cmp_lname()
 * This routine performs a case-insensitive comparsion of two names.
 * The names might be long names or even shortnames.
 * Returns (1): strings identical.
 * Returns (0): strings non-identical.
 *-----------------------------------------------------------------------------
 */
int 	dosname_cmp_lname(char *s1, char *s2)
{
        char    *t1, *t2;
        char    ch1, ch2;

	for (t1 = s1, t2 = s2; *t1 != '\0' && *t2 != '\0'; t1++, t2++){
                if (*t1 >= 'A' && *t1 <= 'Z')
                        ch1 = *t1+32;
                else    ch1 = *t1;
                if (*t2 >= 'A' && *t2 <= 'Z')
                        ch2 = *t2+32;
                else    ch2 = *t2;
                if (ch1 != ch2)
                        return (0);
	}
	if (*t1 == '\0' && *t2 == '\0')
		return (1);
	return (0);
}

/*
 *----------------------------------------------------------------------------
 * end_cluster()
 *----------------------------------------------------------------------------
 */
int     end_cluster(dfs_t *dfs, u_short cluster)
{
        if (fat_entry_size == 12){
                if (cluster > 0x0FF8)
                        return (1);
                return (0);
        }
        else if (fat_entry_size == 16){
                if (cluster > 0xFFF8)
                        return (1);
                return (0);
        }
	return (0);
}

