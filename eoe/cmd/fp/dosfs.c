/*
 * DOS filesystem-level routines.
 */

#include <stdio.h>      /* printf */
#include <stdlib.h>     /* exit */
#include <time.h>
#include <unistd.h>
#include <string.h>     /* memset */
#include <sys/types.h>  /* macclo */
#include <ctype.h> 
#include <malloc.h>
#include <errno.h>

#include "dosfs.h"
#include "smfd.h"
#include "mkfp.h"
#include "macLibrary.h"


/* sector sized scratch buffer */
static u_char * sectorbufp;

static void    set_param_48(bp_t *, u_char *);
static void    set_param_96hi(bp_t *, u_char *);
static void    set_param_35(bp_t *, u_char *);
static void    set_param_35hi(bp_t *, u_char *);
static void    set_param_20m_12b(bp_t *, u_char *);
static void    set_param_20m_16b(bp_t *, u_char *);
static void    (* setbootblkfunc[SMFD_MAX_TYPES])(bp_t *, u_char *);
static int     initdirsector(int, int, int);
static int     initfatsector(int, u_char *, int, int);
static void    to_dostime(u_char *, u_char *);
static void    fatpack(u_char *, bp_t *);
static void    fatunpack(u_char *, bp_t *);
static u_short byteswap(u_short);


int dos_volabel(FPINFO *, char *);

static u_char bootsectable[] = {

0xeb, 0x28, 0x90, 0x49, 0x42, 0x4d, 0x20, 0x50, 0x4e, 0x43, 0x49, 0x00,
0x02, 0x01, 0x01, 0x00, 0x02, 0xe0, 0x00, 0x40, 0x0b, 0xf0, 0x09, 0x00,
0x12, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfa, 0x33, 0xc0, 0x8e, 0xd0, 0xbc,
0xf0, 0x7b, 0xfb, 0xb8, 0xc0, 0x07, 0x8e, 0xd8, 0xbe, 0x5b, 0x00, 0x90,
0xfc, 0xac, 0x0a, 0xc0, 0x74, 0x0b, 0x56, 0xb4, 0x0e, 0xbb, 0x07, 0x00,
0xcd, 0x10, 0x5e, 0xeb, 0xf0, 0x32, 0xe4, 0xcd, 0x16, 0xb4, 0x0f, 0xcd,
0x10, 0x32, 0xe4, 0xcd, 0x10, 0xcd, 0x19, 0x0d, 0x0a, 0x0d, 0x0a, 0x0d,
0x0a, 0x0d, 0x0a, 0x0d, 0x0a, 0x0d, 0x0a, 0x0d, 0x0a, 0x0d, 0x0a, 0x20,
0x20, 0x20, 0x20, 0x54, 0x68, 0x69, 0x73, 0x20, 0x64, 0x69, 0x73, 0x6b,
0x20, 0x69, 0x73, 0x20, 0x6e, 0x6f, 0x74, 0x20, 0x62, 0x6f, 0x6f, 0x74,
0x61, 0x62, 0x6c, 0x65, 0x0d, 0x0a, 0x0d, 0x0a, 0x20, 0x49, 0x66, 0x20,
0x79, 0x6f, 0x75, 0x20, 0x77, 0x69, 0x73, 0x68, 0x20, 0x74, 0x6f, 0x20,
0x6d, 0x61, 0x6b, 0x65, 0x20, 0x69, 0x74, 0x20, 0x62, 0x6f, 0x6f, 0x74,
0x61, 0x62, 0x6c, 0x65, 0x2c, 0x0d, 0x0a, 0x72, 0x75, 0x6e, 0x20, 0x74,
0x68, 0x65, 0x20, 0x44, 0x4f, 0x53, 0x20, 0x70, 0x72, 0x6f, 0x67, 0x72,
0x61, 0x6d, 0x20, 0x53, 0x59, 0x53, 0x20, 0x61, 0x66, 0x74, 0x65, 0x72,
0x20, 0x74, 0x68, 0x65, 0x0d, 0x0a, 0x20, 0x20, 0x20, 0x20, 0x20, 0x73,
0x79, 0x73, 0x74, 0x65, 0x6d, 0x20, 0x68, 0x61, 0x73, 0x20, 0x62, 0x65,
0x65, 0x6e, 0x20, 0x6c, 0x6f, 0x61, 0x64, 0x65, 0x64, 0x0d, 0x0a, 0x0d,
0x0a, 0x50, 0x6c, 0x65, 0x61, 0x73, 0x65, 0x20, 0x69, 0x6e, 0x73, 0x65,
0x72, 0x74, 0x20, 0x61, 0x20, 0x44, 0x4f, 0x53, 0x20, 0x64, 0x69, 0x73,
0x6b, 0x65, 0x74, 0x74, 0x65, 0x20, 0x69, 0x6e, 0x74, 0x6f, 0x0d, 0x0a,
0x20, 0x74, 0x68, 0x65, 0x20, 0x64, 0x72, 0x69, 0x76, 0x65, 0x20, 0x61,
0x6e, 0x64, 0x20, 0x73, 0x74, 0x72, 0x69, 0x6b, 0x65, 0x20, 0x61, 0x6e,
0x79, 0x20, 0x6b, 0x65, 0x79, 0x2e, 0x2e, 0x2e, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x55, 0xaa,    
};


