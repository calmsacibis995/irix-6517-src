#ifndef SYS_MPZDUART_H
#define SYS_MPZDUART_H


/* Copyright 1986,1993, Silicon Graphics Inc., Mountain View, CA. */

/*
 * Typedefs for MP streams driver for the Zilog Z85130 serial communication
 * controller.
 *
 * $Revision: 1.23 $
 */
#include <sys/termio.h>

#ifdef DEBUG

/* If any additions are made to this enum, they must be made to the
 * duact_strings array in idbg.c as well
 */
enum duact {
    DU_ACT_EMPTY,  /* must be first */

    DU_ACT_EXEC_TX,
    DU_ACT_EXEC_TX_RET0,

    DU_ACT_TX_PUTCMD,
    DU_ACT_TX_PUTCMD_RET0,
    DU_ACT_TX_PUTCMD_RET1,

    DU_ACT_RX_BUF_GETCHAR,
    DU_ACT_RX_BUF_GETCHAR_RET0,
    DU_ACT_RX_BUF_GETCHAR_RET1,

    DU_ACT_RX_BUF_PUTCHAR,
    DU_ACT_RX_BUF_PUTCHAR_RET0,
    DU_ACT_RX_BUF_PUTCHAR_RET1,
    DU_ACT_RX_BUF_PUTCHAR_RET2,
    DU_ACT_RX_BUF_PUTCHAR_RUNLOW,
    
    DU_ACT_GET_INCHARS,
    DU_ACT_GET_INCHARS_RET0,

    DU_ACT_RCLR,
    DU_ACT_RCLR_RET0,

    DU_ACT_ZAP,
    DU_ACT_ZAP_RET0,

    DU_ACT_DELAY,
    DU_ACT_DELAY_RET0,

    DU_ACT_FLUSHW,
    DU_ACT_FLUSHW_RET0,

    DU_ACT_FLUSHR,
    DU_ACT_FLUSHR_RET0,

    DU_ACT_SAVE,
    DU_ACT_SAVE_RET0,

    DU_ACT_TCSET,
    DU_ACT_TCSET_RET0,

    DU_ACT_I_IOCTL,
    DU_ACT_I_IOCTL_RET0,
    DU_ACT_I_IOCTL_RET1,
    DU_ACT_I_IOCTL_ITIMER,

    DU_ACT_OPEN,
    DU_ACT_OPEN_RET0,
    DU_ACT_OPEN_RET1,
    DU_ACT_OPEN_RET2,
    DU_ACT_OPEN_RET3,
    DU_ACT_OPEN_RET4,
    DU_ACT_OPEN_MODEM,
    DU_ACT_OPEN_FLOW_MODEM,

    DU_ACT_CLOSE,
    DU_ACT_CLOSE_RET0,

    DU_ACT_RSRV,
    DU_ACT_RSRV_RET0,
    DU_ACT_RSRV_RET1,
    DU_ACT_RSRV_PUTNEXT,

    DU_ACT_WPUT,
    DU_ACT_WPUT_RET0,

    DU_ACT_HANDLE_INTR_HI,
    DU_ACT_HANDLE_INTR_LO,
    DU_ACT_HANDLE_INTR_RET0,
    DU_ACT_HANDLE_INTR_RET1,
    DU_ACT_HANDLE_INTR_CTS_ON,
    DU_ACT_HANDLE_INTR_CTS_OFF,
    DU_ACT_HANDLE_INTR_DCD_ON,
    DU_ACT_HANDLE_INTR_DCD_OFF,
    DU_ACT_HANDLE_INTR_ESTAT_INTR,
    DU_ACT_HANDLE_INTR_TXE_INTR,

    DU_ACT_GETBP,
    DU_ACT_GETBP_RET0,
    DU_ACT_GETBP_RET1,
    DU_ACT_GETBP_RET2,
    DU_ACT_GETBP_RET3,
    DU_ACT_GETBP_RET4,

    DU_ACT_SLOWR,
    DU_ACT_SLOWR_RET0,
    DU_ACT_SLOWR_RET1,
    DU_ACT_SLOWR_RET2,

