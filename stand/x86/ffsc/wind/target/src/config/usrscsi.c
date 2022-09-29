/* usrScsi.c - SCSI initialization */

/* Copyright 1992-1994 Wind River Systems, Inc. */

/*
modification history
--------------------
01e,31aug94,ism  added a #ifdef to keep the RT11 file system from being
		included just because SCSI is included. (SPR #3572)
01d,11jan93,ccc  removed 2nd parameter from call to scsiAutoConfig().
01c,24dec92,jdi  removed note about change of 4th param in scsiPhysDevCreate();
		 mentioned where usrScsiConfig() is found.
01b,24sep92,ccc  note about change of 4th parameter in scsiPhysDevCreate().
01a,18sep92,jcf  written.
*/

/*
DESCRIPTION
This file is used to configure and initialize the VxWorks SCSI support.
This file is included by usrConfig.c.

SEE ALSO: usrExtra.c

NOMANUAL
*/

#ifndef  __INCusrScsic
#define  __INCusrScsic

/* global variables */

SCSI_PHYS_DEV *	pSpd20;
SCSI_PHYS_DEV *	pSpd31;		/* SCSI_PHYS_DEV ptrs (suffix == ID, LUN) */
BLK_DEV *	pSbd0;
BLK_DEV *	pSbd1;
BLK_DEV *	pSbd2;		/* SCSI_BLK_DEV ptrs for Winchester */
BLK_DEV *	pSbdFloppy;	/* ptr to SCSI floppy block device */


/*******************************************************************************
*
* usrScsiConfig - configure a SCSI peripheral (example)
*
* This routine is an example of how to declare a SCSI peripheral
* configuration.  You must edit this routine to reflect the actual
* configuration of your SCSI bus.
* This example is found in src/config/usrScsi.c.
*
* If you are just getting started, you can test your hardware configuration
* by defining SCSI_AUTO_CONFIG in config.h, which will probe the bus and
* display all devices found.  No device should have the same SCSI bus ID as
* your VxWorks SCSI port (default = 7), or the same as any other device.
* Check for proper bus termination.
*
* As an aid to debugging, either of the variables <scsiDebug> or
* <scsiIntsDebug> can be set to TRUE (1).  When the hardware is
* working, be sure to undefine SCSI_AUTO_CONFIG.  Of course, you must
* rework the rest of this routine to conform to your own hardware, as well as
* associated partitioning and file system mappings.
*
* In this example, there are two disk drives on the bus, an 80-Mbyte Winchester
* disk (ID=2, LUN=0) and a 1.2-Mbyte 5.25" floppy drive (ID=3, LUN=1).  The
* floppy is actually interfaced via an OMTI 3500 universal SCSI-to-floppy
* adapter.
*
* The Winchester disk is divided into two 32-Mbyte partitions and a third
* partition with the remainder of the disk.  The first partition is initialized
* as a dosFs device.  The second and third partitions are initialized as
* rt11Fs devices, each with 256 directory entries.
*
* It is recommended that the first partition (BLK_DEV) on a block device be
* a dosFs device, if the intention is eventually to boot VxWorks from the
* device.  This will simplify the task considerably.
*
* The floppy, since it is a removable medium device, is allowed to have only a
* single partition, and dosFs is the file system of choice for this device,
* since it facilitates media compatibility with IBM PC machines.
*
* While the Winchester configuration is fairly straightforward, the floppy
* setup in this example is a bit intricate.  Note that the
* scsiPhysDevCreate() call is issued twice.  The first time is merely to get
* a "handle" to pass to the scsiModeSelect() function, since the default
* media type is sometimes inappropriate (in the case of generic
* SCSI-to-floppy cards).  After the hardware is correctly configured, the
* handle is discarded via the scsiPhysDevDelete() call, after which a
* second scsiPhysDevCreate() call correctly configures the peripheral.
* (Before the scsiModeSelect() call, the configuration information was
* incorrect.) Also note that following the scsiBlkDevCreate() call, the
* correct values for <sectorsPerTrack> and <nHeads> must be set via the
* scsiBlkDevInit() call.  This is necessary for IBM PC compatibility.
*
* The last parameter to the dosFsDevInit() call is a pointer to a
* DOS_VOL_CONFIG structure.  By specifying NULL, you are asking
* dosFsDevInit() to read this information off the disk in the drive.  This
* may fail if no disk is present or if the disk has no valid dosFs
* directory.  Should this be the case, you can use the dosFsMkfs() command to
* create a new directory on a disk.  This routine uses default parameters
* (see dosFsLib) that may not be suitable for your application, in which
* case you should use dosFsDevInit() with a pointer to a valid DOS_VOL_CONFIG
* structure that you have created and initialized.  If dosFsDevInit() is
* used, a diskInit() call should be made to write a new directory on the
* disk, if the disk is blank or disposable.
*
* NOTE
* The variable <pSbdFloppy> is global to allow the above calls to be
* made from the VxWorks shell, i.e.:
* .CS
*     -> dosFsMkfs "/fd0/", pSbdFloppy
* .CE
* If a disk is new, use diskFormat() to format it.
*
* INTERNAL
* The fourth parameter passed to scsiPhysDevCreate() is now
* <reqSenseLength> (previously <selTimeout>).
*
* RETURNS: OK or ERROR.
*
* SEE ALSO:
* .pG "I/O System, Local File Systems"
*/

