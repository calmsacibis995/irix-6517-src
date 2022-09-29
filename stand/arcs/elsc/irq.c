
//#pragma option v  
/* irq.c */
/*
 * contains the interrupt handler.
 */
//#include <16c74.h>
//#include <elsc.h>
//#include <proto.h>

/* These global statements will be removed as soon as I can figure
 * out how to handle global varibles between modules.
 */
/* Global Structure Declarations */
//extern struct MONITOR tasks;		/* data structure containing task status */
//extern struct SYS_STATE reg_state;	/* data struct containing copies of reg. */
//extern struct TIMER TCB;			/* data structure containing task status */

/******************************************************************************
 *																										*
 *	INT() -	INT is the external interrupt handler.  INT handles all 				*
 *				interrupts that are connected to the RA0/INT pin.  In our case		*
 *				only the I2C UART is connected to an external interrupt.				*
 *																										*
 ******************************************************************************/
void
__INT()
{
	SaveContext;
	/* save context of interrupt state register */
	int_state_reg = int_state;

	i2c_slave();

	/* restore context of interrupt state register */
	int_state = int_state_reg;
	RestoreContext;
}

/******************************************************************************
 *																										*
 *	TMR0() -	is an interrupt handler for Timer 0 overflows.  No interrupts		*
 *				should be generated because timer 0 is unused.							*
 *																										*
 ******************************************************************************/
void
__TMR0()
{
	SaveContext;
	/* save context of interrupt state register */
	int_state_reg = int_state;

	/* restore context of interrupt state register */
	int_state = int_state_reg;
	RestoreContext;	
}

/******************************************************************************
 *																										*
 *	RTpin() -is an T0CK1 Interrupt handler.  This function is unused for this	*
 *				design and should not generate an interrupt.								*
 *																										*
 ******************************************************************************/

void
__RTpin()
{

	SaveContext;
	/* save context of interrupt state register */
	int_state_reg = int_state;

	/* restore context of interrupt state register */
	int_state = int_state_reg;
	RestoreContext;
}

/***************************************************************************
 *																									*
 *	PIV() -	PIV is the general peripheral interrupt handler and the			*
 *				main interrupt handler for the ELSC.									*
 *																									*
 *				Mulitple reporting of the same interrupt is avoided by			*
 *				registering the value of the input register after each			*
 *				valid interrupt.  A new interrupt is only reported when			*
 *				the input bit is a different state then the registered			*
 *				copy.																				*
 *																									*
 ***************************************************************************/

