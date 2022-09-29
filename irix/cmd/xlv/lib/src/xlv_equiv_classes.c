/**************************************************************************
 *									  *
 * 		 Copyright (C) 1988, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 1.11 $"

/* Routines to handle equivalence classes.

   An equivalence class is defined as the set of dev_t's (i.e., i/o
   paths) that point at the same physical device.  An equivalence
   class is identified by the "class" of device and a unique id.
   It is assumed that all dev_t's handled are of disks: the partition
   number is stripped before they are stored in equivalence classes.

   Note: NULL classes (i.e., of devices with single paths) are not stored.

   exported functions:
	xlv_add_device: add a dev_t to the set of equivalence classes
	xlv_fill_dp: fill a xlv_tab_disk_part_t with the information
		     in the equivalence class of a given dev_t
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <bstring.h>
#include <fcntl.h>
#include <string.h>
#include <sys/errno.h>
#include <sys/debug.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/dsreq.h>
#include <sys/sysmacros.h>
#include <invent.h>
#include <sys/uuid.h>
#include <sys/xlv_base.h>
#include <sys/xlv_tab.h>
#include <pathnames.h>


#define SERIAL_NUM_SIZE 32 /* must be large enough to hold all serial nums */
#define DEVNAME_LEN	32 /* must be long enough to hold all scsi dev names */

typedef struct eq_cl {
    struct eq_cl	 *next;		/* next equiv class */
    dev_t		  dev[N_PATHS];	/* equivalent dev_t's */
    unsigned char	  class;	/* class of multi-path device */
    unsigned char	  n_paths;	/* number of paths found */
    unsigned short	  snum_len;	/* length of serial number */
    unsigned char	  serial_num[SERIAL_NUM_SIZE]; /* serial number */
} equiv_class_t;

static equiv_class_t	 *equiv_classes = NULL;

/*
 * Given a dev_t for a disk, find the equivalence class to which it
 * belongs (i.e., all other dev_t's going to the same physical disk),
 * and thereby set fields in the given xlv_tab_disk_part_t
 */
void
xlv_fill_dp(xlv_tab_disk_part_t *dp, dev_t dev)
{
    dev_t		  part;
    dev_t                 orig_dev;
    equiv_class_t	 *eq_cl;

    orig_dev = dev;

    /* XXXsbarr
     * We need to be able to get the controller_number for a given dev_t
     * the equiv class code relies on matching controller numbers?
     */

    /* bypass the equiv classes for now.  This will break raid failover */

/* --> */
#ifdef NEEDS_TO_BE_UPDATED
    part = PARTOF(dev);
    dev  = CTLR_UNITOF(dev);
    for (eq_cl = equiv_classes; eq_cl; eq_cl = eq_cl->next) {
	int	  inx;

	for (inx = 0; inx < eq_cl->n_paths; inx++) {
	    if (eq_cl->dev[inx] == dev) {
		/* found a match */
		dp->class = eq_cl->class;
		dp->n_paths = eq_cl->n_paths;
		dp->active_path = dp->retry_path = inx;
		/* copy in devs (we no longer need inx ...) */
		for (inx = 0; inx < eq_cl->n_paths; inx++) {
		    dp->dev[inx] = eq_cl->dev[inx] + part;
		}
		FO_DEBUG(printf("DBG: Fill(%x): class: %d (%s), paths:", dev,
				dp->class, eq_cl->serial_num);
			 for (inx = 0; inx < dp->n_paths; inx++) {
			     printf(" %x", dp->dev[inx]);
			 }
			 printf("\n");
		);

		return;
	    }
	}
    }
#endif

    /* not found: "simple" device */
    dp->class = NULL_CLASS;
    dp->n_paths = 1;
    dp->active_path = dp->retry_path = 0;
    dp->dev[0] = orig_dev;
}


static equiv_class_t *
find_equiv_class(int class, const char *serial_num, unsigned short size)
{
    equiv_class_t	 *eq_cl;
#ifdef DEBUG_FAILOVER
    int			  inx;
#endif

    for (eq_cl = equiv_classes; eq_cl; eq_cl = eq_cl->next) {
	if (eq_cl->class == class && eq_cl->snum_len == size
	    && !memcmp(eq_cl->serial_num, serial_num, size)) {
	    break;
	}
    }

    FO_DEBUG(printf("DBG: find_equiv_class(%d, %s, %d): %s.\n", class,
		    serial_num, size, eq_cl ? "found" : "not found");
	    );
    return eq_cl;
}


