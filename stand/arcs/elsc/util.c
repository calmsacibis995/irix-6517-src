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

#ifdef	SPEEDO
	if (tcb_entry == KEY_TO) {
		TCB.key_switch = 0;
		TCB.to_status.KEY_TO = 0;
	}
	else if (tcb_entry == NMI_TO) {
		TCB.nmi_button = 0;
		TCB.to_status.NMI_TO = 0;
	}
	else if (tcb_entry == RESET_TO) {
		TCB.reset_button = 0;
		TCB.to_status.RESET_TO = 0;
	}
	else if (tcb_entry == DELAY_TO) {
		TCB.sw_delay = 0;
		TCB.to_status.DELAY_TO = 0;
	}
	else if (tcb_entry == FAN_TO) {
		TCB.fan_delay = 0;
		TCB.to_status.FAN_TO = 0;
	}
	else if (tcb_entry == POWER_DN_TO) {
		TCB.power_dn_delay = 0;
		TCB.to_status.POWER_DN_TO = 0;
	}
	else if (tcb_entry == NMI_EX_TO) {
		TCB.nmi_execute = 0;
		TCB.to_status.NMI_EX_TO = 0;
	}
	else if (tcb_entry == RST_EX_TO) {
		TCB.rst_execute = 0;
		TCB.to_status.RST_EX_TO = 0;
	}
	else if (tcb_entry == HBT_TO) {
		TCB.hbt_delay = 0;
		TCB.to_status_1.HBT_INT = 0;
	}

	else if (tcb_entry == LED_BLINK_TO) {
		TCB.led_blink = 0;
		TCB.to_status_1.LED_BLINK = 0;
	}
	else if (tcb_entry == ELSC_INT_TO) {
		TCB.elsc_int_delay = 0;
		TCB.to_status_1.ELSC_INT_TD = 0;
	}

	
#else
	if (tcb_entry == KEY_TO) {
		TCB.key_switch = 0;
		TCB.to_status.KEY_TO = 0;
	}
	else if (tcb_entry == NMI_TO) {
		TCB.nmi_button = 0;
		TCB.to_status.NMI_TO = 0;
	}
	else if (tcb_entry == RESET_TO) {
		TCB.reset_button = 0;
		TCB.to_status.RESET_TO = 0;
	}
	else if (tcb_entry == DELAY_TO) {
		TCB.sw_delay = 0;
		TCB.to_status.DELAY_TO = 0;
	}
	else if (tcb_entry == FAN_TO) {
		TCB.fan_delay = 0;
		TCB.to_status.FAN_TO = 0;
	}
	else if (tcb_entry == POWER_DN_TO) {
		TCB.power_dn_delay = 0;
		TCB.to_status.POWER_DN_TO = 0;
	}
	else if (tcb_entry == NMI_EX_TO) {
		TCB.nmi_execute = 0;
		TCB.to_status.NMI_EX_TO = 0;
	}
	else if (tcb_entry == RST_EX_TO) {
		TCB.rst_execute = 0;
		TCB.to_status.RST_EX_TO = 0;
	}
	else if (tcb_entry == HBT_TO) {
		TCB.hbt_delay = 0;
		TCB.to_status_1.HBT_INT = 0;
	}

	else if (tcb_entry == ELSC_INT_TO) {
		TCB.elsc_int_delay = 0;
		TCB.to_status_1.ELSC_INT_TD = 0;
	}

	else if (tcb_entry == POWER_UP_TO) {
		TCB.power_up_delay = 0;
		TCB.to_status_1.PWR_UP_TD = 0;
	}

#endif

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

#ifndef	SPEEDO

/******************************************************************************
 *									     																*	
 *	dspControl() -	LED display device driver for Control word setup.	  			*
 *																										*
 *					This routine is used to write to the display control word.		*
 *					The routine is designed around the Seimens PLCD5583 				*
 *					intelligent display device.												*																			*
 *																										*
 *					Input parameter is a control charater to be written to the 		*
 *					device																			*
 *																										*
 ******************************************************************************/
void
dspControl(cword)
char	cword;				/* control word to be written */
{
	CPUSTA.GLINTD = 1;
	ext_ptr = DSP_ADD;
	*ext_ptr = cword;
	CPUSTA.GLINTD = 0;

}

