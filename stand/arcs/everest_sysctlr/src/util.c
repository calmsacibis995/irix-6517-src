#include	<string.h>
#include	<stdio.h>
#include	<exec.h>
#include	<proto.h>
#include	<sp.h>
#include	<sys_hw.h>
#include	<io6811.h>

const char	*err_msgs[] = {
	" ", 
	"SCI SERIAL COMM",
	"SPI TRANSFER",
	"PULSE ACCU INPUT",
	"PULSE ACCU OVERFLOW",
	"TIMER OVERFLOW",
	"TIMER OUT COMP 5",
	"TIMER OUT COMP 4",
	"TIMER OUT COMP 3",
	"TIMER OUT COMP 2",
	"TIMER OUT COMP 1",
	"TIMER IN COMP 3",
	"TIMER IN COMP 2",
	"TIMER IN COMP 1",
	"REAL TIME INTERRUPT",
	"INTERRUPT REQUEST",
	"EXTEND INT REQUEST",
	"SOFTWARE INTERRUPT",
	"ILLEGAL OPCODE TRAP",
	"COP FAILURE",
	"COP MONITOR FAILURE",
	"MEMORY FAILURE",
	"STACK FAULT PID # 0",
	"STACK FAULT PID # 1",
	"STACK FAULT PID # 2",
	"STACK FAULT PID # 3",
	"STACK FAULT PID # 4",
	"STACK FAULT PID # 5",
	"STACK FAULT PID # 6",
	"SYSTEM ON",
	"SYSTEM OFF",
	"SYSTEM RESET",
	"NMI",
	"AC FAIL",
	"POKA FAIL",
	"POKB FAIL",
	"POKC FAIL",
	"AMBIENT OVER TEMP",
	"BRD/CHASSIS OVR TEMP",
	"POWER FAIL WARNING",
	"NO SYSTEM CLOCK",
	"1.5V OVER VOLTAGE",
	"1.5VDC UNDER VOLTAGE",
	"5VDC OVER VOLTAGE",
	"5VDC UNDER VOLTAGE",
	"12VDC OVER VOLTAGE",
	"12VDC UNDER VOLTAGE",
	"-5.2VDC OVER VOLTAGE",
	"-5.2V UNDER VOLTAGE",
	"-12VDC OVER VOLTAGE",
	"-12VDC UNDER VOLTAGE",
	"48VDC OVER VOLTAGE",
	"48VDC UNDER VOLTAGE",
	"FP CONTROLLER FAULT",
	"BLOWER A FAILURE",
	"BLOWER B FAILURE",
	"BAD MSG: SYS_MON",
	"BAD MSG: SEQUENCER",
	"SCLR DETECTED",
	"BLOWER FAILURE",
	"SCI OVERRUN ERROR",
	"SCI NOISE ERROR",
	"SCI FRAMING ERROR",
	"BAD MSG: CPU PROC",
	"BOOT ERROR",
	"INVALID CPU COMMAND",
	"CPU NOT RESPONDING",
	"BAD MSG: DISPLAY",
	"POKD FAIL",
	"POKE FAIL",
	"ACIA OVERRUN ERROR",
	"ACIA NOISE ERROR",
	"ACIA FRAMING ERROR",
	"BAD MSG: POK CHK",
	"BAD WARNING / ALARM",
	"BAD ALARM TYPE",
	"BAD WARNING TYPE",
	"FP READ FAULT",
	"BLOWER RPM FAILURE",
	"TEMP SENSOR FAILURE",
	"BLOWER A RPM FAIL",
	"BLOWER B RPM FAIL",
	"POWER CYCLE",
	"1.5VDC HIGH WARNING",
	"1.5VDC LOW WARNING",
	"5VDC HIGH WARNING",
	"5VDC LOW WARNING",
	"12VDC HIGH WARNING",
	"12VDC LOW WARNING",
	"-5.2VDC HIGH WARNING",
	"-5.2VDC LOW WARNING",
	"-12VDC HIGH WARNING",
	"-12VDC LOW WARNING",
	"48VDC HIGH WARNING",
	"48VDC LOW WARNING",
	"FP BUTTON STUCK",
	"FREE MSG NODE ERROR",
	"FREE TCB NODE ERROR",
	"DEBUG SWITCH ERROR",
	"BLOWER A RPM LOW",
	"BLOWER B RPM LOW",
	"BLOWER RPM LOW",
	"BLOWER A RPM HIGH",
	"BLOWER B RPM HIGH",
	"BLOWER RPM HIGH",
	"XMITTER 1 TIMEOUT",
	"XMITTER 2 TIMEOUT",
	"A TO D TIMEOUT"
};
const char *date[] = {
	"???",
	"JAN", "FEB", "MAR", "APR", "MAY", "JUN",
	"JUL", "AUG", "SEP", "OCT", "NOV", "DEC"
	};


