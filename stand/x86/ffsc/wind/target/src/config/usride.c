/* usrIde.c - IDE initialization */

/* Copyright 1992-1995 Wind River Systems, Inc. */

/*
modification history
--------------------
01d,24jan95,jdi  doc cleanup.
01c,12oct94,hdn  used ideRawio() instead of using raw file system.
01b,25oct93,hdn  tuned for IDE driver.
01a,07oct93,sst  part of this program was written by S.Stern
*/

/*
DESCRIPTION
This file is used to configure and initialize the VxWorks IDE support.
This file is included by usrConfig.c.

SEE ALSO: usrExtra.c

NOMANUAL
*/

#ifndef  __INCusrIde
#define  __INCusrIde

#define DOS_ID_OFFSET			3
#define FIRST_PARTITION_SECTOR_OFFSET	(0x1be + 8)
#define VXDOS				"VXDOS"
#define VXEXT				"VXEXT"
 

/*******************************************************************************
*
* usrIdeConfig - mount a DOS file system from an IDE hard disk
*
* This routine mounts a DOS file system from an IDE hard disk.
*
* The <drive> parameter is the drive number of the hard disk;
* 0 is `C:' and 1 is `D:'.
*
* The <fileName> parameter is the mount point, e.g., `/ide0/'.
*
* NOTE: Because VxWorks does not support partitioning, hard disks formatted
* and initialized on VxWorks are not compatible with DOS machines.  This
* routine does not refuse to mount a hard disk that was initialized on
* VxWorks.  The hard disk is assumed to have only one partition with a
* partition record in sector 0.
*
* RETURNS: OK or ERROR.
*
* SEE ALSO:
* .pG "I/O System, Local File Systems, Intel i386/i486 Appendix"
*/

STATUS usrIdeConfig
    (
    int     drive,	/* drive number of hard disk (0 or 1) */
    char *  fileName	/* mount point */
    )
    {
    BLK_DEV *pBlkDev;
    char bootDir [BOOT_FILE_LEN];
    char buffer [1024];
    int offset = 0;
    IDE_RAW ideRaw;
    IDE_RAW *pIdeRaw = &ideRaw;


    if ((UINT)drive >= IDE_MAX_DRIVES)
	{
	printErr ("drive is out of range (0-%d).\n", IDE_MAX_DRIVES - 1);
	return (ERROR);
	}

    /* split off boot device from boot file */

    devSplit (fileName, bootDir);

    /* read the boot sector */

    pIdeRaw->cylinder	= 0;
    pIdeRaw->head	= 0;
    pIdeRaw->sector	= 1;
    pIdeRaw->pBuf	= buffer;
    pIdeRaw->nSecs	= 1;
    pIdeRaw->direction	= 0;

    ideRawio (drive, pIdeRaw);

    /* get an offset if it is formated by MSDOS */

    if ((strncmp((char *)(buffer+DOS_ID_OFFSET), VXDOS, strlen(VXDOS)) != 0) ||
        (strncmp((char *)(buffer+DOS_ID_OFFSET), VXEXT, strlen(VXEXT)) != 0))
        {
        offset =  buffer[FIRST_PARTITION_SECTOR_OFFSET + 3] << 12;
        offset |= buffer[FIRST_PARTITION_SECTOR_OFFSET + 2] << 8;
        offset |= buffer[FIRST_PARTITION_SECTOR_OFFSET + 1] << 4;
        offset |= buffer[FIRST_PARTITION_SECTOR_OFFSET];
	}
  
    if ((pBlkDev = ideDevCreate(drive, 0, offset)) == (BLK_DEV *)NULL)
        {
        printErr ("Error during ideDevCreate: %x\n", errno);
        return (ERROR);
        }

    /* Make DOS file system */

    if (dosFsDevInit(bootDir, pBlkDev, NULL) == (DOS_VOL_DESC *)NULL)
        {
	printErr ("Error during dosFsDevInit: %x\n", errno);
        return (ERROR);
        }

    return (OK);
    }

#endif /* __INCusrIde */
