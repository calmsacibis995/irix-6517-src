/************************************************************************
 * Copyright 1995, Silicon Graphics, Inc.				*
 * ALL RIGHTS RESERVED							*
 *									*
 * UNPUBLISHED -- Rights reserved under the copyright laws of the 	*
 * United States.   Use of a copyright notice is precautionary only 	*
 * and does not imply publication or disclosure.			*
 *									*
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:				*
 * Use, duplication or disclosure by the Government is subject to 	*
 * restrictions as set forth in FAR 52.227.19(c)(2) or subparagraph 	*
 * (c)(1)(ii) of the Rights in Technical Data and Computer Software 	*
 * clause at DFARS 252.227-7013 and/or in similar or successor 		*
 * clauses  in the FAR, or the DOD or NASA FAR Supplement.  		*
 * Contractor/manufacturer is Silicon Graphics, Inc., 			*
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.		*
 *									*
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY	*
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,	*
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS 	*
 * STRICTLY PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF 	*
 * SILICON GRAPHICS, INC.						*
 ************************************************************************/

#ifndef _SYS_SN_KLPARTVAR_H
#define _SYS_SN_KLPARTVAR_H 1

/*
 * VERSION indicates local version number of parameter area. 
 */

#define	_PV_VERSION(_major, _minor)	(((_major) << 8) | (_minor))
#define	PV_VERSION_MAJOR(_v)		((_v) >> 8)
#define	PV_VERSION_MINOR(_v)		((_v) & 0xff)

#define	PV_VERSION_COMP(_v1, _v2)	\
    (PV_VERSION_MAJOR(_v1) == PV_VERSION_MAJOR(_v2))

/*
 * Typedef: part_var_s
 * Purpose: Maps out region  of memory pointed to by the KLDIR XP
 *	    entry.
 * Notes:   This KLDIR entry is filled in and modified by the kernel.
 *	       
 * Versions: Different major version numbers are ALWYAS incompatible, 
 *	     different minor version numbers are compatible if major 
 *	     numbers are the same.
 */
typedef struct part_var_s {
    unsigned short	pv_version;	/* Local Version */
#   define		PV_VERSION _PV_VERSION(1,0)
    char		pv_pad[2];
    __uint32_t		pv_size;	/* Total size  */
    __uint32_t		pv_count;	/* # varables */
    __psunsigned_t	pv_vars[32];	/* Variable - initialized to 0 */
} part_var_t;

#define	PART_VAR_STATE	0		/* Current State */
#define	PART_VAR_HB	1		/* Partition heartbeat */
#define	PART_VAR_HB_FQ	2		/* Partition heartbeat frequency*/
#define	PART_VAR_HB_PM	3		/* Partition heartbeat mask */
#define	PART_VAR_XPC	4		/* XPC variable */

#define	PART_VAR_LIMIT	PART_VAR_XPC	/* Max variable # */

/* PART_VAR_STATE values */

#define	PV_STATE_INVALID	0	/* Invalid state */
#define	PV_STATE_RUNNING	1	/* Normal operation */
#define	PV_STATE_STOPPED	2	/* Stopped - breakpoint */

#endif /* _SYS_SN_KLPARTVAR_H */
