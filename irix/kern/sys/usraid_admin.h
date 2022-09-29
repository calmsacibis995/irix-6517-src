/**************************************************************************
 *									  *
 * 		 Copyright (C) 1993, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 1.3 $"

/*
 * Administrative interface definitions for the UltraStor U144F RAID controller.
 *
 * These are definitions of arguments and argument formats for use
 * with the ioctl() syscall against a file descriptor for the open
 * block or character device referring to an UltraStor U144F RAID
 * controller.
 *
 * The USRAID device driver opens the RAID device with the EXCLUSIVE
 * flag, so no other access to the device is permitted (eg: via /dev/scsi)
 * while the device is open.  The driver releases the exclusive flag
 * when there are no active opens of the device through that driver,
 * however, so operations not supported by this driver can be performed.
 *
 * Please note that this interface may change at any time.
 */

#define	USRAID_DEVNAME		"rad"		/* as in /dev/rdsk/rad4d2vh */

#define	USRAID_IOC_UPPER	(('R'<<24) | ('A'<<16) | ('D'<<8))
#define	USRAID_IOC_MASK		(~0xff)		/* is this a USRAID ioctl? */
#define	USRAID_IOC_(NUM)	(USRAID_IOC_UPPER | (NUM))

/* XXXGROT: renumber the IOC's */

/*============================================================================
 * UltraStor U144F Specific "Maintenance" Commands
 *============================================================================*/

/*
 * There can be only one "maintenance" command in process on the
 * controller at any one time.  A log page tells if there is a
 * maintenance command executing on the controller, what type of
 * command it is (eg: rebuild, integrity check, etc), and the first
 * LBA of the stripe just completed by that command.  It is intended
 * that that information be used by the host to manage the operation
 * of maintenance commands on the controller.
 *
 * The maintenance commands all support a particular method for
 * asynchronous operation.  That method uses two bits in the command,
 * the "immediate" bit and the "abort" bit.
 *
 * The "immediate" bit is used to initiate a maintenance operation
 * and have the command return good status immediately.  The
 * maintenance command continues operating in the controller, but
 * the host is not waiting for a response.  The command operates in
 * the normal block-and-wait-for-completion mode if the "immediate"
 * bit is not set.
 *
 * If an error occurs and the "immediate" bit is not set, traditional
 * error processing happens.  If the "immediate" bit is set, a check
 * condition is returned to the host via either the sentinel command
 * or the next available command.  The returned sense information will
 * then indicate that an "immediate" maintenance command has failed,
 * and the LBA of the error.  If no error occurs and the maintenance
 * command finishes successfully, then no direct indication of that
 * completion is given to the host.  In either case, the log page will
 * be updated to show that there are no active maintenance commands.
 *
 * The "abort" bit is used to interrupt a maintenance operation that
 * is currently running on the controller.  If a maintenance command
 * of type "X" is running, sending another "X" command to the controller,
 * this one with the "abort" bit set, will cause the first "X" command
 * to gracefully stop.
 *
 * As an example, if an "integrity check" command is active, sending
 * another "integrity check" command with with the "abort" bit set
 * will cause the controller to stop checking integrity.  
 *
 * If the active maintenance command did not have the "immediate" bit
 * set, it will return with an error (that it was aborted).  If it
 * was started with the "immediate" bit, there will be no indication
 * to the host.
 *
 * If the active maintenance command is aborted, the second command
 * (the one with the "abort" bit set) will return a good completion.
 * If there are no active maintenance commands to abort, or there is
 * some other problem, the second command will return with an error.
 *
 * SGI plans not to use the "immediate" bit, but will use "abort".
 * That should avoid most of the complexity of the above description.
 */


/*
 * Initialize Logical Unit
 *
 * This command initializes the logical drive to a pattern of 0x00.
 * It starts from the sector specified in the LBA field and runs through
 * the end of the drive.  If the initialize fails for some reason,
 * the LBA of the failure will be returned.
 * 
 * The SCSI "format" command is an alias (with the LBA defaulted to 0)
 * for this command.
 *
 * The ioctl arg should be an unsigned int giving the starting LBA
 * for the operation.
 */
