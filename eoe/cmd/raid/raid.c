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

#ident "$Revision: 1.13 $"

/*
 * raid - UltraStor U144F RAID administrative utility
 */
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/fcntl.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <invent.h>
#include <sys/dkio.h>
#include <sys/dvh.h>
#include <sys/usraid_admin.h>

#define	RAID4		1	/* sector level striping, fixed parity disk */
#define RAID4_STRIPE	32	/* default RAID4 stripe depth in sectors */

#define	RAID5		2	/* sector level striping, floating parity disk */
#define RAID5_STRIPE	32	/* default RAID5 stripe depth in sectors */

#define	READ		1	/* transfer from RAID to program */
#define	WRITE		2	/* transfer from program to RAID */

#define	PART_BASE	32	/* reserve some sectors at the beginning  */

/*
 * This is the structure that is written to the reserved area on each
 * physical disk.  It is used to ensure that this disk came from this
 * RAID (random config ID, a timestamp, and the disk's slot number).
 */
#define RAID_CONFIG_REV		1		/* current rev number */
#define RAID_CONFIG_MAGIC	0xa102366a	/* magic number */
struct raid_config {
	int		version;	/* revision number: backward compat. */
	int		magic;		/* magic number: validity checking */
	unsigned int	config_id;	/* unique ID for this RAID and config */
	unsigned long	time_sec;	/* timestamp at config: seconds */
	unsigned long	time_usec;	/* timestamp at config: microseconds */
	int		slot_number;	/* disks only: which slot is it in */
	int		mode;		/* RAID4 or RAID5 mode */
	int		stripe_depth;	/* stripe depth in sectors */
	int		num_stripes;	/* number of stripes in logical disk */
	char		vendor[9];	/* vendor ID string */
	char		product[17];	/* product ID string */
	char		revision[5];	/* revision ID string */
	int		totalsize;	/* total number sectors on drive */
	int		blocksize;	/* sector size of drive */
};

/*
 * This is the structure we use to track all the related informtion
 * about a specific RAID.
 */
struct raid {
	struct raid		*next;		/* linked list */
	int			error;		/* entry has error, ignore it */
	char			pathname[256];	/* /dev/dsk pathname */
	char			basename[128];	/* base of pathname */
	struct usraid_defn_page	defn_page;	/* global RAID definitions */
	struct usraid_part_page	part_page;	/* per drive definitions */
	struct usraid_phys_conn	phys_info[5];	/* per drive physical info */
	struct usraid_units_down downs;		/* down drive info from ctlr */
	struct raid_config	config[6];	/* per drive config info */
	struct volume_header	vh;		/* volume header from device */
	int			vh_valid;	/* T/F: is volume header valid? */
	int			conf_valid[6];	/* T/F: is config[x] valid? */
	int			initted;	/* RAID has been initialized */
	int			wrtprot;	/* RAID has been write-protected */
	int			deadslot;	/* down disk, cntlr, or -1 */
	int			redo_ctlr;	/* setup to reprogram controller */
};

struct raid	*raidhead;		/* list of RAIDs to process */
struct raid	*list_raids(char *);	/* routine to generate that list */
int		inventory_raids();	/* routine to check HINV for RAIDs */
struct raid	*allocate_raid(char *);

int		raid_fd;		/* file desc. of open RAID device */
int		avoid_cm_message;	/* T/F: don't print "controller bad" msg */

unsigned int	gen_config_id();
char		*progname;		/* argv[0] for error messages */
int		syslogit;		/* message* will also use SYSLOG */

extern int	errno;			/* error result of system calls */

extern char	*partname(char *, int);
extern int	has_fs(char *, int);
extern char	*getmountpt(char *, char *);

usage()
{
	fprintf(stderr, "usage:\n");
	fprintf(stderr, "  %s [-L] -p [-v] [path]\t\t# print config for path or all\n", progname);
	fprintf(stderr, "  %s [-L] -c [-m] [-f] [path]\t\t# check for down drives for path or all\n", progname);
	fprintf(stderr, "  %s [-L] -i [path]\t\t\t# integrity check path or all\n", progname);
	fprintf(stderr, "  %s [-L] -d driveno [-f] path\t# mark down a drive on path (force it)\n", progname);
	fprintf(stderr, "  %s [-L] -r path\t\t\t# rebuild path\n", progname);
	fprintf(stderr, "  %s [-L] -{3|5} [-s stripe] [-S size] [-f] path\t# RAID3/RAID5 format\n", progname);
	fprintf(stderr, "  %s [-L] -z [-f] path\t\t# forcibly reset NVRAM to factory specs\n", progname);
	fprintf(stderr, "  %s [-L] -l firmware [-f] path\t# download new firmware to path\n", progname);
	exit(1);
}

main(argc, argv)
	int argc;
	char *argv[];
{
	int	type, typeset, stripe, stripeset, size, sizeset;
	int	force, mark, driveno, verbose;
	char	*path, *firmpath;
	int	ch, error = 0;
	char	*onlymsg = "only specify p, c, i, d, r, 3, 5, z, or l once";
	enum {
		NONE, PRINT, CHECKDOWN, INTEGRITY, FORCEDOWN,
		REBUILD, FORMAT, FACTORY, DOWNLOAD
	} request = NONE;

	/*
	 * pick up the command line arguments
	 */
#define ASSERT(COND, TEXT) \
	    if (!(COND)) { \
		fprintf(stderr, "%s: usage: %s\n", progname, (TEXT)); \
		error++; \
	    }

	type = typeset = stripe = stripeset = size = sizeset = 0;
	force = mark = driveno = verbose = 0;
	progname = argv[0];
	while ((ch = getopt(argc, argv, "pvcmid:fr35zl:s:S:L")) != EOF)
	{
		switch (ch) {
		case 'p':
			ASSERT(request == NONE, onlymsg);
			request = PRINT;
			break;
		case 'v':
			ASSERT(verbose == 0, "only specify verbose option once");
			verbose = 1;
			break;
		case 'c':
			ASSERT(request == NONE, onlymsg);
			request = CHECKDOWN;
			break;
		case 'm':
			ASSERT(mark == 0, "only specify marking down option once");
			mark = 1;
			break;
		case 'i':
			ASSERT(request == NONE, onlymsg);
			request = INTEGRITY;
			break;
		case 'd':
			ASSERT(request == NONE, onlymsg);
			request = FORCEDOWN;
			driveno = atoi(optarg);
			ASSERT(driveno >= 0, "drive number must be >= 0");
			ASSERT(driveno <= 4, "drive number must be <= 4");
			break;
		case 'f':
			ASSERT(!force, "only specify force option once");
			force++;
			break;
		case 'r':
			ASSERT(request == NONE, onlymsg);
			request = REBUILD;
			break;
		case '3':
			ASSERT(request == NONE, onlymsg);
			request = FORMAT;
			type = RAID4;
			typeset++;
			if (!stripeset)
				stripe = RAID4_STRIPE;
			break;
		case '5':
			ASSERT(request == NONE, onlymsg);
			request = FORMAT;
			type = RAID5;
			typeset++;
			if (!stripeset)
				stripe = RAID5_STRIPE;
			break;
		case 'z':
			ASSERT(request == NONE, onlymsg);
			request = FACTORY;
			break;
		case 'l':
			ASSERT(request == NONE, onlymsg);
			request = DOWNLOAD;
			firmpath = optarg;
			break;
		case 's':
			ASSERT(!stripeset, "only specify stripe depth once");
			stripe = atoi(optarg);
			ASSERT(stripe >= 4, "stripe size must be >= 4");
			ASSERT(stripe <= 64, "stripe size must be <= 64");
			ASSERT(((stripe % 4) == 0), "stripe depth must be a multiple of 4");
			stripeset++;
			break;
		case 'S':
			ASSERT(!sizeset, "only specify drive size once");
			size = atoi(optarg);
			ASSERT(size > 0, "disk size must be > 0");
			sizeset++;
			break;
		case 'L':
			ASSERT(!syslogit, "only specify SYSLOG option once");
			syslogit++;
			break;
		default:
			error++;
			break;
		}
	}
	if (error)
		usage();

	/*
	 * allowable argument structures -
	 *   raid [-L] -p [-v] [path]		# print config for path or all
	 *   raid [-L] -c [-m] [-f] [path]	# check for (and mark) down drives for path or all
	 *   raid [-L] -i [path]		# integrity check path or all
	 *   raid [-L] -d driveno [-f] path	# force down a drive on path
	 *   raid [-L] -r path			# rebuild path
	 *   raid [-L] -{3|5} [-s stripe] [-S size] [-f] path # RAID3/RAID5 format
	 *   raid [-L] -z [-f] path		# forcibly zero NVRAM (reset to factory specs)
	 *   raid [-L] -l firmware [-f] path	# download new firmware to path
	 */
	if (request != PRINT) {
		ASSERT(!verbose, "-v only valid when printing configurations");
	}
	if (request != CHECKDOWN) {
		ASSERT(!mark, "-m only valid when checking for down drives");
	}
	if ((request != FORCEDOWN) && (request != CHECKDOWN) &&
	    (request != FORMAT) && (request != FACTORY) && (request != DOWNLOAD)) {
		ASSERT(!force, "-f only valid with '-c', '-d', '-3', '-5', '-z', '-l' or options");
	}
	if (request != FORMAT) {
		ASSERT(!typeset, "-3 or -5 only valid when formatting");
		ASSERT(!stripeset, "-s only valid when formatting");
		ASSERT(!sizeset, "-S only valid when formatting");
	}

	switch(request) {
	case FORMAT:
		ASSERT(typeset, "must specify RAID type (eg: -3 or -5)");
		ASSERT(!sizeset || (size % stripe) == 0, "disk size must be a multiple of stripe depth");
		/* fallthru */
	case FORCEDOWN:
	case REBUILD:
	case FACTORY:
	case DOWNLOAD:
		if (optind == (argc-1)) {
			path = argv[optind];
		} else if (optind == argc) {
			ASSERT(0, "must specify RAID pathname");
		} else {
			ASSERT(0, "must specify at most one pathname");
		}
		if (geteuid() != 0) {
			fprintf(stderr, "%s: must have root permission to execute\n", argv[0]);
			exit(0);
		}
		break;

	case PRINT:
	case CHECKDOWN:
	case INTEGRITY:
		if (optind == (argc-1)) {
			path = argv[optind];
		} else {
			ASSERT(optind == argc, "must specify at most one pathname");
			path = NULL;
		}
		break;

	default:
		error++;
		break;
	}
	if (error)
		usage();
#undef ASSERT

	openlog("RAID", LOG_CONS, LOG_DAEMON);

	/*
	 * Go do the work
	 */
	switch(request) {
	case FORMAT:	error = format(path, stripe, type, size, force);	break;
	case FORCEDOWN:	error = forcedown(path, driveno, force);		break;
	case REBUILD:	error = rebuild(path);					break;
	case FACTORY:	error = factory(path, force);				break;
	case DOWNLOAD:	error = download(path, firmpath, force);		break;
	case PRINT:	error = print(path, verbose);				break;
	case CHECKDOWN:	error = checkdown(path, mark, force);			break;
	case INTEGRITY:	error = integrity(path);				break;
	}

	closelog();

	exit(error);
}



