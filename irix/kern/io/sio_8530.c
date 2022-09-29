#ident	"$Revision: 1.12 $"
/*
 * Copyright (C) 1986, 1992, 1993, 1994, Silicon Graphics, Inc.
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

/*
 * This is a lower level module for the modular serial i/o driver. This
 * module implements all hardware dependent functions for doing serial
 * i/o on the Everest 8530 uart.
 */

#include "sys/cmn_err.h"
#include "ksys/vfile.h"
#include "sys/sysinfo.h"
#include "sys/z8530.h"
#include "sys/invent.h"
#include "sys/EVEREST/evintr.h"
#include "ksys/serialio.h"
#include "sys/systm.h"
#include "sys/kmem.h"

int console_device_attached;

#if DEBUG_STATS
#define DEBUGINC(x) x++
#define DEBUGADD(x,i) x += (i)
int dp_overrun, overrun;
int dp_pull;
int khalf, tohalf, kone, toone;
int tout_intr, rxintr, txintr;
int rxbytes, txbytes;
#else
#define DEBUGINC(x)
#define DEBUGADD(x,i)
#endif

#define RX_BUF_SZ 128
#define RX_BUF_MASK (RX_BUF_SZ-1)
/*
 * each port has a data structure that looks like this
 */
typedef struct dport {
	sioport_t sioport;		/* MUST BE FIRST MEMBER */

	/* hardware address */
	volatile u_char	*dp_cntrl;	/* port control	reg */
	volatile u_char	*dp_data;	/* port data reg */
	
	__psunsigned_t	dp_swin;	/* Small window pointer */

	int	dp_notify;		/* notifications expected */

	toid_t	dp_toid;		/* toid and delay for switching */
	int	dp_timeout_delay;	/* from 4 chars/intr back to 1  */

	struct rx_buf {
	    u_char c;
	    u_char status;
	}	dp_rx_buf[RX_BUF_SZ];
	short	dp_rx_prod;
	short	dp_rx_cons;

	u_char	dp_index;		/* port number */

	/* software copy of hardware registers */
	u_char	dp_wr1;			/* !P! individual interrupt mask */
	u_char	dp_wr3;			/* !P! receiver parameters */
	u_char	dp_wr4;			/* !P! transmitter/receiver params */
	u_char	dp_wr5;			/* !P! transmitter parameters */
	u_char	dp_wr7;
	u_char	dp_wr15;		/* !P! external events interrupt mask */

	u_char	dp_bitmask;		/* incoming char bitmask */

	u_char	dp_flags;
	u_char	last_char;
	u_char	last_status;
} dport_t;

#define DP_SAW_RX	0x1
#define DP_LAST_VALID	0x2
#define DP_BREAK	0x4
#define DP_TX_DISABLED	0x8

/* get local port type from global sio port type */
#define LPORT(port) ((dport_t*)(port))
#define CONSOLE_DP LPORT(the_console_port)

/* get global port from local port type */
#define GPORT(port) ((sioport_t*)(port))

int	ev_num_serialports;

#define	CNTRL_A		'\001'
#ifdef DEBUG
  #ifndef DEBUG_CHAR
  #define DEBUG_CHAR	CNTRL_A
  #endif
#else /* !DEBUG */
  #define DEBUG_CHAR	CNTRL_A
#endif /* DEBUG */

/* local functions: */
static int z8530_open		(sioport_t *port);
static int z8530_config		(sioport_t *port, int baud, int byte_size,
				 int stop_bits, int parenb, int parodd);
static int z8530_enable_hfc	(sioport_t *port, int enable);
static int z8530_set_extclk	(sioport_t *port, int clock_factor);

    /* data transmission */
static int z8530_write		(sioport_t *port, char *buf, int len);
static int z8530_duwrite	(sioport_t *port, char *buf, int len);
static void z8530_wrflush	(sioport_t *port);
static int z8530_break		(sioport_t *port, int brk);
static int z8530_enable_tx	(sioport_t *port, int enb);

    /* data reception */
static int z8530_read		(sioport_t *port, char *buf, int len);

    /* event notification */
static int z8530_notification	(sioport_t *port, int mask, int on);
static int z8530_rx_timeout	(sioport_t *port, int timeout);

    /* modem control */
static int z8530_set_DTR	(sioport_t *port, int dtr);
static int z8530_set_RTS	(sioport_t *port, int rts);
static int z8530_query_DCD	(sioport_t *port);
static int z8530_query_CTS	(sioport_t *port);

    /* output mode */
static int z8530_set_proto	(sioport_t *port, enum sio_proto proto);

static int z8530_handle_intr	(int, int);
static void z8530_handle_intrs	(void);

static struct serial_calldown z8530_calldown = {
    z8530_open,
    z8530_config,
    z8530_enable_hfc,
    z8530_set_extclk,
    z8530_write,
    z8530_duwrite,
    z8530_wrflush,	/* du flush */
    z8530_break,
    z8530_enable_tx,
    z8530_read,
    z8530_notification,
    z8530_rx_timeout,
    z8530_set_DTR,
    z8530_set_RTS,
    z8530_query_DCD,
    z8530_query_CTS,
    z8530_set_proto,
};
    

#define	BAUD_TO_TIME_CONSTANT(x)	((CLK_SPEED + (x) * CLK_FACTOR) / \
					 ((x) * CLK_FACTOR * 2) - 2)
#define TIME_CONSTANT_TO_BAUD(t)	((CLK_SPEED) / \
					 (((t)+2) * CLK_FACTOR * 2))
