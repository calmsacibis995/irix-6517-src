/******** CPUPROC.C *****************************************************
 *  cpuproc.c handles the CPU commands passed to the system controller	*
 *	by the CPU.  Included in this module is the SCI port and data 		*
 *	structure initialization, CPU_PROC process function, and the 		*
 *	individual CPU related commands.									*
 ************************************************************************/

#include	<exec.h>
#include 	<sp.h>
#include 	<async.h>
#include 	<proto.h>
#include 	<io6811.h>
#include 	<sys_hw.h>
#include 	<stdio.h>
#include 	<string.h>

char mark;
char			buff[341];	/* must be large enough to hold event log file */
char			cpu_rsp;     /* flag used to indicate CPUs response during
								boot arbitration */

extern struct cbufh 	sci_inq,			/* Input SCI data queue */
						sci_outq;			/* Output SCI data queue */


/************************************************************************
 *																		*
 *	cpu_proc	is the process that handles the parasing of commands	*
 *				received from the CPU.									*
 *																		*
 *		4/21/93 -	Changed the error return from getcmd() to just 		*
 *					ignore the result.  An error being returned from	*
 *					the getcmd is now the result of an dropped 		 	*
 *					characters, and will be ingored.					*
 *																		*
************************************************************************/
non_banked void 
cpu_proc()
{
	unsigned short	msg;
	extern char		active_screen;
	struct cbufh 	*inqp,				/* pointer to receiver queue */
					*outqp;				/* pointer to transmitter queue */
	short			to_cnt;				/* timeout count remaining */

	char			first_perf_data;	/* first time data has been received */

	struct 	RTC	*ptod = (struct RTC *) L_RTC;	/* Pointer to RTC */
	extern struct ENV_DATA		env;	/*environmental data */
	struct ENV_DATA				*env_ptr;

	inqp  = &sci_inq;
	outqp = &sci_outq;
	first_perf_data = 1;
	env_ptr = &env;
	env_ptr->invalid_cpu_cmd = 0;

	for (;;) {
		msg = receive(CPU_PROC); 
		switch(msg) {
			case	BOOT_ARBITRATE:
				if(!(ptod->nv_irix_sw & 0x40)) {  
					boot_arb();
					first_perf_data = 1;
				}
				timeout(MONITOR, MON_RUN, TEN_SEC);
				break;
			case	BOOT_DELAY:
				/* just in case the BOOT_DELAY was not cancel during 
					the aborted boot arbitration */
				break;
			case	CPU_CMD:
				/* get the command from the serial data queue */
				if (getcmd(inqp, buff) != 0) 
					break;

				/* Now separate the GET from the SET commands */
				if(buff[0] == 'G') {
					switch(buff[2]) {
						case	'T':
							get_time(buff, buff[1]);
							break;
						case	'D':
							get_debug_sw(buff, buff[1]);
							break;
						case	'E':
							get_env_data(buff, buff[1]);
							break;
						case	'L':
							get_log(buff, buff[1]);
							break;
						case	'P':
							get_lcd(buff, buff[1]);
							break;
						case	'S':
							get_ser_no(buff, buff[1]);
							break;
						case	'C':
							get_check(buff, buff[1]);
							break;
						default:
							env_ptr->invalid_cpu_cmd++;
							break;
					}
					if(putcmd(outqp, buff) < 0) {
						log_event(NO_CPU_RSP, NO_FP_DISPLAY);
						break;
					}
				}
				else if (buff[0] == 'S') {
					switch(buff[1]) {
						case	'M':
							set_bt_msg(buff);
							break;
						case	'T':
							rtc_wr(&buff[2], min(12, strlen(&buff[2])));
							break;
						case	'D':
							set_debug_sw(buff);
							break;
						case	'H':
							set_cpu_hist(buff);
							if (first_perf_data == 1) {
								first_perf_data = 0;
								send(DISPLAY, CPU_PERF);
							}
							break;
						case	'K':			/* Klear Log */
							ptod->nv_event_ctr = 0;
							break;
						case	'O':
							if (cancel_timeout(SEQUENCE, DOWN_KILL))
								send(SEQUENCE,DOWN_KILL);
							else if (cancel_timeout(SEQUENCE, DOWN_PARTIAL))
								send(SEQUENCE,DOWN_PARTIAL);
							else if (cancel_timeout(SEQUENCE, DOWN_TOTAL))
								send(SEQUENCE,DOWN_TOTAL);
							else
								send(SEQUENCE,DOWN_PARTIAL);
							break;
						case	'P':
							set_proc_stat(buff);
							break;
						case	'S':
							set_ser_no(buff);
							break;
						case	'Z':
							to_cnt = cancel_timeout(SEQUENCE, DOWN_TOTAL);
							if (to_cnt > 0) 
								timeout(SEQUENCE,DOWN_TOTAL,to_cnt + ALARM_TIMEOUT);
							to_cnt = cancel_timeout(SEQUENCE, DOWN_KILL);
							if (to_cnt > 0)
								timeout(SEQUENCE,DOWN_KILL,to_cnt + ALARM_TIMEOUT);
							to_cnt = cancel_timeout(SEQUENCE, DOWN_PARTIAL);
							if (to_cnt > 0)
								timeout(SEQUENCE,DOWN_PARTIAL,to_cnt + ALARM_TIMEOUT);
							break;
						case	'C':
							set_check();
							break;
						default:
								env_ptr->invalid_cpu_cmd++;
							break;
					}
				}
				break;
			default:
				log_event(BMSG_CPUPROC, NO_FP_DISPLAY);
				break;
		}
	}
}


