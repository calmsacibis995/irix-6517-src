//#pragma option v  
/* sequence.c */
/*
 * contains functions responsible for changing the state of the module.  The 
 * two main fucnctions are power_up and power_down.  
 *
 */

/***************************************************************************
 *																									*
 *	power_up() - 	controls the power up sequence of the XBOX module.  		*
 *					Power_up() is executed as a result of either the X9_RMTOK	*
 *					or XA_RMTOK signals being asserted.									*
 *																									*
 ***************************************************************************/
void
power_up()
{
	bits pwr_reg;

	/* LEDs Off */
	cancel_timeout(LED_BLINK_TO);
	cntLED(FP_LED|PCI_LED|X0_LED|X1_LED, LED_OFF);


   /* force the registered copy of over temperature to be normal */
	reg_state.reg_rb.TEMP_OVER_L = 1;

	/* Turn on the front panel led */
	if (!mfg_mode)
		cntLED(FP_LED, LED_BLINK|LED_GREEN);
	else
		cntLED(FP_LED, LED_BLINK|LED_YELLOW);

	/* Before Applying Power Assert Reset  */
	reg_state.reg_ra.SYSRST_L = 0;
	PORTA = reg_state.reg_ra;
	
	/*Assert Power Supply Enable  */
	reg_state.reg_ra.PWRON_L = 0;
	PORTA = reg_state.reg_ra;
	
	/* Wait for 3 sec, then check for PS_OK */
	to_value = TWO_SEC;
	delay();
	
	reg_state.reg_ra = PORTA;
	
	/* check for a successful power up sequence */
	if (!reg_state.reg_ra.PSOK) {
		cntLED(FP_LED, LED_RED|LED_BLINK);
		tasks.fw_logical.FWL_PWR_DN = 1;
		NoPower = 1;
		return;
	}

	/* Wait 500 msec and set the PEN Flip-flop */
	to_value = MSEC_500;
	delay();
	
	/* the data value does not matter */
	extMemWt(PEN_REG);

	/* Wait 500 msec and check for POKB */
	to_value = MSEC_500;
	delay();
	
	pwr_reg = extMemRd();
	
	if (!pwr_reg.SC_LPOK) {
		cntLED(FP_LED, LED_RED|LED_BLINK);
		tasks.fw_logical.FWL_PWR_DN = 1;
		NoPower = 1;
		return;
	}

	if (!mfg_mode) {
	/* enable fan check and temperature monitoring after Thirty seconds */
		fan_ptr.fan_stat_1 = 0;
		fan_ptr.fan_stat_2 = 0;
		to_value = THIRTY_SEC;
		timeout(FAN_TO);

		/* Turn on the three green FAN LEDs */
		cntLED(PCI_LED|X0_LED|X1_LED, LED_GREEN);
	}
	else
		/* Blink the FAN LEDs Yellow to warn of the manufacturing mode*/
		cntLED(PCI_LED|X0_LED|X1_LED, LED_YELLOW|LED_BLINK);


	to_value = ONE_SEC;
	delay();

	/* Now release Reset and turn on the Green LED*/
	tasks.status.STAT_SYSPWR = 1;
	if(!mfg_mode)
		cntLED(FP_LED, LED_GREEN);
	else
		cntLED(FP_LED, LED_YELLOW);
	reg_state.reg_ra.SYSRST_L = 1;
	PORTA = reg_state.reg_ra;

}

/****************************************************************************
 *																									*
 *	power_dn() - 	controls the power down sequence of the module.				*
 *			The power down sequence will be in reverse order of the		*
 *			power up sequence.														*
 *																									*
 *																									*
 ****************************************************************************/
void
power_dn()
{

	char fans;
	
	// disable fan checking,  and lower fan speed
	// if possible
	if (!mfg_mode) {
		cancel_timeout(FAN_TO);
		fan_ptr.fan_stat_1.STAT2_FAN_CHK = 0;
		fan_ptr.fan_stat_2.STAT2_CNT_CHK = 0;
		PIE.CA2IE = 0;
	}
	
	/* Reset the Status Flag */
	tasks.status.STAT_SYSPWR = 0;

	/* Disable Temperature Monitoring Circuitry */
	tasks.env.ENV_TMP_EN = 0;
	
	/* Set Reset */
	reg_state.reg_ra.SYSRST_L = 0;
	PORTA = reg_state.reg_ra;
	
	/* remove power supply enable signal */
	reg_state.reg_ra.PWRON_L = 1;
	PORTA = reg_state.reg_ra;

#if 0
	/* Remove PENB */
	reg_state.reg_pwr_add.SC_PENB = 0;
	ext_ptr_store = PWR_ADD;
	extMemWt(reg_state.reg_pwr_add, ?????);
#endif
	/* Manage the status LEDs */
	fans = FP_LED;

	/* Turn off the fan LED's, except any that have failed */
	if (!fan_ptr.fan_stat_1.STAT1_FAN_0)
		fans |= X0_LED;
	if (!fan_ptr.fan_stat_1.STAT1_FAN_1)
		fans |= X1_LED;
	if (!fan_ptr.fan_stat_1.STAT1_FAN_2)
		fans |= PCI_LED;

	cntLED(fans, LED_OFF);

	/* reset the status register */
	reg_state.reg_xbox_status.OVER_TEMP = 0;
	extMemWt(STATUS_REG);

	/* Reset the shutdown flag */
	shutdown_pending = 0;


	/* ensure a two second down time */
	to_value = TWO_SEC;
	delay();


}


