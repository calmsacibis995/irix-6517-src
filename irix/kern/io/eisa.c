/**************************************************************************
 *									  *
 *		Copyright ( C ) 1993, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 1.13 $"

#if IP22 || IP26 || IP28

#include "sys/types.h"
#include "sys/param.h"
#include "sys/cmn_err.h"
#include "sys/debug.h"
#include "sys/sbd.h"
#include "sys/cpu.h"
#include "sys/sema.h"
#include "sys/eisa.h"
#include "sys/pio.h"
#include "sys/systm.h"

/*
 * I/O space address/read/write macros.
 */
#define INB(x)		(*(volatile u_char *)(EISAIO_TO_K1(x)))
#define INH(x)		(*(volatile u_short *)(EISAIO_TO_K1(x)))
#define INW(x)		(*(volatile u_int *)(EISAIO_TO_K1(x)))

#define OUTB(x,y)	(*(volatile u_char *)(EISAIO_TO_K1(x)) = (y))
#define OUTH(x,y)	(*(volatile u_short *)(EISAIO_TO_K1(x)) = (y))
#define OUTW(x,y)	(*(volatile u_int *)(EISAIO_TO_K1(x)) = (y))

/* generic, really belong elsewhere */
#define FALSE			0
#define TRUE			1


#define EISA_NUM_SLOTS		4

/*
 * per interrupt request vector info
 */
struct eisa_intvec {
	void	(*eisa_irq_svc[EISA_NUM_SLOTS])(long);
	long	eisa_arg[EISA_NUM_SLOTS];
	uchar_t	level;
	uchar_t reserved;
	int	count;
} eisa_int_info[16];

/*
 * per dma channel info
 */
struct eisa_ch_state {
	sema_t chan_sem;		/* inuse semaphore for each channel */
	sema_t dma_sem;			/* dma completion semaphore */
	struct eisa_dma_buf *cur_buf;	/* current eisa_dma_buf being dma'ed */
	struct eisa_dma_cb *cur_cb;	/* ptr to current command block */
	int    count;
} e_ch[8];


static void eisaprobe(void);
void eisa_intr(__psint_t, struct eframe_s *);

extern int earlybadaddr(volatile void *, int);
extern void dprintf(char *, ...);

#define LOCK(x)      mutex_lock((x), PZERO)
#define UNLOCK(x)    mutex_unlock((x))
mutex_t	intr_mutex;	/* protect upper and lower half code */

void
eisa_init(void)
{
	volatile int i;

	/* Do we have an EIU? */
	if (earlybadaddr((volatile void *)PHYS_TO_COMPATK1(EIU_MODE_REG), 4)) {
		dprintf("EIU missing -- disabling EISA.\n");
		return;
	}

	/* Setup EIU */
	*(uint_t *)PHYS_TO_COMPATK1(EIU_PREMPT_REG) = 0xffff;
	*(uint_t *)PHYS_TO_COMPATK1(EIU_QUIET_REG) = 0x1;
	*(uint_t *)PHYS_TO_COMPATK1(EIU_MODE_REG) = 0x40f3c07f;

	OUTB(ExtNMIReset, 0x1);		/* Eisa bus reset */
	for (i = 0; i < 10000; i++)
		/* too early to use us_delay */;
	OUTB(ExtNMIReset, 0x0);

	/*
	 * XXX Some might say that I should use defines for the
	 * constant values assigned to these registers, but I
	 * don't see how this functionality can be comprehended w/o
	 * a copy of the ISP spec sitting in front of you.  In that
	 * situation defines would only add another confusing level
	 * of indirection.  These values shouldn't be changed anyway.
	 */

	OUTB(Int1Control, 0x11);	/* ICW1 */
	OUTB(Int2Control, 0x11);	/* ICW1 */

	OUTB(Int1Mask, 0x0);		/* ICW2  ID vector */
	OUTB(Int2Mask, 0x8);		/* ICW2  ID vector */

	OUTB(Int1Mask, 0x4);		/* ICW3  Master */
	OUTB(Int2Mask, 0x2);		/* ICW3  Slave */

	OUTB(Int1Mask, 0x1);		/* ICW4  8086 Mode */
	OUTB(Int2Mask, 0x1);		/* ICW4  8086 Mode */

	/* mask all except cascaded interrupt */
	OUTB(Int1Mask, ~(0x4));		/* OCW1 mask ints */
	OUTB(Int2Mask, 0xff);		/* OCW1 mask ints */

	/* enable dma cascade */
	OUTB(Dma47SingleMask, 0x0);

	/* register the interrupt handler */
	setlclvector(VECTOR_EISA, eisa_intr, 0);

	for (i = 0; i < 8; i++) {
		init_sema(&e_ch[i].dma_sem, 0, "eisadma", i);
		init_sema(&e_ch[i].chan_sem, 1, "eisachan", i);
	}

	mutex_init(&intr_mutex, MUTEX_DEFAULT, "eisa_mutex");

	eisaprobe();
}