non_banked void 
boot_arb() 
{
	extern char 		pstat_buff[41];
	extern char 		btmsg_buff[41];
	extern char 		pwr_dwn_state;
	extern char			boot_prog[42];
	extern char 		active_switch;		/* Register for Switch Debounce */
	unsigned char		cpu_add,
						end_cpu;
	unsigned short		msg;
	char				notitle[1] = {0};
	char				abort;

	struct cbufh	*inq;			/* pointer to cbuf structure */
	struct cbufh	*outq;			/* pointer to cbuf structure */
	extern unsigned char rx_mode;	/* mode of SCI receiver, boot, rdy, bsy */
	struct 	RTC	*ptod = (struct RTC *) L_RTC;	/* Pointer to RTC */
	extern	char		acia_cntl;			/* Memory copy of control reg */

	int arb;

	/* check to make sure a power down has not occurred after the 
		boot arb message was sent.  if a pwr_dwn has occured just return */

	if(pwr_dwn_state == PWR_PARTIAL)
		return;


	pstat_buff[0] = 0;
	btmsg_buff[0] = 0;
	abort = 0;
	rx_mode = ST_BOOT;
	status_fmt(notitle, ACT_STATUS, CY_SCROLL, SCREEN_TO);
	if (PORTD & TERMINATOR) 
		strcpy(boot_prog, "BOOT ARBITRATION IN PROGRESS!\r");
	else
		strcpy(boot_prog, "BOOT ARBITRATION\nIN PROGRESS!");
	fpprint(boot_prog, DSP_INIT);
	inq = &sci_inq;
	outq = &sci_outq;
	cpu_rsp = 0;
	flush(inq);
	flush(outq);

#ifdef LAB
		arb = 10;
		end_cpu = 64;
#else
	if(PORTD & TERMINATOR) {
		arb = 10;
		end_cpu = 64;
	}
	else {
		arb = 50;
		end_cpu = 24;
	}
#endif

	while (!cpu_rsp && arb--) {
		for(cpu_add = 0; cpu_add < end_cpu; cpu_add++) {
			/*load a the serial add, but don't modify the PEN_D,E settings*/
			/* check for an error causing a power down */
			if (pwr_dwn_state == PWR_PARTIAL) {
				arb = 0;
				abort = 1;
				break;
			}

			ptod->nv_ser_add = (ptod->nv_ser_add & 0xC0) | (~cpu_add & 0x3F);
			*L_SER_ADD = ptod->nv_ser_add;
			if(!(PORTD & TERMINATOR)) {
				sprintf(boot_prog,"BOOT ARBITRATION\nIN PROGRESS    %1X:%1X",
					cpu_add >> 2, cpu_add & 0x3);
				fpprint(boot_prog, DSP_INIT);
			}

			timeout(CPU_PROC, BOOT_DELAY, MSEC_30);
			msg = receive(CPU_PROC); 

			/*	if the message is not the BOOT_DELAY message then cancel
			 *	the timeout, place the received message back on the queue
			 *	and terminate the boot arbitration.
			 */
			if (msg != BOOT_DELAY) {
				cancel_timeout(CPU_PROC, BOOT_DELAY);
				send(CPU_PROC, msg);
				abort = 1;
				arb = 0;
				break;
			}

			/* check for a front panel switch closure */
			if(active_switch & (IRQ_MENU|IRQ_SCRLUP|IRQ_SCRLDN|IRQ_EXECUTE)) {
				abort = 1;
				arb = 0;
				break;
			}

			if(ptod->nv_irix_sw & 0x80)   
				/* if in manufacturing mode */
				addq(outq, 'P' + (cpu_add & 0x3));
			else 
				addq(outq, '@' + (cpu_add & 0x3));

			SCCR2 |= SCCR2_TIE;				/* enable TX interrupt */ 



			timeout(CPU_PROC, BOOT_DELAY, MSEC_100); 
			msg = receive(CPU_PROC); 
			/*	if the messeage is not the BOOT_DELAY message then cancel
			 *	the timeout, place the received message back on the queue
			 *	and terminate the boot arbitration.
			 */
			if (msg != BOOT_DELAY) {
				cancel_timeout(CPU_PROC, BOOT_DELAY);
				send(CPU_PROC, msg);
				abort = 1;
				arb = 0;
				break;
			}

			/* check for a front panel switch closure */
			if(active_switch & (IRQ_MENU|IRQ_SCRLUP|IRQ_SCRLDN|IRQ_EXECUTE)) {
				abort = 1;
				arb = 0;
				break;
			}

			/* if expected reponse echo it back to the CPU and then exit*/
			if(cpu_rsp) {
				rx_mode = ST_RDY;
				addq(outq, RSP_CHAR);
				SCCR2 |= SCCR2_TIE;				/* enable TX interrupt */ 
				if (PORTD & TERMINATOR) { 
					status_fmt(notitle, ACT_STATUS, CY_SCROLL, SCREEN_TO);
					sprintf(boot_prog,"ARBITRATION COMPLETE SLOT 0x%1X PROC 0x%1X\r",cpu_add >> 2, cpu_add & 0x3);
				}
				else
					sprintf(boot_prog,"ARBITRATION COMPLETE\nSLOT 0x%1X PROC 0x%1X",cpu_add >> 2, cpu_add & 0x3);
				fpprint(boot_prog, DSP_INIT);
				arb = 0;
				/* get ready for CPU performance data */
				screen_sw(ACT_PERF, NO_TIMEOUT);
				break;
			}
		}
	}
	if (abort) {
		status_fmt(notitle, ACT_STATUS, CY_SCROLL, NO_TIMEOUT);
		sprintf(boot_prog,"ARBITRATION ABORTED\r");
		fpprint(boot_prog, DSP_INIT);
		rx_mode = ST_RDY;
	}
	else if (!cpu_rsp) {
		status_fmt(notitle, ACT_ERROR, CY_SCROLL, NO_TIMEOUT);
		if (PORTD & TERMINATOR) 
			strcpy(boot_prog, "BOOT IS INCOMPLETE FAULT IS NO MASTER\r");
		else
			strcpy(boot_prog, "BOOT IS INCOMPLETE\nFAULT IS NO MASTER");
		fpprint(boot_prog, DSP_INIT);
		log_event(BOOT_ERROR, NO_FP_DISPLAY);
		rx_mode = ST_RDY;
	}
	flush(inq);
	flush(outq);
}

