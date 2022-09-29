#ident "$Header: /proj/irix6.5.7m/isms/irix/lib/libgrio/src/RCS/table512.c,v 1.16 1997/08/07 16:52:33 kanoj Exp $"

#include <stdio.h>
#include <unistd.h>
#include <invent.h>
#include <string.h>
#include <bstring.h>
#include <grio.h>
#include <sys/scsi.h>
#include <sys/sysmacros.h>
#include <sys/bsd_types.h>
#include <sys/dkio.h>
#include <fcntl.h>
#include <sys/fs/xfs_inum.h>
#include <sys/uuid.h>
#include <sys/major.h>

/*
 * This file contains specific information about the performance
 * characteristics of each device found on an SGI system, using
 * a request size of 512k.
 */

/* 	Define Bandwidth Information  for Processor Class devices 	*/
/*	device name		    max opts.	opt. size 		*/
#define INV_PROCESSOR_DEV_INFO		0,		0
#define	INV_CPUBOARD_DEV_INFO		0,		0
#define	INV_CPUCHIP_DEV_INFO		0,		0
#define	INV_FRUCHIP_DEV_INFO		0,		0
#define INV_R2300BOARD_DEV_INFO		0,		0
#define INV_IP4BOARD_DEV_INFO		0,		0
#define INV_IP5BOARD_DEV_INFO		0,		0
#define INV_IP6BOARD_DEV_INFO		0,		0
#define INV_IP7BOARD_DEV_INFO		5000,		524288
#define INV_IP9BOARD_DEV_INFO		5000,		524288
#define INV_IP12BOARD_DEV_INFO		5000,		524288
#define INV_IP17BOARD_DEV_INFO		5000,		524288
#define INV_IP15BOARD_DEV_INFO		5000,		524288
#define INV_IP20BOARD_DEV_INFO		5000,		524288
#define INV_IP19BOARD_DEV_INFO		5000,		524288
#define INV_IP22BOARD_DEV_INFO		5000,		524288
#define INV_IP21BOARD_DEV_INFO		5000,		524288
#define INV_IP25BOARD_DEV_INFO		5000,		524288
#define INV_IP26BOARD_DEV_INFO		5000,		524288
#define INV_IP27BOARD_DEV_INFO		5000,		524288
#define INV_IP28BOARD_DEV_INFO		5000,		524288
#define INV_IP30BOARD_DEV_INFO		5000,		524288
#define INV_IP32BOARD_DEV_INFO		5000,		524288

/*	Device State Information    state id.	      DEV_INFO 	         */
#define INV_R2300BOARD_DEV_STATE	1,	INV_R2300BOARD_DEV_INFO
#define INV_IP4BOARD_DEV_STATE		2, 	INV_IP4BOARD_DEV_INFO
#define INV_IP5BOARD_DEV_STATE		3,	INV_IP5BOARD_DEV_INFO
#define INV_IP6BOARD_DEV_STATE		4,	INV_IP6BOARD_DEV_INFO
#define INV_IP7BOARD_DEV_STATE		5,	INV_IP7BOARD_DEV_INFO
#define INV_IP9BOARD_DEV_STATE		6,	INV_IP9BOARD_DEV_INFO
#define INV_IP12BOARD_DEV_STATE		7,	INV_IP12BOARD_DEV_INFO
#define INV_IP17BOARD_DEV_STATE		8,	INV_IP17BOARD_DEV_INFO
#define INV_IP15BOARD_DEV_STATE		9,	INV_IP15BOARD_DEV_INFO
#define INV_IP20BOARD_DEV_STATE		10,	INV_IP20BOARD_DEV_INFO
#define INV_IP19BOARD_DEV_STATE		11,	INV_IP19BOARD_DEV_INFO
#define INV_IP22BOARD_DEV_STATE		12,	INV_IP22BOARD_DEV_INFO
#define INV_IP21BOARD_DEV_STATE		13,	INV_IP21BOARD_DEV_INFO	
#define INV_IP26BOARD_DEV_STATE		14,	INV_IP26BOARD_DEV_INFO
#define INV_IP25BOARD_DEV_STATE		15,	INV_IP25BOARD_DEV_INFO	
#define INV_IP30BOARD_DEV_STATE		16,	INV_IP30BOARD_DEV_INFO
#define INV_IP28BOARD_DEV_STATE		17,	INV_IP28BOARD_DEV_INFO
#define INV_IP32BOARD_DEV_STATE		18,	INV_IP32BOARD_DEV_INFO
#define INV_IP27BOARD_DEV_STATE		19,	INV_IP27BOARD_DEV_INFO