#define USRAID_INITDRIVE	USRAID_IOC_(0x01)


/*
 * Scan Logical Unit
 *
 * This command reads the contents of the logical drive starting at the
 * specified LBA.  It will run until an error occurs or the the end of
 * the drive is reached.  If an error occurs, the LBA of the failure will
 * be returned.
 *
 * Note that this command does simple reads, basically checking for
 * unreadable sectors, it does not compare the data against the parity.
 * See the "check integrity" command.
 *
 * The ioctl arg should be an unsigned int giving the starting LBA
 * for the operation.
 */
#define USRAID_SCANDRIVE	USRAID_IOC_(0x02)


/*
 * Integrity Check
 *
 * This command initiates an integrity check of the parity sectors of
 * the indicated logical unit.  The integrity check starts at the
 * indicated LBA, and continues through the end of the drive or until
 * an error occurs.  It will either return a command complete (the
 * whole drive's integrity is good), or an error (indicating the address
 * that the failure occurred).
 * 
 * Each stripe is checked to see if the data and parity correspond.
 *
 * The ioctl arg should be an unsigned int giving the starting LBA
 * for the operation.
 */
#define USRAID_INTEGRITY	USRAID_IOC_(0x03)


/*
 * Fix Integrity
 *
 * This command is used if the Integrity Check command failed.
 *
 * This command will update the parity block of the stripe specified by
 * the LBA so that the parity block is correct for the existing data in
 * that stripe.  It will only operate on one stripe.
 *
 * It will return a command complete with no error (the stripe has been
 * restored), or with an error (indicating the address that the failure 
 * occured).
 *
 * The ioctl arg should be an unsigned int.  The starting LBA of the
 * stripe to be fixed is passed in through that long.
 */
#define USRAID_FIXINTEG		USRAID_IOC_(0x04)


/*
 * Rebuild
 *
 * This command rebuilds the data of the specified logical drive.
 * The failed physical unit is NOT determined by internal data
 * structures but from the drive-ID/channel-number specified in the
 * command.  The command will start reconstructing that drive using
 * the existing data sectors and the parity sectors.
 *
 * This command will not complete until either reconstruction is
 * complete, or an unrecoverable error is encountered on the non-failed
 * drives.
 *
 * The ioctl arg should be a pointer to the following structure.
 * The "drive" field tell which drive must be rebuilt.  The "lba"
 * field contains starting LBA for the rebuild, and the value is
 * modified to the first LBA of the last completed stripe on error
 * or good completion.
 */
#define USRAID_REBUILD		USRAID_IOC_(0x05)
struct usraid_rebuild {
	int		drive;	/* which drive to rebuild */
	unsigned int	lba;	/* IN: starting LBA, OUT: final LBA */
};


/*============================================================================
 * Other UltraStor U144F Specific Commands
 *============================================================================*/


/*
 * Execute Pass Through Command
 *
 * This command issues the included CDB as a normal SCSI command to a
 * specific physical disk within the RAID.
 *
 * The command is excecuted in a single threaded mode, ie: no other
 * activity on that drive, and no other commands will be accepted
 * while this command is executing.
 *
 * If the passed through command needs or generates data, it will be
 * transferred from the host to the controller, which will pass it
 * on the the drive.
 *
 * The ioctl arg should be a pointer to the following structure:
 *
 * The drive field identifies which physical drive the command is to
 * be pass to.  The drives are numbered from 0 to 4; corresponding
 * to SCSI IDs and slot numbers.
 *
 * The "fromdisk", "todisk", and "xfercount" fields describe the data
 * transfer required by the executing command.  "fromdisk" and "todisk"
 * can be any combination except 1 and 1.  If any data is to be
 * transferred (ie: fromdisk or todisk is nonzero), then "xfercount"
 * must be nonzero.
 *
 * The SCSI command to be executed is stored in "cdb", and "cdblen"
 * tells how many bytes are valid.
 *
 * The "databuf" field should point to a buffer that either contains
 * the data that the passed through command will be requesting, or a
 * buffer large enough to accept the data being passed back.  If there
 * is no data transfer for the passed through command, the arg may be
 * NULL.  If data transfer is required and the "databuf" field is NULL,
 * an error will be returned.
 *
 * If the passed through command encounters an error, sense data
 * will be gotten and placed in the "sensebuf" field.
 */
