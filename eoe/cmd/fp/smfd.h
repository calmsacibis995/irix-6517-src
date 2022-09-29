/**************************************************************************
 *									  *
 * 		 Copyright (C) 1986, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

/*	This is the header file for the driver for the Scientific Micro
	Systems SCSI floppy.  Many structures are shared with dksc.h so
	so that programs won't have to be changed.  Some fields will
	obviously be ignored.
	Driver first created by Dave Olson, 6/88.
*/
#ident "$Revision: 1.3 $"


/* SCSI disk minor # breakdown (for floppies, we use partition
 * to indicate type of floppy; partitions aren't supported).
 *
 *	  7     6    5          4              3     2     1     0
 *	++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *	+               +                  +                       +
 *	+  Target  ID   +  Logical unit #  +      Partition        +
 *	+               +                  +     (floppy type)     +
 *	++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
*/


#ifndef SMFD_H
#define SMFD_H

#define _SMFDIOC_(x)	(('f'<<8) | x)

/* values returned by smfd_type(), used for table indices, etc */
#define FD_FLOP      0	 /* 360K (48tpi) */
#define FD_FLOP_DB   1	 /* 720K (96tpi,9sec/trk) */
#define FD_FLOP_AT   2	 /* 1.2M (96tpi,15sec/trk) (PC-AT) */
#define FD_FLOP_35_800K 3/* 'low density' 3.5" floppy (800K) */
#define FD_FLOP_35LO 4	 /* 'low density' 3.5" floppy (720K) */
#define FD_FLOP_35   5	 /* 'high density' 3.5" floppy (1.44 Mb) */
#define FD_FLOP_35_20M 6 /* 3.5" 'floptical' (20MB) */
#define FD_FLOP_GENERIC 7 /* any removable media other that above types (pc
                            cards) */

#define SMFD_MAX_TYPES 7	/* use minor # to indicate type */
#define SMFD_TYPE_MASK 0xf

#undef	smfd_type
#define	smfd_type(minor)	(minor & SMFD_TYPE_MASK)

#define MAXFLOPS 1	/* number of floppy drives supported */

#define SMFDSETMODE	_SMFDIOC_(0)	/* set spt, cyls, tpi, etc */
#define SMFDREQSENSE	_SMFDIOC_(1)	/* return last sense key */
#define SMFDRECAL	_SMFDIOC_(2)	/* do a recal on the drive */
#define SMFDEJECT	_SMFDIOC_(3)	/* eject floppy disk, return EINVAL */
					/* if software eject not supported */
#define SMFDMEDIA	_SMFDIOC_(4)	/* return media status */
					/* ioctl(smfd, SMFDMEDIA, int *) */
#define	SMFDMEDIA_READY		(1 << 0)	/* media in drive */
#define	SMFDMEDIA_CHANGED	(1 << 1)	/* media changed */
#define	SMFDMEDIA_WRITE_PROT	(1 << 2)	/* media is write protected */

#define	SMFDNORETRY	_SMFDIOC_(5)	/* no retry on reset/media change during
					   read/write */

#endif