void
eisa_refresh_on(void)
{
	/* setup refresh timer */
	OUTB(IntTimer1CommandMode, 0x74);
	OUTB(IntTimer1RefreshRequest, 0x12);
	OUTB(IntTimer1RefreshRequest, 0x0);
}

void
eisa_refresh_off(void)
{
	/* stop refresh timer */
	OUTB(IntTimer1CommandMode, 0x78);
	OUTB(IntTimer1RefreshRequest, 0x0);
	OUTB(IntTimer1RefreshRequest, 0x0);
}

/*ARGSUSED*/
void
eisa_intr(__psint_t arg, struct eframe_s *ep)
{
	struct eisa_intvec *evec;
	uchar_t isr, tc;
	int i;
	static time_t eint_lbolt;
	static int eint_strays;

	isr = *(volatile uchar_t *)PHYS_TO_COMPATK1(EIU_INTRPT_ACK);
	ASSERT(isr <= 15);

	LOCK(&intr_mutex); /* execute exclusive of upper half */
	evec = &eisa_int_info[isr];

	tc = INB(Dma03StatusCommand);
	for (i = 0; i < 4; i++) {
		if (tc & 0x1) {
			cvsema(&e_ch[i].dma_sem);
			cvsema(&e_ch[i].chan_sem);
		}
		tc >>= 1;
	}
	tc = INB(Dma47StatusCommand);
	for (i = 4; i < 8; i++) {
		if (tc & 0x1) {
			cvsema(&e_ch[i].dma_sem);
			cvsema(&e_ch[i].chan_sem);
		}
		tc >>= 1;
	}

	if (!evec->eisa_irq_svc[0]) {
		if (lbolt < eint_lbolt + HZ) {
			if (eint_strays++ > 2) {	/* disable ints */
				cmn_err(CE_WARN, "eisa_intr: too many stray interrupts--disabling EISA interrupts\n");
				setlclvector(VECTOR_EISA, 0, 0);
				UNLOCK(&intr_mutex);
				return;
			}
		} else {
			eint_lbolt = lbolt;
			eint_strays = 1;
		}
		cmn_err(CE_WARN, "eisa_intr: stray interrupt %d\n", isr);
	}

	for (i = 0; i < EISA_NUM_SLOTS; i++) {
		if (!evec->eisa_irq_svc[i])
			break;			/* done */
		(*evec->eisa_irq_svc[i])(evec->eisa_arg[i]);
	}

	/* EOI */
	if (isr & 0x8)		/* Second controller first */
		OUTB(Int2Control,0x20);
	OUTB(Int1Control,0x20);	/* OCW2 */

	UNLOCK(&intr_mutex);
}

/*ARGSUSED1*/
int
eisa_ivec_alloc(uint_t adap, ushort_t irq_mask, uchar_t level_flag)
{
	int i, vec;
	ushort_t irq_bit;
	int irq_min;
	uchar_t reserved;

	reserved = level_flag & EISA_RESERVED_IRQ;
	level_flag &= ~EISA_RESERVED_IRQ;

	irq_bit = 0x1;
	irq_min = EISA_NUM_SLOTS - 1;

	LOCK(&intr_mutex); /* execute exclusive of the intr routine */

	for (i = 0; i < 16; i++) {
		if (   (irq_bit & irq_mask) 
		    && !eisa_int_info[i].reserved
		    && (reserved ? (eisa_int_info[i].count == 0) : 1)) {

			if (!eisa_int_info[i].count) {
				eisa_int_info[i].level = level_flag;
				vec = i;
				irq_min = 0;
				break;

			} else if (level_flag == eisa_int_info[i].level &&
			    eisa_int_info[i].count < irq_min) {
				vec = i;
				irq_min = eisa_int_info[i].count;
			}
		}
		irq_bit <<= 1;
	}
	if (irq_min < EISA_NUM_SLOTS - 1) {
		eisa_int_info[vec].count++;
		if (reserved) {
		    eisa_int_info[vec].reserved = 1;
		}
		UNLOCK(&intr_mutex);
		return vec;
	} else {
		UNLOCK(&intr_mutex);
		return -1;
	}
}


