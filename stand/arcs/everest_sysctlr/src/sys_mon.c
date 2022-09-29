
#include	<exec.h>
#include	<proto.h>
#include	<sp.h>
#include	<sys_hw.h>
#include	<io6811.h>
#include	<stdio.h>
#include	<string.h>

struct ENV_DATA		env;	/*data structure for environmental data */

const	float trans_volt[] = {
			0.0195, 0.0391, 0.0781, 0.1562, 0.3125, 0.625, 1.250, 2.500};

/* A to D channel Scale factors */
const	float ad_scale[] = {19.223, 4.741, 2.0, 1.0, -2.150, -4.750, 100, 100}; 

const	float trans_percent[] = {0.39, 0,78, 1.56, 3.12, 6.25, 12.5, 25, 50};

const	float tolerance[6][4] = {
			/* Outer Band		Inner Band */
				42.75,	54.25,	0,		0, 			/*48 Volts Range */
				9.67,	14.32,	10.97,	13.02,		/*12 Volts Range */
				4.13,	5.85,	4.59,	5.46,		/* 5 Volts Range */
				0.988,	1.995,	1.23,	1.77,		/*1.5 Volts Range */
				-6.05,	-3.41,	-5.66,	-4.40,		/*-5.2 Volts Range */
				-14.52,	-7.92,	-13.7,	-10.00		/*-12 Volts Range */
		};

/* The operational blower RPM range is from full speed to 900 RPMs.  Below
	900 RPMs the OLS blower is louder then the blower, on the Eveready.  The
	temperature monitored on the Eveready is actually 4 degrees higher then
	the true ambient temperature.  The Eveready table has been adjusted for the
	measured temperatures.
*/
const struct 	BLOWER_OUTPUT	T_fan_set[16] = {
/*			 	RPM					Min		Max		*/
/*		Lower	Nominal	Upper		Temp	Temp	*/
		1400,	1580,	1800,		0,		9999.999,/* forced to full speed */
		0,		1325,	0,			36,		38.999,
		0,		1175,	0,			33,		35.999,
		0,		1075,	0,			30,		32.999,
		0,		950,	0,			27,		29.999,
		0,		900,	0,			0,		26.999,

		0,		880,	0,			72.5,	76.99,
		0,		845,	0,			68,		72.49,
		0,		730,	0,			63.5,	67.99,
		0,		715,	0,			59,		63.49,
		0,		710,	0,			54.5,	58.99,
		0,		700,	0,			50,		54.49,
		0,		680,	0,			45.5,	49.99,
		0,		665,	0,			41,		45.49,
		0,		660,	0,			36.5,	40.99,
		0,		650,	0,			0,		36.49
};

const struct 	BLOWER_OUTPUT	E_fan_set[16] = {
/*			 	RPM					Min		Max		*/
/*		Lower	Nominal	Upper		Temp	Temp	*/
		1279,	1390, 	1668,		39,		9999.999,
		1053,	1145, 	1374,		31,		38.999,
		938,	1020, 	1224,		26,		30.999,
		851,	923,	1110,		0,		25.999,

		0,		950, 	0,			27,		29.999,
		0,		900, 	0,			0,		26.999,
		0,		880, 	0,			72.5,	76.99,
		0,		845, 	0,			68,		72.49,
		0,		730, 	0,			63.5,	67.99,
		0,		715, 	0,			59,		63.49,
		0,		710, 	0,			54.5,	58.99,
		0,		700, 	0,			50,		54.49,
		0,		680, 	0,			45.5,	49.99,
		0,		665, 	0,			41,		45.49,
		0,		650, 	0,			36.5,	40.99,
		0,		0,	 	0,			0,		36.49
};

#define MAX_TEMP			50			/* maximum allowable incoming temp */
#define MAX_BLOW_PERCENT	25			/* Max percentage difference between
										   set and measured rpm */
char			active_off;

/****************************************************************************
 *																			*
 *		Blower_init:	Initilize the fan interrupt routines.				*
 *																			*
 ****************************************************************************/
non_banked void
blower_init()
{
	extern struct		BLOWER_TACH	fan_a_per;
	extern struct		BLOWER_TACH	fan_b_per;
	struct BLOWER_TACH	*ptr;

	/* Initialize Interrupt routine varibles */
	ptr = &fan_a_per;
	ptr->pulse_1 = 0;
	ptr->pulse_2 = 0;
	ptr->half_period = 0;
	ptr->first_pulse = 0;
	ptr->running = 0;
	ptr->overflow = 0;

	ptr = &fan_b_per;
	ptr->pulse_1 = 0;
	ptr->pulse_2 = 0;
	ptr->half_period = 0;
	ptr->first_pulse = 0;
	ptr->running = 0;
	ptr->overflow = 0;

	/* Set Up the input capture for positive going edges */
	TCTL2 = 0x05;

	/* Enable the Interrupts */

	TMSK2 |= 0x80;				/* Timer Overflow Interrupt Enable */

	if(PORTD & TERMINATOR) 
		TMSK1 = 0x3;
	else
		TMSK1 = 0x1;

}

