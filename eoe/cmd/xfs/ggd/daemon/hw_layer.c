#include <diskinvent.h>
#include <malloc.h>
#include <unistd.h>
#include <invent.h>
#include <errno.h>
#include <assert.h>
#include <grio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uuid.h>
#include <sys/syssgi.h>
#include <sys/iograph.h>
#include <sys/capability.h>
#include "ggd.h"
#include "griotab.h"
#include "externs.h"

#define MAXRAIDBW	(1024 * 1024)

extern 	dev_info_t	unknown_dev_info;
STATIC int		inc_bridge_bw = 0;
STATIC int xbow_check(device_node_t *, grio_dev_info_t *, grio_dev_info_t *);

STATIC device_node_t *
add_dev_info(grio_dev_info_t *comp, dev_info_t *ioparm)
{
	device_node_t   *devnp;
	dev_info_t	*dinfo;
	int 		length = DEV_NMLEN, type;
	char		dname[DEV_NMLEN];

	if (ioparm == NULL)
		dinfo = get_dev_io_limits(comp->grio_dev_class, 
				comp->grio_dev_type, comp->grio_dev_state);
	else {
		devnp = devinfo_from_cache(comp->devnum);
		if (devnp != NULL)
			ggd_fatal_error("Double REPLACE");
		dinfo = ioparm;
		/*
		 * We want to be intelligent only for non REPLACEd
		 * bridges.
		 */
		inc_bridge_bw = 0;
	}
	if ((devnp=(device_node_t*)calloc(1, sizeof(device_node_t)))==NULL)
		ggd_fatal_error("calloc failed in add_dev_info");

	/*
	 * Initialize the node structure.
	 */
	devnp->node_number = comp->devnum;
	devnp->ioctlfd = 0;
	type = devnp->flags = comp->grio_dev_type;
	devnp->resv_info.dev = comp->devnum;
	devnp->initialize_done = IN_CACHE;
	devnp->resv_info.maxioops = dinfo->maxioops;
	devnp->resv_info.optiosize = dinfo->optiosize;

	/*
	 * The switch should be based on class and type, but since
	 * none of the types have the same value, we can get away 
	 * with this for now. Also, the device devnp->flags set
	 * above depends on the same logic.
	 */

	switch(type) {
		case INV_XBOW:
			/*
			 * At any time, the xbow can move data between
			 * 4 (actually 2 without datapipes) pairs of its 
			 * widgets, each widget moving max data on 16 
			 * bit links. 
			 */
			devnp->resv_info.maxioops = 4 * 
					devnp->resv_info.maxioops;
		case INV_PCI_BRIDGE:
			dev_to_devname(devnp->node_number, dname, &length);
			devnp->ioctlfd = open(dname, O_RDWR);
			if (devnp->ioctlfd == -1) {
				printf("%s : ", dname);
				ggd_fatal_error("could not access\n");
			}
			if ((type == INV_PCI_BRIDGE) && (inc_bridge_bw)) {
				devnp->resv_info.maxioops = (160*1024*1024) /
						devnp->resv_info.optiosize;
				inc_bridge_bw = 0;
			}
		case INV_HUB:
		case INV_SCSICONTROL:
		/* case INV_CPUBOARD == INV_SCSICONTROL */
		case INV_GIO_SCSICONTROL:
		case INV_PCI_SCSICONTROL:
		case INV_SCSIDRIVE:
		case INV_RAIDCTLR:
			/*
			 * RAID io parameters should be REPLACEd. If not,
			 * print warning message and use a low value.
			 */
			if ((type == INV_RAIDCTLR) && (ioparm == NULL)) {
				dev_to_alias(comp->devnum, dname, &length);
				errorlog("Unspecified RAID %s\n", dname);
				errorlog("Read grio_bandwidth man page and \
					run it on your RAID(s)\n");
				errorlog("Assuming RAID bandwidth of 1Mb/s, optiosize %d bytes\n", defaultoptiosize);
				devnp->resv_info.maxioops = 	
						(MAXRAIDBW/defaultoptiosize);
				devnp->resv_info.optiosize = defaultoptiosize;
			}
			if (dinfo != &unknown_dev_info) {
				if (!insert_info_in_cache(comp->devnum, 
				   (void *)devnp)) {
					init_ggd_info_node(devnp);
					break;
				}
			}
		default:
			if (devnp->ioctlfd > 0) close(devnp->ioctlfd);
			free(devnp);
			devnp = (device_node_t *)0;
			ggd_fatal_error("device failed");
			break;
	}

	return(devnp);
}

device_node_t *
get_dev_info(grio_dev_info_t *comp)
{
	device_node_t   *devnp;

	/*
	 * Is it already in the cache?
	 */
	devnp = devinfo_from_cache(comp->devnum);
	if (devnp != NULL) {
		inc_bridge_bw = 0;
		return(devnp);
	}

	/* info. not found in cache, add it */
	return (add_dev_info(comp, NULL));
}