/* 	Define Bandwidth Information for IOBD Class devices 		*/
/*	device name		    max opts.	opt. size 		*/
#define	INV_IOBD_DEV_INFO		0,		0
#define	INV_CLOVER2_DEV_INFO		0,		0
#define	INV_EVIO_DEV_INFO		5000,		524288
#define INV_IO2_REV1_DEV_INFO		5000,		524288
#define INV_IO2_REV2_DEV_INFO		5000,		524288
#define INV_IO3_REV1_DEV_INFO		5000,		524288
#define INV_IO3_REV2_DEV_INFO		5000,		524288
#define INV_IO4_REV1_DEV_INFO		5000,		524288

#define INV_IO4_ADAP_SCSI_DEV_INFO	1000,		524288
#define INV_IO4_ADAP_VMECC_DEV_INFO	1000,		524288
#define INV_IO4_ADAP_EPC_DEV_INFO	1000,		524288
#define INV_IO4_ADAP_SCIP_DEV_INFO	1000,		524288

/*	Device State Information     state id.	    DEV_INFO 	       */
#define INV_IO2_REV1_DEV_STATE		1,	INV_IO2_REV1_DEV_INFO
#define INV_IO2_REV2_DEV_STATE		2,	INV_IO2_REV2_DEV_INFO
#define INV_IO3_REV1_DEV_STATE		3,	INV_IO3_REV1_DEV_INFO
#define INV_IO3_REV2_DEV_STATE		4,	INV_IO3_REV2_DEV_INFO
#define INV_IO4_REV1_DEV_STATE		33,	INV_IO4_REV1_DEV_INFO

#define INV_IO4_ADAP_SCSI_STATE		IO4_ADAP_SCSI_TYPE,	INV_IO4_ADAP_SCSI_DEV_INFO
#define INV_IO4_ADAP_VMECC_STATE	IO4_ADAP_VMECC_TYPE,	INV_IO4_ADAP_SCSI_DEV_INFO
#define INV_IO4_ADAP_EPC_STATE		IO4_ADAP_EPC_TYPE,	INV_IO4_ADAP_SCSI_DEV_INFO
#define INV_IO4_ADAP_SCIP_STATE		IO4_ADAP_SCIP_TYPE,	INV_IO4_ADAP_SCIP_DEV_INFO



/* 	Define Bandwidth Information for DISK Class devices 		*/
/*	device name		    max opts.	opt. size 		*/
#define INV_DISK_DEV_INFO		0,		0
#define INV_SCSICONTROL_DEV_INFO	32,		524288
#define INV_EVIOSCSI_DEV_INFO		0,		0
#define INV_SCSIDRIVE_DEV_INFO		0,		0
#define INV_DKIPCONTROL_DEV_INFO	0,		0
#define INV_DKIPDRIVE_DEV_INFO		0,		0
#define INV_SCSIFLOPPY_DEV_INFO		0,		0
#define INV_DKIP3200_DEV_INFO		0,		0
#define INV_DKIP3201_DEV_INFO		0,		0
#define INV_DKIP4200_DEV_INFO 		0,		0
#define INV_DKIP4201_DEV_INFO		0,		0
#define INV_DKIP4400_DEV_INFO		0,		0
#define INV_XYL714_DEV_INFO		0,		0
#define INV_XYL754_DEV_INFO		0,		0
#define INV_XYLDRIVE_DEV_INFO		0,		0
#define INV_DKIP4460_DEV_INFO		0,		0
#define INV_XYL7800_DEV_INFO		0,		0
#define INV_JAGUAR_DEV_INFO		32,		524288
#define INV_VSCSIDRIVE_DEV_INFO		0,		0
#define INV_GIO_SCSICONTROL_DEV_INFO	32,		524288
#define INV_SCSIRAID_DEV_INFO		0,		0
#define INV_PCI_SCSICONTROL_DEV_INFO    32,             524288
#define INV_WD93_DEV_INFO		32,		524288
#define INV_WD93A_DEV_INFO		32,		524288
#define INV_WD93B_DEV_INFO		32,		524288
#define INV_WD95A_DEV_INFO		32,		524288
#define INV_SCIP95_DEV_INFO		32,		524288
#define INV_ADP7880_DEV_INFO		32,		524288
#define INV_TEAC_FLOPPY_DEV_INFO	0,		0
#define	INV_QL_REV1_DEV_INFO		32,		524288
#define	INV_QL_REV2_DEV_INFO		32,		524288
#define	INV_QL_REV2_4_DEV_INFO		32,		524288
#define	INV_QL_REV3_DEV_INFO		32,		524288
#define	INV_FCADP_DEV_INFO		32,		524288
#define	INV_QL_REV4_DEV_INFO		32,		524288
#define	INV_QL_DEV_INFO			32,		524288

