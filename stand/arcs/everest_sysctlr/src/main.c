/*
	Mainline module for the Support Processor firmware. Called from 'boot.s'
*/

#include <exec.h>
#include <proto.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <sys_hw.h>
#include <async.h>
#include <sp.h>
#include <io6811.h>



#define	NIL		0	     /* Self Explanatory */

char fw_version[21];
char fw_ver[6] = "2.03";
char fw_date[21] = " 21 JUN 1993";
char fw_type[5] = " LAB";

int						memsize;		/* size of available memory */


/*
	main -
		Initialize the option flags and other system variables, fill
		in the diagnostic table. Jumps to 'execinit' to kick the system
		off.

		4/12/93 -	Added change to handle a SWI restart. 
  																		 
  		4/13/93 -	Added a changed to check for corrupted debug switch  
  					settings.  If corrupted, then store a "0" in the 	 
  					location, and post an error.						 

		4/20/93 -	Added an initialization of the NVRAM copy of the 
					LCD control register in hopes of eliminating the
					Hitachi Display start up of all characters being
					written to the first line

		4/23/93 - 	Changed the structure of the firware version and
					date because the firmware version is going to be
					added to the environmental data.

		4/26/93 -	Added a one-time non-volatile RAM initialization for
					the situation where the non-volatile RAM asignments are
					not compatible.  This makes compatibility transparent
					to the installing SE.
  																		 
*/