/****************************************************************************
 *																			*
 *	dsp_event		Display the event on the LCD display along with the     *
 *					time and date of the event								*
 *																			*
 ***************************************************************************/
non_banked char
dsp_event(event)
char	event;				/*Event num. as stored in CMOS 1 - 5 or 0
 							  for the last entered event */
{
 	char			buff[50];
	char			*str_ptr;
	struct RTC		*ptod = (struct RTC *) L_RTC;	/* Pointer to RTC */
	struct EVENT	*pevt; 							/* Pointer to event log*/
	char			ev_num;							/* event array position */
	char			month;		
	extern char		active_screen;
	extern char		last_screen;
	int				i;


#ifdef STK_CHK
	stk_chk();
#endif


	if (event == 0 && ptod->nv_event_ctr != 0)
		ev_num = ptod->nv_event_ctr - 1;
	else if (event != 0 && ptod->nv_event_ctr >= 0)
		ev_num = event - 1;
	else 		/* error condition */
		return(-1);

	pevt = &ptod->events[ev_num];

	/*copy event message to buffer*/
	strcpy(buff, err_msgs[pevt->nv_ev_code]);

	/*now format the string with the proper time and date info */

	/*find the end of the string and insert a <CR> for a HD display
	  or a space for a CY display*/
	if (PORTD & TERMINATOR) {
		for(i = strlen(buff); i < 21; i++)
			buff[i] = ' ';
		buff[i] = 0;
		str_ptr = strchr(buff, 0);
	}
	else {
		str_ptr = strchr(buff, 0);
		*str_ptr++ = '\n';
	}

	/* read the time and convert from BCD to ASCII*/
	/* Hours */
	*str_ptr++ = ((pevt->nv_ev_time[0] >> 4) & 0x0F) | '0';
	*str_ptr++ = (pevt->nv_ev_time[0] & 0x0F) | '0';
	*str_ptr++ = ':';

	/* Minutes */
	*str_ptr++ = ((pevt->nv_ev_time[1] >> 4) & 0x0F) | '0';
	*str_ptr++ = (pevt->nv_ev_time[1] & 0x0F) | '0';
	*str_ptr++ = ':';

	/* Seconds */
	*str_ptr++ = ((pevt->nv_ev_time[2] >> 4) & 0x0F) | '0';
	*str_ptr++ = (pevt->nv_ev_time[2] & 0x0F) | '0';
	*str_ptr++ = ' ';


	/* Day */
	if (!(PORTD & TERMINATOR))
		*str_ptr++ = ' ';
	*str_ptr++ = ((pevt->nv_ev_time[4] >> 4) & 0x0F) | '0';
	*str_ptr++ = (pevt->nv_ev_time[4] & 0x0F) | '0';
	*str_ptr++ = ' ';
	*str_ptr = 0;

	/* Month */
	/* convert BCD to HEX */
	month = pevt->nv_ev_time[3];
	if (month > 9)
		month -= 6;
	if(month < 1 || month > 12)
		month = 0;
	strcat(str_ptr, date[month]);
	str_ptr = strchr(buff, 0);
	*str_ptr++ = ' ';

	/* Year */
	*str_ptr++ = ((pevt->nv_ev_time[5] >> 4) & 0x0F) | '0';
	*str_ptr++ = (pevt->nv_ev_time[5] & 0x0F) | '0';

	if (PORTD & TERMINATOR)
		*str_ptr++ = '\r';
	*str_ptr++ = 0;

	if (PORTD & TERMINATOR) {
		cy_str(buff);
	}
	else
		fpprint(buff, DSP_INIT);
	return(0);
 }

