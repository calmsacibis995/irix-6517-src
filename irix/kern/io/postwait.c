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

#ident    "$Revision: 1.25 $"

#include "sys/types.h"
#include "sys/systm.h"
#include "sys/sysmacros.h"
#include "sys/param.h"
#include "sys/cred.h"
#include "sys/errno.h"
#include "sys/sema.h"
#include "sys/pda.h"
#include "sys/poll.h"
#include "ksys/vproc.h"
#include "sys/proc.h"
#include "ksys/vfile.h"
#include "sys/vnode.h"
#include "sys/kmem.h"
#include "sys/debug.h"
#include "sys/cmn_err.h"
#include "sys/time.h"
#include "sys/pwioctl.h"
#include "sys/par.h"
#include "sys/immu.h"
#include "sys/time.h"
#include "sys/conf.h"
#include "sys/xlate.h"
#include "sys/dbacc.h"
#include "sys/kabi.h"

#define __PWTIME_CK 0

/*
 * post-wait driver: mainly used for Oracle.
 */

int pwdevflag = D_MP;

/* structure for each postwait entry */
#define CACHELINESZ 128

typedef struct pwe_s {
    lock_t     pwe_lock;
    int        pwe_flag;
    pid_t      pwe_pid;
    int        pwe_timerset;
    int        pwe_timerval;
    uint_t     pwe_postouts;
    uint_t     pwe_postins;
    uint_t     pwe_waits;
    uint_t     pwe_timeouts;
    uint_t     pwe_postmisses;
    sv_t       pwe_sv;
} pwe_t;

/*
 * pwe_flag
 */

#define PWE_POSTED    0x1
#define PWE_WAITING   0x2

/*
 * pwe_timerset
 */

#define PWE_TIMERSET 0x1
#define PWE_BIGTIMER 0x7fffffff

/* structure for each postwait instance */

typedef struct pwi_s {
    lock_t    pwi_lock;
    short     pwi_state;
    short     pwi_nument;
    uid_t     pwi_euid;
    gid_t     pwi_egid;
    int       pwi_perm;
    int       pwi_active;
    pwe_t    *pwi_pwe;
} pwi_t;


/* 
 * pwi_state. 
 * On instance startup , state transits from INVALID to INITING to VALID
 * On instance shutdown, state transits from VALID to EXITING to INVALID 
 */

#define PWI_INVALID  1
#define PWI_INITING  2
#define PWI_VALID    3
#define PWI_EXITING  4

lock_t      pw_lock;
uint_t      pw_count;
__psunsigned_t      pw_timerver;

pwi_t    *pwi;
pwsent_t *pw_stat;        /* staging array for copyout of stats */
int       pw_statinuse;

#define PW_PRIORITY (PZERO+1)
#define PW_MAXINST  32768
#define PW_MAXENT   32768
#define PW_DEFTIMER 10


#if __PWTIME_CK
timespec_t lastran;
#endif

extern int pw_maxinst;            /* config constant */
extern int pw_maxent;             /* config constant */
extern int pw_timer;              /* config constant */

#if _MIPS_SIM == _ABI64
static int irix5_to_pwstat(enum xlate_mode, void *, int, xlate_info_t *);
static int pwstat_to_irix5(void *, int, xlate_info_t *);
#endif

void	pwtimeout(char *);

/*
 * postwait driver
 */

void
pwinit(void)
{
    pwi_t *pib, *pie;
#ifdef DONTUSE
    int s;
#endif /*DONTUSE*/

    if (pw_maxinst > PW_MAXINST || pw_maxinst <= 0)
        pw_maxinst = PW_MAXINST;

    if (pw_maxent > PW_MAXENT || pw_maxent <= 0)
        pw_maxent = PW_MAXENT;

    if (pw_timer == 0)
        pw_timer = PW_DEFTIMER;

    pwi = (pwi_t *)kmem_alloc(pw_maxinst * sizeof(pwi_t), KM_SLEEP | VM_CACHEALIGN);    

    for (pib = pwi, pie = pwi + pw_maxinst; pib < pie; pib++) {
        init_spinlock(&pib->pwi_lock, "pwilock", pib - pwi);
        pib->pwi_state = PWI_INVALID;
        pib->pwi_nument = 0;
        pib->pwi_euid = (uid_t)-1;
        pib->pwi_egid = (gid_t)-1;
        pib->pwi_perm = 0;
        pib->pwi_active = 0;
        pib->pwi_pwe = NULL;
    }

    spinlock_init(&pw_lock, "pwlock");
    pw_count = 0;
    pw_timerver = 0;

    pw_stat = (pwsent_t *)kmem_alloc(pw_maxent * sizeof(pwsent_t), KM_SLEEP | VM_CACHEALIGN);
    pw_statinuse = 0;

#ifdef DONTUSE
        s = mp_mutex_spinlock(&pw_lock);
        pw_count++;

        if (pw_count == 1) {
            pw_timerver++;
	    nanotime(&lastran);
            pwtimeout((char *)pw_timerver);
        }

        mp_mutex_spinunlock(&pw_lock, s);

#endif
}


