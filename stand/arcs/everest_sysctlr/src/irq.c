/*
	irq.c
	Contains the upper level interrupt handling routines.
*/

#include <sp.h>
#include <exec.h>
#include <proto.h>
#include <sys_hw.h>
#include <int6811.h>
#include <io6811.h>
#include <stdio.h>



struct	BLOWER_TACH	fan_a_per;
struct	BLOWER_TACH	fan_b_per;

#ifdef LAB
int	fan_step;
#endif

unsigned char rx1_ch;
/* The global declarations are to speed up interrupt handling */
unsigned char 	irq, irq_buff[10];

/* pointers to circular buffers */
extern struct cbufh		*rx1qp,
						*tx1qp,
						*rx2qp,
						*tx2qp;

extern	char			acia_cntl,			/* Memory copy of control reg */
	 					active_switch,		/* Register for Switch Debounce */
						active_screen;

struct 	RTC	*ptod = (struct RTC *) L_RTC;	/* Pointer to RTC */


unsigned char rx_mode;		/* flag indicating a command is being received */
unsigned char ovr_temp;		/* flag indicating an overtemp has already been*/
							/* processed */
char mf_cmd;				/* flag indicating a manf cmd is being received */
char pwr_dwn_state;

unsigned short	stuck_menu;			/*stuck button counter */
unsigned short	stuck_scrlup;		/*stuck button counter */
unsigned short	stuck_scrldn;		/*stuck button counter */
unsigned short	stuck_execute;		/*stuck button counter */
char			stuck_button;		/*stuck button flag */

/* SCI Serial Communication Interface */
interrupt  void 
SCI_interrupt(void)
{
	struct 	RTC	*ptod = (struct RTC *) L_RTC;	/* Pointer to RTC */
	extern	char		acia_cntl;			/* Memory copy of control reg */
	extern char			cpu_rsp;			/* flag checked during boot arb */
	char				manf_mode;			/* manufacturing mode flag */
	int					loop;

	unsigned char	ch,			/* character buffer */
					status,		/* copy of the SCI status register */
					a_stat;	/* copy of the ACIA status register */

	do {
		/* Determine the source of the interrupt */
		status = SCSR;

		/* check for Receiver data register full condition */
		if (status & SCSR_RDRF) {
			rx1_ch = SCDR;

			/* continue processing the character only if there were not any
				receiver errors */

			manf_mode = ptod->nv_irix_sw & 0x80 ? 1 : 0;
			if(rx1_ch == CNT_Z) {
				flush(rx1qp);
				flush(tx1qp);
				rx_mode = ST_RDY;
			}
			/* check for CPU response during boot mode */
			else if(rx_mode == ST_BOOT && rx1_ch == RSP_CHAR)
				cpu_rsp = 1;

			/* if not in manf mode and a control X is received ignore it */
			else if(!manf_mode && rx1_ch == CNT_X)
				rx1_ch++;

			/* check the debug switch setting for the manufacturing mode */
			else if(manf_mode && (rx_mode == ST_RDY) && (rx1_ch != CNT_X)) { 
				/*jam the char into the transmitter of the 2nd serial port 
				  if the xmiter register is empty and CTS status bit is low 
			  	  (cable connected)
				 */
				loop = 1000;
				while((!(a_stat & ACIA_TDRE) && !(a_stat & ACIA_CTS)) && loop--)
					a_stat = *L_ACIA_STCT;

				/* UART Timeout */
				if (loop <= 0)
					log_event(XMIT2_TO, FP_DISPLAY);

				*L_ACIA_TXRX = rx1_ch;
			}
			else {  
				/* proccess cpu command in manufacturing mode */
				/* Check for rearbitration signal from CPU */
				if ((rx1_ch == CNT_R) && (rx_mode != ST_BOOT)) {
					flush(rx1qp);
					flush(tx1qp);
					send(CPU_PROC, BOOT_ARBITRATE);
					rx_mode = ST_BOOT;
				}
				/* Now check for the receipt of a new command */
				else if ((rx_mode == ST_RDY) && (rx1_ch != '\n')) {  
					/*don't put the cnt-l on the queue when in manf. mode */
					if (!(manf_mode && rx1_ch == CNT_X))
						addq(rx1qp, rx1_ch);
					rx_mode = ST_BUSY;
				}

				/* receipt of command complete, ready for next command */
				else if (rx_mode == ST_BUSY && rx1_ch == '\n') {
					addq(rx1qp, rx1_ch);
					send(CPU_PROC, CPU_CMD);
					rx_mode = ST_RDY;
				}
				else
					addq(rx1qp, rx1_ch);
			}
		}

		/*check for transmitter register empty if the TX interrupt is enabled */
		if ((SCCR2 & SCCR2_TIE) && (SCSR & SCSR_TDRE)) {
			ch = remq(tx1qp);
			if(tx1qp->c_len == 0) /* Disable interrupt if queue is empty*/
				SCCR2 &= ~SCCR2_TIE;
			SCDR = ch;
		}
	}
	while((SCSR & SCSR_RDRF) || ((SCCR2 & SCCR2_TIE) && (SCSR & SCSR_TDRE)));
}