/*============================================================================
 * User requested operational routines
 */

/*
 * Format a new RAID
 *
 * The sequence of operations to set up a new RAID drive:
 *   + Execute the "return physical connection" command for each address
 *       + Force controller to look at and recognize each drive
 *       + Only look for the 5 known addresses
 *       + Save size reported for each drive
 *           + All sizes must match
 *   + Write page 0x38: Drive Group Definition
 *       + Define width 4 (ie: 4 data streams)
 *       + Define stripe block size as some multiple of 4
 *       + Define drive size as 4 * single disk size (as reported above)
 *       + Define parity and no mirroring
 *       + Define RAID4/5 parity diagram (map stream number to parity sector)
 *   + Write page 0x39: Partition Definition
 *       + Define both channel and target to be the slot number
 *           + Ascending order (slot number must map to stream number)
 *       + Define beginning sector to be 0
 *       + Define size to be single disk size (as reported above)
 *   + If !zero then execute the "initialize unit" command
 *       + Used to force the parity to match the data
 *       + Can be avoided in emergency situations
 *           + Reformat with same params, may recover more data that way
 *   + Write the config info to the reserved sector on each disk
 *       + Write to the reserved sector of each disk
 *   + Set the magic number information in the controller
 *       + Use the NVRAM write commands to set the reserved NVRAM bytes
 *       + Set NVRAM bit saying LUN 0 initialized
 *   + Execute a "mode sense" command on LUN 0
 *       + Just to test that all config has gone smoothly
 */
format(char *path, int stripe, int type, int size, int force)
{
	struct raid *raidp;
	struct usraid_phys_conn	*physp;
	struct usraid_units_down downs;
	struct usraid_nvram_data data;
	struct raid_config *cp;
	unsigned int config_id;
	struct timeval tv;
	int drivesz, error = 0, i;
	char buffer[32];

	raidp = allocate_raid(path);

	/*
	 * get a handle on the specified RAID
	 */
	if (error = raidopen(raidp->pathname)) {
		message_perror(raidp, "open of RAID failed");
		return(error);
	}

	/*
	 * Check for mounted filesystems.
	 */
	if (checkforfs(raidp, 1)) {
		error = EPERM;
		goto out;
	}

	/*
	 * poll each drive and check it's size
	 */
	for (i = 0; i < 5; i++) {
		raidp->phys_info[i].drive = i;
		if (ioctl(raid_fd, USRAID_PHYS_CONN, &raidp->phys_info[i]) < 0) {
			error = errno;
			message_perror(raidp, "Drive %d is missing, formatting failed", i);
			break;
		}
		if (i == 0) {
			drivesz = raidp->phys_info[i].totalsize;
		} else if (raidp->phys_info[i].totalsize != drivesz) {
			error = EINVAL;
			message(raidp, "Drive %d is not the same size (%d) as the other drives: %d sectors",
				       1, i, raidp->phys_info[i].totalsize, drivesz);
			break;
		}

		/*
		 * Ensure that this drive is supported in a RAID configuration.
		 */
		if ((strncmp(raidp->phys_info[i].vendor, "SGI     ", 8) != 0) ||
		    (strncmp(raidp->phys_info[i].product, "0664", 4) != 0)) {
			error = EINVAL;
			message(raidp, "Drive %d is not a supported drive type:",
				       1, i, 0);
			message(raidp, "vendor: %s, product: %s, revision: %s",
				       1, raidp->phys_info[i].vendor,
				       raidp->phys_info[i].product);
			break;
		}
	}
	if (error)
		goto out;

	/*
	 * Adjust the device sizes to reserve sectors for our use.
	 */
	for (i = 0; i < 5; i++) 
		raidp->phys_info[i].totalsize -= PART_BASE;
	drivesz -= PART_BASE;
	if ((size > 0) && (drivesz > size))
		drivesz = size;

	/*
	 * Ask for confirmation from the user.
	 */
	if (!confirm(raidp, force, "format", 0)) {
		error = EPERM;
		goto out;
	}

	/*
	 * update the NVRAM to show the drive has been NOT been initialized
	 */
	raidp->initted = 0;
	data.addr = USRAID_NVRAM_INITTED;
	if (ioctl(raid_fd, USRAID_GET_NVRAM, &data) < 0) {
		error = errno;
		message_perror(raidp, "Read of NVRAM init flag failed");
	}
	data.data &= ~0x01;
	if (ioctl(raid_fd, USRAID_SET_NVRAM, &data) < 0) {
		error = errno;
		message_perror(raidp, "Write of NVRAM init flag failed");
	}

	/*
	 * Clear indication that any drives are down and
	 * reset the write-protect bit.
	 */
	for (i = 0; i < 5; i++)
		downs.drive[i] = 1;
	if (ioctl(raid_fd, USRAID_CLR_DOWN, &downs) < 0) {
		error = errno;
		message_perror(raidp, "Clearing down disk info failed");
		goto out;
	}
	raidp->wrtprot = 0;

	/*
	 * ensure that drive size is a multiple of the stripe width
	 */
	if ((drivesz % stripe) != 0)
		drivesz -= (drivesz % stripe);

	/*
	 * generate the appropriate configuration information
	 */
	if (gettimeofday(&tv, NULL) < 0) {
		error = errno;
		message_perror(raidp, "gettimeofday() failed");
		goto out;
	}
	config_id = gen_config_id();
	for (i = 0; i < 6; i++) { 	/* note: 6 entries, last for cntlr */
		cp = &raidp->config[i];
		cp->version		= RAID_CONFIG_REV;
		cp->magic		= RAID_CONFIG_MAGIC;
		cp->config_id		= config_id;
		cp->time_sec		= tv.tv_sec;
		cp->time_usec		= tv.tv_usec;
		cp->slot_number		= i;
		cp->mode		= type;
		cp->stripe_depth	= stripe;
		cp->num_stripes		= drivesz/stripe;
		if (i < 5) {
			physp = &raidp->phys_info[i];
			blankblaster(physp->vendor);
			blankblaster(physp->product);
			blankblaster(physp->revision);
			strcpy(cp->vendor, physp->vendor);
			strcpy(cp->product, physp->product);
			strcpy(cp->revision, physp->revision);
			cp->totalsize	= physp->totalsize;
			cp->blocksize	= physp->blocksize;
		}
		raidp->conf_valid[i]		= 1;
	}
	raidp->initted = 0;
	raidp->deadslot = -1;

	/*
	 * Set up the mode pages on each disk.
	 */
	for (i = 0; i < 5; i++) {
		if (error = program_disk(raidp, i, (i == 0))) {
			goto out;
		}
	}

	/*
	 * program the RAID controller
	 */
	if (error = write_controller(raidp))
		goto out;

	/*
	 * Initialize all sectors on the RAID
	 */
	if (ioctl(raid_fd, USRAID_INITDRIVE, 0) < 0) {
		error = errno;
		message_perror(raidp, "RAID initialization failed");
		goto out;
	}

	/*
	 * write the config info to all 5 disks.
	 *
	 * Note that the reserved sectors are the last 128 sectors
	 * on that drive (ie: relative to the physical disk size)
	 * of which SGI has control of the first 8.
	 */
	for (i = 0; i < 5; i++) {
		if (error = rw_config(raidp, i, WRITE)) {
			goto out;
		}
	}

	/*
	 * update the NVRAM to show the drive has been initialized
	 */
	raidp->initted = 1;
	data.addr = USRAID_NVRAM_INITTED;
	if (ioctl(raid_fd, USRAID_GET_NVRAM, &data) < 0) {
		error = errno;
		message_perror(raidp, "Read of NVRAM init flag failed");
	}
	data.data |= 0x01;
	if (ioctl(raid_fd, USRAID_SET_NVRAM, &data) < 0) {
		error = errno;
		message_perror(raidp, "Write of NVRAM init flag failed");
	}

	/*
	 * execute an "inquiry" on the drive, just to ensure that no
	 * errors come back.
	 */
	if (ioctl(raid_fd, DIOCDRIVETYPE, buffer) < 0) {
		error = errno;
		message_perror(raidp, "inquiry comand on RAID failed");
	}

out:
	raidclose();
	if (!error) {
		i = syslogit;
		syslogit = 1;
		message(raidp, "Successfully formatted for %d blocks, stripe size: %d",
			       2, drivesz*4, stripe*4);
		syslogit = i;
	}
	return(error);
}

