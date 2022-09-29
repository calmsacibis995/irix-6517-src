//#pragma option v  
/* sequence.c */
/*
 * contains functions responsible for changing the state of the module.  The 
 * two main fucnctions are power_up and power_down.  Other needs to change
 * the state of the module i.e. NMI  maybe simple enough to embed in-line
 * with the code.
 *
 */
//#include <16c74.h>
//#include <elsc.h>
//#include <proto.h>

/* Global Structure Declarations */
//extern struct MONITOR tasks;		/* data structure containing task status */

/***************************************************************************
 *																									*
 *	power_up() - 	controls the power up sequence of the module.  				*
 *					Power_up() is executed as a result of the front panel 		*
 *					key switch being turned from the off position to 				*
 *					either the ON or DIAG position.  The interrupt code			*
 *					is responsible for decting a power up state of the 			*
 *					module and not calling power_up().									*
 *																									*
 *					Power_up() may also be called as part of a power cycle		*
 *					command or a power on command entered from the 					*
 *					alternate serial port.													*
 *																									*
 ***************************************************************************/
void
power_up()
{
	bits pwr_reg;

#ifdef 	SPEEDO

       	/* force the registered copy of over temperature to be normal */
	reg_state.reg_ra.TMP_SYS_OT = 1;

	speedoLED(GREEN, LED_BLINK);

	hub_tas = '0';

	/* Before Applying Power Assert Reset  */
	reg_state.reg_out1.SYS_RESET = 0;
	ext_ptr_store = OUTP1;
	extMemWt(reg_state.reg_out1);

	/*Assert Power Supply Enable  */
	reg_state.reg_ra.POWERON_L = 0;
//	PORTA = reg_state.reg_ra;
	PORTA.POWERON_L = 0;
	
	/* Wait for 3 sec, then check for PS_OK */
	if (!mfg_tst)
		to_value = THREE_SEC;
	else
		to_value = MSEC_500;
	delay();
	
	ext_ptr_store = INPT2;
	pwr_reg = extMemRd();
	
	/* check for a successful power up sequence */
	if (!pwr_reg.PS_OK) {
		speedoLED(RED, LED_SOLID);
		if (tasks.env2.ENV2_SPEEDO_SLAVE) 
			slave_cmd_param = SLAVE_PSF;
		else 
			master_cmd_param = SP_PWR_D;

		tasks.fw_logical.FWL_REMOVE_PWR = 1;
		return;
	}


#ifdef	FAN_CHK
	/* enable fan check and temperature monitoring after Thirty seconds */
	fan_ptr.fan_stat_1 = 0;
	fan_ptr.fan_stat_2 = 0;
	slave_fan_stat = 0;
	if (!mfg_tst)
		to_value = THIRTY_SEC;
	else
		to_value = MSEC_500;
	timeout(FAN_TO);
#endif	// FAN_CHK

	to_value = ONE_SEC;
	delay();

	/* set Power On Reset Flag */
	tasks.env1.ENV1_PON = 1;
	
	/* Now release Reset and turn on the Green LED*/
	tasks.status.STAT_SYSPWR = 1;
	speedoLED(GREEN, LED_SOLID);
	reg_state.reg_out1.SYS_RESET = 1;
	ext_ptr_store = OUTP1;
	extMemWt(reg_state.reg_out1);

	i2c_init();
	cmdp.cmd_intr.TOKEN_ENABLE = 1;

#else		// SPEEDO
	
	char pok;					/* return value for pok status */

	/* force the registered copy of over temperature to be normal */
	reg_state.reg_ra.TMP_SYS_OT = 1;

	if (reg_state.reg_in2.PS_OT2)
		return;
	else {
		/* make sure the OT LED is turned off */
		reg_state.reg_out3.SYS_OT_LED = 0;
		
#ifdef NEW_EXT
		ext_ptr_store = OUTP3;
		extMemWt(reg_state.reg_out3);
#else
		CPUSTA.GLINTD = 1;
		INTSTA.INTIE = 0;
		INTSTA.PEIE = 0;
		CPUSTA.GLINTD = 0;
		ext_ptr = OUTP3;
		*ext_ptr = reg_state.reg_out3;
		INTSTA.INTIE = 1;
		INTSTA.PEIE = 1;
#endif	// NEW_EXT
		
	}

	hub_tas = '0';

	/* Blank Both Rows */
	dspMsg(MSG_PWRUP);
	
	/* Before Applying Power Assert Reset and PLL Reset */
	reg_state.reg_out1.SYS_RESET = 0;
	reg_state.reg_out1.SYS_RESET_2 = 0;
	reg_state.reg_out1.PLL_RESET = 0;
	reg_state.reg_out1.PLL_RESET_2 = 0;
	reg_state.reg_out1.DC_OK = 1;

#ifdef NEW_EXT
	ext_ptr_store = OUTP1;
	extMemWt(reg_state.reg_out1);
#else
	CPUSTA.GLINTD = 1;
	INTSTA.INTIE = 0;
	INTSTA.PEIE = 0;
	CPUSTA.GLINTD = 0;
	ext_ptr = OUTP1;	
	*ext_ptr = reg_state.reg_out1;
	INTSTA.INTIE = 1;
	INTSTA.PEIE = 1;
#endif	//NEW_EXT

	/*Assert Power Supply Enable (PS_EN) */
	reg_state.reg_out3.PS_EN = 0;
	
#ifdef NEW_EXT
	ext_ptr_store = OUTP3;
	extMemWt(reg_state.reg_out3);
#else
	CPUSTA.GLINTD = 1;
	INTSTA.INTIE = 0;
	INTSTA.PEIE = 0;
	CPUSTA.GLINTD = 0;
	ext_ptr = OUTP3;	
	*ext_ptr = reg_state.reg_out3;
	INTSTA.PEIE = 1;
	INTSTA.INTIE = 1;
#endif	// NEW_EXT

	/* Wait for 3 sec, then check for PS_OK */
	if (!mfg_tst)
		to_value = THREE_SEC;
	else
		to_value = MSEC_500;
	delay();
	
#ifdef	_005

#ifdef NEW_EXT
	ext_ptr_store = INPT2;
	pwr_reg = extMemRd();
#else
	CPUSTA.GLINTD = 1;
	INTSTA.INTIE = 0;
	INTSTA.PEIE = 0;
	CPUSTA.GLINTD = 0;
	ext_ptr = INPT2;
	pwr_reg = *ext_ptr;
	INTSTA.INTIE = 1;
	INTSTA.PEIE = 1;
#endif	//NEW_EXT
	
	if (!pwr_reg.PS_OK) {
		dspMsg(MSG_PSFL);
		tasks.fw_logical.FWL_REMOVE_PWR = 1;
		return;
	}
	if (!(pwr_reg.PWR_FAIL)) {
		dspMsg(MSG_PFW);
		tasks.fw_logical.FWL_REMOVE_PWR = 1;
		return;
	}
#else
	if (!PORTB.PS_OK) {
		dspMsg(MSG_PSFL);
		tasks.fw_logical.FWL_REMOVE_PWR = 1;
		return;
	}

	if (!(PORTB.PWR_FAIL)) {
		dspMsg(MSG_PFW);
		tasks.fw_logical.FWL_REMOVE_PWR = 1;
		return;
	}
#endif	//_005

	/* Assert PENB */
	reg_state.reg_out2.PENB = 1;

#ifdef NEW_EXT
	ext_ptr_store = OUTP2;
	extMemWt(reg_state.reg_out2);
#else
	CPUSTA.GLINTD = 1;
	INTSTA.INTIE = 0;
	INTSTA.PEIE = 0;
	CPUSTA.GLINTD = 0;
	ext_ptr = OUTP2;	
	*ext_ptr = reg_state.reg_out2;
	INTSTA.INTIE = 1;
	INTSTA.PEIE = 1;
#endif	//NEW_EXT

	/* Wait 400 msec and check for POKB */
	to_value = MSEC_400;
	delay();
	
#ifdef NEW_EXT
	ext_ptr_store = INPT2;
	pwr_reg = extMemRd();
#else
	CPUSTA.GLINTD = 1;
	INTSTA.INTIE = 0;
	INTSTA.PEIE = 0;
	CPUSTA.GLINTD = 0;
	ext_ptr = INPT2;
	pwr_reg = *ext_ptr;
	INTSTA.INTIE = 1;
	INTSTA.PEIE = 1;
#endif	//NEW_EXT
	
#if defined(POK_CHK)

#ifdef	_005
	if (!PORTB.POKB) {
		getPOKBId();
		tasks.fw_logical.FWL_REMOVE_PWR = 1;
		return;
	}
	else
		reg_state.reg_rb.POKB = 1;

#else
	
	if (!pwr_reg.POKB) {
		getPOKBId();
		tasks.fw_logical.FWL_REMOVE_PWR = 1;
		return;
	}
#endif	//_005
#endif	//POK_CHK 

	/* if PWRFAIL & PS_OK & POKB are OK then assert DC_OK */

#ifdef POK_CHK

#ifdef	_005
	if ( PORTB.POKB && pwr_reg.PS_OK && pwr_reg.PWR_FAIL) {
		tasks.status.STAT_SYSPWR = 1;
		reg_state.reg_out1.DC_OK = 0;
		reg_state.reg_out1.PLL_RESET = 1;
		reg_state.reg_out1.PLL_RESET_2 = 1;
		
#ifdef NEW_EXT
		ext_ptr_store = OUTP1;
		extMemWt(reg_state.reg_out1);
#else
		CPUSTA.GLINTD = 1;
		INTSTA.INTIE = 0;
		INTSTA.PEIE = 0;
		CPUSTA.GLINTD = 0;
		ext_ptr = OUTP1;	
		*ext_ptr = reg_state.reg_out1;
		INTSTA.INTIE = 1;
		INTSTA.PEIE = 1;
#endif	// NEW_EXT
		
	}
#else
	if ( pwr_reg.POKB && PORTB.PS_OK && PORTB.PWR_FAIL) {
		tasks.status.STAT_SYSPWR = 1;
		reg_state.reg_out1.DC_OK = 0;
		reg_state.reg_out1.PLL_RESET = 1;
		reg_state.reg_out1.PLL_RESET_2 = 1;
		
#ifdef NEW_EXT
		ext_ptr_store = OUTP1;
		extMemWt(reg_state.reg_out1);
#else
		CPUSTA.GLINTD = 1;
		INTSTA.INTIE = 0;
		INTSTA.PEIE = 0;
		CPUSTA.GLINTD = 0;
		ext_ptr = OUTP1;	
		*ext_ptr = reg_state.reg_out1;
		INTSTA.INTIE = 1;
		INTSTA.PEIE = 1;
#endif	// NEW_EXT
		
	}
#endif	// _005
	
#else

#ifdef	_005
	if (pwr_reg.PS_OK && pwr_reg.PWR_FAIL) {
		tasks.status.STAT_SYSPWR = 1;
		reg_state.reg_out1.DC_OK = 0;
		reg_state.reg_out1.PLL_RESET = 1;
		reg_state.reg_out1.PLL_RESET_2 = 1;

#ifdef NEW_EXT
		ext_ptr_store = OUTP1;
		extMemWt(reg_state.reg_out1);
#else
		CPUSTA.GLINTD = 1;
		INTSTA.INTIE = 0;
		INTSTA.PEIE = 0;
		CPUSTA.GLINTD = 0;
		ext_ptr = OUTP1;	
		*ext_ptr = reg_state.reg_out1;
		INTSTA.INTIE = 1;
		INTSTA.PEIE = 1;
#endif 	// NEW_EXT
		
	}
#else
	if (PORTB.PS_OK && PORTB.PWR_FAIL) {
		tasks.status.STAT_SYSPWR = 1;
		reg_state.reg_out1.DC_OK = 0;
		reg_state.reg_out1.PLL_RESET = 1;
		reg_state.reg_out1.PLL_RESET_2 = 1;

#ifdef NEW_EXT
		ext_ptr_store = OUTP1;
		extMemWt(reg_state.reg_out1);
#else
		CPUSTA.GLINTD = 1;
		INTSTA.INTIE = 0;
		INTSTA.PEIE = 0;
		CPUSTA.GLINTD = 0;
		ext_ptr = OUTP1;	
		*ext_ptr = reg_state.reg_out1;
		INTSTA.INTIE = 1;
		INTSTA.PEIE = 1;
#endif  // NEW_EXT
		
	}
#endif	// _005
#endif	// POK_CHK
	

	/* turn off the Over temperature LED */
	reg_state.reg_out3.SYS_OT_LED = 0;

#ifdef	FAN_CHK
	/* enable fan check and temperature monitoring after Thirty seconds */
	fan_ptr.fan_stat_1 = 0;
	fan_ptr.fan_stat_2 = 0;
	to_value = THIRTY_SEC;
	timeout(FAN_TO);
#endif	// FAN_CHK

	to_value = ONE_SEC;
	delay();

	/* set Power On Reset Flag */
	tasks.env1.ENV1_PON = 1;
	
	/* Now release Reset */
	reg_state.reg_out1.SYS_RESET = 1;
	reg_state.reg_out1.SYS_RESET_2 = 1;

#ifdef NEW_EXT
	ext_ptr_store = OUTP1;
	extMemWt(reg_state.reg_out1);
#else
	CPUSTA.GLINTD = 1;
	INTSTA.INTIE = 0;
	INTSTA.PEIE = 0;
	CPUSTA.GLINTD = 0;
	ext_ptr = OUTP1;	
	*ext_ptr = reg_state.reg_out1;
	INTSTA.INTIE = 1;
	INTSTA.PEIE = 1;
#endif	//NEW_EXT

	if (!reg_state.reg_out1.DC_OK) 
		dspMsg(MSG_PWR_OK);

	i2c_init();
	cmdp.cmd_intr.TOKEN_ENABLE = 1;

#endif	// SPEEDO

}