#define USRAID_PASSTHRU		USRAID_IOC_(0x07)
struct usraid_passthru {
	int	drive;		/* which slot to pass thru to */
	int	fromdisk;	/* data xfer disk->controller (T/F) */
	int	todisk;		/* data xfer controller->disk (T/F) */
	int	xfercount;	/* # bytes of data to be xfer'd by CDB */
	int	cdblen;		/* number of valid bytes in CDB */
	u_char	cdb[12];	/* the SCSI command block itself */
	caddr_t	databuf;	/* ptr to data for CDB */
	u_char	sensebuf[64];	/* sense data if CDB gets an error */
};

#if _KERNEL
struct irix5_usraid_passthru {
	int	drive;		/* which slot to pass thru to */
	int	fromdisk;	/* data xfer disk->controller (T/F) */
	int	todisk;		/* data xfer controller->disk (T/F) */
	int	xfercount;	/* # bytes of data to be xfer'd by CDB */
	int	cdblen;		/* number of valid bytes in CDB */
	u_char	cdb[12];	/* the SCSI command block itself */
	app32_ptr_t databuf;	/* ptr to data for CDB */
	u_char	sensebuf[64];	/* sense data if CDB gets an error */
};
#endif /* _KERNEL */


/*
 * Download new firmware into the controller
 *
 * This command is used to download new firmware into the FLASH EPROM
 * on the controller.
 *
 * The *firmware image* is not checked for validity in any way by the
 * system, the controller is final and only judge of acceptability of
 * the new *image*.  On the other hand, the system will refuse to
 * pass the image to the controller if the controller is not already
 * running an SGI version of firmware.
 *
 * The ioctl arg should point to the following structure.  The buffer
 * field should point to a memory area contining the image to be
 * downloaded, and the size field should contain the number of valid
 * bytes in that image.
 */
#define USRAID_DOWNLOAD		USRAID_IOC_(0x19)
struct usraid_download {
	caddr_t	buffer;		/* firmware image in memory buffer */
	int	size;		/* size of firmware image */
};

#if _KERNEL
struct irix5_usraid_download {
	app32_ptr_t	buffer;		/* firmware image in memory buffer */
	int	size;		/* size of firmware image */
};
#endif /* _KERNEL */


/*
 * Isolate Channel
 * Un-Isolate Channel
 *
 * Isolate channel -
 *     This command makes the specified channel, with all units attached
 *     to it, inactive.  The purpose is to allow the user to replace a
 *     failed unit with a new one.  The effect is exactly the same as if
 *     the disk side SCSI channel had failed.
 *
 * Unisolate channel -
 *     This command makes the specified channel, with all units attached
 *     to it, active after a failed unit has been replaced.  There is no
 *     recovery associated with this command, just re-enabling the SCSI
 *     channel.
 *
 * The ioctl arg should be an integer showing which drive to operate on.
 */
#define USRAID_ISOLATE		USRAID_IOC_(0x0a)
#define USRAID_UNISOLATE	USRAID_IOC_(0x0b)


/*
 * Return Physical Connection
 *
 * This command returns drive specific information about the physical
 * disk in the indicated slot in the RAID.  The SCSI target ID of each
 * drive must match the slot number (ie: the SCSI channel number) that
 * it is plugged into.  If there is no disk in the given slot, then an
 * error will be returned.
 *
 * The ioctl arg should be a pointer to the following structure.  The
 * "drive" field is set to indicate which drive information in requested
 * on, and the other fields are filled in as a result.
 */
