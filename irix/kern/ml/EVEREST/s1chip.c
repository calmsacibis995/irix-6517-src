/**************************************************************************
 *									  *
 * 		 Copyright (C) 1992, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident	"ml/EVEREST/s1chip.c: $Revision: 1.42 $"

#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/cmn_err.h>
#include <sys/reg.h>
#include <sys/debug.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/immu.h>
#include <sys/dmamap.h>
#include <sys/scsi.h>
#include <sys/wd93.h>
#include <sys/wd95a_struct.h>
#include <sys/scip.h>
#include <sys/invent.h>
#include <sys/EVEREST/everest.h>
#include <sys/EVEREST/io4.h>
#include <sys/EVEREST/s1chip.h>
#include <sys/EVEREST/evintr.h>
#include <sys/EVEREST/everror.h>

typedef void (*ScsiIntrFunc)(int);

typedef struct s1chip {
	uchar_t	s1_slot;		/* slot for io4 */
	uchar_t	s1_adap;		/* adapter number */
	uchar_t	s1_adap_type;		/* chips attached */
	ScsiIntrFunc s1_func;		/* interrupt routine */
	int	s1_intr_lvl;		/* interrupt level */
	u_char	*s1_swin;		/* small window */
	uchar_t	s1_num_chans;		/* number of channels */
	uchar_t	s1_chan[3];		/* scsi_chan assigned */
	uchar_t	scip;			/* set if scip chip */
} s1chip_t;

#define	MAX_S1_CHIPS	16		/* maximum s1 chips */
#define	MAX_S1_CHANS	3		/* maximum channel per chip */
#define	MAX_SCSI_CHANNELS	128	/* max scsi buses */
#define	MAX_CHANS_IO4	8		/* max scsi buses per io4 board */

#define	S1_DMA_INTR(_chan)	(S1_DMA_INTERRUPT + _chan * S1_DMA_INT_OFFSET)
#define	S1_CTRL(_chan)		(S1_CTRL_BASE + _chan * S1_CTRL_OFFSET)
#define	S1_DMA(_chan)		(S1_DMA_BASE + _chan * S1_DMA_OFFSET)

#define	S1_GET_REG(_addr, _reg)		load_double((long long *)(_addr + _reg))
#define	S1_PUT_REG(_addr, _reg, _value)	store_double((long long *)(_addr + _reg), (long long)(_value))

#define	S1_ENABLE_DIFF(_win)	store_double((long long *)(SWIN_BASE(_win, 1) + 0x7008), (1<<31))

#define	S1_RESET_BUSES		(S1_SC_S_RESET_0 | S1_SC_S_RESET_1 | S1_SC_S_RESET_2)

#define	ADDR_93B_OFFSET		7
#define	DATA_93B_OFFSET		15

s1chip_t s1chips[MAX_S1_CHIPS];
int	s1_num;
int	wd95cnt;
int	scip_maxadap;

scuzzy_t scuzzy[MAX_SCSI_CHANNELS];
extern int wd93cnt;

/* forward defines */
static s1_find_type(s1chip_t *);
static s1_set_prg(s1chip_t *, int);