void
__PIV()
{
	bits 	int_reg;
	int	i;			/* loop counter */

	SaveContext;
	/* save context of interrupt state register */
	int_state_reg = int_state;
	
	/* 
	 * Fan Rotation tachometer check 
	 */
	 
#ifdef FAN_CHK
	if (PIR.CA1IF && PIE.CA1IE) {
		tasks.env2.ENV2_FAN_RUNNING = 1;
		if (fan_ptr.fan_stat_2.STAT2_PULSE_NO == 0) {
				
			fan_ptr.fan_ov = 0;
			/* only the high reg is used, however both must be read to enable the next update*/
			fan_ptr.fan_pulse_0 = (unsigned long )CA1H ;
			i = CA1L ;
			
			/* if and capture overflow is set, reset the pulse number flag and start over */
			if (TCON2.CA1OVF)
				TCON2.CA1OVF = 0;
			else
				fan_ptr.fan_stat_2.STAT2_PULSE_NO = 1;
		}
		else {
			fan_ptr.fan_stat_2.STAT2_PULSE_NO = 0;
			fan_ptr.fan_pulse_1 = (unsigned long )CA1H;
			/* check to see if an overflow occured right before the capture */
			if (PIR.TMR3IF && (!(CA1H & 0x80)))
				fan_ptr.fan_ov++;	
			fan_ptr.fan_pulse_1 |= (unsigned long )(fan_ptr.fan_ov << 8);
			i = CA1L;

			/* if no caputre overflow record the period, otherwise start again */
			if (!TCON2.CA1OVF) {
				fan_ptr.fan_period = fan_ptr.fan_pulse_1 - fan_ptr.fan_pulse_0;
				/* monitor funciton will re-enable interrupts */
				PIE.CA1IE = 0;
				fan_ptr.fan_stat_2.STAT2_CNT_CHK = 1;
				TCON2.TMR3ON = 0;
			}
			else
				TCON2.CA1OVF = 0;
				
		}
		PIR.CA1IF = 0;
	}

	if (PIR.CA2IF && PIE.CA2IE) {
		tasks.env2.ENV2_FAN_RUNNING = 1;
		if (fan_ptr.fan_stat_2.STAT2_PULSE_NO == 0) {
			
			fan_ptr.fan_ov = 0;
			/* only the high reg is used, however both must be read to enable the next update*/
			fan_ptr.fan_pulse_0 = (unsigned long )CA2H;
			i = CA2L;
			
			/* if and capture overflow is set, reset the pulse number flag and start over */
			if (TCON2.CA2OVF)
				TCON2.CA2OVF = 0;
			else
				fan_ptr.fan_stat_2.STAT2_PULSE_NO = 1;
		}
		else {
			fan_ptr.fan_stat_2.STAT2_PULSE_NO = 0;
			fan_ptr.fan_pulse_1 = (unsigned long )CA2H;
			/* check to see if an overflow occured right before the capture */
			if (PIR.TMR3IF && (!(CA2H & 0x80)))
				fan_ptr.fan_ov++;	
			fan_ptr.fan_pulse_1 |= (unsigned long )(fan_ptr.fan_ov << 8);
			i = CA2L;

			if (!TCON2.CA2OVF) {
				fan_ptr.fan_period = fan_ptr.fan_pulse_1 - fan_ptr.fan_pulse_0;
				/* monitor funciton will re-enable interrupts */
				PIE.CA2IE = 0;
				fan_ptr.fan_stat_2.STAT2_CNT_CHK = 1;
				TCON2.TMR3ON = 0;
				/* adjust fan multiplexer now to allow lines to settle while the fan
				 * measurements are evaluated.  In the case of a timeout, the address
			 	* lines are adjusted in tasks_fan()
				 */
			}
		}
		PIR.CA2IF = 0;
	}

	/* fan pulse counter overflow register */
 	if (PIR.TMR3IF && PIE.TMR3IE) {
		fan_ptr.fan_ov++;
		PIR.TMR3IF = 0;
	}

	
#endif
	
#ifndef	NO_PORTB_INT
	/* Process a PORTB generated interrupt */
	if (PIR.RBIF && PIE.RBIE) {
		int_reg = PORTB;
#ifdef SPEEDO
		/* make sure the proper fan address is written back to PORTB.  The fan_add
			could be off by one depending on if the interrupt occurs during the
			next_fan function between the point the fan_add is incremented to 3, and
			the point the address is reset to zero */
		if (fan_ptr.fan_add == 3)
			i = 0;
		else
			i = fan_ptr.fan_add << 2;
		PORTB = (int_reg & 0xE3) | i;
#else
		PORTB = int_reg;
#endif

#ifndef	SPEEDO

#ifdef	_005
#ifdef POK_CHK
	/* POK Failure */
	if (!PORTB.POKB && reg_state.reg_rb.POKB && !reg_state.reg_out3.PS_EN) 
		tasks.env1.ENV1_POK = 1;
#endif

	/* key switch in a new position */
	if ((PORTB ^ reg_state.reg_rb) & KEY_SW_OFF_MASK) {
		if (!tasks.switches.FPS_KEY_BOUNCE) {
			tasks.switches.FPS_KEY_BOUNCE = 1;
			to_value = ONE_SEC;
			timeout(KEY_TO);
		}
	}

#else
		/* Power Supply Failure */
		if (((int_reg ^ reg_state.reg_rb) & PS_OK_MASK) && !int_reg.PS_OK
			&& !reg_state.reg_out3.PS_EN)
			tasks.env1.ENV1_PS_FAIL = 1;
			
		/* Power Failure Warning */
		if (((int_reg ^ reg_state.reg_rb) & PWR_FAIL_MASK) &&
			!int_reg.PWR_FAIL && tasks.status.STAT_SYSPWR)
			tasks.env1.ENV1_PF = 1;
#endif

#endif	//SPEEDO
		/* System High Temperature */
		if (((int_reg ^ reg_state.reg_rb) & TMP_FANS_HI_MASK) && int_reg.TMP_FANS_HI)
			tasks.env1.ENV1_SYS_HT = 1;
			
		/* Temperature Normal */
		else if (!int_reg.TMP_FANS_HI && reg_state.reg_rb.TMP_FANS_HI) 
			tasks.env1.ENV1_TEMP_OK = 1;
	
		/* Reset Button Pushed ? */
		if (((int_reg ^ reg_state.reg_rb) & RST_MASK) && !tasks.switches.FPS_RST_BOUNCE){
			/* change of state is a closed switch */
			if (!int_reg.RESET_SW ) 
				tasks.switches.FPS_RESET = 1;
			tasks.switches.FPS_RST_BOUNCE = 1;
			to_value = MSEC_250;
			timeout(RESET_TO); 
		}

		/* NMI Button Pushed ? */
		if (((int_reg ^ reg_state.reg_rb) & NMI_MASK) && !tasks.switches.FPS_NMI_BOUNCE){
			/* change of state is a closed switch */
			if (!int_reg.NMI_SW) 
				tasks.switches.FPS_NMI = 1;
			tasks.switches.FPS_NMI_BOUNCE = 1;
			to_value = MSEC_250;
			timeout(NMI_TO); 
		}

		PIR.RBIF = 0;
		reg_state.reg_rb = int_reg;
	}

#endif	//NO_PORTB_INT
	
  /*
   * Check the for a timer interrupt
   */
 	if (PIR.TMR1IF && PIE.TMR1IE) {
		tick();
		PIR.TMR1IF = 0;
	}


  /* 
   * Check for a RS-232 receiver data register full interrupt
   */

	while (PIR.RCIF && PIE.RCIE) {
		ch = RCREG;

		if (! cmdp.cmd_intr.PASS_THRU) {
			/* Process command mode input */

			/* if this is the second Control-T then erase the first one (if in echo mode)
			 *   and transmit one Control-T
			 */
			if ((ch == CNTL_T) && cmdp.cmd_stat.CNTLT_CHAR) {
				/* go back to pass through mode */
				cmdp.cmd_intr.PASS_THRU = 1;
				/* reset the control-T character counter */
				cmdp.cmd_stat.CNTLT_CHAR = 0;
				addq(I2C_OUT, CNTL_T);
				if (cmdp.cmd_stat.CH_ECHO) {
					for (i = 0; i < 4; i++) {
						addq(SCI_OUT, BS);
						addq(SCI_OUT, BS);
						addq(SCI_OUT, ' ');
					}
					addq(SCI_OUT, BS);
					PIE.TXIE = 1;
				}
			
			}

			else {

				if (ch == NULL || ch == NL || ch == CR) {
					/* increment the cmd count */
					cmdp.cmd_intr.SCI_CMD_REQ = 1;
					cmdp.cmd_intr.PASS_THRU = 1;
				}

				/* support backspace or delete */
				if (ch == BS || ch == DEL) {
					if (sci_inq.c_len > 0) {
						if (sci_inq.c_inp == sci_inq.c_start)
							sci_inq.c_inp = sci_inq.c_end;
						sci_inq.c_inp--;
						sci_inq.c_len--;
						if (cmdp.cmd_stat.CH_ECHO) {
							addq(SCI_OUT, BS);
							addq(SCI_OUT, ' ');
							addq(SCI_OUT, BS);
							PIE.TXIE = 1;
						}
					}
				}
				else {
					if (cmdp.cmd_stat.CH_ECHO) {
						addq(SCI_OUT, ch);		/* Echo character */
						if (ch == CR)
							addq(SCI_OUT, NL);
						PIE.TXIE = 1;
					}
					// to be processed by ELSC
					addq(SCI_IN, ch);
				}
					
#ifdef MODEM					
				if(sci_inq.c_len >= sci_inq.c_maxlen) {
#ifdef	SPEEDO
#ifdef RTS_CORRECTION
					reg_state.reg_out1.RTS = 1;
#else
					reg_state.reg_out1.RTS = 0;
#endif
					ext_ptr = OUTP1;	
					*ext_ptr = reg_state.reg_out1;
#else
					
#ifdef RTS_CORRECTION
					reg_state.reg_out3.RTS = 1;
#else
					reg_state.reg_out3.RTS = 0;
#endif
					ext_ptr = OUTP3;	
					*ext_ptr = reg_state.reg_out3;
#endif	//SPEEDO
				}
#endif
			}
		}

		else {
			/*
			 * Process pass-through mode input
			 *
			 * check for Control-T to enter command mode.  Control-T
			 * flushes the output queue and discards ACP output
			 * until the command is complete (necessary for FFSC to
			 * make use of Control-T). The ACP response is also
			 * preceded by a Control-T so it can be disambiguated by
			 * the FFSC.
			 */

			if (ch == CNTL_T) {
				cmdp.cmd_intr.PASS_THRU = 0;
				cmdp.cmd_stat.CNTLT_CHAR = 1;
				if (cmdp.cmd_stat.CH_ECHO) {
					flush(SCI_OUT);
					addq(SCI_OUT, '\r');
					addq(SCI_OUT, '\n');
					addq(SCI_OUT, 'M');
					addq(SCI_OUT, 'S');
					addq(SCI_OUT, 'C');
					addq(SCI_OUT, '>');
					addq(SCI_OUT, ' ');
					PIE.TXIE = 1;
#ifdef SPEEDO
					if (tasks.env2.ENV2_SPEEDO_SLAVE) {
						addq(SCI_OUT, 'S');
						addq(SCI_OUT, 'l');
						addq(SCI_OUT, 'a');
						addq(SCI_OUT, 'v');
						addq(SCI_OUT, 'e');
						addq(SCI_OUT, '!');
						addq(SCI_OUT, '\r');
						addq(SCI_OUT, '\n');
						cmdp.cmd_intr.PASS_THRU = 1;
					}
#endif
				}
			}
			else {
				addq(I2C_OUT, ch);
				cmdp.cmd_stat.CNTLT_CHAR = 0;

#ifdef MODEM					
				if (i2c_outq.c_len >= sci_inq.c_maxlen) {
#ifdef	SPEEDO

#ifdef RTS_CORRECTION
					reg_state.reg_out1.RTS = 1;
#else
					reg_state.reg_out1.RTS = 0;
#endif
					ext_ptr = OUTP1;	
					*ext_ptr = reg_state.reg_out1;
#else
#ifdef RTS_CORRECTION
					reg_state.reg_out3.RTS = 1;
#else
					reg_state.reg_out3.RTS = 0;
#endif
					ext_ptr = OUTP3;	
					*ext_ptr = reg_state.reg_out3;
#endif	//SPEEDO
				}
#endif
			}
		}
		/* post any errors to the front panel display */
		if (RCSTA.FERR || RCSTA.OERR) {
			/* clear the error */
			RCSTA.CREN = 0;
			RCSTA.CREN = 1;
		}
	}

	/* 
	 * Check for a RS-232 transmitter data register empty interrupt
	 */

	if (PIR.TXIF && PIE.TXIE) {
		/* 
		 * if a character is in the queue fetch, and load it into the
		 * tx register.
		 * 
		 * Before sending a character the status of CTS and DCD is checked.  If either
		 *	signal is low the character will not be sent.  If low, the interrupt will 
		 *	be cleared.  The monitor routine will then check for a high CTS and DCD 
		 * condition and if found will enable the Xmit interrupt.
		 */
#ifdef MODEM

		ext_ptr = INPT2;	
		int_reg = *ext_ptr;

		/* check for a flow control SUSPEND condition */
		if (int_reg.CTS)
			PIE.TXIE = 0;

		/* with no SUSPEND present, check for a character ready to transmit */
		else 	if (sci_outq.c_len) {
			ch = remq(SCI_OUT);
			TXREG = ch;
		}

		/* if no charater present, then disable tx interrupt */
		else 
			PIE.TXIE = 0;

#else
		if (sci_outq.c_len) {
			ch = remq(SCI_OUT);
			TXREG = ch;
		}
		else 
			/* if no charater present, then disable tx interrupt */
			PIE.TXIE = 0;
#endif

	}

	tasks.status.STAT_INTERRUPT = 0;
	/* restore context of interrupt state register */
	int_state = int_state_reg;
	RestoreContext;
}

