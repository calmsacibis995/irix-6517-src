/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"$Revision: 1.10 $"

#ifndef _GRIO_DOT_H_
#define _GRIO_DOT_H_

#ifdef __cplusplus
extern "C" {
#endif

#include	<sys/types.h>
#include	<sys/grio.h>

/*
 * maximum number of GRIO streams allowed without a license
 */
#define DEFAULT_GRIO_STREAMS    4

/*
 * maximum number of GRIO streams allowed with an "unlimited" license
 */
#define UNLIMITED_GRIO_STREAMS  MAX_NUM_RESERVATIONS

/*
 * Subcommands for the GRIO_GET_STATS command.
 */
#define GRIO_FILE_RESVS         1
#define GRIO_DEV_RESVS          2
#define GRIO_PROC_RESVS         3

#define GGD_INFO_KEY            0x53637445
#define GGD_INFO_MAGIC          0x53637445

/*
 * this structure defines statisical 
 * information maintained by the ggd daemon
 */
typedef struct ggd_node_summary {
	dev_t	node_num;			/* device # for node */
	int	devtype;			/* device type       */
	int	max_io_rate;			/* rate in kbytes/sec */
	int	remaining_io_rate;		/* rate in kbytes/sec */
	int	optiosize;			/* opt i/o size in bytes */
	int	num_opt_ios;			/* max number of opt i/os */
} ggd_node_summary_t;

typedef struct ggd_info_summary {
	int	rev_count;		/* updated when ggd restarts */
	int	ggd_info_magic;		/* magic number */
	int	num_nodes;		/* num enteries in node_info */
	int	active_cmd_count;	/* num enteries in per_cmd_  */
	int	max_cmd_count;		/* set to GRIO_MAX_COMMANDS  */
	int	per_cmd_count[GRIO_MAX_COMMANDS];
	ggd_node_summary_t node_info[1];
} ggd_info_summary_t;

/*
 *	This file contains the defintions of the structures used to hold
 *	the bandwidth information for each I/O device supported at SGI.
 *	It also defines the structures used to build the tree of devices
 *	currently available on the system.
 */

#define		GET_DEVCLASSES()	get_devclassptr(defaultoptiosize)

/*
 * This is the maximum number of immediate subcategories that any single 
 * device category may have. There are 3 types of device categories:
 * 	class, type, state
 * A single class item cannot have more than NUM_DEV_ITEMS different types
 * associated with it. Similarily, a single type item cannot have more 
 * than NUM_DEV_ITEMS different states associated with it.
 *
 */
#define NUM_DEV_ITEMS	25

/*
 * IO Adaptor Defines: 
 * Specific to Everest platform.
 * 	If the number of boards in the INV_IOBD board class reach to
 *	19, the IO4_ADAPTOR_TYPE will have to be changed.
 *	Currently, there is only the CLOVER2 and the EVIO board in the
 *	INV_IOBD board class.
 */
#define IO4_ADAPTOR_TYPE        19
/*
 * Types of IO4 adaptors that currently support guaranteed rate I/O.
 */
#define IO4_ADAP_SCSI_TYPE	0
#define IO4_ADAP_VMECC_TYPE	1
#define IO4_ADAP_EPC_TYPE	2
#define IO4_ADAP_SCIP_TYPE	3

/*
 * These next structures are used in the table.c file to construct a
 * static table which contains the bandwidth information for each I/O
 * device supported at SGI.
 */


/*
 * This structure holds the optimal I/O size in bytes and the number of 
 * requests of optimal I/O size that can be completed by the device in 
 * one second.
 */
typedef struct dev_info {
        int             maxioops;
        int             optiosize;
} dev_info_t;

/*
 * This structure contains the state information for a device.
 */
typedef struct dev_state {
        int             state_id;
        dev_info_t      dev_info;
} dev_state_t;

/*
 * This structure contains the type information for a device and a list
 * of all the states associated with that device.
 */
typedef struct dev_type {
        dev_state_t     dev_state[NUM_DEV_ITEMS];
        dev_info_t      dev_info;
} dev_type_t;

/*
 * This structure contains the class information for a device and a list
 * of all the types associated with that device.
 */
typedef struct dev_class {
        dev_type_t      dev_type[NUM_DEV_ITEMS];
        dev_info_t      dev_info;
} dev_class_t;


/*
 * The length of the string which uniquely identifies a type of disk. 
 */
#define DISK_ID_SIZE	28

/*
 * This structure contains the device information of a single type of
 * disk drive and the identification string for that type of drive.
 */
typedef struct dev_disk {
        char    disk_id[DISK_ID_SIZE];
        dev_info_t dev_info;
} dev_disk_t;

/*
 * external declarations for lib_grio
 */

extern int		defaultoptiosize;
extern dev_info_t	unknown_dev_info;
extern dev_info_t *	get_dev_io_limits(int, int, int);
extern int	grio_query_device(int, int, int, int);
extern int 	grio_action_list(int, grio_resv_t *);
extern int  	grio_associate_file( int, stream_id_t *);
extern int  	grio_query_fs( dev_t, int *, int *);

/* 
 * routines for backward compatibility only
 */
extern int grio_unreserve_bw( stream_id_t *);
extern int grio_reserve_file( int, grio_resv_t *);
extern int grio_reserve_fs( dev_t, grio_resv_t *);

#ifdef __cplusplus
}
#endif

#endif /* _GRIO_DOT_H_ */

