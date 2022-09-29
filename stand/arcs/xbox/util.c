//#pragma option v  
/* util.c */
/*
 * contains utility routines used to support the LEGO System controller
 */
//#include <16c74.h>h>
//#include <proto.h>

//extern struct TIMER TCB;			/* data structure containing task status */

/******************************************************************************
 *																										*
 *	cancel_timeout() -	Cancels active timeout pointed to by the input			*
 *						parameter.																	*
 *																										*
 ******************************************************************************/
void 
cancel_timeout(tcb_entry)
int tcb_entry;
{

	if (tcb_entry == DELAY_TO) {
		TCB.sw_delay = 0;
		TCB.to_status.DELAY_TO = 0;
	}
	else if (tcb_entry == FAN_TO) {
		TCB.fan_delay = 0;
		TCB.to_status.FAN_TO = 0;
	}
	else if (tcb_entry == LED_BLINK_TO) {
		TCB.led_blink = 0;
		TCB.to_status.LED_BLINK_TO = 0;
	}
	else if (tcb_entry == PWR_DOWN_TO) {
		TCB.led_blink = 0;
		TCB.to_status.PWR_DOWN_TO = 0;
	}
}



/***************************************************************************
 *																									*
 *	delay() -	Delay is a software timeout routine.  When called it will	*
 *				not return to the calling routine until the delay period   		*
 *				has expired.																	*
 *																									*
 *				Delay use the interrupt timer and the TCB for the time			*
 *				base.																				*
 *																									*
 ***************************************************************************/
void
delay()
{

	timeout(DELAY_TO);
	while (TCB.to_status.DELAY_TO == 0);
	/* reset the status before returning */
	TCB.to_status.DELAY_TO = 0;
}

/***************************************************************************
 *																									*
 *		delayUs8Mhz:		A software delay programed for an 8 MHZ crystal		*
 *																									*																							*
 *  Clock Freq. = 8MHz																		*
 *  Inst. Clock = 2MHz																		*
 *  Inst. dur.  = 500ns																		*
 *																									*
 *	 Input paramter of 1 (shortest delay) yields a delay of 5 usec.			*
 *  Each increment of the input paramter will result in an additional 		*
 *	 5 usec delay.																				*
 *																									*
 *																									*
 *		the minimum delay includes 1 T to load the delay parameter before		*
 *		calling this routine, 2T for the call instruction, 2T for the return	*
 *		and 1T to execute the next instruction after this delay routine		*
 *		The longest delay is 0xff * 5 usec = 1275 usec								*
 *																									*
 ***************************************************************************/
void delayUs8Mhz(registerw delay)
{
#asm
   MOVWF __WImage  ;1T
   GOTO	A_LOOP	 ;2T
B_LOOP
   NOP             ;1T
   NOP             ;1T
   NOP             ;1T
   NOP             ;1T
   NOP             ;1T
   NOP             ;1T
   NOP             ;1T
A_LOOP
   DECFSZ __WImage ;1T
   GOTO	B_LOOP	 ;2T
#endasm
}



/******************************************************************************
 *									     																*	
 *	extMemRd() -  Routine to read external memory.  This routine should only	*
 *					  be used by functions that are NOT called by interrupt 			*
 *					  routines.																		*
 *																										*
 *					  the global varible "ext_ptr_store" is loaded prior to 			*
 *					  entering the routine.														*
 *									     																*				
 ******************************************************************************/
char
extMemRd() {

	char	ext_data;
	long far *tbl_ptr;
	/*
	 * Disable Global interrupts before disabling the interrupt enable
	 * bits in the INTSTA register, per Microchip ERATA.
	 */

	CPUSTA.GLINTD = 1;

	/* Now disable the external and Peripheral interrupts. */
	INTSTA.INTIE = 0;
	INTSTA.PEIE = 0;

	/* The global interrupts can now be enabled. */
	CPUSTA.GLINTD = 0;

	/* Load the table pointer. */
	tbl_ptr = EXT_ADD;
	
	/* Read the external memory location */
	ext_data = *tbl_ptr;

	/* Enable the external and peripheral interrupts */
	INTSTA.INTIE = 1;
	INTSTA.PEIE = 1;

	return(ext_data);
}