/****************************************************************************
 *																			*
 *	init_port		Sets the data direction bits for the I/O ports that     *
 *					must be initialized by software.						*
 *																			*
 ***************************************************************************/
 non_banked void
 init_port()
 {

	/* Outputs: bits 3, 4, 5, 6;  Inputs: 0, 1, 2, 7  */
 	DDRA = 0x78;

	/* Outputs: bits 1;  Inputs: 0, 2, 3, 4, 5, 6, 7  */
	DDRD = 0x02;
 }

/**************************************************************************
 * 																		  *
 *	Log_event:		Event codes passed in are stored in a non-volatile    *
 *					RAM buffer, along with the Hours Min, Month Day and   *
 *					Year.  The buffer capacity is 5 events.  If the 	  *
 *					buffer is full the last event will be over written    *
 *					to ensure the capturing of the last event.	The       *
 *					time and date information is stored in BCD format     *
 * 																		  *
 *************************************************************************/
non_banked void 
log_event(event, dsp_flag)
char	event;
char	dsp_flag;				/* dsplay to front panel if "1" */
{
	struct RTC	*ptod = (struct RTC *) L_RTC;		/* Pointer to RTC */
	struct EVENT *pevt;
	int			counter;
	char		notitle[1] = {0};


#ifdef STK_CHK
	stk_chk();
#endif

	if (ptod->nv_sanity != 0x5A) {	/*check to see if this is first write */
		ptod->nv_sanity = 0x5A;
		counter = 1;
	}
	else
		counter = ptod->nv_event_ctr + 1;

	/* always capture the last event, unless the message is SYS_ON and the
	   log if full */
	if(counter > ERROR_MAX)
		counter = ERROR_MAX;

	pevt = &ptod->events[0];		/* set pointer to start of events */
	pevt += (counter - 1);			/* increment it to the new location */

	pevt->nv_ev_code = event;			/* store the event in NV RAM */

	pevt->nv_ev_time[0] = ptod->t_hour;  /* and store the time of the event */
	pevt->nv_ev_time[1] = ptod->t_min;
	pevt->nv_ev_time[2] = ptod->t_sec;
	pevt->nv_ev_time[3] = ptod->t_mon;
	pevt->nv_ev_time[4] = ptod->t_day;
	pevt->nv_ev_time[5] = ptod->t_year;

	/*update the counter and return */
	ptod->nv_event_ctr = counter;

	if(dsp_flag == 1) {
		ptod->nv_lcd_cntl &= ~FAULT_LED_L; 			/* Turn on the fault led */
		status_fmt(notitle, ACT_ERROR, CY_SCROLL, NO_TIMEOUT);
		dsp_event(0);
	}

	return;
}

/**************************************************************************
 * 																		  *
 *	Log_read:		Event codes and corresponding time and date are read  *
 *					from non-volatile memory in a LIFO style, if e_num is *
 *					0.  Otherwise the log entry as defined by e_num is    *
 *					read.  If e_num does not equal to 0, than req_count   *
 *					is force to 1.  The parameter count determine how     *
 *					many events are read. If count is 0 then all events   *
 *					are read and passed out in the msg_buff.              *
 *					The output buffer has the following format:			  *
 *																		  *
 *				   "event 1 text, timedate;event 2 text time/date;..."    *
 *																		  *
 *					The time/date format is HHMMSSMMDDYY				  *
 *											| | | | | |					  *
 *											| | | | | --> Year			  *
 *											| | | | ----> Day			  *
 *											| |	| ------> Month			  *
 *											| | --------> Seconds		  *
 *											| ----------> Minutes		  *
 *											------------> Hours			  *
 *																		  *
 *					The maximum buffer size for 5 events is 170 bytes     *
 *					The number of events read from the log is returned    *
 *					to the calling function.							  *
 *																		  *
 *************************************************************************/