/*
 * Set a RAID to factory new condition, irregardless of attached disks, etc.
 *
 * The sequence of operations to factory reset a RAID drive:
 *   + Use "clear NVRAM" command, then reset controller.
 *       + Firmware will then completely re-initialize NVRAM contents.
 */
factory(char *path, int force)
{
	struct raid *raidp;
	char buffer[32];
	int error = 0, i;

	raidp = allocate_raid(path);

	/*
	 * get a handle on the specified RAID
	 */
	if (error = raidopen(raidp->pathname)) {
		message_perror(raidp, "open of RAID failed");
		goto out;
	}

	/*
	 * Ask for confirmation from the user.
	 */
	if (checkforfs(raidp, 1) || !confirm(raidp, force, "factory zero", 0)) {
		error = EPERM;
		goto out;
	}

	/*
	 * forces controller to completely re-initialize NVRAM.
	 */
	if (ioctl(raid_fd, USRAID_CLEAR_NVRAM, 0) < 0) {
		error = errno;
		message_perror(raidp, "Clear NVRAM ioctl failed");
		goto out;
	}

	/*
	 * execute an "inquiry" on the drive, just to ensure that no
	 * errors come back.
	 */
	if (ioctl(raid_fd, DIOCDRIVETYPE, buffer) < 0) {
		error = errno;
		message_perror(raidp, "inquiry command on RAID failed");
	}

	i = syslogit;
	syslogit = 1;
	message(raidp, "Press the RESET button on the RAID to complete the zeroing", 2);
	syslogit = i;

out:
	raidclose();
	return(errno);
}

/*
 * Rebuild a drive on a RAID
 *
 * The sequence of operations to rebuild a RAID after a drive failure:
 *   + User should replace the disk hardware
 *   + Execute the "return units down" command (indirectly thru list_raids)
 *       + Verify that there is exactly one disk marked as down
 *   + Execute the "return physical connection" command
 *       + Forces the controller to look at the new drive
 *   + Check disk size, etc.
 *       + Refuse to rebuild if different
 *       + Spin drive down again
 *   + Low level format of the new drive (optional)
 *       + Done via pass-thru commands
 *       + Also surface analysis if requested
 *   + Execute the "rebuild" command
 *       + Wait for it to complete, will take awhile
 *   + Write the reserved area on the new disk
 *       + Write to the reserved sector of each disk
 *   + Clear the "down" bit
 */
rebuild(char *path)
{
	struct raid *raidp;
	struct usraid_phys_conn	*physp;
	struct usraid_rebuild rebuild_data;
	struct usraid_nvram_data data;
	struct raid_config *cp;
	unsigned int config_id;
	struct timeval tv;
	int drivesz, error = 0, i;

	raidp = list_raids(path);
	if (raidp == NULL)
		return(ENOENT);
	if (raidp->error) {
		message(raidp, "Can't rebuild this RAID", 0);
		return(raidp->error);
	}
	if (raidp->wrtprot) {
		message(raidp, "Can't rebuild a RAID that has had a write failure, it must be reformatted.", 0);
		return(EIO);
	}

	/*
	 * confirm that a single drive has been marked as down
	 */
	if (raidp->deadslot < 0) {
		message(raidp, "All disks are OK in this RAID", 0);
		return(0);
	}

	if (error = raidopen(raidp->pathname)) {
		message_perror(raidp, "open of RAID failed");
		return(error);
	}

	/*
	 * poll each drive and check it's size
	 */
	for (i = 0; i < 5; i++) {
		physp = &raidp->phys_info[i];
		physp->drive = i;
		if (ioctl(raid_fd, USRAID_PHYS_CONN, physp) < 0) {
			error = errno;
			message_perror(raidp, "Drive %d is missing, rebuild failed", i);
			break;
		}
		physp->totalsize -= PART_BASE;

		if (i == 0) {
			drivesz = physp->totalsize;
		} else if (physp->totalsize != drivesz) {
			error = ENOSPC;
			message(raidp, "Drive %d is not the same size as the other drives: %d sectors",
				       1, i, drivesz);
			break;
		}

		/*
		 * Ensure that this drive is supported in a RAID configuration.
		 */
		if ((strncmp(physp->vendor, "SGI     ", 8) != 0) ||
		    (strncmp(physp->product, "0664", 4) != 0)) {
			error = EINVAL;
			message(raidp, "Drive %d is not a supported drive type:",
				       1, i, 0);
			message(raidp, "vendor: %s, product: %s, revision: %s",
				       1, physp->vendor, physp->product);
			break;
		}
	}
	if (error)
		goto out;

	/*
	 * Set up the mode pages on the rebuilt disk.
	 * If rebuilding drive 0, turn on spindle sync on alternate master
	 * (drive 1).
	 */
	if (raidp->deadslot == 0) {
		if (error = program_disk(raidp, 0, 1)) {
			message(raidp, "Failed to program the mode pages on disk %d:", 1, 0);
			goto out;
		}
		if (error = program_disk(raidp, 1, 0)) {
			message(raidp, "Failed to program the mode pages on disk %d:", 1, 1);
			goto out;
		}
	} else if (error = program_disk(raidp, raidp->deadslot, 0)) {
		message(raidp, "Failed to program the mode pages on disk %d:", 1, raidp->deadslot);
		goto out;
	}

	/*
	 * rebuild the new drive from the existing data/parity info
	 */
	rebuild_data.drive = raidp->deadslot;
	rebuild_data.lba = 0;
	if (ioctl(raid_fd, USRAID_REBUILD, &rebuild_data) < 0) {
		error = errno;
		i = syslogit;
		syslogit = 1;
		message_perror(raidp, "Rebuild failed");
		syslogit = i;
		goto out;
	}

	/*
	 * generate a copy of the config structure for the new drive,
	 * and write it to the reserved area of that disk.
	 *
	 * this is done now so that if power fails while we are doing
	 * the next section, we have a bit more of a chance of seeing
	 * all the disks match.  If we didn't do this, we are almost
	 * guaranteeing that we will think we have a dual failure.
	 */
	cp = &raidp->config[raidp->deadslot];
	physp = &raidp->phys_info[raidp->deadslot];
	*cp = raidp->config[5];	/* struct copy */
	cp->slot_number = raidp->deadslot;
	blankblaster(physp->vendor);
	blankblaster(physp->product);
	blankblaster(physp->revision);
	strcpy(cp->vendor, physp->vendor);
	strcpy(cp->product, physp->product);
	strcpy(cp->revision, physp->revision);
	cp->totalsize = physp->totalsize;
	cp->blocksize = physp->blocksize;

	if (error = rw_config(raidp, raidp->deadslot, WRITE))
		goto out;
	raidp->conf_valid[raidp->deadslot] = 1;

	/*
	 * Clear the indication that the drive is down.
	 */
	bzero((char *)&raidp->downs, sizeof(raidp->downs));
	raidp->downs.drive[raidp->deadslot] = 1;
	if (ioctl(raid_fd, USRAID_CLR_DOWN, &raidp->downs) < 0) {
		error = errno;
		message_perror(raidp, "Clearing down disk info failed");
		goto out;
	}

	/*
	 * generate new config info so that our config won't match the disk
	 * that was just replaced.  If we didn't do this, then pulling
	 * drive A, rebuilding with drive B, and swapping A for B would
	 * get us bad data.  By changing the config, drive A will no
	 * longer match, and will then be recognized as foreign.
	 *
	 * note that this opens a hole where if power failed, we would not
	 * have consistent config info on the disks.  The format "nozero"
	 * option is our way out of this hole.
	 */
	if (gettimeofday(&tv, NULL) < 0) {
		error = errno;
		message_perror(raidp, "gettimeofday() failed");
		goto out;
	}
	config_id = gen_config_id();
	for (i = 0; i < 6; i++) {
		cp = &raidp->config[i];
		cp->config_id	= config_id;
		cp->time_sec	= tv.tv_sec;
		cp->time_usec	= tv.tv_usec;
		if (i < 5) {
			if (error = rw_config(raidp, i, WRITE)) {
				/* XXXGROT: if this ever fails, the RAID is toast */
				goto out;
			}
		}
	}

	/*
	 * write the config ID
	 */
	data.addr = USRAID_NVRAM_CONFIG_ID;
	data.data = cp->config_id;
	if (ioctl(raid_fd, USRAID_SET_NVRAM, &data) < 0) {
		error = errno;
		message_perror(raidp, "Write of NVRAM config-id failed");
	}

	/*
	 * write the timestamp
	 */
	data.addr = USRAID_NVRAM_TIME_SEC;
	data.data = cp->time_sec;
	if (ioctl(raid_fd, USRAID_SET_NVRAM, &data) < 0) {
		error = errno;
		message_perror(raidp, "Write of NVRAM timestamp failed");
	}
	data.addr = USRAID_NVRAM_TIME_USEC;
	data.data = cp->time_usec;
	if (ioctl(raid_fd, USRAID_SET_NVRAM, &data) < 0) {
		error = errno;
		message_perror(raidp, "Write of NVRAM timestamp failed");
	}

	/*
	 * Use the "return units down" command to verify that the
	 * rebuild was successful
	 */
	if (ioctl(raid_fd, USRAID_RET_DOWN, &raidp->downs) < 0) {
		error = errno;
		message_perror(raidp, "Re-reading down disk info failed");
		goto out;
	}
	for (i = 0; i < 5; i++) {
		if (raidp->downs.drive[i]) {
			error = EAGAIN;
			message(raidp, "Controller still shows disk %d as down", 1, i);
			goto out;
		}
	}

out:
	raidclose();
	if (!error) {
		i = syslogit;
		syslogit = 1;
		message(raidp, "Disk %d successfully rebuilt", 2, raidp->deadslot);
		syslogit = i;
	}
	return(error);
}