#define MAX_BAUD	TIME_CONSTANT_TO_BAUD(0)
#define MIN_BAUD	TIME_CONSTANT_TO_BAUD(0xffff)

#define NUMPORTS 16
static dport_t dports[NUMPORTS];

#define IS_RS422(dp) (((dp)->dp_index & 3) == 2)

/* convert real port number to virtual (soft) port number. The ports
 * are mapped as follows:
 *
 * dports[0] = ttyd2
 * dports[1] = ttyd3
 * dports[2] = ttyd4
 * dports[3] = ttyd1
 *
 * dports[4] = ttyd6
 * dports[5] = ttyd7
 * dports[6] = ttyd8
 * dports[7] = ttyd5
 *
 * etc.
 */
#define R2V(r) (((r) & ~3) + (((r) + 1) & 3) + 1)

/* circular buffer to take ducons_write() output. Instead of spinning
 * while the uart drains, we buffer some large amount of data and
 * drain it out efficiently under interrupt control. The text in this
 * buffer takes precedence over normal output. The port need not be
 * open for this to work. Chose 2048 bytes since it matches the IOC3
 * ring buffer size.
 */
#define DUCW_SIZE 2048
#if DUCW_SIZE & (DUCW_SIZE-1)
XXX /* must be a power of 2 */
#endif
#define DUCW_MASK (DUCW_SIZE-1)
static char ducw_buf[DUCW_SIZE];
static int ducw_prod, ducw_cons;
#define PRINTF_PENDING(dp) ((dp) == CONSOLE_DP && \
			    ducw_prod != ducw_cons)
int ducw_intrs_enabled;

extern void evkb_init(void);
extern void evkb_lateinit(void);

extern void *the_kdbx_port;	/* pointer to remote debug port */

void
enable_intrs(void)
{
    int duartnum;

    for(duartnum = 0; duartnum < ev_num_serialports; duartnum += 4) {
	EV_SET_REG(dports[duartnum].dp_swin + EPC_IMSET,
		   EPC_INTR_DUART12);
    }
}

#define UNUSED 255

static void
everest_du_portinit(void)
{
    int i;			/* Index registers */
    int slot;			/* Slot of IO4 for given unit # */
    int adap;			/* IO adapter of Io4 for given unit # */
    int unit;			/* Unit currently being configured */
    evbrdinfo_t *brd;           /* Dereferenced board information struct */
    __psunsigned_t d_base;       /* Base address for EPC DUART values */
    dport_t *dp;		/* Pointer to current dport */
    int base;			/* Current dports index */
    __psunsigned_t swin;	/* Swin value */
    unsigned char map_usage[EV_MAX_SLOTS];
    
    extern int epc_xadap2slot(int, int*, int*);
    
#if defined(EVEREST) && defined(MULTIKERNEL)
    /* Only the golden cell has access to real I/O devices */
    
    if (evmk_cellid != evmk_golden_cellid)
    	return;
#endif /* EVEREST && MULTIKERNEL */
    
    for (i = 0; i < EV_MAX_SLOTS; i++)
	map_usage[i] = UNUSED;
    
    base = 0;
    
    for (i = 0; i < ibus_numadaps; i++) {

	/* Skip over adapter values which aren't relevant */
	if (ibus_adapters[i].ibus_module != EPC_SERIAL)
	    continue;

	/* Map from the adapter value into the actual slot and ioa */
	if (epc_xadap2slot(ibus_adapters[i].ibus_adap, &slot, &adap) != 0) {
	    cmn_err(CE_WARN, 
		    "!epcserial: couldn't map unit %d to slot %d ioa %d\n", 
		    ibus_adapters[i].ibus_unit, slot, adap); 
	    continue;
	}

	unit = ibus_adapters[i].ibus_unit;

	/* Make sure we haven't already configured this adapter */
	if (map_usage[slot] != UNUSED) {
	    cmn_err(CE_WARN, "!epcserial: slot %d, ioa %d already configured "
		    "as unit %d", slot, adap, map_usage[slot]);
	    continue;
	} else {
	    map_usage[slot] = unit;
        } 

	if (unit < 0 || unit > 3) {
	    cmn_err(CE_WARN, "!epcserial: invalid unit number: %d\n", unit);
	    continue;
	}

	brd = &(EVCFGINFO->ecfg_board[slot]);
	swin = SWIN_BASE(brd->eb_io.eb_winnum, adap);
	d_base = swin + BYTE_SELECT;

	/* DUART 1B.  Maps to ttyd2 */
	dp = &dports[base];
	dp->dp_cntrl = (u_char*) (d_base + EPC_DUART1 + CHNB_CNTRL_OFFSET);
	dp->dp_data  = (u_char*) (d_base + EPC_DUART1 + CHNB_DATA_OFFSET);
	dp->dp_index = base;
	dp->dp_bitmask = 0xff;
	dp->dp_swin    = swin;

	/* DUART 1A.  Maps to ttyd3 */
	dp = &dports[base+1];
	dp->dp_cntrl = (u_char*) (d_base + EPC_DUART1 + CHNA_CNTRL_OFFSET);
	dp->dp_data  = (u_char*) (d_base + EPC_DUART1 + CHNA_DATA_OFFSET);
	dp->dp_index = base+1;
	dp->dp_bitmask = 0xff;
	dp->dp_swin    = swin;

	/* DUART 2B. It maps to ttyd4 */
	dp = &dports[base+2];
	dp->dp_cntrl = (u_char*) (d_base + EPC_DUART2 + CHNB_CNTRL_OFFSET);
	dp->dp_data  = (u_char*) (d_base + EPC_DUART2 + CHNB_DATA_OFFSET);
	dp->dp_index = base+2;
	dp->dp_bitmask = 0xff;
	dp->dp_swin    = swin;

	/* DUART 2A. It maps to ttyd1 */
	dp = &dports[base+3];
	dp->dp_cntrl = (u_char*) (d_base + EPC_DUART2 + CHNA_CNTRL_OFFSET);
	dp->dp_data  = (u_char*) (d_base + EPC_DUART2 + CHNA_DATA_OFFSET);
	dp->dp_index = base+3; 
	dp->dp_bitmask = 0xff;
	dp->dp_swin    = swin;

	/* Connect the interrupt and enable it on the EPC */
	evintr_connect((evreg_t *)(swin + EPC_IIDDUART1),
		       EVINTR_LEVEL_EPC_DUART1,
		       SPLHIDEV,
		       EVINTR_DEST_EPC_DUART1,
		       (EvIntrFunc)z8530_handle_intrs,
		       (void *)0);

	EV_SET_REG(swin + EPC_IMSET, EPC_INTR_DUART12);

	if (unit == 0) {
	    the_console_port = &dports[base+3];
	    the_kdbx_port = &dports[base];
	}

	/* Reset the duarts for additional devices */
	if (unit) {
	    EV_SET_REG(swin + EPC_PRSTSET, 0x70);
	    us_delay(10);
	    EV_SET_REG(swin + EPC_PRSTCLR, 0x70);
	    us_delay(10);
 	}

	base += 4;
    }
    ev_num_serialports = base;
    
    ASSERT(the_console_port);
}