/*ARGSUSED*/
int
reserve_path_bandwidth(
	dev_t 		physdev, 
	dev_t		fs_dev,
	int		perdevreqsize,
	int		perdevreqtime,
	int		numbuckets,
	time_t 		start, 
	time_t 		duration,
	stream_id_t	*gr_stream_id, 
	int 		*maxreqsize, 
	time_t 		*maxreqtime, 
	char 		*err_dev, 
	int 		flags,
	__uint64_t	memloc)
{
	grio_dev_info_t *buf;
	device_node_t 	*devnp;
	int 		status, done, numcomps = 0, arg1 = numbuckets;
	int		arg2 = 0, length = DEV_NMLEN;
	device_reserv_info_t 	*dresvp;
	cap_t		ocap;
	cap_value_t	cap_device_mgt = CAP_DEVICE_MGT;

        /*
         * Check if a reservation for this id already exits.
         */
	devnp = devinfo_from_cache(physdev);
	if (devnp) {
		dresvp = &(devnp->resv_info);
        	if (get_resv_from_id(dresvp->resvs, physdev, gr_stream_id)) {
               		*maxreqsize = 0;
              		return ( EBADF );
        	}
	}

	/* 
	 * malloc a big enough buffer.
	 */

	buf = (grio_dev_info_t *)malloc(MAXCOMPS * sizeof(grio_dev_info_t));
	if (buf == NULL)
		ggd_fatal_error("malloc failed in reserve_path_bandwidth");
	
	/* 
	 * get the path from device to dma memory location, returning
	 * error if path can not be determined.
	 */

	ocap = cap_acquire(1, &cap_device_mgt);
	if (syssgi(SGI_GRIO, GRIO_GET_HWGRAPH_PATH, physdev, buf, MAXCOMPS, 
			&numcomps) == -1) {
		cap_surrender(ocap);
		free(buf);
		return(EBADF);
	}
	cap_surrender(ocap);

	/*
	 * reserve the bandwidth on each component
	 */

	status = done = 0;
	while ((done < numcomps) && (status == 0)) {
		/*
		 * Get info about the component
		 */

		devnp = get_dev_info(&buf[done]);

		switch(buf[done].grio_dev_type) {
			case INV_XBOW:
				status = xbow_check(devnp, &buf[done - 1],
						&buf[done + 1]);
				if (status) {
					printf("Bandwidth reservation failure \
					  on xbow: check for sanity of REPLACE\
					  values for xbow, bridges and hubs\n");
					status = ENOSPC;
					dev_to_devname(devnp->node_number, 
						err_dev, &length);
					done++;
					continue;
				}
				arg1 = (int) (buf[done - 1].devnum);
				arg2 = (int) (buf[done + 1].devnum);
			case INV_HUB:
			case INV_SCSICONTROL:
			/* case INV_CPUBOARD == INV_SCSICONTROL */
			case INV_GIO_SCSICONTROL:
			case INV_PCI_SCSICONTROL:
				/*
				 * The foll fall thru code is never executed
				 * by the INV_XBOW/INV_HUB/INV_CPUBOARD
				 * because the class check fails.
				 */
				if ((buf[done].grio_dev_class == INV_DISK) &&
				   (buf[done].grio_dev_state == INV_FCADP)) {
					/*
					 * We want to double the bw of the
					 * bridge since FC does PCI64. Keep 
					 * a flag so the next INV_PCI_BRIDGE
					 * knows to use a higher value.
					 */
					inc_bridge_bw = 1;
				}
				break;
			case INV_PCI_BRIDGE:
				 /* track the scsi controller */
				arg1 = (int) (buf[done - 1].devnum);
				break;
			case INV_SCSIDRIVE:
			case INV_RAIDCTLR:
				assert(devnp);
				break;
			default:
				ggd_fatal_error("wrong path");
				break;
		}

		if (status = reserve_bandwidth(devnp, fs_dev, perdevreqsize, 
		    perdevreqtime, arg1, arg2, start, duration, physdev, 
		    gr_stream_id, maxreqsize, maxreqtime, err_dev, flags)) {

			/*
 			 * could not reserve bandwidth
		  	 */
			dbgprintf( 1,("reserve on 0x%x failed with %d\n", 
			 	   buf[done].devnum, status) );
			dev_to_devname(devnp->node_number, err_dev, 
							&length);
		}
		done++;
	}

	/*
	 * if there was a failure, release all allocated bandwidth.
	 */

	if (status) {
		done--;
		while (done >= 0) {
			devnp = devinfo_from_cache(buf[done].devnum);
			assert(devnp);
			remove_reservation_with_id(devnp, physdev, 
						gr_stream_id);
			done--;
		}
	}

	/*
	 * return success or failing status.
	 */

	free(buf);
	return(status);
}