/****************************************************************************
 *																			*
 *	SYS_MON:	Monitors the system voltages, temperature, and Blower 		*
 *				RPM.														*
 *																			*
 *				4/14/93 -	Change the routine so that a 48V over voltage	*
 *							cause a total shutdown of the system.			*
 *																			*
 *							Made a change, so the firmware will shutdown	*
 *							the system if it detects a voltage failure		*
 *							on the first pass through "MON_RUN_ONCE"		*
 *																			*
 *				4/21/93 -	Increased the number of consecutive invalid		*
 *							RPM readings from 30 to necessary to cause		*
 *							a blower RPM failure.  Increased the number of	*
 *							below tolerance RPM readings required before	*
 *							shuting the system down to 30.					*
 *																			*
 *							Modified the RPM calculation from using a 2 us	*
 *							resolution timer to an 8 us resolution timer	*
 *							to reduce the number of overflows.				*
 *																			*
 *				4/23/93 -	Added a min and max temperature to the 			*
 *							environmental data structure.  Also moved		*
 *							monitors for Blower rpm errors to the data		*
 *							structures.										*
 *																			*
 *				5/11/93 -   Corrected a previous error, on reporteing a		*
 *							Blower RPM High Warning more than once.  Added  *
 *							unique messages for the Blower RPM High 		*
 *							warnings.
 ****************************************************************************/