/*	Device State Information    state id.	       DEV_INFO         */
#define INV_WD93_DEV_STATE		0, 	INV_WD93_DEV_INFO
#define INV_WD93A_DEV_STATE		1, 	INV_WD93A_DEV_INFO
#define INV_WD93B_DEV_STATE		2,	INV_WD93B_DEV_INFO
#define INV_WD95A_DEV_STATE		3,	INV_WD95A_DEV_INFO
#define INV_SCIP95_DEV_STATE		4,	INV_SCIP95_DEV_INFO
#define INV_ADP7880_DEV_STATE		5,	INV_ADP7880_DEV_INFO
#define	INV_QL_REV1_DEV_STATE		6,	INV_QL_REV1_DEV_INFO
#define	INV_QL_REV2_DEV_STATE		7,	INV_QL_REV2_DEV_INFO
#define	INV_QL_REV2_4_DEV_STATE		8,	INV_QL_REV2_4_DEV_INFO
#define	INV_QL_REV3_DEV_STATE		9,	INV_QL_REV3_DEV_INFO
#define	INV_FCADP_DEV_STATE		10,	INV_FCADP_DEV_INFO
#define	INV_QL_REV4_DEV_STATE		11,	INV_QL_REV4_DEV_INFO
#define	INV_QL_DEV_STATE		12,	INV_QL_DEV_INFO
#define INV_XYL754_DEV_STATE		12,	INV_XYL754_DEV_INFO
#define INV_XYL714_DEV_STATE		11,	INV_XYL714_DEV_INFO
#define INV_XYL7800_DEV_STATE		15,	INV_XYL7800_DEV_INFO
#define INV_DKIP3200_DEV_STATE		6,	INV_DKIP3200_DEV_INFO
#define INV_DKIP4200_DEV_STATE		8,	INV_DKIP4200_DEV_INFO
#define INV_DKIP4400_DEV_STATE		10,	INV_DKIP4400_DEV_INFO
#define INV_DKIP3201_DEV_STATE		7,	INV_DKIP3201_DEV_INFO
#define INV_DKIP4201_DEV_STATE		9,	INV_DKIP4201_DEV_INFO
#define INV_DKIP4460_DEV_STATE		14,	INV_DKIP4460_DEV_INFO
#define INV_TEAC_FLOPPY_DEV_STATE	1,	INV_TEAC_FLOPPY_DEV_INFO



/*
 * Definitions for NULL devices
 */
#define NULL_DEV_INFO	0,0
#define NULL_STATE_ID	0

#define NULL_DEV_STATE  NULL_STATE_ID, NULL_DEV_INFO

#define	NULL_DEV_STATE_ALL	NULL_DEV_STATE, \
				NULL_DEV_STATE, \
				NULL_DEV_STATE, \
				NULL_DEV_STATE, \
				NULL_DEV_STATE, \
				NULL_DEV_STATE, \
				NULL_DEV_STATE, \
				NULL_DEV_STATE, \
				NULL_DEV_STATE, \
				NULL_DEV_STATE, \
				NULL_DEV_STATE, \
				NULL_DEV_STATE, \
				NULL_DEV_STATE, \
				NULL_DEV_STATE, \
				NULL_DEV_STATE, \
				NULL_DEV_STATE, \
				NULL_DEV_STATE, \
				NULL_DEV_STATE, \
				NULL_DEV_STATE, \
				NULL_DEV_STATE, \
				NULL_DEV_STATE, \
				NULL_DEV_STATE, \
				NULL_DEV_STATE, \
				NULL_DEV_STATE, \
				NULL_DEV_STATE

#define	NULL_DEV_TYPE		NULL_DEV_STATE_ALL, \
				NULL_DEV_INFO

#define	NULL_DEV_TYPE_ALL	NULL_DEV_TYPE, \
				NULL_DEV_TYPE, \
				NULL_DEV_TYPE, \
				NULL_DEV_TYPE, \
				NULL_DEV_TYPE, \
				NULL_DEV_TYPE, \
				NULL_DEV_TYPE, \
				NULL_DEV_TYPE, \
				NULL_DEV_TYPE, \
				NULL_DEV_TYPE, \
				NULL_DEV_TYPE, \
				NULL_DEV_TYPE, \
				NULL_DEV_TYPE, \
				NULL_DEV_TYPE, \
				NULL_DEV_TYPE, \
				NULL_DEV_TYPE, \
				NULL_DEV_TYPE, \
				NULL_DEV_TYPE, \
				NULL_DEV_TYPE, \
				NULL_DEV_TYPE, \
				NULL_DEV_TYPE, \
				NULL_DEV_TYPE, \
				NULL_DEV_TYPE, \
				NULL_DEV_TYPE, \
				NULL_DEV_TYPE
	
#define	NULL_DEV_CLASS		NULL_DEV_TYPE_ALL, \
				NULL_DEV_INFO



/*  
 * Define CPUBOARD type 
 * The order of the devices in the list is critical.
 * It corresponds to the INV_"DEVICE" number.
 */