/****************************************************************************
 *																									*
 *	power_dn() - 	controls the power down sequence of the module.				*
 *					The power down sequence will be in reverse order of the		*
 *					power up sequence.														*
 *																									*
 *																									*
 ****************************************************************************/
void
power_dn()
{

	// disable fan checking,  and lower fan speed
	// if possible
	cancel_timeout(FAN_TO);
	
#ifdef FAN_CHK
	fan_ptr.fan_stat_2.STAT2_FAN_CHK = 0;
	fan_ptr.fan_stat_2.STAT2_CNT_CHK = 0;
	PIE.CA2IE = 0;
	PIE.CA1IE = 0;
#endif
	
	/* Reset the Status Flag */
	tasks.status.STAT_SYSPWR = 0;

	/* Disable Temperature Monitoring Circuitry */
	tasks.env1.ENV1_TMP_EN = 0;
	
#ifndef	SPEEDO
	/* disable the I2C General Call Messages */
	cmdp.cmd_intr.TOKEN_ENABLE = 0;
#endif


	/* Now assert Panic Interrupt, if not a KCAR */
	if (mp_type != MP_KCAR) {
		reg_state.reg_out1.PANIC_INTR = 0;
		ext_ptr_store = OUTP1;
		extMemWt(reg_state.reg_out1);
	}

	/* Now set Reset */
	reg_state.reg_out1.SYS_RESET = 0;
	
#ifndef 	SPEEDO
	reg_state.reg_out1.SYS_RESET_2 = 0;
	reg_state.reg_out1.PLL_RESET = 0;
	reg_state.reg_out1.PLL_RESET_2 = 0;
#endif	//SPEEDO
	
	ext_ptr_store = OUTP1;
	extMemWt(reg_state.reg_out1);

#ifdef	SPEEDO
	/* remove power supply enable signal */
	reg_state.reg_ra.POWERON_L = 1;
	PORTA.POWERON_L = 1;
	NOP();
	PORTA.POWERON_L = 1;
	NOP();
	PORTA.POWERON_L = 1;

	to_value = MSEC_100;
	delay();

#else
	/* de-assert DC_OK Signal */
	reg_state.reg_out1.DC_OK = 1;
	reg_state.reg_out1.PLL_RESET = 0;
	reg_state.reg_out1.PLL_RESET_2 = 0;

	ext_ptr_store = OUTP1;
	extMemWt(reg_state.reg_out1);

	/* de-assert PENB */
	reg_state.reg_out2.PENB = 0;

	ext_ptr_store = OUTP2;
	extMemWt(reg_state.reg_out2);
	
	/* wait 100 msec and remove power supply enable */
	to_value = MSEC_100;
	delay();
	reg_state.reg_out3.PS_EN = 1;

	ext_ptr_store = OUTP3;
	extMemWt(reg_state.reg_out3);

#endif		//SPEEDO


	
	/* Now release Panic Interrupt, if not a KCAR */
	if (mp_type != MP_KCAR) {
		reg_state.reg_out1.PANIC_INTR = 1;
		ext_ptr_store = OUTP1;
		extMemWt(reg_state.reg_out1);
	}

#if 0
	/* Removed for speedo debug, reset is set above.  Releasing it seems unnecessary
	 * when code space is so tight
	 */
	 
	/* Now set Reset */
	reg_state.reg_out1.SYS_RESET = 1;
	
#ifndef 	SPEEDO
	reg_state.reg_out1.SYS_RESET_2 = 1;
	reg_state.reg_out1.PLL_RESET = 1;
	reg_state.reg_out1.PLL_RESET_2 = 1;
#endif	//SPEEDO
	
	ext_ptr_store = OUTP1;
	extMemWt(reg_state.reg_out1);

#endif
	
	if (tasks.fw_logical.FWL_PS_OT == 1)
		tasks.fw_logical.FWL_PS_OT = 0;

#ifdef	SPEEDO
	/* make sure the OT LED is turned off */
	speedoLED(LED_OFF, LED_SOLID);
#endif
	i2c_init();

	tasks.status.STAT_PENDING_SHUTDOWN = 0;

}


