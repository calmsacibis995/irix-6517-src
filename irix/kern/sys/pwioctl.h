/**************************************************************************
*                                                                        *
*          Copyright (C) 1990-1994 Silicon Graphics, Inc.                *
*                                                                        *
*  These coded instructions, statements, and computer programs  contain  *
*  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
*  are protected by Federal copyright law.  They  may  not be disclosed  *
*  to  third  parties  or copied or duplicated in any form, in whole or  *
*  in part, without the prior written consent of Silicon Graphics, Inc.  *
*                                                                        *
**************************************************************************/
/*
 * sys/pwioctl.h -- structures and defines for poll-able semaphores
 */
#ifndef __PWIOCTL_H__
#define __PWIOCTL_H__

#ident "$Revision: 1.5 $"

#define PWIOC    ('p' << 16 | 'w' << 8)

/* ioctl commands. */

#define PWIOCCREATE (PWIOC|2)   /* Create postwait instance */

/* sysmips arg1 commands */

#define PWIOCREG    (PWIOC|3)    /* register with the post-wait instance */
#define PWIOCWAIT   (PWIOC|4)    /* wait for a timer, post or signal     */
#define PWIOCPOST   (PWIOC|5)    /* post an event                        */
#define PWIOCDEREG  (PWIOC|6)    /* unregister from the post_wait inst.  */
#define PWIOCINFO   (PWIOC|7)    /* postwait instance information        */
#define PWIOCSTATS  (PWIOC|8)    /* postwait instacne stats              */

/*
 * macro to glom instance number and  caller entry.
 */

#define PWCALLERID(i,e) (((i) << 16) | ((e) & 0xffff)) 

/* argument structure for PWIOCCREATE */

typedef struct pwcreate_s {
    short pwc_nument;    /* number of users of this postwait instance */
    short pwc_ent;       /* entry # of the caller of PWIOCCREATE      */
    int   pwc_perm;      /* permission bits                           */
} pwcreate_t;

/* pwc_perm bits, specify who can register with the instance. The creator is
 * always registered automatically. Pemission is not checked for the superuser
 */

#define PWC_PERM_USER    0x1   /* any process with the same euid as the creator */
#define PWC_PERM_GROUP   0x2   /* any process with the same egid as the creator */
#define PWC_PERM_OTHERS  0x4   /* any process                                   */

/* argument structure for PWIOCINFO */

typedef struct pwinfo_s {
    short  pwi_nument;   /* number of users of this postwait instance */
    uint_t pwi_timer;    /* resolution of pw_timer                    */
} pwinfo_t;

/* structure for entries statistics */

typedef struct pwsent_s {
    pid_t  pwse_pid;        /* pid of entry                       */
    uint_t pwse_postouts;   /* posts done by this entry to others */
    uint_t pwse_postins;    /* posts done to this entry by others */
    uint_t pwse_waits;      /* waits done by this                 */
    uint_t pwse_timeouts;   /* timeouts for this entry            */
    uint_t pwse_postmisses; /* post misses woken by next timeout  */
} pwsent_t;

/* argument structure for PWIOCSTATS */

typedef struct pwstat_s {
    short     pws_nument;   /* Size of the following array       */
    pwsent_t *pws_array;    /* Array to hold returned statistics */
} pwstat_t;

#if defined(_KERNEL)
#if _MIPS_SIM == _ABI64
typedef struct irix5_pwstat_s {
    short        pws_nument;   /* Size of the following array       */
    app32_ptr_t  pws_array;    /* Array to hold returned statistics */
} irix5_pwstat_t;
#endif

extern void pwexit(void);
extern int  pwsysmips(sysarg_t arg1, sysarg_t arg2, sysarg_t arg3,
							rval_t *rvalp);
#endif

#endif

