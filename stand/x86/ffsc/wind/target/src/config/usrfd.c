/* usrFd.c - floppy disk initialization */

/* Copyright 1992-1995 Wind River Systems, Inc. */

/*
modification history
--------------------
01c,24jan95,jdi  doc cleanup.
01b,25oct94,hdn  swapped 1st and 2nd parameter of fdDevCreate() and
		 usrFdConfig().
01a,25oct93,hdn  written.
*/

/*
DESCRIPTION
This file is used to configure and initialize the VxWorks floppy disk support.
This file is included by usrConfig.c.

SEE ALSO: usrExtra.c

NOMANUAL
*/

#ifndef  __INCusrFd
#define  __INCusrFd

/* global variables */


/*******************************************************************************
*
* usrFdConfig - mount a DOS file system from a floppy disk
*
* This routine mounts a DOS file system from a floppy disk device.
*
* The <drive> parameter is the drive number of the floppy disk;
* valid values are 0 to 3.
*
* The <type> parameter specifies the type of diskette, which is described
* in the structure table `fdTypes[]' in sysLib.c.  <type> is an index to
* the table.  Currently the table contains two diskette types:
* .iP "" 4
* A <type> of 0 indicates the first entry in the table (3.5" 2HD, 1.44MB);
* .iP
* A <type> of 1 indicates the second entry in the table (5.25" 2HD, 1.2MB).
* .LP
*
* The <fileName> parameter is the mount point, e.g., `/fd0/'.
*
* RETURNS: OK or ERROR.
*
* SEE ALSO:
* .pG "I/O System, Local File Systems, Intel i386/i486 Appendix"
*/

STATUS usrFdConfig
    (
    int     drive,	/* drive number of floppy disk (0 - 3) */
    int     type,	/* type of floppy disk */
    char *  fileName	/* mount point */
    )
    {
    BLK_DEV *pBootDev;
    char bootDir [BOOT_FILE_LEN];

    if ((UINT)drive >= FD_MAX_DRIVES)
	{
	printErr ("drive is out of range (0-%d).\n", FD_MAX_DRIVES - 1);
	return (ERROR);
	}

    /* create a block device spanning entire disk (non-distructive!) */

    if ((pBootDev = fdDevCreate (drive, type, 0, 0)) == NULL)
	{
        printErr ("fdDevCreate failed.\n");
        return (ERROR);
	}

    /* split off boot device from boot file */

    devSplit (fileName, bootDir);

    /* initialize the boot block device as a dosFs device named <bootDir> */

    if (dosFsDevInit (bootDir, pBootDev, NULL) == NULL)
	{
        printErr ("dosFsDevInit failed.\n");
        return (ERROR);
	}

    return (OK);
    }

#endif /* __INCusrFd */