/***************************************************************************
 *																									*
 *	issueHbtRestart() -	Executes an module restart because of a 				*
 *									heart beat timeout.										*																*
 *																									*
 ***************************************************************************/
void
issueHbtRestart()
{

	if (hbt_state == HBT_TIMEOUT) {
		/* send message to display and Hubs */
#ifndef	SPEEDO
		dspMsg(MSG_HBT_TO);
#endif
		to_value = FIVE_SEC;
		timeout(HBT_TO);
		hbt_state = HBT_NMI;

	}

	else if (hbt_state == HBT_NMI) {
#ifdef	SPEEDO
		reg_state.reg_out1.LED_RED = 1;
		ext_ptr_store = OUTP1;
		extMemWt(reg_state.reg_out1);
#endif
		executeNMI();
#ifdef	SPEEDO
		if (tasks.env2.ENV2_SPEEDO_MASTER && tasks.env2.ENV2_SPEEDO_MS_ENABLE)
			master_cmd_param = SP_NMI;
#endif

		hbt_state = HBT_RESTART;
		to_value = hbt_cyc_to;
		timeout(HBT_TO);
	}

	else if (hbt_state == HBT_RESTART){
		tasks.fw_logical.FWL_REMOVE_PWR = 1;
		hbt_state = HBT_READY;
		to_value = FIVE_SEC;
		timeout(POWER_UP_TO);
	}
}