void
dos_formatfunc_init(int f12fg)
{
    setbootblkfunc[FD_FLOP]        = set_param_48;
    setbootblkfunc[FD_FLOP_AT]     = set_param_96hi;
    setbootblkfunc[FD_FLOP_35LO]   = set_param_35;
    setbootblkfunc[FD_FLOP_35]     = set_param_35hi;
    if (f12fg)
        setbootblkfunc[FD_FLOP_35_20M] = set_param_20m_12b;
    else
        setbootblkfunc[FD_FLOP_35_20M] = set_param_20m_16b;

    return;
}


/* get volume header */
int
dos_format(FPINFO * fpinfop)
{
    int       retval = E_NONE;
    bp_t    * bpp;
    bp_t      newbp;
    u_char    fatid = 0xF0;  /* the default FAT ID for 3.5.hi */
    int       fatcount;
    int       rootdircount;
    u_char  * fatspacep;
    u_char  * dirsectorp;
    u_int     fatfullsize;
    u_int     dirfullsize;

        /* Make sure it's a floppy disk */
    if (fpinfop->mediatype != FD_FLOP      &&
        fpinfop->mediatype != FD_FLOP_AT   &&
        fpinfop->mediatype != FD_FLOP_35LO &&
        fpinfop->mediatype != FD_FLOP_35   &&
        fpinfop->mediatype != FD_FLOP_35_20M &&
        fpinfop->mediatype != FD_FLOP_GENERIC )

        return(E_NOTSUPPORTED); 

    if ( fpinfop->mediatype != FD_FLOP_GENERIC )
        /* readjust the boot sector parameters */ 
        (* setbootblkfunc[fpinfop->mediatype])(&newbp, &fatid);

        /* pack or unpack the data structure to or from memory */
    if ((fpinfop->mediatype != FD_FLOP_35) &&
        (fpinfop->mediatype != FD_FLOP_GENERIC))
        fatpack(bootsectable, &newbp);
    else
        fatunpack(bootsectable, &newbp);
/*    ... init partition table here...*/

    /* prepare the fat area space, try to allocate the full space */ 
    fatfullsize = newbp.bp_sectorsize*newbp.bp_fatsize;
    fatspacep = (u_char *) malloc(fatfullsize);
    if (!fatspacep) {
        fatfullsize = newbp.bp_sectorsize;
        fatspacep = (u_char *) safemalloc(fatfullsize);
    } 
    memset(fatspacep, '\0', fatfullsize);
    fatspacep[0] = fatid;
    fatspacep[1] = 0xFF;
    fatspacep[2] = 0xFF;
    if (fpinfop->mediatype == FD_FLOP_35_20M && 
	 setbootblkfunc[FD_FLOP_35_20M] == set_param_20m_16b)
        fatspacep[3] = 0xFF;  /* floptical support 16 bit FAT system */

    /* the size of file entry is 32 bytes and the */
    /* directory sectors should be rounded up.    */
    rootdircount = (32 * newbp.bp_rootentries + newbp.bp_sectorsize - 1) /
                                                  newbp.bp_sectorsize;

        /* initialize the directory sectors */
    dirfullsize = newbp.bp_sectorsize*rootdircount;
    dirsectorp = (u_char *) malloc(dirfullsize);
    if (!dirsectorp) {
        dirfullsize = newbp.bp_sectorsize;
        dirsectorp  = (u_char *) safemalloc(dirfullsize);
    } 
    memset(dirsectorp, '\0', dirfullsize);

    /**** write the file system ***/

    /* move to the beginning */
    lseek(fpinfop->devfd, 0, SEEK_SET);

        /* write disk boot parameter block */
    if((retval = rfpblockwrite(fpinfop->devfd, (u_char *)bootsectable,
                                      newbp.bp_sectorsize)) != E_NONE)

        return(retval);

        /* initialize the FAT sector of the floppy disk */
    fatcount = newbp.bp_fatcount;
    while (fatcount--) 
        if (fatfullsize > newbp.bp_sectorsize) {
            if ((retval = rfpblockwrite(fpinfop->devfd, fatspacep,
                         fatfullsize)) != E_NONE)
                return(retval);
        } else {
            if ((retval = initfatsector(fpinfop->devfd, fatspacep, 
                           newbp.bp_fatsize, newbp.bp_sectorsize)) != E_NONE)
                return(retval); 
        }

    if (dirfullsize > newbp.bp_sectorsize) {
        if ((retval = rfpblockwrite(fpinfop->devfd, dirsectorp,
                                    dirfullsize)) != E_NONE)
            return(retval);
    } else {
        if ((retval = initdirsector(fpinfop->devfd, rootdircount,
				    newbp.bp_sectorsize)) != E_NONE)
            return(retval); 
    }

    if ((fpinfop->volumelabel)[0] != '\0') {

        dfile_t   fentry;

        sectorbufp = (u_char *) safemalloc(newbp.bp_sectorsize);
        memset(sectorbufp, '\0', newbp.bp_sectorsize); 

        memset(&fentry, '\0', sizeof(fentry));
        strncpy(fentry.df_name, fpinfop->volumelabel, 8);
        if (strlen(fpinfop->volumelabel) > 8)
            strncpy(fentry.df_ext, fpinfop->volumelabel+8, 3);
        fentry.df_attribute = ATTRIB_ARCHIVE | ATTRIB_LABEL;
        to_dostime(fentry.df_date, fentry.df_time); 

        memcpy((char *)&sectorbufp[0], (char *)&fentry, sizeof(fentry));    

        lseek(fpinfop->devfd,
             (newbp.bp_reservecount+newbp.bp_fatsize*newbp.bp_fatcount) *
                                           newbp.bp_sectorsize, SEEK_SET);
        if ((retval = rfpblockwrite(fpinfop->devfd, sectorbufp,
                                    newbp.bp_sectorsize)) != E_NONE)
            return(retval);
    }
    return(E_NONE);
}



