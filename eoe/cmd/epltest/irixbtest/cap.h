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

#ifndef CAP_H__
#define CAP_H__

/* Trun on (1) or off (0) DEBUG */
#define DEBUG	1

#include "irixbtest.h"

#define UNUSED

extern	int	cap_unlink(const char *);
extern	int	cap_blockproc(pid_t);
extern	int	cap_unblockproc(pid_t);
extern	int	cap_BSDsetpgrp(pid_t, pid_t);
extern	int	cap_chmod(const char *, mode_t);
extern	int	cap_fchmod(const char *, mode_t);
extern	int	cap_chdir(const char *);
extern	int	cap_rmdir(const char *);
extern	int	cap_mkdir(const char *, mode_t);
extern	mac_t	cap_mac_get_file(const char *);
extern	int	cap_msgctl(int, int, struct msqid_ds *);
extern	int	cap_open(const char *, int, mode_t);
extern	ptrdiff_t cap_prctl(unsigned, pid_t);
extern	int	cap_setgroups(int, gid_t *);
extern	int	cap_BSDsetgroups(int, int *);
extern	int	cap_setregid(uid_t, uid_t);
extern	int	cap_setuid(uid_t);
extern	int	cap_setreuid(uid_t, uid_t);
extern	int	cap_setlabel(const char *, mac_t);
extern	int	cap_setplabel(mac_t);
extern	int	cap_kill(pid_t, int);
extern	int	cap_chown(const char *, uid_t, gid_t);
extern	int	cap_fchown(int, uid_t, gid_t);
extern	int	cap_put_file(const char *, cap_t );
extern	int	cap_perm_all_file(const char *);
extern	int	cap_put_proc(cap_t );
extern	int	cap_perm_all_proc(void);
extern	int	cap_write(int, char *, int);
extern	void	*cap_acl_get_file(const char *, int);
extern	void	*cap_acl_get_fd(int);
extern	int	cap_acl_set_file(const char *, int, acl_t);

extern	void	case_name(int , int , char *, int );
extern	void	case_file(char *, int , char *);

/*
 * Structure used by setsid_proc2, setpgrp_proc2:
 */
struct capparms_2 {
    char *casename;
    short casenum;
    uid_t ruid;        /* RUID */
    uid_t euid;        /* EUID */
    short expect_success;
    int expect_errno;
    char  cflag[MSGLEN];   /* Capability flag*/
};

/*
 * Structure used by bsdsetpgrp_proc1, setpgrp_proc1, setsid_proc1:
 */
struct capparms_3 {
    char *casename;       /* Name of this case */
    short casenum;        /* Number of this case */
    uid_t ruid;           /* RUID */
    uid_t euid;           /*  EUID */
    short flag;           /* Subject does setsid before test call */
    short expect_success;
    int expect_errno;
    char  cflag[MSGLEN];   /* Capability flag*/
};

/*
 * Structure used by set_fcap.c:
 */
struct set_fcap1 {
    char casename[SLEN];   /* Name of this case */
    short casenum;         /* Number of this case */
    uid_t Sruid;           /* Subject RUID invoking chown*/
    uid_t Seuid;           /* Subject EUID invoking chown*/
    uid_t Ouid;            /* Object RUID to create file and dir*/
    gid_t Ogid;            /* Object EUID to create file and dir*/
    short expect_success;
    int   expect_errno;    
    char  cflag[MSGLEN];   /* Capability flag*/
    };

/*
 * Structure used by set_pcap:
 */
struct set_pcap1 {
    char casename[SLEN];   /* Name of this case */
    short casenum;         /* Number of this case */
    uid_t Sruid;           /* Subject RUID invoking chown*/
    uid_t Seuid;           /* Subject EUID invoking chown*/
    short expect_success;
    int   expect_errno;    
    char  cflag[MSGLEN];   /* Capability flag*/
    };

/*
 * Structure used by chown_dcap:
 */