/***************************************************************************
 *																									*
 *	issue_NMI() -	Displays a message on the front panel and issue a			*
 *					NMI sequence.																*
 *																									*
 ***************************************************************************/
void
issueNMI()
{

	if (tasks.status.STAT_SYSPWR) {

#ifdef	SPEEDO
	
		if (tasks.env2.ENV2_SPEEDO_MASTER)
			master_cmd_param = SP_NMI;
		reg_state.reg_out1.LED_RED = 1;
		ext_ptr_store = OUTP1;
		extMemWt(reg_state.reg_out1);
		
#else
		dspMsg(MSG_FP_NMI);
#endif
			
		to_value = ONE_SEC;
		timeout(NMI_EX_TO);

	}
}

/***************************************************************************
 *																									*
 *	issue_RST() -	Displays a message on the front panel and issue a			*
 *					Reset sequence.															*
 *																									*
 ***************************************************************************/
void
issueReset()
{
	if (tasks.status.STAT_SYSPWR) {

#ifdef	SPEEDO
		reg_state.reg_out1.LED_RED = 1;
		ext_ptr_store = OUTP1;
		extMemWt(reg_state.reg_out1);
#else
		dspMsg(MSG_FP_RESET);
#endif

		sendHubInt(INT_RST);
		to_value = ONE_SEC;
		timeout(RST_EX_TO);
	}


}