extern char *kopt_find(char *);

/*
 * initialize the DUART ports. this is done to allow debugging soon after
 * booting
 */

void
everest_du_init(void)
{
    dport_t *dp;
    static int duinit_flag = 0;
    char *env;
    int d_baud;
    u_char du_cntrl;	/* XXXrs - for RD_CNTRL */
    int def_console_baud;
    
    if (duinit_flag++)		/* do it at most once */
	return;
    
    everest_du_portinit();
    
    /*
     * this should be the only place we reset the chips. enable global
     * interrupt too
     */
    for (dp = dports; dp < &dports[ev_num_serialports]; dp += 2) {
	ASSERT(dp->dp_cntrl);

	/* Before the reset, flush any pending output. This
	 * is to prevent early printf diagnostic output
	 * still pending in the uart from being clobbered
	 */
	if (dp == &dports[CONSOLE_DP->dp_index / 2 * 2])
	    while((RD_CNTRL(CONSOLE_DP->dp_cntrl, RR1) &
		   RR1_ALL_SENT) == 0);

	WR_CNTRL(dp->dp_cntrl, WR9, WR9_HW_RST | WR9_MIE)
    }
    
    /*
     * deassert all output signals, enable Z85130 functions, transmitter
     * interrupt when FIFO is completely empty
     */
    for (dp = dports; dp < &dports[ev_num_serialports]; dp++) {
	ASSERT(dp->dp_cntrl);

	dp->dp_wr15 = WR15_Z85130_EXTENDED;
	WR_CNTRL(dp->dp_cntrl, WR15, dp->dp_wr15);

	WR_CNTRL(dp->dp_cntrl, WR7, WR7_TX_FIFO_INT|WR7_EXTENDED_READ);

	DU_DTR_DEASSERT(dp->dp_wr5);
	DU_RTS_DEASSERT(dp->dp_wr5);
	WR_CNTRL(dp->dp_cntrl, WR5, dp->dp_wr5)
    }
    
#if defined(EVEREST) && defined(MULTIKERNEL)
    if (evmk_cellid != evmk_golden_cellid)
	return;
#endif /* EVEREST && MULTIKERNEL */
    
    env = kopt_find("dbaud");
    if (env && (d_baud = atoi(env)) > 0)
	def_console_baud = d_baud;
    else
	def_console_baud = 9600;
    
    /* initialize the secondary console */
    z8530_config(the_console_port, def_console_baud,
		 8, 1, 0, 0);
    
#ifdef DEBUG
    if(env && d_baud <= 0)
	cmn_err(CE_WARN, "non-numeric or 0 value for dbaud ignored (%s)", env);
#endif

    if (env = kopt_find("rbaud"))
        z8530_config(the_kdbx_port, atoi(env), 8, 1, 0, 0);

    /* finish up with the keyboard/mouse ports */
    evkb_init();
}

/* export console device numbers */
static dev_t cons_devs[2];
dev_t
get_cons_dev(int which)
{
    ASSERT(which == 1 || which == 2);

    return(cons_devs[which - 1]);
}

/* late init stuff that must be done after the hwgraph is inited 
 * (du_init is called before hwgraph init time)
 */