void
s1_init(unchar slot, unchar padap, unchar window, int scip)
{
	register int		chip_num, chan_num;
	u_char			*swin;
	register s1chip_t	*s1p;
	register uint		reg_val;
	int			driver_bus_num, level;
	static int		first_io4 = -1;

	if (first_io4 == -1)
		first_io4 = slot;
	if (slot == first_io4)
		slot = 0;

	chip_num = s1_num;
	s1_num++;
	ASSERT(s1_num < MAX_S1_CHIPS);

	s1p = &s1chips[chip_num];
	s1p->s1_slot = slot;
	s1p->s1_adap = padap;
	s1p->s1_swin = swin = (u_char *) SWIN_BASE(window, padap);
	s1p->scip = (char) scip;

	S1_ENABLE_DIFF(window);		/* go enable the differential busses */

	S1_PUT_REG(swin, S1_PIO_CLEAR, 0);

	reg_val = (uint)S1_GET_REG(swin, S1_IBUS_ERR_STAT);
	S1_PUT_REG(swin, S1_IBUS_ERR_STAT, reg_val);

	reg_val = (uint)S1_GET_REG(swin, S1_STATUS_CMD);
	S1_PUT_REG(swin, S1_STATUS_CMD, reg_val);

	if (s1_find_type(s1p) < 0) {
		cmn_err(CE_NOTE, "no scsi channels on slot %d, adap %d\n", slot, padap);
		return;
	}

	switch (s1p->s1_adap_type) {
	    case S1_H_ADAP_95A:
		s1p->s1_func = wd95intr;
		break;

	    case S1_H_ADAP_93B:
		s1p->s1_func = wd93intr;
		break;
	}

	for (chan_num = 0; chan_num < s1p->s1_num_chans; chan_num++) {
		register scuzzy_t *sp;
		register int bus_slot;

		switch (s1p->s1_adap) {
		    case 4:
			bus_slot = 0;
			break;
		    case 5:
			bus_slot = 2;
			break;
		    case 6:
			bus_slot = 5;
		}

		driver_bus_num = s1p->s1_slot * MAX_CHANS_IO4 +
				 bus_slot + chan_num;
		s1p->s1_chan[chan_num] = driver_bus_num;
		sp = &scuzzy[driver_bus_num];

		switch (s1p->s1_adap_type) {
		case S1_H_ADAP_95A:
			if (scip) {
				if (driver_bus_num > scip_maxadap)
					scip_maxadap = driver_bus_num;
			}
			else {
				if (driver_bus_num >= wd95cnt)
					wd95cnt = driver_bus_num + 1;
			}
			sp->d_addr = (volatile u_char *)(s1p->s1_swin +
					S1_CTRL(chan_num));
			break;

		case S1_H_ADAP_93B:
			if (driver_bus_num >= wd93cnt)
				wd93cnt = driver_bus_num + 1;
			{
			extern int wd93burstdma;

			sp->d_addr = (volatile u_char *)(s1p->s1_swin +
					 S1_CTRL(chan_num) + ADDR_93B_OFFSET);
			sp->d_data = (volatile u_char *)(s1p->s1_swin +
					 S1_CTRL(chan_num) + DATA_93B_OFFSET);
			sp->d_clock = 0x90;
			wd93burstdma = 1;
			break;
			}
		}

		sp->s1_base = (volatile u_char *) s1p->s1_swin + 4;
		sp->dma_write = (volatile u_int *)
			(s1p->s1_swin + S1_DMA(chan_num) + S1_DMA_WRITE_OFF + 4);
		sp->dma_read = (volatile u_int *)
			(s1p->s1_swin + S1_DMA(chan_num) + S1_DMA_READ_OFF + 4);
		sp->dma_xlatlo = (volatile u_int *)
			(s1p->s1_swin + S1_DMA(chan_num) + S1_DMA_TRANS_LOW + 4);
		sp->dma_xlathi = (volatile u_int *)
			(s1p->s1_swin + S1_DMA(chan_num) + S1_DMA_TRANS_HIGH + 4);
		sp->dma_flush = (volatile u_int *)
			(s1p->s1_swin + S1_DMA(chan_num) + S1_DMA_FLUSH + 4);
		sp->dma_reset = (volatile u_int *)
			(s1p->s1_swin + S1_DMA(chan_num) + S1_DMA_CHAN_RESET + 4);
		sp->statcmd = (volatile u_int *) (sp->s1_base + S1_STATUS_CMD);
		sp->ibuserr = (volatile u_int *) (sp->s1_base + S1_IBUS_ERR_STAT);
		sp->channel = chan_num;
	}
	evintr_connect((evreg_t *)(swin + S1_DMA_INTR(0)),
		EVINTR_LEVEL_S1_CHIP(chip_num),
		SPLDEV,
		EVINTR_DEST_S1CHIP,
		scip ? (EvIntrFunc)scipintr : (EvIntrFunc)s1chip_intr,
		scip ? (void *) (__psunsigned_t) s1p->s1_chan[0] :
		       (void *) (__psunsigned_t) chip_num);
	level = S1_GET_REG(swin, S1_DMA_INTR(0));
	S1_PUT_REG(swin, S1_DMA_INTR(1), level);
	S1_PUT_REG(swin, S1_DMA_INTR(2), level);
	S1_PUT_REG(swin, S1_INTERRUPT, level);
}