/* SPI Serial Transfer Complete */
interrupt  void 
SPI_interrupt(void)
{
	log_event(SPI_ERR, FP_DISPLAY);
}

/* Pulse Accumulator Input Edge */
interrupt  void 
PAIE_interrupt(void)
{
	log_event(PAIE_ERR, FP_DISPLAY);
}

/* Pulse Accumulator Overflow */
interrupt  void 
PAO_interrupt(void)
{
	log_event(PAO_ERR, FP_DISPLAY);
}

/* Timer Overflow */
interrupt  void 
TO_interrupt(void)
{
	struct	BLOWER_TACH	*ptr;

	/* increment the two overflow counters only after the fist pulse
		has been measured*/
	ptr = &fan_a_per;
	if (ptr->first_pulse)
		ptr->overflow++;

	if (PORTD & TERMINATOR) {
		ptr = &fan_b_per;
		if (ptr->first_pulse)
			ptr->overflow++;
	}

	TFLG2 &= ~0x7F;

}

/* Timer Output Compare 5 */
interrupt  void 
TOC5_interrupt(void)
{
	log_event(TOC5_ERR, FP_DISPLAY);
}

/* Timer Output Compare 4 */
interrupt  void 
TOC4_interrupt(void)
{
	log_event(TOC4_ERR, FP_DISPLAY);
}

/* Timer Output Compare 3 */
interrupt  void 
TOC3_interrupt(void)
{
	log_event(TOC3_ERR, FP_DISPLAY);
}

/* Timer Output Compare 2 */
interrupt  void 
TOC2_interrupt(void)
{
	log_event(TOC2_ERR, FP_DISPLAY);
}

/* Timer Output Compare 1 */
interrupt  void 
TOC1_interrupt(void)
{
	log_event(TOC1_ERR, FP_DISPLAY);
}

/****************************************************************************
 *																			*
 *		TIC3_interrupt:		This Input Capture port is used to detect the 	*
 *							pulses generated by the main blower assembly. 	*
 *							The	tachometer generates two pulses per 		*
 *							revolution.	The maximum speed of the blower 	*
 *							is 1420 rpm.									*
 *																			*
 *			4/21/93	-		Modified the routine to correctly handle over   *
 *							flow situations.  Also, the pulse readings are	*
 *							forced to start over if a front panel button	*
 *							has just been pushed, to avoid invalid 			*
 *							readings.										*
 *																			*
 *			4/22/93	-		The Motorola Reference manual explains how to	*
 *							handle overflows if they occur close in time	*
 *							to the input capture routine.  However, the		*
 *							procedure they used is base on an assumption	*
 *							on how fast the processor responds to the 		*
 *							interrupt.  These assumptions do not work 100%	*
 *							of the time with this application.  Since the 	*
 *							reading of every pulse is not necessary, this	*
 *							routine will only handle cases where overflows	*
 *							are not present during the reading.				*
 ****************************************************************************/
interrupt  void 
TIC3_interrupt(void)
{
	struct	BLOWER_TACH	*ptr;
	unsigned short  counter;

	/* If a button has just been pushed force the reading to the first
	   pulse and avoid the situation of missing overflows during the 
	   processing of a button interrupt.
	 */

	counter = TIC3;
	ptr = &fan_a_per;
	if ((TFLG2 & 0x80) || active_switch) 
		ptr->first_pulse = 0;

	else {

		/* Process the first edge */
		if (!ptr->first_pulse) {		/* check for beginning of period mea. */
			ptr->overflow = 0;
			ptr->pulse_1 = counter;
			ptr->first_pulse = 1;
		}
		else { 							/* Process the second edge */
			ptr->pulse_2 = counter;
			ptr->first_pulse = 0;
			ptr->half_period = (ptr->overflow*0x10000)+ptr->pulse_2 - ptr->pulse_1;
		}
	}
	/* reset interrupt flag in set status flag */
	TFLG1 = 0x01;
	ptr->running = 1;
}