#define USRAID_PHYS_CONN	USRAID_IOC_(0x0c)
struct usraid_phys_conn {
	int	drive;		/* IN: which drive to ask about */
	char	vendor[9];	/* OUT: vendor ID string (8 valid bytes) */
	char	product[17];	/* OUT: product ID string (16 valid bytes) */
	char	revision[5];	/* OUT: revision ID string (4 valid bytes) */
	int	totalsize;	/* OUT: total number sectors on drive */
	int	blocksize;	/* OUT: sector size of drive */
};


/*
 * Return Physical Units Down
 * Clear Physical Units Down
 * Set Physical Units Down
 *
 * Return physical units down -
 *     The data will show which drives are marked as being down.
 *     A non-zero value shows the corresponding drive is down.
 *
 * Clear physical units down -
 *     The data will show which drives are are to be marked as being
 *     up again.  A non-zero value says to mark the drive as up.
 *     This command is normally used after the host has completed its
 *     drive replacement procedure in order to declare that the drive
 *     is now usable.
 *
 * Set physical units down -
 *     The data will show which drives are are to be marked as being
 *     down.  A non-zero value says to mark the drive as down.  This
 *     command is used to force the controller to treat a physical
 *     disk in the RAID as if it had failed.
 *
 * In SGI's usage of the U144F controller, the SCSI target ID of
 * a drive must match the channel number that it is plugged into.
 * There are 5 elements in the array, indexed by the slot-number.
 * Each element refers to the corresponding drive.
 *
 * The ioctl arg should be a pointer to an array of 5 integers.  The
 * elements of the array will be used to reference the corresponding
 * drive in the RAID.  A non-zero element shows that a drive is down,
 * or that you would like it to be changed to down or up.
 */
#define USRAID_RET_DOWN		USRAID_IOC_(0x0d)
#define USRAID_CLR_DOWN		USRAID_IOC_(0x0e)
#define USRAID_SET_DOWN		USRAID_IOC_(0x0f)
struct usraid_units_down {
	int	drive[5];
};


/*
 * Get NVRAM
 * Set NVRAM
 * Clear NVRAM
 *
 * Get NVRAM -
 *     The ioctl will return 4 bytes from the controller's NVRAM.
 *     The 4 bytes will come from byte offset "4*addr" into the
 *     controller's NVRAM.  This command is normally used to read
 *     saved configuration state in the NVRAM.
 *
 * Set NVRAM -
 *     The ioctl will transfer 4 bytes to the controller's NVRAM.
 *     The 4 bytes will be put in byte offset "4*addr" into the
 *     controller's NVRAM.  This command is normally used to write
 *     saved configuration state in the NVRAM.
 *
 * Clear NVRAM - 
 *     This ioctl will clears the "key" byte in the NVRAM so that
 *     when the controller is next power cycled, the entire NVRAM
 *     will be re-initialized to the factory ship state.  All
 *     settings or status stored in the NVRAM will be lost.
 *     NOTE: this command has no data phase, no arguments are required.
 *
 * The NVRAM is accessed in 32 bit segments.  If the NVRAM were an
 * array of 32 bit quantities, the "addr" would be the array index
 * of the desired entry.  An "addr" of 6 will return bytes 24, 25,
 * 26, and 27 from the NVRAM.  The NVRAM is 256 bytes long, and SGI
 * has exclusive use of the last 32 bytes (8 words).
 *
 * For the "get" and "put" commands, the ioctl arg should be a
 * pointer to the following structure.  The "addr" field is set
 * to indicate which word in NVRAM is to be operated on, and the
 * "data" field is filled in as a result.  Only the last 8 words
 * are writeable through this interface, but all are readable.
 *
 * For the "clear" command, the ioctl arg is ignored.
 */
#define USRAID_GET_NVRAM	USRAID_IOC_(0x10)
#define USRAID_SET_NVRAM	USRAID_IOC_(0x11)
#define USRAID_CLEAR_NVRAM	USRAID_IOC_(0x12)
struct usraid_nvram_data {
	int		addr;	/* IN: NVRAM word address */
	unsigned int	data;	/* IN/OUT: data to write/read */
};

/*
 * Word offsets into the NVRAM of relevant information.
 * SGI has the final 8 words (out of 64) reserved for our use.
 */