/*
 * Download new firmware to a RAID
 *
 * The sequence of operations to download new firmware to a RAID:
 */
download(char *path, char *firmpath, int force)
{
	struct usraid_download download;
	struct raid *raidp;
	struct stat statb;
	int firm_fd, error = 0, i;
	char *buffer;

	/*
	 * Get a copy of the firmware into a memory buffer.
	 */
	if (stat(firmpath, &statb) < 0) { 
		error = errno;
		message_perror(NULL, "stat failed: %s", firmpath);
		return(error);
	}
	if ((buffer = (char *)malloc((unsigned)statb.st_size)) == NULL) {
		error = errno;
		message_perror(NULL, "malloc failed");
		return(error);
	}
	if ((firm_fd = open(firmpath, O_RDONLY)) < 0) {
		error = errno;
		message_perror(NULL, "firmware open failed: %s", firmpath);
		return(error);
	}
	if (read(firm_fd, buffer, statb.st_size) < statb.st_size) {
		error = errno;
		message_perror(NULL, "firmware read failed: %s", firmpath);
		close(firm_fd);
		return(error);
	}
	if (close(firm_fd) < 0) {
		error = errno;
		message_perror(NULL, "firmware close failed: %s", firmpath);
		return(error);
	}

	/*
	 * Check out the controller
	 */
	raidp = list_raids(path);
	if (raidp == NULL)
		return(ENOENT);

	/*
	 * Ask for confirmation from the user.
	 */
	(void)checkforfs(raidp, 0);
	if (!confirm(raidp, force, "download new firmware to", 0))
		return(EPERM);

	/*
	 * Download the firmware into the controller.
	 */
	if (error = raidopen(raidp->pathname)) {
		message_perror(raidp, "open of RAID failed");
		return(error);
	}
	download.buffer = buffer;
	download.size = statb.st_size;
	if (ioctl(raid_fd, USRAID_DOWNLOAD, (caddr_t)&download)) {
		error = errno;
		message(raidp, "Downloading new firmware failed", 1);
	}
	raidclose();

	i = syslogit;
	syslogit = 1;
	message(raidp, "Press the RESET button on the RAID to have the new firmware take effect", 2);
	syslogit = i;
	return(error);
}

/*
 * Check the integrity of the parity on a RAID
 *
 * The sequence of operations to check the integrity of a RAID:
 *   + Can be run while RAID is active
 *   + Execute the "integrity check" command on the indicated RAID
 *       + Will run until an error, or the end of the RAID
 *   + Execute the "fix integrity" command on any error return
 *       + Will regenerate the parity sector(s) for the indicated stripe
 */
integrity(char *path)
{
	struct raid *raidp;
	int error = 0, i;

	raidp = list_raids(path);
	if (raidp == NULL)
		return(ENOENT);
	for ( ; raidp; raidp = raidp->next) {
		if (raidp->error) {
			error = raidp->error;
			continue;
		}

		/*
		 * refuse to check integrity of a RAID that has a drive down
		 */
		if (raidp->deadslot >= 0) {
			message(raidp, "Cannot check integrity, drive %d is down",
				       0, raidp->deadslot);
			continue;
		}

		/*
		 * loop thru drive checking and correcting parity as required
		 */
		if (error = raidopen(raidp->pathname)) {
			message_perror(raidp, "open of RAID failed");
			continue;
		}
		if (ioctl(raid_fd, USRAID_INTEGRITY, 0) < 0) {
			error = errno;
			i = syslogit;
			syslogit = 1;
			message_perror(raidp, "Integrity check failed");
			syslogit = i;
		} else {
			message(raidp, "Integrity check successfull", 2);
		}
		raidclose();
	}
	return(error);
}

/*
 * Force down a specific drive on a specific RAID.
 *
 * The sequence of operations to force down a drive:
 *   + Check for any down drives on the raid.
 *       + Confirmation req'd if we are forcing down a drive and
 *         one is already down.
 *   + If a RAID is corrupted (ie: marked with an error), don't do anything.
 */
forcedown(char *path, int driveno, int force)
{
	struct usraid_units_down downs;
	struct raid *raidp;
	int error = 0, doit = 0, i;
	char buffer[32];

	raidp = list_raids(path);
	if (raidp == NULL)
		return(ENOENT);
	if (raidp->error) {
		message(raidp, "Cannot force a drive down on this RAID", 0);
		return(raidp->error);
	}

	/*
	 * If the odd-man-out is the controller, reprogram it
	 * with the info from the disks
	 */
	if (raidp->deadslot == 5) {
		error = EIO;
	} else if ((raidp->deadslot >= 0) &&
		   (raidp->downs.drive[raidp->deadslot] == 0)) {
		/*
		 * One drive's config info doesn't match the others,
		 * but it is claimed to be up by the controller.
		 */
		if ((driveno == raidp->deadslot) || force) {
			doit++;
		} else {
			error = EIO;
			message(raidp, "Aborting: this would down a second disk in the array", 1);
		}
	} else if (raidp->deadslot >= 0) {
		if (driveno == raidp->deadslot) {
			error = EIO;
			message(raidp, "Drive %d is already marked down", 0, raidp->deadslot);
		} else if (force) {
			doit++;
		} else {
			error = EIO;
			message(raidp, "Aborting: this would down a second disk in the array", 1);
		}
	} else {
		doit++;
	}

	if (doit) {
		if (error = raidopen(raidp->pathname)) {
			message_perror(raidp, "open of RAID failed");
		} else {
			/*
			 * Ask for confirmation from the user.
			 */
			if (!confirm(raidp, force, "force down drive %d on", driveno)) {
				return(EPERM);
			} else {
				/*
				 * Always SYSLOG the message that we are forcing down a drive.
				 */
				doit = syslogit;
				syslogit = 1;
				message(raidp, "Disk %d is being forced down", 0, driveno);
				bzero(&downs, sizeof(downs));
				downs.drive[driveno] = 1;
				if (ioctl(raid_fd, USRAID_SET_DOWN, &downs) < 0) {
					error = errno;
					message_perror(raidp, "Failed to force disk %d down", driveno);
				}
				syslogit = doit;

				/*
				 * If we are forcing down drive 0, we must reset the master
				 * for the spindle synch line.
				 */
				if ((driveno == 0) && (error = program_disk(raidp, 1, 1))) {
					message(raidp, "Failed to program drive 1 to master the spindle synch line:", 1);
				}
			}
			raidclose();
		}
	}
	return(error);
}

/*
 * Check for down drives on any RAID on the system.
 *
 * The sequence of operations to check for down drives:
 *   + Execute the "return units down" command for each RAID
 *   + If the RAID has been marked with an error (ie: its corrupted)
 *       + Spin down the whole RAID.
 *   + If a drive's config info doesn't match, but it is marked "up":
 *       + Force that drive down.
 */
checkdown(char *path, int mark, int force)
{
	struct usraid_units_down downs;
	struct raid *raidp;
	int error = 0, i;

	avoid_cm_message = mark;
	raidp = list_raids(path);
	avoid_cm_message = 0;
	if (raidp == NULL)
		return(ENOENT);
	for ( ; raidp; raidp = raidp->next) {
		if (raidp->error && !raidp->redo_ctlr) {
			error = raidp->error;
			continue;
		}

		/*
		 * If the odd-man-out is the controller, reprogram it
		 * with the info from the disks
		 */
		if ((raidp->deadslot == 5) || (raidp->redo_ctlr)) {
			error = EIO;
			if (mark) {
				if (error = raidopen(raidp->pathname)) {
					error = errno;
					message_perror(raidp, "open of RAID failed");
				} else if (!confirm(raidp, force, "reprogram the controller for", 0)) {
					error = EPERM;
				} else {
					i = syslogit;
					syslogit = 1;
					message(raidp, "Reprogramming controller to match disks", 0);
					/* 
					 * GROT: if this is the 2nd controller of a dual set, we won't
					 * get the down drive info from the primary
					 */
					raidp->config[5] = raidp->config[0];	/* struct copy */
					raidp->config[5].slot_number = 5;
					raidp->initted = 1;
					error = write_controller(raidp);
					syslogit = i;
					raidclose();
				}
			}
		} else if ((raidp->deadslot >= 0) &&
			   (raidp->downs.drive[raidp->deadslot] == 0)) {
			/*
			 * One drive's config info doesn't match the others,
			 * but it is claimed to be up by the controller.
			 */
			error = EIO;
			if (mark) {
				if (error = raidopen(raidp->pathname)) {
					error = errno;
					message_perror(raidp, "open of RAID failed");
				} else if (!confirm(raidp, force, "force down drive %d on",
							   raidp->deadslot)) {
					error = EPERM;
				} else {
					i = syslogit;
					syslogit = 1;
					message(raidp, "Disk %d is being forced down,",
						       0, raidp->deadslot);
					message(raidp, "the RAID needs to be rebuilt", 0);
					bzero(&downs, sizeof(downs));
					downs.drive[raidp->deadslot] = 1;
					if (ioctl(raid_fd, USRAID_SET_DOWN, &downs) < 0) {
						error = errno;
						message_perror(raidp, "Failed to force disk %d down",
								      raidp->deadslot);
					}
					syslogit = i;

					/*
					 * If we are forcing down drive 0, we must reset the master
					 * for the spindle synch line.
					 */
					if ((raidp->deadslot == 0) && 
					    (error = program_disk(raidp, 1, 1))) {
						message(raidp, "Failed to program drive 1 to master the spindle synch line:", 1);
					}
					raidclose();
				}
			}
		} else if (raidp->deadslot >= 0) {
			error = EIO;
			message(raidp, "Drive %d is down", 0, raidp->deadslot);

			if (error = raidopen(raidp->pathname)) {
				error = errno;
				message_perror(raidp, "open of RAID failed");
			} else {
				/*
				 * If we are forcing down drive 0, we must reset the master
				 * for the spindle synch line.
				 */
				if ((raidp->deadslot == 0) &&
				    (error = program_disk(raidp, 1, 1))) {
					message(raidp, "Failed to program drive 1 to master the spindle synch line:", 1);
				}
				raidclose();
			}
		}
	}
	return(error);
}

