/*
 *=============================================================================
 *		File:		mace.c
 *		Purpose:	This file contains the MACE 'driver'. 
 *				It is responsible for the registering,
 *				deregistering and handling interrupts 
 *				that fanout at the MACE level. 
 *
 *		NOTE:		There are exactly two CRIME interrupts
 *				that fall into this category:
 *					- MACE_PERIPH_MISC
 *					- MACE_PERIPH_SERIAL
 *				Both are MACE-ISA interrupts.
 *
 *     Right now, if CRIME interrupt x fans out at MACE, we expect all drivers
 *     (that share this CRIME line) to register through setmaceisavector(), so
 *     that this interrupt can be serviced satisfactorily within: mace_intr()
 * 
 *=============================================================================
 */

#include "sys/types.h"
#include "ksys/xthread.h"
#include "sgidefs.h"
#include "sys/pda.h"
#include "sys/sysinfo.h"
#include "sys/ksa.h"
#include "sys/reg.h"
#include "sys/sbd.h"
#include "sys/crime.h"
#include "sys/mace.h"
#include "sys/cmn_err.h"
#include "sys/kmem.h"
#include "sys/systm.h"
#include "sys/debug.h"
#include "sys/conf.h"
#include "sys/timers.h"
#include "sys/IP32.h"
#include "sys/PCI/pciio.h"

/*
 *----------------------------------------------------------------------------
 * Macros pertaining to MACE's peripheral status/mask registers
 *----------------------------------------------------------------------------
 */
#define	MAX_VECS	 (10)		

#define	MACE_PARALLEL_INTR	(0x00000000000F0000)
#define	MACE_SERIAL_INTR	(0x00000000FFF00000)
#define MACE_PCKM_INTR		(0x0000000000000A00)

#define MACE_STATUS_REGISTER      PHYS_TO_K1(ISA_INT_STS_REG)
#define MACE_INTMSK_REGISTER      PHYS_TO_K1(ISA_INT_MSK_REG)

/*
 * XXX:
 * The pciio_pio_* calls are part of the CRIME 1.1 PIO read bug workaround.
 * Don't change them unless you understand the implications.
 */
#define	READ_MACE_STAT()    (pciio_pio_read64((uint64_t *)MACE_STATUS_REGISTER))
#define	READ_MACE_MASK()    (pciio_pio_read64((uint64_t *)MACE_INTMSK_REGISTER))
#define	WRITE_MACE_MASK(rg) (pciio_pio_write64((uint64_t)(rg), (uint64_t *)MACE_INTMSK_REGISTER))


/*
 *----------------------------------------------------------------------------
 * macevec_t: 
 * Per-MACE interrupt structure. 
 * Multiple such structures could be mapped 
 * to one particular CRIME interrupt.
 *----------------------------------------------------------------------------
 */
typedef	struct	macevec_s {
	intvec_func_t	mv_isr;		/* Interrupt handler */
	int		mv_intr;	/* CRIME interrupt number */
	__psint_t	mv_arg;		/* Dummy arg passed to isr */
	_macereg_t	mv_status;	/* MACE status passed to isr   */
	_macereg_t	mv_macebits;	/* Bits to check for in status */
	thd_int_t       mv_tinfo;       /* ithread information structure */
} macevec_t;

#define mv_tflags       mv_tinfo.thd_flags
#define	mv_pri		mv_tinfo.thd_pri
#define mv_isync        mv_tinfo.thd_isync
#define mv_thread       mv_tinfo.thd_xthread
#define mv_latstats     mv_tinfo.thd_latstats

/*
 *----------------------------------------------------------------------------
 * mace_tblent_t:
 * This is a table of various mace vectors.
 *----------------------------------------------------------------------------
 */
typedef	struct	mace_tblent_s {
	int		mt_num_intrs;		/* Num of intrs */
	macevec_t	mt_intr_info[MAX_VECS]; /* Array of macevec */
} mace_tblent_t;

/*
 *----------------------------------------------------------------------------
 * mace_tbl:
 * This is an array of mace vectors.
 *----------------------------------------------------------------------------
 */
mace_tblent_t	mace_tbl;

int	mace_active_ithread = 0;

/* From IP32intr.c */
extern	int	ithreads_enabled;