static int
get_SAUNA_serialnum(inventory_t *inv, char *serial_num, unsigned short *size)
{
#ifdef NEED_TO_DO_SCSI_INQUIRY
/* defines for SCSI inquiry, from the SCSI standard */
#define INQUIRY 0x12
#define INQUIRY_LENGTH 96		/* inquiry length needed */
#define VENDOR_ID 8
#define VENDOR_SPECIFIC 36
#define IS_DISK(inq) (((inq)[0] & 0x1f) == 0 && (inq)[1] != 0) /* valid disk */
#define INQUIRY_RETRIES 5
#endif /* NEED_TO_DO_SCSI_INQUIRY */

/* defines for SCSI sense data */
#define MODE_SENSE 0x1a
#define SENSE_LEN 32

/* defines for Data General CLARiiON RAID (vendor-specific) */
#define MODESENSE_RETRIES 5
#define DGC_SYS_CONFIG_PAGE 0x2A
#define DGC_SERIAL_NUM_OFF 48
#define DGC_SERIAL_NUM_LEN 12
#define DGC_PEER_CTLR_PAGE 0x25
#define DGC_PEER_PAGE_LEN 24
#define DGC_ID_A_OFFSET 14
#define DGC_ID_B_OFFSET 15
#define DGC_SIG_A_OFFSET 16
#define DGC_SIG_B_OFFSET 20
#define DGC_SIG_LEN 4
#define DGC_SNUM_LUN_LEN 26
#define MAX_RAID5_LUNS 8

    int			  scsi_fd, error, n_tries = 0, lun;
    extern int		  errno;
    char		  dev_name[DEVNAME_LEN];
    char		  cmd_return[140];
    char		  scsi_cmd[6];
    char		  sense[SENSE_LEN];
    dsreq_t		  ds;
#ifdef DEBUG_FAILOVER
    char		  err_msg[128];
#endif

    ASSERT(*size >= DGC_SNUM_LUN_LEN);

    if (inv->inv_type == INV_RAIDCTLR) {
	lun = 0;
    }
    else {
	lun = inv->inv_state;
    }
    scsi_devname(dev_name, inv->inv_controller, inv->inv_unit, lun);

    if ((scsi_fd = open(dev_name, O_RDWR)) == -1) {
	if (errno == ENOENT) {
	    FO_DEBUG(fprintf(stderr,
			     "DBG: Device node %s is missing: ignoring it.\n",
			     dev_name);
		     );
	    return NULL_CLASS;
	}
	else if (errno == EBUSY) {
	    fprintf(stderr, "Device %s is busy.\nTerminate the application "
		    "using it, and retry the XLV command.\n", dev_name);
	    return NULL_CLASS;
	}
	else {
	    FO_DEBUG(sprintf(err_msg, "DBG: Failed to open %s", dev_name);
		     perror(err_msg);
		     );
	    return NULL_CLASS;
	}
    }

#ifdef NEED_TO_DO_SCSI_INQUIRY
    scsi_cmd[0] = INQUIRY;
    scsi_cmd[4] = INQUIRY_LENGTH;
    scsi_cmd[1] = scsi_cmd[2] = scsi_cmd[3] = scsi_cmd[5] = 0;

    for (n_tries = INQUIRY_RETRIES; n_tries; n_tries--) {
	bzero(&ds, sizeof(ds));
	ds.ds_flags = DSRQ_READ | DSRQ_SENSE;
	ds.ds_time = 5000; /* timeout in milli-seconds: 5 secs */
	ds.ds_cmdbuf = scsi_cmd;
	ds.ds_cmdlen = sizeof(scsi_cmd);
	ds.ds_databuf = cmd_return;
	ds.ds_datalen = INQUIRY_LENGTH;
	ds.ds_sensebuf = sense;
	ds.ds_senselen = sizeof(sense);

	error = ioctl(scsi_fd, DS_ENTER, &ds);
	if (error == -1 || ds.ds_ret != DSRT_SENSE) {
	    /* we are done if there is no Check Condition */
	    break;
	}
	sleep(1);
    }

    if (error == -1 || ds.ds_ret != DSRT_OK && ds.ds_ret != DSRT_SHORT
	&& ds.ds_ret != 0) {
	FO_DEBUG(fprintf(stderr,
			 "DBG: Ignoring SCSI device %s: inquiry failed.\n",
			 dev_name););
	close(scsi_fd);
	return NULL_CLASS;
    }
#endif /* NEED_TO_DO_SCSI_INQUIRY */

    /* this is a Data General CLARiiON disk: get serial number */
    bzero(cmd_return, sizeof(cmd_return));
    scsi_cmd[0] = MODE_SENSE;
    scsi_cmd[2] = DGC_SYS_CONFIG_PAGE;
    scsi_cmd[4] = DGC_SERIAL_NUM_OFF + DGC_SERIAL_NUM_LEN;
    scsi_cmd[1] = scsi_cmd[3] = scsi_cmd[5] = 0;

    for (n_tries = MODESENSE_RETRIES; n_tries; n_tries--) {
	ds.ds_flags = DSRQ_READ | DSRQ_SENSE;
	ds.ds_time = 5000;	/* timeout in milli-seconds */
	ds.ds_cmdbuf = scsi_cmd;
	ds.ds_cmdlen = sizeof(scsi_cmd);
	ds.ds_databuf = cmd_return;
	ds.ds_datalen = sizeof(cmd_return);
	ds.ds_sensebuf = sense;
	ds.ds_senselen = sizeof(sense);
	ds.ds_ret = 0;

	error = ioctl(scsi_fd, DS_ENTER, &ds);
	if (error == -1 || ds.ds_ret != DSRT_SENSE) {
	    /* we are done if there is no Check Condition */
	    break;
	}

	sleep(1);
    };

    if (error == -1 || ds.ds_ret != DSRT_OK && ds.ds_ret != DSRT_SHORT
	&& ds.ds_ret != 0) {
	FO_DEBUG(printf("DBG: mode sense: error = %d, errno = %d, ret = %d.\n",
			error, errno, ds.ds_ret););
	fprintf(stderr,
		"Ignoring SCSI device %s: could not get serial number.\n",
		dev_name);
	close(scsi_fd);
	return NULL_CLASS;
    }

    bcopy(cmd_return + DGC_SERIAL_NUM_OFF, serial_num, DGC_SERIAL_NUM_LEN);

    bzero(cmd_return, sizeof(cmd_return));
    scsi_cmd[0] = MODE_SENSE;
    scsi_cmd[2] = DGC_PEER_CTLR_PAGE;
    scsi_cmd[4] = DGC_PEER_PAGE_LEN;
    scsi_cmd[1] = scsi_cmd[3] = scsi_cmd[5] = 0;

    for (n_tries = MODESENSE_RETRIES; n_tries; n_tries--) {
	ds.ds_flags = DSRQ_READ | DSRQ_SENSE;
	ds.ds_time = 5000; /* timeout in milli-seconds */
	ds.ds_cmdbuf = scsi_cmd;
	ds.ds_cmdlen = sizeof(scsi_cmd);
	ds.ds_databuf = cmd_return;
	ds.ds_datalen = sizeof(cmd_return);
	ds.ds_sensebuf = sense;
	ds.ds_senselen = sizeof(sense);
	ds.ds_ret = 0;

	error = ioctl(scsi_fd, DS_ENTER, &ds);
	if (error == -1 || ds.ds_ret != DSRT_SENSE) {
	    /* we are done if there is no Check Condition */
	    break;
	}

	sleep(1);
    };

    if (error == -1 || ds.ds_ret != DSRT_OK && ds.ds_ret != DSRT_SHORT
	&& ds.ds_ret != 0) {
	FO_DEBUG(printf("DBG: mode sense: error = %d, errno = %d, ret = %d.\n",
			error, errno, ds.ds_ret););
	fprintf(stderr,
		"Ignoring SCSI device %s: could not get serial number.\n",
		dev_name);
	close(scsi_fd);
	return 0;
    }

    FO_DEBUG(printf("DBG: %s: id: %d, peer: %d; sigs: %x, %x.\n", dev_name,
		    cmd_return[DGC_ID_A_OFFSET], cmd_return[DGC_ID_B_OFFSET],
		    *(ulong *) (cmd_return+DGC_SIG_A_OFFSET),
		    *(ulong *) (cmd_return+DGC_SIG_B_OFFSET)););
    /* make signature unique, no matter which SP it is obtained from */
    if (strncmp(cmd_return + DGC_SIG_A_OFFSET,
		cmd_return + DGC_SIG_B_OFFSET, 4) <= 0) {
	serial_num[DGC_SERIAL_NUM_LEN] = cmd_return[DGC_ID_A_OFFSET];
	serial_num[DGC_SERIAL_NUM_LEN+1] = cmd_return[DGC_ID_B_OFFSET];
	bcopy(cmd_return + DGC_SIG_A_OFFSET,
	      serial_num + DGC_SERIAL_NUM_LEN + 2, DGC_SIG_LEN);
	bcopy(cmd_return + DGC_SIG_B_OFFSET,
	      serial_num + DGC_SERIAL_NUM_LEN + 6, DGC_SIG_LEN);
    }
    else {
	serial_num[DGC_SERIAL_NUM_LEN] = cmd_return[DGC_ID_B_OFFSET];
	serial_num[DGC_SERIAL_NUM_LEN+1] = cmd_return[DGC_ID_A_OFFSET];
	bcopy(cmd_return + DGC_SIG_B_OFFSET,
	      serial_num + DGC_SERIAL_NUM_LEN + 2, DGC_SIG_LEN);
	bcopy(cmd_return + DGC_SIG_A_OFFSET,
	      serial_num + DGC_SERIAL_NUM_LEN + 6, DGC_SIG_LEN);
    }
    sprintf(serial_num + DGC_SNUM_LUN_LEN-4, "-%02d", inv->inv_state);
    *size = DGC_SNUM_LUN_LEN;
    FO_DEBUG(printf("DBG: %s is a DG RAID with serial number:\n",
		    dev_name);
	     for (n_tries = 0; n_tries < DGC_SNUM_LUN_LEN; n_tries++) {
		 printf(" %02x", serial_num[n_tries]);
	     }
	     printf(".\n");
	    );
    close(scsi_fd);

    return DGC_CLASS;
}


