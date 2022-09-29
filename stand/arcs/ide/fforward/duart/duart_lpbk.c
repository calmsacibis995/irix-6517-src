#include "sys/types.h"
#include "sys/param.h"
#include "sys/sbd.h"
#include "sys/cpu.h"
#include "sys/z8530.h"
#include "fault.h"
#include "setjmp.h"
#include "libsk.h"
#include "libsc.h"
#include "uif.h"

#define	APPLE_MODE		4
#define	EXTERNAL_LPBK_MODE	2
#define	INTERNAL_LPBK_MODE	1

#ifdef IP20
#define	KEYBOARD_CHANNEL	0
#define	MOUSE_CHANNEL		1
#define	TTY_CONSOLE_CHANNEL	2
#define MAX_CHANNEL 3
#define Z85x30 "Z85130"
/* enable interrupt + power switch */
#define DUART_SR SR_IEC|SR_IBIT3
#else
#define	TTY_CONSOLE_CHANNEL	0
#define MAX_CHANNEL 1
#define Z85x30 "Z85230"
/* enable interrupt + power switch */
#define DUART_SR SR_PROMBASE|SR_IEC|SR_IBIT3|SR_IBIT4
#endif

#define	DUART_WAIT		1000
#define	BAD_RR1_STATUS	(RR1_FRAMING_ERR | RR1_RX_ORUN_ERR | RR1_PARITY_ERR)

static void configure_duart(volatile u_char *, u_short, int);

#ifdef IP20
static void setup_enet_carrier_detection(void);
#endif

static u_char *duart_ctrl[MAX_CHANNEL*2] = {
#ifdef IP20
	(u_char *)PHYS_TO_K1(DUART0A_CNTRL),
	(u_char *)PHYS_TO_K1(DUART0B_CNTRL),
#endif
	(u_char *)PHYS_TO_K1(DUART1B_CNTRL),
	(u_char *)PHYS_TO_K1(DUART1A_CNTRL),
#ifdef IP20
	(u_char *)PHYS_TO_K1(DUART2B_CNTRL),
	(u_char *)PHYS_TO_K1(DUART2A_CNTRL),
#endif
};

#undef DELAY
#define DELAY(n) us_delay(time_k*(n))
static int time_k = 1;

/* DUART loopback test */
int
duart_loopback (int argc, char **argv)

