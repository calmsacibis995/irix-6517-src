#ifndef __DEVMGMT_H__
#define __DEVMGMT_H__
#ifdef __cplusplus
extern "C" {
#endif
#ident "$Revision: 1.6 $"
/*
*
* Copyright 1992, Silicon Graphics, Inc.
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
/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/


#ifndef __SVR4_1__	/* svr4 flavor */
/*
 * devmgmt.h
 *
 * Contents:
 *    -	Device Management definitions,
 *    -	getvol() definitions
 */

/*
 * Device management definitions
 *	- Default pathnames (relative to installation point)
 * 	- Environment variable namess
 * 	- Standard field names in the device table
 *	- Flags
 *	- Miscellaneous definitions
 */


/*
 * Default pathnames (relative to the package installation
 * point) to the files used by Device Management:
 *
 *	DTAB_PATH	Device table
 *	DGRP_PATH	Device group table
 *	DVLK_PATH	Device reservation table
 */

#define	DTAB_PATH			"/etc/device.tab"
#define	DGRP_PATH			"/etc/dgroup.tab"
#define	DVLK_PATH			"/etc/devlkfile"


/*
 * Names of environment variables
 *
 *	OAM_DEVTAB	Name of variable that defines the pathname to
 *			the device-table file
 *	OAM_DGROUP	Name of variable that defines the pathname to
 *			the device-group table file
 *	OAM_DEVLKTAB	Name of variable that defines the pathname to
 *			the device-reservation table file
 */

#define	OAM_DEVTAB			"OAM_DEVTAB"
#define	OAM_DGROUP			"OAM_DGROUP"
#define	OAM_DEVLKTAB			"OAM_DEVLKTAB"


/*
 * Standard field names in the device table
 */
 
#define	DTAB_ALIAS			"alias"
#define	DTAB_CDEVICE			"cdevice"
#define	DTAB_BDEVICE			"bdevice"
#define	DTAB_PATHNAME			"pathname"


/* 
 * Flags:
 *	For getdev() and getdgrp():
 *		DTAB_ANDCRITERIA	Devices must meet all criteria
 *					instead of any of the criteria
 *		DTAB_EXCLUDEFLAG	The list of devices or device groups
 *					is the list that is to be excluded,
 *					not those to select from.
 *		DTAB_LISTALL		List all device groups, even those that
 *					have no valid members (getdgrp() only).
 */

#define	DTAB_ANDCRITERIA		0x01
#define	DTAB_EXCLUDEFLAG		0x02
#define	DTAB_LISTALL			0x04


/*
 * Miscellaneous Definitions
 *
 *	DTAB_MXALIASLN	Maximum alias length
 */

#define	DTAB_MXALIASLN			14

/*
 * Device Management Structure definitions
 *	reservdev	Reserved device description
 */


/*
 * struct reservdev
 *
 *	Structure describes a reserved device.
 *
 *  Elements:
 *	char   *devname		Alias of the reserved device
 *	pid_t	key		Key used to reserve the device
 */

struct reservdev{
	char   *devname;
	pid_t	key;
};

/*
 * Device Management Functions:
 *
 *	devattr()	Returns a device's attribute
 *	devreserv()	Reserves a device
 *	devfree()	Frees a reserved device
 *	reservdev()	Return list of reserved devices
 *	getdev()	Get devices that match criteria
 *	getdgrp()	Get device-groups containing devices 
 *			that match criteria
 *	listdev()	List attributes defined for a device
 *	listdgrp()	List members of a device-group
 */

	char		       *devattr(char *, char *);
	int			devfree(long, char *);
	char		      **devreserv(long, char **[]);
	char		      **getdev(char **, char **, int);
	char		      **getdgrp(char **, char **, int);
	char		      **listdev(char *);
	char		      **listdgrp(char *);
	struct reservdev      **reservdev(void);

/*
 * getvol() definitions
 */

#define	DM_BATCH	0x0001
#define DM_ELABEL	0x0002
#define DM_FORMAT	0x0004
#define DM_FORMFS	0x0008
#define DM_WLABEL	0x0010
#define DM_OLABEL	0x0020

	int			getvol(char *, char *, int, char *);	

#else /* __SVR4_1__ */		/* svr4.1 flavor */
#include <sys/types.h>
/*
 * devmgmt.h
 *
 * Contents:
 *    -	Device Management definitions,
 *    -	getvol() definitions
 */

/*
 * Device Management Definitions:
 *	- Default pathnames of Device Management data files
 *	- Device attribute names & valid values in Device Database
 *	- Flags
 *	- Miscellaneous definitions
 */

/*
 * Default pathnames of Device Management data files:
 */
/*
 * Device Database files (DDB files):
 *      - DDB_TAB = DTAB_PATH   OAM attributes of devices
 *      - DDB_DSFMAP            DSF mapping to devices
 */

#define DTAB_PATH       "/etc/device.tab"
#define DDB_TAB         DTAB_PATH
#define DDB_DSFMAP      "/etc/security/ddb/ddb_dsfmap"

/*
 * Old (saved copy) of Device Database files 
 *      - ODDB_TAB 		OAM attributes of devices
 *      - ODDB_DSFMAP            DSF mapping to devices
 */

#define ODDB_TAB	 "/etc/Odevice.tab"
#define ODDB_DSFMAP      "/etc/security/ddb/Oddb_dsfmap"

/*
 * Other Device Management files:
 *      - DGRP_PATH		Device group table
 *      - DVLK_PATH		Device reservation table
 */
#define	DGRP_PATH	"/etc/dgroup.tab"
#define	DVLK_PATH	"/etc/devlkfile"

/*
 * Names of environment variables
 *
 *	OAM_DGROUP	Name of variable that defines the pathname to
 *			the device-group table file
 *	OAM_DEVLKTAB	Name of variable that defines the pathname to
 *			the device-reservation table file
 */

#define	OAM_DGROUP			"OAM_DGROUP"
#define	OAM_DEVLKTAB			"OAM_DEVLKTAB"


/*
 * Standard device attribute names in Device Database(DDB)
 */
/* Security attributes of devices */
#define DDB_ALIAS		"alias"


/* device special file attributes of devices */
#define DDB_CDEVICE	        "cdevice"
#define DDB_BDEVICE	        "bdevice"
#define DDB_CDEVLIST		"cdevlist"
#define DDB_BDEVLIST		"bdevlist"

/* OA&M attributes of devices */
#define DTAB_ALIAS		DDB_ALIAS
#define DDB_PATHNAME		"pathname"
/* any other OA&M attrs defined here */

/*
 * Device types: defines the type of device being allocated.
 *	DEV_DSF		specified device special file(dsf) allocated
 *	DEV_ALIAS	all dsfs mapped to logical device alias allocated
 */
#define DEV_DSF		1
#define DEV_ALIAS	2

#

/* 
 * Flags:
 *	For getdev() and getdgrp():
 *		DTAB_ANDCRITERIA	Devices must meet all criteria
 *					instead of any of the criteria
 *		DTAB_EXCLUDEFLAG	The list of devices or device groups
 *					is the list that is to be excluded,
 *					not those to select from.
 *		DTAB_LISTALL		List all device groups, even those that
 *					have no valid members (getdgrp() only).
 */

#define	DTAB_ANDCRITERIA		0x01
#define	DTAB_EXCLUDEFLAG		0x02
#define	DTAB_LISTALL			0x04

/*
 * Miscellaneous Definitions
 *
 *	DDB_MAXALIAS	Maximum alias length
 *	DDB_MAGICLEN	length of magic number in DDB files
 */

#define DDB_MAXALIAS		64 + 1 /* +1 for '\0' to end the string  */
#define DDB_MAGICLEN			35
#define MAGICTOK			"MAGIC%NO"
#define DTAB_MXALIASLN			DDB_MAXALIAS


/*
 * Device Management Structure definitions
 *	reservdev	Reserved device description
 */
 
struct reservdev{
	char   *devname;	/* Alias of the reserved device */
	pid_t	key;		/* Key used to reserve the device */
};

/*
 * Device Management Functions:
 *
 *	devattr()	Returns a device's attribute
 *	devreserv()	Reserves a device
 *	devfree()	Frees a reserved device
 *	reservdev()	Return list of reserved devices
 *	getdev()	Get devices that match criteria
 *	getdgrp()	Get device-groups containing devices 
 *			that match criteria
 *	listdev()	List attributes defined for a device
 *	listdgrp()	List members of a device-group
 */

	char		       *devattr(char *, char *);
	int			devfree(long, char *);
	char		      **devreserv(long, char **[]);
	char		      **getdev(char **, char **, int);
	char		      **getdgrp(char **, char **, int);
	char		      **listdev(char *);
	char		      **listdgrp(char *);
	struct reservdev      **reservdev(void);

/*
 * getvol() definitions
 */

#define	DM_BATCH	0x0001
#define DM_ELABEL	0x0002
#define DM_FORMAT	0x0004
#define DM_FORMFS	0x0008
#define DM_WLABEL	0x0010
#define DM_OLABEL	0x0020

int			getvol(char *, char *, int, char *);	

#endif /* __SVR4_1__ */

#ifdef __cplusplus
}
#endif
#endif /* !__DEVMGMT_H__ */