/******************************************************************************
 *									     																*	
 *	extMemWt() -  Routine to write external memory.  This routine should only	*
 *					  be used by functions that are NOT called by interrupt 			*
 *					  routines.																		*
 *																										*
 *					  the global varible "ext_ptr" is loaded prior to entering		*
 *					  the routine.																	*
 *									     																*				
 ******************************************************************************/
void
extMemWt(ext_reg)
char	ext_reg;
{
	long		data_reg;
	long		far *tbl_ptr;


	/* disable Global interrupts per Microchip ERATA */

	CPUSTA.GLINTD = 1;

	/* now disable the external and Peripheral interrupts */
	INTSTA.INTIE = 0;
	INTSTA.PEIE = 0;

	/* the global interrupt can now be enabled */
	CPUSTA.GLINTD = 0;
	
	/* Load the table pointer with the default external address*/
	tbl_ptr = EXT_ADD;

	if (ext_reg == LED_REG) {
		/* set the bit for the PAL to tell the difference between the two registers */
		data_reg = 0;
		/* and load the data register */
		data_reg  = (long) (reg_state.reg_fail_leds);
	}

	else if (ext_reg == STATUS_REG) {
		/* set the bit for the PAL to tell the difference between the two registers */
		data_reg = 0x2000;
		/* and load the data register */
		data_reg  |= (long) (reg_state.reg_xbox_status);
	}
		
	else if (ext_reg == PEN_REG){
		/* set the bit for the PAL to tell the difference between the two registers */
		data_reg = 0x4000;
		/* and load the data register */
		data_reg  |= (long) (reg_state.reg_xbox_status);
	}
	/* read the external memory location */
	*tbl_ptr = data_reg;

	/* Enable the external and peripheral interrupts */
	INTSTA.INTIE = 1;
	INTSTA.PEIE = 1;
}

/***************************************************************************
 *																									*
 *	fan_fail() - Logs the failed fan, checks for single fan failures only. 	*
 *				 Multiple fan failure are checked as part of the Next_fan()		*
 *				 routine, and only after all invidual fans have been 				*
 *				 checked.																		*
 *																									*
 ***************************************************************************/
void
fan_fail()
{
	/* First check to see if the retry count has been exhausted */
	if (fan_ptr.fan_retry == FAN_MAX_RETRY) {

		fan_ptr.fan_retry = 0;
		
		/* check status bit and if not set, set it. */
		if (!(fan_ptr.fan_stat_1 & (1 << fan_ptr.fan_add))) 
			fan_ptr.fan_stat_1 |= (1 << fan_ptr.fan_add);
	}

	/* Try the same fan again */
	else {
		/* increment the retry count */
		fan_ptr.fan_retry++;
		
		/* decrement the address, next_fan() will increment the address, so by decrementing it
		 * the same fan will be selected for the retry.
		 */
		fan_ptr.fan_add--;
	}
	next_fan();
}

/***************************************************************************
 *																									*
 *	next_fan() -	The next fan to be monitored is addressed and the 			*
 *					input capture interrupts are enabled.								*
 *																									*
 *					After all fans are measured, over temperature conditions		*
 *					are checked, and if present the system is shutdown.			*
 *					Over temperature conditions are:										*
 *						Two or more fan failures.											*
 *						Failure of Fan 1, 2, or 3.											*
 *						A single fan failure and a high temperature condition		*
 *																									*
 ***************************************************************************/
