#ifndef __CLSHM_H__
#define __CLSHM_H__
/*
 * File: clshm.h
 *
 *	Header file to be included by applications in order to use
 * 	shared-memory across partitions over CrayLink.
 *
 * Copyright 1998, Silicon Graphics, Inc.
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



#if defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS)

#	include "sys/SN/SN0/arch.h"	/* for partid_t def */

/* version control defines */
#define CLSHM_DRV_MAJOR_VERS	0x01
#define CLSHM_DRV_MINOR_VERS	0x01

#define	PREFIX	('X'<<8)

/* the "ctl" ioctls below can be called only by root and only on 
 * /dev/clshmctl devices
*/
#define	CL_SHM_SET_CFG  (PREFIX | 1) /* ctl: set legonet shmem config */
#define CL_SHM_GET_CFG	(PREFIX | 2) /* ctl: get legonet shmem config */
#define	CL_SHM_STARTUP	(PREFIX | 14) /* ctl: increment open count */
#define	CL_SHM_SHUTDOWN	(PREFIX | 15) /* ctl: decrement open count */

/* look at msg.type for what is going on in clshmd.h */
#define CL_SHM_DAEM_MSG	(PREFIX | 31)

/* CL_SHM_SET_CFG, CL_SHM_GET_CFG, and CL_SHM_STARTUP */
typedef	struct clshm_config_s {	/* sysadmin configs. resources */ 
    __uint32_t	max_pages;	/* max pages to share */
    __uint32_t	page_size;	/* size of page */
    __uint32_t  major_version;	/* major version number of driver */
    __uint32_t	minor_version;	/* minor version number of driver */
    __uint32_t	clshmd_pages;	/* number pages the daemon uses */
    __uint32_t	clshmd_timeout;	/* ms time for daem to wake up */
} clshm_config_t;

/* clshm_msg_t passed into */
/* CL_SHM_DAEM_MSG */

/* nothing passed in */
/* CL_SHM_SHUTDOWN */

#endif /* C || C++ */
#endif /* ! __CLSHM_H__ */