#define INV_CPUBOARD_TYPE	INV_R2300BOARD_DEV_STATE,\
				INV_IP4BOARD_DEV_STATE,  \
				INV_IP5BOARD_DEV_STATE,  \
				INV_IP6BOARD_DEV_STATE,  \
				INV_IP7BOARD_DEV_STATE,  \
				INV_IP9BOARD_DEV_STATE,  \
				INV_IP12BOARD_DEV_STATE, \
				INV_IP17BOARD_DEV_STATE, \
				INV_IP15BOARD_DEV_STATE, \
				INV_IP20BOARD_DEV_STATE, \
				INV_IP19BOARD_DEV_STATE, \
				INV_IP22BOARD_DEV_STATE, \
				INV_IP21BOARD_DEV_STATE, \
				INV_IP26BOARD_DEV_STATE, \
				INV_IP25BOARD_DEV_STATE, \
				INV_IP30BOARD_DEV_STATE, \
				INV_IP28BOARD_DEV_STATE, \
				INV_IP32BOARD_DEV_STATE, \
				INV_IP27BOARD_DEV_STATE, \
				NULL_DEV_STATE,          \
				NULL_DEV_STATE,          \
				NULL_DEV_STATE,          \
				NULL_DEV_STATE,          \
				NULL_DEV_STATE,          \
				NULL_DEV_STATE,          \
				INV_CPUBOARD_DEV_INFO

#define INV_CPUCHIP_TYPE	NULL_DEV_TYPE
#define INV_FRUCHIP_TYPE	NULL_DEV_TYPE

/*  
 * Define the PROCESSOR class of devices
 * The order of the devices in the list is critical.
 * It corresponds to the INV_"DEVICE" number.
 */
#define INV_PROCESSOR_CLASS	NULL_DEV_TYPE,    \
				INV_CPUBOARD_TYPE,\
				INV_CPUCHIP_TYPE, \
				INV_FRUCHIP_TYPE, \
				NULL_DEV_TYPE,    \
				NULL_DEV_TYPE,    \
				NULL_DEV_TYPE,    \
				NULL_DEV_TYPE,    \
				NULL_DEV_TYPE,    \
				NULL_DEV_TYPE,    \
				NULL_DEV_TYPE,    \
				NULL_DEV_TYPE,    \
				NULL_DEV_TYPE,    \
				NULL_DEV_TYPE,    \
				NULL_DEV_TYPE,    \
				NULL_DEV_TYPE,    \
				NULL_DEV_TYPE,    \
				NULL_DEV_TYPE,    \
				NULL_DEV_TYPE,    \
				NULL_DEV_TYPE,	  \
				NULL_DEV_TYPE,	  \
				NULL_DEV_TYPE,	  \
				NULL_DEV_TYPE,	  \
				NULL_DEV_TYPE,	  \
				NULL_DEV_TYPE,	  \
				INV_PROCESSOR_DEV_INFO

/*  
 * Define the CLOVER2 type of devices.
 * The order of the devices in the list is critical.
 * It corresponds to the INV_"DEVICE" number.
 */
#define INV_CLOVER2_TYPE	INV_IO2_REV1_DEV_STATE, \
				INV_IO2_REV2_DEV_STATE, \
				INV_IO3_REV1_DEV_STATE, \
				INV_IO3_REV2_DEV_STATE, \
				NULL_DEV_STATE,         \
				NULL_DEV_STATE,         \
				NULL_DEV_STATE,         \
				NULL_DEV_STATE,         \
				NULL_DEV_STATE,         \
				NULL_DEV_STATE,         \
				NULL_DEV_STATE,         \
				NULL_DEV_STATE,         \
				NULL_DEV_STATE,         \
				NULL_DEV_STATE,         \
				NULL_DEV_STATE,         \
				NULL_DEV_STATE,         \
				NULL_DEV_STATE,         \
				NULL_DEV_STATE,         \
				NULL_DEV_STATE,         \
				NULL_DEV_STATE,         \
				NULL_DEV_STATE,         \
				NULL_DEV_STATE,         \
				NULL_DEV_STATE,         \
				NULL_DEV_STATE,         \
				NULL_DEV_STATE,         \
				INV_CLOVER2_DEV_INFO

/*  
 * Define the EVIO (Everest I/O) type of devices.
 * The order of the devices in the list is critical.
 * It corresponds to the INV_"DEVICE" number.
 */