void
next_fan()
{
	char update;

	tasks.env.ENV_FAN_RUNNING = 0;

	fan_ptr.fan_add++;

	reg_state.reg_rb &= 0xF3;
	if ((fan_ptr.fan_add)  < FANS_PER_MUX)
		reg_state.reg_rb |= (fan_ptr.fan_add << 2);
	PORTB = reg_state.reg_rb;
	
	fan_ptr.fan_stat_2.STAT2_PULSE_NO = 0;

	/* enable the pulse counting status bit */
	fan_ptr.fan_stat_2.STAT2_FAN_LRTR = 0;
	fan_ptr.fan_stat_2.STAT2_FAN_CHK = 1;

	/* cancel a previous fan timeout */
	cancel_timeout(FAN_TO);
	
	/* set a timeout for detection of a locked rotor */
	to_value = FAN_LCK_ROTOR_DELAY;
	timeout(FAN_TO);
	
	if (fan_ptr.fan_add < FANS_PER_MUX) {
		TMR3L = 0;
		TMR3H = 0;
		
		/* read the capture register to empty any stale values */
		temp = CA2L ;
		temp += CA2H;
		TCON2.CA2OVF = 0;

		/* clear timer3 overflow flag */
		PIR.TMR3IF = 0;

		/* now turn on the timer */
		TCON2.TMR3ON = 1;

		/* enable the pulse capturing */
		PIR.CA2IF = 0;
		PIE.CA2IE = 1;
	}
	
	/* reset the address to Zero and if any failure exists then
	 * check for multiple failures */
	else  {
		fan_ptr.fan_add = 0;

			/* post a message if a single fan has failed and it was detected for the
			 * first time through the loop
			 */

			update = 0;
			if (fan_ptr.fan_stat_1.STAT1_FAN_0 != reg_state.reg_xbox_status.X0_FAN_FAIL) {
				if (fan_ptr.fan_stat_1.STAT1_FAN_0) {
			 		/* set the status register */
			 		reg_state.reg_xbox_status.X0_FAN_FAIL = 1;
			 		/* set the fail LED */
			 		cntLED(X0_LED|FP_LED, LED_BLINK|LED_YELLOW);
				}
				else {
					/* no failure, reset the registers */
			 		reg_state.reg_xbox_status.X0_FAN_FAIL = 0;
					cntLED(X0_LED, LED_GREEN);
				}
				/* set the flag to update the external registers */
				update = 1;
			}
				
			
			if (fan_ptr.fan_stat_1.STAT1_FAN_1 != reg_state.reg_xbox_status.X1_FAN_FAIL) {
				if (fan_ptr.fan_stat_1.STAT1_FAN_1) {
			 		/* set the status register */
			 		reg_state.reg_xbox_status.X1_FAN_FAIL = 1;
			 		/* set the fail LED */
			 		cntLED(X1_LED|FP_LED, LED_BLINK|LED_YELLOW);
				}
				else {
					/* no failure, reset the registers */
			 		reg_state.reg_xbox_status.X1_FAN_FAIL = 0;
					cntLED(X1_LED, LED_GREEN);
				}
				update = 1;
			}

			if (fan_ptr.fan_stat_1.STAT1_FAN_2 != reg_state.reg_xbox_status.PCI_FAN_FAIL) {
				if (fan_ptr.fan_stat_1.STAT1_FAN_2) {
			 		/* set the status register */
			 		reg_state.reg_xbox_status.PCI_FAN_FAIL = 1;
			 		/* set the fail LED */
			 		cntLED(PCI_LED|FP_LED, LED_BLINK|LED_YELLOW);
				}
				else {
					/* no failure, reset the registers */
			 		reg_state.reg_xbox_status.PCI_FAN_FAIL = 0;
					cntLED(PCI_LED, LED_GREEN);
				}
				update = 1;
			}
			

		 	/* Check for a shutdown condition, Both X0 and X1 fan, or the PCI fan */
		 	if ((fan_ptr.fan_stat_1.STAT1_FAN_0 && fan_ptr.fan_stat_1.STAT1_FAN_1)
		 			|| fan_ptr.fan_stat_1.STAT1_FAN_2) {
		 		if(!shutdown_pending) {
			 		cntLED(FP_LED, LED_BLINK|LED_RED);
					tasks.fw_logical.FWL_SFT_PWR_DN = 1;
		 		}
		 	}

			if (update) {
				/* check to see if the fp led needs to be green */
				if (!(fan_ptr.fan_stat_1 & 0x07))
					cntLED(FP_LED, LED_GREEN);
					
		 		/*output the status register */
				extMemWt(STATUS_REG);
			}
		/* reset the fan address and capture control circuitry */
		TMR3L = 0;
		TMR3H = 0;
		
		/* read the capture register to empty any stale values */
		temp = CA2L ;
		temp += CA2H;
		TCON2.CA2OVF = 0;

		/* clear timer3 overflow flag */
		PIR.TMR3IF = 0;

		/* now turn on the timer */
		TCON2.TMR3ON = 1;

		/* enable the pulse capturing */
		PIR.CA2IF = 0;
		PIE.CA2IE = 1;
	}
}