non_banked void
sys_mon()
{
	struct ENV_DATA		*env_ptr;
	struct	SYS_TEMP	temp_a;
	struct	SYS_TEMP	*ptr_a;
	struct 	RTC	*ptod = (struct RTC *) L_RTC;	/* Pointer to RTC */
	extern struct		BLOWER_TACH	fan_a_per;
	extern struct		BLOWER_TACH	fan_b_per;
	struct BLOWER_TACH	*fint_ptr;	/*interrupt data structure ptr */
	struct 				BLOWER_OUTPUT	*fan_ptr;
	extern const char	cmd_mode[2];
	extern const char	dsp_mode[2];
	extern char 		pwr_dwn_state;
	extern unsigned char rx_mode;
	extern char			fw_ver[6];
	char				shutdown_mode;
	char				*sptr;

	float				ad_buff[12],
						rpm,
						int_temp,
						temp,
						*vptr;

	int					ad_temp,
						ad_channel,
						voltage_err,
						v48_fault,
						rpm_check,
						fan_a_err,
						fan_b_err,
						rpm_a_cnt,
						rpm_b_cnt,
						rpm_a_err,
						rpm_b_err,
						rpm_chk_a,				/* rpm check counter */
						rpm_chk_b,				/* rpm check counter */
						ovr_temp_err,
						i,j,y,
						pass,
						fan_a_hi,				/* RPMs above tolerance */
						fan_b_hi,				/* RPMs above tolerance */
						f_index_a,
						f_index_b,
						loop;

	char				*result_reg,
						sensor_err,
						buff[41],
						fan_a,
						fan_adjust,
						voltage_wrn,
						voltage_wrn_r;

	unsigned short	msg;
#ifdef LAB
	extern char active_switch;
	extern int fan_step;
	int			fan_dtoa;
#endif
	extern char active_screen;

	ptr_a = &temp_a;

	/* initialize the temperature buffer queues */
	ptr_a->index = 0;
	ptr_a->entries = 0;
	ovr_temp_err = 0;
	sensor_err = 0;
	fan_a_err = 0;
	fan_b_err = 0;
	voltage_err = 0;
	voltage_wrn = 0;
	voltage_wrn_r = 0;
	v48_fault = 0;
	rpm_check = 0;
	rpm_a_cnt = 0;
	rpm_b_cnt = 0;
	rpm_chk_a = 0;
	rpm_chk_b = 0;
	f_index_a = 0;
	f_index_b = 0;
	fan_a_hi = 0;
	fan_b_hi = 0;
#ifdef LAB
	fan_dtoa = 0x10;
	fan_step = 1;
#endif

	/* initialize the data sturcture */
	env_ptr = &env;
	env_ptr->V48 = 0;
	env_ptr->V12 = 0;
	env_ptr->V5 = 0;
	env_ptr->V1p5 = 0;
	env_ptr->VM5p2 = 0;
	env_ptr->VM12 = 0;
	env_ptr->temp_a = 0;
	env_ptr->blow_a_tac = 0;
	env_ptr->blow_b_tac = 0;
	env_ptr->blower_out = 0;
	env_ptr->con_rpm_a_err = 0;
	env_ptr->con_rpm_b_err = 0;
	env_ptr->total_rpm_a_err = 0;
	env_ptr->total_rpm_b_err = 0;
	env_ptr->max_rpm_a = 0;
	env_ptr->min_rpm_a = 20000;
	env_ptr->max_rpm_b = 0;
	if(PORTD & TERMINATOR)
		env_ptr->min_rpm_b = 20000;
	else
		env_ptr->min_rpm_b = 0;
	env_ptr->max_temp = 0;
	env_ptr->min_temp = 100;
	strcpy(env_ptr->fwver, fw_ver);



	for (;;) {
		msg = receive(MONITOR); 

		if(msg != MON_RUN && msg != MON_IDLE &&  msg != RPM_CHECK_TO &&
			msg != MON_RUN_ONCE) {
			log_event(BMSG_SYSMON, NO_FP_DISPLAY);
			continue;
		}

		if (msg == RPM_CHECK_TO) {
			rpm_check = 1;
			continue;
		}

		/* kill the running of the process if in boot mode */
		if(rx_mode == ST_BOOT)
			continue;

		/* don't continue if in a partial power down, and the message
		   is for a normal run */
		if (pwr_dwn_state == PWR_PARTIAL && msg == MON_RUN)
			continue;

		shutdown_mode = ptod->nv_irix_sw & 0x1000 ? 0 : 1;

		/* Record Voltage Readings and Blower Speed*/
		for(ad_channel = 0, vptr = &env.V48; ad_channel < 7; ad_channel ++) {
			j = 0;
			for(pass = 0; pass < 3; pass++) {
				/*set channel for read */
				ADCTL = ad_channel;

				/*wait for conversion completion flag*/
				loop = 1000;
				while(!(ADCTL & 0x80) && loop--);

				if(loop <= 0)
					log_event(ATOD_TO, FP_DISPLAY);

				/* translate the Voltage and Temperature readings */
				result_reg = &ADR1;
				for(i = 0; i < 4; result_reg++, i++, j++) {
					ad_temp = *result_reg;
					/* go through each bit */
					for (ad_buff[j] = 0, y = 0; y < 8; y++) 
						if(ad_temp & (1 << y)) 
							ad_buff[j] +=  trans_volt[y];
				}
				delay(SW_2MS);
			}

			/* now average and store the results */
			if (ad_channel < 6) {
				for (i = 0, *vptr = 0; i < 12; i++) 
					*vptr += ad_buff[i];
				*vptr /= 12;
				*vptr++ *= ad_scale[ad_channel];
			}

			else if (ad_channel == 6) {		
				/*store temperature in circular queues */
				/* Incoming Temperature A */
				/* average the 12 readings and divide by 10mv / degree F. */
				for (i = 0, ptr_a->temp[ptr_a->index] = 0; i < 12; i++) 
					ptr_a->temp[ptr_a->index] += ad_buff[i];
				ptr_a->temp[ptr_a->index] /= 12;
				/* check for the reading to be between 0 and 300 degrees */
				if (ptr_a->temp[ptr_a->index] >= 0 && 
					ptr_a->temp[ptr_a->index] <= 3) 

					/* scale the reading */
					ptr_a->temp[ptr_a->index] *= ad_scale[ad_channel];

				else {
					/* jam a max reading to prevent problems in the sprintf
						lib. converting an invalid floating point value to a
						string */
					ptr_a->temp[ptr_a->index] *= 9999;
					if(!sensor_err) {
						/* only log error once */
						log_event(TEMP_SENSOR_ERR, FP_DISPLAY);
						sensor_err = 1;
					}
				}



				int_temp = (ptr_a->temp[ptr_a->index] - 32) * 5 / 9;

				/* adjust index */
				if (ptr_a->index == TEMP_LEN-1)
					ptr_a->index = 0;
				else 
					ptr_a->index++;

				/* adjust entry number */
				if (ptr_a->entries != TEMP_LEN)
					ptr_a->entries++;
			}
		}

		/* Calculate the running averages for the temperatures readings */
		env_ptr = &env;
		for (env_ptr->temp_a=0, i = 0; i < ptr_a->entries; i++)
			env_ptr->temp_a += ptr_a->temp[i];
		env_ptr->temp_a /= ptr_a->entries;

		/* now convert the temperature to Celsius */
		env_ptr->temp_a = (env_ptr->temp_a - 32) * 5 / 9;

		/* Record min and max temperatures */
		if (env_ptr->temp_a > env_ptr->max_temp)
			env_ptr->max_temp = env_ptr->temp_a;
		if (env_ptr->temp_a < env_ptr->min_temp)
			env_ptr->min_temp = env_ptr->temp_a;


		/* Adjust the Blower Assemblies if necessary */

		/* BLOWER A */
		if (PORTD & TERMINATOR)
			fan_ptr=T_fan_set;
		else
			fan_ptr=E_fan_set;

#ifdef LAB
if((ptod->nv_irix_sw & 0x0300) != 0x0300) {
#endif
		if(ptod->nv_irix_sw & 0x0800)  {
			if(fan_a != 0x0) {
				rpm_check = 0;
				rpm_chk_a = 0;
				rpm_chk_b = 0;
				cancel_timeout(MONITOR, RPM_CHECK_TO);
				timeout(MONITOR, RPM_CHECK_TO, ONE_MIN);
			}
			fan_a = 0x0;
			env_ptr->blower_out = 0;
			*L_BLOW_SET = (fan_a << 4 | fan_a);
		}
		else {
			/* First Check the Temperature against the current speed Setting */
			fan_a = env_ptr->blower_out & 0x0F;
			fan_ptr += fan_a;
			temp = env_ptr->temp_a;
			if(temp < fan_ptr->low_range ||  temp > fan_ptr->hi_range) {
				/* Find the correct speed setting */
				if (PORTD & TERMINATOR)
					fan_ptr=T_fan_set;
				else
					fan_ptr=E_fan_set;
				for (fan_a = 0; fan_a <= 3; fan_a++, fan_ptr++) {
					if(temp >= fan_ptr->low_range && temp <= fan_ptr->hi_range){
						env_ptr->blower_out = (fan_a << 4) | fan_a;
						*L_BLOW_SET = env_ptr->blower_out;
						rpm_check = 0;
						rpm_chk_a = 0;
						rpm_chk_b = 0;
						f_index_a = 0;
						f_index_b = 0;
						cancel_timeout(MONITOR, RPM_CHECK_TO);
						timeout(MONITOR, RPM_CHECK_TO, ONE_MIN);
						break;
					}
				}
			}
		}

		/* Check to see if the incoming air stream exceeds the maximum
		   allowable temperature, but only report the error once 
		 */
		 if(temp > MAX_TEMP && !ovr_temp_err) {
		 	ovr_temp_err = 1;
			log_event(TEMP_AMB, FP_DISPLAY);
#ifdef LAB
			if (shutdown_mode) {
#endif
				send(SEQUENCE, INCOMING_TEMP_OFF);
#ifdef LAB
			}
#endif
		}

#ifdef LAB
}
#endif

		/* Check the voltages and their tolerances */

		/* 48 VDC Source */

		/* The voltages are check against a dual tolerance band.  If the 
		   readings are outside the first band but within the second band
		   a warning is sent to the CPU, and an event is logged.  No other
		   action is taken.

		   If a reading is outside the second band the event is logged and the
		   system is shutdown.

		   48 Volts is only checked against the outer tolerance band because 
		   the accuracy of the measurement circuitry (+/2.25V) is not good 
		   enough to guarantee a warning before a POK failure is received.
		 */

		/* Outer Tolerance */
		if(env_ptr->V48 < tolerance[0][0]) { 
			if (!(voltage_err & V48_FLAG)) {	/* Only log fault once */
				log_event(V48_LO, FP_DISPLAY);
				voltage_err |= V48_FLAG;
			}
		}
		else if(env_ptr->V48 > tolerance[0][1]) {
			if (!(voltage_err & V48_FLAG)) {	/* Only log fault once */
				log_event(V48_HI, FP_DISPLAY);
				ptod->nv_status |= RESET_FERR;
				shutdown(KILL_ALL);				/* shut down now!! */
			}
		}

		if (msg == MON_RUN || msg == MON_RUN_ONCE) {
			/* 12 VDC Source */
			if(env_ptr->V12 < tolerance[1][0]) {
				if (!(voltage_err & V12_FLAG)) {	/* Only log fault once */
					log_event(V12_LO, FP_DISPLAY);
					voltage_err |= V12_FLAG;
				}
			}
			else if(env_ptr->V12 > tolerance[1][1]) {
				if (!(voltage_err & V12_FLAG)) {	/* Only log fault once */
					log_event(V12_HI, FP_DISPLAY);
					voltage_err |= V12_FLAG;
					ptod->nv_status |= RESET_FERR;
				}
			}

			/* Inner Tolerance */
			else if(env_ptr->V12 < tolerance[1][2]) { 
				if (!(voltage_wrn & V12_FLAG)) {	/* Only log fault once */
					log_event(V12_LO_WARN, FP_DISPLAY);
					voltage_wrn |= V12_FLAG;
				}
			}
			else if(env_ptr->V12 > tolerance[1][3]) {
				if (!(voltage_wrn & V12_FLAG)) {	/* Only log fault once */
					log_event(V12_HI_WARN, FP_DISPLAY);
					voltage_wrn |= V12_FLAG;
				}
			}


			/* 5.0 VDC Source */
			if(env_ptr->V5 < tolerance[2][0]) {
				if (!(voltage_err & V5_FLAG)) {	/* Only log fault once */
					log_event(V5_LO, FP_DISPLAY);
					voltage_err |= V5_FLAG;
				}
			}
			else if(env_ptr->V5 > tolerance[2][1]) {
				if (!(voltage_err & V5_FLAG)) {	/* Only log fault once */
					log_event(V5_HI, FP_DISPLAY);
					voltage_err |= V5_FLAG;
					ptod->nv_status |= RESET_FERR;
				}
			}

			/* Inner Tolerance */
			else if(env_ptr->V5 < tolerance[2][2]) {
				if (!(voltage_wrn & V5_FLAG)) {	/* Only log fault once */
					log_event(V5_LO_WARN, FP_DISPLAY);
					voltage_wrn |= V5_FLAG;
				}
			}
			else if(env_ptr->V5 > tolerance[2][3]) {
				if (!(voltage_wrn & V5_FLAG)) {	/* Only log fault once */
					log_event(V5_HI_WARN, FP_DISPLAY);
					voltage_wrn |= V5_FLAG;
				}
			}


			/* 1.5 VDC Source */
			if(env_ptr->V1p5 < tolerance[3][0]) {
				if (!(voltage_err & V1P5_FLAG)) {	/* Only log fault once */
					log_event(V1p5_LO, FP_DISPLAY);
					voltage_err |= V1P5_FLAG;
				}
			}
			else if(env_ptr->V1p5 > tolerance[3][1]) {
				if (!(voltage_err & V1P5_FLAG)) {	/* Only log fault once */
					log_event(V1p5_HI, FP_DISPLAY);
					voltage_err |= V1P5_FLAG;
					ptod->nv_status |= RESET_FERR;
				}
			}

			/* Inner Tolerance */
			else if(env_ptr->V1p5 < tolerance[3][2]) {
				if (!(voltage_wrn & V1P5_FLAG)) {	/* Only log fault once */
					log_event(V1p5_LO_WARN, FP_DISPLAY);
					voltage_wrn |= V1P5_FLAG;
				}
			}
			else if(env_ptr->V1p5 > tolerance[3][3]) {
				if (!(voltage_wrn & V1P5_FLAG)) {	/* Only log fault once */
					log_event(V1p5_HI_WARN, FP_DISPLAY);
					voltage_wrn |= V1P5_FLAG;
				}
			}


#ifdef LAB
			if((ptod->nv_irix_sw & 0x0400) == 0) {
#endif
				/* -5.2 VDC Source */
				if(env_ptr->VM5p2 < tolerance[4][0]) {
					if (!(voltage_err & VM5P2_FLAG)) {/* Only log fault once */
						log_event(VM5p2_HI, FP_DISPLAY);
						voltage_err |= VM5P2_FLAG;
						ptod->nv_status |= RESET_FERR;
					}
				}
				else if(env_ptr->VM5p2 > tolerance[4][1]) {
					if (!(voltage_err & VM5P2_FLAG)) {/* Only log fault once */
						log_event(VM5p2_LO, FP_DISPLAY);
						voltage_err |= VM5P2_FLAG;
					}
				}

				/* Inner Tolerance */
				else if(env_ptr->VM5p2 < tolerance[4][2]) {
					if (!(voltage_wrn & VM5P2_FLAG)) {/* Only log fault once */
						log_event(VM5p2_HI_WARN, FP_DISPLAY);
						voltage_wrn |= VM5P2_FLAG;
					}
				}
				else if(env_ptr->VM5p2 > tolerance[4][3]) {
					if (!(voltage_wrn & VM5P2_FLAG)) {/* Only log fault once */
						log_event(VM5p2_LO_WARN, FP_DISPLAY);
						voltage_wrn |= VM5P2_FLAG;
					}
				}


				/* -12 VDC Source */
				if(env_ptr->VM12 < tolerance[5][0]) {
					if (!(voltage_err & VM12_FLAG)) {/* Only log fault once */
						log_event(VM12_HI, FP_DISPLAY);
						voltage_err |= VM12_FLAG;
						ptod->nv_status |= RESET_FERR;
					}
				}
				else if(env_ptr->VM12 > tolerance[5][1]) {
					if (!(voltage_err & VM12_FLAG)) {/* Only log fault once */
						log_event(VM12_LO, FP_DISPLAY);
						voltage_err |= VM12_FLAG;
					}
				}

				/* Inner Tolerance */
				else if(env_ptr->VM12 < tolerance[5][2]) {
					if (!(voltage_wrn & VM12_FLAG)) {/* Only log fault once */
						log_event(VM12_HI_WARN, FP_DISPLAY);
						voltage_wrn |= VM12_FLAG;
					}
				}
				else if(env_ptr->VM12 > tolerance[5][3]) {
					if (!(voltage_wrn & VM12_FLAG)) {/* Only log fault once */
						log_event(VM12_LO_WARN, FP_DISPLAY);
						voltage_wrn |= VM12_FLAG;
					}
				}

#ifdef LAB
			}
#endif
		}


		/* Check the FAN Tach. Outputs and see if they are running */
		/* Only report a blower failure once */
		if (msg != MON_RUN_ONCE) {
			fint_ptr = &fan_a_per;
			if (fint_ptr->running == 0) { 
				if (!fan_a_err) {
					if(PORTD & TERMINATOR)  
						log_event(BLOW_A_ERR, FP_DISPLAY);
					else 
						log_event(BLOW_ERR, FP_DISPLAY);
				}
				fan_a_err |= 1;
				env_ptr->blow_a_tac = 0; 
			}
			else {
				/* Store the measured rate in RPM */
				/* 2 pulses per revolution, and 1420 rpm at 100% */
				/* 2 micro sec timer tick */
				rpm = 60 / (fint_ptr->half_period * 2 * 8e-6);
				if (rpm > env_ptr->max_rpm_a)
					env_ptr->max_rpm_a = rpm;
				else if (rpm < env_ptr->min_rpm_a)
					env_ptr->min_rpm_a = rpm;

				/* check for 10 consecutive invalid measurements before
			   	forcing the measurement to zero
			 	*/
				if (rpm < 0 || rpm > 3000) 
					rpm_a_cnt++;
				else
					rpm_a_cnt = 0;
				if (rpm_a_cnt == 30) 
					rpm = 0;


				env_ptr->blow_a_tac = rpm; 
#ifdef LAB
				if((ptod->nv_irix_sw & 0x0300) != 0x0300) 
#endif
						fint_ptr->running = 0;	/*reset flag for next pass */
			}

			if(PORTD & TERMINATOR) {
				fint_ptr = &fan_b_per;
				if(fint_ptr->running == 0) { 
					if (!fan_b_err) 
						log_event(BLOW_B_ERR, FP_DISPLAY);
					fan_b_err |= 1;
					env_ptr->blow_b_tac = 0; 
				}
				else {
					/* Store the measured rate in RPM */
					/* 2 pulses per revolution, and 1420 rpm at 100% */
					/* 2 micro sec timer tick */
					rpm = 60 / (fint_ptr->half_period * 2 * 8e-6);
					if (rpm > env_ptr->max_rpm_b)
						env_ptr->max_rpm_b = rpm;
					else if (rpm < env_ptr->min_rpm_b)
						env_ptr->min_rpm_b = rpm;

					/* check for 30 consecutive invalid measurements before
					   forcing the measurement to zero
					 */
					if (rpm < 0 || rpm > 3000) 
						rpm_b_cnt++;
					else
						rpm_b_cnt = 0;
					if (rpm_b_cnt == 30) 
						rpm = 0;

					env_ptr->blow_b_tac = rpm;

					/*reset flag for next pass */
#ifdef LAB
					if((ptod->nv_irix_sw & 0x0300) != 0x0300) 
#endif
						fint_ptr->running = 0;		
				}
			}
		}

		/* Check the measured blower rpm against the expected 
		   rpm value.  Ten consecutive failing values are necessary
		   in order to enter a logged event.
		 */

#ifdef LAB
if((ptod->nv_irix_sw & 0x0300) != 0x0300) {
#endif

		if (rpm_check) {
			if ((env_ptr->blow_a_tac > fan_ptr->hi_rpm) && !fan_a_hi) {
				if (PORTD & TERMINATOR)
					log_event(BLOW_A_HI_WARN, NO_FP_DISPLAY);
				else
					log_event(BLOW_HI_WARN, NO_FP_DISPLAY);
				fan_a_hi = 1;
			}

			if ((env_ptr->blow_a_tac < fan_ptr->low_rpm) && !fan_a_err) {
				rpm_chk_a++;
				env_ptr->total_rpm_a_err++;
			}

			if (PORTD & TERMINATOR) {
				if ((env_ptr->blow_b_tac > fan_ptr->hi_rpm) && !fan_b_hi) {
					log_event(BLOW_B_HI_WARN, NO_FP_DISPLAY);
					fan_b_hi = 1;
				}
				if ((env_ptr->blow_b_tac < fan_ptr->low_rpm) && !fan_b_err) {
					rpm_chk_b++;
					env_ptr->total_rpm_b_err++;
				}
			}

			if (rpm_chk_a > env_ptr->con_rpm_a_err)
				env_ptr->con_rpm_a_err = rpm_chk_a;
			if (rpm_chk_b > env_ptr->con_rpm_b_err)
				env_ptr->con_rpm_b_err = rpm_chk_b;


			if (!rpm_a_err) {
				if (rpm_chk_a >= 10) {
					if(fan_a + f_index_a == 0) { 	/* blower speed at max */
						if (PORTD & TERMINATOR)
							log_event(BLOW_A_RPM, FP_DISPLAY);
						else
							log_event(BLOW_RPM, FP_DISPLAY);
						rpm_a_err |= 1;
					}
					else {							/* still room for more */
						fan_adjust = 1;
						f_index_a--;
						rpm_chk_a = 0;
						if (PORTD & TERMINATOR)
							log_event(BLOW_A_RPM_WARN, NO_FP_DISPLAY);
						else
							log_event(BLOW_RPM_WARN, NO_FP_DISPLAY);
					}
				}
			}
			if (!rpm_b_err) {
				if (rpm_chk_b >= 10) {
					if(fan_a + f_index_b == 0) { 	/* blower speed at max */
						log_event(BLOW_B_RPM, FP_DISPLAY);
						rpm_b_err |= 1;
					}
					else {							/* still room for more */
						fan_adjust = 1;
						f_index_b--;
						rpm_chk_b = 0;
						log_event(BLOW_B_RPM_WARN, NO_FP_DISPLAY);
					}
				}
			}

			if (fan_adjust) {
				*L_BLOW_SET = ((fan_a + f_index_b) << 4) | (fan_a + f_index_a);
				rpm_check = 0;
				cancel_timeout(MONITOR, RPM_CHECK_TO);
				timeout(MONITOR, RPM_CHECK_TO, TEN_SEC);
				cpu_cmd(CPU_WARNING, WRN_BLOWER);
				fan_adjust = 0;
			}
		}

#ifdef LAB
}
#endif

		if(((ptod->nv_irix_sw & 0x0300) == 0x0100) && active_screen != ACT_SWITCH && active_screen != ACT_ERROR) {
			if(PORTD & TERMINATOR) {
				strcpy(buff, "\r       BACKPLANE VOLTAGE MONITOR\r");
				status_fmt(buff, ACT_VOLTS_DSP, CY_NO_SCROLL, NO_TIMEOUT);

				buff[0] = CY_KLEAR;
				sprintf(&buff[1], "     48   Volts = %.0f\r\r", env_ptr->V48);
				cy_str(buff);
				sprintf(buff, "      12   Volts = %.1f\r\r", env_ptr->V12);
				cy_str(buff);
				sprintf(buff, "       5   Volts = %.2f\r\r", env_ptr->V5);
				cy_str(buff);
				sprintf(buff, "      1.5  Volts = %.2f\r\r", env_ptr->V1p5);
				cy_str(buff);
				sprintf(buff, "     -5.2  Volts = %.1f\r\r", env_ptr->VM5p2);
				cy_str(buff);
				sprintf(buff, "      -12  Volts = %.1f\r\r", env_ptr->VM12);
				cy_str(buff);
			}
			else {
				screen_sw(ACT_VOLTS_DSP, NO_TIMEOUT);
				sprintf(buff, "%.0f     %.1f    %.2f\n %.2f  %.1f  %.1f", 
					env_ptr->V48, env_ptr->V12, env_ptr->V5, 
					env_ptr->V1p5, env_ptr->VM5p2, env_ptr->VM12);
				fpprint(buff, DSP_INIT);
			}
		}
		else if(((ptod->nv_irix_sw & 0x0300) == 0x0200)&& active_screen != ACT_SWITCH && active_screen != ACT_ERROR){
			if (PORTD & TERMINATOR) {
				strcpy(buff, "\r   BLOWER SPEED & TEMPERATURE MONITOR\r");
				status_fmt(buff, ACT_TEMP_DSP, CY_NO_SCROLL, NO_TIMEOUT);

				sprintf(buff,"\r        D/A SETTING:    %2X\r", 
					((fan_a << 4) | fan_a));
				cy_str(buff);
				sprintf(buff,"\r        BLOWER A SPEED: %.0f RPM\r", 
					env_ptr->blow_a_tac);
				cy_str(buff);
				sprintf(buff,"\r        BLOWER B SPEED: %.0f RPM\r", 
					env_ptr->blow_b_tac);
				cy_str(buff);
				sprintf(buff,"\r     INLET TEMPERATURE: %.0f", env_ptr->temp_a);
				sptr = strchr(buff, 0);
				*sptr++ = DEGREE_CY;
				*sptr++ = 'C';
				*sptr++ = '\r';
				*sptr++ = '\r';
				*sptr++ = '\r';
				*sptr++ = '\r';
				*sptr++ = '\r';
				*sptr = 0;
				cy_str(buff);
			}
			else {
				screen_sw(ACT_TEMP_DSP, NO_TIMEOUT);
				sprintf(buff,"D/A:%1X %4.0f RPM\nIT: %4.1f AT: %4.1f", 
					fan_a, rpm, int_temp, env_ptr->temp_a);
				fpprint(buff, DSP_INIT);
			}
		}
#ifdef LAB
		/* D / A Test Code */
		if(((ptod->nv_irix_sw & 0x0300) == 0x0300) && active_screen != ACT_SWITCH && active_screen != ACT_ERROR) {
			fint_ptr = &fan_a_per;
			if(fan_step) { 
				if (fan_dtoa == 0x0)
					fan_dtoa = 0xF;
				else
					fan_dtoa--;
				*L_BLOW_SET = (fan_dtoa << 4) | fan_dtoa;
			}
			if (PORTD & TERMINATOR) {
				strcpy(buff, "\r          BLOWER SPEED CONTROL\r");
				status_fmt(buff, ACT_FANTST, CY_NO_SCROLL, NO_TIMEOUT);
				cy_str(cmd_mode);
				cy_str("I 1\r");
				cy_str(dsp_mode);
				strcpy(&buff[1], "        PRESS THE EXECUTE BUTTON\r");
				cy_str(buff);
				strcpy(buff, "       TO ADJUST THE BLOWER SPEED\r\r");
				cy_str(buff);

				sprintf(buff,"     D/A SETTING:          %X\r", 
					(fan_dtoa << 4) | fan_dtoa);
				cy_str(buff);
				sprintf(buff,"     SAMPLED TEMPERATURE:  %4.1f\r",int_temp);
				cy_str(buff);
				sprintf(buff,"     AVERAGED TEMPERATURE: %4.1f\r", 
					env_ptr->temp_a);
				cy_str(buff);
				if(fint_ptr->running) {
					rpm = 60 / (fint_ptr->half_period * 2 * 8e-6);
					fint_ptr->running = 0;
					sprintf(buff,"     BLOWER A RPM:         %4.0f\r", rpm);
				}
				else 
					strcpy(buff, "     BLOWER A FAN SPEED NOT DETECTED\r");
				cy_str(buff);
				fint_ptr = &fan_b_per;
				if(fint_ptr->running) {
					rpm = 60 / (fint_ptr->half_period * 2 * 8e-6);
					fint_ptr->running = 0;
					sprintf(buff,"     BLOWER B RPM:         %4.0f\r", rpm);
				}
				else 
					strcpy(buff, "     BLOWER B FAN SPEED NOT DETECTED\r");
				cy_str(buff);
			}
			else {
				screen_sw(ACT_FANTST, NO_TIMEOUT);
				if(fint_ptr->running) {
					rpm = 60 / (fint_ptr->half_period * 2 * 8e-6);
					fint_ptr->running = 0;
					sprintf(buff,"IT: %4.1f  AT: %4.1f\n%X: RPM: %4.0f", 
						int_temp, env_ptr->temp_a, fan_dtoa, rpm);
				}
				else
					sprintf(buff,"INDEX 1: %X\nFAN NOT DETECTED", fan_dtoa);
				fpprint(buff, DSP_INIT);
			}

			if (fan_step) {
				fan_step = 0;
				active_switch = 0;
			}
		}
#endif

#ifdef CLK_CHK
		if (msg == MON_RUN) {
			/* Check the Backplane Clock */
			if( !(PORTA & BP_CLOCK)) {
				log_event(NO_CLCK, FP_DISPLAY);
#ifdef LAB
				if (shutdown_mode) 
#endif
					send(SEQUENCE, DOWN_PARTIAL);
			}
		}
#endif

		/* adjust the fans to the lowest speed if they are not turning */
		if(fan_a_err && fan_b_err)
			*L_BLOW_SET = 0xFF;
		else if(fan_a_err && !fan_b_err)
			*L_BLOW_SET = (fan_a << 4 | 0xF);
		else if(!fan_a_err && fan_b_err)
			*L_BLOW_SET = (0xF0 | fan_a);

		/* if a voltage exceeds the outer tolerance, shutdown immediately,
		   and if a voltage exceeds the inner tolerance send a warning
		   to the CPU */
		if (!voltage_err && (voltage_wrn != voltage_wrn_r)) {
			voltage_wrn_r = voltage_wrn;
			cpu_cmd(CPU_WARNING, WRN_VOLT); 
		}

#ifdef LAB
		if (shutdown_mode) {
#endif
			if (voltage_err && (msg == MON_RUN || msg == MON_RUN_ONCE)) 
				send(SEQUENCE, DOWN_PARTIAL);
			else if ((fan_a_err || fan_b_err || rpm_a_err || rpm_b_err) && (msg == MON_RUN)) 
				send(SEQUENCE, WARNING_FAN);
			else if (msg == MON_RUN)
				timeout(MONITOR, MON_RUN, ONE_SEC);
			else if (msg == MON_IDLE)
				timeout(MONITOR, MON_IDLE, ONE_SEC);
#ifdef LAB
		}
		else {
		if (msg == MON_RUN)
			timeout(MONITOR, MON_RUN, ONE_SEC);
		else if (msg == MON_IDLE)
			timeout(MONITOR, MON_IDLE, ONE_SEC);
		}
#endif
	}
}



