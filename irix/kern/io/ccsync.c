/**************************************************************************
 *									  *
 * 		 Copyright (C) 1994 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or dudlicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/


/*
 * ccsync.c - Access to IP19 and IP21's cc2 reg, provide power fortran
 *		join support.
 */

#if EVEREST && !LARGE_CPU_COUNT

#ident	"$Id: ccsync.c,v 1.41 1997/05/09 21:26:08 sfc Exp $"

#include <sys/types.h>
#include <sys/param.h>
#include <ksys/ddmap.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/debug.h>
#include <sys/sysmacros.h>
#include <sys/uio.h>
#include <sys/cred.h>
#include <sys/conf.h>
#include <sys/kmem.h>
#include <sys/sema.h>
#include <sys/ddi.h>
#include <sys/pda.h>
#include <ksys/vproc.h>
#include <sys/proc.h>
#include <sys/kthread.h>
#include <sys/invent.h>
#include <sys/EVEREST/everest.h>
#include <sys/EVEREST/evconfig.h>
#include <sys/runq.h>
#include <sys/sysmp.h>
#include <sys/cmn_err.h>
#if IP19
#include <sys/EVEREST/IP19.h>
#endif /* IP19 */
#if IP21
#include <sys/EVEREST/IP21.h>
#endif /* IP21 */
#if IP25
#include <sys/EVEREST/IP25.h>
#endif /* IP25 */
#include <sys/EVEREST/ccsync.h>


int ccsyncdevflag = D_MP;

#define CCMAXGROUP	16		/* max number of groups supported */
#define SYNC_REQ_MNR	0x100		/* only valid open minor number */

#define BV_VEC 1
#ifdef LARGE_CPU_COUNT
typedef cpumask_t sbv_t;
#else
typedef unsigned long long sbv_t;
#endif

struct ccsync_s {
	pid_t		cc_pid;		/* id of master */
	__psunsigned_t	cc_id;		/* caller's vhandle id */
	sbv_t		cc_pset[BV_VEC];	/* processor set */
	char		cc_cpu;		/* allocated cpu */
	char		cc_state;		
#define CC_ALLOC	0x1		/* group allocated */
#define CC_CPU		0x2		/* cpu allocated */
#define CC_LOCKONLY	0x4		/* mustrun unlock only */
};
typedef struct ccsync_s ccsync_t;

static ccsync_t cclist[CCMAXGROUP];

#ifndef IP25
static char ccsyncstate = 0;		/* driver state */
#define ST_CC1_FOUND	0x1
#endif

#if DEBUG
static char ccdebug = 0;		/* driver debugging */
#define CC_DBGNOHW	0x1		/* No hardware */
#define CC_DBGPRINT	0x2		/* debug output */
static void *ccpage;
#endif /* DEBUG */

static mutex_t ccsync_mutex;
#ifndef IP25
static sbv_t ccfreecpus[BV_VEC];
#endif

void
ccsyncinit(void)
{
#ifndef IP25
	int i, slot, type;
#endif

#ifndef IP25
	ASSERT( EV_MAX_CPUS < (sizeof(ccfreecpus)*8) );

	/* Check for any rev 1 CCs and mark driver down if found */
	for( slot = 0 ; slot < EV_MAX_SLOTS ; slot++ ) {
		type = BOARD(slot)->eb_type;
		if( (type == EVTYPE_IP19 || type == EVTYPE_IP21) &&
			BOARD(slot)->eb_enabled &&
			(BOARD(slot)->eb_un.ebun_cpu.eb_ccrev < 2) ) {
#if DEBUG
			if( ccdebug & CC_DBGNOHW )
				continue;
#endif /* DEBUG */
			ccsyncstate |= ST_CC1_FOUND;
#if DEBUG
			if( ccdebug & CC_DBGPRINT )
				cmn_err(CE_CONT,"CC SYNC driver disabled due to rev 1 CC in slot %d\n",slot);
#endif /* DEBUG */
			return;

		}
	}
#endif /* !IP25 */

	/* general setup */
	mutex_init(&ccsync_mutex, MUTEX_DEFAULT, "ccsync");

#ifndef IP25
	/* set up free cpu bitmask */
	for( i = 0 ; i < BV_VEC ; i++ )
		ccfreecpus[i] = ~0;
#endif /* !IP25 */

#if DEBUG && !IP25
	if( ccdebug && CC_DBGNOHW ) {
		if( (ccpage = kvpalloc(1,0,0)) == NULL ) {
			ccsyncstate |= ST_CC1_FOUND;
			cmn_err(CE_NOTE,"CC SYNC DEBUG moded disbled\n");
			return;
		}
		cmn_err(CE_CONT,"CC SYNC driver in DEBUG mode\n");
	}
#endif /* DEBUG && !IP25 */

	/* Add it to the inventory */
	add_to_inventory(INV_PROCESSOR, INV_CCSYNC, 0, 0, 0);
}