/***************************************************************************
 *																									*
 *	cntLED		Routine controls the tri-color LEDs, in the on state			*
 *					The "led" input parameter identifies the led or group of		*
 *					leds to be turned on.  The "mode" input parameter 				*
 *					identifies the color and blink state of the led(s).			*
 *																									*
 *					This routine accepts blinking for every color possiblity,	*
 *					however only the yellow color will blink on the fan leds.	*
 *					the front panel led will blink all three colors, red, green	*
 *					and amber.  Blink is always between the color and off.		*
 *																									*
 ***************************************************************************/
void
cntLED(led, mode)
bits	led;
char	mode;
{
	
	/* set the color and the "BLINK" state for each LED*/
	if (led & FP_LED) {
		if (mode == LED_OFF) {
			reg_state.reg_fail_leds.FP_GREEN_BIT  = 1;
			reg_state.reg_fail_leds.FP_RED_BIT = 1;
		}
		else if (mode & LED_GREEN) {
			reg_state.reg_fail_leds.FP_GREEN_BIT  = 0;
			reg_state.reg_fail_leds.FP_RED_BIT = 1;
		}
		else if (mode & LED_YELLOW) {
			reg_state.reg_fail_leds.FP_GREEN_BIT  = 0;
			reg_state.reg_fail_leds.FP_RED_BIT = 0;
		}
		else if (mode & LED_RED) {
			reg_state.reg_fail_leds.FP_GREEN_BIT  = 1;
			reg_state.reg_fail_leds.FP_RED_BIT = 0;
		}
		fp_color = reg_state.reg_fail_leds & 0x03;		/* used for blinking */
		blink_state.FP_LED_BLINK = (mode & LED_BLINK) ? 1 : 0;
	}

	if (led & PCI_LED) {
		if (mode == LED_OFF) {
			reg_state.reg_fail_leds.PCI_GREEN_BIT  =1;
			reg_state.reg_fail_leds.PCI_YELLOW_BIT =1;
		}
		else if (mode & LED_GREEN) {
			reg_state.reg_fail_leds.PCI_GREEN_BIT  =0;
			reg_state.reg_fail_leds.PCI_YELLOW_BIT =1;
		}
		else if (mode & LED_YELLOW) {
			reg_state.reg_fail_leds.PCI_GREEN_BIT  =1;
			reg_state.reg_fail_leds.PCI_YELLOW_BIT =0;
		}
		blink_state.PCI_LED_BLINK = (mode & LED_BLINK) ? 1 : 0;
	}

	if (led & X0_LED) {
		if (mode == LED_OFF) {
			reg_state.reg_fail_leds.X0_GREEN_BIT  = 1;
			reg_state.reg_fail_leds.X0_YELLOW_BIT = 1;
		}
		else if (mode & LED_GREEN) {
			reg_state.reg_fail_leds.X0_GREEN_BIT  = 0;
			reg_state.reg_fail_leds.X0_YELLOW_BIT = 1;
		}
		else if (mode & LED_YELLOW) {
			reg_state.reg_fail_leds.X0_GREEN_BIT  = 1;
			reg_state.reg_fail_leds.X0_YELLOW_BIT = 0;
		}
		blink_state.X0_LED_BLINK = (mode & LED_BLINK) ? 1 : 0;
	}

	if (led & X1_LED) {
		if (mode == LED_OFF) {
			reg_state.reg_fail_leds.X1_GREEN_BIT  = 1;
			reg_state.reg_fail_leds.X1_YELLOW_BIT = 1;
		}
		else if (mode & LED_GREEN) {
			reg_state.reg_fail_leds.X1_GREEN_BIT  = 0;
			reg_state.reg_fail_leds.X1_YELLOW_BIT = 1;
		}
		else if (mode & LED_YELLOW) {
			reg_state.reg_fail_leds.X1_GREEN_BIT  = 1;
			reg_state.reg_fail_leds.X1_YELLOW_BIT = 0;
		}
		else if (mode & LED_RED) {
		}
		blink_state.X1_LED_BLINK = (mode & LED_BLINK) ? 1 : 0;
	}
	extMemWt(LED_REG);

	if(blink_state) {
		to_value = LED_BLINK_TIME;
		timeout(LED_BLINK_TO);
	}
	else
		cancel_timeout(LED_BLINK_TO);
}