/******************************************************************************
 *									     																*	
 *	dspChar() -	LED display device driver.										  			*
 *																										*
 *					Displays a single character on the display.
 *																										*
 *					Input parameters are the character number (starting with zero)	*
 *					and the value to be displayed												*
 *																										*
 ******************************************************************************/
void
dspChar(ch_num, ch_val)
char	ch_num;			/* Character number to be written to */
char 	ch_val;			/* Value to be displayed */
{
	CPUSTA.GLINTD = 1;
	ext_ptr = (DSP_ADD | CH_0) + ch_num ;
	*ext_ptr = ch_val;
	CPUSTA.GLINTD = 0;

}

/******************************************************************************
 *									     																*	
 *	dspMsg() -	LED display device driver.										  			*
 *																										*
 *					Displays 8 ASCII characters on the 1 x 8 display. Driver			*
 *					is designed around the Seimens PLCD5583 intelligent display		*
 *					device.																			*
 *																										*
 *					Input parameter is a pointer to a null terminated string			*
 *																										*
 *					The message is stored in program space where every location is	*
 *					16 bits long instead of 8.  Therefore, the routing reads two	*
 *					characters for every program memory address location.				*
 *																										*
 *					In the event a FFSC is attached, the message is also sent		*
 *					out the serial port.  A ninth (tag) character sent after the 	*
 *					8 message text characters.  This character informs the FFSC		*
 *					about the severity of the message 										*
 *																										*
 ******************************************************************************/
void
dspMsg(cptr)
char 	* cptr;			/* pointer to null terminated string */
{
	char	i;					/* loop counter */
	char ch;

	CPUSTA.GLINTD = 1;
	ext_ptr = DSP_ADD | CH_0;
	str_ptr = cptr & 0x7FFF;

	if (tasks.fw_logical.FWL_FFSC_PRESENT) 
		load_cnt_t_msg();
	
	for (i = 0; i < 8;i += 2, str_ptr += 1) {

		/* Read the long from program space */
		l_ch = *str_ptr;

		/* display the first byte if non zero */
		*ext_ptr = (char ) l_ch;
		ext_ptr += 1;
		if (tasks.fw_logical.FWL_FFSC_PRESENT) 
			addq(SCI_OUT, (char ) l_ch);

		/* display the second byte if non zero */
		ch = (char ) (l_ch >> 8);
		*ext_ptr = ch;
		ext_ptr +=1;
		if (tasks.fw_logical.FWL_FFSC_PRESENT) 
			addq(SCI_OUT, ch);

	}
	if (tasks.fw_logical.FWL_FFSC_PRESENT) {
		l_ch = *str_ptr;
		addq(SCI_OUT, (char ) l_ch);
		load_CR_NL();
	}
		
	CPUSTA.GLINTD = 0;

}

/******************************************************************************
 *									     																*	
 *	dspRamMsg() -	LED display device driver.									  			*
 *																										*
 *					Displays upto 8 ASCII characters on the 1 x 8 display. Driver	*
 *					is designed around the Seimens PLCD5583 intelligent display		*
 *					device.																			*
 *																										*
 *					The source of the information to be displayed comes from  RAM	*
 *					and not ROM like the 'dspMsg' and therefore does not generate	*
 *					table reads.																	*
 *																										*
 *					Input parameter is a pointer to a null terminated string			*
 *																										*
 ******************************************************************************/
void
dspRamMsg(sptr)
char 	near * sptr;		/* pointer to string */
{
	char	i;					/* loop counter */
	char ch;

	CPUSTA.GLINTD = 1;
	ext_ptr = DSP_ADD | CH_0;

	if (tasks.fw_logical.FWL_FFSC_PRESENT) 
		load_cnt_t_msg();


	for (i = 0; i < 8; i++, ext_ptr += 1) {
		if (sptr[i] == 0)
			break;
		*ext_ptr = sptr[i];
		if (tasks.fw_logical.FWL_FFSC_PRESENT) {
			ch = sptr[i];
			addq(SCI_OUT, ch);
		}
		
	}

	if (tasks.fw_logical.FWL_FFSC_PRESENT) {
		/* Send the tag character */
		addq(SCI_OUT, '4');
		load_CR_NL();
	}
	CPUSTA.GLINTD = 0;

}