static void
add_eq_class(int class, char *serial_num, int size, dev_t dev)
{
    equiv_class_t	 *eq_cl;

    eq_cl = find_equiv_class(class, serial_num, size);
    if (eq_cl) {
	/* this disk's class already exists */
	int	  inx;

	FO_DEBUG(printf("DBG: Add device(%x): found class %d (%s).\n", dev,
			class, serial_num););

	for (inx = 0;
	     inx < eq_cl->n_paths && eq_cl->dev[inx] != dev;
	     inx++) ;

	FO_DEBUG(printf("DBG: Add device(%x): inx = %d, n_p = %d,"
			" dev = %x, dev0 = %x.\n", dev, inx,
			eq_cl->n_paths, dev, eq_cl->dev[0]););

	if (inx == eq_cl->n_paths) {
	    /* we haven't seen this devno before -- add it in */
	    if (eq_cl->n_paths < N_PATHS) {
		eq_cl->dev[eq_cl->n_paths++] = dev;
	    }
	    else {
		fprintf(stderr, "Too many paths to device %s: "
			"ignoring path %s.\n",
			devtorawpath(eq_cl->dev[0]),
			devtorawpath(dev));
	    }
	}
    }
    else {
	/* this disk's class doesn't exist -- make a new one */
	FO_DEBUG(printf("DBG: Add device(%x): new class %d (%s).\n", dev,
			class, serial_num););

	eq_cl = (equiv_class_t *) malloc(sizeof(equiv_class_t));
	eq_cl->class = class;
	memcpy(eq_cl->serial_num, serial_num, size);
	eq_cl->snum_len = size;
	eq_cl->n_paths = 1;
	eq_cl->dev[0] = dev;
	eq_cl->next = equiv_classes;
	equiv_classes = eq_cl;
    }
}


