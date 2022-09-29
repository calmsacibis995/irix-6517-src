#ident "$Header: /proj/irix6.5.7m/isms/eoe/cmd/xfs/ggd/include/RCS/griotab.h,v 1.30 1997/09/30 18:50:43 kanoj Exp $"

/*
 *  hw_layer.c uses this structure to cache information about each device.
 */

typedef struct device_node {
	dev_t	node_number;		/* device number */
	int	initialize_done;	/* disk private */
	int	ioctlfd;		/* bridge/xbow private */
	int	flags;			/* INV_ type */
	int	ggd_info_node_num;
	device_reserv_info_t resv_info;
} device_node_t;

/*
 * initialize_done  - level of initialization.
 */
#define		IN_CACHE		1
#define		TOLD_KERNEL		2

/*
 *  Size of the known disks array.
 */
#define NUMDISKS 	500

extern device_node_t *error_device_node;	/* device failing bw resv */
