/*
 * Copyright 1991, Silicon Graphics, Inc. 
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

#ifndef	MAC_H__
#define	MAC_H__

#include "irixbtest.h"
#include <sys/wait.h>

/*
 * Define short label type names to use in tests.
 */

/*
 * MSEN label type names.
 */
#define MSA        MSEN_ADMIN_LABEL	
#define MSE        MSEN_EQUAL_LABEL	
#define MSH        MSEN_HIGH_LABEL		
#define MSMH       MSEN_MLD_HIGH_LABEL	
#define MSL        MSEN_LOW_LABEL		
#define MSM        MSEN_MLD_LABEL		
#define MSML       MSEN_MLD_LOW_LABEL	
#define MST        MSEN_TCSEC_LABEL	
#define MSU        MSEN_UNKNOWN_LABEL	
/*
 * MINT label type names.
 */
#define MIB        MINT_BIBA_LABEL		
#define MIE        MINT_EQUAL_LABEL	
#define MIH        MINT_HIGH_LABEL		
#define MIL        MINT_LOW_LABEL		


/*
 * Structure used by openw_mac, openr_mac, access_mac, chmod_mac,
 * exece_mac, getlabel_mac0.
 */
struct macinfo {
	char casename[SLEN];     /* Test case name. */
	short casenum;           /* Test case number. */
	short c_flag;		 /* capability flag */
	short expect_success;    /* True if positive, false if negative. */
	int expect_errno;        /* Zero if positive, errno if negative. */
	int flag;                /* Open flag, amode, or Exec type. */
	unsigned char dir_msen;  /* Directory msen label. */
	unsigned char dir_mint;  /* Directory mint label. */
	unsigned char file_msen; /* File msen label. */ 
	unsigned char file_mint; /* File mint label. */
	unsigned char proc_msen; /* Process msen label. */
	unsigned char proc_mint; /* Process mint label. */
};


/*
 * Structure used by bsdgetpgrp_mac, bsdsetpgrp_mac, kill_mac,
 * msgctl_mac, msgget_mac, ptracer_mac, ptracew_mac, rdublk_mac,
 * setlabel_dac.
*/
struct macinfo_2 {
	char casename[SLEN];     /* Test case name. */
	short casenum;           /* Test case number. */
	short c_flag;		 /* capability flag */
	short expect_success;    /* True if positive, false if negative. */
	int expect_errno;        /* Zero if positive, errno if */
				 /* negative. */
	unsigned char O_msen;    /* Object msen label. */ 
	unsigned char O_mint;    /* Object mint label. */
	unsigned char S_msen;    /* Subject msen label. */
	unsigned char S_mint;    /* Subject mint label. */
	pid_t uid;               /* Subject & Object ruid & euid */
};

#endif /* ifndef mac.h */
