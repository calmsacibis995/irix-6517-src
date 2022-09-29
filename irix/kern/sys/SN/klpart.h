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

#ifndef	_SYS_SN_KLPART_H
#define	_SYS_SN_KLPART_H 1

/*
 * VERSION indicates local version number of parameter area. 
 */

#define	_KLP_VERSION(_major, _minor)	(((_major) << 8) | (_minor))
#define	KLP_VERSION_MAJOR(_v)		((_v) >> 8)
#define	KLP_VERSION_MINOR(_v)		((_v) & 0xff)

#define	KLP_VERSION_COMP(_v1, _v2)	\
    (KLP_VERSION_MAJOR(_v1) == KLP_VERSION_MAJOR(_v2))

/*
 * Typedef: klp_t 
 * Purpose: Maps out the format of the KLDIR entry for partitioning. 
 * NOTES:   This structure currently fits in the tail of the KLDIR entry.
 *	    If you change it, be sure it still fits, and change version
 *	    number. The KLDIR entry "pointer" field must be set to point to 
 *	    this structure, even when in tail. It may be moved out
 *	    later if required.
 * 
 *	    This KLDIR entry is filled in by the prom, and the kernel
 *	    updates the "state" to "KERNEL".
 *	       
 * Versions: Different major version numbers are ALWAYS incompatible, 
 *	     different minor version numbers are compatible if major
 *	     numbers is the same.
 */
typedef struct klp_s {
    unsigned short 	klp_version;	/* Current version */
#   define		KLP_VERSION	_KLP_VERSION(1,0) /* 1.0  */
    unsigned char	klp_state;	/* Current partition state (below) */
    unsigned 		klp_cpuA:1;	/* cpu A installed and enabled */
    unsigned		klp_cpuB:1;	/* cpu B installed and enabled */
    unsigned		klp_cpuC:1;	/* cpu C installed and enabled */
    unsigned		klp_cpuD:1;	/* cpu D installed and enabled */
    unsigned			:4;	/* resrved, must be 0 */
    unsigned char	klp_partid;	/* partition ID */
    unsigned char	klp_domainid;	/* domain ID */
    unsigned short	klp_cluster;	/* cluster ID */
    unsigned short	klp_cellid;	/* cell ID */
    unsigned short	klp_module;	/* module ID */
    unsigned short      klp_ncells;	/* # cells participating */
    unsigned short	klp_rsvd;	/* Reserved - 0 */
} klp_t;

#define	KLP_STATE_INVALID	0	/* Nothing known */
#define	KLP_STATE_PROM		1	/* In prom, fields valid */
#define	KLP_STATE_PROM_BOOT	2	/* In prom, requesting boot */
#define	KLP_STATE_LAUNCHED	3	/* Prom launched into guest */
#define	KLP_STATE_KERNEL	4	/* In kernel */

#define	KLP_STATE_VALID(_s)	\
    (((_s) > KLP_STATE_INVALID) && ((_s) <= KLP_STATE_KERNEL))

#endif /* _SYS_SN_KLPART_H */