/*
 * Print configuration information for the indicated RAID,
 * or for all of them if a specific RAID is not given
 *
 * The sequence of operations to print configuration info:
 *   + Print: stripe block size, drive size, and parity mode (4/5)
 *   + Print: size, vendor, and revision info on each physical drive
 *   + Print: any down disks
 *   + Print: the LUN 0 initialized bit from the NVRAM
 */
print(char *path, int verbose)
{
	struct raid_config *cp;
	struct raid *raidp;
	int error = 0, i, msgs = 0;
	char buffer[256];

	raidp = list_raids(path);
	if (raidp == NULL)
		return(ENOENT);
	for ( ; raidp; raidp = raidp->next) {
		/*
		 * print the RAID's general parameters
		 */
		sprintf(buffer, "%s, sectors: %d, stripe size: %d",
				(raidp->defn_page.mode == 0) ? "RAID5" : "RAID3",
				raidp->defn_page.num_stripes * raidp->defn_page.stripe_depth * 4,
				raidp->defn_page.stripe_depth * 4);

		/*
		 * print a message for various error conditions
		 */
		if ((raidp->deadslot >= 0) && (raidp->deadslot <= 4)) {
			sprintf(buffer, "%s, drive %d down", buffer, raidp->deadslot);
			msgs++;
		}
		if (raidp->deadslot == 5) {
			sprintf(buffer, "%s, ctlr replaced", buffer);
			msgs++;
		}
		if (!raidp->initted) {
			sprintf(buffer, "%s, not initialized", buffer);
			msgs++;
		}
		if (raidp->error) {
			sprintf(buffer, "%s, must reformat", buffer);
			msgs++;
			error = raidp->error;
		}
		if (raidp->wrtprot) {
			sprintf(buffer, "%s, write protected", buffer);
			msgs++;
		}
		if (!msgs)
			sprintf(buffer, "%s, OK", buffer);
		message(raidp, buffer, 2);

		/*
		 * If we are using the hidden "verbose" option, print
		 * all the info about the disks in the RAID.
		 */
		if (verbose) {
			cp = &raidp->config[5];
			message(raidp, "  Controller: \"%s\", \"%s\", \"%s\"", 2,
				       cp->vendor, cp->product, cp->revision);
			for (i = 0; i < 5; i++) {
				if (i == raidp->deadslot) {
					message(raidp, "  [%d]: *** drive is down ***", 2, i);
				} else {
					cp = &raidp->config[i];
					sprintf(buffer, "  [%d]: %d blocks of %d bytes, \"%s\", \"%s\", \"%s\"",
						       i, cp->totalsize, cp->blocksize,
						       cp->vendor, cp->product, cp->revision);
					message(raidp, buffer, 2);
				}
			}
		}
	}
	return(error);
}


/*============================================================================
 * RAID management/checking functions
 */

/*
 * Collect and validate config information for the indicated RAID,
 * or for all of them if a specific RAID is not given
 *
 * The sequence of operations is:
 *   + If a path is specified:
 *       + Get info on just the indicated RAID
 *   + If a path is not specified:
 *       + Use getinvent() to look for RAID controllers
 *           + They are SCSI disks, but with more info?
 *   + In either case, for each selected RAID, validate all info
 */
struct raid *
list_raids(char *path)
{
	struct raid *raidp = NULL;
	char *ch;

	if (path) {
		raidp = allocate_raid(path);
	} else {
		if (scaninvent(inventory_raids, 0) < 0) {
			message_perror(NULL, "scaninventory() failed");
			exit(1);
		}
		raidp = raidhead;
	}

	if (raidp) {
		read_raids(raidp);
		check_raids(raidp);
	}
	return(raidp);
}

/*
 * Collect but don't validate config information for the indicated RAID,
 * or for all of them if a specific RAID is not given
 *
 * The sequence of operations to collect configuration info:
 *   + If a path is specified:
 *       + Get info on just the indicated RAID
 *   + If a path is not specified:
 *       + Use getinvent() to look for RAID controllers
 *           + They are SCSI disks, but with more info?
 *   + In either case, for each selected RAID, collect the following:
 *       + Read page 0x38: Drive Group Definition
 *           + Read: depth, stripe block size, drive size, and parity mode
 *       + Execute the "return physical connection" command for each address
 *           + Check info on each physical drive
 *           + Only look for the 5 known addresses
 *       + Read page 0x39: Partition Definition
 *           + Read: target and channel number, beginning sector, and length
 *       + Execute the "return units down" command for each RAID
 *       + Read the magic number information in the controller
 *       + Read the config info from the reserved sectors on each disk
 */
read_raids(struct raid *raidp)
{
	int i, j;

	for (  ; raidp; raidp = raidp->next) {

		if (raidp->error = raidopen(raidp->pathname)) {
			message_perror(raidp, "open of RAID failed");
			continue;
		}

		/*
		 * read the RAID's general parameters
		 */
		raidp->error = read_controller(raidp);

		/*
		 * check for down drives in the RAID
		 */
		raidp->deadslot = -1;
		if (ioctl(raid_fd, USRAID_RET_DOWN, &raidp->downs) < 0) {
			raidp->error = errno;
			message_perror(raidp, "Reading Units Down from controller failed");
			raidp->conf_valid[5] = 0;	/* now ignore controller info */
		} else {
			for (i = 0; i < 5; i++) {
				if (raidp->downs.drive[i]) {
					if (raidp->deadslot >= 0) {
						message(raidp, "More than one disk is marked down", 1);
						raidp->error = EIO;
						goto nextone;
					}
					raidp->deadslot = i;
				}
			}
		}

#ifdef GROT
/*
 * The IBM 2.0GB drives take their sweet time about responding to an
 * inquiry message, they seem to be doing diagnostics and such.  If
 * we did the PHYS_CONN here, we would spend 5 seconds on each RAID,
 * ie: 5 minutes on a farm of 60 RAIDs.
 */
		/*
		 * read info for each physical drive (even down drives)
		 */
		for (i = 0; i < 5; i++) {
			raidp->phys_info[i].drive = i;
			if (ioctl(raid_fd, USRAID_PHYS_CONN, &raidp->phys_info[i]) < 0) {
				message_perror(raidp, "Drive %d is missing", i);
			}
			raidp->phys_info[i].totalsize -= PART_BASE;
		}
#endif /* GROT */

		/*
		 * read the config info from the reserved sector on all disks
		 *
		 * Note that the reserved sectors are the last 128 sectors
		 * on that drive (ie: relative to the physical disk size),
		 * of which SGI has control of the first 8.
		 */
		for (i = 0; i < 5; i++) {
			if (rw_config(raidp, i, READ))
				raidp->conf_valid[i] = 0;
			else
				raidp->conf_valid[i] = 1;
		}

		/*
		 * Read up any volume header that may be present on the disk.
		 */
		if (getdiskheader(raid_fd, &raidp->vh) == -1) {
			raidp->vh_valid = 0;
		} else {
			raidp->vh_valid = 1;
		}
nextone:
		raidclose();
	}
}

/*
 * Validate config information for the given list of RAIDs.
 *
 * The sequence of operations to validate the configuration for each RAID:
 *   + If one disk is out of date WRT to the other disks and controller
 *       + Range check all info on "good" disks and controller
 *       + Dont do anything, wait for rebuild() to fix it
 *   + If the controller is out of date WRT to all disks
 *       + Reprogram the controller with data from the disks
 *   + If more than one odd-man-out in disks and controller
 *       + RAID is trashed, must format
 */
check_raids(struct raid *raidp)
{
	struct raid_config *cp, *tcp;
	struct usraid_defn_page *defp;
	struct usraid_phys_conn *physp;
	int error, i;

	for (  ; raidp; raidp = raidp->next) {
		/*
		 * compare config_id's, timestamps, and magic numbers
		 * this will tell us which (if any) info source is the
		 * odd-man-out.  After this, deadslot is known accurate.
		 */
		if (find_oddman(raidp) == -2) {
			raidp->error = EIO;
			message(raidp, "Raid corrupted, must reformat", 1);
			continue;
		}

		/*
		 * check the config info from reserved sector on each "up" disk
		 *
		 * Note that the reserved sectors are the first 32 physical sectors
		 * on that drive (ie: relative to the physical disk size, not the
		 * partitions we've defined).
		 */
		error = 0;
		for (i = 0; i < 6; i++) {
			if (i == raidp->deadslot)
				continue;
			cp = &raidp->config[i];
			defp = &raidp->defn_page;

			if ((cp->magic != RAID_CONFIG_MAGIC) ||
			    (cp->version != RAID_CONFIG_REV)) {
				message(raidp, "Drive %d's config info is unrecognizable", 1, i);
				error++;
				continue;
			}
			if ((cp->config_id == 0) ||
			    ((cp->mode != RAID4) && (cp->mode != RAID5)) ||
			    (cp->stripe_depth < 1) || (cp->num_stripes < 1)) {
				message(raidp, "Drive %d's config info contains illegal values", 1, i);
				error++;
			}
			if ((raidp->deadslot != 5) &&
			    ((cp->mode != (defp->mode ? RAID4 : RAID5)) ||
			     (cp->stripe_depth != defp->stripe_depth))) {
				message(raidp, "Disk %d config info doesn't match NVRAM info", 1, i);
				error++;
			}
			if (error)
				continue;
			if (cp->slot_number != i) {
				message(raidp, "Drive %d should be in slot %d, raid ignored",
					       1, i, cp->slot_number);
				raidp->error = EIO;
			}
		}
		if (!raidp->initted)
			message(raidp, "Raid not initialized", 0);

		/*
		 * If the odd-man-out is the controller, reprogram it
		 * with the info from the disks
		 */
		if (raidp->deadslot == 5) {
			raidp->error = EIO;
			raidp->redo_ctlr = 1;
			if (!avoid_cm_message) {
				message(raidp, "The controller has been replaced, use the '-c' and '-m'", 1);
				message(raidp, "arguments to reprogram the controller to match the", 1);
				message(raidp, "config info on the drives", 1);
			}
		} else if ((raidp->deadslot >= 0) &&
			   (raidp->downs.drive[raidp->deadslot] == 0)) {
			/*
			 * One drive's config info doesn't match the others,
			 * but it is claimed to be up by the controller.
			 */
			if (!avoid_cm_message) {
				message(raidp, "Disk %d has been replaced, but it wasn't marked down",
					      1, raidp->deadslot);
				message(raidp, "use the '-c' and '-m' arguments to mark it down", 1);
			}
		}

		/*
		 * fatal errors detected, must reformat the drive
		 */
		if (error > 1) {
			message(raidp, "Raid corrupted, must reformat", 1);
			raidp->error = EIO;
		}
	}
}