/********************************************************
 *                                                      *
 *    The following functions are used to set the boot  *
 *    parameters for each supported floppy media.       *
 *                                                      * 
 ********************************************************/


static void 
set_param_48(bp_t * cbpp, u_char * fatidp)
{
    cbpp->bp_sectorsize = 512;
    cbpp->bp_sectorspercluster = 2;
    cbpp->bp_reservecount = 1;
    cbpp->bp_fatcount = 2;
    cbpp->bp_rootentries = 112;
    cbpp->bp_sectorcount = 720;
    cbpp->bp_mediatype = 0xFD;
    cbpp->bp_fatsize = 2;
    cbpp->bp_sectorspertrack = 9;
    cbpp->bp_headcount = 2;
    cbpp->bp_hiddensectors = 0;

    *fatidp = 0xFD;

    return;
}


static void
set_param_96hi(bp_t * cbpp, u_char * fatidp)
{
    cbpp->bp_sectorsize = 512;
    cbpp->bp_sectorspercluster = 1;
    cbpp->bp_reservecount = 1;
    cbpp->bp_fatcount = 2;
    cbpp->bp_rootentries = 224;
    cbpp->bp_sectorcount = 2400;
    cbpp->bp_mediatype = 0xF9;
    cbpp->bp_fatsize = 7;
    cbpp->bp_sectorspertrack = 15;
    cbpp->bp_headcount = 2;
    cbpp->bp_hiddensectors = 0;

    *fatidp = 0xF9;

    return;
}


static void
set_param_35(bp_t * cbpp, u_char * fatidp)
{
    cbpp->bp_sectorsize = 512;
    cbpp->bp_sectorspercluster = 2;
    cbpp->bp_reservecount = 1;
    cbpp->bp_fatcount = 2;
    cbpp->bp_rootentries = 112;  /* 0x70 */
    cbpp->bp_sectorcount = 1440; /* 0x5A0 */
    cbpp->bp_mediatype = 0xF9;
    cbpp->bp_fatsize = 3;
    cbpp->bp_sectorspertrack = 9;
    cbpp->bp_headcount = 2;
    cbpp->bp_hiddensectors = 0;

    *fatidp = 0xF9;

    return;
}


