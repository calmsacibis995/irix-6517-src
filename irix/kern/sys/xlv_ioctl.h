#ifndef __SYS_XLV_IOCTL_H__
#define __SYS_XLV_IOCTL_H__

/*
 * xlv_ioctl.h
 *
 *	Interface to get XLV volume information.
 *
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */

#ident "$Revision: 1.2 $"

#if defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS)

#ifdef __cplusplus
extern "C" {
#endif

/* 
 * Structure returned by the DIOCGETXLV ioctl() to describe the
 * subvolume geometry.
 */
#define	XLV_DISK_GEOMETRY_VERS	2		/* subvol_size == 64 bits */
typedef struct xlv_disk_geometry_s {
	__uint32_t		version;
	__uint64_t		subvol_size;	/* in blocks */
	__int32_t		trk_size;	/* in blocks */
} xlv_disk_geometry_t;

#if defined(_KERNEL)
typedef struct irix5_o32_xlv_disk_geometry_s {
	app32_uint_t		version;
	app32_long_long_t	subvol_size;	/* in blocks */
	app32_int_t		trk_size;	/* in blocks */
} irix5_o32_xlv_disk_geometry_t;

typedef struct irix5_n32_xlv_disk_geometry_s {
	app32_uint_t		version;
	app32_long_long_t	subvol_size;	/* in blocks */
	app32_int_t		trk_size;	/* in blocks */
} irix5_n32_xlv_disk_geometry_t;
#endif

/* 
 * Structure returned by the DIOCGETXLVDEV ioctl to list the 
 * subvolume device nodes in a volume.  These are external device
 * numbers.
 */
#define	XLV_GETDEV_VERS	1
typedef struct {
        __uint32_t        	version;
        dev_t           	data_subvol_dev;
        dev_t           	log_subvol_dev;
        dev_t           	rt_subvol_dev;
} xlv_getdev_t;

#ifdef __cplusplus
}
#endif

#endif /* C || C++ */

#endif /* !__SYS_XLV_IOCTL_H__ */