/* ARGSUSED */
void
s1chip_intr(eframe_t *ep, void *arg)
{
	register s1chip_t	*s1p = &s1chips[(__psunsigned_t)arg];
	register u_char		*swin = s1p->s1_swin;
	register int		 status;

	status = S1_GET_REG(swin, S1_STATUS_CMD);
	if (status & S1_SC_INT_CNTRL_ANY) {
		register int	chan_num;

		for (chan_num = 0; chan_num < s1p->s1_num_chans; chan_num++) {
			if (status & S1_SC_INT_CNTRL(chan_num)) {
				(*s1p->s1_func)(s1p->s1_chan[chan_num]);
			}
		}
	}
}

static
s1_find_type(s1chip_t *s1p)
{
	register volatile u_char 	*ha;

	s1_set_prg(s1p, S1_H_ADAP_95A);
	ha = (s1p->s1_swin + S1_CTRL(0));
	if (wd95_present((volatile char *)ha)) {
		s1p->s1_adap_type = S1_H_ADAP_95A;
		ha = (s1p->s1_swin + S1_CTRL(2));
		if (wd95_present((volatile char *)ha)) {
			s1p->s1_num_chans = 3;
		} else {
			s1p->s1_num_chans = 2;
		}
	}
	else {
		s1_set_prg(s1p, S1_H_ADAP_93B);
		s1p->s1_num_chans = 1;
		s1p->s1_adap_type = S1_H_ADAP_93B;
	}
	return s1p->s1_num_chans;
}

/* dma programming values */
#define	DMA_PGM_LENGTH		16
static char dmapg_r_95a[] = { 0x07, 0x47, 0x47, 0x03, 0x03, 0x03, 0x03, 0x03,
			      0x02, 0x02, 0x13, 0x23, 0x47, 0x47, 0x07, 0x07 };
static char dmapg_w_95a[] = { 0x07, 0x47, 0x47, 0x03, 0x03, 0x03, 0x03, 0x01,
			      0x01, 0x03, 0x13, 0x23, 0x47, 0x47, 0x07, 0x07 };
static char dmapg_r_93b[] = { 0x07, 0x47, 0x47, 0x03, 0x02, 0x02, 0x13, 0x03,
			      0x23, 0x47, 0x47, 0x00, 0x00, 0x00, 0x00, 0x00 };
static char dmapg_w_93b[] = { 0x07, 0x47, 0x47, 0x03, 0x01, 0x01, 0x13, 0x03,
			      0x23, 0x47, 0x47, 0x00, 0x00, 0x00, 0x00, 0x00 };
/* branch values and offsets */
#define	HA_DMA_RD_BRANCH	0
#define	HA_DMA_WR_BRANCH	2

static char dmapg_b_95a[] = { 0x03, 0x07, 0x03, 0x07 };
static char dmapg_b_93b[] = { 0x03, 0x04, 0x03, 0x04 };

/* micro processor interface values */
#define	UPINTF_PGM_LENGTH	16
static char uintf_r_95a[] = { 0x03, 0x03, 0x07, 0x07, 0x03, 0x43, 0x43, 0x42,
			      0x62, 0x62, 0x42, 0x42, 0x42, 0xc2, 0x42, 0x43 };
static char uintf_w_95a[] = { 0x03, 0x03, 0x07, 0x07, 0x43, 0x43, 0x61, 0x61,
			      0x41, 0x41, 0x41, 0x41, 0x41, 0x43, 0x43, 0x43 };