/* ARGSUSED */
int
ccsyncopen(dev_t *devp, int oflag, int otyp, cred_t *crp)
{ 
	int grp;
#ifndef IP25
	int i;
#endif


	/* Make sure driver's up and valid minor number */
#ifndef IP25
	if (ccsyncstate & ST_CC1_FOUND)
		return ENODEV;
#endif
	if (geteminor(*devp) != SYNC_REQ_MNR) {
		/*
		 * allow a priveleged user to open a specific dev #
		 */
		if (!_CAP_ABLE(CAP_PROC_MGT))
			return ENODEV;

		grp = geteminor(*devp);

		if (grp < 0 || grp >= CCMAXGROUP)
			return ENODEV;

		if (cclist[grp].cc_state == 0)
			return EINVAL;

		return (0);
	}
	mutex_lock(&ccsync_mutex,PRIBIO);

	/* allocate a group */
	for( grp = 0 ; grp < CCMAXGROUP ; grp++ )
		if( cclist[grp].cc_state == 0 ) {
			cclist[grp].cc_state = CC_ALLOC;
			break;
		}

	mutex_unlock(&ccsync_mutex);

	/* All groups are in use */
	if( grp >= CCMAXGROUP )
		return EBUSY;

#if DEBUG
	if( ccdebug & CC_DBGPRINT )
		printf("syncopen: group %d allocated\n",grp);
#endif /* DEBUG */

	*devp = makedevice(getemajor(*devp),grp);

#ifndef IP25
	/* reset allowed cpuset */
	for( i = 0 ; i < BV_VEC ; i++ )
		cclist[grp].cc_pset[i] = ~0;
#endif /* !IP25 */

	return 0;
}


/* ARGSUSED */
int
ccsyncclose(dev_t dev, int oflag, int otyp, cred_t *crp)
{
	int grp = geteminor(dev);


#ifndef IP25
	ASSERT( (ccsyncstate & ST_CC1_FOUND) == 0 );
#endif /* !IP25 */
	ASSERT( grp < CCMAXGROUP );
	ASSERT( cclist[grp].cc_state & CC_ALLOC );

	mutex_lock(&ccsync_mutex,PRIBIO);

	cclist[grp].cc_state &= ~CC_ALLOC;

	mutex_unlock(&ccsync_mutex);

#if DEBUG
	if( ccdebug & CC_DBGPRINT )
		printf("syncclose: group %d\n",grp);
#endif /* DEBUG */

	return 0;
}

/* ARGSUSED */
int
ccsyncioctl(dev_t dev, int cmd, void *arg, int mode, struct cred *crp, 
	int *rvalp)
{
	int	grp = geteminor(dev);
#ifndef IP25
	int	i, ind, bit;
	sbv_t	newset[BV_VEC];
#endif /* !IP25 */
	ccsyncinfo_t info;
	cpu_cookie_t	was_running;

#ifndef IP25
	ASSERT( (ccsyncstate & ST_CC1_FOUND) == 0 );
#endif /* !IP25 */
	ASSERT( grp < CCMAXGROUP );
	ASSERT( cclist[grp].cc_state & CC_ALLOC );

#if DEBUG
	if( ccdebug & CC_DBGPRINT )
		printf("syncclose: group %d, cmd %d\n", grp, cmd);
#endif /* DEBUG */

	switch( cmd ) {

	case CCSYNCIO_PSET:
#ifndef IP25

		if( copyin(arg,(char *)&newset,sizeof(newset)) )
			return EFAULT;

		/* run a sanity check on the pset */
		for( i = 0 ; i < sizeof(newset)*8 ; i++ ) {

			ind = i / (sizeof(sbv_t)*8);
			bit = i % (sizeof(sbv_t)*8);

			if( (newset[ind] & ((sbv_t)1 << bit)) && 
				!cpu_isvalid(i) )
				return EINVAL;
		}

		mutex_lock(&ccsync_mutex,PRIBIO);
		/* It's OK, copy it over */
		for( i = 0 ; i < BV_VEC ; i++ )
			cclist[grp].cc_pset[i] = newset[i];
		mutex_unlock(&ccsync_mutex);
#endif /* !IP25 */

		break;

	case CCSYNCIO_INFO:
		/*
		 * get info about open device
		 */
		info.ccsync_master = cclist[grp].cc_pid;
		info.ccsync_flags = 0;
		if (cclist[grp].cc_state & CC_CPU) {
			info.ccsync_flags |= CCSYNC_MAPPED;

			info.ccsync_cpu = cclist[grp].cc_cpu;

			was_running = setmustrun(cclist[grp].cc_cpu);
			info.ccsync_value =
				*((uint8_t *)(EV_SYNC_SIGNAL | (grp << 20)));
			restoremustrun(was_running);
		}
		if (cclist[grp].cc_state & CC_LOCKONLY)
			info.ccsync_flags |= CCSYNC_MRLOCKONLY;

		if( copyout(&info, arg, sizeof(info)) )
			return EFAULT;
		break;

	default:
		return EINVAL;
	}

	return 0;
}