/***************************************************************************
 *																									*
 *	tick() - 	Tick is called from the timer 2 interrupt handler, and is	*
 *				responsible for managments of the timeout TCB queue entries.	*
 *				Each time called tick will decrement any non zer entry of 		*
 *				TCB.  Once an entry is decremented to zero, tick set the 		*
 *				corresponding bit in the status byte									*
 *																									*
 *																									*
 ***************************************************************************/
void
tick()
{



	if (TCB.sw_delay) {
		TCB.sw_delay--;
		/* if time has expired set the status bit */
		if(TCB.sw_delay == 0)
			TCB.to_status.DELAY_TO = 1;
	}
	if (TCB.fan_delay) {
		TCB.fan_delay--;
		/* if time has expired set the status bit */
		if(TCB.fan_delay == 0) 
			TCB.to_status.FAN_TO = 1;
	}

	/* led blinking code */
	/* fan fail leds only blink between yellow and off, but the front panel led will blink between the
	 * three colors and off.
	 */
	if (TCB.led_blink) {
		TCB.led_blink--;
		/* if time has expired toggle the led */
		/* only the yellow led is put in the blink mode, green is always solid */
		if(TCB.led_blink == 0) {
			if (blink_state.FP_LED_BLINK) {
				if ((reg_state.reg_fail_leds & 0x03) == fp_color) {
					/* turn the led off */
					reg_state.reg_fail_leds |= 0x03;
				}
				else {
					/* restore the color */
					reg_state.reg_fail_leds &= 0xFC;
					reg_state.reg_fail_leds |= fp_color;
				}
			}
			if (blink_state.PCI_LED_BLINK) {
				reg_state.reg_fail_leds.PCI_GREEN_BIT = 1;
				reg_state.reg_fail_leds.PCI_YELLOW_BIT   = reg_state.reg_fail_leds.PCI_YELLOW_BIT ? 0 : 1;
			}

			if (blink_state.X0_LED_BLINK) {
				reg_state.reg_fail_leds.X0_GREEN_BIT = 1;
				reg_state.reg_fail_leds.X0_YELLOW_BIT   = reg_state.reg_fail_leds.X0_YELLOW_BIT ? 0 : 1;
			}

			if (blink_state.X1_LED_BLINK) {
				reg_state.reg_fail_leds.X1_GREEN_BIT = 1;
				reg_state.reg_fail_leds.X1_YELLOW_BIT   = reg_state.reg_fail_leds.X1_YELLOW_BIT ? 0 : 1;
			}


			/* Output the new values */
			extMemWt(LED_REG);

			to_value = LED_BLINK_TIME;
			timeout(LED_BLINK_TO);
		}

	}
	if (TCB.pwr_down) {
		TCB.pwr_down--;
		/* if time has expired set the status bit */
		if(TCB.pwr_down == 0) 
			TCB.to_status.PWR_DOWN_TO = 1;
	}

}


/*****************************************************************************
 *																										*
 *	timeout() -	Time out loads delay values into the appropriate TCB entry.		*
 *				Maximum values accepted are 0xFFFF, equating to a 10.9 hour			*
 *				timeout period.																	*
 *																										*
 *				The status bit is clear each time a new value is loaded.				*
 *																										*
 ****************************************************************************/
void 
timeout(tcb_entry)
char tcb_entry;
{


	if (tcb_entry == DELAY_TO) {
		TCB.sw_delay = to_value;
		TCB.to_status.DELAY_TO = 0;
	}
	else if (tcb_entry == FAN_TO) {
		TCB.fan_delay = to_value;
		TCB.to_status.FAN_TO = 0;
	}
	else if (tcb_entry == LED_BLINK_TO) {
		TCB.led_blink = to_value;
		TCB.to_status.LED_BLINK_TO = 0;
	}
	else if (tcb_entry == PWR_DOWN_TO) {
		TCB.pwr_down = to_value;
		TCB.to_status.PWR_DOWN_TO = 0;
	}
}