non_banked void
main()
{

	extern unsigned char 	ovr_temp;
	extern char 			pwr_dwn_state;
	extern char				active_switch;
	extern char 			active_screen;
	char					fp_buttons;
	extern unsigned char 	rx_mode;
	extern unsigned short	stuck_menu;				/*stuck button counter */
	extern unsigned short	stuck_scrlup;			/*stuck button counter */
	extern unsigned short	stuck_scrldn;			/*stuck button counter */
	extern unsigned short	stuck_execute;			/*stuck button counter */
	extern char				stuck_button;			/*stuck button flag */
	extern int				db_sw_enable;
	struct RTC				*ptod = (struct RTC *) L_RTC;	/* Pointer to RTC */
	extern char				boot_prog[42];			/* Boot Progress Message*/
	char					no_title[1] = {0};
	char					*sptr;

	int						i;

	strcpy(fw_version, fw_ver);
	strcat(fw_version, fw_date);
#ifdef LAB
	strcat(fw_version, fw_type);
#endif

	/* check the sanity byte for the magic number before taking any
	actions on the debug switch.  If the byte has an incorrect value
	in it, log_event will store the correct magic number.
	*/
	if(ptod->nv_sanity != 0x5A) {
		ptod->nv_irix_sw 		= 0;
		ptod->nv_irix_sw_not 	= 0xFFFF;
		ptod->nv_status 		= 0;
		ptod->nv_cmd_chk = 0;

		/* zero out the serial number */
		for(i = 0,sptr = ptod->nv_ser_num; i < 10; i++)
			*sptr++ = 0x0A;
	}

	/* Non-Volatile RAM Inter firmware version initialization:
	   if a firmware update requires a one time initialization of
	   non-volatile RAM, DO IT HERE */
	if(strcmp(fw_ver, ptod->nv_fwver)) {

		/* check sum the serial number (new for V1.4) */
		for(i = 0,ptod->nv_ser_cksum = 0,sptr = ptod->nv_ser_num; i < 10; i++)
			ptod->nv_ser_cksum ^= *sptr++;

		/* store the complement of the debug switches */
		ptod->nv_irix_sw_not = ~ptod->nv_irix_sw;

		/* update the firmware version in non-volatile RAM */
		strcpy(ptod->nv_fwver, fw_ver);

	}

	stuck_menu    = 0;
	stuck_scrldn  = 0;
	stuck_scrlup  = 0;
	stuck_execute = 0;
	active_switch = 0;
	active_screen = 0;
	ptod->nv_lcd_cntl = 0; 
#ifdef LAB
	db_sw_enable  = 1;
#else
	db_sw_enable  = 0;
#endif
	if (ptod->nv_status == RESET_IOT || ptod->nv_status == RESET_SWI) 
		pwr_dwn_state = PWR_UP;
	else
		pwr_dwn_state = PWR_PARTIAL;

	delay (SW_200MS);				/* wait a max of 200 ms */
	ptod->t_rega = 0x20;			/* only necessary for new clock part 
									   to start the internal osc.*/

	ptod->t_rega = 0x23;			/* start the square wave output of the */
	ptod->t_regb = 0x0A;			/* rtc to show it is running */


	if (ptod->nv_status != RESET_PO && ptod->nv_status != RESET_FERR) {
		for (i = 1; i < NUMPROC; i++)
			stop(i);
	}


	fp_buttons = *L_IRQ & (IRQ_EXECUTE | IRQ_MENU | IRQ_SCRLDN); 

	blower_init();
	fpinit();				/* re-init front panel LCD	*/
	init_port();				/* Set the necessary Data Direction Ports */
	async_init(INIT_ALL);		/* Serial channel setup */

	if (ptod->nv_status == RESET_IOT) 
		log_event(IOT_ERR, FP_DISPLAY);
	else if (ptod->nv_status == RESET_COP) 
		log_event(NOCOP_ERR, FP_DISPLAY);
	else if (ptod->nv_status == RESET_CME) 
		log_event(CME_ERR, FP_DISPLAY);
	else
		dsp_ver(ACT_STATUS);

	/* initialize memory copy of Power control register and serial address reg*/
	if (ptod->nv_status == RESET_PO || ptod->nv_status == RESET_PFW || 
		ptod->nv_status == RESET_FERR) {
		ptod->nv_pcntl = RI_ON;
		ptod->nv_ser_add = 0;
		ptod->nv_cmd_chk = 0;
		ovr_temp = 0;
		*L_PWR_CNT = ptod->nv_pcntl;
		ptod->nv_ser_add = 0x3F;			
		*L_SER_ADD = ptod->nv_ser_add;		/* Init. ser. address port but keep
										PEN_D and PEN_E off */
		strcpy(boot_prog, "BOOT ARBITRATION\nHAS NOT STARTED!");
	}




	execinit();					/* initialize RTE */
								/* (we become process #0) */
	rti_init();					/* enable RTI timer */

	if (ptod->nv_status == RESET_PO || ptod->nv_status & RESET_PFW) {
		if (fp_buttons & IRQ_EXECUTE) {
		/* if the EXECUTE  button (only) is depressed do not issue power up */ 
			ptod->nv_lcd_cntl |= FAULT_LED_L; 
			*L_LCD_CNT = ptod->nv_lcd_cntl;		/* turn of the fault LED */		
			active_switch |= IRQ_EXECUTE;
			rx_mode = ST_RDY;
			send(MONITOR, MON_IDLE); 
			send(DISPLAY, MENU);
			timeout(DISPLAY, EXECUTE_DEBOUNCE, ONE_SEC);
		}
		else {
			send(SEQUENCE, POWER_UP); 
		}

		if (fp_buttons & (IRQ_MENU | IRQ_SCRLDN)) {
			/* clear debug switch and reset error log counter */
			active_switch |= IRQ_MENU;
			active_switch |= IRQ_SCRLDN;
			timeout(DISPLAY, MENU_DEBOUNCE, ONE_SEC);
			timeout(DISPLAY, SCRLDN_DEBOUNCE, ONE_SEC);
			ptod->nv_irix_sw = 0; 
			ptod->nv_irix_sw_not = 0xFFFF; 
			ptod->nv_event_ctr = 0;
		}
	}
	else if (ptod->nv_status & RESET_FERR){
		if (PORTD & TERMINATOR) {
			status_fmt(no_title, ACT_STATUS, CY_SCROLL, NO_TIMEOUT);
			cy_str("\rLAST SHUTDOWN RESULT OF FATAL ERROR!!!\r\r"); 
			cy_str("CHECK LOG FOR CAUSE OF SHUTDOWN AND\r");
			cy_str("CYCLE SYSTEM POWER TO CONTINUE\r");
			cy_str("STARTUP WHEN SAFE.");
		}
		else {
			screen_sw(ACT_STATUS, SCREEN_TO);
			fpprint("PREVIOUS FATAL ERROR\nCHECK LOG/CYCLE PWR", DSP_INIT);
		}
		send(MONITOR, MON_IDLE); 
	}

	/* allow the blower(s) enough time to start working */
	delay (SW_500MS);				/* wait a max of 200 ms */
	enable_interrupt();				/* enable all interrupts */
	enable_xirq();

	/* warn the cpu about the system controller's IOT*/
	if(ptod->nv_status == RESET_IOT)
		cpu_cmd(CPU_WARNING, WRN_IOT);
	else if(ptod->nv_status == RESET_COP)
		cpu_cmd(CPU_WARNING, WRN_COP);
	else if(ptod->nv_status == RESET_CME)
		cpu_cmd(CPU_WARNING, WRN_OSC);
	else if(ptod->nv_status == RESET_SWI)
		cpu_cmd(CPU_WARNING, WRN_SWI);

	if(ptod->nv_status != RESET_PO && ptod->nv_status != RESET_PFW &&
	   ptod->nv_status != RESET_FERR) 
		send(MONITOR, MON_RUN);


	send(POK_CHK, POK_SCAN);
	cancel_timeout(MONITOR, RPM_CHECK_TO);
	timeout(MONITOR, RPM_CHECK_TO, ONE_MIN);

	if (ptod->nv_status != RESET_FERR)
		ptod->nv_status = 0;

	stop(0);				/* let processes initialize */
	/* NOTREACHED */
}

/*	When the "stop(0)" statement above is executed, RTE will start
	dispatching the various SysCntrl processes in priority order.  Each will
	execute its initialization code and then block on a "receive" call
	waiting for a request.  When they're all done, this "boot"
	process will finally be started by RTE (it's the lowest priority
	process in the system).  The boot process will be responsible for booting
	the CPU(s) in the system.
*/