/*
 * If this device is one of the multi-path devices we know of,
 * add it into a class of equivalent paths, either a new class,
 * or a pre-existing one.
 */

/* serial number was based on order number, so we may have duplicates */

void
xlv_add_device(inventory_t *inv, char *devname, dev_t dev)
{
    char		  serial_num[SERIAL_NUM_SIZE];
    unsigned short	  size = SERIAL_NUM_SIZE;
    unsigned char	  class;

#ifdef NEEDS_TO_BE_UPDATED

    if (inv->inv_class == INV_SCSI && inv->inv_type == INV_RAIDCTLR
	|| (inv->inv_class == INV_DISK && inv->inv_type == INV_SCSIDRIVE
	    && (inv->inv_state & INV_RAID5_LUN))) {
	/* SAUNA RAID controller or lun on a SAUNA RAID ctlr */

        /* shut off bit so we can get real lun number */
	inv->inv_state &= ~INV_RAID5_LUN;
	class = get_SAUNA_serialnum(inv, serial_num, &size);
    }
    else return;

    if (class != NULL_CLASS) {
	dev_t	  disk_dev;

	/* we know this disk ... enter it into an equiv class */
	if (inv->inv_type == INV_RAIDCTLR) {
	    int lun;

	    for (lun = 0; lun < MAX_RAID5_LUNS; lun++) {
		sprintf(serial_num + DGC_SNUM_LUN_LEN-4, "-%02d", lun);
		disk_dev = disk_devno(inv->inv_controller, inv->inv_unit, lun);
		if (disk_dev != makedev(0, 0)) {
		    add_eq_class(class, serial_num, size, disk_dev);
		}
	    }
	}
	else {
	    disk_dev = disk_devno(inv->inv_controller, inv->inv_unit,
				  inv->inv_state);
	    if (disk_dev != makedev(0, 0)) {
		add_eq_class(class, serial_num, size, disk_dev);
	    }
	}
    }
    else {
	/* regular old disk */
	FO_DEBUG(printf("DBG: Add device(cl: %d, ty: %d, st: %d): "
			"NULL class.\n", inv->inv_class, inv->inv_type,
			inv->inv_state););
    }

    return;

#endif

}
