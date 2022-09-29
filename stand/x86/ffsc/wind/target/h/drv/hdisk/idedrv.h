/* ideDrv.h - IDE disk controller header */

/* Copyright 1984-1993 Wind River Systems, Inc. */
/*
modification history
--------------------
01c,25oct94,hdn  added ideRawio() function prototype.
01b,10oct94,hdn  added IDE_RAW structure for ideRawio().
01a,19oct93,hdn  written.
*/

#ifndef __INCideDrvh
#define __INCideDrvh

#ifdef __cplusplus
extern "C" {
#endif


#ifndef _ASMLANGUAGE

#include "blkio.h"

typedef struct ideDev
    {
    BLK_DEV blkDev;
    int drive;			/* drive no. 0 - 1 */
    int blkOffset;		/* sector offset */
    } IDE_DEV;

typedef struct ideType
    {
    int cylinders;		/* number of cylinders */
    int heads;			/* number of heads */
    int sectorsTrack;		/* number of sectors per track */
    int bytesSector;		/* number of bytes per sector */
    int precomp;		/* precompensation cylinder */
    } IDE_TYPE;

typedef struct ideParams 
    {
    short config;		/* general configuration */
    short cylinders;		/* number of cylinders */
    short removcyl;		/* number of removable cylinders */
    short heads;		/* number of heads */
    short bytesTrack;		/* number of unformatted bytes/track */
    short bytesSec;		/* number of unformatted bytes/sector */
    short sectorsTrack;		/* number of sectors/track */
    short bytesGap;		/* minimum bytes in intersector gap */
    short bytesSync;		/* minimum bytes in sync field */
    short vendstat;		/* number of words of vendor status */
    char serial[20];		/* controller serial number */
    short type;			/* controller type */
    short size;			/* sector buffer size, in sectors */
    short bytesEcc;		/* ecc bytes appended */
    char rev[8];		/* firmware revision */
    char model[40];		/* model name */
    short nsecperint;		/* sectors per interrupt */
    short usedmovsd;		/* can use double word read/write? */
    short spare[207];		/* */
    } IDE_PARAM;

typedef struct ideRaw
    {				/* this is for IDERAWACCESS ioctl */
    UINT cylinder;		/* cylinder (0 -> (cylindres-1)) */
    UINT head;			/* head (0 -> (heads-1)) */
    UINT sector;			/* sector (1 -> sectorsTrack) */
    char *pBuf;			/* pointer to buffer (bytesSector * nSecs) */
    UINT nSecs;			/* number of sectors (1 -> sectorsTrack) */
    UINT direction;		/* read=0, write=1 */
    } IDE_RAW;


/* max number of IDE drives */

#define IDE_MAX_DRIVES	2


/* IDE registers */

#define	IDE_DATA	0x1f0		/* (RW) data register (16 bits)	*/
#define IDE_ERROR	0x1f1		/* (R)  error register		*/
#define	IDE_PRECOMP	0x1f1		/* (W)  write precompensation	*/
#define	IDE_SECCNT	0x1f2		/* (RW) sector count		*/
#define	IDE_SECTOR	0x1f3		/* (RW) first sector number	*/
#define	IDE_CYL_LO	0x1f4		/* (RW) cylinder low byte	*/
#define	IDE_CYL_HI	0x1f5		/* (RW) cylinder high byte	*/
#define	IDE_SDH		0x1f6		/* (RW) sector size/drive/head	*/
#define	IDE_COMMAND	0x1f7		/* (W)  command register	*/
#define	IDE_STATUS 	0x1f7		/* (R)  immediate status	*/
#define	IDE_A_STATUS	0x3f6	 	/* (R)  alternate status	*/
#define	IDE_D_CONTROL	0x3f6	 	/* (W)  disk controller control	*/
#define	IDE_D_ADDRESS	0x3f7	 	/* (R)  disk controller address */


/* diagnostic code */

#define DIAG_OK		0x01

/* control register */

#define  CTL_4BIT	 0x8		/* use 4 head bits (wd1003) */
#define  CTL_RST	 0x4		/* reset controller */
#define  CTL_IDS	 0x2		/* disable interrupts */

/* status register */

#define	STAT_BUSY	0x80		/* controller busy */
#define	STAT_READY	0x40		/* selected drive ready */
#define	STAT_WRTFLT	0x20		/* write fault */
#define	STAT_SEEKCMPLT	0x10		/* seek complete */
#define	STAT_DRQ	0x08		/* data request */
#define	STAT_ECCCOR	0x04		/* ECC correction made in data */
#define	STAT_INDEX	0x02		/* index pulse from selected drive */
#define	STAT_ERR	0x01		/* error detect */

/* size/drive/head register */

#define SDH_IBM		0xa0		/* 512 bytes sector, ecc */

/* commands */

#define	CMD_RECALIB	0x10		/* recalibrate */
#define	CMD_SEEK	0x70		/* seek */
#define	CMD_READ	0x20		/* read sectors with retries */
#define	CMD_WRITE	0x30		/* write sectors with retries */
#define	CMD_FORMAT	0x50		/* format track */
#define	CMD_DIAGNOSE	0x90		/* execute controller diagnostic */
#define	CMD_INITP	0x91		/* initialize drive parameters */
#define	CMD_READP	0xEC		/* identify */


/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

STATUS	ideDrv (int vector, int level, BOOL manualConfig);
BLK_DEV	*ideDevCreate (int drive, int nBlks, int offset);
STATUS	ideRawio (int drive, IDE_RAW *pIdeRaw);

#else

STATUS	ideDrv ();
BLK_DEV	*ideDevCreate ();
STATUS	ideRawio ();

#endif  /* __STDC__ */

#endif  /* _ASMLANGUAGE */

#ifdef __cplusplus
}
#endif

#endif /* __INCideDrvh */