/*ARGSUSED1*/
int
eisa_ivec_set(uint_t adap, int e_irq, void (*e_intr)(long), long e_arg)
{
	struct eisa_intvec *evec;
	int i;

	if (e_irq < 0 || e_irq > 15)
		return -1;

	LOCK(&intr_mutex); /* execute exclusive of the intr routine */

	evec = &eisa_int_info[e_irq];

	for (i = 0; i < EISA_NUM_SLOTS; i++) 
		if (!evec->eisa_irq_svc[i])
			break;			/* done */

	if (i == EISA_NUM_SLOTS) {
		UNLOCK(&intr_mutex);
		return -1;
	}

	evec->eisa_irq_svc[i] = e_intr;
	evec->eisa_arg[i] = e_arg;

	if (i == 0) {
		uchar_t imr;
		uchar_t mode;

		if (evec->level)		/* level trigger */
			if (e_irq & 0x8) {
				mode = INB(Int2TrigMode);
				OUTB(Int2TrigMode,(mode|(1<<(e_irq-8))));
			} else {
				mode = INB(Int1TrigMode);
				OUTB(Int1TrigMode,(mode|(1<<e_irq)));
		}

		/* enable eisa intrpt */
		if (e_irq & 0x8) {
			imr = INB(Int2Mask);
			OUTB(Int2Mask,(imr & ~(1<<(e_irq-8))));
		} else {
			imr = INB(Int1Mask);
			OUTB(Int1Mask, (imr & ~(1<<e_irq)));
		}
	}

	UNLOCK(&intr_mutex);
	return i;
}

static void
eisaprobe(void)
{
    extern int showconfig;
    long slot;
    caddr_t slotaddr;
    caddr_t idaddr;
    uchar_t pcmp[4];		/* Compressed product ID */
    uchar_t eisa_id[8];
    register uchar_t *cp;

    for (slot = 0; slot < EISA_NUM_SLOTS; ++slot) {
	/* get EISA slot address */
	slotaddr = (caddr_t)((slot + 1) << 12);

	/* get the offset to the EISA ID */
	idaddr = slotaddr + EisaId;

	/* get the compressed ID */
	while (((pcmp[0]=INB((__psint_t)idaddr+0)) & 0xf0) == 0x70) ;
	pcmp[1] = INB((__psint_t)idaddr+1);
	pcmp[2] = INB((__psint_t)idaddr+2);
	pcmp[3] = INB((__psint_t)idaddr+3);

	if (*((int *)pcmp) != -1) {
	    /* convert it to ascii */
	    cp = eisa_id;
	    *cp++ = 0x40 | pcmp[0]>>2 & 0x1F;
	    *cp++ = 0x40 |(pcmp[0]<<3 | pcmp[1]>>5) & 0x1F;
	    *cp++ = 0x40 | pcmp[1] & 0x1F;
	    *cp++ = 0x30 | pcmp[2]>>4 & 0x0F;
	    *cp++ = 0x30 | pcmp[2] & 0x0F;
	    *cp++ = 0x30 | pcmp[3]>>4 & 0x0F;
	    *cp++ = 0x30 | pcmp[3] & 0x0F;
	    *cp = 0;
	    if (showconfig)
	    	dprintf("EISA board %s at slot %d\n", eisa_id,slot+1);

	} else if (showconfig)
	    dprintf("no EISA board found at slot %d\n", slot+1);
    }
}

/*
 * Provide exclusivity with parwbad().
 */
static volatile uint_t *lock_memory = (uint_t *)PHYS_TO_K1(LOCK_MEMORY);

#define MC_LOCK		0
#define MC_UNLOCK	1