/****************************************************************************
 *																			*
 *		TIC2_interrupt:		This Input Capture port is used to detect the 	*
 *							pulses generated by the aux blower assembly. 	*
 *							The aux blower is present in the Terminator.	*
 *							The	tachometer generates two pulses per 		*
 *							revolution.	The maximum speed of the blower 	*
 *							is 1420 rpm.									*
 *																			*
 *			4/21/93	-		Modified the routine to correctly handle over   *
 *							flow situations.  Also, the pulse readings are	*
 *							forced to start over if a front panel button	*
 *							has just been pushed, to avoid invalid 			*
 *							readings.										*
 *																			*
 *			4/22/93	-		The Motorola Reference manual explains how to	*
 *							handle overflows if they occur close in time	*
 *							to the input capture routine.  However, the		*
 *							procedure they used is base on an assumption	*
 *							on how fast the processor responds to the 		*
 *							interrupt.  These assumptions do not work 100%	*
 *							of the time with this application.  Since the 	*
 *							reading of every pulse is not necessary, this	*
 *							routine will only handle cases where overflows	*
 *							are not present during the reading.				*
 ****************************************************************************/
interrupt  void 
TIC2_interrupt(void)
{
	struct	BLOWER_TACH	*ptr;
	unsigned short  counter;

	counter = TIC2;
	ptr = &fan_b_per;

	/* If a button has just been pushed force the reading to the first
	   pulse and avoid the situation of missing overflows during the 
	   processing of a button interrupt.
	 */

	if ((TFLG2 & 0x80) || active_switch) 
		ptr->first_pulse = 0;

	else {

		/* Process the first edge */
		if (!ptr->first_pulse) {		/* check for beginning of period mea. */
			ptr->overflow = 0;
			ptr->pulse_1 = counter;
			ptr->first_pulse = 1;
		}
		else { 							/* Process the second edge */
			ptr->pulse_2 = counter;
			ptr->first_pulse = 0;
			ptr->half_period = (ptr->overflow*0x10000)+ptr->pulse_2 - ptr->pulse_1;
		}
	}
	/* reset interrupt flag in set status flag */
	TFLG1 = 0x02;
	ptr->running = 1;
}

/* Timer Input Compare 1 */
interrupt  void 
TIC1_interrupt(void)
{
	log_event(TIC1_ERR, FP_DISPLAY);
}

/* Real Time Interrupt */
interrupt  void 
RTI_interrupt(void)
{
	tick();
	/*reset the RTI Flag*/
	TFLG2 &= ~0xBF;
}

/*	4/29/93 -	Changed the shutdown constant to KILL_ALL in the event
				of a stuck button shutdown
 */
