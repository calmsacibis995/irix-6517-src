/*
 * Copyright (c) 1995 Spider Systems Limited
 *
 * This Source Code is furnished under Licence, and may not be
 * copied or distributed without express written agreement.
 *
 * All rights reserved.  Made in Scotland.
 *
 * Authors: Fraser Moore, George Wilkie
 *
 * frsctl.c of sfr module
 *
 * SpiderFRAME-RELAY
 * Release 1.0.3 95/06/15
 * 
 * 
 */

/*
 * FRS low level control functions
 */

#include <sys/types.h>
#include <stropts.h>
#include <streamio.h>
#include "frsctl.h"
#include <malloc.h>
 
#ifndef TIMEOUT
#define TIMEOUT 60
#endif

/*
 * frs_set_snid() - sends FR_SETSNID ioctl to FRS driver
 *
 * This function sends the FR_SETSNID ioctl to the FRS driver.  It 
 * constructs the ioctl from the ppaval and the mux value.
 *
 *	fd:		FRS file descriptor.
 *
 *	mux:		Lower stream link identifier.
 *
 *	snid:		SNID value to associated with the lower stream.
 *
 * Returns: An integer which is the I_STR ioctl return value.
 *
 */

int
frs_set_snid(int fd, int mux, uint32 snid)
{
	struct strioctl sioptr;
	struct fr_snid_ioc iocptr;

	sioptr.ic_cmd = FR_SETSNID;
	sioptr.ic_len = sizeof( struct fr_snid_ioc );
	sioptr.ic_timout = TIMEOUT;
	sioptr.ic_dp = (char *)&iocptr;

	iocptr.fr_snid_ppa = snid;
	iocptr.fr_snid_index = mux;

	return(S_IOCTL(fd, I_STR, (char *)&sioptr));
}

/*
 * frs_get_ppa_stats() - gets status report for a PPA
 *
 * This function gets the statistics for a given PPA.  It sends an
 * FR_GET_PPASTATS ioctl message to the driver.  It then prints the
 * data to stdout.
 *
 *	frsfd:		A value associated with the file descriptor for
 *			the open stream to the frs driver.
 *
 *	ppaval:		The PPA whose stats are to be retrieved.
 *
 * Returns : An integer which is the ioctl return value
 *
 */

int frs_get_ppa_stats(int frsfd, uint32 ppaval, struct fr_ppa_stats *stats)
{
	struct fr_ppastats_ioc iocptr;
	struct strioctl sioptr;

	sioptr.ic_cmd = FR_GET_PPASTATS;
	sioptr.ic_len = sizeof( struct fr_ppastats_ioc );
	sioptr.ic_timout = TIMEOUT;
	sioptr.ic_dp = (char *)&iocptr;

	iocptr.fr_ppastats_ppa = ppaval;

	if ( S_IOCTL( frsfd, I_STR, &sioptr ) < 0 )
		return (-1);

	*stats = iocptr.fr_ppa;		/* struct copy */
	return 0;
}

/*
 * frs_zero_ppa_stats() - clears all status variables for a PPA
 *
 * This function clears all the stats variables for a given PPA.  It
 * sends down an FR_ZERO_PPASTATS ioctl message to the driver.
 *
 *	frsfd:		A value associated with the file descriptor for
 *			the open stream to the frs driver.
 *
 *	ppaval:		The PPA whose stats are to be cleared.
 *
 * Returns: An integer which is the ioctl return value
 *
 */

int frs_zero_ppa_stats(int frsfd, uint32 ppaval)
{
	struct fr_ppastats_ioc iocptr;
	struct strioctl sioptr;

	sioptr.ic_cmd = FR_ZERO_PPASTATS;
	sioptr.ic_len = sizeof( struct fr_ppastats_ioc );
	sioptr.ic_timout = TIMEOUT;
	sioptr.ic_dp  = (char *)&iocptr;

	iocptr.fr_ppastats_ppa = ppaval;

	return S_IOCTL(frsfd, I_STR, &sioptr);
}


/*
 * frs_get_pvc_stats() - gets status report for a DLCI
 *
 * This function gets the statistics for a given PPA and DLCI.  It sends
 * down an FR_GET_PVCSTATS ioctl message to the driver.  It then prints 
 * the values to stdout.
 *
 *	frsfd:		A value associated with the file descriptor for
 *			the open stream to the frs driver.
 *
 *	ppaval:		The PPA which the DLCI belongs to.
 *
 *	dlcival:	The DLCI whose stats are being retrieved.
 *
 * Returns: An integer which is the ioctl return value
 *
 */

