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

#ident	"sys/EVEREST/s1chip.c: $Revision: 1.17 $"

#include <sys/types.h>
#include <sys/cmn_err.h>
#include <sys/reg.h>
#include <sys/debug.h>
#include <sys/param.h>
#include <sys/immu.h>
#include <sys/scsi.h>
#include <sys/scsidev.h>
#include <sys/EVEREST/everest.h>
#include <sys/EVEREST/io4.h>
#include <sys/EVEREST/s1chip.h>
#include <sys/EVEREST/evintr.h>
#include <libsc.h>

#define	WD95A	1

typedef struct s1chip {
	uchar_t	s1_slot;		/* slot for io4 */
	uchar_t	s1_adap;		/* adapter number */
	uchar_t	s1_win;			/* window for this bus */
	uchar_t	s1_adap_type;		/* chips attached */
	int	(*s1_func)();		/* interrupt routine */
	int	s1_intr_lvl;		/* interrupt level */
	caddr_t	s1_swin;		/* small window */
	uchar_t	s1_num_chans;		/* number of channels */
	uchar_t	s1_chan[3];		/* scsi_chan assigned */
} s1chip_t;

#include <libsk.h>

#define	MAX_S1_CHIPS		16	/* maximum s1 chips */
#define	MAX_S1_CHANS		3	/* maximum channel per chip */
#define	MAX_SCSI_CHANNELS	150	/* max scsi buses */
#define	MAX_CHANS_IO4		10	/* max scsi buses per io4 board */

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
int	s1_num = 0;

/* This variable controls whether all scsi busses are probed or note.
 * By default, we do probe all busses (multi_io4 = 1).  If you wish
 * to probe only the busses on the master_io4, set multi_io4 to 0
 * before calling ev_scsiedtinit.
 */
int	multi_io4 = 1;

extern int scsicnt;	/* equivalent of wd95cnt for wd93 */

scuzzy_t scuzzy[MAX_SCSI_CHANNELS];

static int		first_io4 = -1;
static void		s1_set_prg(s1chip_t *, int);
extern int		wd95_present(volatile char *);