static char uintf_r_93b[] = { 0x43, 0x43, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42,
			      0x42, 0x42, 0x42, 0x42, 0xc3, 0x43, 0x43, 0x43 };
static char uintf_w_93b[] = { 0x43, 0x43, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
			      0x41, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43, 0x43 };

#define	HA_UP_RD_BRANCH		0
#define	HA_UP_RD_END		2
#define	HA_UP_WR_BRANCH		3
#define	HA_UP_WR_END		5

static char uintf_b_95a[] = { 0xa, 0x0, 0xf, 0x8, 0x0, 0xf };
static char uintf_b_93b[] = { 0x0, 0x0, 0xe, 0x0, 0x0, 0xb };

/* config offsets for the values for the upintf and dma pgms */
#define	HA_CFG_UPINTF		0
#define	HA_CFG_DMA		1

static short config_95a[] = { 0xd40, 0x7f };
static short config_93b[] = { 0xc80, 0x6b };

typedef struct s1_pgm_info {
	char	*dma_r;
	char	*dma_w;
	char	*dma_b;
	char	*up_r;
	char	*up_w;
	char	*up_b;
	short	*cfg_d_u;
} s1_pgm_info_t;

s1_pgm_info_t	s1_pgmi_95a = {
	dmapg_r_95a, dmapg_w_95a, dmapg_b_95a,
	uintf_r_95a, uintf_w_95a, uintf_b_95a,
	config_95a
};
s1_pgm_info_t	s1_pgmi_93b = {
	dmapg_r_93b, dmapg_w_93b, dmapg_b_93b,
	uintf_r_93b, uintf_w_93b, uintf_b_93b,
	config_93b
};

static
s1_set_prg(
	s1chip_t	*s1p,
	int		ha_type)
{
	register char	*wp, *rp;
	caddr_t		drp, dwp, sp;
	int		i;
	s1_pgm_info_t	*dp;

	sp = (caddr_t)s1p->s1_swin;

	switch (ha_type) {
	    case S1_H_ADAP_95A:
		dp = &s1_pgmi_95a;
		S1_PUT_REG(sp, S1_STATUS_CMD, (S1_SC_DATA_16 | 1));
		break;

	    case S1_H_ADAP_93B:
		dp = &s1_pgmi_93b;
		S1_PUT_REG(sp, S1_STATUS_CMD, (S1_SC_DATA_16 | 0));
		break;

	    default:
		return -1;
	}

	S1_PUT_REG(sp, S1_STATUS_CMD, (S1_RESET_BUSES | 0));

	/* go program the dma write and read programs */
	wp = dp->dma_w;
	rp = dp->dma_r;
	drp = sp + S1_INTF_R_SEQ_REGS;
	dwp = sp + S1_INTF_W_SEQ_REGS;

	for (i = 0; i < DMA_PGM_LENGTH; i++) {
		S1_PUT_REG(drp, 0, *rp++);
		S1_PUT_REG(dwp, 0, *wp++);
		drp += 8;
		dwp += 8;
	}

	S1_PUT_REG(sp, S1_INTF_R_OP_BR_0, dp->dma_b[HA_DMA_RD_BRANCH]);
	S1_PUT_REG(sp, S1_INTF_R_OP_BR_1, dp->dma_b[HA_DMA_RD_BRANCH + 1]);

	S1_PUT_REG(sp, S1_INTF_W_OP_BR_0, dp->dma_b[HA_DMA_WR_BRANCH]);
	S1_PUT_REG(sp, S1_INTF_W_OP_BR_1, dp->dma_b[HA_DMA_WR_BRANCH + 1]);

	/* go program the dma write and read programs */
	wp = dp->up_w;
	rp = dp->up_r;
	drp = sp + S1_UP_R_SEQ_REGS;
	dwp = sp + S1_UP_W_SEQ_REGS;

	for (i = 0; i < UPINTF_PGM_LENGTH; i++) {
		S1_PUT_REG(drp, 0, *rp++);
		S1_PUT_REG(dwp, 0, *wp++);
		drp += 8;
		dwp += 8;
	}

	S1_PUT_REG(sp, S1_UP_R_OP_BR_0, dp->up_b[HA_UP_RD_BRANCH]);
	S1_PUT_REG(sp, S1_UP_R_OP_BR_1, dp->up_b[HA_UP_RD_BRANCH + 1]);
	S1_PUT_REG(sp, S1_UP_R_END, dp->up_b[HA_UP_RD_END]);

	S1_PUT_REG(sp, S1_UP_W_OP_BR_0, dp->up_b[HA_UP_WR_BRANCH]);
	S1_PUT_REG(sp, S1_UP_W_OP_BR_1, dp->up_b[HA_UP_WR_BRANCH + 1]);
	S1_PUT_REG(sp, S1_UP_W_END, dp->up_b[HA_UP_WR_END]);

	/* set up the conf regs */
	S1_PUT_REG(sp, S1_INTF_CONF, dp->cfg_d_u[HA_CFG_DMA]);
	S1_PUT_REG(sp, S1_UP_CONF, dp->cfg_d_u[HA_CFG_UPINTF]);

	S1_PUT_REG(sp, S1_STATUS_CMD, (S1_RESET_BUSES | 1));

	return 0;
}


