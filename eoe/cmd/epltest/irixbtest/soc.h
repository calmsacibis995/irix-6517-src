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

#ifndef	SOC_H__
#define	SOC_H__

#include "irixbtest.h"
#include <sys/socket.h>
#include <sys/so_dac.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <net/soioctl.h>
#include <net/if.h>
#include <sys/t6attrs.h>

#define JUNK     9999

/*
 * UNIX socket name size
 */
#define SASIZE 14
/*
 * Short MSEN label type names.
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
 * Short MINT label type names.
 */
#define MIB        MINT_BIBA_LABEL		
#define MIE        MINT_EQUAL_LABEL	
#define MIH        MINT_HIGH_LABEL		
#define MIL        MINT_LOW_LABEL		

/* 
 * Structures and arrays used to pass label contents 
 * and description to socket test routines.
 */

struct msen_l {
	unsigned char   msen;       
	unsigned char   level;      
	unsigned short  catcount;   
	unsigned short  catlist[10];
};

#define    NMSEN      9

/*
 * MSEN  LEVEL CATC  --- CATEGORIES ---
 */
/* REFERENCED */
static struct msen_l msenlist[] = {
    MSA,    0,  0,  0,0,0,0,0, 0,0,0,0,0, 
    MSH,    0,  0,  0,0,0,0,0, 0,0,0,0,0, 
    MSMH,   0,  0,  0,0,0,0,0, 0,0,0,0,0, 
    MSL,    0,  0,  0,0,0,0,0, 0,0,0,0,0, 
    MSML,   0,  0,  0,0,0,0,0, 0,0,0,0,0, 
    MST,  200, 10,  0,1,2,3,4, 5,6,7,8,9,
    MST,    1,  3,  16000, 60000, 65534, 0, 0,   0, 0, 0, 0, 0,
    MSM,  200, 10,  0,1,2,3,4, 5,6,7,8,9,
    MSM,    1,  3,  50000, 55000, 65534, 0, 0,   0, 0, 0, 0, 0,
};

/* REFERENCED */
static char *msendesc[] = {
    "MSEN_ADMIN",
    "MSEN_HIGH",
    "MSEN_MLDHIGH",
    "MSEN_LOW",
    "MSEN_MLDLOW",
    "MSEN_TCSEC, 10 Cats",
    "MSEN_TCSEC, 3 Cats",
    "MSEN_MLD, 10 Cats",
    "MSEN_MLD, 3 Cats",
};

struct mint_l {
	unsigned char   mint;       
	unsigned char   grade;      
	unsigned short  divcount;   
	unsigned short  divlist[10]; 
};

#define    NMINT      5 

/*
 * MINT  GRADE DIVC  --- DIVISIONS ---
 */
/* REFERENCED */
static struct mint_l mintlist[] = {
    MIH,    0,  0,  0,0,0,0,0, 0,0,0,0,0, /* 1 */
    MIL,    0,  0,  0,0,0,0,0, 0,0,0,0,0, /* 2 */
    MIB,  200, 10,  0,1,2,3,4, 5,6,7,8,9,
    MIB,    1,  3,  30000, 50000, 65533, 0, 0,  0, 0, 0, 0, 0,
    MIE,    0,  0,  0,0,0,0,0, 0,0,0,0,0,
};

/* REFERENCED */
static char *mintdesc[] = {
    "MINT_HIGH",
    "MINT_LOW",
    "MINT_BIBA, 10 DIVS",
    "MINT_BIBA, 3 DIVS",
    "MINT_EQUAL",
};

/*
 * Other socket information.
 */
#define    NDOMAINS   2 
#define    NTYPES     2 
#define    NUIDS      2 

/* REFERENCED */
static int   domain[] = { AF_INET, AF_UNIX };
/* REFERENCED */
static int   type[] = { SOCK_STREAM, SOCK_DGRAM };
/* REFERENCED */
static uid_t uid[] = { SUPER, TEST0 };

/* REFERENCED */
static char *domaindesc[] = { "INET" ,"UNIX" };
/* REFERENCED */
static char *typedesc[] = { "STREAM", "DGRAM" };
/* REFERENCED */
static char *uiddesc[] = { "super", "not super" };

/*
 * This set used only by lbl_set_soc() for ioctl call.
 */
#define NSMSEN 6

/*
 * MSEN  LEVEL CATC  --- CATEGORIES ---
 */