/******************************************************************************
 *									     																*	
 *	load_cnt_t_msg -  loads a "^tmsg" stirng in the SCI output queue				*
 *					  be used by functions that are NOT called by interrupt 			*
 *									     																*				
 ******************************************************************************/
void
load_cnt_t_msg()
{

	addq(SCI_OUT, CNTL_T);
	addq(SCI_OUT, 'd');
	addq(SCI_OUT, 's');
	addq(SCI_OUT, 'p');
	addq(SCI_OUT, ' ');
}
#endif

/******************************************************************************
 *									     																*	
 *	load_CR_NL -  loads a "\r\n" stirng in the SCI output queue.  Also enables	*
 *					  tranmitter interrupt.														*
 *									     																*				
 ******************************************************************************/
void
load_CR_NL()
{
	addq(SCI_OUT, '\r');
	addq(SCI_OUT, '\n');
	PIE.TXIE = 1;
}

/******************************************************************************
 *									     																*	
 *	extMemRd() -  Routine to read external memory.  This routine should only	*
 *					  be used by functions that are NOT called by interrupt 			*
 *					  routines.																		*
 *																										*
 *					  the global varible "ext_ptr_store" is loaded prior to 			*
 *					  entering the routine.														*																*
 *									     																*				
 ******************************************************************************/
char
extMemRd() {

	char	ext_data;
	
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
	ext_ptr = ext_ptr_store;
	
	/* Read the external memory location */
	ext_data = *ext_ptr;

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
extMemWt(ext_data)
char	ext_data;
{

	/* disable Global interrupts per Microchip ERATA */

	CPUSTA.GLINTD = 1;

	/* now disable the external and Peripheral interrupts */
	INTSTA.INTIE = 0;
	INTSTA.PEIE = 0;

	/* the global interrupt can now be enabled */
	CPUSTA.GLINTD = 0;
	
	/* Load the table pointer. */
	ext_ptr = ext_ptr_store;

	/* read the external memory location */
	*ext_ptr = ext_data;

	/* Enable the external and peripheral interrupts */
	INTSTA.INTIE = 1;
	INTSTA.PEIE = 1;
}

#ifndef	SPEEDO
#ifdef POK_CHK
/******************************************************************************
 *									     																*	
 *	getPOKBId() -	Routine Identifies the board causing a POK failure,  			*
 *					if possible.  A generic message is displayed if	     				*
 *					he board can not be identified		             					*
 *																										*
 *					Global Variable Usage:														*
 *						temp:	holds read value of I/O expander.							*
 *						temp1:  Dynamic message value starting with the					*
 *								message for bit 0 of the I/0 expander and					*
 *								adding the bit number for the message 						*
 *								number to be displayed.											*
 *						temp2:  A flag indicating a board has been 						*
 *								identified.															*
 *						count:	bit counter for "for" loop									*
 *									     																*				
 ******************************************************************************/
void
getPOKBId(void)
{

	int	pok_reg;
	int	count;

#ifdef	POK_SLOT

	pok_reg = 0;
	
	/* First Mid-Plane I/O Expander */
	i2c_cp.i2c_add = MP_40 | I2C_READ;
	i2c_cp.i2c_byte_count = 1;
	i2c_cp.i2c_xfer_ptr = &pok_reg;
	if (i2cAccess() < 0)

	~pok_reg;
	
	for (count = 0; count < 6; count++) {
		switch (pok_reg & (1 << count)) {
			case	0x01:
				dspMsg(MSG_POK_N0);
				break;
			case	0x02:
				dspMsg(MSG_POK_N1);
				break;
			case	0x04:
				dspMsg(MSG_POK_N2);
				break;
			case	0x08:
				dspMsg(MSG_POK_N3);
				break;
			case	0x10:
				dspMsg(MSG_POK_R0);
				break;
			case	0x20:
				dspMsg(MSG_POK_R1);
				break;
			default:
				dspMsg(MSG_POK);
				break;
		}
	}
#else	//POK_SLOT
		dspMsg(MSG_POK);
#endif
}
#endif
#endif

#ifdef FAN_CHK
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
		if (fan_ptr.fan_add < FANS_PER_MUX) {
			if (!(fan_ptr.fan_stat_1 & (1 << fan_ptr.fan_add))) 
				fan_ptr.fan_stat_1 |= (1 << fan_ptr.fan_add);
		}
		else if (!fan_ptr.fan_stat_2.STAT2_FAN_9) 
			fan_ptr.fan_stat_2.STAT2_FAN_9 = 1;
	}

	/* Try the same fan again */
	else {
		/* increment the retry count */
		fan_ptr.fan_retry++;
		
		/* decrement the address, next_fan() will increment the address, so by decrementing it
		 * the same fan will be selected for the retry.
		 */
		if (mp_type != MP_KCAR)
			fan_ptr.fan_add--;
	}
	next_fan();
}