    DU_ACT_PROCESS_INCHAR,
    DU_ACT_PROCESS_INCHAR_RET0,
    DU_ACT_PROCESS_INCHAR_RET1,
    DU_ACT_PROCESS_INCHAR_RET2,
    DU_ACT_PROCESS_INCHAR_RET3,
    DU_ACT_PROCESS_INCHAR_RET4,
    DU_ACT_PROCESS_INCHAR_RET5,
    DU_ACT_PROCESS_INCHAR_RET6,
    DU_ACT_PROCESS_INCHAR_RET7,
    DU_ACT_PROCESS_INCHAR_RET8,
    DU_ACT_PROCESS_INCHAR_RET9,
    DU_ACT_PROCESS_INCHAR_RX_XOFF,

    DU_ACT_RX,
    DU_ACT_RX_RET0,
    DU_ACT_RX_RET1,
    DU_ACT_RX_RET2,
    DU_ACT_RX_RET3,
    DU_ACT_RX_RET4,

    DU_ACT_CON,
    DU_ACT_CON_RET0,

    DU_ACT_COFF,
    DU_ACT_COFF_RET0,

    DU_ACT_TX,
    DU_ACT_TX_RET0,
    DU_ACT_TX_RET1,
    DU_ACT_TX_RET2,
    DU_ACT_TX_RET3,
    DU_ACT_TX_RET4,
    DU_ACT_TX_RET5,
    DU_ACT_TX_RET6,
    DU_ACT_TX_RET7,
    DU_ACT_TX_RET8,
    DU_ACT_TX_RET9,
    DU_ACT_TX_RET10,
    DU_ACT_TX_RET11,
    DU_ACT_TX_RET12,
    DU_ACT_TX_RET13,
    DU_ACT_TX_RET14,
    DU_ACT_TX_XON,
    DU_ACT_TX_XOFF,
    DU_ACT_TX_ENABLE,

    DU_ACT_GL_SPH,
    DU_ACT_GL_SPH_RET0,

    DU_ACT_CONS_READ,
    DU_ACT_CONS_READ_RET0,

    DU_ACT_CONS_WRITE,
    DU_ACT_CONS_WRITE_RET0,
    DU_ACT_CONS_WRITE_RET1,

    DU_ACT_CONPOLL,
    DU_ACT_CONPOLL_RET0,
    DU_ACT_CONPOLL_RET1,
    DU_ACT_CONPOLL_RET2,
    DU_ACT_CONPOLL_RET3,

    DU_ACT_GETCHAR,
    DU_ACT_GETCHAR_RET0,
    DU_ACT_GETCHAR_RET1,

    DU_ACT_PUTCHAR,
    DU_ACT_PUTCHAR_RET0,

    DU_ACT_TXRDY,
    DU_ACT_TXRDY_RET0,

    DU_ACT_TXRDY_NOLOCK,
    DU_ACT_TXRDY_NOLOCK_RET0,

    DU_ACT_MIPS_PUTCHAR,
    DU_ACT_MIPS_PUTCHAR_RET0,
    DU_ACT_MIPS_PUTCHAR_ENABLE,

    DU_ACT_MIPS_PUTCHAR_NOLOCK,
    DU_ACT_MIPS_PUTCHAR_NOLOCK_RET0,

    DU_ACT_MIPS_GETCHAR,
    DU_ACT_MIPS_GETCHAR_RET0,
    DU_ACT_MIPS_GETCHAR_RET1,
    DU_ACT_MIPS_GETCHAR_FRAME_ERR,
    DU_ACT_MIPS_GETCHAR_OVERRN_ERR,
    DU_ACT_MIPS_GETCHAR_PARITY_ERR,

    DU_ACT_NOLOCK_GETCHAR,
    DU_ACT_NOLOCK_GETCHAR_RET0,
    DU_ACT_NOLOCK_GETCHAR_RET1,

    DU_ACT_STOP_TX,
    DU_ACT_STOP_TX_RET0,