struct chown_cparm1 {
    char casename[SLEN];   /* Name of this case */
    short casenum;         /* Number of this case */
    uid_t Sruid;           /* Subject RUID invoking chown*/
    uid_t Seuid;           /* Subject EUID invoking chown*/
    uid_t Ouid;            /* Object RUID to create file and dir*/
    gid_t Ogid;            /* Object EUID to create file and dir*/
    uid_t new_Ouid;        /* Object RUID to chown file to*/
    gid_t new_Ogid;        /* Object EUID to chown file to*/
    short expect_success;
    int   expect_errno;    
    char  cflag[MSGLEN];   /* Capability flag*/
    };

/*
 * Structure used by openr_dcap, openw_dcap and exece_dcap.
 */
struct cap_cparm1 {
    char casename[SLEN];   /* Name of this case */
    short casenum;         /* Number of this case */
    uid_t Sruid;           /* Subject RUID */
    uid_t Seuid;           /* Subject EUID */
    uid_t Ouid;            /* Object RUID to create file and dir*/
    gid_t Ogid;            /* Object EUID to create file and dir*/
    int   flag;            /* Open flag, amode, or Exec type. */
    short expect_success;
    int   expect_errno;    
    char  cflag[MSGLEN];   /* Capability flag*/
    };

/*
 * Structure used by kill_dcap
 */
struct capparms_4 {
    char *casename;
    short casenum;
    uid_t ruid;         /* OBJECT RUID */
    uid_t euid;         /* OBJECT EUID */
    uid_t Sruid;        /* SUBJECT RUID */
    uid_t Seuid;        /* SUBJECT  EUID */
    int flag1;          /* signal; use_Ppid; or unused */
    short flag2;        /* do_setsid; use_badpid */
    short expect_success;
    int expect_errno;
    char  cflag[MSGLEN]; /* Capability flag*/
};

/*
 * Structure used by setreuid_pcap, 
 */
struct capparms_5 {
    char *casename;
    short casenum;
    uid_t ruid;			/*  RUID */
    uid_t euid;			/*  EUID */
    uid_t Sruid;		/* SUBJECT RUID */
    uid_t Seuid;		/* SUBJECT  EUID */
    short expect_success;
    int expect_errno;
    char  cflag[MSGLEN];	/* Capability flag*/
};

/*
 * Structure used by mknod_dcap:
 */
struct mknod_cparm1 {
    char casename[SLEN];   /* Name of this case */
    short casenum;         /* Number of this case */
    uid_t Sruid;           /* Subject RUID invoking chown*/
    uid_t Seuid;           /* Subject EUID invoking chown*/
    uid_t Ouid;            /* Object RUID to create file and dir*/
    gid_t Ogid;            /* Object EUID to create file and dir*/
    short expect_success;
    int   expect_errno;    
    char  cflag[MSGLEN];   /* Capability flag*/
    };

/*
 * Structure used by setregid_pcap:

 */
struct setregid_cparm1 {
    char casename[SLEN];   /* Name of this case */
    short casenum;         /* Number of this case */
    uid_t Sruid;           /* Subject RUID */
    uid_t Seuid;           /* Subject EUID */
    gid_t Srgid0;          /* Subject RGID, first call */
    gid_t Segid0;          /* Subject EGID, first call*/
    gid_t Srgid1;          /* Subject RGID second call */
    gid_t Segid1;          /* Subject EGID second call*/
    short expect_success;
    int expect_errno;
    char  cflag[MSGLEN];   /* Capability flag*/
};
/*
 * Structure used by set_fxid.
 */
struct fxid_cparm1 {
    char casename[SLEN];   /* Name of this case */
    short casenum;         /* Number of this case */
    uid_t Suid;            /* Subject RUID */
    gid_t Sgid;            /* Subject GID */
    uid_t Ouid;            /* Object RUID to create file and dir*/
    gid_t Ogid;            /* Object EUID to create file and dir*/
    mode_t xflag;          /* Open flag, amode, or Exec type. */
    mode_t flag;           /* Open flag, amode, or Exec type. */
    short expect_success;
    int   expect_errno;    
    char  cflag[MSGLEN];   /* Capability flag*/
    };

#endif /* ifndef cap.h */