void
orb_rmw(volatile void *addr, unsigned int m)
{
	int s;

	s = splhi();
	*lock_memory = MC_LOCK;

	*(uchar_t *)addr |= (uchar_t)m;

	*lock_memory = MC_UNLOCK;
	splx(s);
}

void
orh_rmw(volatile void *addr, unsigned int m)
{
	int s;

	s = splhi();
	*lock_memory = MC_LOCK;

	*(ushort_t *)addr |= (uchar_t)m;

	*lock_memory = MC_UNLOCK;
	splx(s);
}

void
orw_rmw(volatile void *addr, unsigned int m)
{
	int s;

	s = splhi();
	*lock_memory = MC_LOCK;

	*(ulong_t *)addr |= (ulong_t)m;

	*lock_memory = MC_UNLOCK;
	splx(s);
}

void
andb_rmw(volatile void *addr, unsigned int m)
{
	int s;

	s = splhi();
	*lock_memory = MC_LOCK;

	*(uchar_t *)addr &= (uchar_t)m;

	*lock_memory = MC_UNLOCK;
	splx(s);
}

void
andh_rmw(volatile void *addr, unsigned int m)
{
	int s;

	s = splhi();
	*lock_memory = MC_LOCK;

	*(ushort_t *)addr &= (uchar_t)m;

	*lock_memory = MC_UNLOCK;
	splx(s);
}

void
andw_rmw(volatile void *addr, unsigned int m)
{
	int s;

	s = splhi();
	*lock_memory = MC_LOCK;

	*(ulong_t *)addr &= (ulong_t)m;

	*lock_memory = MC_UNLOCK;
	splx(s);
}

uchar_t
pio_testsetb(piomap_t *map, iopaddr_t paddr, uchar_t val)
{
	int s;
	uchar_t *addr, r;

	addr = (uchar_t *)pio_mapaddr(map, paddr);

	s = splhi();
	*lock_memory = MC_LOCK;

	r = *addr;
	*addr = val;

	*lock_memory = MC_UNLOCK;
	splx(s);

	return(r);
}

ushort_t
pio_testseth(piomap_t *map, iopaddr_t paddr, ushort_t val)
{
	int s;
	ushort_t *addr, r;

	addr = (ushort_t *)pio_mapaddr(map, paddr);

	s = splhi();
	*lock_memory = MC_LOCK;

	r = *addr;
	*addr = val;

	*lock_memory = MC_UNLOCK;
	splx(s);

	return(r);
}

uchar_t
pio_testsetw(piomap_t *map, iopaddr_t paddr, ulong_t val)
{
	int s;
	ulong_t *addr, r;

	addr = (ulong_t *)pio_mapaddr(map, paddr);

	s = splhi();
	*lock_memory = MC_LOCK;

	r = *addr;
	*addr = val;

	*lock_memory = MC_UNLOCK;
	splx(s);

	return(r);
}

static struct eisa_ch_off {
	ushort_t	Address;
	ushort_t	Count;
	ushort_t	StatusCommand;
	ushort_t	Request;
	ushort_t	SingleMask;
	ushort_t	Mode;
	ushort_t	ClearBytePointer;
	ushort_t	LowPage;
	ushort_t	HighPage;
	ushort_t	HighCount;
	ushort_t	IntStatusChain;
	ushort_t	ExtMode;
} eoff[8] = {

{ DmaCh0Address, DmaCh0Count, Dma03StatusCommand, Dma03Request, Dma03SingleMask,
  Dma03Mode, Dma03ClearBytePointer, DmaCh0LowPage, DmaCh0HighPage,
  DmaCh0HighCount, Dma03IntStatusChain, Dma03ExtMode },

{ DmaCh1Address, DmaCh1Count, Dma03StatusCommand, Dma03Request, Dma03SingleMask,
  Dma03Mode, Dma03ClearBytePointer, DmaCh1LowPage, DmaCh1HighPage,
  DmaCh1HighCount, Dma03IntStatusChain, Dma03ExtMode },

{ DmaCh2Address, DmaCh2Count, Dma03StatusCommand, Dma03Request, Dma03SingleMask,
  Dma03Mode, Dma03ClearBytePointer, DmaCh2LowPage, DmaCh2HighPage,
  DmaCh2HighCount, Dma03IntStatusChain, Dma03ExtMode },

{ DmaCh3Address, DmaCh3Count, Dma03StatusCommand, Dma03Request, Dma03SingleMask,
  Dma03Mode, Dma03ClearBytePointer, DmaCh3LowPage, DmaCh3HighPage,
  DmaCh3HighCount, Dma03IntStatusChain, Dma03ExtMode },

{ 0, 0, Dma47StatusCommand, Dma47Request, Dma47SingleMask,
  Dma47Mode, Dma47ClearBytePointer, 0, 0,
  0, Dma47IntStatusChain, Dma47ExtMode },

{ DmaCh5Address, DmaCh5Count, Dma47StatusCommand, Dma47Request, Dma47SingleMask,
  Dma47Mode, Dma47ClearBytePointer, DmaCh5LowPage, DmaCh5HighPage,
  DmaCh5HighCount, Dma47IntStatusChain, Dma47ExtMode },

{ DmaCh6Address, DmaCh6Count, Dma47StatusCommand, Dma47Request, Dma47SingleMask,
  Dma47Mode, Dma47ClearBytePointer, DmaCh6LowPage, DmaCh6HighPage,
  DmaCh6HighCount, Dma47IntStatusChain, Dma47ExtMode },

{ DmaCh7Address, DmaCh7Count, Dma47StatusCommand, Dma47Request, Dma47SingleMask,
  Dma47Mode, Dma47ClearBytePointer, DmaCh7LowPage, DmaCh7HighPage,
  DmaCh7HighCount, Dma47IntStatusChain, Dma47ExtMode }

};