/* Interrupt ReQuest */
interrupt  void 
IRQ_interrupt(void)
{


	unsigned char		ch,			/* character buffer */
						cmd_buf[5],	/* manf. cmd command buffer */
						cmd_buf1[10],
						status;		/* copy of the SCI status register */

	unsigned char		cpu;		/* combination of brd and proc */
	int					brd;		/* brd number used in manf. cmd */
	int					proc;		/* proc number used in manf. cmd */
	int					i;
	int					loop;

	struct RTC	*ptod = (struct RTC *) L_RTC;		/* Pointer to RTC */

	extern int			db_sw_enable;

	/* debounce the front panel switch readings */
	irq = *L_IRQ;
	for (i = 0; i < 10; i++) {
		irq_buff[i] = *L_IRQ;
	}
	for (i = 0; i < 10; i++) {
		irq |= (irq_buff[i] & 0x7F);	/* Catch any Active High Signals */
	}


	/*check for External Serial Port Interrupt*/
	if(irq & IRQ_SER_PRT) { 

		/* Read Status Register */
		status = *L_ACIA_STCT;

		/* check for Receiver data register full condition */
		if (status & ACIA_RDRF) {
			ch = *L_ACIA_TXRX;

			/* process the character only if there are no receiver errors */
			if ((status & (ACIA_OVRN | ACIA_PE | ACIA_FE)) == 0) { 

					/*first check for the beginning of a command*/
				if (ch == CNT_X) { 
					mf_cmd = 1;
					flush(rx2qp);
				}
				else if (mf_cmd) { 
					/* Part of a manufacturing command */
					addq(rx2qp, ch);

					/* check for command completion */
					if (mf_cmd && ch == CNT_Y) {
						addq(rx2qp, '\0');
						mf_cmd = 0;
						for (i = 0; i < 5; i++) {
							cmd_buf[i] = remq(rx2qp);
							if (cmd_buf[i] == 0)
								break;
						}
						switch (cmd_buf[0]) {
							case	'S':			/* Select CPU */
							case	's':			/* Select CPU */
								sscanf(&cmd_buf[1],"%1x%1x", &brd, &proc);
								cpu = (brd << 2) | (proc & 0x3); 
								/*load a the serial add, but don't 
								   modify the PEN_D,E settings*/
								ptod->nv_ser_add = (ptod->nv_ser_add & 0xC0) | (~cpu & 0x3F);
								*L_SER_ADD = ptod->nv_ser_add;
								delay(SW_20MS);
								async_init(INIT_PORT);
								addq(tx1qp, CNT_X);
								addq(tx1qp, '30' + (proc & 0x3));
								addq(tx1qp, '\0');
								SCCR2 |= SCCR2_TIE;
								break;
							case	'R':			/* Issue an SCLR */
							case	'r':			/* Issue an SCLR */
								send(SEQUENCE, FP_SCLR);
								break;
							case	'N':			/* Issue a NMI */
							case	'n':			/* Issue a NMI */
								send(SEQUENCE, MN_NMI);
								break;
							case	'C':			/* Cycle Power */
							case	'c':			/* Cycle Power */
								log_event(PWR_CYCLE, FP_DISPLAY);
								send(SEQUENCE, DOWN_PARTIAL);
								timeout(SEQUENCE, POWER_UP, FIVE_SEC);
								break;
							case	'D':			
							case	'd':
													/* Set Debug Settings */
								cmd_buf1[0] = 'x';		/* don't care */
								cmd_buf1[1] = 'x';		/* don't care */
								cmd_buf1[2] = cmd_buf[1];
								cmd_buf1[3] = cmd_buf[2];
								cmd_buf1[4] = cmd_buf[3];
								cmd_buf1[5] = cmd_buf[4];
								cmd_buf1[6] = 0;
								set_debug_sw(cmd_buf1);
								break;
							default:
								break;
						}
					}
				}
				/* check the debug switch setting for the manufacturing mode */
				else if(ptod->nv_irix_sw & 0x80) { 
						/* not the beggining of a command or part of a cmd,
						 * so jam the character in the xmitter register 
						 */
						loop = 1000;
						while(!(SCSR & SCSR_TDRE) && loop--); 
						/* UART Timeout */
						if (loop <= 0)
							log_event(XMIT1_TO, FP_DISPLAY);

						SCDR = ch;
					}
				else
					addq(rx2qp, ch);
			}
		}

		/* IF the interrupt bit is still enabled, and since the xmitter 
		   intterupt is not enabled by software, an interrupt here is 
		   probably caused by the DCD bit toggling as a result of the serial 
		   cable being connected.  The data register needs to be read to clear
		   the interrupt, but only  if a valid charater is not present
		 */

		if (status & ACIA_IRQ && !(status & ACIA_RDRF)) 
			ch = *L_ACIA_TXRX;
	}

	/* check for front panel switch interrupts*/
	if((irq & 0x0f) == (IRQ_SCRLUP | IRQ_SCRLDN ))  {
		stuck_scrlup++;
		stuck_scrldn++;

		/* if the key switch is in the ON state set the flag to -1 */
		if (!(PORTD & SWITCH_ON))
			db_sw_enable = -1;

		/* if the key switch is in the MGR state set the flag to -1 */
		/* and the flag is -1 set flag to 1 */
		else if (!(PORTD & SWITCH_MGR) && (db_sw_enable == -1)) {
			db_sw_enable = 1;
			timeout(DISPLAY, DEBUG_SET_TO, THREE_MIN);
		}
	}
	else {
		if(irq & IRQ_MENU ) {
			stuck_menu++;
				if (!(active_switch & IRQ_MENU)) {
				active_switch |= IRQ_MENU;
				send(DISPLAY, MENU);
			}
		}
		else if(irq & IRQ_SCRLUP) {
			stuck_scrlup++;
			if (!(active_switch & IRQ_SCRLUP)) {
				active_switch |= IRQ_SCRLUP;
				send(DISPLAY, SCROLL_UP);
			}
		}
		else if(irq & IRQ_SCRLDN) {
			stuck_scrldn++;
			if (!(active_switch & IRQ_SCRLDN)) {
				active_switch |= IRQ_SCRLDN;
				send(DISPLAY, SCROLL_DN);
			}
		}
		else if(irq & IRQ_EXECUTE) {
			stuck_execute++;
			if (!(active_switch & IRQ_EXECUTE)) {
				active_switch |= IRQ_EXECUTE;
#ifdef LAB
				if (active_screen == ACT_FANTST)
					fan_step = 1;
				else
					send(DISPLAY, EXECUTE);
#else
				send(DISPLAY, EXECUTE);
#endif
			}
		}
	}

	/*check for SCLR Interrupt*/
	if(irq & IRQ_SCLR && (ptod->nv_pcntl & SCLR)) {
		/*wait for 200 msec and then reboot the system */
		delay(SW_200MS);
		log_event(SCLR_DETECT, FP_DISPLAY);
		/* Turn off Fail LED */
		ptod->nv_lcd_cntl |= FAULT_LED_L;
		*L_LCD_CNT = ptod->nv_lcd_cntl;		/* turn off the fault LED */
		send(CPU_PROC, BOOT_ARBITRATE);
		ptod->nv_cmd_chk = 0;
	}

	/*  the remainder of the irq code is to detect a failed or stuck front
		panel button.  In addition to a button being stuck the code will
		monitor the chassis/board over temperature switch and the key switch
		being in the off position.  All other functions of the system 
		controller will not work properly, but the main concern here is to 
		avoid a hazardous condition.
	*/

	/* check for the key switch being in the off position */
	/* and a button being held for a min. of 5 seconds.   */
	/* Shut down without and warning */
	if ((stuck_menu > 0x2000   || stuck_scrlup > 0x2000  ||
		stuck_scrldn > 0x2000 || stuck_execute > 0x2000) && 
		(*L_IRQ & IRQ_KEY)) { 
		log_event(STUCK_BUTTON, FP_DISPLAY);
		log_event(SYS_OFF, FP_DISPLAY);
		shutdown(KILL_ALL);
	}

	/* Check for an over temperature interrupt and a button
	   being stuck closed 
	 */
	if ((stuck_menu > 0x2000   || stuck_scrlup > 0x2000  ||
		stuck_scrldn > 0x2000 || stuck_execute > 0x2000) && 
		(*L_XIRQ & XIRQ_OVERTEMP)) {
		log_event(STUCK_BUTTON, FP_DISPLAY);
		log_event(TEMP_BRD, FP_DISPLAY);
		shutdown(KILL_ALL);
	}

	if (stuck_menu > 0xA000   || stuck_scrlup > 0xA000 ||
		stuck_scrldn > 0xA000 || stuck_execute > 0xA000   ) {

		/* log an error message if one has not already been posted*/
		if(!stuck_button) {
			log_event(STUCK_BUTTON, FP_DISPLAY);
			stuck_button = 1;
		}
	}
}