void
du_lateinit()
{
    vertex_hdl_t mttydir_vhdl, cons_vhdl, ttynumdir_vhdl;
    /*REFERENCED*/
    graph_error_t rc;
    int x;

#if defined(EVEREST) && defined(MULTIKERNEL)
    /* no devices to install on non-golden cells since all I/O is
     * golden cell only.
     */
    if (evmk_cellid != evmk_golden_cellid)
    	return;
#endif /* EVEREST && MULTIKERNEL */
    rc = hwgraph_path_add(hwgraph_root, "machdep/ttys", &mttydir_vhdl);
    ASSERT(rc == GRAPH_SUCCESS);

    for(x = 0; x < NUMPORTS; x++) {
	char name[3];
	dport_t *dp = &dports[x];

	if (dp->dp_cntrl == 0)
	    continue;

	/* create a subdir for this port num */
	sprintf(name, "%d", R2V(x));
	rc = hwgraph_path_add(mttydir_vhdl, name, &ttynumdir_vhdl);
	ASSERT(rc == GRAPH_SUCCESS);
	
	/* add device nodes for this device in this subdir */
	sio_initport(GPORT(dp),
		     ttynumdir_vhdl,
		     IS_RS422(dp) ? NODE_TYPE_ALL_RS422 : NODE_TYPE_ALL_RS232,
		     dp == CONSOLE_DP);

	/* If this is an even numbered port, init the locks. 
	 */
	if (!(dp->dp_index & 1)) {
	    struct sio_lock *lockp;
	    lockp = (struct sio_lock*)kmem_zalloc(sizeof(struct sio_lock), 0);
	    ASSERT(lockp);

	    /* XXX streams problem prevents using mutexes for the port
	     * lock. If this is fixed in the future SIO_LOCK_MUTEX
	     * should be used for the non-console case
	     */
	    lockp->lock_levels[0] = 
		(dp == &dports[CONSOLE_DP->dp_index / 2 * 2]) ?
		    SIO_LOCK_SPL7 : SIO_LOCK_SPLHI;
	    sio_init_locks(lockp);
	    GPORT(dp)->sio_lock = lockp;
	}
	else
	    GPORT(dp)->sio_lock = GPORT(dp-1)->sio_lock;

	GPORT(dp)->sio_vhdl = ttynumdir_vhdl;
	GPORT(dp)->sio_calldown = &z8530_calldown;

	/* create inventory entries */
	device_inventory_add(ttynumdir_vhdl,
			     INV_SERIAL,
			     INV_INVISIBLE,
			     -1,0,0);
	
	/* make real hwgraph nodes */
	device_controller_num_set(ttynumdir_vhdl, R2V(x));
	sio_make_hwgraph_nodes(GPORT(dp));
    }

    rc = hwgraph_traverse(hwgraph_root, "ttys/ttyd1", &cons_vhdl);
    ASSERT(rc == GRAPH_SUCCESS);
    cons_devs[0] = vhdl_to_dev(cons_vhdl);
    rc = hwgraph_traverse(hwgraph_root, "ttys/ttyd2", &cons_vhdl);
    ASSERT(rc == GRAPH_SUCCESS);
    cons_devs[1] = vhdl_to_dev(cons_vhdl);
    
    /* do late init for keyboard/mouse */
    evkb_lateinit();

    console_device_attached = 1;
}

static int
z8530_open(sioport_t *port)
{
    dport_t *dp = LPORT(port);

    ASSERT(L_LOCKED(port, L_OPEN));

    if (!dp->dp_cntrl)
	return(1);

    /* reset to interrupt every char */
    if (dp->dp_wr7 & WR7_RX_HALF_FIFO) {
	dp->dp_wr7 &= (~WR7_RX_HALF_FIFO);
	WR_CNTRL(dp->dp_cntrl, WR7, dp->dp_wr7);
    }

    return(0);
}

static int
z8530_config(sioport_t *port,
	    int baud,
	    int byte_size,
	    int stop_bits,
	    int parenb,
	    int parodd)
{
    dport_t *dp = LPORT(port);
    int diff, actual_baud;

    volatile unsigned char *cntrl = dp->dp_cntrl;
    u_short time_constant;
    extern int baud_to_cbaud(speed_t);	       /* in streamio.c */

    ASSERT(L_LOCKED(port, L_CONFIG));

    if (baud > MAX_BAUD || baud < MIN_BAUD)
	    return(1);

    time_constant = BAUD_TO_TIME_CONSTANT(baud);
    actual_baud = TIME_CONSTANT_TO_BAUD(time_constant);

    diff = actual_baud - baud;
    if (diff < 0)
	    diff = -diff;
    
    /* fail if we're not within 1% of requested baud rate or time
     * constant is out of range
     */
    if ((diff * 100 > actual_baud)
	|| ((unsigned)time_constant > 0xffff))
	    return(1);

    dp->dp_wr3 = WR3_RX_ENBL;
    dp->dp_wr5 = dp->dp_wr5 & (WR5_RTS | WR5_DTR) | WR5_TX_ENBL;

    dp->dp_wr4 = WR4_X16_CLK;
    dp->dp_wr4 |= stop_bits ? WR4_2_STOP : WR4_1_STOP;

    switch (byte_size) {
      case 5:
	dp->dp_bitmask = 0x1f;
	break;
      case 6:
	dp->dp_bitmask = 0x3f;
	dp->dp_wr3 |= WR3_RX_6BIT;
	dp->dp_wr5 |= WR5_TX_6BIT;
	break;
      case 7:
	dp->dp_bitmask = 0x7f;
	dp->dp_wr3 |= WR3_RX_7BIT;
	dp->dp_wr5 |= WR5_TX_7BIT;
	break;
      case 8:
	dp->dp_bitmask = 0xff;
	dp->dp_wr3 |= WR3_RX_8BIT;
	dp->dp_wr5 |= WR5_TX_8BIT;
	break;
    }
    
    /* set parity */
    if (parenb) {
	dp->dp_wr4 |= WR4_PARITY_ENBL;
	if (!parodd)
	    dp->dp_wr4 |= WR4_EVEN_PARITY;
    }

    /* set transmitter/receiver control */
    WR_CNTRL(cntrl, WR4, dp->dp_wr4);
    WR_CNTRL(cntrl, WR3, dp->dp_wr3 & ~WR3_RX_ENBL);
    WR_CNTRL(cntrl, WR5, dp->dp_wr5 & ~WR5_TX_ENBL);
    
    /* use NRZ encoding format*/
    WR_CNTRL(cntrl, WR10, 0x0);
    
    /* use internal BRG if is not using external clock */
    WR_CNTRL(cntrl, WR11, WR11_RCLK_BRG | WR11_TCLK_BRG);

    /* set time constant according to baud rate */
    WR_CNTRL(cntrl, WR14, 0x0);
    WR_CNTRL(cntrl, WR12, time_constant & 0xff);
    WR_CNTRL(cntrl, WR13, time_constant >> 8);
    
    /* enable internal BRG if is not using external clock */
    WR_CNTRL(cntrl, WR14, WR14_BRG_ENBL);
    
    /* enable transmitter/receiver */
    WR_CNTRL(cntrl, WR3, dp->dp_wr3);
    WR_CNTRL(cntrl, WR5, dp->dp_wr5);

    return(0);
}