void
wd93_init(void)
{
	int	i, j;

	for (i = 0; i < s1_num; i++)
	{
		if (s1chips[i].scip)
		{
			if (scipfw_download(s1chips[i].s1_swin + 4))
				continue;
			for (j = 0; j < s1chips[i].s1_num_chans; j++)
				scip_earlyinit(s1chips[i].s1_chan[j]);
			scip_startup(s1chips[i].s1_swin + 4);
		}
		else if (s1chips[i].s1_adap_type == S1_H_ADAP_95A)
		{
			for (j = 0; j < s1chips[i].s1_num_chans; j++)
				wd95_earlyinit(s1chips[i].s1_chan[j]);
		}
	}
}


/*
 * Return non-zero if SCSI bus goes external to chassis/cabinet.
 * Generally implies that sync mode limited to 5 MT/s.
 */
int
scsi_is_external(const int adap)
{
	register scuzzy_t	*scuzzi;

	scuzzi = &scuzzy[adap];
	return *((volatile u_int *) (scuzzi->s1_base + S1_DRIVERS_CONF)) &
	       (0x20 << (3 * scuzzi->channel));
}


/*
 * Start up the S1 DMA engine.
 */
void
wd93_go(dmamap_t *map, caddr_t addr, int readflag)
{
	register uint offset;

        offset = ev_kvtoiopnum(map->dma_virtaddr);
        *scuzzy[map->dma_adap].dma_xlatlo = io_ctob(offset) |
			  ((__psunsigned_t) map->dma_virtaddr & IO_NBPC-1);
        *scuzzy[map->dma_adap].dma_xlathi = offset >> 20;

	offset = (__psunsigned_t) addr & (NBPC -1);
	if (readflag)
		*(volatile int *) scuzzy[map->dma_adap].dma_write = offset;
	else
		*(volatile int *) scuzzy[map->dma_adap].dma_read = offset;
}

/*
 * Start up S1 DMA engine using direct user dma.
 */