non_banked char 
log_read(msg_buf, e_num, req_count, clear)
char	*msg_buf;
char	e_num;				/* starting logged entry number */
char	req_count;				/* Number of events requested (0) is all*/
char	clear;					/* 1- clear log after read */
{

	char 			count;			/* Number of events to be returned */
	char 			buff[13];
	struct RTC		*ptod = (struct RTC *) L_RTC;	/* Pointer to RTC */
	struct EVENT 	*pevt;
	int				i,j,k;			/* loop varibles */

	pevt = &ptod->events[0];		/* set pointer to start of events */
	buff[12] = 0;					/* NULL terminate the buffer string */

	/* check to see if request count is larger than the number
	   of events stored, and see if the reqest is for the entire
	   buffer
	 */

	if (ptod->nv_event_ctr == 0)
		return(0);
	if (req_count > ptod->nv_event_ctr || req_count == 0)
		count = ptod->nv_event_ctr;
	else
		count = req_count;

	/* if e_num is greater than 0 than reg_count is forced to be 1 */
	if(e_num > 0) 
		count == 1;
	if(e_num > ptod->nv_event_ctr)
		return(0);

	/* set pointer for disired starting event */
	pevt = &ptod->events[0];		/* set pointer to start of events */
	pevt += (count - 1);
	msg_buf[0] = 0;
	for(i = count -1; i >= 0; i--, pevt--) {
		strcat(msg_buf, err_msgs[pevt->nv_ev_code]);
		strcat(msg_buf, ",");
		/* convert time from BCD to ASCII */
		for(j = 0, k = 0; j < 6; j++, k += 2) {
			buff[k] = ((pevt->nv_ev_time[j] >> 4) & 0x0F) | '0';
			buff[k+1] = (pevt->nv_ev_time[j] & 0x0F) | '0';
		}
		strcat(msg_buf, buff);
		strcat(msg_buf, ";");
	}

	/* if the clear flag is set, clear the event counter */
	if (clear)
		ptod->nv_event_ctr = 0;

	return(count);
}



/*
	rtc_rd -
		Read real time clock. Outputs are a pointer to the buffer to
		write the ascii time of day and a byte count. 'abuf' must be 
		at least 'count' bytes long. 'count' must be even. The input
		date is converted from ASCII to BCD format before being written
		to the RTC chip.  The input buffer format is "MMDDYYHHMMSS"


		When I get the documentation I might want to make several reads
		to be prevent returning a value taken during the updating process
*/

non_banked void
rtc_rd(abuf, count)
char	*abuf;		/* Pointer to ascii buffer */
char	count;		/* Number of ascii bytes to read */
{
	struct RTC	*ptod = (struct RTC *) L_RTC;		/* Pointer to RTC */
	int			i, j;							/* Loop index variables */
	char 		tbuf[6];
	char 		loop_cnt;

	/* Before reading the time lets make sure an update is not
	   occurring as we are reading.  However, we also do not want
	   to be in an endless loop in the event of a failed RTC chip
	 */
	loop_cnt = 0;
	while((loop_cnt++ < 50) && (ptod->t_rega & RTC_UIP));

	disable_interrupt();
	tbuf[0] = ptod->t_mon;
	tbuf[1] = ptod->t_day;
	tbuf[2] = ptod->t_year;
	tbuf[3] = ptod->t_hour;
	tbuf[4] = ptod->t_min;
	tbuf[5] = ptod->t_sec;
	enable_interrupt();

	for(i=0,j=0; j < count; i++,j+=2) {
		abuf[j] = ((tbuf[i] >> 4) & 0x0F) | '0';
		abuf[j+1] = (tbuf[i] & 0x0F) | '0';
	}
}

/*
	rtc_wr -
		Write real time clock. Inputs are a pointer to the buffer to
		write the ascii time of day and a byte count. 'abuf' must be 
		at least 'count' bytes long, 'count' must be even. The input
		date is converted from ASCII to BCD format before being written
		to the RTC chip.  The input buffer format is "MMDDYYHHMMSS"
 */

non_banked void
rtc_wr(abuf, count)
char	*abuf;							/* Pointer to ascii buffer */
char	count;							/* Number of ascii bytes to write */
{
	struct RTC	*ptod = (struct RTC *) L_RTC;			/* Pointer to RTC */

	ptod->t_regb = RTC_SETBIT | RTC_SQWE | RTC_24H | RTC_BCD;  

	if(count >= 2)
		ptod->t_mon  = (((abuf[0] & 0x0F) << 4)  | (abuf[1] & 0x0F));
	if(count >= 4)
		ptod->t_day  = (((abuf[2] & 0x0F) << 4)  | (abuf[3] & 0x0F));
	if(count >= 6)
		ptod->t_year = (((abuf[4] & 0x0F) << 4)  | (abuf[5] & 0x0F));
	if(count >= 8)
		ptod->t_hour = (((abuf[6] & 0x0F) << 4)  | (abuf[7] & 0x0F));
	if(count >= 10)
		ptod->t_min  = (((abuf[8] & 0x0F) << 4)  | (abuf[9] & 0x0F));
	if(count >= 12)
		ptod->t_sec  = (((abuf[10] & 0x0F) << 4) | (abuf[11] & 0x0F));

	ptod->t_regb &= ~RTC_SETBIT;					/* Enable Clock Updates */
}