#define INV_EVIO_TYPE		INV_IO4_REV1_DEV_STATE, \
				NULL_DEV_STATE,		\
				NULL_DEV_STATE,		\
				NULL_DEV_STATE,		\
				NULL_DEV_STATE,		\
				NULL_DEV_STATE,		\
				NULL_DEV_STATE,		\
				NULL_DEV_STATE,		\
				NULL_DEV_STATE,		\
				NULL_DEV_STATE,		\
				NULL_DEV_STATE,		\
				NULL_DEV_STATE,		\
				NULL_DEV_STATE,		\
				NULL_DEV_STATE,		\
				NULL_DEV_STATE,		\
				NULL_DEV_STATE,		\
				NULL_DEV_STATE,		\
				NULL_DEV_STATE,		\
				NULL_DEV_STATE,		\
				NULL_DEV_STATE,		\
				NULL_DEV_STATE,		\
				NULL_DEV_STATE,		\
				NULL_DEV_STATE,		\
				NULL_DEV_STATE,		\
				NULL_DEV_STATE,		\
				INV_EVIO_DEV_INFO

#define INV_ADAPTOR_TYPE	INV_IO4_ADAP_SCSI_STATE,	\
				INV_IO4_ADAP_VMECC_STATE,	\
				INV_IO4_ADAP_EPC_STATE,		\
				INV_IO4_ADAP_SCIP_STATE,	\
				NULL_DEV_STATE,		\
				NULL_DEV_STATE,		\
				NULL_DEV_STATE,		\
				NULL_DEV_STATE,		\
				NULL_DEV_STATE,		\
				NULL_DEV_STATE,		\
				NULL_DEV_STATE,		\
				NULL_DEV_STATE,		\
				NULL_DEV_STATE,		\
				NULL_DEV_STATE,		\
				NULL_DEV_STATE,		\
				NULL_DEV_STATE,		\
				NULL_DEV_STATE,		\
				NULL_DEV_STATE,		\
				NULL_DEV_STATE,		\
				NULL_DEV_STATE,		\
				NULL_DEV_STATE,		\
				NULL_DEV_STATE,		\
				NULL_DEV_STATE,		\
				NULL_DEV_STATE,		\
				NULL_DEV_STATE,		\
				INV_EVIO_DEV_INFO

/*  
 * Define the IOBD class of devices.
 * The order of the devices in the list is critical.
 * It corresponds to the INV_"DEVICE" number.
 */
#define INV_IOBD_CLASS		NULL_DEV_TYPE,	  \
				INV_CLOVER2_TYPE, \
				INV_EVIO_TYPE,    \
				NULL_DEV_TYPE,    \
				NULL_DEV_TYPE,    \
				NULL_DEV_TYPE,    \
				NULL_DEV_TYPE,    \
				NULL_DEV_TYPE,    \
				NULL_DEV_TYPE,    \
				NULL_DEV_TYPE,    \
				NULL_DEV_TYPE,    \
				NULL_DEV_TYPE,    \
				NULL_DEV_TYPE,    \
				NULL_DEV_TYPE,    \
				NULL_DEV_TYPE,    \
				NULL_DEV_TYPE,    \
				NULL_DEV_TYPE,    \
				NULL_DEV_TYPE,    \
				NULL_DEV_TYPE,    \
				INV_ADAPTOR_TYPE, \
				NULL_DEV_TYPE,    \
				NULL_DEV_TYPE,    \
				NULL_DEV_TYPE,    \
				NULL_DEV_TYPE,    \
				NULL_DEV_TYPE,    \
				INV_IOBD_DEV_INFO

/* 
 * Define the various DISK types of devices 
 */
#define INV_DKIPCONTROL_DEV_TYPE	NULL_DEV_STATE_ALL, \
					INV_DKIPCONTROL_DEV_INFO	

#define INV_DKIP3201_DEV_TYPE		NULL_DEV_STATE_ALL, \
					INV_DKIP3201_DEV_INFO

#define INV_DKIP3200_DEV_TYPE		NULL_DEV_STATE_ALL, \
					INV_DKIP3200_DEV_INFO

#define INV_DKIP4201_DEV_TYPE		NULL_DEV_STATE_ALL, \
					INV_DKIP4201_DEV_INFO

#define INV_DKIP4460_DEV_TYPE		NULL_DEV_STATE_ALL, \
					INV_DKIP4460_DEV_INFO

#define INV_DKIP4200_DEV_TYPE		NULL_DEV_STATE_ALL, \
					INV_DKIP4200_DEV_INFO

#define INV_DKIP4400_DEV_TYPE		NULL_DEV_STATE_ALL, \
					INV_DKIP4400_DEV_INFO

#define INV_JAGUAR_DEV_TYPE		NULL_DEV_STATE_ALL, \
					INV_JAGUAR_DEV_INFO

#define INV_XYL714_DEV_TYPE		NULL_DEV_STATE_ALL, \
					INV_XYL714_DEV_INFO

#define INV_XYL754_DEV_TYPE		NULL_DEV_STATE_ALL, \
					INV_XYL754_DEV_INFO

#define INV_XYL7800_DEV_TYPE		NULL_DEV_STATE_ALL, \
					INV_XYL7800_DEV_INFO

#define INV_SCSIDRIVE_DEV_TYPE		NULL_DEV_STATE_ALL, \
					INV_SCSIDRIVE_DEV_INFO