/*
 * eisa dma utility routines
 */

/*ARGSUSED1*/
int
eisa_dmachan_alloc(uint_t adap, uchar_t dma_mask)
{
	int i, dma_min, dmachan;
	uchar_t dma_bit;

	dma_bit = 0x1;
	dma_min = EISA_NUM_SLOTS - 1;
	dmachan = -1;

	for (i = 0; i < 8; i++) {
		if (dma_bit & dma_mask && e_ch[i].count < dma_min) {
			dma_min = e_ch[i].count;
			dmachan = i;
		}
		dma_bit <<= 1;
	}
	if (dma_min < EISA_NUM_SLOTS - 1) {
		e_ch[dmachan].count++;
		return dmachan;
	} else {
		return -1;
	}
}

struct eisa_dma_buf *
eisa_dma_get_buf(uchar_t mode)
{
	return(kmem_zalloc(sizeof(struct eisa_dma_buf), mode));
}

struct eisa_dma_cb *
eisa_dma_get_cb(uchar_t mode)
{
	return(kmem_zalloc(sizeof(struct eisa_dma_cb), mode));
}

void
eisa_dma_free_buf(struct eisa_dma_buf *ebufptr)
{
	kmem_free(ebufptr, sizeof(struct eisa_dma_buf));
}

void
eisa_dma_free_cb(struct eisa_dma_cb *ecbptr)
{
	kmem_free(ecbptr, sizeof(struct eisa_dma_cb));
}

int
eisa_dma_prog(uint_t adap, struct eisa_dma_cb *cb, int chan, uchar_t mode)
{
	uchar_t reg;
	struct eisa_ch_off *ec_off;
	struct eisa_ch_state *ec;
	struct eisa_dma_buf *ebuf;
	uint_t tcount;
	int s;

	if (adap != 0 || chan < 0 || chan > 7) {
		cmn_err(CE_WARN, "eisa%d: Invalid eisa channel (%d) specified for dma\n", adap, chan);
		return FALSE;
	}

	ec_off = &eoff[chan];
	ec = &e_ch[chan];

	if (mode == EISA_DMA_NOSLEEP) {
		if (cpsema(&ec->chan_sem) == 0)
			return FALSE;	/* couldn't acquire channel */
	} else {
		psema(&ec->chan_sem, PZERO);
	}

	/*
	 * bit fields of the modes struct in cb are organized around
	 * the mode registers.  Just need to embed the channel #.
	 */
	reg = (ushort_t)cb->moderegs | (chan & 0x3);		/* mode */
	OUTB(ec_off->Mode, reg);

	reg = ((ushort_t)cb->moderegs >> 8) | (chan & 0x3);	/* extnd mode */
	OUTB(ec_off->ExtMode, reg);

	ebuf = ec->cur_buf = cb->reqrbufs;

	s = spleisa();		/* raise spl for ClearBytePointer Ops */

	OUTB(ec_off->ClearBytePointer, 1);
	reg = ebuf->address;
	OUTB(ec_off->Address, reg);	/* lsb addr */
	reg = ebuf->address >> 8;
	OUTB(ec_off->Address, reg);	/* msb addr */
	reg = ebuf->address >> 16;
	OUTB(ec_off->LowPage, reg);	/* low page */
	reg = ebuf->address >> 24;
	OUTB(ec_off->HighPage, reg);	/* high page */

	OUTB(ec_off->ClearBytePointer, 1);
	tcount = ebuf->count - 1;
	reg = tcount;
	OUTB(ec_off->Count, reg);	/* lsb count */
	reg = tcount >> 8;
	OUTB(ec_off->Count, reg);	/* middle count */
	reg = tcount >> 16;
	OUTB(ec_off->HighCount, reg);	/* msb count */

	splx(s);

	return TRUE;
}

