#include	<exec.h>
#include	<proto.h>
#include	<sp.h>
#include	<sys_hw.h>
#include	<io6811.h>


/****************************************************************************
 *																			*
 *	SEQUENCER:																*
 *																			*
 *				4/16/93 -	Added a send(MONITOR, MON_IDLE) after an 		*
 *							unsuccessful power up, to allow the sys_mon		*
 *							process to monitor 48 volts, fan speed and		*
 *							temp. in the partial power down state			*
 *																			*
 *				4/20/93 -	Changed the power down nomenclature to KILL_ALL *
 *							when shutting the system down due to an error   *
 *							with the key switch still in the ON position.	*
 *																			*
 ****************************************************************************/
non_banked void
sequencer()
{
	unsigned short	msg;
	extern char pwr_dwn_state;
	extern unsigned char rx_mode;
	struct RTC	*ptod = (struct RTC *) L_RTC;		/* Pointer to RTC */
	char notitle[1] = {0};

	char error_shutdn;

	error_shutdn = 0;
	for (;;) {
		msg = receive(SEQUENCE);
		switch(msg) {
			case	DOWN_KILL:					/*use kill pwr to power down */
				shutdown(KILL_ALL);
				break;
			case	DOWN_PARTIAL:				/* leave 48V up */
				shutdown(PARTIAL);
				break;
			case	DOWN_TOTAL:					/* Normal power down */
				if(!error_shutdn)
					log_event(SYS_OFF, FP_DISPLAY);
				shutdown(TOTAL);
				break;
			case	DOWN_1SEC:
				timeout(SEQUENCE, DOWN_PARTIAL, ONE_SEC);
				break;
			case	DOWN_10SEC:
				timeout(SEQUENCE, DOWN_PARTIAL, TEN_SEC);
				break;
			case	SWITCH_OFF:
				/* if in boot mode, shut down now, else wait for timeout */
				if ((rx_mode == ST_BOOT) || (pwr_dwn_state == PWR_PARTIAL)) 
					send(SEQUENCE, DOWN_TOTAL);
				else {
					/* inform the CPU of a pending shutdown */
					cpu_cmd(CPU_ALARM, ALM_OFF);
					timeout(SEQUENCE, DOWN_TOTAL, ALARM_TIMEOUT);
					status_fmt(notitle, ACT_STATUS, CY_SCROLL, NO_TIMEOUT);
					fpprint("SHUTDOWN PENDING...\r", DSP_INIT);
				}
				break;
			case	TEMPERATURE_OFF:
				/* if in boot mode, shut down now, else wait for timeout */
				error_shutdn = 1;
				if ((rx_mode == ST_BOOT) || (pwr_dwn_state == PWR_PARTIAL))
					send(SEQUENCE, DOWN_KILL);
				else {
					/* inform the CPU of a pending shutdown */
					cpu_cmd(CPU_ALARM, ALM_TEMP);
					timeout(SEQUENCE, DOWN_KILL, ALARM_TIMEOUT);
				}
				break;
			case	INCOMING_TEMP_OFF:
				error_shutdn = 1;
				/* if in boot mode, shut down now, else wait for timeout */
				if ((rx_mode == ST_BOOT) || (pwr_dwn_state == PWR_PARTIAL))
					send(SEQUENCE, DOWN_KILL);
				else {
					/* inform the CPU of a pending shutdown */
					cpu_cmd(CPU_ALARM, ALM_TEMP);
					timeout(SEQUENCE, DOWN_KILL, ALARM_TIMEOUT);
				}
				break;
			case	WARNING_FAN:
				/* inform the CPU of a pending shutdown */
				/* if in boot mode, shut down now, else wait for timeout */
				error_shutdn = 1;
				if (rx_mode == ST_BOOT)
					send(SEQUENCE, DOWN_PARTIAL);
				else {
					cpu_cmd(CPU_ALARM, ALM_BLOWER);
					timeout(SEQUENCE, DOWN_PARTIAL, ALARM_TIMEOUT);
				}
				break;
			case	POWER_UP:
				if (powerup()) {
					send(MONITOR, MON_RUN_ONCE);
					timeout(CPU_PROC, BOOT_ARBITRATE, ONE_SEC);
				}
				else
					send(MONITOR, MON_IDLE);
				break;
			case	FP_SCLR:
				if (ptod->nv_pcntl & PENA) {
					ptod->nv_cmd_chk = 0;
					ptod->nv_pcntl &= ~SCLR;
					*L_PWR_CNT = ptod->nv_pcntl;
					delay(SW_100MS);
					ptod->nv_pcntl |= SCLR;
					*L_PWR_CNT = ptod->nv_pcntl;
					log_event(SYS_RESET, FP_DISPLAY);
					/* Turn off Fail LED */
					ptod->nv_lcd_cntl |= FAULT_LED_L;
					*L_LCD_CNT = ptod->nv_lcd_cntl;	/* turn off the fault LED */
					send(CPU_PROC, BOOT_ARBITRATE);
				}
				else {
					status_fmt(notitle, ACT_STATUS, CY_SCROLL, NO_TIMEOUT);
					if(PORTD & TERMINATOR)
						fpprint("EBUS IS POWERED DOWN, SCLR IGNORED", DSP_INIT);
					else
						fpprint("EBUS IS POWERED DOWN\rSCLR IGNORED", DSP_INIT);
				}
				break;
			case	MN_NMI:			/*front panel command */
				if (ptod->nv_pcntl & PENA) {
					ptod->nv_pcntl |= BP_NMI;
					*L_PWR_CNT = ptod->nv_pcntl;
					delay(SW_50MS);
					ptod->nv_pcntl &= ~BP_NMI;
					*L_PWR_CNT = ptod->nv_pcntl;
					log_event(NMI, FP_DISPLAY);
					ptod->nv_lcd_cntl |= FAULT_LED_L;
					*L_LCD_CNT = ptod->nv_lcd_cntl;	/* turn off the fault LED */
				}
				else {
					status_fmt(notitle, ACT_STATUS, CY_SCROLL, NO_TIMEOUT);
					if (PORTD & TERMINATOR)
						fpprint("EBUS IS POWERED DOWN, NMI IGNORED", DSP_INIT);
					else
						fpprint("EBUS IS POWERED DOWN\rNMI IGNORED", DSP_INIT);
				}
				break;
			default:
				log_event(BMSG_SEQUEN, NO_FP_DISPLAY);
				break;
		}
	}
}

