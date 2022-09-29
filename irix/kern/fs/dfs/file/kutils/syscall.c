/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: syscall.c,v 65.8 1998/03/31 18:09:48 lmc Exp $";
static void _identFunc(const char *x) {if(!x)_identFunc(_identString);}
#endif


/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/* Copyright (C) 1996, 1989 Transarc Corporation - All rights reserved */

#include <dcedfs/param.h>		/* Should be always first */
#include <dcedfs/sysincludes.h>		/* Standard vendor system headers */
#include <dcedfs/osi.h>
#include <dcedfs/osi_sysconfig.h>
#include <dcedfs/syscall.h>
#ifdef SGIMIPS
#include <sys/systm.h>
#endif

RCSID("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/kutils/RCS/syscall.c,v 65.8 1998/03/31 18:09:48 lmc Exp $")

/* for possible CM/PX communication. */
unsigned int afscall_timeSynchDistance = 0x7ffffff1;
unsigned int afscall_timeSynchDispersion = 0;

#ifdef SGIMIPS
int afs_nosys(long, long, long, long, long, long *);
extern int afscall_cm(long, long, long, long, long, long *);
extern int afscall_cm_pioctl(long, long, long, long, long, long *);
extern int afscall_setpag(long, long, long, long, long, long *);
extern int afscall_resetpag(long, long, long, long, long, long *);
extern int afscall_exporter(long, long, long, long, long, long *);
extern int afscall_volser(long, long, long, long, long, long *);
extern int afscall_aggr(long, long, long, long, long, long *);
extern int afscall_getpag(long, long, long, long, long, long *);
extern int afscall_cm_newtgt(long, long, long, long, long, long *);
extern int afscall_vnode_ops(long, long, long, long, long, long *);
extern int afscall_plumber(long, long, long, long, long, long *);
extern int afscall_tkm_control(long, long, long, long, long, long *);
extern int afscall_icl(long, long, long, long, long, long *);
extern int afscall_newtgt(long, long, long, long, long, long *);
extern int afscall_bomb(long, long, long, long, long, long *);
#else
int afs_nosys(long, long, long, long, long, int *);
extern int afscall_vnode_ops(), afscall_plumber(), afscall_tkm_control();
extern int afscall_icl(), afscall_newtgt(), afscall_bomb();
#endif /* SGIMIPS */
#if	defined(AFS_OSF_ENV) || defined(AFS_HPUX_ENV)
extern int afscall_krpcdbg();
#endif
#ifdef	AFS_HPUX_ENV
extern int afscall_kload();
#endif
#ifdef	KTC_SYSCALL
extern int afscall_ktc();
#endif
#ifdef	TESTING_FP_SYSCALL
extern int afscall_fpkint();
#endif
#ifdef	TESTING_XCRED_SYSCALL
extern int afscall_xcredkint();
#endif

/*
 * AFS syscall table.
 *
 * The syscall table is initialized dynamically by 'configure'
 * routines - there are three: cm_configure, px_configure, and epi_configure.
 * cm and px are responsible for hooking their respective VFS file systems into
 * the system VFS switch table. In addition, cm, px, and epi are responsible
 * for installing their respective system call vectors into this system call
 * table below. In the AFS_DYNAMIC case, the configure routines are called at
 * the time the kernel extensions are loaded.
 *
 * In the non AFS_DYNAMIC case, afs_syscall() calls the appropriate configure
 * routine before the first call to the system call, installing it in the
 * system call table.
 */