{
	/* do 600 last to please the keyboard */
	static u_short 	baud[] = {
				38400, 19200, 9600, 7200,
				4800, 2400, 1200, 300, 600,
			};
	int 		channel;
	char 		*command = *argv;
	volatile u_char	*ctrl;
	u_char 		data;
	volatile u_char *datap;
	static u_char 	*duart_data[MAX_CHANNEL*2] = {
#ifdef IP20
				(u_char *)PHYS_TO_K1(DUART0A_DATA),
				(u_char *)PHYS_TO_K1(DUART0B_DATA),
#endif
				(u_char *)PHYS_TO_K1(DUART1B_DATA),
				(u_char *)PHYS_TO_K1(DUART1A_DATA),
#ifdef IP20
				(u_char *)PHYS_TO_K1(DUART2B_DATA),
				(u_char *)PHYS_TO_K1(DUART2A_DATA),
#endif
			};
	int		errcount = 0;
	jmp_buf 	faultbuf;
	extern int	gfxalive;
	int		i;
	int		j;
	int		k;
	int		mode;
	/* do 0x00 and 0x01 last to turn off LEDs on keyboard */
	static u_char 	pattern[] = {
		~0x00, (u_char)~0xaa, (u_char)~0xcc, (u_char)~0xf0,
		0xf0, 0xcc, 0xaa, 0x00,
	  	0x01,
	};
	u_char		rr0;
	u_char		rr1;
	k_machreg_t	old_SR = get_SR();
#ifdef IP20
	char *c = getenv("console");
#endif

#ifdef IP20
	for (i = 0; i < MAX_CHANNEL; i += 2) {
		ctrl = duart_ctrl[i];
		WR_CNTRL(ctrl, WR15, WR15_Z85130_EXTENDED)
		WR_CNTRL(ctrl, WR7, WR7_EXTENDED_READ)
		if (RD_CNTRL(ctrl, RR14) != WR7_EXTENDED_READ) {
			msg_printf(ERR,
				"Z8530 IS BEING USED INSTEAD OF Z85130\n");
			return 1;
		}
	}
#endif
	if (argc != 2)
		errcount++;
	else {			/* parse command options */
		argv++;	

		if ((*argv)[0] != '-')
			errcount++;
		else {
			switch ((*argv)[1]) {

	    		case 'a':	/* -a -> apple compatibility */
				mode = APPLE_MODE;
				(void)atob(*argv + 2, &channel);
				break;

	    		case 'e':	/* -e -> external loopback */
				mode = EXTERNAL_LPBK_MODE;
				(void)atob(*argv + 2, &channel);
				break;

	    		case 'i':	/* -i -> internal loopback */
				mode = INTERNAL_LPBK_MODE;
				(void)atob(*argv + 2, &channel);
				break;

	    		default:
				errcount++;
				break;
	    		}
		}
	}

	if (channel > MAX_CHANNEL)
		errcount++;

	if (errcount) {
		msg_printf(ERR,
	    		"Usage: %s -(a|e|i)(0-%d)\n", command, MAX_CHANNEL);
		return errcount;
	}


#ifdef IP20
	/* if console is set to serial port, the keyboard/mouse ports
	 * never get configured.  This will lock up the duart tests.  The
	 * simple way to get around it is to just not do the test. */
	if (c != 0 && (*c == 'd' || *c == 'r') && channel <= MOUSE_CHANNEL)
		return 0;
#endif

#ifndef IP20
	if (is_ioc1 ()) {
		time_k = 2;
	} else {
		time_k = 1;
	}
#endif

	if (
#ifdef IP20
		(gfxalive && channel == KEYBOARD_CHANNEL) ||
#endif
		(!gfxalive && channel == TTY_CONSOLE_CHANNEL)) {
		msg_printf(VRB,
			"Cannot run loopback test on console channel (ch %d)\n", 
			TTY_CONSOLE_CHANNEL);
		return errcount;
	}

#ifdef IP20
	if (mode == EXTERNAL_LPBK_MODE && channel == MOUSE_CHANNEL) {
		msg_printf(VRB,
			"Cannot run external loopback test on mouse channel\n");
		return errcount;
	}

	if (mode == APPLE_MODE && channel <= MAX_CHANNEL - 2) {
		msg_printf(VRB,
			"Cannot run APPLE compatibility test on channel %d\n", channel);
		return errcount;
	}
#endif

	if (mode == INTERNAL_LPBK_MODE)
		msg_printf(VRB,
			"DUART channel %d internal loopback test\n", channel);
	else if (mode == EXTERNAL_LPBK_MODE)
		msg_printf(VRB,
			"DUART channel %d external loopback test\n", channel);
	else
		msg_printf(VRB,
			"DUART channel %d APPLE compatibility test\n", channel);

	ctrl = duart_ctrl[channel];
	datap = duart_data[channel];

	/* check I/O signals */
	if (mode != INTERNAL_LPBK_MODE
#ifdef IP20
		&& channel != KEYBOARD_CHANNEL
#endif
						) {
		/* disable i/o external status interrupt, reset signals */
		WR_CNTRL(ctrl, WR15, 0x0)
		WR_WR0(ctrl, WR0_RST_EXT_INT)

		if (mode == EXTERNAL_LPBK_MODE) {
			/*
		 	 * the loopback must have RTS connected to CTS,
			 * DTR to DCD, and Rx to Tx
		 	 */
		    	WR_CNTRL(ctrl, WR5, WR5_RTS)
	    		rr0 = RD_RR0(ctrl);
		    	if ((rr0 & (RR0_CTS | RR0_DCD)) != RR0_CTS) {
	    			msg_printf(ERR,
		    			"No loopback / RTS-DTR not connected to CTS-DCD / Bad "Z85x30"\n");
	    	    		return ++errcount;
	    		}

		    	WR_CNTRL(ctrl, WR5, WR5_DTR)
	    		rr0 = RD_RR0(ctrl);
		    	if ((rr0 & (RR0_CTS | RR0_DCD)) != RR0_DCD) {
	    			msg_printf(ERR,
		    			"No loopback / RTS-DTR not connected to CTS-DCD / Bad "Z85x30"\n");
	    	    		return ++errcount;
	    		}
		} else {

			/*
		 	 * the loopback must have DTR connected to CTS,
			 * Rx- to Tx-, and Rx+ to Tx+
		 	 */
	    		WR_CNTRL(ctrl, WR5, WR5_DTR)
	    		rr0 = RD_RR0(ctrl);
	    		if (!(rr0 & RR0_CTS)) {
	    			msg_printf(ERR,
		    			"No loopback / DTR not connected to CTS / Bad "Z85x30"\n");
				return ++errcount;
	    		}

	    		WR_CNTRL(ctrl, WR5, 0x0)
	    		rr0 = RD_RR0(ctrl);
	    		if (rr0 & RR0_CTS) {
	    			msg_printf(ERR,
		    			"No loopback / DTR not connected to CTS / Bad "Z85x30"\n");
				return ++errcount;
	    		}
		}

	}

	/*
	 * since z8530cons.c enables external status interrupt in order to
	 * detect ethernet carrier, we must disable it before running the
	 * test.  otherwise, we may get phantom interrupt
	 */
#ifdef IP20
	if (channel == KEYBOARD_CHANNEL)
		WR_CNTRL(duart_ctrl[MOUSE_CHANNEL], WR1, 0x0)

	/* enable receiver interrupt to go to the MIPS */
	*(volatile u_char *)PHYS_TO_K1(LIO_0_MASK_ADDR) |= LIO_DUART_MASK;
#else
	/* enable receiver interrupt to go to the MIPS */
	*K1_LIO_0_MASK_ADDR |= LIO_LIO2_MASK;
	*K1_LIO_2_MASK_ADDR |= LIO_DUART_MASK;
#endif

	/* use different baud rates and different data patterns */
	for (i = 0; i < sizeof(baud) / sizeof(baud[0]); i++) {
		configure_duart(ctrl, baud[i], mode);

		WR_CNTRL(ctrl, WR1, WR1_RX_INT_ERR_ALL_CHR)
		WR_CNTRL(ctrl, WR9, WR9_MIE)	/* enable interrupt */

		for (j = 0; j < sizeof(pattern) / sizeof(u_char); j++) {
	    		if (setjmp(faultbuf)) {
				if ((_exc_save == EXCEPT_NORM)
			    	    && ((_cause_save & CAUSE_EXCMASK) == EXC_INT)
			    	    && (_cause_save & CAUSE_IP3)
#ifdef IP20
			    	    && (*(volatile u_char *)PHYS_TO_K1(LIO_0_ISR_ADDR)
						    & LIO_DUART)
#else
			    	    && (*(volatile u_char *)(K1_LIO_0_ISR_ADDR) & LIO_LIO2)
				    && (*(volatile u_char *)(K1_LIO_2_ISR_ADDR) & LIO_DUART)
#endif
			            && (RD_RR0(ctrl) & RR0_RX_CHR)) {
					rr1 = RD_CNTRL(ctrl, RR1);
					data = RD_DATA(datap);
		    			if (data != pattern[j]) {
						errcount++;
						msg_printf(ERR,
			    				"Baud: %d, data expected: 0x%02x, actual: 0x%02x\n",
			    				baud[i], pattern[j], data);
						goto duart_lpbk_done;
		    			}

					if (rr1 & BAD_RR1_STATUS) {
						errcount++;
						msg_printf(ERR,
							"Bad Rx status, RR1: 0x%02x\n",
							rr1);
						goto duart_lpbk_done;
					}

		    			continue;
				} else {	/* non-DUART interrupt */

		    			errcount++;
		    			msg_printf(ERR, "Phantom interrupt\n");
		    			show_fault();
		    			goto duart_lpbk_done;
				}
	    		}

			/* make sure interrupt go away */
			while (get_cause() & CAUSE_IP3)
				;

			/* enable interrupt (plus power switch interrupt) */
	    		nofault = faultbuf;
	    		set_SR(DUART_SR);

	    		msg_printf(DBG, "Baud: %d, data: 0x%02x\n",
		    		baud[i], pattern[j]);

			/* wait for Tx empty */
	    		for (k = DUART_WAIT; k > 0; k--) {
	            		if (RD_RR0(ctrl) & RR0_TX_EMPTY)
					break;
		    		else
		    			DELAY(100);
	    		}

			/* too long */
	    		if (!(RD_RR0(ctrl) & RR0_TX_EMPTY)) {
		    		errcount++;
		    		msg_printf(ERR,
		    			"Time out waiting for Tx ready 0x%x\n",
								RD_RR0(ctrl));
		    		goto duart_lpbk_done;
	    		}

	    		WR_DATA(datap, pattern[j])
			DELAY(DUART_WAIT * 100);

			errcount++;
			msg_printf(ERR, "Time out waiting for Rx ready: 0x%x\n",
				RD_RR0(ctrl));
			goto duart_lpbk_done;
	    	}
	}

	if (mode != APPLE_MODE)
		goto duart_lpbk_done;

	/* make sure transmitter can be disabled by deasserting RTS */
	configure_duart(ctrl, 9600, APPLE_MODE);
	WR_CNTRL(ctrl, WR5, WR5_TX_8BIT | WR5_TX_ENBL)

	/* wait for Tx empty */
	for (k = DUART_WAIT; k > 0; k--) {
		if (RD_RR0(ctrl) & RR0_TX_EMPTY)
			break;
		else
			DELAY(100);
	}

	/* too long */
	if (!(RD_RR0(ctrl) & RR0_TX_EMPTY) || RD_RR0(ctrl) & RR0_RX_CHR) {
		errcount++;
		msg_printf(ERR, "Time out waiting for Tx/Rx status\n");
		goto duart_lpbk_done;
	}

	WR_DATA(datap, pattern[0])
	DELAY(DUART_WAIT * 100);

	if (RD_RR0(ctrl) & RR0_RX_CHR) {
		errcount++;
		msg_printf(ERR,
			"Deasserting RTS did not disable Tx in apple compatibility mode\n");
	}

duart_lpbk_done:
	set_SR(old_SR);		/* restore SR */
	nofault = 0;
#ifdef IP20
	*(volatile u_char *)PHYS_TO_K1(LIO_0_MASK_ADDR) &= ~LIO_DUART_MASK;
#else
	*K1_LIO_0_MASK_ADDR &= ~LIO_LIO2_MASK;
	*K1_LIO_2_MASK_ADDR &= ~LIO_DUART_MASK;
#endif

	configure_duart(ctrl, 9600, EXTERNAL_LPBK_MODE);

	if (errcount)
		sum_error("DUART loopback");
	else
		okydoky();

	return errcount;
}