int frs_get_pvc_stats(int frsfd, uint32 ppaval, uint32 dlcival, struct fr_pvc_stats *stats)
{
	struct fr_pvcstats_ioc iocptr;
	struct strioctl sioptr;

	sioptr.ic_cmd = FR_GET_PVCSTATS;
	sioptr.ic_len = sizeof( struct fr_pvcstats_ioc );
	sioptr.ic_timout = TIMEOUT;
	sioptr.ic_dp = (char *)&iocptr;

	iocptr.fr_pvcstats_ppa = ppaval;
	iocptr.fr_pvcstats_dlci = dlcival;

	if ( S_IOCTL(frsfd, I_STR, (char *)&sioptr) < 0 )
		return -1;

	*stats = iocptr.fr_pvc;		/* struct copy */
	return 0;
}


/*
 * frs_zero_pvc_stats() - clears all status variables for a DLCI
 *
 * This function clears the stats variables for a given PPA and DLCI.
 * It sends down an FR_ZERO_PVCSTATS ioctl message to the driver.
 *
 *	frsfd:		A value associated with the file descriptor for
 *			the open stream to the frs driver.
 *
 *	ppaval:		The PPA which the DLCI belongs to.
 *
 *	dlcival:	The DLCI whose stats are being cleared.
 *
 * Returns: An integer which is the ioctl return value
 *
 */

int frs_zero_pvc_stats(int frsfd, uint32 ppaval, uint32 dlcival)
{
	struct fr_pvcstats_ioc iocptr;
	struct strioctl sioptr;

	sioptr.ic_cmd = FR_ZERO_PVCSTATS;
	sioptr.ic_len = sizeof( struct fr_pvcstats_ioc );
	sioptr.ic_timout = TIMEOUT;
	sioptr.ic_dp = (char *)&iocptr;

	iocptr.fr_pvcstats_ppa = ppaval;
	iocptr.fr_pvcstats_dlci = dlcival;
	return S_IOCTL(frsfd, I_STR, (char *)&sioptr);
}


/*
 * frs_get_ppa_status() - gets an PPA status report
 *
 * This function gets a status report for a given PPA value.  It sends
 * a FR_GETSTATUS ioctl message to the frs driver.  It then prints the
 * status report to stdout.
 *
 *	ppaval:		A value associated with the PPA.
 *
 *	frsfd:		A value associated with the file descriptor
 *			for the stream to the frs driver.
 *
 * Returns: An integer which is the ioctl return value
 */

int frs_get_ppa_status(int frsfd, uint32 ppaval, uint32 *netstat, uint32 *count, 
unsigned long  **val)
{
	struct fr_status_ioc iocptr;
	struct strioctl sioptr;
	char *data;
	int size;

	sioptr.ic_cmd = FR_GETSTATUS;
	sioptr.ic_len = sizeof( struct fr_status_ioc );
	sioptr.ic_timout = TIMEOUT;
	sioptr.ic_dp = (char *)&iocptr;

	iocptr.fr_status_ppa = ppaval;

	if ( S_IOCTL(frsfd, I_STR, (char *)&sioptr) < 0 )
		return -1;

	*netstat = iocptr.fr_status_netstat;
	*count = iocptr.fr_status_pvccount;

	if ( *count == 0 )
		return 0;

	size = (*count) * sizeof(unsigned long);

	/* allocate buffer space */
	data = malloc(size);

	if ( S_READ(frsfd, data, size) < 0 )
	{
		free(data);
		return -1;
	}

	*val = (unsigned long *)data;
	return 0;
}

/*
 * frs_setctune() - sends the tuning parameters to the frs driver
 * 
 * This function sends the tuning parameters to the frs driver.  It fills
 * in all the relevant fields of the strioctl structure and copies the
 * parameters from the tp structure.
 *
 *	ppaval:		The ppa value for the tuning parameters.
 *
 *	dlcival:	The dlci value for the tuning parameters.
 *
 *	tp:		A pointer to a structure which holds all the tuning
 *			parameters read from the template file.
 *
 *	frsfd:		A value associated with the file descriptor for
 *			the open stream to the frs driver.
 *
 * Return: int
 *
 */

int frs_setctune(int frsfd, uint32 ppaval, uint32 dlcival, struct fr_pvc_tune *tp)
{
	struct fr_pvc_ioc iocptr;
	struct strioctl sioptr;

	sioptr.ic_cmd = FR_SETCTUNE;
	sioptr.ic_len = sizeof( struct fr_pvc_ioc );
	sioptr.ic_timout = TIMEOUT;
	sioptr.ic_dp = (char *)&iocptr;

	iocptr.fr_pvc_ppa = ppaval;
	iocptr.fr_pvc_dlci = dlcival;
	iocptr.fr_pvc = *tp;		/* struct copy */

	return S_IOCTL(frsfd, I_STR, (char *)&sioptr);
}