/* REFERENCED */
static struct msen_l smsenlist[] = {
    MSA,    0,  0,  0,0,0,0,0, 0,0,0,0,0, 
    MSH,    0,  0,  0,0,0,0,0, 0,0,0,0,0, 
    MSL,    0,  0,  0,0,0,0,0, 0,0,0,0,0, 
    MST,  200, 10,  0,1,2,3,4, 5,6,7,8,9,
    MST,    1,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    MSE,    0,  0,  0,0,0,0,0,0,0,0,0,0 
};

/* REFERENCED */
static char *smsendesc[] = {
    "MSEN_ADMIN",
    "MSEN_HIGH",
    "MSEN_LOW",
    "MSEN_TCSEC, Level&CATS",
    "MSEN_TCSEC, Level",
    "MSEN_EQUAL",
};

/*
 * These arrays used only by mac_trans_soc()
 */

struct msen_trans_l {
	unsigned char   msen;       
	unsigned char   level;      
	unsigned short  catcount;   
	unsigned short  catlist[16];
};

struct mint_trans_l {
	unsigned char   mint;       
	unsigned char   grade;      
	unsigned short  divcount;   
	unsigned short  divlist[16]; 
};

struct msen_iflabel_l {
	unsigned char	msen_max, level_max;
	unsigned short  catcount_max, catlist_max[16];
	unsigned char	msen_min, level_min;
	unsigned short  catcount_min, catlist_min[16];
};

struct mint_iflabel_l {
	unsigned char	mint_max, grade_max;
	unsigned short  divcount_max, divlist_max[16];
	unsigned char	mint_min, grade_min;
	unsigned short  divcount_min, divlist_min[16];
};

struct so_minfo {
	short cmdindex;       /* Index of iflabelcmd[] to use */
	short msenindex;      /* Index of msen_tlist[] to use */
	short mintindex;      /* Index of mint_tlist[] to use */
	short expect_success; /* True if case is positive. */
	int expect_errno;     /* ERRNO if case is neg, 0 if pos. */
	int type;             /* SOCK_STREAM or SOCK_DGRAM */
};    

/*
 * Command lines passed to system() to set up loopback
 * network interface label.
 */
/* REFERENCED */
static char *iflabelcmd[] = {
    "iflabel lo0 SGIPSO2 msenhigh/mintlow msenlow/minthigh 3",		/* 0 */
    "iflabel lo0 SGIPSO2 msentcsec,100,7,12,18,37,48,63,97/mintlow "
    "msenlow/mintbiba,120,3,6,9,12,15,18,21,100,101,120 3",		/* 1 */
    "iflabel lo0 CIPSO2 "
    "msentcsec,100/mintbiba,95,7,8,9,100,205,309,405,505,604,650,800,900,1000,2000,3000,65530 "
    "msentcsec,1/mintbiba,95,7,8,9,100,205,309,405,505,604,650,800,900,1000,2000,3000,65530 3", /* 2 */
    "iflabel lo0 CIPSO2 msentcsec,75,1,2,3,4,5,6,8,9,12,20,24,48,100,200,900,65533/mintlow msentcsec,30,1,2/mintlow 3",	/* 3 */
    "iflabel lo0 BSO msentcsec,192,1,2,3/mintlow msentcsec,128,1,2,3/mintlow 128 128",					/* 4 */
    "iflabel lo0 MONO msentcsec,125,2,5,6/mintbiba,250,5,10,15",	/* 5 */
};
/*
 * msen  catcount
 * level     ---------------------- catlist -- ---------------------------
 */
/* REFERENCED */
static struct msen_trans_l msen_tlist[] = {
    MSH, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    MSM, 75,16,  1,  2,  3,  4,  5,  6,  8,  9, 12, 20, 24, 48,100,200,900,65533,
    MST, 50, 4,  7, 12, 18, 37,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    MST,100, 7,  7, 12, 18, 37, 48, 63, 97,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    MST,150, 4,  7, 12, 18, 37,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    /* 0-4 */  
    MST, 50, 4,  7, 12, 18, 40,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    MST, 25, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    MST,  2, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    MST,  1, 1,  1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    MST, 65, 4,  2,  4,  6,  8,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    /* 5-9*/  
    MST, 25, 4,  2,  4,  6,  8,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    MST,160, 3,  1,  2,  3,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    MST,110, 3,  1,  2,  3,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    MST,160, 2,  2,  4,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    MST,125, 3,  2,  5,  6,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    /* 10-14*/
};
/*
 * mint  divcount
 * grade     ---------------------- divlist -- ---------------------------
 */