    DU_ACT_START_TX,
    DU_ACT_START_TX_RET0,
    DU_ACT_START_TX_RET1,
    DU_ACT_START_TX_ENABLE,
    
    DU_ACT_STOP_RX,
    DU_ACT_STOP_RX_RET0,
    DU_ACT_STOP_RX_RTS_DRT_DEASSERT,
    
    DU_ACT_START_RX,
    DU_ACT_START_RX_RET0,
    DU_ACT_START_RX_CTS_ON,
    DU_ACT_START_RX_CTS_OFF,
    DU_ACT_START_RX_DCD_ON,
    DU_ACT_START_RX_DCD_OFF,
    DU_ACT_START_RX_RTS_DTR_ASSERT,

    DU_ACT_CONFIG,
    DU_ACT_CONFIG_RET0,
    DU_ACT_CONFIG_RET1,
    DU_ACT_CONFIG_RET2,

    DU_ACT_IFLOW,
    DU_ACT_IFLOW_RET0,
    DU_ACT_IFLOW_RET1,
    DU_ACT_IFLOW_RET2,
    DU_ACT_IFLOW_RET3,
    DU_ACT_IFLOW_RTS_ASSERT,
    DU_ACT_IFLOW_RTS_DEASSERT,
    
    DU_ACT_BREAK,
    DU_ACT_BREAK_RET0,

    DU_ACT_MAX /* must be last */
};

struct duactentry {
	enum duact dae_act;
	time_t	dae_sec;
	long	dae_usec;
	int	dae_info;
	int	dae_cpu;
};

#define	DU_ACTLOG_SIZE		128		/* must be a power of 2 */
#endif /* DEBUG */

/*
 * each port has a data structure that looks like this
 */