/***************************************************************************
 *																									*
 *	executeNMI() -	Executes the NMI sequence on the across the midplane. 	*					NMI sequence.																*
 *																									*
 ***************************************************************************/
void
executeNMI()
{

	reg_state.reg_out1.NMI_OUT = 0;
	ext_ptr_store = OUTP1;
	extMemWt(reg_state.reg_out1);
	
	to_value = MSEC_100;
	delay();
	hub_tas = '0';
	reg_state.reg_out1.NMI_OUT = 1;
	
#ifdef	SPEEDO
	/* restore the LED to the same state as before the NMI */
		reg_state.reg_out1 &= 0xCF;
		reg_state.reg_out1 |= led_state;
#endif
	
	ext_ptr_store = OUTP1;
	extMemWt(reg_state.reg_out1);

	i2c_init();

	if (tasks.status.STAT_SYSPWR)
		cmdp.cmd_intr.TOKEN_ENABLE = 1;


}

/***************************************************************************
 *																									*
 *	executeReset() -	Executes the RST sequence on the across the midplane. *
 *																									*
 ***************************************************************************/
void
executeReset()
{

	reg_state.reg_out1.SYS_RESET = 0;

#ifndef	SPEEDO
	/*
	 * DC_OK is de-asserted because of a hardware problem with a processor never restarting
	 * when a reset is received while the processor is arbitrating with the hub for the bus.
	 * De-asserting DC_OK will release the processor in this case.
	 */
	reg_state.reg_out1.DC_OK = 1;
	
	reg_state.reg_out1.SYS_RESET_2 = 0;
#endif	//SPEEDO

	ext_ptr_store = OUTP1;
	extMemWt(reg_state.reg_out1);
	
	to_value = MSEC_100;
	delay();
	hub_tas = '0';

	/* Assert DC_OK and remove Reset */
	reg_state.reg_out1.SYS_RESET = 1;
	
#ifdef	SPEEDO
	/* restore the LED to the same state as before the NMI */
		reg_state.reg_out1 &= 0xCF;
		reg_state.reg_out1 |= led_state;
#else
	reg_state.reg_out1.DC_OK = 0;
	reg_state.reg_out1.SYS_RESET_2 = 1;
#endif	//SPEEDO

	ext_ptr_store = OUTP1;
	extMemWt(reg_state.reg_out1);

	i2c_init();
	if (tasks.status.STAT_SYSPWR)
		cmdp.cmd_intr.TOKEN_ENABLE = 1;


}