int
postwait_installed(void)
{
	return SGI_DBACF_POSTWAIT;
}

void
postwait_getparams(struct dba_conf *dbc)
{
	dbc->dbcf_pw_maxinst = pw_maxinst;
	dbc->dbcf_pw_maxent = pw_maxent;
	dbc->dbcf_pw_timer = pw_timer;
}

/*ARGSUSED*/
int
pwopen(dev_t dev)
{
    return 0;
}

/*ARGSUSED*/
pwread(dev_t dev)
{
    return EINVAL;
}

/*ARGSUSED*/
pwwrite(dev_t dev)
{
    return EINVAL;
}

/*ARGSUSED*/
int
pwpoll(dev_t dev)
{
    return EINVAL;
}

/*ARGSUSED*/
int
pwclose(dev_t dev)
{
    return 0;
}

void
pwtimeout(char *timerver)
{

    pwi_t *pib, *pie;
    pwe_t *peb, *pee;
    int s;
    int exiting;
    
#if __PWTIME_CK
    timespec_t timenow;

    nanotime(&timenow);
    if (timenow.tv_sec - lastran.tv_sec > 2) {
	 printf("pwtimeout last ran at 0x%x.0x%x now is 0x%x.0x%x\n",
		lastran.tv_sec, lastran.tv_nsec, timenow.tv_sec, timenow.tv_nsec);
    }
    lastran = timenow;
#endif

    if (pw_count == 0 || timerver != (char *)pw_timerver)
        return;

    exiting = 0;

    for (pib = pwi, pie = pwi + pw_maxinst; pib < pie; pib++) {

        if (pib->pwi_state == PWI_INVALID || pib->pwi_state == PWI_INITING)
            continue;
        
        if (pib->pwi_state == PWI_EXITING) {
            exiting = 1;
            continue;
        }

        peb = pib->pwi_pwe;
        pee = pib->pwi_pwe + pib->pwi_nument;
        for (; peb < pee; peb++) {
            if (peb->pwe_pid != NOPID) {
                if (peb->pwe_timerset == PWE_TIMERSET) {
                    if (--peb->pwe_timerval <= 0) {
                        peb->pwe_timeouts++;
			sv_signal(&peb->pwe_sv);
                    }
                } 
#ifdef notdef
                if ((peb->pwe_flag & (PWE_WAITING | PWE_POSTED)) == (PWE_WAITING | PWE_POSTED)) {
                    /*
                     * in case process missed earlier wakeup 
                     */
                    peb->pwe_postmisses++;
		    sv_signal(&peb->pwe_sv);
                }
#endif
            }
        }
    }


    if (exiting == 1) {
        for (pib = pwi, pie = pib + pw_maxinst; pib < pie; pib++) {
            s = mp_mutex_spinlock(&pib->pwi_lock);
            if (pib->pwi_state != PWI_EXITING) {
                mp_mutex_spinunlock(&pib->pwi_lock, s);
                continue;
            }
            pib->pwi_state = PWI_INVALID;
            pib->pwi_nument = 0;
            nested_spinunlock(&pib->pwi_lock);

            nested_spinlock(&pw_lock);
            pw_count--;
            mp_mutex_spinunlock(&pw_lock, s);
        }
    }
    
    (void)fast_timeout(pwtimeout, timerver, pw_timer);
}