#ifdef IP20
/* configure the DUART */
static void
configure_duart(volatile u_char *p, u_short baud, int mode)
{
	volatile u_char *port_config = (u_char *)PHYS_TO_K1(PORT_CONFIG);
	u_short time_constant =
		(CLK_SPEED + CLK_FACTOR * baud) / (CLK_FACTOR * 2 * baud) - 2;

	if (mode == APPLE_MODE) {
		if ((u_int)p & CHNA_CNTRL_OFFSET)
			*port_config &= ~PCON_SER0RTS;
		else
			*port_config &= ~PCON_SER1RTS;
	} else {
		if ((u_int)p & CHNA_CNTRL_OFFSET)
			*port_config |= PCON_SER0RTS;
		else
			*port_config |= PCON_SER1RTS;
	}

	/* software reset */
	if ((u_int)p & CHNA_CNTRL_OFFSET)
		WR_CNTRL(p, WR9, WR9_RST_CHNA)
	else
		WR_CNTRL(p, WR9, WR9_RST_CHNB)

	/* odd parity for keyboard */
	if (p == duart_ctrl[KEYBOARD_CHANNEL])
		WR_CNTRL(p, WR4, WR4_X16_CLK | WR4_1_STOP | WR4_PARITY_ENBL)
	else
		WR_CNTRL(p, WR4, WR4_X16_CLK | WR4_1_STOP)
	WR_CNTRL(p, WR3, WR3_RX_8BIT)
	if (mode == APPLE_MODE)
		WR_CNTRL(p, WR5, WR5_TX_8BIT | WR5_RTS)
	else
		WR_CNTRL(p, WR5, WR5_TX_8BIT)
	WR_CNTRL(p, WR10, 0x0)			/* NRZ encoding */
	WR_CNTRL(p, WR11, WR11_RCLK_BRG | WR11_TCLK_BRG)

	/* set up baud rate time constant */
	WR_CNTRL(p, WR14, 0x0)			/* disable BRG */
	WR_CNTRL(p, WR12, time_constant & 0xff)
	WR_CNTRL(p, WR13, (time_constant >> 8) & 0xff)
	if (mode != INTERNAL_LPBK_MODE)
		WR_CNTRL(p, WR14, WR14_BRG_ENBL)
	else
		WR_CNTRL(p, WR14, WR14_BRG_ENBL | WR14_LCL_LPBK)

	/* Rx/Tx must not be enabled before setting up other parameters */
	WR_CNTRL(p, WR3, WR3_RX_8BIT | WR3_RX_ENBL)
	if (mode == APPLE_MODE)
		WR_CNTRL(p, WR5, WR5_TX_8BIT | WR5_TX_ENBL | WR5_RTS)
	else
		WR_CNTRL(p, WR5, WR5_TX_8BIT | WR5_TX_ENBL)


	/* sometimes a BREAK gets generated during reset */
	WR_WR0(p, WR0_RST_EXT_INT)			/* reset latches */
	if (RD_RR0(p) & RR0_BRK) {
		while (RD_RR0(p) & RR0_BRK)
			;
		WR_WR0(p, WR0_RST_EXT_INT)		/* reset latches */
	}

	/* clear FIFO and error */
	while (RD_RR0(p) & RR0_RX_CHR) {
		RD_CNTRL(p, RR8);
		WR_WR0(p, WR0_RST_ERR)
	}
	setup_enet_carrier_detection();
}
static void
setup_enet_carrier_detection(void)
{
	volatile u_char *cntrl = (u_char *)PHYS_TO_K1(DUART0B_CNTRL);

	/* for etherent carrier detection */
	WR_CNTRL(cntrl, WR9, 0x00)          /* no interrupt to the MIPS */
	WR_CNTRL(cntrl, WR15, WR15_DCD_IE)  /* latch carrier signal */
	WR_CNTRL(cntrl, WR1, WR1_EXT_INT_ENBL)
}
#else
/* configure the DUART */
static void
configure_duart(volatile u_char *p, u_short baud, int mode)
{
	u_short time_constant =
		(CLK_SPEED + CLK_FACTOR * baud) / (CLK_FACTOR * 2 * baud) - 2;
	extern unsigned int _hpc3_write2;

	if (mode == APPLE_MODE) {
		if ((__psunsigned_t)p & CHNA_CNTRL_OFFSET)
			_hpc3_write2 &= ~UART1_ARC_MODE;
		else
			_hpc3_write2 &= ~UART0_ARC_MODE;
	} else {
		if ((__psunsigned_t)p & CHNA_CNTRL_OFFSET)
			_hpc3_write2 |= UART1_ARC_MODE;
		else
			_hpc3_write2 |= UART0_ARC_MODE;
	}
	*(volatile unsigned int *)PHYS_TO_K1(HPC3_WRITE2) = _hpc3_write2;

	/* software reset */
	if ((__psunsigned_t)p & CHNA_CNTRL_OFFSET)
		WR_CNTRL(p, WR9, WR9_RST_CHNA)
	else
		WR_CNTRL(p, WR9, WR9_RST_CHNB)

	WR_CNTRL(p, WR4, WR4_X16_CLK | WR4_1_STOP)
	WR_CNTRL(p, WR3, WR3_RX_8BIT)
	if (mode == APPLE_MODE)
		WR_CNTRL(p, WR5, WR5_TX_8BIT | WR5_RTS)
	else
		WR_CNTRL(p, WR5, WR5_TX_8BIT)
	WR_CNTRL(p, WR10, 0x0)			/* NRZ encoding */
	WR_CNTRL(p, WR11, WR11_RCLK_BRG | WR11_TCLK_BRG)

	/* set up baud rate time constant */
	WR_CNTRL(p, WR14, 0x0)			/* disable BRG */
	WR_CNTRL(p, WR12, time_constant & 0xff)
	WR_CNTRL(p, WR13, (time_constant >> 8) & 0xff)
	if (mode != INTERNAL_LPBK_MODE)
		WR_CNTRL(p, WR14, WR14_BRG_ENBL)
	else
		WR_CNTRL(p, WR14, WR14_BRG_ENBL | WR14_LCL_LPBK)

	/* Rx/Tx must not be enabled before setting up other parameters */
	WR_CNTRL(p, WR3, WR3_RX_8BIT | WR3_RX_ENBL)
	if (mode == APPLE_MODE)
		WR_CNTRL(p, WR5, WR5_TX_8BIT | WR5_TX_ENBL | WR5_RTS)
	else
		WR_CNTRL(p, WR5, WR5_TX_8BIT | WR5_TX_ENBL)


	/* sometimes a BREAK gets generated during reset */
	WR_WR0(p, WR0_RST_EXT_INT)			/* reset latches */
	if (RD_RR0(p) & RR0_BRK) {
		while (RD_RR0(p) & RR0_BRK)
			;
		WR_WR0(p, WR0_RST_EXT_INT)		/* reset latches */
	}

	/* clear FIFO and error */
	while (RD_RR0(p) & RR0_RX_CHR) {
		RD_CNTRL(p, RR8);
		WR_WR0(p, WR0_RST_ERR)
	}
}
#endif
