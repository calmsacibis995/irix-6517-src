/*
 * Copyright 1988-1996, Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */

#ifndef _IOCONFIG_H_
#define _IOCONFIG_H_

#include <sys/conf.h>
#ifdef IOCONFIG_TEST

#define EDGE_LBL_PARTITION		"partition"
#define EDGE_LBL_VOLUME			"volume"
#define EDGE_LBK_SCSI_CTLR		"scsi_ctlr"
#define INFO_LBL_CONTROLLER_NAME	"sgi_controller_name"
#define MAXDEVNAME			1024
#define SIOC_MKHWG                      (('z' << 8) | 5)
#define INV_IOC3_DMA                    16
#define INV_IOC3_PIO                    17
#define INFO_LBL_INVENT                 "sgi_invent"
#define MTALIAS				(('t' << 8) | 'l')
#define DIOCMKALIAS			(('d' << 8) | 46)

#else

#include <sys/iograph.h>

#endif	/* IOCONFIG_TEST */


#define USAGE_STR			"ioconfig <root of the file tree>"
#define IOCONFIG_CTLR_NUM_FILE		"/etc/ioconfig.conf"
#define IOPERMFILE			"/etc/ioperms"
#define MAX_NAME_LEN			100
#define SLASH				"/"
#define MAX_HWG_DEPTH			100
#define MAX_CMD_LEN			(MAXPATHLEN + 30)
#define MAX_LINE_LEN			(MAXPATHLEN + MAX_NAME_LEN * 2 + 10)

#define IOC_SCSICONTROL			0
#define IOC_TTY				1
#define IOC_NETWORK			2
#define	IOC_ETHER_EF			3
#define IOC_RAD				4
#define IOC_PCK				5
#define IOC_PCM				6
#define IOC_EINT			7
#define IOC_PLP				8

#define ASTERISK_STR			"*"
#define POUND_CHAR			'#'

int ioc_ctlr_num_get(inventory_t *);

/* Starting controller numbers for each
 * kind of device
 */
#define SCSI_CTLR_START			2
#define TTY_START			3
#define ETHER_EF_START			1
#define NETWORK_START			0
#define RAD_START			1
#define PCKM_START			2
#define EINT_START			2
#define PLP_START			1

/* #if 1... 
 * For testing the /var/sysgen/ioconfig device
 * policy specification
 */
#if 1
#define MAX_BASE_DEVICES		(IOC_PLP + 1)
#else
#define MAX_BASE_DEVICES		1
#endif

#define MAX_DEVICES			(MAX_BASE_DEVICES + 10)
#define IGNORE				-1
#define IGNORE_STR			"-1"
#define GENERIC				1
#define SPECIAL_CASE			0		
/*
 * Define a table of device specific paramters
 * to process_ioc_device_vertex()
 */
struct device_args_s {
	char	suffix[MAX_NAME_LEN];
	char	pattern[MAX_NAME_LEN];
	int	start_num;
	int	ioctl_num;
};
typedef struct device_args_s device_args_t;
struct device_process_s {
	int	inv_class;
	int	inv_type;
	int	inv_state;
	int 	generic;
	long	info;
	/*	void	(*process_fn)(const char *,inventory_t *); */
};
typedef struct device_process_s device_process_t;
typedef void (*process_fn_t)(const char *,inventory_t *);
extern 	int    debug;
#endif /* #ifndef _IOCONFIG_H_ */