void
wd95_usergo(dmamap_t *map, struct buf *bp, char *curaddr, int readflag)
{
	register uint			pgno;
	register __psunsigned_t		addr;

	/*
	 * Find the address translation that corresponds to curaddr.  Since
	 * we may restart DMA multiple times at multiple places in our buffer,
	 * we will need to find out which translation entry corresponds to
	 * curaddr.
	 */
	addr = (__psunsigned_t) bp->b_un.b_addr;
	pgno = io_btoct(io_poff(addr) + (__psunsigned_t) curaddr - addr);
	addr = (__psunsigned_t) bp->b_private + 4 * pgno;

	/*
	 * We now have the address of the first translation entry.
	 * Program the S1 DMA registers to point to our it.
	 */
	pgno = ev_kvtoiopnum((caddr_t) addr);
	*scuzzy[map->dma_adap].dma_xlatlo = io_ctob(pgno) | (addr & IO_NBPC-1);
	*scuzzy[map->dma_adap].dma_xlathi = pgno >> 20;

	/*
	 * Now enable the S1 by writing its DMA offset register.
	 */
	addr = (__psunsigned_t) curaddr & (IO_NBPC -1);
	if (readflag)
		*(volatile int *) scuzzy[map->dma_adap].dma_write = addr;
	else
		*(volatile int *) scuzzy[map->dma_adap].dma_read = addr;
}


/*
 * Flush the DMA and check for an S1/IBUS error.
 * Returns 0 if the S1 detects no error on this slice.
 */
/* ARGSUSED */
int
scsidma_flush(dmamap_t *map, int readflag)
{
	register scuzzy_t	*scuzzi;
	register int	 	 adap;		/* external adaptor # */
	register int		 chan;		/* channel # on S1 */
	register ulong		 stat;
	int			 retcode = 0;

	adap = map->dma_adap;
	scuzzi = &scuzzy[adap];
	chan = scuzzi->channel;

	/*
	 * Flush DMA and wait.
	 */
	stat = 0;
	*scuzzi->dma_flush = 0;
	while (*scuzzi->statcmd & S1_SC_DMA(chan))
		if (++stat == 5000)
		{
			retcode = 1;
			break;
		}
	
	if (io4ia_war && io4_flush_cache((paddr_t) scuzzi->statcmd))
		panic("SCSI IO4 cache flush");

#define S1_HANDLE_IBUS_ERR 0
#if S1_HANDLE_IBUS_ERR
	/*
	 * We have to check for the case where channel one or two did
	 * a clear on channel zero.
	 */
	if (chan == 0 && scuzzi->errchan0 != 0)
	{
		printf("S1: IBUS ERROR: IO4 slot %d bus %d\n",
		       (adap / MAX_CHANS_IO4),
		       (adap % MAX_CHANS_IO4));
		printf("               Error cleared by other channel\n");
		scuzzi->errchan0 = 0;
		retcode = 1;
	}
#endif

	/*
	 * Check for S1 errors (on the SCSI chip side, mostly)
	 */
	stat = *scuzzi->statcmd;
	if (stat & S1_SC_ERR_MASK(chan))
	{
		printf("S1: STATUS ERROR: IO4 slot %d bus %d\n",
		       (adap / MAX_CHANS_IO4),
		       (adap % MAX_CHANS_IO4));
		printf("                 S1 status register 0x%x\n", stat);

		if (stat & S1_SC_PIO_DROP)
			*((int *) &scuzzi->s1_base[S1_PIO_CLEAR]) = 0;

		if (stat & S1_SC_D_ERR(chan))
		{
			*scuzzi->dma_reset = 0;
			DELAY(1000000);
		}

		/* clear the errors that we care about */
		*scuzzi->statcmd = (stat & S1_SC_ERR_MASK(chan));

		retcode = 1;
	}

#if S1_HANDLE_IBUS_ERR
	/*
	 * Check for an IBUS error.
	 *
	 * To workaround a chip bug, we have to clear an error on channel 0
	 * if we get one on a different channel after an error has occurred
	 * on 0.  Thus, if we aren't channel 0, and there is an error on 0,
	 * we clear it here, and set a bit in the scuzzi struct.
	 */
	if (stat & S1_SC_IBUS_ERR)
	{
	    stat = *scuzzi->ibuserr;

	    if (stat & (S1_IERR_ERR_CH(chan) | S1_IERR_OVR_CH(chan)))
	    {
		printf("S1: IBUS ERROR: IO4 slot %d bus %d\n",
		       (adap / MAX_CHANS_IO4),
		       (adap % MAX_CHANS_IO4));
		printf("         IBUS Error Status 0x%x\n", stat);
		printf("         IBUS error log registers 0x%x 0x%x\n",
		       *((int *) &scuzzi->s1_base[S1_IBUS_LOG_LOW]),
		       *((int *) &scuzzi->s1_base[S1_IBUS_LOG_HIG]));
		
		/*
		 * clear the errors
		 */
		if (chan != 0)
		{
		    printf("         Clearing error in channel 0\n");
		    if (stat & S1_IERR_ERR_CH(0))
		    {
			*scuzzi->ibuserr = S1_IERR_ERR_ALL(0);
			scuzzi->errchan0 = 1;
		    }
		    if (stat & S1_IERR_OVR_CH(0))
		    {
			*scuzzi->ibuserr = S1_IERR_OVR_ALL(0);
			scuzzi->errchan0 = 2;
		    }
		}

		if (stat & S1_IERR_ERR_CH(chan))
		    *scuzzi->ibuserr = S1_IERR_ERR_ALL(chan);
		if (stat & S1_IERR_OVR_CH(chan))
		    *scuzzi->ibuserr = S1_IERR_OVR_ALL(chan);

		retcode = 1;
	    }
	}
#else
	if (stat & S1_SC_IBUS_ERR)
		everest_error_handler(0, 0);
#endif

	if (retcode)
	{
		stat = *scuzzi->statcmd & S1_SC_ERR_MASK(chan);
		if (stat)
			printf("S1: error after resetting chip;"
			       " CSR == 0x%x\n", stat);
		scuzzi->dmaerror = 1;
	}

	return retcode;
}