/*ARGSUSED*/
static int
z8530_enable_hfc(sioport_t *port, int enable)
{
    return(1);
}

/*ARGSUSED*/
static int
z8530_set_extclk(sioport_t *port, int clock_factor)
{
    return(1);
}

static int
z8530_write(sioport_t *port, char *buf, int len)
{
    int olen = len;
    dport_t *dp = LPORT(port);

    ASSERT(L_LOCKED(port, L_WRITE));

    if (dp->dp_flags & DP_TX_DISABLED)
	return(0);

    while(len > 0 && (RD_RR0(dp->dp_cntrl) & RR0_TX_EMPTY)) {
	
	WR_DATA(dp->dp_data, *buf & dp->dp_bitmask);
	buf++;
	len--;
    }
    DEBUGADD(txbytes, olen - len);
    return(olen - len);
}

static void
ducw_send(void)
{
    dport_t *dp = CONSOLE_DP;

    if (dp->dp_flags & DP_TX_DISABLED)
	return;

    /* send as many bytes to the uart from the ducw buffer as we can */
    while(ducw_prod != ducw_cons && (RD_RR0(dp->dp_cntrl) & RR0_TX_EMPTY)) {
	WR_DATA(dp->dp_data, ducw_buf[ducw_cons & DUCW_MASK] & dp->dp_bitmask);
	ducw_cons++;
    }

    /* If ducw buffer is now empty, and port doesn't want lowat
     * notification, disable TX interrupt
     */
    if (ducw_prod == ducw_cons && !(dp->dp_notify & N_OUTPUT_LOWAT)) {
	dp->dp_wr1 &= ~WR1_TX_INT_ENBL;
	WR_CNTRL(dp->dp_cntrl, WR1, dp->dp_wr1);
    }
}

static int
z8530_duwrite(sioport_t *port, char *buf, int len)
{
    int bytes, i, olen = len;
    dport_t *dp = LPORT(port);
    ASSERT(dp == CONSOLE_DP);

    while(len > 0) {
	/* If the buffer is full, spin trying to drain it */
	if (ducw_prod - ducw_cons == DUCW_SIZE) {
	    while((RD_RR0(dp->dp_cntrl) & RR0_TX_EMPTY) == 0);
	    WR_DATA(dp->dp_data, ducw_buf[ducw_cons & DUCW_MASK] & dp->dp_bitmask);
	    ducw_cons++;
	}
	bytes = len;
	i = DUCW_SIZE - (ducw_prod & DUCW_MASK);
	if (i < bytes)
	    bytes = i;
	i = DUCW_SIZE - (ducw_prod - ducw_cons);
	if (i < bytes)
	    bytes = i;
	bcopy(buf, ducw_buf + (ducw_prod & DUCW_MASK), bytes);
	buf += bytes;
	len -= bytes;
	ducw_prod += bytes;
    }

    ducw_send();

    /* make sure the TX interrupt is enabled */
    if (!(dp->dp_wr1 & WR1_TX_INT_ENBL) &&
	!(dp->dp_flags & DP_TX_DISABLED)) {
	dp->dp_wr1 |= WR1_TX_INT_ENBL;
	WR_CNTRL(dp->dp_cntrl, WR1, dp->dp_wr1);
    }

    return(olen);
}

static void
z8530_wrflush(sioport_t *port)
{
    dport_t *dp = LPORT(port);
    ASSERT(sio_port_islocked(port));
    ASSERT(dp == CONSOLE_DP);
    
    while(ducw_prod != ducw_cons) {
	while((RD_RR0(dp->dp_cntrl) & RR0_TX_EMPTY) == 0);
	WR_DATA(dp->dp_data, ducw_buf[ducw_cons & DUCW_MASK] & dp->dp_bitmask);
	ducw_cons++;
    }
}

static int
z8530_break(sioport_t *port, int brk)
{
    dport_t *dp = LPORT(port);

    ASSERT(L_LOCKED(port, L_BREAK));
    
    if (brk)
	dp->dp_wr5 |= WR5_SEND_BRK;
    else
	dp->dp_wr5 &= ~WR5_SEND_BRK;
    WR_CNTRL(dp->dp_cntrl, WR5, dp->dp_wr5);
    return(0);
}