/*
 * Find the odd-man-out between 5 disks and the controller of a RAID
 *
 * The sequence of operations to validate the configuration for each RAID:
 *   + Compare config_id's, and timestamps to figure it out
 *   + Use a voting scheme, any one difference is OK
 *       + Only one "odd-man-out" allowed, error if more than one
 *   + We may have a new controller
 *       + Deadslot may be bogus, reset if necessary
 *       + Set deadslot to 5 if controller doesn't match
 *   + We may have a new disk, so ensure the magic number is right
 *   + We may not have gotten config sector info off all disks.
 *       + If all else matches, assume that one is deadslot.
 *       + If not all else matches, reformat.
 *   + Deadslot will always end up pointing to the odd-man-out
 *       + if retval >= 0, there is something down, and retval == deadslot
 *       + if retval == -1, there is nothing down, and deadslot is -1
 *       + if retval == -2, must reformat, and deadslot is dont care
 */
find_oddman(struct raid *raidp)
{
	struct raid_config *cp, *tcp;
	int votecnt, retval, i, j;
	struct {
		int		magic;
		unsigned int	config_id;
		unsigned long	time_sec;
		unsigned long	time_usec;
		int		who;
		int		refcount;
		int		valid;
	} vote[6];

	/*
	 * Match each config entry against the voting array.  Note that we
	 * may not have gotten the config sector info off all disks, so
	 * we count an invalid entry as a new pattern.
	 */
	votecnt = 0;
	bzero((char *)&vote[0], sizeof(vote));
	for (cp = &raidp->config[0], i = 0; i < 6; cp++, i++) {
		for (j = 0; j < votecnt; j++) {
			if ((raidp->conf_valid[i]) && (vote[j].valid) &&
			    (cp->magic     == vote[j].magic) &&
			    (cp->config_id == vote[j].config_id) &&
			    (cp->time_sec  == vote[j].time_sec) &&
			    (cp->time_usec == vote[j].time_usec)) {
				vote[j].who = i;
				vote[j].refcount++;
				break;
			}
		}
		if (j == votecnt) {
			vote[votecnt].magic     = cp->magic;
			vote[votecnt].config_id = cp->config_id;
			vote[votecnt].time_sec  = cp->time_sec;
			vote[votecnt].time_usec = cp->time_usec;
			vote[votecnt].who       = i;
			vote[votecnt].refcount++;
			vote[votecnt].valid     = raidp->conf_valid[i];
			votecnt++;
		}
	}

	/*
	 * Check how many distinct "votes" (ie: patterns) we got:
	 *   + one: no inconsistencies
	 *       + use deadslot info from controller NVRAM
	 *   + two: if one vote a singleton, then we found odd-man-out
	 *       + if odd-man-out is controller, set deadslot to controller
	 *       + if odd-man-out is not the deadslot, then reformat
	 *   + two: otherwise, reformat
	 *   + otherwise, reformat
	 */
	if (votecnt == 1)
		retval = raidp->deadslot;
	else if (votecnt == 2) {
		if (vote[0].refcount == 1)
			retval = vote[0].who;
		else if (vote[1].refcount == 1)
			retval = vote[1].who;
		else
			retval = -2;
		if (retval >= 0) {
			if (retval == 5)
				raidp->deadslot = 5;
			else if (raidp->deadslot == -1)
				raidp->deadslot = retval;
			else if (raidp->deadslot != retval) {
				message(raidp, "Controller and drives disagree on which drive failed,", 1);
				message(raidp, "controller says %d failed, while drives say %d failed",
					       1, raidp->deadslot, retval);
				retval = -2;
			}
		}
	} else
		retval = -2;

	return(retval);
}


/*
 * Read the contents of the controller config into the local config structure
 */
read_controller(struct raid *raidp)
{
	struct usraid_nvram_data data;
	struct raid_config *cp;
	int error = 0, i;
	char buffer[32];

	/*
	 * Do an inquiry to find firmware rev level, etc.
	 */
	bzero(buffer, sizeof(buffer));
	if (ioctl(raid_fd, DIOCDRIVETYPE, buffer) < 0) {
		error = errno;
		message_perror(raidp, "Read of inquiry string failed");
	}
	cp = &raidp->config[5];
	bcopy(&buffer[0],  cp->vendor,   8);
	bcopy(&buffer[8],  cp->product,  16);
	bcopy(&buffer[24], cp->revision, 4);
	blankblaster(cp->vendor);
	blankblaster(cp->product);
	blankblaster(cp->revision);

	/*
	 * read the RAID's general parameters
	 */
	if (ioctl(raid_fd, USRAID_GET_DEFN_PG, &raidp->defn_page) < 0) {
		error = errno;
		message_perror(raidp, "Read of Drive Definition Page failed");
	}

	/*
	 * read the physical to logical mapping
	 */
	if (ioctl(raid_fd, USRAID_GET_PART_PG, &raidp->part_page) < 0) {
		error = errno;
		message_perror(raidp, "Read of Partition Definition Page failed");
	}

	/*
	 * read the bit showing whether the drive has been initialized
	 */
	data.addr = USRAID_NVRAM_INITTED;
	if (ioctl(raid_fd, USRAID_GET_NVRAM, &data) < 0) {
		error = errno;
		message_perror(raidp, "Read of NVRAM init flag failed");
	}
	raidp->initted = (data.data & 0x01) ? 1 : 0;

	/*
	 * read the config ID
	 */
	data.addr = USRAID_NVRAM_CONFIG_ID;
	if (ioctl(raid_fd, USRAID_GET_NVRAM, &data) < 0) {
		error = errno;
		message_perror(raidp, "Read of NVRAM config-id failed");
	}
	cp->config_id = data.data;

	/*
	 * read the timestamp
	 */
	data.addr = USRAID_NVRAM_TIME_SEC;
	if (ioctl(raid_fd, USRAID_GET_NVRAM, &data) < 0) {
		error = errno;
		message_perror(raidp, "Read of NVRAM timestamp failed");
	}
	cp->time_sec = data.data;

	data.addr = USRAID_NVRAM_TIME_USEC;
	if (ioctl(raid_fd, USRAID_GET_NVRAM, &data) < 0) {
		error = errno;
		message_perror(raidp, "Read of NVRAM timestamp failed");
	}
	cp->time_usec = data.data;

	/*
	 * read the status of the write-protect bit
	 */
	data.addr = USRAID_NVRAM_FLAGBITS;
	if (ioctl(raid_fd, USRAID_GET_NVRAM, &data) < 0) {
		error = errno;
		message_perror(raidp, "Read of NVRAM flag bits failed");
	}
	if (data.data & USRAID_NVRAM_WRTPROT)
		raidp->wrtprot = 1;

	/*
	 * fill in the rest of the config info derived from the controller
	 */
	cp->version	 = RAID_CONFIG_REV;
	cp->magic	 = RAID_CONFIG_MAGIC;
	cp->stripe_depth = raidp->defn_page.stripe_depth;
	cp->num_stripes  = raidp->defn_page.num_stripes;
	cp->mode	 = (raidp->defn_page.mode == 0) ? RAID5 : RAID4;
	cp->slot_number	 = 5;
	raidp->conf_valid[5] = (error == 0);

	return(error);
}

/*
 * (Re)program the controller with info from the controller config structure
 */
