
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

#ident "$Revision: 1.10 $"

/* lvinit: initializes the Logical volume driver */

#include <sys/types.h>
#include <sys/dkio.h>
#include <sys/dvh.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/lv.h>
#include "lvutils.h"



extern int errno;
struct volmember memhead = {0};
struct logvol    *loghead = NULL;
int readonly = 1;
char *progname;
int error = 0;
char *fname = "/etc/lvtab";


void vol_init();
struct logvol *devnametovol();

main(argc, argv)
int argc;
char **argv;
{
register struct logvol *vol;
register int i;
extern int optind;
extern char *optarg;

	progname = argv[0];

	if (getuid() != 0)
	{
		fprintf(stderr,"must be superuser to run %s\n", progname);
		exit(-1);
	}

	while ((i = getopt(argc, argv, "l:")) != EOF)
	{
		switch (i)
		{
		case 'l':
			fname = optarg;
			break;
		default:
			usage();
		};
	}

	importlvtab(fname);
	argc -= optind;
	if (argc == 0)
	{
		vol = loghead;
		while (vol)
		{
			vol_init(vol);
			vol = vol->next;
		}
	}
	else
	{
		for (i = optind; i < (argc + optind); i++)
		{
			vol = devnametovol(argv[i]);
			if (!vol)
			{
				fprintf(stderr, "%s not found in lvtab file %s\n", argv[i], fname);
				error = -1;
				continue;
			}
			else vol_init(vol);
			}
		}
	exit(error);
}

void
vol_init(vol)
register struct logvol *vol;
{
char namebuf[MAXDEVPATHLEN];
int	lvfd;
struct lvioarg arg;
int	err = 0;

	if (!(vol->status & L_GOODTABENT))
	{
		fprintf(stderr,"lvinit: bad lvtab entry for %s\n",
			vol->tabent->devname);
		error = -1;
		return;
	}

	volmemsetup(vol);
	if (checkvolpaths(vol) == -1)
		err++;
	if (getvolheads(vol) == -1)
		err++;
	if (getlvlabs(vol) == -1)
		err++;

	/* Conversion for long dev_t: */

	if (lv_dev_conv(vol))
		return;

	if (err)
		goto initerr;

	/* Now that we have the labels, we can check that the devices are
	 * connected correctly, ie that their current devs agree with
	 * the devs recorded for them in their labels when they were
	 * initialized.
	 */

	check_dev_connections(1);

	vol_labels_check(vol);
	vol_devs_check(vol);

	if (vol->status != (L_GOODTABENT | L_GOTALL_LABELS | L_GOODLABELS | L_DEVSCHEKD))
		goto initerr;

	if (vol_to_tab_check(vol, COMPLETE_VOL)) 
	{
		error = -1;
		return;
	}

	/*
	 * lvdev_init will return -1 for serious errors and 1 if
	 * the volume is already initialized.  Only override the
	 * the value of error, which is the return value of lvinit,
	 * if the error returned from lvdev_init is worse than
	 * the current value of error.
	 */
	if ((err = lvdev_init(vol, &vol->vmem[0]->lvlab->lvdescrip, 0)) != 0)
	{
		if (error != -1)
		{
			error = err;
		}
	}
	return;

initerr:
	(void)printbadvol(vol, stderr, EXISTING_VOLUME);
	error = -1;
}

usage()
{
	fprintf(stderr,"usage: lvinit [-l lvtabname] [lvnames]\n");
	exit(1);
}

/* Conversion for old dev_t. We check for an old magic number; if found,
 * we will fire up a "mklv -f" to remake the volume newstyle.
 *
 * This is quick 'n dirty; in the best of all possible worlds we'd do
 * a lot of checking. But this should only ever happen once in the life
 * of a volume, when sherwood is first run.
 */

char cmdbuf[100];

lv_dev_conv(vol)
register struct logvol *vol;
{
int i;

	for (i = 0; i < vol->tabent->ndevs; i++)
		if (vol->vmem[i]->lvlab &&
		    (vol->vmem[i]->lvlab->lvmagic == LV_OMAGIC)) goto fixit;
		
	return (0);
fixit:
	bzero(cmdbuf, 100);
	sprintf(cmdbuf, "/etc/mklv -f -l %s %s\n", fname, vol->tabent->devname);
	if (system(cmdbuf))
		fprintf(stderr,"Warning: conversion of logical volume %s failed.\n", vol->tabent->devname);
	return(1);
}