/************************************************************************
 *																		*
 *	4/14/93	-	Moved the error logging for a power failure from this	*
 *				routine to the shutdown routine, to reduce the lag		*
 *				time from when a power failure is detected to when		*
 *				the PENs are de-asserted.								*
 *																		*
 ************************************************************************/

/* eXtended Interrupt ReQuest */
interrupt  void 
XIRQ_interrupt(void)
{
	unsigned char	xirq_reg;

	xirq_reg = (*L_XIRQ & 0x3F);

	/*check for Power, Failure Interrupts */
	if ((xirq_reg & XIRQ_PFW) && (pwr_dwn_state != PWR_D_TOTAL)) {
		shutdown(PWR_FAIL);
	}
}

/* SoftWare Interrupt */
interrupt  void 
SWI_interrupt(void)
{
	ptod->nv_status = RESET_SWI;
	/* jam the stack pointer to the initial value */
	_opc(0x8E); _opc(0x1F); _opc(0xFE);		/* LDS 0x1FFE */
	sys_reset();
}

/* Illegal Opcode Trap */
interrupt  void 
IOT_interrupt(void)
{
	ptod->nv_status = RESET_IOT;
	/* jam the stack pointer to the initial value */
	_opc(0x8E); _opc(0x1F); _opc(0xFE);		/* LDS 0x1FFE */
	sys_reset();
}

/* COP Failure */
interrupt  void 
NOCOP_interrupt(void)
{
	ptod->nv_status = RESET_COP;
	/* jam the stack pointer to the initial value */
	_opc(0x8E); _opc(0x1F); _opc(0xFE);		/* LDS 0x1FFE */
	sys_reset();
}

/* COP Monitor Failure */
interrupt  void 
CME_interrupt(void)
{
	ptod->nv_status = RESET_CME;
	/* jam the stack pointer to the initial value */
	_opc(0x8E); _opc(0x1F); _opc(0xFE);		/* LDS 0x1FFE */
	sys_reset();
}