static void
set_param_35hi(bp_t * cbpp, u_char * fatidp)
{
    /* Don't need to do anything */
    return;
}


static void
set_param_20m_12b(bp_t * cbpp, u_char * fatidp)
{
    cbpp->bp_sectorsize = 512;
    cbpp->bp_sectorspercluster = 16;
    cbpp->bp_reservecount = 1;
    cbpp->bp_fatcount = 2;
    cbpp->bp_rootentries = 240;
    cbpp->bp_sectorcount = 40662;
    cbpp->bp_mediatype = 0xF9;
    cbpp->bp_fatsize = 8;
    cbpp->bp_sectorspertrack = 27;
    cbpp->bp_headcount = 6;
    cbpp->bp_hiddensectors = 0;

    *fatidp = 0xF9;

    return;
}


static void
set_param_20m_16b(bp_t * cbpp, u_char * fatidp)
{
    cbpp->bp_sectorsize = 512;
    cbpp->bp_sectorspercluster = 4;
    cbpp->bp_reservecount = 1;
    cbpp->bp_fatcount = 2;
    cbpp->bp_rootentries = 240;
    cbpp->bp_sectorcount = 40662;
    cbpp->bp_mediatype = 0xF9;
    cbpp->bp_fatsize = 40;
    cbpp->bp_sectorspertrack = 27;
    cbpp->bp_headcount = 6;
    cbpp->bp_hiddensectors = 0;

    *fatidp = 0xF9;

    return;
}


static int
initdirsector(int fd, int rdircount, int sectorsz)
{
    int retval = E_NONE;
    int count = rdircount;

    while(count--) {  /* clean up the rest of sectors */
        if ((retval = rfpblockwrite(fd, sectorbufp,  sectorsz)) != E_NONE)
            return(retval);
    }
    return(retval);
}


static int 
initfatsector(int fd, u_char * fatbuf, int fatsize, int sectorsz)
{
    int retval = E_NONE;
    int size = fatsize;

    if ((retval = rfpblockwrite(fd, fatbuf,  sectorsz)) != E_NONE)
        return(retval);

    while(--size) {   /* clean up the rest of sectors */
        if ((retval = rfpblockwrite(fd, sectorbufp,  sectorsz)) !=E_NONE)
            return(retval);
    }

    return(retval);
}


static void
to_dostime(u_char *d_date, u_char *d_time)
{
    struct tm  tm;
    time_t     mtime;

    mtime = time(NULL);
    tm = *localtime(&mtime);
    d_date[0] = tm.tm_mday | (((tm.tm_mon + 1) & 0x7) << 5);
    d_date[1] = ((tm.tm_mon + 1) >> 3) | ((tm.tm_year - 80) << 1);
    d_time[0] = (tm.tm_sec / 2) | ((tm.tm_min & 0x7) << 5);
    d_time[1] = (tm.tm_min >> 3) | (tm.tm_hour << 3);
}


int
dos_volabel(FPINFO * fpinfop, char * labelp)
{
    int retval = E_NONE;
    int count;
    char vlabel[MAX_DOS_VOL_NAME];
    char * strp;

    /* blank the label with space character */
    memset (vlabel, ' ', MAX_DOS_VOL_NAME);

    /* check  the illegal printable chars in the volume label */ 
    if (strpbrk(labelp, "*?/\\|.,;:+=<>[]()&^")) 
        return(!retval);
   
    count = 0;
    while (* labelp && count < MAX_DOS_VOL_NAME) {
        if (!isprint(* labelp))
            return(retval != E_NONE);
        else if (isspace(* labelp) || * labelp == '\t') {
        vlabel[count++] = ' ';
        labelp = spaceskip(labelp);
        } else
        vlabel[count++] = *labelp++;
    }
    for (count = 0; count < MAX_DOS_VOL_NAME; count++)
	vlabel[count] = toupper(vlabel[count]);
    strncpy(fpinfop->volumelabel, vlabel, MAX_DOS_VOL_NAME);
    return(E_NONE);
}


static u_short
byteswap(u_short inshort)
{
    u_short outshort;

    outshort = (inshort & 0xFF) << 8 | inshort >> 8;

    return(outshort);
}