/*ARGSUSED*/
pwioctl(dev_t dev, int cmd, void *arg, int flag, cred_t *crp, int *rvalp)
{
    register int s;
    int error;
    size_t free = 0;

    pwcreate_t pwarg;
    
    pwi_t *pib, *pie;
    pwe_t *pe, *peb, *pee;

    short nument;
    
    switch (cmd) {
    case PWIOCCREATE:

	 if (copyin((void *)arg, (void *)&pwarg, sizeof (pwarg)))
            return EFAULT;

        nument = pwarg.pwc_nument;
        if (nument < 0 || nument > pw_maxent)
            return EINVAL;
        
        if (nument == 0)
            nument = pw_maxent;

        if (pwarg.pwc_ent < 0 || pwarg.pwc_ent > (nument - 1))
            return EINVAL;

        for (pib = pwi, pie = pwi + pw_maxinst; pib < pie; pib++) {
            s = mp_mutex_spinlock(&pib->pwi_lock);
            if (pib->pwi_state == PWI_INVALID) {
                pib->pwi_state = PWI_INITING;
                mp_mutex_spinunlock(&pib->pwi_lock, s);
                break;
            }
            mp_mutex_spinunlock(&pib->pwi_lock, s);
        }
        
        if (pib == pie)
            return ENOSPC;

        /* if nument set to pw_maxent, update user space */

        if (pwarg.pwc_nument == 0) {
            pwarg.pwc_nument = nument;
            if (copyout((void *)&pwarg, (void *)arg, sizeof(pwarg)))
                        return EFAULT;
        }

        /* No more errors after this point */

        pib->pwi_perm = pwarg.pwc_perm;
        pib->pwi_euid = get_current_cred()->cr_uid;
        pib->pwi_egid = get_current_cred()->cr_gid;

        if (pib->pwi_pwe == NULL) {
            pib->pwi_pwe = (pwe_t *)kmem_alloc(pw_maxent * sizeof(pwe_t), KM_SLEEP | VM_CACHEALIGN);

            free = (size_t) (pw_maxent * sizeof(pwe_t));
        }
        pib->pwi_nument = nument;

        for (peb = pib->pwi_pwe, pee = pib->pwi_pwe + pw_maxent; peb < pee; peb++) {
            init_spinlock(&peb->pwe_lock, "pwelock", peb - pib->pwi_pwe);
            peb->pwe_flag = 0;
            peb->pwe_pid = NOPID;
            peb->pwe_timerset = 0;
            peb->pwe_timerval = PWE_BIGTIMER;
            peb->pwe_postouts = 0;
            peb->pwe_postins = 0;
            peb->pwe_waits = 0;
            peb->pwe_timeouts = 0;
            peb->pwe_postmisses = 0;
	    sv_init(&peb->pwe_sv, SV_DEFAULT, NULL);
        }

        /* Register yourself otherwise timeout may deallocate the instance */

        pe = pib->pwi_pwe + pwarg.pwc_ent;
        pe->pwe_pid = current_pid();
        pib->pwi_active++;

        /* Mark yourself for cleanup on exit */

	if ((error = add_exit_callback(current_pid(), 0,
				(void (*)(void *))pwexit, 0)) != 0) {
		if (error == 1)
			error = ENOPROC;
		else if (error == 2)
			error = EAGAIN;

                if (free) {
                        kmem_free(pib->pwi_pwe, free);
                        pib->pwi_pwe = NULL;
                }
		return error;
	}
        /* Mark the entry as valid. */

        s = mp_mutex_spinlock(&pib->pwi_lock);
        pib->pwi_state = PWI_VALID;
        mp_mutex_spinunlock(&pib->pwi_lock, s);

        /*  Increment pw_count and start the timer if pw_count was zero */

        s = mp_mutex_spinlock(&pw_lock);
        pw_count++;

        if (pw_count == 1) {
            pw_timerver++;
#if __PWTIME_CK
	    nanotime(&lastran);
#endif
            pwtimeout((char *)pw_timerver);
        }

        mp_mutex_spinunlock(&pw_lock, s);

        /* Return the instance number */

        *rvalp = (pib - pwi);
        return 0;
    
    default:
        return EINVAL;
    }
}