#define INV_VSCSIDRIVE_DEV_TYPE		NULL_DEV_STATE_ALL, \
					INV_VSCSIDRIVE_DEV_INFO

#define INV_SCSIRAID_DEV_TYPE		NULL_DEV_STATE_ALL, \
					INV_SCSIRAID_DEV_INFO


/*  
 * Define SCSICONTROL type 
 * The order of the devices in the list is critical.
 * It corresponds to the INV_"DEVICE" number.
 */
#define INV_SCSICONTROL_DEV_TYPE	INV_WD93_DEV_STATE,  \
					INV_WD93A_DEV_STATE, \
					INV_WD93B_DEV_STATE, \
					INV_WD95A_DEV_STATE, \
					INV_SCIP95_DEV_STATE,\
					INV_ADP7880_DEV_STATE,\
					INV_QL_REV1_DEV_STATE,	\
					INV_QL_REV2_DEV_STATE,	\
					INV_QL_REV2_4_DEV_STATE,\
					INV_QL_REV3_DEV_STATE,	\
					INV_FCADP_DEV_STATE, \
					INV_QL_REV4_DEV_STATE,	\
					INV_QL_DEV_STATE,    \
					NULL_DEV_STATE,      \
					NULL_DEV_STATE,      \
					NULL_DEV_STATE,      \
					NULL_DEV_STATE,      \
					NULL_DEV_STATE,      \
					NULL_DEV_STATE,      \
					NULL_DEV_STATE,      \
					NULL_DEV_STATE,      \
					NULL_DEV_STATE,      \
					NULL_DEV_STATE,      \
					NULL_DEV_STATE,      \
					NULL_DEV_STATE,      \
					INV_SCSICONTROL_DEV_INFO

/*  
 * Define GIO type.
 * The order of the devices in the list is critical.
 * It corresponds to the INV_"DEVICE" number.
 */
#define INV_GIO_SCSICONTROL_DEV_TYPE	INV_WD93_DEV_STATE,  \
					INV_WD93A_DEV_STATE, \
					INV_WD93B_DEV_STATE, \
					INV_WD95A_DEV_STATE, \
					INV_SCIP95_DEV_STATE,\
					INV_ADP7880_DEV_STATE,\
					INV_QL_REV1_DEV_STATE,	\
					INV_QL_REV2_DEV_STATE,	\
					INV_QL_REV2_4_DEV_STATE,\
					INV_QL_REV3_DEV_STATE,	\
					INV_FCADP_DEV_STATE, \
					INV_QL_REV4_DEV_STATE,	\
					INV_QL_DEV_STATE,    \
					NULL_DEV_STATE,      \
					NULL_DEV_STATE,      \
					NULL_DEV_STATE,      \
					NULL_DEV_STATE,      \
					NULL_DEV_STATE,      \
					NULL_DEV_STATE,      \
					NULL_DEV_STATE,      \
					NULL_DEV_STATE,      \
					NULL_DEV_STATE,      \
					NULL_DEV_STATE,      \
					NULL_DEV_STATE,      \
					NULL_DEV_STATE,      \
					INV_GIO_SCSICONTROL_DEV_INFO

/*  
 * Define PCI type.
 * The order of the devices in the list is critical.
 * It corresponds to the INV_"DEVICE" number.
 */
#define INV_PCI_SCSICONTROL_DEV_TYPE	NULL_DEV_STATE,      \
					NULL_DEV_STATE,      \
					NULL_DEV_STATE,      \
					NULL_DEV_STATE,      \
					NULL_DEV_STATE,	     \
					INV_ADP7880_DEV_STATE,\
					NULL_DEV_STATE,      \
					NULL_DEV_STATE,      \
					NULL_DEV_STATE,      \
					NULL_DEV_STATE,      \
					NULL_DEV_STATE,      \
					NULL_DEV_STATE,      \
					NULL_DEV_STATE,      \
					NULL_DEV_STATE,      \
					NULL_DEV_STATE,      \
					NULL_DEV_STATE,      \
					NULL_DEV_STATE,      \
					NULL_DEV_STATE,      \
					NULL_DEV_STATE,      \
					NULL_DEV_STATE,      \
					NULL_DEV_STATE,      \
					NULL_DEV_STATE,      \
					NULL_DEV_STATE,      \
					NULL_DEV_STATE,      \
					NULL_DEV_STATE,      \
					INV_PCI_SCSICONTROL_DEV_INFO
/*  
 * Define WDKIOPDRIVE type.
 * The order of the devices in the list is critical.
 * It corresponds to the INV_"DEVICE" number.
 */