#ifdef AFS_IRIX_ENV
struct afs_sysent afs_sysent[] = {
/*
 *  Entry Point          Config    Syscall            Real Entry Point
 *                       Routine   Number
 */
    afscall_cm,		/*  CM   AFSCALL_CM         : afscall_cm           */
    afscall_cm_pioctl,	/*  CM   AFSCALL_PIOCTL     : afscall_cm_pioctl    */
    afscall_setpag,	/*  CM   AFSCALL_SETPAG     : afscall_setpag    */
    afscall_exporter,   /*  PX   AFSCALL_PX         : afscall_exporter     */
    afscall_volser,	/*  PX   AFSCALL_VOLSER     : afscall_volser       */
    afscall_aggr,	/*  PX   AFSCALL_AGGR       : afscall_aggr         */
    afs_nosys,		/*  EPI  AFSCALL_EPISODE    : afscall_episode      */
    afs_nosys,		/*  OBS  AFSCALL_KTC        : afscall_ktc   [obs.] */
    afs_nosys,          /*  OBS  TESTING_FP_SYSCALL : afscall_fpkint[obs.] */
    afs_nosys,          /*  OBS  TESTING_XCRED_SYSCALL: afscall_xcredkint [obs.] */
    afscall_vnode_ops,	/*  ALL  AFSCALL_VNODE_OPS  : afscall_vnode_ops     */
    afscall_getpag,	/*  CM   AFSCALL_GETPAG     : afscall_getpag     */
    afscall_plumber,	/*  ALL  AFSCALL_PLUMBER    : afscall_plumber     : osi mem leak support */
    afscall_tkm_control,/*  ALL  AFSCALL_TKM        : afscall_tkm_control : sys admin hook to TKM*/
    afscall_icl,	/*  ALL  AFSCALL_ICL        : afscall_icl : deal with in core log */
    afs_nosys,          /*  ALL  AFSCALL_KRPCDBG    : afscall_krpcdbg : krpc debug hooks */
    afscall_cm_newtgt,	/*  CM   AFSCALL_NEWTGT     : afscall_cm_newtgt */
    afscall_bomb,	/*  ALL  AFSCALL_BOMB       : afscall_bomb */
    afs_nosys,		/*  ALL	 AFSCALL_KLOAD	    : afscall_kload (HP/UX) */
    afs_nosys,		/*  AT   AFSCALL_AT         : afscall_at */
#ifdef	AFS_HPUX_ENV
/* Add enough dummies to make the table at least 31 entries long. */
    afs_nosys,
    afs_nosys,
    afs_nosys,
    afs_nosys,
    afs_nosys,
    afs_nosys,
    afs_nosys,
    afs_nosys,
    afs_nosys,
    afs_nosys,
    afs_nosys,
    afs_nosys,
    afs_nosys,
    afs_nosys,
    afs_nosys
#endif	/* AFS_HPUX_ENV */
};
#else
struct afs_sysent afs_sysent[] = {
/*
 *  Entry Point          Config    Syscall            Real Entry Point
 *                       Routine   Number
 */
    afs_nosys,		/*  CM   AFSCALL_CM         : afscall_cm           */
    afs_nosys,		/*  CM   AFSCALL_PIOCTL     : afscall_cm_pioctl    */
    afs_nosys,		/*  CM   AFSCALL_SETPAG     : afscall_setpag    */
    afs_nosys,		/*  PX   AFSCALL_PX         : afscall_exporter     */
    afs_nosys,		/*  PX   AFSCALL_VOLSER     : afscall_volser       */
    afs_nosys,		/*  PX   AFSCALL_AGGR       : afscall_aggr         */
    afs_nosys,		/*  EPI  AFSCALL_EPISODE    : afscall_episode      */
    afs_nosys,		/*  OBS  AFSCALL_KTC        : afscall_ktc   [obs.] */
    afs_nosys,          /*  OBS  TESTING_FP_SYSCALL : afscall_fpkint[obs.] */
    afs_nosys,          /*  OBS  TESTING_XCRED_SYSCALL: afscall_xcredkint [obs.] */
    afs_nosys,		/*  ALL  AFSCALL_VNODE_OPS  : afscall_vnode_ops     */
    afs_nosys,		/*  CM   AFSCALL_GETPAG     : afscall_getpag     */
    afs_nosys,		/*  ALL  AFSCALL_PLUMBER    : afscall_plumber     : osi mem leak support */
    afs_nosys,		/*  ALL  AFSCALL_TKM        : afscall_tkm_control : sys admin hook to TKM*/
    afs_nosys,		/*  ALL  AFSCALL_ICL        : afscall_icl : deal with in core log */
    afs_nosys,          /*  ALL  AFSCALL_KRPCDBG    : afscall_krpcdbg : krpc debug hooks */
    afs_nosys,		/*  CM   AFSCALL_NEWTGT     : afscall_cm_newtgt */
    afs_nosys,		/*  ALL  AFSCALL_BOMB       : afscall_bomb */
    afs_nosys,		/*  ALL	 AFSCALL_KLOAD	    : afscall_kload (HP/UX) */
    afs_nosys,		/*  AT   AFSCALL_AT         : afscall_at */
    afs_nosys,		/*  CM   AFSCALL_RESETPAG   : afscall_resetpag */
#ifdef	AFS_HPUX_ENV
/* Add enough dummies to make the table at least 31 entries long. */
    afs_nosys,
    afs_nosys,
    afs_nosys,
    afs_nosys,
    afs_nosys,
    afs_nosys,
    afs_nosys,
    afs_nosys,
    afs_nosys,
    afs_nosys,
    afs_nosys,
    afs_nosys,
    afs_nosys,
    afs_nosys
#endif	/* AFS_HPUX_ENV */
};
#endif /* AFS_IRIX_ENV */