/* REFERENCED */
static struct mint_trans_l mint_tlist[] = {
    MIL,  0, 0,  0, 0, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    MIB, 95,16,  7, 8, 9,100,205,309,405,505,604,650,800,900,1000,2000,3000,65530,

    MIB,110, 3,  3, 6, 9,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    MIB,120,10,  3, 6, 9, 12, 15, 18, 21,100,101,120,  0,  0,  0,  0,  0,  0,
    MIB,130, 3,  3, 6, 9,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
 
    MIB,110, 3,  3, 6,10,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    MIB,250, 3,  5,10,15,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    MIH,  0, 0,  0, 0, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
};
/* REFERENCED */
static struct so_minfo so_trans_minfo[] = {
    0, 0, 0, PASS,             0, SOCK_STREAM,  /* 0 limit of range */
    0, 2, 2, PASS,             0, SOCK_STREAM,  /* 1 in range,cat/dev OK */
    0, 1, 0, FAIL,        EACCES, SOCK_STREAM,  /* 2 too many cats */
    0, 0, 1, FAIL,        EACCES, SOCK_STREAM,  /* 3 too many divs */
    0, 3, 3, FAIL,        EACCES, SOCK_STREAM,  /* 4 too many cats/divs */
    1, 2, 2, PASS,             0, SOCK_STREAM,  /* 5 in range,cat/dev OK */
    1, 4, 2, FAIL,        EACCES, SOCK_STREAM,  /* 6 bad level */
    1, 2, 4, FAIL,        EACCES, SOCK_STREAM,  /* 7 bad grade */
    1, 5, 2, FAIL,        EACCES, SOCK_STREAM,  /* 8 bad cat */
    1, 2, 5, FAIL,        EACCES, SOCK_STREAM,  /* 9 bad div */
    1, 0, 2, FAIL,        EACCES, SOCK_STREAM,  /* 10 bad msen */
    1, 2, 7, FAIL,        EACCES, SOCK_STREAM,  /* 11 bad mint */
    2, 6, 1, PASS,             0, SOCK_STREAM,  /* 12 in range,cat OK */
    2, 0, 1, FAIL,        EACCES, SOCK_STREAM,  /* 13 non-tcsec */
    2, 6, 0, FAIL,        EACCES, SOCK_STREAM,  /* 14 bad mint */
    2, 7, 0, FAIL,        EACCES, SOCK_STREAM,  /* 15 bad level */
    2, 8, 0, FAIL,        EACCES, SOCK_STREAM,  /* 16 bad cat */
    3, 1, 0, FAIL,        EACCES, SOCK_STREAM,  /* 17 too many cats */
    4,11, 0, PASS,             0, SOCK_STREAM,  /* 18 in range,cat OK */
    4,11, 2, FAIL,        EACCES, SOCK_STREAM,  /* 19 bad mint */
    4,12, 0, FAIL,        EACCES, SOCK_STREAM,  /* 20 bad level */ 
    4,13, 0, FAIL,        EACCES, SOCK_STREAM,  /* 21 bad cat */
    5,14, 6, PASS,             0, SOCK_STREAM,  /* 22 matching */
    5,14, 2, FAIL,        EACCES, SOCK_STREAM,  /* 23 non-matching */
 