int   	mace_devflag = D_MP;
int	is_macetbl_initialized = 0;

static int is_registered_misc = 0;    /* Have we registered: PERIPH_MISC ?   */
static int is_registered_serial = 0;  /* Have we registered: PERIPH_SERIAL ? */

/*
 *----------------------------------------------------------------------------
 * Function Prototypes
 *----------------------------------------------------------------------------
 */
void	mace_intr(eframe_t *ep, __psint_t intr);
void	mace_tbl_init(void);
void	setmaceisavector(int intr, _macereg_t macebits, intvec_func_t isr);

static int		mace_is_thd(int intr, _macereg_t macebits);
static void		mace_stray(eframe_t *ep, __psint_t arg);

static void             macevec_intrd(macevec_t *mv);
static void             macevec_intrd_setup(macevec_t *mv);

/*
 *----------------------------------------------------------------------------
 * mace_mask_disable()
 * This routine disables all 'driver_mask' bits in the MACE mask register.
 *----------------------------------------------------------------------------
 */
void	mace_mask_disable(_macereg_t driver_mask)
{
	int	s;
	_macereg_t m_mask;

	s = spl7();

	m_mask = READ_MACE_MASK();
	m_mask &= ~driver_mask;
	WRITE_MACE_MASK(m_mask);

	splx(s);
}

/*
 *----------------------------------------------------------------------------
 * mace_mask_enable()
 * This routine enables all 'driver_mask' bits in the MACE mask register.
 *----------------------------------------------------------------------------
 */
void	mace_mask_enable(_macereg_t driver_mask)
{
	int	s;
	_macereg_t m_mask;

	s = spl7();

	m_mask = READ_MACE_MASK();
	m_mask |= driver_mask;
	WRITE_MACE_MASK(m_mask);

	splx(s);
}

/*
 *----------------------------------------------------------------------------
 * mace_mask_update()
 * This routine updates the contexts of the MACE mask register based on the:
 * 
 * - driver_mask = Those bits in the MACE mask register that pertain to driver.
 * - currnt_mask = The actual values of those bits in the MACE mask register
 * 	           that need to be written out to MACE.
 *----------------------------------------------------------------------------
 */
void	mace_mask_update(_macereg_t driver_mask, _macereg_t currnt_mask)
{
	int	s;
	_macereg_t m_mask;

	s = spl7();

	m_mask = READ_MACE_MASK();
	m_mask &= ~driver_mask;
	m_mask |=  currnt_mask;
	WRITE_MACE_MASK(m_mask);

	splx(s);
}

/*
 *----------------------------------------------------------------------------
 * mace_mask_read()
 * This routine allows the MACE mask register to be read while all interrupts
 * are disabled. Hopefully drivers will use this over their own macros.
 *----------------------------------------------------------------------------
 */
_macereg_t	mace_mask_read()
{
	int	s;
	_macereg_t m_mask;

	s = spl7();
	m_mask = READ_MACE_MASK();
	splx(s);

	return (m_mask);
}

/*
 *----------------------------------------------------------------------------
 * mace_mask_write()
 * This routine allows the MACE mask register to be written out while all intrs
 * are disabled. Hopefully drivers will use this over their own macros.
 *----------------------------------------------------------------------------
 */
void	mace_mask_write(_macereg_t m_mask)
{
	int	s;

	s = spl7();
	WRITE_MACE_MASK(m_mask);
	splx(s);
}

/* 
 *----------------------------------------------------------------------------
 * mace_stray()
 *----------------------------------------------------------------------------
 */
/*ARGSUSED*/
static void
mace_stray(eframe_t *ep, __psint_t arg)
{
	cmn_err(CE_WARN, "mace_stray() interrupt - CRIME int = %d\n", intr);
	return;
}

/*
 *---------------------------------------------------------------------------
 * mace_tbl_init()
 * This routine is used to initialize the mace_tbl[]
 * NOTE: In all mace vectors, 'macebits' is set to 0 initially.
 *---------------------------------------------------------------------------
 */