int
pwsysmips(sysarg_t arg1, sysarg_t arg2, sysarg_t arg3, rval_t *rvalp)
{
    register pwi_t *pi;
    register pwe_t *pe, *pt;

    register short i, e;
    register int s;
    /* REFERENCED */
    int error;

    int wakeproc;

    pwinfo_t    pwinfo;
    pwstat_t    pwstat;

    if (rvalp != NULL)
        rvalp->r_val1 = 0;

    switch (arg1) {
    case PWIOCREG:

        i = arg2 >> 16;
        e = arg2 & 0xffff;
    
        if (i < 0 || i > (pw_maxinst - 1))
               return EINVAL;

        pi = pwi + i;

        /* Have to a lock to increment pwi_active. Might as well */
        /* take it now                                           */

        s = mp_mutex_spinlock(&pi->pwi_lock);
        if (pi->pwi_state != PWI_VALID) {
            mp_mutex_spinunlock(&pi->pwi_lock, s);
            return EINVAL;
        }
        if (e < 0 || e > (pi->pwi_nument - 1)) {
            mp_mutex_spinunlock(&pi->pwi_lock, s);
            return EINVAL;
        }
        pe = pi->pwi_pwe + e;
        if (pe->pwe_pid != NOPID) {
            mp_mutex_spinunlock(&pi->pwi_lock, s);
            return EINVAL;
        }

        while (1) {
            if (get_current_cred()->cr_uid == 0)
                break;

            if ((pi->pwi_perm & PWC_PERM_USER) &&
                (pi->pwi_euid == get_current_cred()->cr_uid))
                break;

            if ((pi->pwi_perm & PWC_PERM_GROUP) &&
                (pi->pwi_egid == get_current_cred()->cr_gid))
                break;

            if (pi->pwi_perm & PWC_PERM_OTHERS)
                break;

            mp_mutex_spinunlock(&pi->pwi_lock, s);
            return EACCES;
        }

        pe->pwe_pid = current_pid();
        pi->pwi_active++;
        nested_spinunlock(&pi->pwi_lock);

        nested_spinlock(&pe->pwe_lock);
        pe->pwe_flag = 0;
        pe->pwe_pid = current_pid();
        pe->pwe_timerset = 0;
        pe->pwe_timerval = PWE_BIGTIMER;
        pe->pwe_postouts = 0;
        pe->pwe_postins = 0;
        pe->pwe_waits = 0;
        pe->pwe_timeouts = 0;
        pe->pwe_postmisses = 0;
        sv_init(&pe->pwe_sv, SV_DEFAULT, NULL);
        mp_mutex_spinunlock(&pe->pwe_lock, s);

	/* Mark for cleanup on exit */
	error = add_exit_callback(current_pid(), 0,
					(void (*)(void *))pwexit, 0);
	ASSERT(error == 0);

	return 0;

    case PWIOCDEREG:
        
        i = arg2 >> 16;
        e = arg2 & 0xffff;
    
        if (i < 0 || i > (pw_maxinst - 1))
               return EINVAL;

        pi = pwi + i;

        if (pi->pwi_state != PWI_VALID) 
            return EINVAL;
        
        if (e < 0 || e > (pi->pwi_nument - 1))
            return EINVAL;
        
        pe = pi->pwi_pwe + e;
        if (pe->pwe_pid != current_pid())
            return EACCES;

        /* In case someone is posting me */

        s = mp_mutex_spinlock(&pe->pwe_lock);
        pe->pwe_flag = 0;
        pe->pwe_pid = NOPID;
        pe->pwe_timerset = 0;
        pe->pwe_timerval = PWE_BIGTIMER;
        sv_destroy(&pe->pwe_sv);
        nested_spinunlock(&pe->pwe_lock);

        nested_spinlock(&pi->pwi_lock);
        pi->pwi_active--;
        if (pi->pwi_active == 0)
            pi->pwi_state = PWI_EXITING;
        mp_mutex_spinunlock(&pi->pwi_lock,s);

        return 0;

    case PWIOCPOST:        /* Post an event */
        
        i = arg2 >> 16;
        e = arg2 & 0xffff;
    
        if (i < 0 || i > (pw_maxinst - 1))
               return EINVAL;

        pi = pwi + i;

        if (pi->pwi_state != PWI_VALID) 
            return EINVAL;
        
        if (e < 0 || e > (pi->pwi_nument - 1))
            return EINVAL;
        
        pe = pi->pwi_pwe + e;
        if (pe->pwe_pid != current_pid())
            return EACCES;
        
        if (arg3 < 0 || arg3 > (pi->pwi_nument - 1))
            return EINVAL;

        pt = pi->pwi_pwe + arg3;

        if (pt->pwe_pid == NOPID)
            return 0;
        
        pe->pwe_postouts++;

        s = mp_mutex_spinlock(&pt->pwe_lock);
	if ((pt->pwe_flag & PWE_WAITING) && !(pt->pwe_flag & PWE_POSTED))
		sv_signal(&pt->pwe_sv);
        pt->pwe_flag |= PWE_POSTED;
        pt->pwe_postins++;
        mp_mutex_spinunlock(&pt->pwe_lock, s);

        return 0;
        
    case PWIOCWAIT:        /* Wait for event */
        
        i = arg2 >> 16;
        e = arg2 & 0xffff;
    
        if (i < 0 || i > (pw_maxinst - 1))
               return EINVAL;

        pi = pwi + i;

        if (pi->pwi_state != PWI_VALID) 
            return EINVAL;
        
        if (e < 0 || e > (pi->pwi_nument - 1))
            return EINVAL;
        
        pe = pi->pwi_pwe + e;
        if (pe->pwe_pid != current_pid())
            return EACCES;

        s = mp_mutex_spinlock(&pe->pwe_lock);

        pe->pwe_waits++;
        pe->pwe_timerval = (arg3 <= 0) ? PWE_BIGTIMER : arg3;
        pe->pwe_timerset = (arg3 > 0) ? PWE_TIMERSET : 0;

        while (! (pe->pwe_flag & PWE_POSTED) ) {
            pe->pwe_flag |= PWE_WAITING;
	    if (sv_wait_sig(&pe->pwe_sv, PW_PRIORITY, &pe->pwe_lock, s)) {
                s = mp_mutex_spinlock(&pe->pwe_lock);
                pe->pwe_flag &= ~PWE_WAITING;
                pe->pwe_timerset = 0; 
                pe->pwe_timerval = PWE_BIGTIMER; 
                mp_mutex_spinunlock(&pe->pwe_lock, s);
                return EINTR;
            }
            s = mp_mutex_spinlock(&pe->pwe_lock);
            if (pe->pwe_timerval <=0)
                break;
        }

        pe->pwe_flag &= ~PWE_WAITING;
        if (pe->pwe_flag & PWE_POSTED) {
            pe->pwe_flag &= ~PWE_POSTED;
            pe->pwe_timerset = 0; 
            pe->pwe_timerval = PWE_BIGTIMER; 
            mp_mutex_spinunlock(&pe->pwe_lock, s);
            return 0;
        } else if (pe->pwe_timerset == PWE_TIMERSET && pe->pwe_timerval <= 0) {
            pe->pwe_timerset = 0; 
            pe->pwe_timerval = PWE_BIGTIMER; 
            mp_mutex_spinunlock(&pe->pwe_lock, s);
            return EINTR;
        }

        return 0;

    case PWIOCINFO:
       
        if (arg2 < 0 || arg2 > (pw_maxinst - 1))
            return EINVAL;

        if (copyin((void *)arg3, (void *)&pwinfo, sizeof (pwinfo)))
            return EFAULT;
        
        pi = pwi + arg2;

        s = mp_mutex_spinlock(&pi->pwi_lock);

        if (pi->pwi_state != PWI_VALID) {
            mp_mutex_spinunlock(&pi->pwi_lock, s);
            return EINVAL;
        }

        pwinfo.pwi_nument = pi->pwi_nument;
        pwinfo.pwi_timer  = pw_timer;

        mp_mutex_spinunlock(&pi->pwi_lock, s);

        if (copyout((void *)&pwinfo, (void *)arg3, sizeof(pwinfo)))
            return EFAULT;

        return 0;

    case PWIOCSTATS:
        
        if (arg2 < 0 || arg2 > (pw_maxinst - 1))
            return EINVAL;

        if (COPYIN_XLATE((void *)arg3, (void *)&pwstat, sizeof (pwstat),
			 irix5_to_pwstat, get_current_abi(), 1))
            return EFAULT;

        if (pwstat.pws_nument <= 0)
            return EINVAL;
        
        pi = pwi + arg2;

        /* Get control of the statistic array                    */
        /* It should be O.K to loop here, because the holder of  */
        /* the lock will give it up when it exits the kernel     */

        s = mp_mutex_spinlock(&pw_lock);
        while (pw_statinuse) {
            mp_mutex_spinunlock(&pw_lock, s);
            qswtch(RESCHED_Y);;
            s = mp_mutex_spinlock(&pw_lock);
        }
        pw_statinuse = 1;
        nested_spinunlock(&pw_lock);

        nested_spinlock(&pi->pwi_lock);

        if (pi->pwi_state != PWI_VALID) {
            mp_mutex_spinunlock(&pi->pwi_lock, s);
            s = mp_mutex_spinlock(&pw_lock);
            pw_statinuse = 0;
            mp_mutex_spinunlock(&pw_lock,s);
            return EINVAL;
        }

        if (pwstat.pws_nument > pi->pwi_nument)
            pwstat.pws_nument = pi->pwi_nument;

        for (e = 0; e < pwstat.pws_nument; e++) {
            pe = pi->pwi_pwe + e;
            pw_stat[e].pwse_pid        = pe->pwe_pid;
            pw_stat[e].pwse_postouts   = pe->pwe_postouts;
            pw_stat[e].pwse_postins    = pe->pwe_postins;
            pw_stat[e].pwse_waits      = pe->pwe_waits;
            pw_stat[e].pwse_timeouts   = pe->pwe_timeouts;
            pw_stat[e].pwse_postmisses = pe->pwe_postmisses;
        }

        mp_mutex_spinunlock(&pi->pwi_lock, s);

        if (XLATE_COPYOUT((void *)&pwstat, (void *)arg3, sizeof(pwstat),
			  pwstat_to_irix5, get_current_abi(), 1)) {
            s = mp_mutex_spinlock(&pw_lock);
            pw_statinuse = 0;
            mp_mutex_spinunlock(&pw_lock, s);
            return EFAULT;
        }

        if (copyout((void *)pw_stat, (void *)pwstat.pws_array,
		    pwstat.pws_nument * sizeof(pwsent_t))) {
            s = mp_mutex_spinlock(&pw_lock);
            pw_statinuse = 0;
            mp_mutex_spinunlock(&pw_lock, s);
            return EFAULT;
        }

        s = mp_mutex_spinlock(&pw_lock);
        pw_statinuse = 0;
        mp_mutex_spinunlock(&pw_lock, s);

        return 0;

    default:
        return EINVAL;
    }
}