/* ARGSUSED */
int
wd93dma_flush(dmamap_t *map, int readflag)
{
	register int adap;
	register int retcode;
	register scuzzy_t *scuzzi;

	adap = map->dma_adap;
	scuzzi = &scuzzy[adap];
	retcode = scuzzi->dmaerror;
	scuzzi->dmaerror = 0;
	return retcode;
}


/*
 * wd93_int_present - is a wd93 interrupt present
 * In machine specific code because of IP4
 */
wd93_int_present(wd93dev_t *dp)
{
	/* reads the S1 command status */
	return (*scuzzy[dp->d_adap].statcmd &
		S1_SC_INT_CNTRL(scuzzy[dp->d_adap].channel));
}


/*
 * Yank the reset line and reset the DMA
 */
void
wd93_resetbus(int adap)
{
	*scuzzy[adap].statcmd = (2 << scuzzy[adap].channel) | 0;
	DELAY(200);
	*scuzzy[adap].statcmd = (2 << scuzzy[adap].channel) | 1;
	*scuzzy[adap].dma_reset = 0;
}


void *
wd93_getchanmap(void *map)
{
	return map;
}


#ifdef EVEREST
/* Kludge IA workaround */
/* ARGSUSED */
int
possibly_diff_io4(dev_t dev1, dev_t dev2)
{
	unsigned int ctlr1;
	unsigned int ctlr2;
	scsi_part_info_t *pinfo;

	/*
         * extract controller numbers for both devices.  On failure return
         * true.  Note: This is more expensive than the pre HWG dev_t
         * comparisons -San
	 */

	if ((pinfo = scsi_part_info_get(hwgraph_connectpt_get(dev1))) == NULL)
		return 1;
	ctlr1 = device_controller_num_get(SDI_CTLR_VHDL(SPI_DISK_INFO(pinfo)));

	if ((pinfo = scsi_part_info_get(hwgraph_connectpt_get(dev2))) == NULL)
		return 1;

	ctlr2 = device_controller_num_get(SDI_CTLR_VHDL(SPI_DISK_INFO(pinfo)));

	if (ctlr1 < SCSI_MAXCTLR && ctlr2 < SCSI_MAXCTLR)
	{
		return ((ctlr1 >> 3) != (ctlr2 >> 3));
	}

	return 1;
}
#endif