/* 4/12/93 - 	Added a SWI restart CPU Warning
 */
void
cpu_cmd(class, type)
char	class;					/* command class: Alarm or Warning */
char	type;					/* the type of alarm or Warning */
{

	char 			cmd_buf[3];
	char 			bad_cmd;
	struct cbufh 	*outqp;				/* pointer to receiver queue */


	outqp = &sci_outq;
	bad_cmd = 0;
	switch(class) {
		case	CPU_ALARM:
			cmd_buf[0] = 'A';
			switch (type) {
				case	ALM_TEMP:				/* Over Temperature Failure */
					cmd_buf[1] = 'T';
					break;
				case	ALM_BLOWER:				/* Blower Failure */
					cmd_buf[1] = 'B';
					break;
				case	ALM_OFF:				/* Switch Off Condition */
					cmd_buf[1] = 'O';
					break;
				default:
					log_event(BAD_ALM_TYPE, NO_FP_DISPLAY);
					bad_cmd = 1;
					break;
			}
			break;
		case	CPU_WARNING:
			cmd_buf[0] = 'W';
			switch (type) {
				case	WRN_COP:				/* COP Timer Failure */
					cmd_buf[1] = 'T';
					break;
				case	WRN_OSC:				/* Sys Cntl Osc Failure */
					cmd_buf[1] = 'C';
					break;
				case	WRN_IOT:				/* Sys Cntl Illegal Op code */
					cmd_buf[1] = 'I';
					break;
				case	WRN_BLOWER:				/* Blower RPM out of Tolerance*/
					cmd_buf[1] = 'B';
					break;
				case	WRN_VOLT:				/* Voltage Monitor Warning */
					cmd_buf[1] = 'V';
					break;
				case	WRN_SWI:				/* SWI Restart Warning */
					cmd_buf[1] = 'S';
					break;
				default:
					log_event(BAD_ALM_TYPE, NO_FP_DISPLAY);
					bad_cmd = 1;
					break;
			}
			break;
		default:
			log_event(BAD_WARNING, NO_FP_DISPLAY);
			bad_cmd = 1;
			break;
	}

	if (!bad_cmd) {
		cmd_buf[2] = 0;
		putcmd(outqp, cmd_buf);
	}
}