void
mace_tbl_init(void)
{
    	int	vec_indx;
	macevec_t	*mv;

	mace_tbl.mt_num_intrs = 0;
	for (vec_indx = 0; vec_indx < MAX_VECS; vec_indx++){
		mv = &mace_tbl.mt_intr_info[vec_indx];
		mv->mv_isr	= mace_stray;
		mv->mv_intr	= MACE_PERIPH_SERIAL;
		mv->mv_arg	= 0;
		mv->mv_status   = 0;
		mv->mv_macebits	= 0;
	}
	return;
}

/*
 *---------------------------------------------------------------------------
 * mace_is_thd()
 * This routine looks at: (intr, macebits) and 
 * returns (1) - if this interrupt is ithreaded.
 * returns (0) - if this interrupt is not ithreaded.
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
int	
mace_is_thd(int intr, _macereg_t macebits)
{
	if (macebits & MACE_PARALLEL_INTR)
		return (1);
	if (macebits & MACE_SERIAL_INTR)
		return (1);
	if (macebits & MACE_PCKM_INTR)
		return (1);
	return (0);
}

/*
 *---------------------------------------------------------------------------
 * setmaceisavector()
 * This routine is invoked by MACE-ISA drivers for interrupt registration.
 * The drivers provide: 
 *
 *	- intr:		The CRIME interrupt in question
 *	- macebits:	The bits they expect to see in MACE-ISA status reg.
 *	- isr:		The interrupt service routine to be invoked.
 *
 * Within this routine, we create a new mace vector and insert it into the 
 * mace_tlb. We also initialize the ithread within which this isr will 
 * execute. Before doing so, we check to ensure that the 'macebits' field
 * doesn't overlap with other mace vectors that map into the same index in
 * the mace_tbl[].
 * 
 * NOTE: We call setcrimevector() once for each CRIME interrupt, providing
 * mace_intr() as the isr that is to be used to field this CRIME interrupt.
 *---------------------------------------------------------------------------
 */
void	
setmaceisavector(int intr, _macereg_t macebits, intvec_func_t isr)
{
	int		thd_no;
    	int		vec_indx, s;		
	macevec_t	*mv, *free_mv;	
	char		thd_name[32];
	extern int	default_intr_pri;


	s = spl7();

	if (!is_macetbl_initialized){
		mace_tbl_init();
		is_macetbl_initialized = 1;
	}

	switch (intr){
	case MACE_PERIPH_MISC:
	    if (!is_registered_misc){
		is_registered_misc = 1;
	    	setcrimevector(intr, SPL6, mace_intr, intr, CRM_EXCL);
	    }
	    break;
	case MACE_PERIPH_SERIAL:
	    if (!is_registered_serial){
		is_registered_serial = 1;
	    	setcrimevector(intr, SPL6, mace_intr, intr, CRM_EXCL);
	    }
	    break;
	default:
	    goto error;
	}

	free_mv= 0;
	for (vec_indx = 0; vec_indx < MAX_VECS; vec_indx++){
		mv = &mace_tbl.mt_intr_info[vec_indx];		
		if (mv->mv_macebits){
			if (mv->mv_macebits & macebits)
				goto error;
		}
		else {
			if (!free_mv){
			        thd_no  = vec_indx;
				free_mv = mv;
			}
		}
	}
	if (!free_mv)
		goto error;

	mace_tbl.mt_num_intrs++;

	free_mv->mv_isr 	= isr;
	free_mv->mv_intr	= intr;
	free_mv->mv_arg		= 0;		
	free_mv->mv_status      = 0;
	free_mv->mv_macebits	= macebits;
	free_mv->mv_pri		= default_intr_pri;

	sprintf(thd_name, "mvec(%d:%d)", intr, thd_no);

        if (mace_is_thd(intr, macebits)){
                atomicSetInt(&free_mv->mv_tflags, THD_ISTHREAD|THD_REG);
                if (ithreads_enabled){
                        xthread_setup(thd_name, free_mv->mv_pri,
					&free_mv->mv_tinfo,
                                      (xt_func_t *) macevec_intrd_setup,
                                      (void *) free_mv);
                }
        }
	splx(s);
	return;

error:
	/* Some error in registering interrupt */
    	panic("setmaceisavector: Failure in registering MACE-ISA intr");
	splx(s);
	return;
}