#define USRAID_NVRAM_INITTED	0	/* bit 0, RAID initialized */
#define USRAID_NVRAM_FIRSTSPARE	56	/* first SGI spare byte in NVRAM */
#define USRAID_NVRAM_FLAGBITS	59	/* 4byte: flag bits */
#define   USRAID_NVRAM_WRTPROT	0x0001	/* bit: RAID is marked write-protected */
#define USRAID_NVRAM_TIME_SEC	61	/* 4byte: time of format (sec) */
#define USRAID_NVRAM_TIME_USEC	62	/* 4byte: time of format (usec) */
#define USRAID_NVRAM_CONFIG_ID	63	/* 4byte: config identifier */


/*============================================================================
 * UltraStor U144F Specific Sense/Mode Pages
 *============================================================================*/


/*
 * Miscellaneous Control Page
 *
 * This mode page determines the controller's performance characteristics
 * while a maintenance operation is in progress.
 *
 * utilization - shows the maximum percentage utilization of the
 *     controller's bandwidth to be used by maintenance commands.
 *     Possible values range from 1 thru 100 (0x64).  A value of 0
 *     is special and may (in the future) refer to a special mode.
 *
 * The ioctl arg should be a pointer to the following structure.
 * The fields can be set or read via this structure.
 */
#define USRAID_SET_MISC_PG	USRAID_IOC_(0x13)
#define USRAID_GET_MISC_PG	USRAID_IOC_(0x14)
struct usraid_misc_page {
	int	utilization;	/* % ctlr usage for maint cmds */
};


/*
 * Drive Group Definition Control Page
 *
 * This mode page defines the I/O optimization characteristics of the
 * RAID.  It controls the striping parameters and RAID4 versus RAID5 mode.
 *
 * RAID5 is optimized for many concurrent threads of I/O, while RAID4 is
 * (very much like RAID3) optimized for a small number of very high
 * bandwidth I/O threads.
 *
 *   stripe depth - defines the number of logically contigous blocks on
 *      a given drive before the next logically contiguous block is on
 *      the next drive.  In other words, it is 25% of the total stripe size.
 *	This field is limited to between 1 block and 128 blocks, inclusive.
 *
 *   num_stripes - the number of stripes of the indicated size in the whole
 *      RAID.  Range: 1 thru 0xffffffff.
 *
 *   mode - choose between RAID5 (mode == 0) and RAID4 (mode == 1).
 *
 * The ioctl arg should be a pointer to the following structure.
 * The fields can be set or read via this structure.
 */
#define USRAID_SET_DEFN_PG	USRAID_IOC_(0x15)
#define USRAID_GET_DEFN_PG	USRAID_IOC_(0x16)
struct usraid_defn_page {
	int		stripe_depth;	/* # sectors on 1 drive in a stripe */
	unsigned int	num_stripes;	/* # stripes in the RAID */
	int		mode;		/* 0==RAID5 mode, 1==RAID4 mode */
};


/*
 * Partition Definition Control Page
 *
 * This mode page defines a "partition".  Note that these are not the
 * same kinds of things as a UNIX partition.  These really controll how
 * much of the physical drives will be available as part of the RAID.
 * Sectors not included in the "partition" are NOT available for ANY use.
 * The same base sector number and number of sectors will be used for
 * all drives in the RAID.
 *
 * This gives the controller a range of sectors from each drive that
 * the "drive group definition" page will then use to define a RAID4
 * or RAID5 storage area.
 *
 * base - defines the first sector on each drive that belongs to the
 *     "partition".  Note that these "partitions" aren't the same as
 *     UNIX partitions.
 *
 * size - number of sectors on each drive that are part of the "partition".
 *
 * The ioctl arg should be a pointer to the following structure.
 * The fields can be set or read via this structure.
 */
#define USRAID_SET_PART_PG	USRAID_IOC_(0x17)
#define USRAID_GET_PART_PG	USRAID_IOC_(0x18)
struct usraid_part_page {
	unsigned int base;	/* base addr of user data on drive */
	unsigned int size;	/* number sectors of user data on each drive */
};