void
eisa_dma_disable(uint_t adap, int chan)
{
	uchar_t reg;
	struct eisa_ch_off *ec_off;
	struct eisa_ch_state *ec;

	if (adap != 0 || chan < 0 || chan > 7) {
		cmn_err(CE_WARN, "eisa%d: Invalid eisa channel (%d) specified for dma disable\n", adap, chan);
		return;
	}

	ec_off = &eoff[chan];
	ec = &e_ch[chan];

	reg = EISA_DMAMASK_SET | (chan & 0x3);
	OUTB(ec_off->SingleMask, reg);

	ec->cur_buf = NULL;
	ec->cur_cb = NULL;

	cvsema(&ec->dma_sem);
	cvsema(&ec->chan_sem);
}

void
eisa_dma_stop(uint_t adap, int chan)
{
	uchar_t reg;
	struct eisa_ch_off *ec_off;
	struct eisa_ch_state *ec;

	if (adap != 0 || chan < 0 || chan > 7) {
		cmn_err(CE_WARN, "eisa%d: Invalid eisa channel (%d) specified for dma stop\n", adap, chan);
		return;
	}

	ec_off = &eoff[chan];
	ec = &e_ch[chan];

	reg = EISA_DMAMASK_SET | (chan & 0x3);
	OUTB(ec_off->SingleMask, reg);

	reg = EISA_REQUEST_CLEAR | (chan & 0x3);
	OUTB(ec_off->Request, reg);

	ec->cur_buf = NULL;
	ec->cur_cb = NULL;

	cvsema(&ec->dma_sem);
	cvsema(&ec->chan_sem);
}

void
eisa_dma_enable(uint_t adap, struct eisa_dma_cb *cb, int chan, uchar_t mode)
{
	uchar_t reg;
	struct eisa_ch_off *ec_off;
	struct eisa_ch_state *ec;

	if (adap != 0 || chan < 0 || chan > 7) {
		cmn_err(CE_WARN, "eisa%d: Invalid eisa channel (%d) specified for dma enable\n", adap, chan);
		return;
	}

	ec_off = &eoff[chan];
	ec = &e_ch[chan];

	reg = EISA_DMAMASK_CLEAR | (chan & 0x3);
	OUTB(ec_off->SingleMask, reg);

	if (cb->proc)
		(*cb->proc)(cb->procparam);

	if (mode == EISA_DMA_SLEEP)
		psema(&ec->dma_sem, PZERO);
}

void
eisa_dma_swstart(uint_t adap, int chan, uchar_t mode)
{
	uchar_t reg;
	struct eisa_ch_off *ec_off;
	struct eisa_ch_state *ec;

	if (adap != 0 || chan < 0 || chan > 7) {
		cmn_err(CE_WARN, "eisa%d: Invalid eisa channel (%d) specified for dma enable\n", adap, chan);
		return;
	}

	ec_off = &eoff[chan];
	ec = &e_ch[chan];

	reg = EISA_DMAMASK_CLEAR | (chan & 0x3);
	OUTB(ec_off->SingleMask, reg);

	reg = EISA_REQUEST_SET | (chan & 0x3);
	OUTB(ec_off->Request, reg);

	if (mode == EISA_DMA_SLEEP)
		psema(&ec->dma_sem, PZERO);
}

#endif /* IP22 || IP26 || IP28 */