#ifdef ITHREAD_LATENCY
#define SET_ISTAMP(latstat)	xthread_set_istamp(latstat)
#else
#define SET_ISTAMP(latstat)
#endif /* ITHREAD_LATENCY */

/*
 *----------------------------------------------------------------------------
 * mace_intr()
 * This routine is responsible for examining the MACE-ISA status register and
 * servicing all interrupts that might be set inside it. It traverses a table
 * of macevectors, examining it for relevant bits. When it finds a match, it
 * then either (a) masks off bits in mask register, schedules the appropriate 
 * ithread or  (b) invokes the required interrupt service routine in question.
 *----------------------------------------------------------------------------
 */
/*ARGSUSED*/
void
mace_intr(eframe_t *ep, __psint_t intr)
{
        macevec_t       *mv;
        _macereg_t      m_stat;
        _macereg_t      activeints;
        int             vec_indx;

        /*
         * Attempt to service all mace-fanout interrupts in one
         * invocation of mace_intr(), rather than bounce back and
         * forth to/from crime_intr(). Saves us some performance
         * Also, read MACE status/mask register just once. That's
         * cuz MACE status register changes on us when we disable
         * corresponding bits in the mask register. Ugly.
         */
        m_stat     = READ_MACE_STAT();
        activeints = m_stat;
        ASSERT(activeints);

        /*
         * We look through all the vectors in mace_tbl
         * We then match individual vector 'macebits'
         * and service interrupts, turning them off 
         * inside: activeints. Once we're done looking
         * at all the mace vectors, we can exit
         */
        for (vec_indx = 0; vec_indx < MAX_VECS; vec_indx++){
                mv = &mace_tbl.mt_intr_info[vec_indx];
                if (!activeints){
                        /* 
                         * There are no more interrupts
                         * to be serviced in: activeints
                         */
                         return;
                }
                if (activeints & mv->mv_macebits){
                        /*
                         * Found a mace vector that matches
                         * certain bits inside: activeints
                         */
                         if (mv->mv_tflags & THD_OK){
                                /*
                                 * Vector is threaded
                                 * - store status in vector
                                 * - disable 'macebits' in mask
                                 * - vsema() the ithread
                                 */
                                mv->mv_status = m_stat;
				SET_ISTAMP(mv->mv_latstats);
                                mace_mask_disable(mv->mv_macebits);
                                vsema(&mv->mv_isync);
                         }
                         else {
                                /* Vector isn't threaded */
                                (*mv->mv_isr)(0, mv->mv_arg);
                         }
                         /* Done with certain 'macebits' */
                         /* Remove them from 'activeints' */
                         activeints &= ~mv->mv_macebits;
                }
        }
        return;
}

/*
 *----------------------------------------------------------------------------
 * macevec_intrd_setup()
 * This routine initializes the ithread, transfers control to: macevec_intrd() 
 * and returns. When a vsema() is issued against this, macevec_intrd() will 
 * execute from the beginning.
 *----------------------------------------------------------------------------
 */
static  void
macevec_intrd_setup(macevec_t *mv)
{
     xthread_set_func(KT_TO_XT(curthreadp), (xt_func_t *) macevec_intrd, mv);
     atomicSetInt(&mv->mv_tflags, THD_INIT);
     ipsema(&mv->mv_isync);
     /* NOT REACHED */
}
  
/*
 *----------------------------------------------------------------------------
 * macevec_intrd()
 * This routine is a wrapper around the specific isr that runs as an ithread
 * The ithread remains waiting at the very beginning. Everytime someone sig-
 * nals this ithread by means of a vsema(), the ithread will re-execute from
 * the very beginning, and invoke the required isr. Note: This is specific
 * to interrupt service routines that fan out at MACE. We do certain things      * differently within this routine (from crimevec_intrd()).
 *----------------------------------------------------------------------------
 */
static void
macevec_intrd(macevec_t *mv)
{
        /* Execute isr, passing to it the status register */
        /* bits that were tucked away in the mace vector  */

#ifdef ITHREAD_LATENCY
	xthread_update_latstats(mv->mv_latstats);
#endif
        (*mv->mv_isr)(0, mv->mv_status);

        ipsema(&mv->mv_isync);
        /* NOT REACHED */  
}