#if	!defined(AFS_DYNAMIC) || defined(AFS_HPUX_ENV)

/*
 * For AFS_DYNAMIC, we assume the configure routines have already been called
 * at kernel extension load time.
 * For non AFS_DYNAMIC, we need afs_syscall() to be self-configuring.
 * The first time it is called with a certain AFS syscall number,
 * if calls the corresponding configuration routine in config_rtn[].
 */
extern int cm_configure(), px_configure();
#ifndef AFS_IRIX_ENV
extern int epi_configure();
#endif /* AFS_IRIX_ENV */
/*
 * Install the afs syscalls which are common to any configuration.
 */
int all_configure()
{
    AfsCall(AFSCALL_VNODE_OPS, afscall_vnode_ops);
    AfsCall(AFSCALL_PLUMBER, afscall_plumber);
    AfsCall(AFSCALL_TKM, afscall_tkm_control);
    AfsCall(AFSCALL_ICL, afscall_icl);
#if	defined(AFS_OSF_ENV) || defined(AFS_HPUX_ENV)
    AfsCall(AFSCALL_KRPCDBG, afscall_krpcdbg);
#endif
#ifdef	AFS_HPUX_ENV
    AfsCall(AFSCALL_KLOAD, afscall_kload);
/*
 * XXX 1.0.2 HP port defined AFSCALL_KLOAD as 31.  For now, we must link with
 * XXX files that make calls to afs_syscall and pass 31 to it.  So we must be
 * XXX able to cope with 31.
 */
    AfsCall(31, afscall_kload);
#endif
    AfsCall(AFSCALL_BOMB, afscall_bomb);
    return 0;
}

#endif

#ifndef	AFS_DYNAMIC

#define NIL 0


/*
 * Configure routines for each system call.
 * N.B. This table is modified at runtime.
 */
CFG_RTN config_rtn[AFSCALL_LAST + 1] = {
	    cm_configure,	/* AFSCALL_CM       */
	    cm_configure,	/* AFSCALL_PIOCTL   */
	    cm_configure,	/* AFSCALL_SETPAG   */
	    px_configure,	/* AFSCALL_PX       */
	    px_configure,	/* AFSCALL_VOLSER   */
	    px_configure,	/* AFSCALL_AGGR     */
#ifdef AFS_IRIX_ENV
	    NIL,	        /* AFSCALL_EPISODE  */
#else
	    epi_configure,	/* AFSCALL_EPISODE  */
#endif /*  AFS_IRIX_ENV */
	    NIL,
	    NIL,
	    NIL,
	    all_configure,	/* AFSCALL_VNODE_OPS */
	    cm_configure,	/* AFSCALL_GETPAG    */
	    all_configure,	/* AFSCALL_PLUMBER   */
	    all_configure,	/* AFSCALL_TKM       */
	    all_configure,	/* AFSCALL_ICL       */
            all_configure,      /* AFSCALL_KRPCDBG   */
	    cm_configure,       /* AFSCALL_NEWTGT    */
	    all_configure,	/* AFSCALL_BOMB	     */
	    all_configure,	/* AFSCALL_KLOAD     */
#ifdef SGIMIPS
	    NIL,		/* NFS/DFS gateway?  */
				/* We'll leave it alone for now. */
#else
	    at_configure,	/* AFSCALL_AT        */
#endif
	    cm_configure        /* AFSCALL_RESETPAG  */
};