/* ARGSUSED */
int
ccsyncmap(dev_t dev, vhandl_t *vt, off_t addr, int len, u_int prot)
{
	int grp = geteminor(dev);
#ifdef IP25
	int cpu = cpuid(), err;
#else
	int bit, cpu, ind, err;
#endif
	void *regaddr;


#ifndef IP25
	ASSERT( (ccsyncstate & ST_CC1_FOUND) == 0 );
#endif /* !IP25 */
	ASSERT( grp < CCMAXGROUP );
	ASSERT( cclist[grp].cc_state & CC_ALLOC );

#ifndef IP25
	mutex_lock(&ccsync_mutex,PRIBIO);

	if( cclist[grp].cc_state & CC_CPU ) {
		err = EINVAL;
		goto err_0;
	}

	/* See if user is already constrained to a particular cpu. */
	if ( (cpu = curthreadp->k_mustrun) != PDA_RUNANYWHERE ) {
		ind = cpu / (sizeof(sbv_t) * 8);
		bit = cpu % (sizeof(sbv_t) * 8);
		ASSERT(ind < BV_VEC);
		if( !((cclist[grp].cc_pset[ind] & ((sbv_t)1 << bit)) &&
		    (ccfreecpus[ind] & ((sbv_t)1 << bit)) 
			&& cpu_isvalid(cpu)) ) {
			err = EBUSY;
			goto err_0;
		    }
		cclist[grp].cc_state |= CC_LOCKONLY;
	}
	else {
		for( cpu = maxcpus - 1 ; cpu >= 0 ; cpu-- ) {
			ind = cpu / (sizeof(sbv_t) * 8);
			bit = cpu % (sizeof(sbv_t) * 8);
			/* make sure it's in the cpuset, 
			 * it's not in use and
			 * it's valid.
			 */
			if( (cclist[grp].cc_pset[ind] & ((sbv_t)1 << bit)) &&
			    (ccfreecpus[ind] & ((sbv_t)1 << bit)) && 
			    cpu_isvalid(cpu) )
			    break;
		}

		/* a cpu isn't available which fits the bill */
		if( cpu < 0 ) {
			err = EBUSY;
			goto err_0;
		}
	}

	/* grab the cpu */
	cclist[grp].cc_state |= CC_CPU;
	ccfreecpus[ind] &= ~((sbv_t)1 << bit);

	mutex_unlock(&ccsync_mutex);

	/* force us onto a particular cpu */
	if( err = mp_mustrun(0, cpu, 1, 1) )
		goto err_1;

#endif /* !IP25 */
	/* map in the correct sync signal */
	regaddr = (void *)(__psunsigned_t)(
		(EV_SYNC_SIGNAL | (grp << 20)) & ~(NBPP-1) );

#if DEBUG
	if( ccdebug & CC_DBGPRINT )
		printf("ccsyncmap: reg addr 0x%lx, mask 0x%x\n",regaddr,
			1 << grp);

	if( ccdebug & CC_DBGNOHW)
		regaddr = ccpage;
#endif /* DEBUG */

	/* now map it in */
	if( err = v_mapphys(vt,regaddr,NBPP) ) 
		goto err_2;
	/* set the group mask register to accept our groups irqs 
	 * now that we're running on the proper cpu.
	 */
#if DEBUG
	if( !(ccdebug & CC_DBGNOHW) )
#endif /* DEBUG */
		EV_SET_LOCAL(EV_IGRMASK, 1 << grp);

	/* save some state needed to unmap... */
	cclist[grp].cc_cpu = cpu;
	cclist[grp].cc_id = v_gethandle(vt);
	cclist[grp].cc_pid = current_pid();

#if DEBUG
	if( ccdebug & CC_DBGPRINT )
		printf("map: grp %d id %lx cpu %d\n",grp,cclist[grp].cc_id,
			cclist[grp].cc_cpu);
#endif /* DEBUG */
	return 0;

	/* Back out state changes upon error */
err_2:
#ifndef IP25
	(void) mp_runanywhere(0, 1, cclist[grp].cc_state & CC_LOCKONLY);
err_1:
	mutex_lock(&ccsync_mutex,PRIBIO);
	cclist[grp].cc_state &= ~(CC_LOCKONLY | CC_CPU);
err_0:
	mutex_unlock(&ccsync_mutex);
#endif /* !IP25 */

	return err;
}

