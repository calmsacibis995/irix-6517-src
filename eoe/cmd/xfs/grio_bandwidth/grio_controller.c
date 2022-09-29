#include "grio_bandwidth.h"

/*
 * Read the /etc/ioconfig.conf file to determine the SCSI controller
 * number for the device node.
 */
typedef struct ioconfig_entry_s {
	int     sc_ctlr_num;
	char    sc_canonical_name[DEV_NMLEN];
} ioconfig_entry_t;

int
get_controller_num(char *nodename, int verbose)
{
	int			rc, found = 0;
	ioconfig_entry_t	cursor;
	FILE 			*config_fp = fopen(IOCONFIG_CTLR_NUM_FILE, "r");

	if (config_fp == NULL) {
		if (verbose) printf("failed to open %s\n", 
					IOCONFIG_CTLR_NUM_FILE);
		return(-1);
	}

	if (verbose) {
		printf("Scanning %s for controller number\n", 
				IOCONFIG_CTLR_NUM_FILE);
	}

	while ((rc = fscanf(config_fp,"%d %s", &cursor.sc_ctlr_num,
			cursor.sc_canonical_name)) != EOF) {
		if (rc != 2) continue;
		if (strcmp(cursor.sc_canonical_name, nodename) == 0) {
			found = 1;
			break;
		}
	}
	fclose(config_fp);
	if (found)
		return(cursor.sc_ctlr_num);
	else
		return(-1);
}


/*
 * List all the disks (including RAID disks) on this SCSI controller.
 */
void
get_devices_on_ctlr(int *units, device_name_t *list, int scsinum, int verbose)
{
	inventory_t	*invp;
	char		tmpname[DEV_NMLEN];

	if (verbose) {
		printf("Finding all disks on SCSI controller %d\n", scsinum);
	}

	setinvent();
	while (invp = getinvent()) {
		if ((invp->inv_class != INV_DISK) || 
			(invp->inv_type != INV_SCSIDRIVE) || 
			(invp->inv_controller != scsinum))
				continue;
		list[*units].scsi_ctlr = scsinum;
		list[*units].scsi_unit = (int)invp->inv_unit;
		if (invp->inv_state > INV_RAID5_LUN) 
			list[*units].scsi_lun = invp->inv_state - INV_RAID5_LUN;
		if (list[*units].scsi_lun) 
			sprintf(tmpname,"/dev/rdsk/dks%dd%dl%dvol", scsinum,
				list[*units].scsi_unit, list[*units].scsi_lun);
		else
			sprintf(tmpname,"/dev/rdsk/dks%dd%dvol", scsinum,
				list[*units].scsi_unit);
		if (user_diskname_to_nodename(tmpname, list[*units].devicename)
				== -1) {
			fprintf(stderr, "grio_bandwidth: failed to get disk "
				"%d, %d, %d.\n", scsinum, 
				list[*units].scsi_unit, list[*units].scsi_lun);
			exit(1);
		}
		if (verbose) {
			printf("Found disk %d %d %d, named %s\n", scsinum,
				list[*units].scsi_unit, list[*units].scsi_lun,
				list[*units].devicename);
		}
		(*units)++;
	}
}

/*
 * Determine the bw that can be provided by a SCSI controller by adding
 * up the bw provided by each disk attached to the controller to one io
 * stream.
 */
int
measure_controller_bw(char *nodename, int sampletime, int iosize, int rw, 
			int addparams, int verbose)
{
	int		num, ret, ctlr_io_count, num_devices = 0;
	device_name_t	devicenames[MAX_NUM_SCSI_UNITS];

	/*
 	 * Determine controller number.
	 */
	num = get_controller_num(nodename, verbose);
	if (num == -1) {
		if (verbose) printf("Could not determine controller number\n");
		return(-1);
	}

	if (verbose) {
		printf("Determining bandwidth of SCSI controller %d\n", num);
	}

	/*
	 * Get all luns of all disks (including RAIDS) on this controller.
	 */
	get_devices_on_ctlr(&num_devices, devicenames, num, verbose);

	/*
	 * Fire off concurrent disk accesses.
	 */
	ret = measure_multiple_disks_io(num_devices, devicenames, sampletime,
		iosize, rw, addparams, verbose, 1, 1, 0);

	/*
	 * Check if error occured during bandwidth measurement.
	 */
	if (ret) {
		return(ret);
	}

	/*
	 * Sum up the I/O for disks on the controller.
	 * This will be the controller bandwidth.
	 */
	ctlr_io_count = get_maxioops_for_raid_controller(0, num_devices);
	fprintf( stdout,"SCSI controller %d provided on average:\n", num);
	fprintf( stdout,"\t\t%d ops of size %d each second\n", 
						ctlr_io_count, iosize);
	
	/*
	 * add REPLACE line for SCSI controller.
	 */
	if (addparams) {
		remove_nodename_params_from_config_file(nodename);
		add_nodename_params_to_config_file(nodename, ctlr_io_count,
				iosize);
	}

	return(0);
}

