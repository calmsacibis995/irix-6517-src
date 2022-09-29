/**************************************************************************
 *									  *
 * 		 Copyright (C) 1986-1993 Silicon Graphics, Inc.	  	  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

/*
 * Device name management.
 */

#ident "$Revision: 1.6 $"
#ifdef __STDC__
	#pragma weak dev_to_devname = _dev_to_devname
	#pragma weak fdes_to_devname = _fdes_to_devname
	#pragma weak filename_to_devname = _filename_to_devname
	#pragma weak dev_to_drivername = _dev_to_drivername
	#pragma weak fdes_to_drivername = _fdes_to_drivername
	#pragma weak filename_to_drivername = _filename_to_drivername
#endif

#include "synonyms.h"
#include <sys/types.h>
#include <sys/attributes.h>
#include <sys/conf.h>
#include <sys/hwgraph.h>
#include <sys/iograph.h>
#include <sys/stat.h>
#include <invent.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Determine a canonical name for a device ID.
 * Returns pointer to device name on success; NULL if 
 * we're not able to return a name for the specified device.
 */
char *
dev_to_devname(dev_t dev, char *devname, int *length)
{
	char handle_name[40];

	sprintf(handle_name, "%s.devhdl/%u/", _PATH_HWGFS, dev);
	return(filename_to_devname(handle_name, devname, length));
}


/*
 * Given a file descriptor of a special file, return the corresponding
 * device name.
 */
char *
fdes_to_devname(int fdes, char *devname, int *length)
{
	int dev_length;
	int error;

	dev_length = *length-1;
	if (dev_length <= 0)
		return(NULL);

	error = attr_getf(fdes, _DEVNAME_ATTR, 
			devname, &dev_length, 0);

	if (error)
		return(NULL);

	devname[dev_length] = '\0';
	*length = dev_length+1;

	return(devname);
}


/*
 * Given a file name of a special file, return the corresponding
 * device name.
 */
char *
filename_to_devname(char *filename, char *devname, int *length)
{
	int dev_length;
	int error;

	/* dev_length should be a minimum of 0.
	 * We don't bail if 0 because we may still want to
	 * return the required length of the device name.
	 */
	dev_length = (*length <= 1) ? 0 : (*length-1);
	
	/* Get the canonical file name of the special file */
	error = attr_get(filename, _DEVNAME_ATTR, 
			devname, &dev_length, 0);

	/* Set the required length */
	*length = dev_length + 1;

	/* If something went wrong return a NULL */
	if (error)
		return(NULL);

	/* NULL-terminate the string */
	devname[dev_length] = '\0';

	return(devname);
}

/*
 * Given a device, return the name of the device driver that controls
 * the corresponding device.
 */
char * 
dev_to_drivername(dev_t dev, char *drivername, int *length)
{
	char handle_name[40];

	sprintf(handle_name, "%s.devhdl/%u/", _PATH_HWGFS, dev);
	return(filename_to_drivername(handle_name, drivername, length));
}

/*
 * Given a file descriptor of a special file, return the name of the
 * device driver that controls the corresponding device.
 */
char * 
fdes_to_drivername(int fdes, char *drivername, int *length)
{
	int driver_length;
	int error;

	driver_length = *length - 1;
	if (driver_length <= 0)
		return(NULL);

	error = attr_getf(fdes, _DRIVERNAME_ATTR, 
			drivername, &driver_length, 0);

	if (error)
		return(NULL);

	drivername[driver_length] = '\0';
	*length = driver_length + 1;

	return(drivername);
}

/*
 * Given the name of a special file, return the name of the device driver
 * that controls the corresponding device.
 */
char * 
filename_to_drivername(char *filename, char *drivername, int *length)
{
	int driver_length;
	int error;

	driver_length = *length - 1;
	if (driver_length <= 0)
		return(NULL);

	error = attr_get(filename, _DRIVERNAME_ATTR, 
			drivername, &driver_length, 0);

	if (error)
		return(NULL);

	drivername[driver_length] = '\0';
	*length = driver_length + 1;

	return(drivername);
}