/* Go through each entry of each instance */
void
pwexit(void)
{
    register pwi_t *pi;
    register short i, e, ctr;
    register int s;

    for (i = 0; i < pw_maxinst; i++) {
        pi = pwi + i;
        s = mp_mutex_spinlock(&pi->pwi_lock);
        if (pi->pwi_state != PWI_VALID) {
            mp_mutex_spinunlock(&pi->pwi_lock, s);
            continue;
        }
        ctr = pi->pwi_nument;
        mp_mutex_spinunlock(&pi->pwi_lock, s);

        for (e = 0; e < ctr; e++)
            pwsysmips(PWIOCDEREG, PWCALLERID(i,e), 0, 0);
    }
}

#if _MIPS_SIM == _ABI64
/*ARGSUSED*/
static int pwstat_to_irix5(void *from, int count, register xlate_info_t *info)
{
     XLATE_COPYOUT_PROLOGUE(pwstat_s, irix5_pwstat_s);
     target->pws_nument = source->pws_nument;
     target->pws_array = (app32_ptr_t)(__psunsigned_t)source->pws_array;
     return 0;
}

/*ARGSUSED*/
static int irix5_to_pwstat(enum xlate_mode mode, void *to, int count, xlate_info_t *info)
{
     COPYIN_XLATE_PROLOGUE(irix5_pwstat_s, pwstat_s);
     target->pws_nument = source->pws_nument;
     target->pws_array = (void *)(__psunsigned_t)source->pws_array;
     return 0;
}

#endif /* _ABI64 */