/* ARGSUSED */
int
ccsyncunmap(dev_t dev, vhandl_t *vt)
{
	int	grp = geteminor(dev);
	__psunsigned_t	id = v_gethandle(vt);
#ifndef IP25
	int	cpu, i, bit;
#endif

#ifdef IP25
	mutex_lock(&ccsync_mutex,PRIBIO);
	/* mmap calls unmap when ccsyncmap fails!  YUCK!!!!
	 * just return since we've already cleaned
	 * up the driver's state.
	 */
	if(cclist[grp].cc_id != id) {
#if DEBUG
		if ( ccdebug & CC_DBGPRINT )
			printf("unfail: grp %d id %lx cpu %d\n",grp,id,
				cclist[grp].cc_cpu);
#endif /* DEBUG */
		mutex_unlock(&ccsync_mutex);
		return 0;
	}
	mutex_unlock(&ccsync_mutex);

#else /* !IP25 */

	mutex_lock(&ccsync_mutex,PRIBIO);
	/* mmap calls unmap when ccsyncmap fails!  YUCK!!!!
	 * just return since we've already cleaned
	 * up the driver's state.
	 */
	if( ((cclist[grp].cc_state & CC_CPU) == 0) ||
	    (cclist[grp].cc_id != id)) {
#if DEBUG
		if (ccdebug & CC_DBGPRINT)
			printf("unfail: grp %d id %lx cpu %d\n",grp,id,
				cclist[grp].cc_cpu);
#endif /* DEBUG */
		mutex_unlock(&ccsync_mutex);
		return 0;
	}
	mutex_unlock(&ccsync_mutex);

#if DEBUG
	if ( ccdebug & CC_DBGPRINT )
		printf("unmap: grp %d id %lx cpu %d\n",grp,id,
			cclist[grp].cc_cpu);
#endif /* DEBUG */

	ASSERT( (ccsyncstate & ST_CC1_FOUND) == 0 );
	ASSERT( grp < CCMAXGROUP );
	ASSERT( cclist[grp].cc_id == id );

	cpu = cclist[grp].cc_cpu;
	i = cpu/(sizeof(sbv_t)*8);
	bit = cpu % (sizeof(sbv_t)*8);

	/* turn off the mustrun if the master is doing the
	 * the unmap.  If it's the child, too bad for the
	 * master, he's stuck...
	 */
	if (current_pid() == cclist[grp].cc_pid &&
	    (curuthread->ut_flags & UT_MUSTRUNLCK) &&
	     curthreadp->k_mustrun == cclist[grp].cc_cpu)
		(void) mp_runanywhere(0, 1, cclist[grp].cc_state & CC_LOCKONLY);
#if DEBUG
	else
		cmn_err(CE_NOTE,"ccsync: non-master free'd cc register\n");
#endif

	mutex_lock(&ccsync_mutex,PRIBIO);

	/* free up the cpu and the lock. */
	cclist[grp].cc_state &= ~(CC_LOCKONLY | CC_CPU);
	ccfreecpus[i] |= ((sbv_t)1 << bit); 
		
	mutex_unlock(&ccsync_mutex);

#endif /* !IP25 */
	return 0;
}

#endif /* EVEREST */