#define INV_DKIPDRIVE_DEV_TYPE		INV_DKIP3200_DEV_STATE, \
					INV_DKIP4200_DEV_STATE, \
					INV_DKIP4400_DEV_STATE, \
					INV_DKIP3201_DEV_STATE, \
					INV_DKIP4201_DEV_STATE, \
					INV_DKIP4460_DEV_STATE, \
					NULL_DEV_STATE,         \
					NULL_DEV_STATE,         \
					NULL_DEV_STATE,         \
					NULL_DEV_STATE,         \
					NULL_DEV_STATE,         \
					NULL_DEV_STATE,         \
					NULL_DEV_STATE,         \
					NULL_DEV_STATE,         \
					NULL_DEV_STATE,         \
					NULL_DEV_STATE,         \
					NULL_DEV_STATE,         \
					NULL_DEV_STATE,         \
					NULL_DEV_STATE,         \
					NULL_DEV_STATE,         \
					NULL_DEV_STATE,         \
					NULL_DEV_STATE,         \
					NULL_DEV_STATE,         \
					NULL_DEV_STATE,         \
					NULL_DEV_STATE,         \
					INV_DKIPDRIVE_DEV_INFO

/*  
 * Define XYLDRIVE type.
 * The order of the devices in the list is critical.
 * It corresponds to the INV_"DEVICE" number.
 */
#define INV_XYLDRIVE_DEV_TYPE		INV_XYL754_DEV_STATE,   \
					INV_XYL714_DEV_STATE,   \
					INV_XYL7800_DEV_STATE,  \
					NULL_DEV_STATE,         \
					NULL_DEV_STATE,         \
					NULL_DEV_STATE,         \
					NULL_DEV_STATE,         \
					NULL_DEV_STATE,         \
					NULL_DEV_STATE,         \
					NULL_DEV_STATE,         \
					NULL_DEV_STATE,         \
					NULL_DEV_STATE,         \
					NULL_DEV_STATE,         \
					NULL_DEV_STATE,         \
					NULL_DEV_STATE,         \
					NULL_DEV_STATE,         \
					NULL_DEV_STATE,         \
					NULL_DEV_STATE,         \
					NULL_DEV_STATE,         \
					NULL_DEV_STATE,         \
					NULL_DEV_STATE,         \
					NULL_DEV_STATE,         \
					NULL_DEV_STATE,         \
					NULL_DEV_STATE,         \
					NULL_DEV_STATE,         \
					INV_XYLDRIVE_DEV_INFO

/*  
 * Define SCSIFLOPPY type.
 * The order of the devices in the list is critical.
 * It corresponds to the INV_"DEVICE" number.
 */
#define INV_SCSIFLOPPY_DEV_TYPE		INV_TEAC_FLOPPY_DEV_STATE, \
					NULL_DEV_STATE,            \
					NULL_DEV_STATE,            \
					NULL_DEV_STATE,            \
					NULL_DEV_STATE,            \
					NULL_DEV_STATE,            \
					NULL_DEV_STATE,            \
					NULL_DEV_STATE,            \
					NULL_DEV_STATE,            \
					NULL_DEV_STATE,            \
					NULL_DEV_STATE,            \
					NULL_DEV_STATE,            \
					NULL_DEV_STATE,            \
					NULL_DEV_STATE,            \
					NULL_DEV_STATE,            \
					NULL_DEV_STATE,            \
					NULL_DEV_STATE,            \
					NULL_DEV_STATE,            \
					NULL_DEV_STATE,            \
					NULL_DEV_STATE,            \
					NULL_DEV_STATE,            \
					NULL_DEV_STATE,            \
					NULL_DEV_STATE,            \
					NULL_DEV_STATE,            \
					NULL_DEV_STATE,            \
					INV_SCSIFLOPPY_DEV_INFO
				

/*  
 * Define DISK class of devices. 
 * The order of the devices in the list is critical.
 * It corresponds to the INV_"DEVICE" number.
 */
#define INV_DISK_CLASS        	NULL_DEV_TYPE,               \
			       	INV_SCSICONTROL_DEV_TYPE,    \
				INV_SCSIDRIVE_DEV_TYPE,      \
				INV_DKIPCONTROL_DEV_TYPE,    \
				INV_DKIPDRIVE_DEV_TYPE,      \
				INV_SCSIFLOPPY_DEV_TYPE,     \
				INV_DKIP3200_DEV_TYPE,       \
				INV_DKIP3201_DEV_TYPE,       \
				INV_DKIP4200_DEV_TYPE,       \
				INV_DKIP4201_DEV_TYPE,       \
				INV_DKIP4400_DEV_TYPE,       \
				INV_XYL714_DEV_TYPE,         \
				INV_XYL754_DEV_TYPE,         \
				INV_XYLDRIVE_DEV_TYPE,       \
				INV_DKIP4460_DEV_TYPE,       \
				INV_XYL7800_DEV_TYPE,        \
				INV_JAGUAR_DEV_TYPE,         \
				INV_VSCSIDRIVE_DEV_TYPE,     \
				INV_GIO_SCSICONTROL_DEV_TYPE,\
				INV_SCSIRAID_DEV_TYPE,	     \
				INV_DISK_DEV_INFO