/*ARGSUSED*/
int
unreserve_path_bandwidth(dev_t sdev, stream_id_t *gr_stream_id, 
			 __uint64_t memloc)
{
	grio_dev_info_t *buf;
	device_node_t 	*devnp;
	int 		status, done, numcomps = 0;
	cap_t		ocap;
	cap_value_t	cap_device_mgt = CAP_DEVICE_MGT;

	/* 
	 * malloc a big enough buffer.
	 */

	buf = (grio_dev_info_t *)malloc(MAXCOMPS * sizeof(grio_dev_info_t));
	if (buf == NULL)
		ggd_fatal_error("malloc failed in unreserve_path_bandwidth");
	
	/* 
	 * get the path from device to dma memory location, returning
	 * error if path can not be determined.
	 */

	ocap = cap_acquire(1, &cap_device_mgt);
	if (syssgi(SGI_GRIO, GRIO_GET_HWGRAPH_PATH, sdev, buf, MAXCOMPS, 
			&numcomps) == -1) {
		cap_surrender(ocap);
		free(buf);
		return(EBADF);
	}
	cap_surrender(ocap);

	/*
	 * release the reserved bandwidth for each component
	 */

	for (done = 0; done < numcomps; done++) {
		devnp = devinfo_from_cache(buf[done].devnum);
		assert(devnp);
		status = remove_reservation_with_id(devnp, 
				buf[0].devnum, gr_stream_id);
		if ( status )
			dbgprintf( 1,("could not remove resv id on dev 0x%x\n", buf[done].devnum));
	}

	return(status);
}


/*
 * Given information about the source and destination widgets, this
 * checks to see that none of them are sourcing or sinking more than
 * what the xbow can pass. It is sufficient to check that neither of
 * the 2 widgets have a maximum bw more than the xbow links.
 * RETURNS :
 *	0 on success
 *	1 on failure
 * ASSUMPTION :
 *	One of the input widgets is a PCI bridge, and the other is
 *	a hub. This code assumes that the xbow is never a bottleneck,
 *	so it is superfluous anyway.
 * NOTE:
 *	The code takes into account that different widgets have
 *	either 8 or 16 bit connections to the xbow.
 */

STATIC int
xbow_check(device_node_t *xbow, grio_dev_info_t *wid1, grio_dev_info_t *wid2)
{
	/*
 	 * The ggd bandwidth tables store xbow iops/bw info in
	 * terms of a 16 bit connection.
	 */

	int xbow16iops = (xbow->resv_info.maxioops) / 4;
	int xbow16size = xbow->resv_info.optiosize;
	int xbow16bw   = xbow16iops * xbow16size;
	int xbow8bw    = xbow16bw / 2;
	int xbowbw;
	device_node_t	*dev1, *dev2;
	int wid1iops, wid1size, wid1bw;
	int wid2iops, wid2size, wid2bw;

	/*
	 * Check that the input widgets are valid by checking their
	 * device types.
	 */
	if ((xbow == 0) || (wid1 == 0) | (wid2 == 0))
		return(1);

	/*
	 * Find the bw info of each widget.
	 */
	dev1 = get_dev_info(wid1);
	dev2 = get_dev_info(wid2);
	if ((dev1 == 0) || (dev2 == 0))
		return(1);

	/*
	 * Does wid1 have a bw greater than xbow link?
	 */
	wid1iops = dev1->resv_info.maxioops;
	wid1size = dev1->resv_info.optiosize;
	wid1bw = wid1iops * wid1size;
	switch (wid1->grio_dev_type) {
		case INV_PCI_BRIDGE:
			xbowbw = xbow8bw;
			break;
		case INV_HUB:
			xbowbw = xbow16bw;
			break;
		default:
			return(1);
	}
	if (wid1bw > xbowbw)
		return(1);

	/*
	 * Does wid2 have a bw greater than xbow link?
	 */
	wid2iops = dev2->resv_info.maxioops;
	wid2size = dev2->resv_info.optiosize;
	wid2bw = wid2iops * wid2size;
	switch (wid2->grio_dev_type) {
		case INV_PCI_BRIDGE:
			xbowbw = xbow8bw;
			break;
		case INV_HUB:
			xbowbw = xbow16bw;
			break;
		default:
			return(1);
	}
	if (wid2bw > xbowbw)
		return(1);

	return(0);
}


/*
 * This procedure supports the administrative setting up of iops 
 * and iosize for each component in the machine. Code is invoked
 * by adding the REPLACE clause in /etc/grio_disks during startup.
 * Returns 0 on success and 1 on failure. (The first REPLACE
 * clause takes effect)
 */