#endif


#ifdef FAN_CHK
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
	int count;
	int loop;

	tasks.env2.ENV2_FAN_RUNNING = 0;

	if (mp_type != MP_KCAR)
		fan_ptr.fan_add++;
	else
		fan_ptr.fan_add = 0;

#ifdef	SPEEDO
	reg_state.reg_rb &= 0xE3;
	if ((fan_ptr.fan_add)  < FANS_PER_MUX)
		reg_state.reg_rb |= (fan_ptr.fan_add << 2);
	PORTB = reg_state.reg_rb;
#else
	reg_state.reg_out3 &= 0xF8;
	if ((fan_ptr.fan_add)  < FANS_PER_MUX)
		reg_state.reg_out3 |= fan_ptr.fan_add;
		
	ext_ptr_store = OUTP3;
	extMemWt(reg_state.reg_out3);
#endif	//SPEEDO
	
	fan_ptr.fan_stat_2.STAT2_PULSE_NO = 0;

	/* enable the pulse counting status bit */
	fan_ptr.fan_stat_2.STAT2_FAN_LRTR = 0;
	fan_ptr.fan_stat_2.STAT2_FAN_CHK = 1;

	/* cancel a previous fan timeout */
	cancel_timeout(FAN_TO);
	/* set a timeout for detection of a locked rotor */
	to_value = FAN_LCK_ROTOR_DELAY;
	timeout(FAN_TO);
	
	if ((fan_ptr.fan_add < FANS_PER_MUX) && (mp_type != MP_KCAR)) {
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
	
	/* or set up the capture of fan 8 */
	else if (fan_ptr.fan_add == 0x08) {
		/* enable capture of fan 8 */
		TMR3L = 0;
		TMR3H = 0;
		
		/* read the capture register to empty any stale values */
		temp = CA1L ;
		temp += CA1H;
		TCON2.CA1OVF = 0;
		
		/* clear timer3 overflow flag */
		PIR.TMR3IF = 0;

		/* now turn on the timer */
		TCON2.TMR3ON = 1;

		/* enable the pulse capturing */
		PIR.CA1IF = 0;
		PIE.CA1IE = 1;
	}
	/* reset the address to Zero and if any failure exists then
	 * check for multiple failures */
	else  {
		fan_ptr.fan_add = 0;
		if (fan_ptr.fan_stat_1 || fan_ptr.fan_stat_2.0) {
			if (!fan_ptr.fan_stat_2.STAT2_FAN_HI) {
				/* 
				 * set fans on Hi Speed, but only after all 9 have been 
				 * and only if it has not been set before checked 
				 */

				/* for KCAR reset the status bit, it will set next time if at high
				 * speed the blower does not run at the lower RPM level
				 */
				if (mp_type == MP_KCAR)
					fan_ptr.fan_stat_1.STAT1_FAN_0 = 0;
					
				fan_ptr.fan_stat_2.STAT2_FAN_HI = 1;

				/* Set the fans to high and disable the checking for 10 sec */
				tasks.fw_logical.FWL_FAN_SPEED = 1;
			}

			/* first check the ninth fan */
			if (fan_ptr.fan_stat_2.0) 
				count = 1;
			else
				count = 0;

			for (loop = 0; loop < 8; loop++) {
				if (fan_ptr.fan_stat_1 & ( 1 << loop))
				count++;
			}

#ifdef	SPEEDO
			/* post a message if a single fan has failed and it was detected for the
			 * first time through the loop
			 */
			 if ((count == 1) && !fan_ptr.fan_stat_2.STAT2_FAN_FL) {
				fan_ptr.fan_stat_2.STAT2_FAN_FL = 1;
				if (tasks.env2.ENV2_SPEEDO_SLAVE) 
					slave_cmd_param = SLAVE_FF;
				else
					sendHubInt(INT_FAN_OUT);
				speedoLED(AMBER, LED_BLINK);
			 }
			 
			/* get ready to shutdown for multiple failures */
			if ((count > 1) && !fan_ptr.fan_stat_2.STAT2_FAN_M_FL &&
					!tasks.status.STAT_PENDING_SHUTDOWN) {

				if (tasks.env2.ENV2_SPEEDO_SLAVE)
					slave_cmd_param = SLAVE_MFF;
				else {
					tasks.status.STAT_PENDING_SHUTDOWN = 1;
					fan_ptr.fan_stat_2.STAT2_FAN_M_FL = 1;
					tasks.fw_logical.FWL_PWR_DN = 1;
					sendHubInt(INT_PD_FAN);
				}
				
				speedoLED(RED, LED_BLINK);
			}
			
			/* Check for a single fan failure and a high temperature condition */
			else if ((count > 0) && !tasks.status.STAT_PENDING_SHUTDOWN
					&& tasks.status.STAT_SYSHT) {
				if (tasks.env2.ENV2_SPEEDO_SLAVE)
					slave_cmd_param = SLAVE_OT;
				else {
					tasks.status.STAT_PENDING_SHUTDOWN = 1;
					tasks.fw_logical.FWL_PWR_DN = 1;
					sendHubInt(INT_PD_TEMP);
				}
				
				speedoLED(RED, LED_BLINK);
			}
		

#else		//SPEEDO

			/* post a message if a single fan has failed and it was detected for the
			 * first time through the loop
			 */
			 if ((count == 1) && !fan_ptr.fan_stat_2.STAT2_FAN_FL) {
				fan_ptr.fan_stat_2.STAT2_FAN_FL = 1;
				sendHubInt(INT_FAN_OUT);
				dspMsg(MSG_FANFAIL);
			 }
			 
			
			/* get ready to shutdown for multiple failures */
			else if (count > 1 && !fan_ptr.fan_stat_2.STAT2_FAN_M_FL &&
				!tasks.status.STAT_PENDING_SHUTDOWN) {

				fan_ptr.fan_stat_2.STAT2_FAN_M_FL = 1;
				tasks.fw_logical.FWL_PWR_DN = 1;
				tasks.status.STAT_PENDING_SHUTDOWN =1;
				dspMsg(MSG_M_FANFL);
				sendHubInt(INT_PD_FAN);
				reg_state.reg_out3.SYS_OT_LED = 1;
				
			}
			/* Check for a single fan failure and a high temperature condition or a
			 * fan failure and a KCAR */
			else if (count > 0 && !tasks.status.STAT_PENDING_SHUTDOWN &&
					(tasks.status.STAT_SYSHT || (mp_type == MP_KCAR))) {
				tasks.fw_logical.FWL_PWR_DN = 1;
				tasks.status.STAT_PENDING_SHUTDOWN = 1;
				dspMsg(MSG_FANFL);
				sendHubInt(INT_PD_FAN);
				reg_state.reg_out3.SYS_OT_LED = 1;
				
			}
		
			/* Check for a failure of fan 1, 2, or 3 and fans are at high speed*/
			else if ((fan_ptr.fan_stat_1 & 0x07) && fan_ptr.fan_stat_2.STAT2_FAN_HI &&
					!tasks.status.STAT_PENDING_SHUTDOWN ) {
				tasks.fw_logical.FWL_PWR_DN = 1;
				tasks.status.STAT_PENDING_SHUTDOWN = 1;
				dspMsg(MSG_FANFL);
				sendHubInt(INT_PD_FAN);
			
				reg_state.reg_out3.SYS_OT_LED = 1;
			}
#endif	//SPEEDO
	
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

#endif

#ifdef	SPEEDO
/***************************************************************************
 *																									*
 *	speedoLED	Routine controls the front panel tri-color LED.					*
 *																									*
 ***************************************************************************/
void
speedoLED(color, mode)
char	color;
char	mode;
{
	/* set the color */
	if (color == RED) {
		reg_state.reg_out1.LED_RED = 1;
		reg_state.reg_out1.LED_GREEN = 0;
	}
	else if (color == GREEN) {
		reg_state.reg_out1.LED_RED = 0;
		reg_state.reg_out1.LED_GREEN = 1;
	}
	else if (color == AMBER) {
		reg_state.reg_out1.LED_RED = 1;
		reg_state.reg_out1.LED_GREEN = 1;
	}
	else {
		reg_state.reg_out1.LED_RED = 0;
		reg_state.reg_out1.LED_GREEN = 0;
	}

	ext_ptr_store = OUTP1;
	extMemWt(reg_state.reg_out1);

	led_state = reg_state.reg_out1 & 0x30;
	if(mode == LED_BLINK) {
		to_value = LED_BLINK_TIME;
		timeout(LED_BLINK_TO);
	}
	else
		cancel_timeout(LED_BLINK_TO);
}
#endif
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


	if (TCB.key_switch) {
		TCB.key_switch--;
		/* if time has expired set the status bit */
		if(TCB.key_switch == 0) 
			TCB.to_status.KEY_TO = 1;
	}

	if (TCB.nmi_button) {
		TCB.nmi_button--;
		/* if time has expired set the status bit */
		if(TCB.nmi_button == 0)
			TCB.to_status.NMI_TO = 1;
	}

	if (TCB.reset_button) {
		TCB.reset_button--;
		/* if time has expired set the status bit */
		if(TCB.reset_button == 0)
			TCB.to_status.RESET_TO = 1;
	}

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
	if (TCB.power_dn_delay) {
		TCB.power_dn_delay--;
		/* if time has expired set the status bit */
		if(TCB.power_dn_delay == 0) 
			TCB.to_status.POWER_DN_TO = 1;
	}
	if (TCB.nmi_execute) {
		TCB.nmi_execute--;
		/* if time has expired set the status bit */
		if(TCB.nmi_execute == 0) 
			TCB.to_status.NMI_EX_TO = 1;
	}
	if (TCB.rst_execute) {
		TCB.rst_execute--;
		/* if time has expired set the status bit */
		if(TCB.rst_execute == 0) 
			TCB.to_status.RST_EX_TO = 1;
	}
	if (TCB.hbt_delay) {
		TCB.hbt_delay--;
		/* if time has expired set the status bit */
		if(TCB.hbt_delay == 0) 
			TCB.to_status_1.HBT_INT = 1;
	}

	if (TCB.elsc_int_delay) {
		TCB.elsc_int_delay--;
		if (TCB.elsc_int_delay == 0) {
			TCB.to_status_1.ELSC_INT_TD = 1;
			cmdp.cmd_intr.ELSC_INT = 0;
		}
	}

	if (TCB.power_up_delay) {
		TCB.power_up_delay--;
		if (TCB.power_up_delay == 0)
			TCB.to_status_1.PWR_UP_TD = 1;
	}


#ifdef	SPEEDO
	if (TCB.led_blink) {
		TCB.led_blink--;
		/* if time has expired toggle the led */
		if(TCB.led_blink == 0) {
			if (reg_state.reg_out1 & 0x30)
				reg_state.reg_out1 &= 0xCF;
			else
				reg_state.reg_out1 |= led_state;
#if 0
			if (led_state.LED_GREEN)
				reg_state.reg_out1.LED_GREEN = reg_state.reg_out1.LED_GREEN ? 0 : 1;
			if (led_state.LED_RED)
				reg_state.reg_out1.LED_RED   = reg_state.reg_out1.LED_RED   ? 0 : 1;
#endif
			/* Load the table pointer. */
			ext_ptr = OUTP1;

			/* write the external memory location */
			*ext_ptr = reg_state.reg_out1;

			to_value = LED_BLINK_TIME;
			timeout(LED_BLINK_TO);
		}

	}
#endif

	/* Possibly send a token or grant the bus to the ELSC */
#ifdef	SPEEDO
	if(!tasks.env2.ENV2_SPEEDO_SLAVE)
	sendI2cToken();
#else
	sendI2cToken();
#endif

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

#ifdef	SPEEDO
	if (tcb_entry == KEY_TO) {
		TCB.key_switch = to_value;
		TCB.to_status.KEY_TO = 0;
	}
	else if (tcb_entry == NMI_TO) {
		TCB.nmi_button = to_value;
		TCB.to_status.NMI_TO = 0;
	}
	else if (tcb_entry == RESET_TO) {
		TCB.reset_button = to_value;
		TCB.to_status.RESET_TO = 0;
	}
	else if (tcb_entry == DELAY_TO) {
		TCB.sw_delay = to_value;
		TCB.to_status.DELAY_TO = 0;
	}
	else if (tcb_entry == FAN_TO) {
		TCB.fan_delay = to_value;
		TCB.to_status.FAN_TO = 0;
	}
	else if (tcb_entry == POWER_DN_TO) {
		TCB.power_dn_delay = to_value;
		TCB.to_status.POWER_DN_TO = 0;
	}
	else if (tcb_entry == NMI_EX_TO) {
		TCB.nmi_execute = to_value;
		TCB.to_status.NMI_EX_TO = 0;
	}
	else if (tcb_entry == RST_EX_TO) {
		TCB.rst_execute = to_value;
		TCB.to_status.RST_EX_TO = 0;
	}
	else if (tcb_entry == HBT_TO) {
		TCB.hbt_delay = to_value;
		TCB.to_status_1.HBT_INT = 0;
	}
	else if (tcb_entry == LED_BLINK_TO) {
		TCB.led_blink = to_value;
		TCB.to_status_1.LED_BLINK = 0;
	}
	else if (tcb_entry == ELSC_INT_TO) {
		TCB.elsc_int_delay = to_value;
		TCB.to_status_1.ELSC_INT_TD = 0;
	}

	else if (tcb_entry == POWER_UP_TO) {
		TCB.power_up_delay = to_value;
		TCB.to_status_1.PWR_UP_TD = 0;
	}
	
	
#else
	
	if (tcb_entry == KEY_TO) {
		TCB.key_switch = to_value;
		TCB.to_status.KEY_TO = 0;
	}
	else if (tcb_entry == NMI_TO) {
		TCB.nmi_button = to_value;
		TCB.to_status.NMI_TO = 0;
	}
	else if (tcb_entry == RESET_TO) {
		TCB.reset_button = to_value;
		TCB.to_status.RESET_TO = 0;
	}
	else if (tcb_entry == DELAY_TO) {
		TCB.sw_delay = to_value;
		TCB.to_status.DELAY_TO = 0;
	}
	else if (tcb_entry == FAN_TO) {
		TCB.fan_delay = to_value;
		TCB.to_status.FAN_TO = 0;
	}
	else if (tcb_entry == POWER_DN_TO) {
		TCB.power_dn_delay = to_value;
		TCB.to_status.POWER_DN_TO = 0;
	}
	else if (tcb_entry == NMI_EX_TO) {
		TCB.nmi_execute = to_value;
		TCB.to_status.NMI_EX_TO = 0;
	}
	else if (tcb_entry == RST_EX_TO) {
		TCB.rst_execute = to_value;
		TCB.to_status.RST_EX_TO = 0;
	}
	else if (tcb_entry == HBT_TO) {
		TCB.hbt_delay = to_value;
		TCB.to_status_1.HBT_INT = 0;
	}
	else if (tcb_entry == ELSC_INT_TO) {
		TCB.elsc_int_delay = to_value;
		TCB.to_status_1.ELSC_INT_TD = 0;
	}
	else if (tcb_entry == POWER_UP_TO) {
		TCB.power_up_delay = to_value;
		TCB.to_status_1.PWR_UP_TD = 0;
	}

#endif

}