write_controller(struct raid *raidp)
{
	struct usraid_nvram_data data;
	int error = 0, i;

	/*
	 * set the RAID's parameters, controller has already been
	 * forced to look at each drive
	 */
	raidp->defn_page.stripe_depth	= raidp->config[5].stripe_depth;
	raidp->defn_page.num_stripes	= raidp->config[5].num_stripes;
	if (raidp->config[0].mode == RAID5)
		raidp->defn_page.mode = 0;
	else
		raidp->defn_page.mode = 1;
	if (ioctl(raid_fd, USRAID_SET_DEFN_PG, &raidp->defn_page) < 0) {
		error = errno;
		message_perror(raidp, "Write of Drive Definition Page failed");
		return(error);
	}

	/*
	 * associate the physical disks with the RAID
	 */
	raidp->part_page.base = PART_BASE;	/* reserved sectors are at beginning */
	raidp->part_page.size = raidp->config[5].num_stripes * raidp->config[5].stripe_depth;
	if (ioctl(raid_fd, USRAID_SET_PART_PG, &raidp->part_page) < 0) {
		error = errno;
		message_perror(raidp, "Write of Partition Definition Page failed");
		return(error);
	}

	/*
	 * write the bit showing whether the drive has been initialized
	 */
	data.addr = USRAID_NVRAM_INITTED;
	if (ioctl(raid_fd, USRAID_GET_NVRAM, &data) < 0) {
		error = errno;
		message_perror(raidp, "Read of NVRAM init flag failed");
	}
	if (raidp->initted)
		data.data |= 0x01;
	else
		data.data &= 0x01;
	if (ioctl(raid_fd, USRAID_SET_NVRAM, &data) < 0) {
		error = errno;
		message_perror(raidp, "Write of NVRAM init flag failed");
	}

	/*
	 * write the config ID
	 */
	data.addr = USRAID_NVRAM_CONFIG_ID;
	data.data = raidp->config[5].config_id;
	if (ioctl(raid_fd, USRAID_SET_NVRAM, &data) < 0) {
		error = errno;
		message_perror(raidp, "Write of NVRAM config-id failed");
	}

	/*
	 * write the timestamp
	 */
	data.addr = USRAID_NVRAM_TIME_SEC;
	data.data = raidp->config[5].time_sec;
	if (ioctl(raid_fd, USRAID_SET_NVRAM, &data) < 0) {
		error = errno;
		message_perror(raidp, "Write of NVRAM timestamp failed");
	}

	data.addr = USRAID_NVRAM_TIME_USEC;
	data.data = raidp->config[5].time_usec;
	if (ioctl(raid_fd, USRAID_SET_NVRAM, &data) < 0) {
		error = errno;
		message_perror(raidp, "Write of NVRAM timestamp failed");
	}

	/*
	 * write the write-protect bit
	 */
	data.addr = USRAID_NVRAM_FLAGBITS;
	data.data = 0;
	if (raidp->wrtprot)
		data.data |= USRAID_NVRAM_WRTPROT;
	if (ioctl(raid_fd, USRAID_SET_NVRAM, &data) < 0) {
		error = errno;
		message_perror(raidp, "Write of NVRAM flag bits failed");
	}

	return(error);
}


/*
 * Mode page header info: common to all pages.
 */
struct modehdr {
	u_char	rsvd1;
	u_char	mediumtype;
	u_char	WP:1, rsvd2:2, DPOFUA:1, rsvd3:4;
	u_char	blkdesc_len;
	u_char	rsvd4;
	u_char	blkcount[3];
	u_char	rsvd5;
	u_char	blksize[3];
	u_char	rsvd6:2, pagecode:6;
	u_char	pagelength;
};

/*
 * Specific structure of mode page 0 - Vendor Unique
 */
struct modepage00 {
	struct modehdr	header;
	u_char		QPE:1, UQE:1, DWD:1, rsvd1:5;
	u_char		ASDPE:1, rsvd2:1, CMDAC:1, RPFAE:1, rsvd3:3, CPE:1;
	u_char		rsvd4;
	u_char		rsvd5:2, DSN:1, FRDD:1, DPSDP:1, WPEN:1, rsvd6:2;
	u_char		rsvd7[2];
	u_char		rsvd8:3, DRD:1, led_mode:4;
	u_char		rsvd9[3];
};

/*
 * Specific structure of mode page 1 - Error Recovery
 */
struct modepage01 {
	struct modehdr	header;
	u_char		AWRE:1, ARRE:1, TB:1, RC:1, EER:1, PER:1, DTE:1, DCR:1;
	u_char		read_retry;
	u_char		corr_span;
	u_char		head_offset;
	u_char		strobe_offset;
	u_char		rsvd1;
	u_char		write_retry;
	u_char		rsvd2;
	u_char		recovery_time[2];
};

/*
 * Specific structure of mode page 4 - Geometry
 */
struct modepage04 {
	struct modehdr	header;
	u_char		cylinders[3];
	u_char		heads;
	u_char		cylwrite_precomp[3];
	u_char		cylwrite_reducurr[3];
	u_char		step_rate[2];
	u_char		landing_zone[3];
	u_char		rsvd1:6, RPL:2;
	u_char		rot_offset;
	u_char		rsvd2;
	u_char		rot_rate[2];
	u_char		rsvd3[2];
};

/*
 * Specific structure of mode page 8 - Read caching
 */
struct modepage08 {
	struct modehdr	header;
	u_char		rsvd1:5, WCE:1, MF:1, RCD:1;
	u_char		read_reten:4, write_reten:4;
	u_char		dispref_len[2];
	u_char		minpref_len[2];
	u_char		maxpref_len[2];
	u_char		maxpref_ceil[2];
	u_char		rsvd2;
	u_char		cache_segs;
	u_char		rsvd3[6];
};

/*
 * Program the various mode pages on the disks attached to the RAID.
 * The features we are fiddling with are:
 *    PFA reporting, PER reporting, and spindle synch.
 */
program_disk(struct raid *raidp, int disknum, int syncmaster)
{
	struct modepage00 page00;
	struct modepage01 page01;
	struct modepage04 page04;
	struct modepage08 page08;
	int error = 0, err;

	/* 
	 * Check (and fix up) mode page 0 - Vendor Unique (set PFA)
	 */
	if (err = passthru_modesense(disknum, 0x00, sizeof(page00), &page00)) {
		error = err;
		message_perror(raidp, "couldn't read PFA bit on disk %d", disknum);
	} else if (page00.RPFAE != 1) {
		page00.RPFAE = 1;
		if (err = passthru_modeselect(disknum, 0x00, sizeof(page00), &page00)) {
			error = err;
			message_perror(raidp, "couldn't set PFA bit on disk %d", disknum);
		}
	}

	/* 
	 * Check (and fix up) mode page 1 - Error Recovery (set PER)
	 */
	if (err = passthru_modesense(disknum, 0x01, sizeof(page01), &page01)) {
		error = err;
		message_perror(raidp, "couldn't read PER bit on disk %d", disknum);
	} else if (page01.PER != 1) {
		page01.PER = 1;
		if (err = passthru_modeselect(disknum, 0x01, sizeof(page01), &page01)) {
			error = err;
			message_perror(raidp, "couldn't set PER bit on disk %d", disknum);
		}
	}

	/* 
	 * Check (and fix up) mode page 4 - Geometry (set spindle sync)
	 */
	if (err = passthru_modesense(disknum, 0x04, sizeof(page04), &page04)) {
		error = err;
		message_perror(raidp, "couldn't read spindle sync state on disk %d", disknum);
	} else if (page04.RPL != (syncmaster ? 0x02 : 0x01)) {
		page04.RPL = (syncmaster ? 0x02 : 0x01);
		if (err = passthru_modeselect(disknum, 0x04, sizeof(page04), &page04)) {
			error = err;
			message_perror(raidp, "couldn't set spindle sync state on disk %d", disknum);
		}
		/* XXXGROT: change in RPL require a spindown/spinup to make it take effect */
	}

#if 0
	/*
	 * Check (and fix up) mode page 8 - Read Caching
	 */
	if (err = passthru_modesense(disknum, 0x08, sizeof(page08), &page08)) {
		error = err;
		message_perror(raidp, "couldn't read XXXX on disk %d", disknum);
	} else if (0) {
		/* XXX = 1; */
		if (err = passthru_modeselect(disknum, 0x08, sizeof(page08), &page08)) {
			error = err;
			message_perror(raidp, "couldn't set XXXX on disk %d", disknum);
		}
	}
#endif /* 0 */

	return(error);
}


/*
 * Mode Sense Command - opcode 0x1a
 * Read mode pages from the device.
 */
struct mode_sense {
	u_char	opcode;
	u_char	rsvd1;
	u_char	pagecontrol:2, pagecode:6;
	u_char	rsvd2;
	u_char	alloclen;
	u_char	rsvd3;
};

/*
 * Pass a SCSI mode sense command to the given drive.
 */
passthru_modesense(int disknum, int pagenum, int pagelen, char *buffer)
{
	struct usraid_passthru pt_args;
	struct mode_sense sense_cmd;
	int error = 0;

	/*
	 * Fill out a standard SCSI mode sense command.
	 */
	bzero(&sense_cmd, sizeof(sense_cmd));
	sense_cmd.opcode = 0x1a;
	sense_cmd.pagecontrol = 0;
	sense_cmd.pagecode = pagenum;
	sense_cmd.alloclen = pagelen;

	/*
	 * Fill in the ioctl control parameters.
	 */
	pt_args.drive = disknum;
	pt_args.todisk = 0;
	pt_args.fromdisk = 1;
	pt_args.xfercount = pagelen;
	pt_args.cdblen = sizeof(sense_cmd);
	bcopy(&sense_cmd, pt_args.cdb, sizeof(sense_cmd));
	pt_args.databuf = buffer;

	/*
	 * Execute the mode sense command.
	 */
	if (ioctl(raid_fd, USRAID_PASSTHRU, &pt_args) < 0) {
		error = errno;
	}
	return(error);
}

/*
 * Mode Select Command - opcode 0x15
 * Write new mode pages to the device.
 */
struct mode_select {
	u_char	opcode;
	u_char	rsvd1:3, pageflag:1, rsvd2:3, savepage:1;
	u_char	rsvd3[2];
	u_char	paramlen;
	u_char	rsvd4;
};

/*
 * Pass a SCSI mode sense command to the given drive.
 */
