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

#ifndef	LBL_H__
#define	LBL_H__

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
* Structure used by getlabel_lbl0 and setlabel_lbl0. 
*/
struct label_info {
	char *casename;        /* Test case name. */
	short casenum;              /* Test case number. */
	unsigned char obj_msen;     /* Obj msen type. */ 
	unsigned char obj_level;    /* Obj msen level. */ 
	unsigned short obj_catcount; /* Obj msen catcount. */ 
	unsigned short obj_catlist[10]; /* Obj cats. */  
	unsigned char obj_mint;     /* Obj mint type. */
	unsigned char obj_grade;    /* Obj mint grade. */ 
	unsigned short obj_divcount; /* Obj mint divcount. */ 
	unsigned short obj_divlist[10]; /* Obj divs. */  
	int expect_success;
	int expect_errno;
};


/*
* Structure used by setlabel_lbl1. 
*/
struct err_info {
	char *casename;        /* Test case name. */
	short casenum;         /* Test case number. */
	int expect_errno;
	short use_symlink;     /* For ENOTDIR test */
};


/* Test values exceeding boundaries in mac_label.h */

#define MSEN_TOOHIGH    '['
#define MSEN_TOOLOW     '@'
#define MINT_TOOHIGH    '{'
#define MINT_TOOLOW     '`'

#endif /* end lbl.h */