/****************************************************************************
 *																			*
 *	Powerup:		Applies power to the system per the system controller	*
 *					specification.  The 1msec time periods in between 		*
 *					events are easily asorbed by the instruction rates		*
 *																			*
 *					4/16/93 -	Moved the pwr_dwn state flag of PWR_UP		*
 								from the end of the routine to the 			*
 *								beginning.  The initial value of a partial	*
 *								power up was fooling the shutdown routine	*
 *								if a POK failure occurs during power up		*
 *								into thing the system was already powered	*
 *								down, resulting in the power remaining 		*
 *								after the failure.							*
 *																			*
 ****************************************************************************/
non_banked int 
powerup(void)
{

	struct RTC	*ptod = (struct RTC *) L_RTC;		/* Pointer to RTC */
	extern char 			pwr_dwn_state;
	char				shutdown_mode;
	int					fan_err;
	fan_err = 0;

#ifdef LAB
	shutdown_mode = ptod->nv_irix_sw & 0x1000 ? 0 : 1;
#else
	shutdown_mode = 1;
#endif

	pwr_dwn_state = PWR_UP;

	/*Assert PENA */
	ptod->nv_pcntl |= PENA;
	*L_PWR_CNT = ptod->nv_pcntl;

#if 0
	/* changed on 6/17/93 at request of Nick */
	delay(SW_200MS);
#endif
	delay(SW_500MS);

	/*Assert PENB */
	ptod->nv_pcntl |= PENB;
	*L_PWR_CNT = ptod->nv_pcntl;

	delay(SW_200MS);
	/*Check  for a POK_A or POK_B*/
	if ((*L_XIRQ & 0x06) == XIRQ_POK_A) {
		log_event(POKA_FAIL, FP_DISPLAY);
		if (shutdown_mode) {
			send(SEQUENCE, DOWN_PARTIAL);
			return(0);
		}
	}

	if ((*L_XIRQ & 0x06) == XIRQ_POK_B) {
		log_event(POKB_FAIL, FP_DISPLAY);
		if (shutdown_mode) {
			send(SEQUENCE, DOWN_PARTIAL);
			return(0);
		}
	}

	/*Assert PENC */
	ptod->nv_pcntl |= PENC;
	*L_PWR_CNT = ptod->nv_pcntl;

	delay(SW_200MS);

	/*Check for a POK_C Failure*/
	if ((*L_XIRQ & 0x06) == XIRQ_POK_C) {
		log_event(POKC_FAIL, FP_DISPLAY);
		if (shutdown_mode) {
			send(SEQUENCE, DOWN_PARTIAL);
			return(0);
		}
	}

	/*Assert PEND */
	ptod->nv_ser_add |= PEND;
	*L_SER_ADD = ptod->nv_ser_add;

	delay(SW_200MS);

	/*Check for a POK_D Failure*/
	if (*L_XIRQ & XIRQ_POK_D) {
		log_event(POKD_FAIL, FP_DISPLAY);
		if (shutdown_mode) {
			send(SEQUENCE, DOWN_PARTIAL);
			return(0);
		}
	}

	/*Assert PENE */
	ptod->nv_ser_add |= PENE;
	*L_SER_ADD = ptod->nv_ser_add;

	delay(SW_200MS);

	/*Set timeout for POK_E check*/
	timeout(POK_CHK, POKE_ENABLE, THIRTY_SEC);


	/* Wait 500 msec and then check the system clock */
	delay(SW_500MS);

#ifdef CLK_CHK
	if( !(PORTA & BP_CLOCK)) {
		log_event(NO_CLCK, FP_DISPLAY);
		if (shutdown_mode) {
			send(SEQUENCE, DOWN_PARTIAL);
			return(0);
		}
	}
#endif

	/* Remove PCLR and SCLR */
	ptod->nv_pcntl |= PCLR;
	*L_PWR_CNT = ptod->nv_pcntl;
	delay(SW_4MS);
	ptod->nv_pcntl |= SCLR;
	*L_PWR_CNT = ptod->nv_pcntl;

	log_event(SYS_ON, FP_DISPLAY);
	ptod->nv_lcd_cntl |= FAULT_LED_L;
	*L_LCD_CNT = ptod->nv_lcd_cntl;			/* turn off the fault LED */				

return(1);
}