typedef struct dport {
	/* hardware address */
	volatile u_char	*dp_cntrl;	/* port control	reg */
	volatile u_char	*dp_data;	/* port data reg */
	
	u_char	dp_index;		/* port number */

	/* z8530 return 1's in unused bits, so, we need a bit mask */
	u_char	dp_bitmask;

	/*
	 * Access spinlock to protect hardware/software consistancy,
	 * chip-level access and fields shared by interrupt and
	 * non-interrupt code.  Currently the protected fields are:
	 *
	 *	dp_porthandler
	 *	dp_rq
	 *	dp_state
	 *	dp_wq
	 *	dp_wr1
	 *	dp_wr3
	 *	dp_wr4
	 *	dp_wr5
	 *	dp_wr15
	 *
	 * These fields are also marked with !P! below.
	 *
	 * Note that only one lock is used per chip to single thread
	 * all chip access.  Locks are assigned as follows:
	 *
	 *	DUART0: (dport[0] & dport[1]) uses dport[0].dp_port_lock
	 *	DUART1: (dport[2] & dport[3]) uses dport[2].dp_port_lock
	 *	DUART2: (dport[4] & dport[5]) uses dport[4].dp_port_lock
	 */
	lock_t	dp_port_lock;		/* lock to protect critical stuff */
	int	dp_port_locktrips;	/* multiple locks by same cpu */
	int	dp_port_lockcpu;	/* cpu holding lock */

	/* software copy of hardware registers */
	u_char	dp_wr1;			/* !P! individual interrupt mask */
	u_char	dp_wr3;			/* !P! receiver parameters */
	u_char	dp_wr4;			/* !P! transmitter/receiver params */
	u_char	dp_wr5;			/* !P! transmitter parameters */
	u_char	dp_wr7;
	u_char	dp_wr15;		/* !P! external events interrupt mask */

	int	dp_rsrv_blocked;	/* rsrv routine blocked */
	int	dp_rsrv_blk_consec;
	int	dp_rsrv_blk_consec_max;

	struct termio dp_termio;
#define dp_iflag dp_termio.c_iflag	/* use some of the bits (see below) */
#define dp_cflag dp_termio.c_cflag	/* use all of the standard bits */
#define dp_ospeed dp_termio.c_ospeed
#define dp_ispeed dp_termio.c_ispeed
#define dp_line  dp_termio.c_line	/* line discipline */
#define dp_cc    dp_termio.c_cc		/* control characters */

	int	(*dp_porthandler)(unsigned char);	/* !P! textport hook */
	queue_t	*dp_rq, *dp_wq;		/* !P! our queues */
	u_int	dp_state;		/* !P! current state */
	sv_t	dp_close_wait;		/* closes wait here for clean-up */

	mblk_t	*dp_cur_rbp;		/* current input buffer */
	mblk_t	*dp_nxt_rbp;		/* "next" input buffer */
	mblk_t	*dp_wbp;		/* current output buffer */

	toid_t	dp_tid;			/* (recent) output delay timer ID */

	int	dp_fe, dp_over;		/* framing/overrun errors counts */
	int	dp_parity;		/* parity error count */
	int	dp_allocb_fail;		/* loss due to allocb() failures */
	int	dp_rb_over;		/* loss due to input buffer overflow */
	int	dp_rb_over1;		/* loss due to input buffer overflow */
	int	dp_rb_hiwat;
	int	dp_rxchars;		/* chars received */
	int	dp_txchars;		/* chars xmitted */
	short	*dp_rx_buf;		/* Low-level RX buffer */
        lock_t  dp_rx_buflock;		/* Spinlock for RX buffer */
        volatile int dp_rx_rind;	/* RX buffer read index	*/
        volatile int dp_rx_wind;	/* RX buffer write index */
	u_int	dp_full_rx_buf;		/* Losses due to full RX buffer */
	u_int	dp_hiwat_rx_buf;	/* RX buffer above hiwat mark */

#ifdef DEBUG
#define MAX_CHARCNT 11
	u_int	dp_rx_charcnt[MAX_CHARCNT];
	u_int	dp_rx_max_charcnt;
#endif /* DEBUG */

#define TX_BUFSIZE	4		/* Must be a power of 2 */

	volatile struct {		/* Low-level TX buffer */
		u_char cmd;
		u_char arg;
#ifdef DEBUG
		/*
		 * Each read or write of the tx buffer is logged by placing
		 * the value of the global sequence counter into the
		 * appropriate sequence mark slot below and then incrementing
		 * the global counter.  Thus, the relative sequence of
		 * all tx buffer related actions can be reconstructed.
		 */
		u_int wseq;             /* Last write sequence mark         */
		u_int rseq;             /* Last read sequence mark          */
		u_short wcpu;           /* CPU that did last write          */
		u_short rcpu;           /* CPU that did last read           */
#endif /* DEBUG */
	} dp_tx_buf[TX_BUFSIZE];

        lock_t  dp_tx_buflock;		/* Spinlock for TX buffer */

        volatile int dp_tx_rind;	/* TX buffer read index		*/
        volatile int dp_tx_wind;	/* TX buffer write index	*/
	u_int	dp_full_tx_buf;		/* Full TX buffer errors	*/

#ifdef DEBUG
        volatile int dp_tx_seq;		/* Command sequence counter         */
#endif /* DEBUG */

#if DEBUG
	volatile struct duactentry dp_actlog[DU_ACTLOG_SIZE];
        volatile uint dp_al_wind;	/* Activity log write index */
#endif /* DEBUG */

	__psunsigned_t	dp_swin;		/* Small window pointer */
	u_char	dp_last_rr3;		/* RR3 seen at high intr level */
	u_char	dp_rxsched;		/* chars were received at hi intr */
	u_char	dp_rx_lointr_running;	/* lo level intr is running */
	u_char	dp_saw_rx;		/* received chars on this port */
} dport_t;


#define RX_BUFSIZE	256
#if RX_BUFSIZE & (RX_BUFSIZE-1)		/* Must be a power of 2 */
choke;
#endif
#define RXBUFSIZE (RX_BUFSIZE * sizeof(short))
	
extern struct dport *du_minordev_to_dp(dev_t);

#endif /* SYS_MPZDUART_H */