/************************************************************************
 *																		*
 *	GET_CHECK:		Retrieves current state comand length check flag.	*
 *																		*
 ************************************************************************/
non_banked void
get_check(buff, seq)
char	*buff,							/* pointer to response buffer */
		seq;							/* command sequence number */
{
	struct RTC	*ptod = (struct RTC *) L_RTC;			/* Pointer to RTC */

	buff[0] = 'R';
	buff[1] = seq;
	buff[2] = 'C';
	if(ptod->nv_cmd_chk > 0)
		buff[3] = 'Y';
	else
		buff[3] = 'N';
	buff[4] = 0;
}

/************************************************************************
 *																		*
 *	GET_DEBUG_SW:	Retrieve the IRIX debug switch setting from 		*
 *					non-volatile memory, and build a CPU command		*
 *					response.											*
 *																		*
 *		4/13/93 -	Added a changed to check for corrupted debug switch *
 *					settings.  If corrupted, then store a "0" in the 	*
 *					location, and post an error.						*
 *																		*
 ************************************************************************/
non_banked void
get_debug_sw(buff, seq)
char	*buff,							/* pointer to response buffer */
		seq;							/* command sequence number */
{
	struct RTC	*ptod = (struct RTC *) L_RTC;			/* Pointer to RTC */

	buff[0] = 'R';
	buff[1] = seq;
	buff[2] = 'D';
	/* make sure the debug switch settings are not corrupted */
	if ((ptod->nv_irix_sw ^ ptod->nv_irix_sw_not) != 0xFFFF) {
		ptod->nv_irix_sw = 0;
		ptod->nv_irix_sw_not = 0xFFFF;
		log_event(DBSW_ERR, NO_FP_DISPLAY);
	}
	sprintf(&buff[3],"%04X", ptod->nv_irix_sw);
}

/************************************************************************
 *																		*
 *	GET_ENV_DATA:	Retrieve the "environmental" data from the system	*
 *					controller and build a CPU command					*
 *					response.											*
 *																		*
 *					All data is converted to ASCII format.  The 		*
 *					response includes and is ordered as follows:		*
 *					o 48 Volts 											*
 *					o 12 Volts											*
 *					o 5 Volts											*
 *					o 1.5 Volts											*
 *					o -5.2 Volts										*
 *					o -12 Volts											*
 *					o Terminator Temp A / Eveready Temp					*
 *					o Terminator Temp B / Not valid for Eveready		*
 *					o Terminator Blower A RPM / Eveready Blower RPM		*
 *					o Terminator Blower B RPM / Not valid for Eveready	*
 *					o Blower Control D/A Setting						*
 *																		*	
 *			4/23/93 -	Added now environmental data to the information *
 *						passed to the CPU.
 ************************************************************************/
non_banked void
get_env_data(buff, seq)
char	*buff,							/* pointer to response buffer */
		seq;							/* command sequence number */
{
	extern struct ENV_DATA	env;	/*data structure for environmental data */
	struct 	ENV_DATA		*p;		/* pointer to env data sturcture */
	p = &env;

	buff[0] = 'R';
	buff[1] = seq;
	buff[2] = 'E';
	sprintf(&buff[3],"%2.0f;%2.1f;%1.2f;%1.2f;",p->V48, p->V12, p->V5, p->V1p5);
	sprintf(&buff[strlen(buff)],"%1.1f;%2.1f;", p->VM5p2, p->VM12);
	sprintf(&buff[strlen(buff)],"%.0f;", p->temp_a);
	sprintf(&buff[strlen(buff)],"%.0f;%.0f;", p->blow_a_tac, p->blow_b_tac);
	sprintf(&buff[strlen(buff)],"%2X;", p->blower_out);
	strcat(buff, p->fwver);
	strcat(buff,";");
	sprintf(&buff[strlen(buff)],"%.0f;%.0f;", p->max_temp, p->min_temp);
	sprintf(&buff[strlen(buff)],"%.0f;%.0f;", p->max_rpm_a, p->min_rpm_a);
	sprintf(&buff[strlen(buff)],"%.0f;%.0f;", p->max_rpm_b, p->min_rpm_b);
	sprintf(&buff[strlen(buff)],"%d;%d;", p->con_rpm_a_err, p->total_rpm_a_err);
	sprintf(&buff[strlen(buff)],"%d;%d;", p->con_rpm_b_err, p->total_rpm_b_err);
	sprintf(&buff[strlen(buff)],"%d;%d;", p->num_free_msg, p->min_free_msg);
	sprintf(&buff[strlen(buff)],"%d;%d;", p->num_free_tcb, p->min_free_tcb);
	sprintf(&buff[strlen(buff)],"%d", p->invalid_cpu_cmd);
}