/********************************************************************
 *	 																*
 *		4/16/93 -	removed the PWR fail log event from this 		*
 *					routine. Shutdown now logs the event to			*
 *					reduce the delay from a PWR_FAIL until power	*
 *					is removed from the system						*
 *	 																*
 ********************************************************************/
non_banked  void 
pok_chk(void)
{
	unsigned char	xirq_reg;
	int				voltage_fail;
	extern char		pwr_dwn_state;
	unsigned short	msg;
	struct 	RTC	*ptod = (struct RTC *) L_RTC;	/* Pointer to RTC */
	char			active_alarm;
	char			poke_chk;

#ifdef LAB
	char				shutdown_mode;
	shutdown_mode = ptod->nv_irix_sw & 0x1000 ? 0 : 1;
#endif
	active_off = 0; 				/*only report a switch off once */
	active_alarm = 0;
	poke_chk = 0;

	for (;;) {

		msg = receive(POK_CHK); 
		if(msg != POK_SCAN && msg != POKE_ENABLE) {
			log_event(BMSG_POKCHK, NO_FP_DISPLAY);
			continue;
		}
		if(msg == POKE_ENABLE)
			poke_chk = 1;

		else {
			/*check for SWITCH OFF SIGNAL */
			if ((*L_IRQ & IRQ_KEY) && !active_off) {
				active_off = 1;
				send(SEQUENCE, SWITCH_OFF);
			}
	
			xirq_reg = (*L_XIRQ & 0x3F);
		
			/*check for Power, Failure Interrupts */
			if ((xirq_reg & XIRQ_PFW) && (pwr_dwn_state != PWR_D_TOTAL)) {
				shutdown(PWR_FAIL);
			}
		
			voltage_fail = 0;
			/* Voltges from PENA control the POKs of B and C. Therefore if
			 * POKA is asserted then it of no use to look at POKB and POKC
			 */
			if ((xirq_reg & 0x06) == XIRQ_POK_A && !(active_alarm & XIRQ_POK_A)) {
				log_event(POKA_FAIL, FP_DISPLAY);
				voltage_fail |= 1;
				active_alarm |= XIRQ_POK_A;
			}
			else {
				if (xirq_reg & XIRQ_POK_B && !(active_alarm & XIRQ_POK_B)) {
					log_event(POKB_FAIL, FP_DISPLAY);
					voltage_fail |= 1;
					active_alarm |= XIRQ_POK_B;
				}
				if (xirq_reg & XIRQ_POK_C && !(active_alarm & XIRQ_POK_C)) {
					log_event(POKC_FAIL, FP_DISPLAY);
					voltage_fail |= 1;
					active_alarm |= XIRQ_POK_C;
				}
			}
			if (xirq_reg & XIRQ_POK_D && !(active_alarm & XIRQ_POK_D)) {
				log_event(POKD_FAIL, FP_DISPLAY);
				voltage_fail |= 1;
				active_alarm |= XIRQ_POK_D;
			}
			if ((xirq_reg & XIRQ_POK_E) && (ptod->nv_ser_add & PENE) &&
				!(active_alarm & XIRQ_POK_E) && poke_chk) {
				log_event(POKE_FAIL, FP_DISPLAY);
				active_alarm |= XIRQ_POK_E;
			}

			/* if POKE is reset after a failure, reset the alarm flag */
			else if (!(xirq_reg & XIRQ_POK_E) && (ptod->nv_ser_add & PENE) &&
					(active_alarm & XIRQ_POK_E) && poke_chk) 
				active_alarm &= ~XIRQ_POK_E;
		
			if (xirq_reg & XIRQ_OVERTEMP && !(active_alarm & XIRQ_OVERTEMP)) {
				log_event(TEMP_BRD, FP_DISPLAY);
				send(SEQUENCE, TEMPERATURE_OFF);
				active_alarm |= XIRQ_OVERTEMP;
			}
	#ifdef LAB
			if (shutdown_mode) {
	#endif
				if (voltage_fail)
					shutdown(PARTIAL);
	#ifdef LAB
			}
	#endif
	
			timeout(POK_CHK, POK_SCAN, MSEC_100); 
		}
	}
}