static int
z8530_enable_tx(sioport_t *port, int enb)
{
    dport_t *dp = LPORT(port);

    ASSERT(L_LOCKED(port, L_ENABLE_TX));

    /* if we're already in the desired state, we're done */
    if ((enb && !(dp->dp_flags & DP_TX_DISABLED)) ||
	(!enb && (dp->dp_flags & DP_TX_DISABLED)))
	return(0);

    if (enb) {
	dp->dp_flags &= ~DP_TX_DISABLED;

	/* if there is pending printf data, we must push it along
	 * since the upper layer will never give it another nudge
	 */
	if (PRINTF_PENDING(dp))
	    ducw_send();

	/* enable the TX intr */
	dp->dp_wr1 |= WR1_TX_INT_ENBL;
	WR_CNTRL(dp->dp_cntrl, WR1, dp->dp_wr1);
    }
    else {
	dp->dp_flags |= DP_TX_DISABLED;

	/* disable the TX intr */
	dp->dp_wr1 &= ~WR1_TX_INT_ENBL;
	WR_CNTRL(dp->dp_cntrl, WR1, dp->dp_wr1);
    }
    return(0);
}

    /* data reception */
static int
z8530_read(sioport_t *port, char *buf, int len)
{
    int total = 0, hw_not_touched = 1;
    char status, c;
    dport_t *dp = LPORT(port);
    u_char du_cntrl;

    ASSERT(L_LOCKED(port, L_READ));

    while(len > 0) {
	if (dp->dp_flags & DP_LAST_VALID) {
	    dp->dp_flags &= ~DP_LAST_VALID;
	    c = dp->last_char;
	    status = dp->last_status;
	}
	else if (dp->dp_rx_prod != dp->dp_rx_cons) {
	    struct rx_buf *buf = dp->dp_rx_buf + (dp->dp_rx_cons++ & RX_BUF_MASK);
	    status = buf->status;
	    c = buf->c;
	}
	else if (RD_RR0(dp->dp_cntrl) & RR0_RX_CHR) {
	    status = RD_CNTRL(dp->dp_cntrl, RR1);
	    if (status & (RR1_FRAMING_ERR | RR1_RX_ORUN_ERR | RR1_PARITY_ERR))
		WR_WR0(dp->dp_cntrl, WR0_RST_ERR);
	    c = RD_DATA(dp->dp_data);
	    hw_not_touched = 0;
	}
	else
	    break;

	if ((status & (RR1_FRAMING_ERR | RR1_RX_ORUN_ERR | RR1_PARITY_ERR)) ||
	    ((dp->dp_flags & DP_BREAK) && c == 0)) {

#if DEBUG_STATS
	    if (status & RR1_RX_ORUN_ERR)
		overrun++;
#endif

	    if (total == 0) {
		if ((status & RR1_FRAMING_ERR) && (dp->dp_notify & N_FRAMING_ERROR))
		    UP_NCS(port, NCS_FRAMING);
		if ((status & RR1_RX_ORUN_ERR) && (dp->dp_notify & N_OVERRUN_ERROR))
		    UP_NCS(port, NCS_OVERRUN);
		if ((status & RR1_PARITY_ERR) && (dp->dp_notify & N_PARITY_ERROR))
		    UP_NCS(port, NCS_PARITY);
		if (c == 0 && (dp->dp_flags & DP_BREAK)) {
		    dp->dp_flags &= ~DP_BREAK;
		    if (dp->dp_notify & N_BREAK)
			UP_NCS(port, NCS_BREAK);
		}
		len = 1;
	    }
	    else {
		dp->dp_flags |= DP_LAST_VALID;
		dp->last_char = c;
		dp->last_status = status;
		break;
	    }
	}
	*buf++ = c & dp->dp_bitmask;
	len--;
	total++;
    }

    if (!hw_not_touched)
	dp->dp_flags |= DP_SAW_RX;

    DEBUGADD(rxbytes, total);

    return(total);
}

    /* event notification */
static int
z8530_notification(sioport_t *port, int mask, int on)
{
    dport_t *dp = LPORT(port);
    char wr15, wr1;

    ASSERT(L_LOCKED(port, L_NOTIFICATION));
    ASSERT(mask);

    wr1 = wr15 = 0;

    if (mask & N_DATA_READY)
	wr1 |= WR1_RX_INT_ERR_ALL_CHR;
    if (mask & N_OUTPUT_LOWAT)
	wr1 |= WR1_TX_INT_ENBL;
    if (mask & N_DDCD)
	wr15 |= WR15_DCD_IE;
    if (mask & N_DCTS)
	wr15 |= WR15_CTS_IE;
    if (mask & N_BREAK)
	wr15 |= WR15_BRK_IE;

    if (on) {
	dp->dp_notify |= mask;

	dp->dp_wr1 |= wr1;
	dp->dp_wr15 |= wr15;
	if (dp->dp_wr15 & (WR15_DCD_IE | WR15_CTS_IE | WR15_BRK_IE))
	    dp->dp_wr1 |= WR1_EXT_INT_ENBL;
    }
    else {
	/* don't disable the TX interrupt if there is pending printf
	 * data
	 */
	if (PRINTF_PENDING(dp))
	    wr1 &= ~WR1_TX_INT_ENBL;

	dp->dp_notify &= ~mask;

	dp->dp_wr1 &= ~wr1;
	dp->dp_wr15 &= ~wr15;
	if (!(dp->dp_wr15 & (WR15_DCD_IE | WR15_CTS_IE | WR15_BRK_IE)))
	    dp->dp_wr1 &= ~WR1_EXT_INT_ENBL;
    }

    WR_CNTRL(dp->dp_cntrl, WR15, dp->dp_wr15);
    WR_CNTRL(dp->dp_cntrl, WR1, dp->dp_wr1);

    /* make sure the external status bits are current */
    WR_WR0(dp->dp_cntrl, WR0_RST_EXT_INT);

    return(0);
}

