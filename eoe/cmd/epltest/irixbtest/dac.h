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

#ifndef	DAC_H__
#define	DAC_H__

#include "irixbtest.h"
#include <sys/wait.h>

#define UNUSED

/*
* Structure used by getlabel_dac0.
*/
struct dacinfo {
	char *casename;          /* Test case name. */
	short casenum;           /* Test case number. */
	short c_flag;		 /* Capability flag */
	short expect_success;    /* True if positive, false if negative. */
	int expect_errno;        /* Zero if positive, errno if negative. */
	uid_t uid;               /* Subject EUID */
	gid_t gid;               /* Subject EGID */
	uid_t F_owner;           /* File owner. */ 
	gid_t F_group;           /* File group. */ 
	mode_t F_mode;           /* File mode. */
	uid_t D_owner;           /* Directory owner. */
	gid_t D_group;           /* Directory group. */
	mode_t D_mode;           /* Directory mode. */
	};


/*
 * Structure used by bsdsetgroups_proc1, bsdsetpgrp_proc2, setsid_proc2
 * rdname_dac1, setgroups_proc1, setpgrp_proc2:
 */
struct dacparms_2 {
    char *casename;
    short casenum;
    short c_flag;	/* capability flag */
    uid_t ruid;        /* RUID */
    uid_t euid;        /* EUID */
    short expect_success;
    int expect_errno;
};

/*
 * Structure used by bsdsetpgrp_proc1, setpgrp_proc1, setsid_proc1:
 */
struct dacparms_3 {
    char *casename;       /* Name of this case */
    short casenum;        /* Number of this case */
    uid_t ruid;           /* RUID */
    uid_t euid;           /*  EUID */
    short flag;           /* Subject does setsid before test call */
    short expect_success;
    int expect_errno;
};

/*
 * Structure used by kill_dac, setpgid_dac1, 
 * bsdsetpgrp_dac, procsz_dac, rdublk_dac:
 */
struct dacparms_4 {
    char *casename;
    short casenum;
    short   c_flag;	/* Capabiliity Flag */
    uid_t ruid;         /* OBJECT RUID */
    uid_t euid;         /* OBJECT EUID */
    uid_t Sruid;        /* SUBJECT RUID */
    uid_t Seuid;        /* SUBJECT  EUID */
    int flag1;          /* signal; use_Ppid; or unused */
    short flag2;        /* do_setsid; use_badpid */
    short expect_success;
    int expect_errno;
};

/*
 * Structure used by bsdgetpgrp_dac, ptracer_dac1, ptracer_dac2,
 * ptracew_dac1, ptracew_dac2 setpgid_dac2, setreuid_proc1, 
 */
struct dacparms_5 {
    char *casename;
    short casenum;
    uid_t ruid;  /*  RUID */
    uid_t euid;  /*  EUID */
    uid_t Sruid; /* SUBJECT RUID */
    uid_t Seuid; /* SUBJECT  EUID */
    short c_flag; /* use capability for positive case */
    short expect_success;
    int expect_errno;
};


/*
 * Structure used by setregid_proc1:

 */
struct setregid_pinfo1 {
    char casename[SLEN];   /* Name of this case */
    short casenum;         /* Number of this case */
    short c_flag;	   /* Capability flag */
    uid_t Sruid;           /* Subject RUID */
    uid_t Seuid;           /* Subject EUID */
    gid_t Srgid0;          /* Subject RGID, first call */
    gid_t Segid0;          /* Subject EGID, first call*/
    gid_t Srgid1;          /* Subject RGID second call */
    gid_t Segid1;          /* Subject EGID second call*/
    short expect_success;
    int expect_errno;
};


/*
 * Structure used by chown_dac:

 */
struct chown_dparm1 {
    char casename[SLEN];   /* Name of this case */
    short casenum;         /* Number of this case */
    short c_flag;	   /* Capability flag */
    uid_t Sruid;           /* Subject RUID invoking chown*/
    uid_t Seuid;           /* Subject EUID invoking chown*/
    uid_t Ouid;            /* Object RUID to create file and dir*/
    gid_t Ogid;            /* Object EUID to create file and dir*/
    uid_t new_Ouid;        /* Object RUID to chown file to*/
    gid_t new_Ogid;        /* Object EUID to chown file to*/
    short expect_success;
    int expect_errno;
};

#endif /* ifndef dac.h */