STATUS usrScsiConfig ()

    {
    char modeData [4];		/* array for floppy MODE SELECT data */

    /* NOTE: Either of the following global variables may be set or reset
     * from the VxWorks shell. Under 5.0, they should NOT both be set at the
     * same time, or output will be interleaved and hard to read!! They are
     * intended as an aid to trouble-shooting SCSI problems.
     */

#if FALSE			/* by default, these are NOT turned on */
    scsiDebug = TRUE;		/* enable SCSI debugging output */
    scsiIntsDebug = TRUE;	/* enable SCSI interrupt debugging output */
#endif	/* FALSE */

    taskDelay (sysClkRateGet () * 1);	/* allow devices to reset */

#ifdef SCSI_AUTO_CONFIG		/* configure physical devices on SCSI bus */

    printf ("Auto-configuring SCSI bus...\n\n");

    scsiAutoConfig (pSysScsiCtrl);
    scsiShow (pSysScsiCtrl);
    printf ("\n");
    return (OK);

#else	/* !SCSI_AUTO_CONFIG */

    /* configure Winchester at busId = 2, LUN = 0 */

    if ((pSpd20 = scsiPhysDevCreate (pSysScsiCtrl, 2, 0, 0, NONE, 0, 0, 0))
        == (SCSI_PHYS_DEV *) NULL)
	{
        SCSI_DEBUG_MSG ("usrScsiConfig: scsiPhysDevCreate failed.\n",
			0, 0, 0, 0, 0, 0);
	}
    else
	{
	/* create block devices */

        if (((pSbd0 = scsiBlkDevCreate (pSpd20, 0x10000, 0)) == NULL)       ||
            ((pSbd1 = scsiBlkDevCreate (pSpd20, 0x10000, 0x10000)) == NULL) ||
            ((pSbd2 = scsiBlkDevCreate (pSpd20, 0, 0x20000)) == NULL))
	    {
    	    return (ERROR);
	    }

        if ((dosFsDevInit  ("/sd0/", pSbd0, NULL) == NULL) )
	    {
	    return (ERROR);
	    }

#ifdef INCLUDE_RT11FS
	    if ((rt11FsDevInit ("/sd1/", pSbd1, 0, 256, TRUE) == NULL) ||
	    (rt11FsDevInit ("/sd2/", pSbd2, 0, 256, TRUE) == NULL))
	    {
	    return (ERROR);
	    }
#endif
	}

    /* configure floppy at busId = 3, LUN = 1 */

    if ((pSpd31 = scsiPhysDevCreate (pSysScsiCtrl, 3, 1, 0, NONE, 0, 0, 0))
	== (SCSI_PHYS_DEV *) NULL)
	{
        printErr ("usrScsiConfig: scsiPhysDevCreate failed.\n");
	return (ERROR);
	}

    /* Zero modeData array, then set byte 1 to "medium code" (0x1b). NOTE:
     * MODE SELECT data is highly device-specific. If your device requires
     * configuration via MODE SELECT, please consult the device's Programmer's
     * Reference for the relevant data format.
     */

    bzero (modeData, sizeof (modeData));
    modeData [1] = 0x1b;

    /* issue the MODE SELECT command to correctly configure floppy controller */

    scsiModeSelect (pSpd31, 1, 0, modeData, sizeof (modeData));

    /* delete and re-create the SCSI_PHYS_DEV so that INQUIRY will return the
     * new device parameters, i.e., correct number of blocks
     */

    scsiPhysDevDelete (pSpd31);

    if ((pSpd31 = scsiPhysDevCreate (pSysScsiCtrl, 3, 1, 0, NONE, 0, 0, 0))
	== (SCSI_PHYS_DEV *) NULL)
	{
        printErr ("usrScsiConfig: scsiPhysDevCreate failed.\n");
	return (ERROR);
	}

    if ((pSbdFloppy = scsiBlkDevCreate (pSpd31, 0, 0)) == NULL)
	{
        printErr ("usrScsiConfig: scsiBlkDevCreate failed.\n");
	return (ERROR);
	}

    /* Fill in the <blksPerTrack> (blocks (or sectors) per track) and <nHeads>
     * (number of heads) BLK_DEV fields, since it is impossible to ascertain
     * this information from the SCSI adapter card. This is important for
     * PC compatibility, primarily.
     */

    scsiBlkDevInit ((SCSI_BLK_DEV *) pSbdFloppy, 15, 2);

    /* Initialize as a dosFs device */

    /* NOTE: pSbdFloppy is declared globally in case the following call
     * fails, in which case dosFsMkfs or dosFsDevInit can be
     * called (from the shell) with pSbdFloppy as an argument
     * (assuming pSbdFloppy != NULL)
     */

    if (dosFsDevInit ("/fd0/", pSbdFloppy, NULL) == NULL)
	{
        printErr ("usrScsiConfig: dosFsDevInit failed.\n");
	return (ERROR);
	}

    return (OK);

#endif	/* SCSI_AUTO_CONFIG */
    }

#endif /* __INCusrScsic */