/* read README */
#define	INV_MISC_DEV_INFO		0,		0
#define	INV_PCI_BRIDGE_DEV_INFO		180,		524288
#define	INV_XBOW_DEV_INFO		800,		524288
#define INV_HUB_DEV_INFO		600,		524288

#define	INV_PCI_BRIDGE_DEV_TYPE		NULL_DEV_STATE_ALL,	\
					INV_PCI_BRIDGE_DEV_INFO
#define	INV_XBOW_DEV_TYPE		NULL_DEV_STATE_ALL,	\
					INV_XBOW_DEV_INFO
#define	INV_HUB_DEV_TYPE		NULL_DEV_STATE_ALL,	\
					INV_HUB_DEV_INFO

#define	INV_MISC_CLASS		NULL_DEV_TYPE,			\
				NULL_DEV_TYPE,			\
				NULL_DEV_TYPE,			\
				INV_XBOW_DEV_TYPE,		\
				INV_HUB_DEV_TYPE,		\
				INV_PCI_BRIDGE_DEV_TYPE,	\
				NULL_DEV_TYPE,			\
				NULL_DEV_TYPE,			\
				NULL_DEV_TYPE,			\
				NULL_DEV_TYPE,			\
				NULL_DEV_TYPE,			\
				NULL_DEV_TYPE,			\
				NULL_DEV_TYPE,			\
				NULL_DEV_TYPE,			\
				NULL_DEV_TYPE,			\
				NULL_DEV_TYPE,			\
				NULL_DEV_TYPE,			\
				NULL_DEV_TYPE,			\
				NULL_DEV_TYPE,			\
				NULL_DEV_TYPE,			\
				INV_MISC_DEV_INFO


/* 
 * The remaining device classes are not currently implemented.
 */
#define INV_MEMORY_CLASS	NULL_DEV_CLASS
#define INV_SERIAL_CLASS	NULL_DEV_CLASS
#define INV_PARALLEL_CLASS	NULL_DEV_CLASS
#define INV_TAPE_CLASS		NULL_DEV_CLASS
#define INV_GRAPHICS_CLASS	NULL_DEV_CLASS
#define INV_NETWORK_CLASS	NULL_DEV_CLASS
#define INV_SCSI_CLASS		NULL_DEV_CLASS
#define INV_AUDIO_CLASS		NULL_DEV_CLASS
#define INV_VIDEO_CLASS		NULL_DEV_CLASS
#define INV_BUS_CLASS		NULL_DEV_CLASS
#define	INV_COMPRESSION_CLASS	NULL_DEV_CLASS
#define	INV_VSCSI_CLASS		NULL_DEV_CLASS
#define	INV_DISPLAY_CLASS	NULL_DEV_CLASS
#define	INV_UNC_SCSILUN_CLASS	NULL_DEV_CLASS
#define	INV_PCI_CLASS		NULL_DEV_CLASS
#define	INV_PCI_NO_DRV_CLASS	NULL_DEV_CLASS
#define	INV_PROM_CLASS		NULL_DEV_CLASS


/*  
 * The order of the devices in the list is critical.
 * It corresponds to the INV_"CLASS" number.
 */
dev_class_t     devclasses512[] = {
			{ NULL_DEV_CLASS 	},		/* 0 */
			{ INV_PROCESSOR_CLASS	},		/* 1 */
			{ INV_DISK_CLASS	},		/* 2 */
			{ INV_MEMORY_CLASS	},		/* 3 */
			{ INV_SERIAL_CLASS	},		/* 4 */
			{ INV_PARALLEL_CLASS	},		/* 5 */
			{ INV_TAPE_CLASS 	},		/* 6 */
			{ INV_GRAPHICS_CLASS 	},		/* 7 */
			{ INV_NETWORK_CLASS 	},		/* 8 */
			{ INV_SCSI_CLASS 	},		/* 9 */
			{ INV_AUDIO_CLASS	},		/* 10 */
			{ INV_IOBD_CLASS	},		/* 11 */
			{ INV_VIDEO_CLASS 	},		/* 12 */
			{ INV_BUS_CLASS 	},		/* 13 */
			{ INV_MISC_CLASS 	},		/* 14 */
			{ INV_COMPRESSION_CLASS	},		/* 15 */
			{ INV_VSCSI_CLASS 	},		/* 16 */
			{ INV_DISPLAY_CLASS 	},		/* 17 */
			{ INV_UNC_SCSILUN_CLASS	},		/* 18 */
			{ INV_PCI_CLASS 	},		/* 19 */
			{ INV_PCI_NO_DRV_CLASS 	},		/* 20 */
			{ INV_PROM_CLASS 	},		/* 21 */
};
