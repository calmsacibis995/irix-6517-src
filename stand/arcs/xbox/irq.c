
//#pragma option v  
/* irq.c */
/*
 * contains the interrupt handler.
 */

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
 *				main interrupt handler for the XBOX										*
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
	 

	if (PIR.CA2IF && PIE.CA2IE) {
		tasks.env.ENV_FAN_RUNNING = 1;
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

	
	
  /*
   * Check the for a timer interrupt
   */
 	if (PIR.TMR1IF && PIE.TMR1IE) {
		tick();
		PIR.TMR1IF = 0;
	}

	/* restore context of interrupt state register */
	int_state = int_state_reg;
	RestoreContext;
}