short config_done[AFSCALL_LAST + 1];

#endif	/* !AFS_DYNAMIC */

#ifdef SGIMIPS
/* 
 * Main entry of all afs system calls 
 */
struct args {
    long syscall;
    long parm1;
    long parm2;
    long parm3;
    long parm4;
    long parm5;
};


afs_syscall(struct args *uap, rval_t *rvp)
{
    __int64_t *retval = &rvp->r_val1;

    /* Under 6.2, rval1 is 64-bit despite the nature of the executing bin.
     * Thus, using long is not sufficient when passing rval to the 
     * underlying system call. To prevent modifications in too many places, 
     * I have declared a temp var and then typecast that into the real 
     * rval1 after the function returns. - brat
     */
    long tmp_rv = 0;

    DEFINE_OSI_UERROR;


    int sc = uap->syscall;
    int (*fun)(long, long, long, long, long, long *);

    if (sc < 0 || sc > AFSCALL_LAST) {
	    osi_setuerror(EINVAL);
	    return(EINVAL);
    }

#ifndef	AFS_DYNAMIC

    /*
     * Based on the syscall being made, we call the appropriate
     * configuration routine.  Currently there are three
     * independent config routines, one each for CM, PX, and EPISODE.
     * These config routines will install the correct entry points
     * in the afs syscall table, and install a VFS if required.
     * In the AFS_DYNAMIC case, these configuration routines will
     * have been called when the kernel extensions were loaded.
     */
    if (!config_done[sc]) {
	    if (config_rtn[sc] != NIL)
		(*config_rtn[sc])(SYSCONFIG_CONFIGURE, NULL, 0, NULL, 0);
	    config_done[sc]++;	/* only do this once */
    }
#endif  /* !AFS_DYNAMIC */
    
    /*
     * If the call fails, the return value contains the error code;
     * a 0 return is a successful call.  If a system call actually
     * needs to return something more, it would have to be passed
     * back via retval.
     */
    fun = afs_sysent[sc].afs_call;
    osi_PreemptionOff();
    osi_setuerror((*fun)(uap->parm1, uap->parm2,
			 uap->parm3, uap->parm4,
			 uap->parm5, &tmp_rv));
    *retval = (__int64_t)tmp_rv;
    osi_RestorePreemption(0);
    return (osi_getuerror());
}
#endif /* SGIMIPS */

int afs_nosys(long parm1, long parm2, long parm3, long parm4, long parm5,
#ifdef SGIMIPS
	      long *retvalP)
#else
	      int *retvalP)
#endif /* SGIMIPS */
{

#ifdef SGIMIPS
    /* REFERENCED */
#endif /* SGIMIPS */
    DEFINE_OSI_UERROR;

    *retvalP = 0;	/* no special info to return */
    osi_setuerror(ENOSYS);
#ifdef SGIMIPS
    return (ENOSYS);
#else
    return (osi_getuerror());
#endif /* SGIMIPS */
}

void afs_set_syscall(
    int entry,
#ifdef SGIMIPS
    int (*call)(long, long, long, long, long, long *))
#else
    int (*call)(long, long, long, long, long, int *))
#endif /* SGIMIPS */
{
    afs_sysent[entry].afs_call = call;
}