/*
 * frs_getctune() - gets dlci values from the frs driver
 *
 * This function gets the dlci tuner values for a particular dlci.
 * When it has recieved the values from the driver it calls
 * print_dlci_vals() to display the values.
 *
 *	ppaval:		The value of the PPA whose tuning parameters
 *			are required.
 *
 *	dlcivals:	The value of the dlci whose tuning paramters
 *			are reuired.
 *
 *	frsfd:		A value associated with the file descriptor for
 *			the open stream to the frs driver.
 *
 * Returns: int
 *
 */

int frs_getctune(int frsfd, uint32 ppaval, uint32 dlcival, struct fr_pvc_tune
*tp)
{
	struct fr_pvc_ioc iocptr;
	struct strioctl sioptr;

	sioptr.ic_cmd = FR_GETCTUNE;
	sioptr.ic_len = sizeof( struct fr_pvc_ioc );
	sioptr.ic_timout = 0;
	sioptr.ic_dp = (char *)&iocptr;

	iocptr.fr_pvc_ppa = ppaval;
	iocptr.fr_pvc_dlci = dlcival;

	if ( S_IOCTL(frsfd, I_STR, (char *)&sioptr) < 0 )
		return -1;
	
	*tp = iocptr.fr_pvc;	/* struct copy */
	return 0;
}


/*
 * frs_dereg_pvc() - deregisters an DLCI
 *
 * This function deregisters a given DLCI.  It sends an FR_DEREGPVC
 * ioctl message to the frs driver.
 *
 *	ppaval:		A value associated with the PPA which the
 *			DLCI is registered to.
 *
 *	dlcival:	A value associated with the DLCI to be 
 *			deregistered.
 *
 *	frsfd:		A value associated with the file descriptor for
 *			the open stream to the frs driver.
 *
 * Returns: void
 */


int frs_dereg_pvc(int frsfd, uint32 ppaval, uint32 dlcival)
{
	struct fr_deregpvc_ioc iocptr;
	struct strioctl sioptr;

	sioptr.ic_cmd = FR_DEREGPVC;
	sioptr.ic_len = sizeof( struct fr_pvc_ioc );
	sioptr.ic_timout = 0;
	sioptr.ic_dp  = (char *)&iocptr;

	iocptr.fr_deregpvc_ppa = ppaval;
	iocptr.fr_deregpvc_dlci = dlcival;

	return S_IOCTL(frsfd, I_STR, (char *)&sioptr);
}

/*
 * frs_settune() - sends the tuning parameters to the frs driver
 * 
 * This function sends the tuning parameters to the frs driver.  It fills
 * in all the relevant fields of the strioctl structure and copies the
 * parameters from the tp structure.
 *
 *	ppaval:		The ppa value for the tuning parameters.
 *
 *	lmistyle:	This inidicates whether the driver is acting as the
 *			network or the user.
 *
 *	tp:		A pointer to a structure which holds all the tuning
 *			parameters read from the template file.
 *
 *	frsfd:		A value associated with the file descriptor for
 *			the open stream to the frs driver.
 *
 * Return: int
 *
 */

int frs_settune(int frsfd, uint32 ppaval, struct fr_ppa_tune *tp)
{
	struct fr_ppa_ioc iocptr;
	struct strioctl sioptr;

	sioptr.ic_cmd = FR_SETTUNE;
	sioptr.ic_len = sizeof( struct fr_ppa_ioc );
	sioptr.ic_timout = 0;
	sioptr.ic_dp = (char *)&iocptr;
	
	iocptr.fr_ppa_ppa = ppaval;
	iocptr.fr_ppa = *tp;		/* struct copy */

	return S_IOCTL(frsfd, I_STR, &sioptr);
}


/*
 * frs_gettune() - gets ppa values from the frs driver
 *
 * This function gets the ppa tuner values for a particular ppa.
 * When it has recieved the values from the driver it calls
 * print_ppa_vals() to display the values.
 *
 *	ppaval:		The value of the PPA whose tuning parameters
 *			are required.
 *
 *	frsfd:		A value associated with the file descriptor for
 *			the open stream to the frs driver.
 *
 * Returns: int
 *
 */

int frs_gettune(int frsfd, uint32 ppaval, struct fr_ppa_tune *tp)
{
	struct fr_ppa_ioc iocptr;
	struct strioctl sioptr;

	sioptr.ic_cmd = FR_GETTUNE;
	sioptr.ic_len = sizeof( struct fr_ppa_ioc );
	sioptr.ic_timout = TIMEOUT;
	sioptr.ic_dp = (char *)&iocptr;

	iocptr.fr_ppa_ppa = ppaval;

	if ( S_IOCTL(frsfd, I_STR, &sioptr) < 0 )
		return -1;
	*tp = iocptr.fr_ppa;	/* struct copy */
	return 0;
}
