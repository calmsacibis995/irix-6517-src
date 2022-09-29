/* nec765Fd.h - NEC 765 floppy disk controller header */

/* Copyright 1984-1993 Wind River Systems, Inc. */
/*
modification history
--------------------
01e,31aug95,hdn  added error code,
		 FD_UNFORMATED FD_WRITE_PROTECTED FD_DISK_NOT_PRESENT
01d,25oct94,hdn  added FD_RAW structure for fdRawio().
		 added fdRawio() function declarations.
01c,22apr94,hdn  moved FD_DMA_BUF, FD_DMA_BUF_SIZE to config.h.
01b,06apr94,hdn  changed the DMA address from 0x1000 to 0x2000.
01a,30sep93,hdn  written.
*/

#ifndef __INCnec765Fdh
#define __INCnec765Fdh

#ifdef __cplusplus
extern "C" {
#endif


#ifndef _ASMLANGUAGE

#include "blkio.h"

typedef struct fdDev
    {
    BLK_DEV blkDev;
    int fdType;		/* floppy disk type */
    int drive;		/* drive no. 0 - 3 */
    int blkOffset;	/* sector offset */
    } FD_DEV;

typedef struct fdType
    {
    int sectors;	/* no of sectors */
    int sectorsTrack;	/* sectors per track */
    int heads;		/* no of heads */
    int cylinders;	/* no of cylinders */
    int secSize;	/* bytes per sector, 128 << secSize */
    char gap1;		/* gap1 size for read, write */
    char gap2;		/* gap2 size for format */
    char dataRate;	/* data transfer rate */
    char stepRate;	/* stepping rate */
    char headUnload;	/* head unload time */
    char headLoad;	/* head load time */
    char mfm;		/* MFM bit for read, write, format */
    char sk;		/* SK bit for read */
    char *name;		/* name */
    } FD_TYPE;

typedef struct fdRaw
    {				/* this is for IDERAWACCESS ioctl */
    UINT cylinder;		/* cylinder (0 -> (cylindres-1)) */
    UINT head;			/* head (0 -> (heads-1)) */
    UINT sector;			/* sector (1 -> sectorsTrack) */
    char *pBuf;			/* pointer to buffer (bytesSector * nSecs) */
    UINT nSecs;			/* number of sectors (1 -> sectorsTrack) */
    UINT direction;		/* read=0, write=1 */
    } FD_RAW;


/* max number of FD drives */

#define FD_MAX_DRIVES		4

/* error code */

#define FD_UNFORMATED		-2	/* unformated diskette */
#define FD_WRITE_PROTECTED	-3	/* write protected diskette */
#define FD_DISK_NOT_PRESENT	-4	/* no diskette */


/* FDC IO address */

#define FD_REG_OUTPUT		0x3f2
#define FD_REG_STATUS		0x3f4
#define FD_REG_COMMAND		0x3f4
#define FD_REG_DATA		0x3f5
#define FD_REG_INPUT		0x3f7
#define FD_REG_CONFIG		0x3f7

/* FDC Digital Output Register */

#define FD_DOR_DRIVE0_SEL	0x00
#define FD_DOR_DRIVE1_SEL	0x01
#define FD_DOR_DRIVE2_SEL	0x02
#define FD_DOR_DRIVE3_SEL	0x03
#define FD_DOR_RESET		0x00
#define FD_DOR_CLEAR_RESET	0x04
#define FD_DOR_DMA_DISABLE	0x00
#define FD_DOR_DMA_ENABLE	0x08
#define FD_DOR_DRIVE_ON		0x10

/* FDC Main Status Register */

#define FD_MSR_DRIVE0_SEEK	0x01
#define FD_MSR_DRIVE1_SEEK	0x02
#define FD_MSR_DRIVE2_SEEK	0x04
#define FD_MSR_DRIVE3_SEEK	0x08
#define FD_MSR_FD_BUSY		0x10
#define FD_MSR_EXEC_MODE	0x20
#define FD_MSR_DIRECTION	0x40
#define FD_MSR_RQM		0x80

/* FDC Commands */

#define FD_CMD_SPECIFY		0x03
#define FD_CMD_SENSEDRIVE	0x04
#define FD_CMD_RECALIBRATE	0x07
#define FD_CMD_SENSEINT		0x08
#define FD_CMD_SEEK		0x0f
#define FD_CMD_READ		0x06
#define FD_CMD_WRITE		0x05
#define FD_CMD_FORMAT		0x0d

#define FD_CMD_LEN_SPECIFY	3
#define FD_CMD_LEN_SENSEDRIVE	2
#define FD_CMD_LEN_RECALIBRATE	2
#define FD_CMD_LEN_SENSEINT	1
#define FD_CMD_LEN_SEEK		3
#define FD_CMD_LEN_RW		9
#define FD_CMD_LEN_FORMAT	6

/* FDC's DMA channel */

#define FD_DMA_CHAN		2


/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

STATUS		fdDrv		(int vector, int level);
BLK_DEV		*fdDevCreate	(int fdType, int drive, int nBlks, int offset);
STATUS		fdRawio		(int drive, int fdType, FD_RAW *pFdRaw);

#else

STATUS		fdDrv		();
BLK_DEV		*fdDevCreate	();
STATUS		fdRawio		();

#endif  /* __STDC__ */

#endif  /* _ASMLANGUAGE */

#ifdef __cplusplus
}
#endif

#endif /* __INCnec765Fdh */