int
add_admin_info(char *devname, dev_info_t *devparms)
{
	grio_dev_info_t		dvinfo;
	struct stat		sbuf;
	const char		*disk_keyword = "/" EDGE_LBL_DISK;
	const char		*lun0_keyword = "/" EDGE_LBL_LUN "/0";
	const char		*scsi_keyword = "/" EDGE_LBL_SCSI_CTLR;
	const char		*pci_keyword = "/" EDGE_LBL_PCI;
	const char		*xbow_keyword = "/" EDGE_LBL_XBOW;
	const char		*node_keyword = "/" EDGE_LBL_NODE;
	const char		*osys_keyword = "/hw" "/" EDGE_LBL_NODE;
	char			hwgname[DEV_NMLEN];
	int			length = DEV_NMLEN;

	/*
	 * First determine the type of device from the user
	 * supplied hwgfs pathname. Then, modify the user 
	 * supplied name to an ioctl handle to introduce the
	 * device info in the cache. The ioctl handle is what
	 * ggd uses to keep track of the hw components. Keep
	 * algorithm synced up with grio_vhdl_list.
	 */

	if ((devparms->maxioops <= 0) || (devparms->optiosize <= 0))
		return(-1);
	if (stat(devname, &sbuf) == -1)
		return(1);
	dvinfo.devnum = sbuf.st_rdev;

	/*
	 * Convert to hwg pathname.
	 */
	dev_to_devname(dvinfo.devnum, hwgname, &length);

	/*
	 * This should be the scsi_ctlr/y/target/z/lun/0/disk.
	 */
	if ((strstr(hwgname, disk_keyword))) {
		dvinfo.grio_dev_class = INV_DISK;
		dvinfo.grio_dev_type = INV_SCSIDRIVE;
	}

	/*
	 * This should be the scsi_ctlr/y/target/z/lun/0 raid ctlr.
	 */
	else if ((strstr(hwgname, lun0_keyword))) {
		dvinfo.grio_dev_class = INV_SCSI;
		dvinfo.grio_dev_type = INV_RAIDCTLR;
	}

	/*
	 * This should be the pci/x/scsi_ctlr/y point.
	 */
	else if ((strstr(hwgname, scsi_keyword))) {
		dvinfo.grio_dev_class = INV_DISK;
		dvinfo.grio_dev_type = INV_SCSICONTROL;
			/* INV_GIO_SCSICONTROL, INV_PCI_SCSICONTROL is same */
	}

	/*
	 * This should be pci/controller.
	 */
	else if ((strstr(hwgname, pci_keyword))) {
		dvinfo.grio_dev_class = INV_MISC;
		dvinfo.grio_dev_type = INV_PCI_BRIDGE;
	}

	/*
	 * This should be xtalk/0/xbow.
	 */
	else if ((strstr(hwgname, xbow_keyword))) {
		dvinfo.grio_dev_class = INV_MISC;
		dvinfo.grio_dev_type = INV_XBOW;
	}

	/*
	 * This could be /hw/node (system) on pre O series.
	 */
	else if ((strstr(hwgname, osys_keyword))) {
		dvinfo.grio_dev_class = INV_PROCESSOR;
		dvinfo.grio_dev_type = INV_CPUBOARD;
	}

	/*
	 * This should be slot/nx/node.
	 */
	else if ((strstr(hwgname, node_keyword))) {
		dvinfo.grio_dev_class = INV_MISC;
		dvinfo.grio_dev_type = INV_HUB;
	}

	if (add_dev_info(&dvinfo, devparms))
		return(0);

	return(1);
}


/*
 * This procedure issues the ioctls to the pci bridge and xbow
 * to program the gbrs with the right values.
 */
int
access_guaranteed_registers(device_node_t *devnp, reservations_t *resvp)
{
	dev_resv_info_t		*dresvp = DEV_RESV_INFO(resvp);
	unsigned long long	reqbw;
	grio_ioctl_info_t	ioctl_info;
	cap_t			ocap;
	cap_value_t		cap_device_mgt = CAP_DEVICE_MGT;

	/* bandwidth has already been normalized to bytes/sec */
	reqbw = dresvp->iosize;

	ioctl_info.prev_vhdl = resvp->pcibridge;
	ioctl_info.next_vhdl = resvp->hubdevice;
	ioctl_info.reqbw = reqbw;
	
	ocap = cap_acquire(1, &cap_device_mgt);
	if (IS_START_RESV(resvp)) {
		ioctl(devnp->ioctlfd, GIOCSETBW, &ioctl_info);
	} else if ((IS_END_RESV(resvp)) && (RESV_STARTED(resvp))) {
		ioctl(devnp->ioctlfd, GIOCRELEASEBW, &ioctl_info);
	}
	cap_surrender(ocap);
	return(0);
}