void
s1_init(unchar slot, unchar padap, unchar window)
{
	register int		chip_num, chan_num;
	caddr_t			swin;
	register s1chip_t	*s1p;
	register uint		reg_val;
	int			driver_bus_num;
	int 			phys_slot = slot;

	if (first_io4 == -1)
		first_io4 = slot;
	if (slot == first_io4)
		slot = 0;

	/* By default, for speed, we only initialize the scsi channels
	 * on the master IO4.  Diagnostics, however, may want to 
	 * initialize all the S1's in the system.  If this is the case,
	 * the multi_io4 global variable should be set to 1.
	 */
	if (!multi_io4 && slot)
		return;

	chip_num = s1_num;
	s1_num++;
	ASSERT(s1_num < MAX_S1_CHIPS);

	s1p = &s1chips[chip_num];
	s1p->s1_slot = slot;
	s1p->s1_adap = padap;
	s1p->s1_win = window;
	s1p->s1_swin = swin = (caddr_t)SWIN_BASE(window, padap);

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
		/*
		 * Special case the first SCSI bus we come across
		 */
		if (chip_num == 0 && chan_num == 0)
			driver_bus_num = 0;
		else
			driver_bus_num = s1p->s1_slot * MAX_CHANS_IO4 +
					 bus_slot + chan_num;
		s1p->s1_chan[chan_num] = driver_bus_num;
		sp = &scuzzy[driver_bus_num];

		switch (s1p->s1_adap_type) {
		case S1_H_ADAP_95A:
			if (driver_bus_num >= scsicnt)
				scsicnt = driver_bus_num + 1;
			sp->d_addr = (volatile u_char *)(s1p->s1_swin +
					S1_CTRL(chan_num));
			break;
		default:
			printf("Error: unrecognized SCSI bus controller at"
			       "slot %d padap %d.\n", slot, padap);
			return;
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
}


int
s1_find_type(s1chip_t *s1p)
{
	register caddr_t	ha;

#if WD95A
	s1_set_prg(s1p, S1_H_ADAP_95A);
	ha = (caddr_t)(s1p->s1_swin + S1_CTRL(0));
	if (wd95_present(ha)) {
		s1p->s1_adap_type = S1_H_ADAP_95A;
		ha = (caddr_t)(s1p->s1_swin + S1_CTRL(2));
		if (wd95_present(ha)) {
			s1p->s1_num_chans = 3;
		} else {
			s1p->s1_num_chans = 2;
		}
	} else
#endif
	{
		s1_set_prg(s1p, S1_H_ADAP_93B);
		s1p->s1_num_chans = 1;
		s1p->s1_adap_type = S1_H_ADAP_93B;
	}

	return 0;		/* ??? */
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


static void
s1_set_prg(s1p, ha_type)
s1chip_t	*s1p;
int		ha_type;
{
	register char	*wp, *rp;
	caddr_t		drp, dwp, sp;
	int		i, cnf;
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
		return;
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

	return;
}

void
scsi_init()
{
	int	i, j;
	char    *cp;

	/* If scsihostid is set, print a message indicating the current
	 * setting.
	 */
	if ((cp = getenv("scsihostid")) != NULL) {
	        int id = atoi(cp);
		if (id < 0 || id > 7)
		        printf("WARNING: scsihostid environment variable is"
			       " invalid.  Using host id of 0.\n");
		else
		        printf("SCSI Host ID is %d.\n", id);
	}
	for (i = 0; i < s1_num; i++)
		for (j = 0; j < s1chips[i].s1_num_chans; j++)
		    switch(s1chips[i].s1_adap_type) {
		    case S1_H_ADAP_95A:
			wd95_earlyinit(s1chips[i].s1_chan[j]);
			break;
		
		    case S1_H_ADAP_93B:
			printf("s1: bus %d ignored; 93b not supported\n",
			       s1chips[i].s1_chan[j]);
			/* scsiunit_init(s1chips[i].s1_chan[j]); */
			break;
		    }
}


/*
 * Start up the S1 DMA engine.
 */
void
scsi_go(dmamap_t *map, caddr_t addr, int readflag)
{
	register uint offset;

if (*scuzzy[map->dma_adap].statcmd & (S1_SC_DMA(scuzzy[map->dma_adap].channel)))
printf("Hey, dma is already active\n");
	offset = (__psunsigned_t) addr & (IO_NBPC -1);
	if (readflag)
		*(volatile int *) scuzzy[map->dma_adap].dma_write = offset;
	else
		*(volatile int *) scuzzy[map->dma_adap].dma_read = offset;
}

#include <sys/sysmacros.h>
#include <sys/sbd.h>

/*
 * Flush the DMA and check for an S1/IBUS error.
 * Returns 0 if the S1 detects no error on this slice.
 */
int
s1dma_flush(dmamap_t *map)
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
#if	EVEREST
	/* If data in uncached space */
	if (IS_KSEG1(map))
		flush_iocache(s1adap2slot(adap));
#endif
	while (*scuzzi->statcmd & S1_SC_DMA(chan))
		if (++stat == 5000)
		{
			retcode = 1;
			break;
		}

#define S1_HANDLE_IBUS_ERR 1
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


int
scsidma_flush(dmamap_t *map, int readflag)
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
 * s1_int_present - is a scsi interrupt present
 */
int
s1_int_present(int adap)
{
	/* reads the S1 command status */
	return (*scuzzy[adap].statcmd &
		S1_SC_INT_CNTRL(scuzzy[adap].channel));
}


/*
 * scsi_int_present - is a scsi interrupt present
 */
int
scsi_int_present(scsidev_t *dp)
{
	/* reads the S1 command status */
	return (*scuzzy[dp->d_adap].statcmd &
		S1_SC_INT_CNTRL(scuzzy[dp->d_adap].channel));
}


/*
 * Yank the reset line and reset the DMA
 */
void
s1_reset(int adap)
{
	*scuzzy[adap].statcmd = (2 << scuzzy[adap].channel) | 0;
	DELAY(200);
	*scuzzy[adap].statcmd = (2 << scuzzy[adap].channel) | 1;
	*scuzzy[adap].dma_reset = 0;
}


void *
scsi_getchanmap(void *map)
{
	return map;
}


void *
scsi_freechanmap(void *map)
{
	return 0;
}


dmamap_t *
dma_mapalloc(int type, int adap, int npages, int flag)
{
        struct map *map;
        dmamap_t   *dmamap;

#if 0
        if (type != DMA_SCSI)
                map = iamap_get(type,adap)->map;
#endif
        dmamap = (dmamap_t *) dmabuf_malloc(sizeof(dmamap_t));
        dmamap->dma_type = type;
        dmamap->dma_adap = adap;

        if (type == DMA_SCSI)
        {
                dmamap->dma_virtaddr = (caddr_t)
                        dmabuf_malloc(1026*sizeof(int));
                dmamap->dma_size = 1026;
        }
#if 0
        else
        {
                /* For DMA prefetch, must map extra page */
                npages++;
                dmamap->dma_size = npages;

                while ((dmamap->dma_virtaddr = malloc(map,npages)) == 0) {
                        psema(mapout(map), PMEM);
                }
                /* 0 means not mapped yet, dma_map maps it */
                dmamap->dma_addr = 0;
        }
#endif
        return dmamap;
}

#define ev_kvtophyspnum(_x) (KDM_TO_PHYS((__psunsigned_t) (_x)) >> IO_PNUMSHFT)

int
dma_map(dmamap_t *dmamap, caddr_t addr, int len)
{
        uint    page, startpage, endpage;
        uint    *map;
        int     ret;
        extern scuzzy_t scuzzy[];

        startpage = io_btoct(addr);
        endpage = io_btoct(addr + len);
        map = (uint *) dmamap->dma_virtaddr;

        if (2 + endpage - startpage > dmamap->dma_size)
        {
                endpage = startpage + dmamap->dma_size - 2;
                ret = (io_ctob(endpage) + IO_NBPC) - (__psunsigned_t) addr;
        }
        else
                ret = len;
        for (page = startpage; page <= endpage; page++, map++)
                *map = (uint)ev_kvtophyspnum((caddr_t) io_ctob(page)) << 4;

        /* set last page to point to first page in memory for read ahead */
        *map = 0;

        page = (uint)ev_kvtophyspnum((caddr_t) dmamap->dma_virtaddr);
        *scuzzy[dmamap->dma_adap].dma_xlatlo =
                io_ctob(page) | ((__psint_t)dmamap->dma_virtaddr & IO_NBPC-1);
        *scuzzy[dmamap->dma_adap].dma_xlathi = page >> 20;

        return ret;
}

/*
 * Conservative effort to find the IO4 slot given a channel Number.
 * Will require change if the algorithm in s1_init gets updated to 
 * support multiple IO4s. 
 * We may have to use the following code when the s1_init routine gets
 * updated to follow one in Kernel.
 * return (adap ? (adap/MAX_CHANS_IO4) : s1chips[0].s1_slot); 
 */
int
s1adap2slot(int chan)
{
	int 	slot;

	slot = (chan/MAX_CHANS_IO4);
	if (!slot)
		slot = first_io4;
	return (slot);
}

/*
 * Return non-zero if SCSI bus goes external to chassis/cabinet.
 * Generally implies that sync mode limited to 5 MT/s.
 */
int
scsi_is_external(const int adap)
{
        register scuzzy_t       *scuzzi;

        scuzzi = &scuzzy[adap];
        return *((volatile u_int *) (scuzzi->s1_base + S1_DRIVERS_CONF)) &
               (0x20 << (3 * scuzzi->channel));
}



#define SCIP_ADDR(number, base) (*((volatile uint *) (base + number)))

/*
 * Control DMA download -- does one transfer only
 */
int
scipctrldma(volatile u_char *scipbase, void *addr, int sramloc, int count,
	    int direction)
{
	uint	page;

	if (direction == 1)	/* read from SCIP SRAM */
		direction = 0xC020;
	else
		direction = 0xC018;
	SCIP_ADDR(0xC000, scipbase) = sramloc;
	page = ev_kvtophyspnum(addr);
	SCIP_ADDR(0xC008, scipbase) = page >> 20;
	SCIP_ADDR(0xC010, scipbase) = io_ctob(page) | io_poff(addr);
	SCIP_ADDR(direction, scipbase) = count;
	while (!(SCIP_ADDR(direction, scipbase) & (3 << 12)));
	if (SCIP_ADDR(direction, scipbase) & (1 << 13))
		return 1;
	return 0;
}


/****************************************************************
* $Id: s1chip.c,v 1.17 1996/07/08 21:53:57 jeffs Exp $
*
* The SCIP firmware is stored in initialized data structures. It is
* downloaded by the SCIP driver using cpu iniated Control DMAs into the 
* local memory of the SCIP which resides between 0x20000 - 0x3ffff in
* the SCIP address map.
*
* Generated by: gwh
*         Date: Thu Jul  7 12:02:47 PDT 1994
*
*
******************************************************************/
int fragment0[] = { 
	0x890c001f,0x890c001f,0x91340008,0x1140a000,0x1110a000,0x800c001f,
	0x800c001f,0x88000009,0x0820a000,0x1810a000,0x9b100006,0xb0000000,
	0x93180000,0xc012004c,0xb0000800,0x93180002,0xc012004c,0xb0001000,
	0x0000a010,0xc03a0060,0x55555555,0xaaaaaaaa,0x00000000,0xffffffff,
	0x08120058,0x1012005c,0x98021000,0x0b200000,0x13400004,0x9b000008,
	0x8307fffc,0xc00a006c,0x98021000,0x23100000,0x84240000,0xc02a0144,
	0x2b100004,0x85440000,0xc02a0144,0x9b000008,0x8307fffc,0xc00a0084,
	0x08120054,0x10120050,0x98021000,0x0b200000,0x13400004,0x9b000008,
	0x8307fffc,0xc00a00b4,0x98021000,0x23100000,0x84240000,0xc02a0144,
	0x2b100004,0x85440000,0xc02a0144,0x9b000008,0x8307fffc,0xc00a00cc,
	0x98021000,0x8b000000,0x890c0010,0x89740000,0x0b200000,0x9b000004,
	0x8307fffc,0xc00a00f4,0x98021000,0x8b000000,0x890c0010,0x89740000,
	0x23100000,0x84240000,0xc02a0144,0x9b000004,0x8307fffc,0xc00a0114,
	0x88000005,0x0e208000,0xc03a014c,0x88000009,0x0e208000,0x86040800,
	0xc02a015c,0x88000020,0xc03a016c,0xc0220168,0x88000010,0xc03a016c,
	0x88000040,0x0820a000,0x40000001,0xbad0bad0  };
struct firmware
{
	unsigned addr;
	unsigned size;
	int *fragment;
} firmware_s[] = {
	{ 0x20000, sizeof(fragment0),fragment0 } };


/*
 * Download firmware to the SCIP.
 * Returns non-zero if there is an error.
 */
int
scipfw_download(volatile u_char *scipbase)
{
	char			*fw;
	int			 i;
	int			 err = 0;

	fw = dmabuf_malloc(sizeof(fragment0));
	bcopy(fragment0, fw, sizeof(fragment0));
	for (i = 0; i < sizeof(fragment0); i += 8)
		err |= scipctrldma(scipbase, &fw[i], i, 8, 0);
	dmabuf_free(fw);
	return err;
}


static struct datapattern {
	int	hi;
	int	lo;
} scippattern[] = {
	{0, 0xFFFFFFFF},
	{0x55555555, 0xAAAAAAAA},
	{0xAAAAAAAA, 0x55555555},
	{0xFFFFFFFF, 0}
};

int
scip_diag(u_char slot, u_char padap, u_char window)
{
	volatile u_char	*scipaddr;
	uint		*buf;
	int		 i, j;
	int		 count;

	scipaddr = (caddr_t) SWIN_BASE(window, padap) + 4;
	buf = dmabuf_malloc(128);

	if (SCIP_ADDR(0x6000, scipaddr) & 0x4000) /* clear illegal PIO error */
		SCIP_ADDR(0x6000, scipaddr) = 0x4000;
	SCIP_ADDR(0xC028, scipaddr) = 0;	/* clear control DMA error */
	SCIP_ADDR(0xA000, scipaddr) = 0x701;	/* reset PCs for all tasks */
	SCIP_ADDR(0xA000, scipaddr) = 0x1700;	/* clear SRAM parity error */
	SCIP_ADDR(0xA000, scipaddr) = 0x10000;	/* clear PIO timeout error */
	SCIP_ADDR(0xA008, scipaddr) = 0xFE0000; /* clear parity error bits */
	SCIP_ADDR(0x6000, scipaddr) = 0x1FFD0;  /* mux sel for WE clock edge */
	SCIP_ADDR(0x7028, scipaddr) = SCIP_ADDR(0x7028, scipaddr);
	SCIP_ADDR(0x7020, scipaddr) = 0;

	/*
	 * Write data patterns to all of memory
	 */
	for (count = 0;
	     count < sizeof(scippattern) / sizeof(struct datapattern);
	     count++)
	{
		for (j = 0; j < 32; j += 2) {
			buf[j] = scippattern[count].hi;
			buf[j+1] = scippattern[count].lo;
		}
		for (i = 0; i < 128 * 1024; i += 128) {
			if (scipctrldma(scipaddr, buf, i, 128, 0))
				return 1;
		}
		for (i = 0; i < 128 * 1024; i += 128) {
			if (scipctrldma(scipaddr, buf, i, 128, 1))
				return 1;
			for (j = 0; j < 32; j += 2)
				if (buf[j] != scippattern[count].hi ||
				    buf[j+1] != scippattern[count].lo)
					return 1;
		}
	}

	/*
	 * Write incrementing address pattern.
	 */
	for (i = 0; i < 128 * 1024; i += 128) {
		for (j = 0; j < 32; j++)
			buf[j] = i + j * 4;
		if (scipctrldma(scipaddr, buf, i, 128, 0))
			return 1;
	}
	for (i = 0; i < 128 * 1024; i += 128) {
		if (scipctrldma(scipaddr, buf, i, 128, 1))
			return 1;
		for (j = 0; j < 32; j++)
			if (buf[j] != i + j * 4)
				return 1;
	}

	/*
	 * Download diagnostic firmware, start tasks, and wait for status.
	 */
	if (scipfw_download(scipaddr))
		return 1;
	SCIP_ADDR(0xA000, scipaddr) = 0x11;
	SCIP_ADDR(0xA000, scipaddr) = 0x21;
	SCIP_ADDR(0xA000, scipaddr) = 0x41;
	for (i = 0; i <= 0x1000; i += 0x800) {
		for (j = 0; j < 120; j++) {
			if (SCIP_ADDR(0x8000+i, scipaddr) & 8)
				return 1;
			if (SCIP_ADDR(0x8000+i, scipaddr) & 4)
				break;
			DELAY(1000);
		}
		if (j == 100)
			return 1;
	}

	return 0;
}