/****************************************************************************
 *																			*
 *	Shutdown:		Remove power from the system per the system controller	*
 *					specification.  The 1msec time periods in between 		*
 *					events are easily asorbed by the instruction rates		*
 *																			*
 *					4/14/93 -	Modifed the key off shutdown so that if 	*
 *								the	turned on before power goes away, a		*
 *								software restart will occur.				*
 *																			*
 *								Moved the logging of a power fail warning	*
 *								from the interrupt code (xirq_interrupt())	*
 *								to this function, due to the long delay		*
 *								of removing the power because of the error	*
 *								log routine.  The error log statement is	*
 *								now after the PENs are disable.	One draw	*
 *								back is the error logging might not 		*
 *								if the power goes away too quickly.			*
 *																			*
 *					4/20/93 -	Added a third shutdown mode, KILL_ALL. 		*
 *								Previously the key switch was examined to 	*
 *								determine if it was a normal shut down or	*
 *								the power kill was used.  This confused 	*
 *								the code when the key was switched off		*
 *								and then back on again.
 ****************************************************************************/
non_banked monitor void 
shutdown(type)
unsigned char type;				/* type of shutdown */
{

	extern char pwr_dwn_state;
	struct RTC	*ptod = (struct RTC *) L_RTC;		/* Pointer to RTC */

	/* remove the sys_mon process from the timeout queue or ready queue
	   and if a partial power down reschedule the system monitor for the 
	   idle state.
	 */
	 cancel_timeout(MONITOR, MON_RUN);

	/* if already in a partial power down state and type is partial
		then exit now */
	if (type == PARTIAL && pwr_dwn_state == PWR_PARTIAL)
		return;
	/* set pwr_dwn flag to prevent the logging of a power fail warning */
	if (type == PARTIAL)
		pwr_dwn_state = PWR_PARTIAL;
	else
		pwr_dwn_state = PWR_D_TOTAL;

	/* on a power fail remove apply PCLR and SCLR, remove PEN_C, PEN_D, and 
		PEN_E.  Then wait 1 msec and remove PEN_B followed by a 15 msec wait
		and then remove PEN_A.

		While waiting for the power to go away, monitor the PWR_FAIL line, and 
		if the iterrupt condition goes away (BROWN OUT) reset the system and
		apply power.
	 */
	if (type == PWR_FAIL) {
		ptod->nv_pcntl &= ~(SCLR | PCLR | PENC);
		*L_PWR_CNT = ptod->nv_pcntl;

		ptod->nv_ser_add &= ~(PENE | PEND);
		*L_SER_ADD = ptod->nv_ser_add;

		delay(SW_1MS);

		/*De-assert PENB */
		ptod->nv_pcntl &= ~PENB;
		*L_PWR_CNT = ptod->nv_pcntl;

		delay(SW_15MS);

		/*De-assert PENA */
		ptod->nv_pcntl &= ~PENA;
		*L_PWR_CNT = ptod->nv_pcntl;

		log_event(PWR_FAIL, FP_DISPLAY);

		/*wait here to die or for power fail alarm to go away */
		while(1) {
			if (!(*L_XIRQ & XIRQ_PFW)) {
				ptod->nv_status = RESET_PFW;

				/* jam the stack pointer to the initial value */
				_opc(0x8E); _opc(0x1F); _opc(0xFE);		/* LDS 0x1FFE */
				sys_reset();
			}
		}
	}
	else {
		/*issue SCLR */
		ptod->nv_pcntl &= ~SCLR;
		*L_PWR_CNT = ptod->nv_pcntl;

		/*issue PCLR */
		delay(SW_1MS);
		ptod->nv_pcntl &= ~PCLR;
		*L_PWR_CNT = ptod->nv_pcntl;
		delay(SW_1MS);

		/*De-assert PENE */
		ptod->nv_ser_add &= ~PENE;
		*L_SER_ADD = ptod->nv_ser_add;

		/* Wait 200 Msec */
		delay(SW_200MS);
	
		/*De-assert PEND */
		ptod->nv_ser_add &= ~PEND;
		*L_SER_ADD = ptod->nv_ser_add;
	
		/* Wait 200 Msec */
		delay(SW_200MS);
	
		/*De-assert PENC */
		ptod->nv_pcntl &= ~PENC;
		*L_PWR_CNT = ptod->nv_pcntl;
	
		/* Wait 200 Msec */
		delay(SW_200MS);
	
		/*De-assert PENB */
		ptod->nv_pcntl &= ~PENB;
		*L_PWR_CNT = ptod->nv_pcntl;
	
		/* Wait 200 Msec */
		delay(SW_200MS);
	
		/*De-assert PENA */
		ptod->nv_pcntl &= ~PENA;
		*L_PWR_CNT = ptod->nv_pcntl;
	
		/*Remove All Power if the key switch or TOTAL shutdown */
		if (type == TOTAL) {
			/* Normal Power Down; Assert Remote Inhibit */
			ptod->nv_pcntl &= ~RI_ON;
			*L_PWR_CNT = ptod->nv_pcntl;
			/*Wait here until the power goes away */
			while(1) {
				/*	if the key switch is turned to ON or MGR before power
				goes away do a software restart
				*/
				if (!(*L_IRQ & IRQ_KEY)) {
					/* jam the stack pointer to the initial value */
					_opc(0x0F);							/*Set Interrupt Mask*/
					_opc(0x8E); _opc(0x1F); _opc(0xFE);	/* LDS 0x1FFE */
					sys_reset();
				}
			}
		}
		else if (type == KILL_ALL) {
			/* Error condition; Kill All Power */
			ptod->nv_pcntl &= ~RI_ON;
			ptod->nv_pcntl |= KILL_PWR;
			*L_PWR_CNT = ptod->nv_pcntl;
			/*Wait here until the power goes away */
			while(1);
		}
	
		/* On a partial power down schedule the monitor to run in the idle
		   state. */
		if(type == PARTIAL)
			send(MONITOR, MON_IDLE);
	}
}
