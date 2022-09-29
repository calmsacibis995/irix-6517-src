/*-*-C-*-******************************************************************
 *                                                                        *
 *               Copyright (C) 1995, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ifndef mediaclient_included
#define mediaclient_included

/*
 *  This is the media client interface.  The basic idea is that a
 *  client creates a port to monitor a system's removable media
 *  devices, then at asynchronous intervals it is notified that the
 *  system's device state has changed.
 *
 *  A system's state has three parts: volumes, devices and partitions.
 *  A volume is the backing store behind a (potential) file system.
 *  A device is a hardware device such as a CD-ROM drive.
 *  A partition is a section of a device's media that may be part of a volume.
 *
 *  Each volume is made up of one or more partitions.  Each device, if
 *  its media is present, has zero or more partitions.  Each partition
 *  is part of exactly one device.
 */

#include <sys/invent.h>			/* for inventory_t, __uint64_t */
#include <sys/time.h>			/* for time_t */

#if defined(__cplusplus)
extern "C" {
#endif

#   define MC_IX_WHOLE		-1	/* partition index for whole disk */

    /* device formats, aka types. */

#   define MC_FMT_RAW		"raw"	/* raw data */
#   define MC_FMT_EFS		"efs"	/* SGI Extent File System */
#   define MC_FMT_XFS		"xfs"	/* SGI "x" File System */
#   define MC_FMT_DOS		"dos"	/* Microsoft DOS filesystem */
#   define MC_FMT_HFS		"hfs"	/* Apple Hierarchical File System */
#   define MC_FMT_ISO9660	"iso9660" /* ISO std. 9660 (CD-ROM) */
#   define MC_FMT_MUSIC		"music"	/* Audio (DAT) */
#   define MC_FMT_CDDA		"cdda"	/* Compact Disc Digital Audio */

    /* Subformats.  In (mc_volume *)->mcv_subformat. */

#   define MC_SBFMT_PHOTOCD	"photocd"/* Photo CD */
#   define MC_SBFMT_INST	"inst"	/* SGI installation volume */

#   define MC_DEV_NOTIGNORED	(1 << 0)
#   define MC_DEV_SUSPENDED	(1 << 1)

/* MC_VOL_FORCE_UNMOUNT is used to overload a boolean (mcv_mounted)
 * to indicate as part of MCE_FORCEUNMOUNT events which volume is
 * going to be unmounted.
 */
#   define MC_VOL_FORCEUNMOUNT	(1 << 1)

    typedef int                 mc_bool_t;
    typedef void               *mc_closure_t;
    typedef void               *mc_port_t;
    typedef struct mc_device    mc_device_t;
    typedef struct mc_partition mc_partition_t;
    typedef struct mc_system    mc_system_t;
    typedef struct mc_volume    mc_volume_t;
    typedef struct mc_event     mc_event_t;
    typedef struct mc_what_next mc_what_next_t;

    struct mc_system {
	unsigned int    mcs_n_volumes;	/* number of volumes in system */
	unsigned int    mcs_n_devices;	/* number of devices in system */
	unsigned int    mcs_n_parts;	/* number of partitions in system */
	mc_volume_t    *mcs_volumes;	/* ptr to array of volumes */
	mc_device_t    *mcs_devices;	/* ptr to array of devices */
	mc_partition_t *mcs_partitions;	/* ptr to array of partitions */
    };

    struct mc_volume {
	char            *mcv_label;	/* volume label, if any */
	char            *mcv_fsname;	/* e.g., /dev/dsk/dks... */
	char            *mcv_dir;	/* mount point */
	char            *mcv_type;	/* volume type (format), MC_FMT_xxx */
	char            *mcv_subformat;	/* volume subformat, MC_SBFMT_xxx */
	char            *mcv_mntopts;	/* mount options */
	mc_bool_t	 mcv_mounted;	/* is volume mounted? */
	mc_bool_t	 mcv_exported;	/* is volume exported? */
	unsigned int     mcv_nparts;	/* number of partition ptrs */
	mc_partition_t **mcv_parts;	/* points to array of part. ptrs */
    };

    struct mc_device {
	char            *mcd_name;	/* device's short name */
	char            *mcd_fullname;	/* device's printable name */
	char            *mcd_ftrname;	/* device's name as used in FTRs */
	char		*mcd_devname;	/* name of some special file that
					   refers to device. */
	inventory_t      mcd_invent;	/* device's H/W inventory record */
	mc_bool_t        mcd_monitored;	/* is mediad monitoring it? */
	/* Actually, mcd_monitored is two flag bits, MC_DEV_NOTIGNORED
	 * and MC_DEV_SUSPENDED.   It's declared as a bool for
	 * pseudocompatibility with IRIX 6.3.
	 */
	mc_bool_t        mcd_media_present; /* is media present? */
	mc_bool_t        mcd_write_protected; /* is device write-locked? */
	mc_bool_t	 mcd_shared;	/* is device shared? */
	unsigned int     mcd_nparts;	/* number of partition ptrs */
	mc_partition_t **mcd_parts;	/* points to array of part. ptrs */
    };

    struct mc_partition {
	mc_device_t     *mcp_device;	/* device of which this is part */
	char            *mcp_format;	/* partition format, MC_FMT_xxx */
	int              mcp_index;	/* partition # or MC_IX_WHOLE */
	unsigned int     mcp_sectorsize;/* size of sector in bytes */
	__uint64_t       mcp_sector0;	/* starting sector number */
	__uint64_t       mcp_nsectors;	/* number of sectors in part. */
    };
 
    typedef enum mc_event_type {

	MCE_NO_EVENT,			/* no event. */
	MCE_NO_DEVICES,			/* no devices in system */
	MCE_CONFIG,			/* device config changed */

	/* Device events */

	MCE_INSERTION,			/* media was inserted in device */
	MCE_EJECTION,			/* media was ejected from device */
	MCE_SUSPEND,			/* monitoring was suspended */
	MCE_RESUME,			/* monitoring was resumed */

	/* Volume events */

	MCE_MOUNT,			/* volume was mounted */
	MCE_DISMOUNT,			/* volume was dismounted */
	MCE_EXPORT,			/* volume was exported */
	MCE_UNEXPORT,			/* volume was unexported */
	MCE_FORCEUNMOUNT		/* forced dismount pending */

    } mc_event_type_t;

    struct mc_event {
	mc_event_type_t  mce_type;
	int     	 mce_volume;
	int		 mce_device;
    };

    /*
     *  An mc_port is the object through which everything is accessed.
     *
     *	mc_create_port() creates a port and returns an opaque pointer
     *	to it.  Pass in the IP address of the host to be monitored
     *	(INADDR_LOOPBACK from <netinet/in.h> to monitor the local system)
     *	and a callback function.  The function is called whenever
     *	the system's media state changes.
     *
     *	mc_destroy_port() destroys a port and all its resources.
     *	N.B., if the application has allocated resources on mc's
     *	behalf, e.g., timeouts or selection mask bits, it's the app's
     *	job to clean those up.
     *
     *	mc_execute() should be called at various times.  Call it once
     *	after creating an mc_port.  It returns an mc_what_next
     *	structure describing when it should be called again.
     *	mc_what_next specifies that mc_execute should be called when a
     *	file descriptor is readable or writable (as determined by
     *	select(2)) or after a timeout of some number of seconds'
     *	duration.
     *
     *	mc_system_state() returns an mc_system structure describing
     *	the current state of that system.  It may be called at any
     *	time.  It returns NULL if mediad on that system has not yet
     *	been reached.
     *
     *  The mc_port will contact mediad on the given machine.  If the
     *  machine is down, or if mediad is not responding, the mc_port
     *  will repeatedly try to connect.  Once it connects, it will
     *  receive messages from mediad asynchronously.  For these
     *  reasons, it needs scheduling support from the application.
     */

    typedef void (*mc_event_proc_t)(mc_port_t,
				    mc_closure_t,
				    const mc_event_t *);

    typedef enum mc_what {
	MC_IDLE,			/* never seen */
	MC_INPUT,			/* wait for input */
	MC_OUTPUT,			/* wait for output */
	MC_TIMEOUT			/* wait for timeout */
    } mc_what_t;

    typedef enum mc_failure_code {
	MCF_SUCCESS,			/* no problem, man! */
	MCF_SYS_ERROR,			/* system error - check errno */
	MCF_NO_HOST,			/* can't contact host */
	MCF_NO_MEDIAD,			/* can't contact mediad */
	MCF_INPUT_ERROR			/* error parsing an event */
    } mc_failure_code_t;

    struct mc_what_next {
	mc_what_t mcwn_what;		/* N.B. MC_IDLE never seen */
	int mcwn_fd;			/* descriptor for select */
	time_t mcwn_seconds;		/* timeout duration */
	mc_failure_code_t mcwn_failcode;
    };

    mc_port_t mc_create_port(__uint32_t IPaddress,
			     mc_event_proc_t, mc_closure_t);
    void      mc_destroy_port(mc_port_t);

    /*
     * register to receive "forced unmount" events before getting killed
     * via fuser -k
     */
    void      mc_forced_unmounts(mc_port_t);

    const mc_what_next_t *mc_execute(mc_port_t);

    const mc_system_t *mc_system_state(mc_port_t);


    /*
     *  An alternate way to connect to mediad is to use mc_port_connect().
     *  mc_port_connect blocks until it has connected to mediad, then
     *  it returns the descriptor on which the caller should block
     *  on input.  The caller would look like this.
     *
     *		mc_port_t p = mc_create_port(whatever...);
     *  	int fd = mc_port_connect(p);
     *		// ...
     *		// live a full, happy life,
     *		// and select on fd for reading for as long as mc_execute()
     *		// returns MC_INPUT
     *		// ...
     *		mc_destroy_port(p);
     *
     * 	If mediad is not running or is not accepting connections when
     *  mc_port_connect is * called, mc_port_connect returns -1.
     */

    int mc_port_connect(mc_port_t);

    /*
     *  Convenience functions.
     *
     *  mc_dup_system() duplicates an mc_system and all its subordinate
     *  structures.  Think of it as a copy constructor.
     *
     *  mc_destroy_system() destroys an mc_system returned by mc_dup_system().
     */

    mc_system_t *mc_dup_system(const mc_system_t *);
    void mc_destroy_system(mc_system_t *);

#if defined(__cplusplus)
}					// end extern "C"
#endif

#ifdef JOKE
/*
 * mc_system, mc_device, mc_partition, and mc_volume are registered
 * trademarks of McDonald's, Inc.
 */
#endif /* JOKE */

#endif /* !mediaclient_included */