/************************************************************************
 *																		*
 *	GET_LOG:		Retrieve the system event log, and build a CPU 		*
 *					command	response.  The event log is cleared after	*
 *					the log is read.									*
 *																		*
 ************************************************************************/
non_banked void
get_log(buff, seq)
char	*buff,							/* pointer to response buffer */
		seq;							/* command sequence number */
{
	struct RTC	*ptod = (struct RTC *) L_RTC;			/* Pointer to RTC */

	buff[0] = 'R';
	buff[1] = seq;
	buff[2] = 'L';
	if(ptod->nv_cmd_chk > 0)
		log_read(&buff[3], 0, 0, 0);
	else
		log_read(&buff[3], 0, 0, 1);
}

/************************************************************************
 *																		*
 *	GET_LCD:		Retrieve the type of LCD panel present.        		*
 *					The response is a "T" for the terminator display   	*
 *					and an "E" for the Eveready display.				*	
 *																		*
 ************************************************************************/
non_banked void
get_lcd(buff, seq)
char	*buff,							/* pointer to response buffer */
		seq;							/* command sequence number */
{
	buff[0] = 'R';
	buff[1] = seq;
	buff[2] = 'P';
	if(PORTD & TERMINATOR)
		buff[3] = 'T';
	else
		buff[3] = 'E';
	buff[4] = 0;
}

/************************************************************************
 *																		*
 *	GET_SER_NO:		Retrieve the system serial number, and build a CPU 	*
 *					command	response.									*
 *																		*
 *																		*
 *		4/26/93 -	Added a check sum to the serial number.
 ************************************************************************/
non_banked void
get_ser_no(buff, seq)
char	*buff,							/* pointer to response buffer */
		seq;							/* command sequence number */
{
	struct RTC	*ptod = (struct RTC *) L_RTC;			/* Pointer to RTC */
	char		*sptr = ptod->nv_ser_num;				/* pointer to ser no */
	int			i;										/* loop counter */
	char		chk_sum;

	buff[0] = 'R';
	buff[1] = seq;
	buff[2] = 'S';

	/* Verify the check sum, and return serial number if valid.  If 
		invalid return an empty string */
	for(i = 0, chk_sum = 0, sptr = ptod->nv_ser_num; i < 10; i++)
		chk_sum ^= *sptr++;

	sptr = ptod->nv_ser_num;
	if (chk_sum == ptod->nv_ser_cksum) {
		for(i = 0; i < 10; i++)
			buff[i+3] = *sptr++;
	}
	else {
		buff[3] = 0x0A;
		i = 1;
	}
	buff[i+3] = 0;
}

/************************************************************************
 *																		*
 *	GET_TIME:		Retrieve the time and date from the real time		*
 *					chip, and build the CPU command response.			*
 *																		*
 ************************************************************************/
non_banked void
get_time(buff, seq)
char	*buff,							/* pointer to response buffer */
		seq;							/* command sequence number */
{
	struct RTC	*ptod = (struct RTC *) L_RTC;			/* Pointer to RTC */

	buff[0] = 'R';
	buff[1] = seq;
	buff[2] = 'T';
	rtc_rd(&buff[3], 12);
	buff[15] = 0;
}

non_banked void
set_bt_msg(buff)
char	*buff;							/* pointer to response buffer */
{
	extern char btmsg_buff[41];
	struct PROC_MSG_ST	*win;
	extern struct PROC_MSG_ST	proc_boot_win;

	strncpy(btmsg_buff, &buff[2], 40); 
	btmsg_buff[40] = 0;

	if (PORTD & TERMINATOR) {
		win = &proc_boot_win;
		win->new_msg |= NEW_BT_MSG;
	}
	send(DISPLAY, BOOT_STAT); 
}