passthru_modeselect(int disknum, int pagenum, int pagelen, caddr_t buffer)
{
	struct usraid_passthru pt_args;
	struct mode_select select_cmd;
	int error = 0;

	/*
	 * mode select doesn't accept direct mode sense info
	 */
	buffer[0] = 0;
	buffer[12] &= 0x7f;

	/*
	 * Fill out a standard SCSI mode select command.
	 */
	bzero(&select_cmd, sizeof(select_cmd));
	select_cmd.opcode = 0x15;
	select_cmd.savepage = 1;
	select_cmd.paramlen = pagelen;

	/*
	 * Fill in the ioctl control parameters.
	 */
	pt_args.drive = disknum;
	pt_args.todisk = 1;
	pt_args.fromdisk = 0;
	pt_args.xfercount = pagelen;
	pt_args.cdblen = sizeof(select_cmd);
	bcopy(&select_cmd, pt_args.cdb, sizeof(select_cmd));
	pt_args.databuf = buffer;

	/*
	 * Execute the mode select command.
	 */
	if (ioctl(raid_fd, USRAID_PASSTHRU, &pt_args) < 0) {
		error = errno;
	}
	return(error);
}


/*
 * The SCSI read and write commands.
 */
struct scsi_rw_cmd {
	u_char	opcode;
	u_char	pad1;
	u_char	lba[4];
	u_char	pad2;
	u_char	length[2];
	u_char	pad3;
};

/*
 * Read and write from/to the reserved sectors on disks.
 */
rw_config(struct raid *raidp, int drive, int rw)
{
	struct usraid_passthru pt_data;
	struct scsi_rw_cmd rw_cmd;
	char buffer[512];
	int command, error = 0;

	/*
	 * Set up read or write command
	 */
	bzero(&rw_cmd, sizeof(rw_cmd));
	if (rw == WRITE)
		rw_cmd.opcode = 0x2a;
	else
		rw_cmd.opcode = 0x28;

#define B(v,s) ((uchar_t)((v) >> s))
#define USR_MSB_SPLIT2(v,a)                                 (a)[0]=B(v,8), (a)[1]=B(v,0)
#define USR_MSB_SPLIT4(v,a) (a)[0]=B(v,24), (a)[1]=B(v,16), (a)[2]=B(v,8), (a)[3]=B(v,0)

	USR_MSB_SPLIT4(0, rw_cmd.lba);	/* reserved sector is physical sector 0 */
	USR_MSB_SPLIT2(1, rw_cmd.length);

	/*
	 * Set up passthru control block
	 */
	bzero(buffer, sizeof(buffer));
	if (rw == WRITE) {
		bcopy(&raidp->config[drive], buffer, sizeof(raidp->config[drive]));
		pt_data.todisk = 1;
		pt_data.fromdisk = 0;
	} else {
		pt_data.todisk = 0;
		pt_data.fromdisk = 1;
	}
	pt_data.drive = drive;
	pt_data.xfercount = 512;
	pt_data.cdblen = sizeof(rw_cmd);
	bcopy(&rw_cmd, pt_data.cdb, sizeof(rw_cmd));
	pt_data.databuf = buffer;

	/*
	 * Execute the command
	 */
	if (ioctl(raid_fd, USRAID_PASSTHRU, &pt_data) < 0) {
		error = errno;
	} else if (rw == READ)
		bcopy(buffer, &raidp->config[drive], sizeof(raidp->config[drive]));
	return(error);
}


/*============================================================================
 * generic utility functions
 */

/*
 * generic routine to get a handle for a given RAID
 */
raidopen(char *path)
{
	if ((raid_fd = open(path, O_RDWR)) < 0)
		return(errno);
	return(0);
}

/*
 * generic routine to release the handle for the current RAID
 */
raidclose()
{
	close(raid_fd);
	raid_fd = 0;
}

/*
 * allocate_raid - allocate and clear memory for a structure to
 * store information about a particular RAID.
 */
struct raid *
allocate_raid(char *path)
{
	struct raid *tmp;
	char *base;

	tmp = (struct raid *)malloc(sizeof(struct raid));
	if (tmp == NULL) {
		message_perror(NULL, "malloc() failed");
		exit(1);
	}
	bzero(tmp, sizeof(struct raid));

	strcpy(tmp->pathname, path);
	if (base = strrchr(path, '/'))
		strcpy(tmp->basename, ++base);
	else
		strcpy(tmp->basename, path);
	return(tmp);
}

/*
 * inventory_raids - used by scaninvent() to find RAID controllers.
 */
inventory_raids(inventory_t *inv, int arg)
{
	struct raid *tmp;
	char pathname[128], buffer[128], *base;
	int error = 0;

	if ((inv->inv_class != INV_DISK) ||
	    (inv->inv_type != INV_SCSIRAID))
		return(0);

	sprintf(pathname, "/dev/rdsk/%s%dd%dvh", USRAID_DEVNAME,
			  inv->inv_controller, inv->inv_unit);

	/*
	 * Decide if the RAID is an UltraStor U144 RAID with SGI firmware.
	 * This program won't deal with any other kind of RAID box.
	 */
	if (error = raidopen(pathname))
		return(0);		/* don't stop scanning */
	if ( (ioctl(raid_fd, DIOCDRIVETYPE, buffer) >= 0) &&
	     (strncmp(&buffer[0], "ULTRA   ", 8) == 0) &&
	     (strncmp(&buffer[8], "U144 RAID    SGI", 16) == 0) ) {
		tmp = allocate_raid(pathname);
		tmp->next = raidhead;
		raidhead = tmp;
	}
	raidclose();
	return(0);
}

/*
 * gen_config_id - generate a number that is as unique as we can
 * easily make it.  Used to help detect when a disk or controller
 * has been replaced.
 */
unsigned int
gen_config_id()
{
	struct timeval tv;
	int seed;

	if (gettimeofday(&tv, NULL) < 0) {
		message_perror(NULL, "gettimeofday() failed");
		exit(1);
	}
	seed  = (tv.tv_sec % 10000) * 100000;
	seed += (tv.tv_usec / 10000) * 1000;
	seed += getpid() % 1000;
	return(seed);
}

/*
 * blankblaster - zero out any trailing blanks in a string
 */
blankblaster(char *str)
{
	for ( ; *str; str++)		/* find the end of the string */
		;
	for (str--; *str == ' '; str--)	/* NULL out trailing spaces */
		*str = 0;
}

/*
 * Have the user confirm that they REALLY want to do an operation.
 */
confirm(struct raid *raidp, int force, char *text, int arg1)
{
	char buffer[80], inline[256], *token, *ch;
	int i;

	if (force)
		return(1);
	sprintf(buffer, text, arg1);
	for (i = 0; i < 4; i++) {
		fprintf(stderr, "Do you really want to %s %s? (yes, no) ", buffer, raidp->basename);
		fgets(inline, sizeof(inline), stdin);
		if (token = strtok(inline, " \t\n")) {
			for (ch = token; *ch; ch++)
				*ch = tolower(*ch);
			if (strcmp(token, "yes") == 0)
				return(1);
			if (strcmp(token, "no") == 0)
				break;
		}
		fprintf(stderr, "Please answer with \"yes\" or \"no\"\n");
	}
	fprintf(stderr, "Confirmation not given, operation aborted\n");
	return(0);
}

/*
 * Check for mounted filesystems in a partition on a RAID before we
 * do something that may affect the data in the RAID.
 * Uses utility routines from libdisk.
 */
checkforfs(struct raid *raidp, int errflag)
{
	struct partition_table *part;
	char *pname, *mntpt;
	int fsvalid, i, error = 0;

	if (!raidp->vh_valid) {
		/*
		 * Haven't read it yet, or it wasn't valid, in either case
		 * read up any volume header that may be present on the disk.
		 */
		if (getdiskheader(raid_fd, &raidp->vh) == -1) {
			raidp->vh_valid = 0;
		} else {
			raidp->vh_valid = 1;
		}
		if (!raidp->vh_valid)
			return(0);
	}

	for (i = 0, part = raidp->vh.vh_pt; part < &raidp->vh.vh_pt[NPARTAB]; part++, i++) {
		if (part->pt_nblks <= 0)
			continue;
		if (part->pt_type == PTYPE_EFS) {
			/*
			 * Check the partition for a valid looking superblock
			 */
			pname = partname(raidp->pathname, i);
			fsvalid = has_fs(pname, part->pt_nblks);
			if (fsvalid) {
				/*
				 * Check to see if the partition is mounted anywhere
				 */
				mntpt = getmountpt("/etc/fstab", pname);
				if (*mntpt == 0) {
					message(raidp, "appears to contain a filesystem, but is not mounted", 0);
				} else {
					error = EINVAL;
					message(raidp, "device contains a filesystem mounted at %s", errflag, mntpt);
				}
			}
		}
	}
	return(error);
}

/*
 * message printing routines - variations on a theme
 */
message(struct raid *raidp, char *text, int errflag,
	       char *arg1, char *arg2, char *arg3)
{
	char buffer[128], *base;
	char *type[] = { "warning: ", "error: ", "" };

	base = raidp ? raidp->basename : "";
	sprintf(buffer, text, arg1, arg2, arg3);
	if (raidp)
		fprintf(stderr, "%s: %s: %s%s\n", progname, base, type[errflag], buffer);
	else
		fprintf(stderr, "%s: %s%s\n", progname, type[errflag], buffer);
	if (syslogit) {
		if (errflag)
			syslog(LOG_ALERT, "%s: %s\n", base, buffer);
		else
			syslog(LOG_ERR, "%s: %s\n", base, buffer);
	}
}

message_perror(struct raid *raidp, char *text, char *arg1, char *arg2)
{
	char buffer[200], *base;
	int error;

	error = errno;
	base = raidp ? raidp->basename : "";
	sprintf(buffer, text, arg1, arg2);
	if (raidp)
		fprintf(stderr, "%s: %s: %s: %s\n", progname, base, buffer, strerror(error));
	else
		fprintf(stderr, "%s: %s: %s\n", progname, buffer, strerror(error));
	if (syslogit) {
		syslog(LOG_ALERT, "%s: %s: %s\n", base, buffer, strerror(error));
	}
}