/*ARGSUSED*/
static int
z8530_rx_timeout(sioport_t *port, int timeout)
{
    ASSERT(L_LOCKED(port, L_RX_TIMEOUT));

    LPORT(port)->dp_timeout_delay = timeout;

    /* we don't support hardware rx timeout, even though we care about
     * the value passed in.
     */
    return(1);
}

static int
z8530_set_DTR(sioport_t *port, int dtr)
{
    dport_t *dp = LPORT(port);

    ASSERT(L_LOCKED(port, L_SET_DTR));

    if (dtr)
	DU_DTR_ASSERT(dp->dp_wr5);
    else
	DU_DTR_DEASSERT(dp->dp_wr5);
    WR_CNTRL(dp->dp_cntrl, WR5, dp->dp_wr5);
    return(0);
}

static int
z8530_set_RTS(sioport_t *port, int rts)
{
    dport_t *dp = LPORT(port);

    ASSERT(L_LOCKED(port, L_SET_RTS));

    if (rts)
	DU_RTS_ASSERT(dp->dp_wr5);
    else
	DU_RTS_DEASSERT(dp->dp_wr5);
    WR_CNTRL(dp->dp_cntrl, WR5, dp->dp_wr5);
    return(0);

}

static int
z8530_query_DCD(sioport_t *port)
{
    dport_t *dp = LPORT(port);
    ASSERT(L_LOCKED(port, L_QUERY_DCD));

    return(DU_DCD_ASSERTED(RD_RR0(dp->dp_cntrl)));
}

static int
z8530_query_CTS(sioport_t *port)
{
    dport_t *dp = LPORT(port);
    ASSERT(L_LOCKED(port, L_QUERY_CTS));

    return(DU_CTS_ASSERTED(RD_RR0(dp->dp_cntrl)));
}

static int
z8530_set_proto(sioport_t *port, enum sio_proto proto)
{
    if ((IS_RS422(LPORT(port)) && proto == PROTO_RS422) ||
	(!IS_RS422(LPORT(port)) && proto == PROTO_RS232))
	return(0);
    return(1);
}

static void
z8530_intr_timeout(void *arg)
{
    int duartnum = (int)(long)arg;

    DEBUGINC(tout_intr);

    z8530_handle_intr(duartnum, 1);
}

static void
pull_rx_data(dport_t *dp)
{
    struct rx_buf *buf;
    u_char du_cntrl;

    DEBUGINC(dp_pull);

    while(RD_RR0(dp->dp_cntrl) & RR0_RX_CHR) {
	buf = dp->dp_rx_buf + (dp->dp_rx_prod & RX_BUF_MASK);
	buf->status = RD_CNTRL(dp->dp_cntrl, RR1);
	if (buf->status & (RR1_FRAMING_ERR | RR1_RX_ORUN_ERR | RR1_PARITY_ERR))
	    WR_WR0(dp->dp_cntrl, WR0_RST_ERR);
	buf->c = RD_DATA(dp->dp_data);
	dp->dp_rx_prod++;
#if DEBUG_STATS
	if (buf->status & RR1_RX_ORUN_ERR)
	    overrun++;
#endif
    }
    if ((u_short)(dp->dp_rx_prod - dp->dp_rx_cons) > RX_BUF_SZ) {
	DEBUGINC(dp_overrun);
	dp->dp_rx_cons = dp->dp_rx_prod - RX_BUF_SZ;
    }
}

static void
do_data_rx(dport_t *dp)
{
    int duartnum = dp->dp_index / 2 * 2;
    toid_t *toid = &dports[duartnum].dp_toid;

    dp->dp_flags &= ~DP_SAW_RX;

    if (dp->dp_notify & N_DATA_READY)
	UP_DATA_READY(GPORT(dp));

    if (!(dp->dp_flags & DP_SAW_RX))
	/* nobody read in the pending RX data. This will cause the RX
	 * interrupt to trigger again. We could just disable the RX
	 * intr here, but it would be even better to pull in the RX
	 * data to a buffer until someone can use it, since this will
	 * lessen the likelihood of an RX overrun
	 */
	pull_rx_data(dp);

    /* If there is not currently a timeout pending, start one off.  */
    if (*toid == 0 && dp->dp_timeout_delay)
	*toid = timeout(z8530_intr_timeout,
			(void*)(long)duartnum,
			dp->dp_timeout_delay);
    
    /* If chars were received this interrupt and a timeout is pending
     * we want to interrupt every 4 chars received. If no chars were
     * received this interrupt or no timeout is pending, we want to
     * interrupt every char
     */
    if (*toid) {
	if ((dp->dp_wr7 & WR7_RX_HALF_FIFO) == 0) {
	    dp->dp_wr7 |= WR7_RX_HALF_FIFO;
	    WR_CNTRL(dp->dp_cntrl, WR7, dp->dp_wr7);
	    DEBUGINC(tohalf);
	}
	else
	    DEBUGINC(khalf);
    }
    else {
	if (dp->dp_wr7 & WR7_RX_HALF_FIFO) {
	    dp->dp_wr7 &= (~WR7_RX_HALF_FIFO);
	    WR_CNTRL(dp->dp_cntrl, WR7, dp->dp_wr7);
	    DEBUGINC(toone);
	}
	else
	    DEBUGINC(kone);
    }
}