    0, 0, 0, PASS,             0, SOCK_DGRAM,  /* 24 limit of range */
    0, 2, 2, PASS,             0, SOCK_DGRAM,  /* 25 in range,cat/dev OK */
    0, 1, 0, FAIL,        EACCES, SOCK_DGRAM,  /* 26 too many cats */
    0, 0, 1, FAIL,        EACCES, SOCK_DGRAM,  /* 27 too many divs */
    0, 3, 3, FAIL,        EACCES, SOCK_DGRAM,  /* 28 too many cats/divs */
    1, 2, 2, PASS,             0, SOCK_DGRAM,  /* 29 in range,cat/dev OK */
    1, 4, 2, FAIL,        EACCES, SOCK_DGRAM,  /* 30 bad level */
    1, 2, 4, FAIL,        EACCES, SOCK_DGRAM,  /* 31 bad grade */
    1, 5, 2, FAIL,        EACCES, SOCK_DGRAM,  /* 32 bad cat */
    1, 2, 5, FAIL,        EACCES, SOCK_DGRAM,  /* 33 bad div */
    1, 0, 2, FAIL,        EACCES, SOCK_DGRAM,  /* 34 bad msen */
    1, 2, 7, FAIL,        EACCES, SOCK_DGRAM,  /* 35 bad mint */
    2, 6, 1, PASS,             0, SOCK_DGRAM,  /* 36 in range,cat OK */
    2, 0, 1, FAIL,        EACCES, SOCK_DGRAM,  /* 37 non-tcsec */
    2, 6, 0, FAIL,        EACCES, SOCK_DGRAM,  /* 38 bad mint */
    2, 7, 0, FAIL,        EACCES, SOCK_DGRAM,  /* 39 bad level */
    2, 8, 0, FAIL,        EACCES, SOCK_DGRAM,  /* 40 bad cat */
    3, 1, 0, FAIL,        EACCES, SOCK_DGRAM,  /* 41 too many cats */
    4,11, 0, PASS,             0, SOCK_DGRAM,  /* 42 in range,cat OK */
    4,11, 2, FAIL,        EACCES, SOCK_DGRAM,  /* 43 bad mint */
    4,12, 0, FAIL,        EACCES, SOCK_DGRAM,  /* 44 bad level */ 
    4,13, 0, FAIL,        EACCES, SOCK_DGRAM,  /* 45 bad cat */
    5,14, 6, PASS,             0, SOCK_DGRAM,  /* 46 matching */
    5,14, 2, FAIL,        EACCES, SOCK_DGRAM,  /* 47 non-matching */
};
/* REFERENCED */
static char *so_trans_mdesc[] = {
	"Stream, SGIPSO2, limit of range",
	"Stream, SGIPSO2, in range,cat/dev OK",
	"Stream, SGIPSO2, too many cats",
	"Stream, SGIPSO2, too many divs",
	"Stream, SGIPSO2, too many cats/divs",
	"Stream, SGIPSO2, in range,cat/dev OK",
	"Stream, SGIPSO2, bad level",
	"Stream, SGIPSO2, bad grade",
	"Stream, SGIPSO2, bad cat",
	"Stream, SGIPSO2, bad div",
	"Stream, SGIPSO2, bad msen",
	"Stream, SGIPSO2, bad mint",
	"Stream, CIPSO2, in range,cat OK",
	"Stream, CIPSO2, non-tcsec",
	"Stream, CIPSO2, bad mint",
	"Stream, CIPSO2, bad level",
	"Stream, CIPSO2, bad cat",
	"Stream, CIPSO2, too many cats",
	"Stream, BSO, in range,cat OK",
	"Stream, BSO, bad mint",
	"Stream, BSO, bad level", 
	"Stream, BSO, bad cat",
	"Stream, MONO, matching",
	"Stream, MONO, non-matching",

	"Dgram, SGIPSO2, limit of range",
	"Dgram, SGIPSO2, in range,cat/dev OK",
	"Dgram, SGIPSO2, too many cats",
	"Dgram, SGIPSO2, too many divs",
	"Dgram, SGIPSO2, too many cats/divs",
	"Dgram, SGIPSO2, in range,cat/dev OK",
	"Dgram, SGIPSO2, bad level",
	"Dgram, SGIPSO2, bad grade",
	"Dgram, SGIPSO2, bad cat",
	"Dgram, SGIPSO2, bad div",
	"Dgram, SGIPSO2, bad msen",
	"Dgram, SGIPSO2, bad mint",
	"Dgram, CIPSO2, in range,cat OK",
	"Dgram, CIPSO2, non-tcsec",
	"Dgram, CIPSO2, bad mint",
	"Dgram, CIPSO2, bad level",
	"Dgram, CIPSO2, bad cat",
	"Dgram, CIPSO2, too many cats",
	"Dgram, BSO, in range,cat OK",
	"Dgram, BSO, bad mint",
	"Dgram, BSO, bad level", 
	"Dgram, BSO, bad cat",
	"Dgram, MONO, matching",
	"Dgram, MONO, non-matching",
};

/*
 * Structure used by agent_dac()
 */
struct so_dacinfo {
    char *casename;          /* Test case name. */
    short casenum;           /* Test case number. */
    short expect_success;    /* True if positive, false if negative. */
    uid_t uid;               /* Sender's EUID */
    int uidcount;            /* so_uidcount passed to ioctl */
    uid_t uidlist0;          /* so_uidlist[0] passed to ioctl */
    uid_t uidlist1;          /* etc. */
    uid_t uidlist2;          
    int type;                /* socket type */
};

#endif	/* ifndef soc.h */