static void
fatpack(u_char * bpp, bp_t * cbpp) 
{
    u_char * bpptr = bpp;
    u_short  tmpshort; 

    bpptr += sizeof(u_char) * 3;  /* magic number */
    bpptr += sizeof(u_char) * 8;  /* oem identification */

    tmpshort = byteswap(cbpp->bp_sectorsize);
    memcpy (bpptr, &tmpshort, sizeof(u_short));
    bpptr += sizeof(u_short);     /* sector size in bytes */

    memcpy (bpptr, &cbpp->bp_sectorspercluster, sizeof(u_char));
    bpptr += sizeof(u_char);      /* cluster size in sectors */

    tmpshort = byteswap(cbpp->bp_reservecount);
    memcpy (bpptr, &tmpshort, sizeof(u_short));
    bpptr += sizeof(u_short);     /* # of reserved sectors */

    memcpy (bpptr, &cbpp->bp_fatcount, sizeof(u_char));
    bpptr += sizeof(u_char);      /* # of FAT */

    tmpshort = byteswap(cbpp->bp_rootentries);
    memcpy (bpptr, &tmpshort, sizeof(u_short));
    bpptr += sizeof(u_short);     /* # of entries in root */

    tmpshort = byteswap(cbpp->bp_sectorcount);
    memcpy (bpptr, &tmpshort, sizeof(u_short));
    bpptr += sizeof(u_short);     /* # of sectors on disk */

    memcpy (bpptr, &cbpp->bp_mediatype, sizeof(u_char));
    bpptr += sizeof(u_char);      /* disk type */

    tmpshort = byteswap(cbpp->bp_fatsize);
    memcpy (bpptr, &tmpshort, sizeof(u_short));
    bpptr += sizeof(u_short);     /* FAT size in sectors */

    tmpshort = byteswap(cbpp->bp_sectorspertrack);
    memcpy (bpptr, &tmpshort, sizeof(u_short));
    bpptr += sizeof(u_short);     /* sectors per track */

    tmpshort = byteswap(cbpp->bp_headcount);
    memcpy (bpptr, &tmpshort, sizeof(u_short));
    bpptr += sizeof(u_short);     /* # of heads */

    memcpy (bpptr, &cbpp->bp_hiddensectors, sizeof(u_long));
                                  /* # of hidden sectors */

    return;
}


static void
fatunpack(u_char * bpp, bp_t * cbpp) 
{
    u_char * bpptr = bpp;

    bpptr += sizeof(u_char) * 3;  /* magic number */
    bpptr += sizeof(u_char) * 8;  /* oem identification */

    memcpy (&cbpp->bp_sectorsize, bpptr, sizeof(u_short));
    cbpp->bp_sectorsize = byteswap(cbpp->bp_sectorsize);
    bpptr += sizeof(u_short);     /* sector size in bytes */

    memcpy (&cbpp->bp_sectorspercluster, bpptr, sizeof(u_char));
    bpptr += sizeof(u_char);      /* cluster size in sectors */

    memcpy (&cbpp->bp_reservecount, bpptr, sizeof(u_short));
    cbpp->bp_reservecount= byteswap(cbpp->bp_reservecount);
    bpptr += sizeof(u_short);     /* # of reserved sectors */

    memcpy (&cbpp->bp_fatcount, bpptr, sizeof(u_char));
    bpptr += sizeof(u_char);      /* # of FAT */

    memcpy (&cbpp->bp_rootentries, bpptr, sizeof(u_short));
    cbpp->bp_rootentries = byteswap(cbpp->bp_rootentries);
    bpptr += sizeof(u_short);     /* # of entries in root */

    memcpy (&cbpp->bp_sectorcount, bpptr, sizeof(u_short));
    cbpp->bp_sectorcount = byteswap(cbpp->bp_sectorcount);
    bpptr += sizeof(u_short);     /* # of sectors on disk */

    memcpy (&cbpp->bp_mediatype, bpptr, sizeof(u_char));
    bpptr += sizeof(u_char);      /* disk type */

    memcpy (&cbpp->bp_fatsize, bpptr, sizeof(u_short));
    cbpp->bp_fatsize = byteswap(cbpp->bp_fatsize);
    bpptr += sizeof(u_short);     /* FAT size in sectors */

    memcpy (&cbpp->bp_sectorspertrack, bpptr, sizeof(u_short));
    cbpp->bp_sectorspertrack = byteswap(cbpp->bp_sectorspertrack);
    bpptr += sizeof(u_short);     /* sectors per track */

    memcpy (&cbpp->bp_headcount, bpptr, sizeof(u_short));
    cbpp->bp_headcount = byteswap(cbpp->bp_headcount);
    bpptr += sizeof(u_short);     /* # of heads */

    memcpy (&cbpp->bp_hiddensectors, bpptr, sizeof(u_long));
                                  /* # of hidden sectors */

    return;
}