static void
do_rx_fallback(dport_t *dp)
{
    /* If there is no pending timeout and we are interrupting every 4
     * chars, we may not notice one or two chars pending in the input
     * fifo. Set to interrupt every char in this case
     */
    if (dports[dp->dp_index / 2 * 2].dp_toid == 0 &&
	(dp->dp_wr7 & WR7_RX_HALF_FIFO)) {
	dp->dp_wr7 &= (~WR7_RX_HALF_FIFO);
	WR_CNTRL(dp->dp_cntrl, WR7, dp->dp_wr7);
	DEBUGINC(toone);
    }
}

static int
z8530_handle_intr(int duartnum, int from_timeout)
{
    dport_t *dp0 = &dports[duartnum];	/* port B on DUART */
    dport_t *dp1 = &dports[duartnum+1];	/* port A on DUART */
    char rr0, rr3;
    u_char du_cntrl;
    
    SIO_LOCK_PORT(GPORT(dp0));

    if (from_timeout)
	dp0->dp_toid = 0;

    rr3 = RD_CNTRL(dp1->dp_cntrl, RR3);

    if (rr3 == 0) {
	do_rx_fallback(dp0);
	do_rx_fallback(dp1);
	SIO_UNLOCK_PORT(GPORT(dp0));
	return(0);
    }

    if (rr3 & RR3_CHNB_EXT_IP) {
	rr0 = RD_RR0(dp0->dp_cntrl);
	if (dp0->dp_notify & N_DCTS)
	    UP_DCTS(GPORT(dp0), DU_CTS_ASSERTED(rr0));
	if (dp0->dp_notify & N_DDCD)
	    UP_DDCD(GPORT(dp0), DU_DCD_ASSERTED(rr0));
	if (rr0 & RR0_BRK)
	    dp0->dp_flags |= DP_BREAK;
	/* reset latches */
	WR_WR0(dp0->dp_cntrl, WR0_RST_EXT_INT);
    }
    
    if (rr3 & RR3_CHNA_EXT_IP) {
	rr0 = RD_RR0(dp1->dp_cntrl);
	if (dp1->dp_notify & N_DCTS)
	    UP_DCTS(GPORT(dp1), DU_CTS_ASSERTED(rr0));
	if (dp1->dp_notify & N_DDCD)
	    UP_DDCD(GPORT(dp1), DU_DCD_ASSERTED(rr0));
	if (rr0 & RR0_BRK)
	    dp1->dp_flags |= DP_BREAK;
	/* reset latches */
	WR_WR0(dp1->dp_cntrl, WR0_RST_EXT_INT);
    }

    if (rr3 & RR3_CHNB_RX_IP) {
	DEBUGINC(rxintr);
	do_data_rx(dp0);
    }

    if (rr3 & RR3_CHNA_RX_IP) {
	DEBUGINC(rxintr);
	do_data_rx(dp1);
    }

    if (rr3 & RR3_CHNB_TX_IP) {
	DEBUGINC(txintr);
	if (dp0->dp_notify & N_OUTPUT_LOWAT)
	    UP_OUTPUT_LOWAT(GPORT(dp0));
    }
    
    /* give kernel printf output precedence over serial port
     * output. Don't send any serial port output until all kernel
     * printf output is drained.
     */
    if (rr3 & RR3_CHNA_TX_IP) {
	DEBUGINC(txintr);
	if (PRINTF_PENDING(dp1))
	    ducw_send();
	else if (dp1->dp_notify & N_OUTPUT_LOWAT)
	    UP_OUTPUT_LOWAT(GPORT(dp1));

	/* For the console port, the TX interrupt can also be turned
	 * on by kernel printf, so we can't rely on it being disabled
	 * just because N_OUTPUT_LOWAT wasn't requested, as we can
	 * with ordinary ports
	 */
	else if (dp1->dp_wr1 & WR1_TX_INT_ENBL) {
	    dp1->dp_wr1 &= ~WR1_TX_INT_ENBL;
	    WR_CNTRL(dp1->dp_cntrl, WR1, dp1->dp_wr1);
	}
    }
    
    SIO_UNLOCK_PORT(GPORT(dp0));
    return(1);
}

static void
z8530_handle_intrs(void)
{
    int did_something, duartnum, i;
    
    do {
	did_something = 0;

	for(duartnum = 0; duartnum < ev_num_serialports; duartnum += 4) {
	    for (i = duartnum; i < duartnum+4; i += 2) {
		if (z8530_handle_intr(i, 0))
		    did_something = 0;
	    }
	}
    } while (did_something);

    enable_intrs();
}

/*
 * poll for control-A from the console before hwgraph is setup, may cause
 * loss of character.  this routine should only be defined in the UART
 * driver that controls the console
 */
void
console_debug_poll(void)
{
    char c, status;
    dport_t *dp = CONSOLE_DP;
    u_char du_cntrl;

    if (dp && kdebug && (RD_RR0(dp->dp_cntrl) & RR0_RX_CHR)) {
	status = RD_CNTRL(dp->dp_cntrl, RR1);
	if (status & (RR1_FRAMING_ERR | RR1_RX_ORUN_ERR | RR1_PARITY_ERR))
	    WR_WR0(dp->dp_cntrl, WR0_RST_ERR);
	c = RD_DATA(dp->dp_data);

	if (c == DEBUG_CHAR)
	    debug("ring");
    }
}