/************************************************************************
 *																		*
 *	SET_CHECK:		Toggle the command length checking function.		*
 *					If set then reset it else if reset then set it.		*
 *																		*
 ************************************************************************/
non_banked void
set_check()
{
	struct RTC	*ptod = (struct RTC *) L_RTC;			/* Pointer to RTC */

	if(ptod->nv_cmd_chk == 1)
		ptod->nv_cmd_chk = 0;
	else
		ptod->nv_cmd_chk = 1;
}

/************************************************************************
 *																		*
 *	SET_DEBUG_SW:	Store the IRIX debug switch setting into 			*
 *					non-volatile memory. 								*
 *																		*
 *		4/13/93 -	Added a changed to check for corrupted debug switch *
 *					settings.  If corrupted, then store a "0" in the 	*
 *					location, and post an error.						*
 *																		*
 *		4/21/93	-	Added a check for the correct length.  If incorrect *
 *					the command is ingored.								*
 ************************************************************************/
non_banked void
set_debug_sw(buff)
char	*buff;							/* pointer to input buffer */
{
	struct RTC	*ptod = (struct RTC *) L_RTC;			/* Pointer to RTC */
	int	temp;

	if(strlen(buff) == 6) {
	sscanf(&buff[2],"%x", &temp);
		ptod->nv_irix_sw 		= (unsigned short) temp;
		ptod->nv_irix_sw_not	= (unsigned short) ~temp;
	}
}

/****************************************************************************
 *																			*
 *	SET_CPU_HIST:	converts the received command string to indiviual 		*
 *					non-normalized integer values from 0 to 100%.			*
 *					The display functions for both the 2 line display and 	*
 *					the 8-line display is then responsible for scaling the  *
 *					values for the apporiate display						*
 *																			*
 ****************************************************************************/
non_banked void
set_cpu_hist(buff)
char	*buff;							/* pointer to input buffer */
{
	char		*str_ptr;
	extern signed char	histogram[64];	/* global memory data location */
	extern int			num_proc;
	extern char			active_perf;	/* Active Perf data ready */
	int			i;						/* loop counter */
	int			temp;

	str_ptr = &buff[2];
	for(i = 0; i < 64; i++) {
		sscanf(str_ptr,"%d", &temp);
		histogram[i] = temp;
		str_ptr = strchr (str_ptr, ',');
		if (str_ptr == 0)
			break;
		else
			str_ptr++;
	}
	/* Set Number of Processor varible */
	num_proc = i + 1;

	/* Null out the remaining processor locations */
	for (i++;i < 12; i++) 
		histogram[i] = 0;

	active_perf = 1;
}

non_banked void
set_proc_stat(buff)
char	*buff;							/* pointer to input buffer */
{
	extern char pstat_buff[41];
	extern char btmsg_buff[41];
	struct PROC_MSG_ST	*win;

	strncpy(pstat_buff, &buff[2], 39); 
	btmsg_buff[39] = 0;
	if (PORTD & TERMINATOR) {
		win = &proc_boot_win;
		win->new_msg |= NEW_PROC_MSG;
		send(DISPLAY, BOOT_STAT); 
	}
	else if(btmsg_buff[0] != 0)
		send(DISPLAY, BOOT_STAT); 
}

/************************************************************************
 *																		*
 *	SET_SER_NO:		Store the system serial number			 			*
 *					non-volatile memory. 								*
 *																		*
 *		4/26/93 -	Added a check sum to the serial number.
 ************************************************************************/
non_banked void
set_ser_no(buff)
char	*buff;							/* pointer to input buffer */
{
	struct RTC	*ptod = (struct RTC *) L_RTC;			/* Pointer to RTC */
	char		*sptr = ptod->nv_ser_num;				/* pointer to ser no */
	int			i;										/* loop counter */
	char		chk_sum;

	for(i = 0; i < 10; i++)
		*sptr++ = buff[i+2];

	/* check sum the serial number*/
	for(i = 0, chk_sum = 0, sptr = ptod->nv_ser_num; i < 10; i++)
		chk_sum ^= *sptr++;
	ptod->nv_ser_cksum = chk_sum;

}
