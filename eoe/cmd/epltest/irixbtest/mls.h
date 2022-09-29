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

#ifndef	MLS_H__
#define	MLS_H__

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

struct mlsinfo {
	int i0;                  /* Index of struct 0. */
	int i1;                  /* Index of struct 1. */
	int expect_retval;       /* Expected return val, 0 or 1. */
};

struct msen_l {
	unsigned char   msen;       
	unsigned char   level;      
	unsigned short  catcount;   
	unsigned short   catlist[10];
};

/*
 * TYPE  LEVEL CATC  --- CATEGORIES ---
 */
/* REFERENCED */
static struct msen_l msenlist[] = {
    MSA,    0,  0,  0,0,0,0,0,0,0,0,0,0, /* 0 */ 
    MSE,    0,  0,  0,0,0,0,0,0,0,0,0,0, /* 1 */ 
    MSH,    0,  0,  0,0,0,0,0,0,0,0,0,0, /* 2 */ 
    MSMH,   0,  0,  0,0,0,0,0,0,0,0,0,0, /* 3 */ 
    MSL,    0,  0,  0,0,0,0,0,0,0,0,0,0, /* 4 */ 
    MSML,   0,  0,  0,0,0,0,0,0,0,0,0,0, /* 5 */ 
    MST,    1,  0,  0,0,0,0,0,0,0,0,0,0, /* 6 */ 
    MST,    1,  2,  1,2,0,0,0,0,0,0,0,0, /* 7 */ 
    MST,    1,  2,  3,4,0,0,0,0,0,0,0,0, /* 8 */ 
    MST,    1,  4,  1,2,3,4,65534,0,0,0,0,0, /* 9 */ 
    MST,    2,  0,  0,0,0,0,0,0,0,0,0,0, /* 10 */ 
    MST,    2,  4,  1,2,3,4,65534,0,0,0,0,0, /* 11 */ 
    MSM,    1,  0,  0,0,0,0,0,0,0,0,0,0, /* 12 */ 
    MSM,    1,  2,  1,2,0,0,0,0,0,0,0,0, /* 13 */ 
    MSM,    1,  2,  3,4,0,0,0,0,0,0,0,0, /* 14 */ 
    MSM,    1,  4,  1,2,3,4,65534,0,0,0,0,0, /* 15 */ 
    MSM,    2,  0,  0,0,0,0,0,0,0,0,0,0, /* 16 */ 
    MSM,    2,  4,  1,2,3,4,65534,0,0,0,0,0, /* 17 */ 
};

/* REFERENCED */
static char *msendesc[] = {
    "MSEN_ADMIN",
    "MSEN_EQUAL",
    "MSEN_HIGH",
    "MSEN_MLDHIGH",
    "MSEN_LOW",
    "MSEN_MLDLOW",
    "MSEN_TCSEC lev:1",
    "MSEN_TCSEC lev:1 cat:1,2",
    "MSEN_TCSEC lev:1 cat:3,4",
    "MSEN_TCSEC lev:1 cat:1,2,3,4",
    "MSEN_TCSEC lev:2",
    "MSEN_TCSEC lev:2 cat:1,2,3,4",
    "MSEN_MLD lev:1",
    "MSEN_MLD lev:1 cat:1,2",
    "MSEN_MLD lev:1 cat:3,4",
    "MSEN_MLD lev:1 cat:1,2,3,4",
    "MSEN_MLD lev:2",
    "MSEN_MLD lev:2 cat:1,2,3,4",
};

struct mint_l {
	unsigned char   mint;       
	unsigned char   grade;      
	unsigned short  divcount;   
	unsigned short   divlist[10]; 
};

/*
 * TYPE  GRADE DIVC  --- DIVISIONS ---
 */
/* REFERENCED */
static struct mint_l mintlist[] = {
    MIE,    0,  0,  0,0,0,0,0,0,0,0,0,0, /* 0 */
    MIH,    0,  0,  0,0,0,0,0,0,0,0,0,0, /* 1 */
    MIL,    0,  0,  0,0,0,0,0,0,0,0,0,0, /* 2 */
    MIB,    1,  0,  0,0,0,0,0,0,0,0,0,0, /* 3 */
    MIB,    1,  2,  1,2,0,0,0,0,0,0,0,0, /* 4 */
    MIB,    1,  2,  3,4,0,0,0,0,0,0,0,0, /* 5 */
    MIB,    1,  4,  1,2,3,4,65534,0,0,0,0,0, /* 6 */
    MIB,    2,  0,  0,0,0,0,0,0,0,0,0,0, /* 7 */
    MIB,    2,  4,  1,2,3,4,65534,0,0,0,0,0, /* 8 */
};

/* REFERENCED */
static char *mintdesc[] = {
    "MINT_EQUAL",
    "MINT_HIGH",
    "MINT_LOW",
    "MINT_BIBA, grade 1",
    "MINT_BIBA, grade 1, divs 1,2",
    "MINT_BIBA, grade 1, divs 3,4",
    "MINT_BIBA, grade 1, divs 1,2,3,4",
    "MINT_BIBA, grade 2",
    "MINT_BIBA, grade 2, divs 1,2,3,4",
};

#endif	/* ifndef mls.h */
