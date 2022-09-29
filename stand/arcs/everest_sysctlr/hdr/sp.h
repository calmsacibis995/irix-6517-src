/*	sp.h  	*/
/*
 *	Support Processor General Header
 */

/* General definitions.
 */
#define min(x,y)	(((x) < (y)) ? (x) : (y))

#define		M_CMD			1		/* CPU command to processes */
#define		M_TIMOUT		2		/* Timeout to processes */
#define		PANIC			3		/* Message to ADMIN PROCESS */
#define		MON_RUN			4		/* Message to Start Monitor process */
#define		MON_IDLE		5		/* Start Monitor process in Idle mode */
#define		RPM_CHECK_TO	6		/* Enable RPM checking*/
#define		POK_SCAN		7		/* Message to Start POK checking */
#define		MON_RUN_ONCE	8		/* Check the voltages on a single pass */
#define		POKE_ENABLE		9		/* Enable POKE monitoring */

/* TYPES OF RESETS */
#define		RESET_PO	(char) 0x00		/* POWER ON RESET */
#define		RESET_IOT	(char) 0x01		/* ILLEGAL OP CODE TRAP RESET */
#define		RESET_COP	(char) 0x02		/* COMPUTER OPERATION PROPERLY RESET */
#define		RESET_CME	(char) 0x04		/* CLOCK MONITOR RESET */
#define		RESET_PFW	(char) 0x08		/* CLOCK MONITOR RESET */
#define		RESET_FERR	(char) 0x10		/* PREVIOUS POWER OFF FROM FATAL ERROR*/
#define		RESET_SWI	(char) 0x20		/* Reset caused from exec error */

#ifdef FASTTIMER
/* Time out periods
 */
 /* The following values are calculated for a RTI timer period of 4.10 ms 
 	and are used only with the timeout call */

#define		MSEC_4		1		/* 4 msec worth of 4 msec counts */
#define		MSEC_20		5		/* 20 msec worth of 4 msec counts */
#define		MSEC_50		13		/* 50 msec worth of 4 msec counts */
#define		MSEC_100	25		/* 100 msec worth of 4 msec counts */
#define		MSEC_250	63		/* 250 msec worth of 4 msec counts */
#define		MSEC_500	125		/* 500 msec worth of 4 msec counts */
#define		ONE_SEC		250		/* 1 second worth of 4 msec counts */
#define		TWO_SEC		500		/* 2 second worth of 4 msec counts */
#define		FIVE_SEC	1250	/* 5 seconds worth of 4 msec counts */
#define		TEN_SEC		2500	/* 10 seconds worth of 4 msec counts */
#define		THIRTY_SEC	7500	/* 30 seconds worth of 4 msec counts */
#define		ONE_MIN		15000	/* 1 minute worth of 4 msec counts */
#define		TWO_MIN		30000	/* 2 minutes worth of 4 msec counts */
#define		THREE_MIN	45000	/* 3 minutes worth of 4 msec counts */

#else

/* the following values are calculated for a RTI time period of 32.77 ms */
/* the timeout values will be at least the time period and maybe longer 
   due to the 32.77ms increments */

#define		MSEC_4		1		/* 4 msec worth of 32.77 msec counts */
#define		MSEC_30		1		/* 20 msec worth of 32.77 msec counts */
#define		MSEC_50		2		/* 50 msec worth of 32.77 msec counts */
#define		MSEC_100	3		/* 100 msec worth of 32.77 msec counts */
#define		MSEC_250	8		/* 250 msec worth of 32.77 msec counts */
#define		MSEC_500	16		/* 500 msec worth of 32.77 msec counts */
#define		ONE_SEC		31		/* 1 second worth of 32.77 msec counts */
#define		TWO_SEC		62		/* 2 second worth of 32.77 msec counts */
#define		FIVE_SEC	153		/* 5 seconds worth of 32.77 msec counts */
#define		TEN_SEC		306		/* 10 seconds worth of 32.77 msec counts */
#define		THIRTY_SEC	918		/* 30 seconds worth of 32.77 msec counts */
#define		ONE_MIN		1836	/* 1 minute worth of 32.77 msec counts */
#define		TWO_MIN		3672	/* 2 minutes worth of 32.77 msec counts */
#define		THREE_MIN	5508	/* 3 minutes worth of 32.77 msec counts */

#endif

/* default screen timeout values */
#define		NO_TIMEOUT		0	/* No default screen timeout */
#define		SCREEN_TO		THIRTY_SEC		/*default timeout value */
#define		ALARM_TIMEOUT	FIVE_SEC	


/* Software Time values to be used only with the delay routine */
#define		SW_100US	0		/* 100 usec delay value (min delay) */
#define		SW_1MS		20		/* 1 msec delay value */
#define		SW_2MS		40		/* 2 msec delay value */
#define		SW_4MS		80		/* 4 msec delay value */
#define		SW_5MS		100		/* 5 msec delay value */
#define		SW_10MS		200		/* 10 msec delay value */
#define		SW_15MS		300		/* 15 msec delay value */
#define		SW_20MS		400		/* 20 msec delay value */
#define		SW_30MS		600		/* 30 msec delay value */
#define		SW_50MS		1000	/* 50 msec delay value */
#define		SW_100MS	2000	/* 100 msec delay value */
#define		SW_150MS	3000	/* 150 msec delay value */
#define		SW_200MS	4000	/* 200 msec delay value */
#define		SW_500MS	10000	/* 100 msec delay value */
#define		SW_1SEC		20000	/* 1 second delay value */
#define		SW_5SEC		100000	/* 5 second delay value */


/* Process semaphore and port numbers
 */

#define		SEQUENCE	(char) 1		/* Power Sequencing */
#define		MONITOR		(char) 2		/* Environment Monitor */
#define		DISPLAY		(char) 3		/* Front Panel Display */
#define		CPU_PROC	(char) 4		/* CPU read / write process */
#define		POK_CHK		(char) 5		/* POKx, KEY, OVERTEMP process */

/*SEQUECNER PROCESS COMMANDS */
#define	DOWN_PARTIAL		(char) 0x1	/* Shutdown, but not 48V */
#define	DOWN_TOTAL			(char) 0x2	/* Shutdown, All Power*/
#define	DOWN_1SEC			(char) 0x3	/* Shutdown In 1 Sec */
#define	DOWN_10SEC			(char) 0x4	/* Shutdown In 10 Sec */
#define	POWER_UP			(char) 0x5	/* Power up the system */
#define	SWITCH_OFF			(char) 0x6	/* FP Switch is in off position */
#define	TEMPERATURE_OFF		(char) 0x7	/* Over Temperature Failure */
#define	WARNING_FAN			(char) 0x8	/* Blower Failure */
#define	REBOOT_SCLR			(char) 0x9	/* SCLR Detected, Reboot */
#define	FP_SCLR				(char) 0xA	/* Front Panel Command SCLR */
#define	TST_NMI				(char) 0xB	/* Debug NMI */
#define	MN_NMI				(char) 0xC	/* Front Panel Commanded NMI */
#define	TST_SCLR			(char) 0xD	/* Debug SCLR */
#define	INCOMING_TEMP_OFF 	(char) 0xE	/* Incoming air stream temp failure */
#define	DOWN_KILL 			(char) 0xF	/* Kill Power */

/* SEQUENCER DEFINES */
#define		PARTIAL			(char) 0x1		/* Shutdown everything except 48V*/
#define		TOTAL			(char) 0x2		/* Total Shutdown */
#define		KILL_ALL		(char) 0x3		/* Total Shutdown by kill power */
/*CPU PROCESS MESSAGES */
#define		BOOT_ARBITRATE	(char) 0x20		/* Rearbitrate the Boot Master CPU*/
#define		CPU_CMD			(char) 0x21		/* Command Received from CPU*/
#define		BOOT_DELAY		(char) 0x22		/* Delay during Boot Arbitration */

/* Power Down States */
#define		PWR_UP			(char) 0x0		/* System is Powered Up */
#define		PWR_PARTIAL		(char) 0x1		/* Only 48V and V5Aux are powered */
#define		PWR_D_TOTAL		(char) 0x2		/* All Power is being removed */
/* DISPLAY PROCESS MESSAGES */
#define	MENU				(char) 0x30		/* MENU COMMAND */
#define	SCROLL_UP			(char) 0x31		/* MENU SCROLL UP COMMAND */
#define	SCROLL_DN			(char) 0x32		/* MENU SCROLL DOWN COMMAND */
#define	EXECUTE				(char) 0x33		/* MENU EXECUTE COMMAND */
#define	BOOT_STAT			(char) 0x34		/* BOOT STATUS DISPLAY*/
#define	CPU_PERF			(char) 0x35		/* CPU PERFORMANCE DISPLAY*/
#define	DEFAULT_SCREEN		(char) 0x36		/* Switches the screen to default*/
#define	EXECUTE_DEBOUNCE	(char) 0x37		/* Switch Debounce timeout */
#define	SCRLDN_DEBOUNCE		(char) 0x38		/* Switch Debounce timeout */
#define	SCRLUP_DEBOUNCE		(char) 0x39		/* Switch Debounce timeout */
#define	MENU_DEBOUNCE		(char) 0x3A		/* Switch Debounce timeout */
#define	DEBUG_SET_TO		(char) 0x3B		/* Debug Setting Timeout */

/* GENERAL DEFINES FOR DISPLAY PROCESS */
#define		D_INIT			(char) 0x0		/* Initialize MENU flag */
#define		D_S_UP			(char) 0x1		/* Scroll Up flag */
#define		D_S_DOWN		(char) 0x2		/* Scroll Down flag */
#define		D_EXECUTE		(char) 0x3		/* Scroll Down flag */

#define		NO_SW			(char) 0x00		/* No Switch of Screens */
#define		ACT_MENU		(char) 0x01		/* Active Menu SCREEN */
#define		ACT_SWITCH		(char) 0x02		/* Active Debug Switch SCREEN */
#define		ACT_LOG			(char) 0x03		/* Active Event Log SCREEN */
#define		ACT_BOOT		(char) 0x04		/* Active Boot status SCREEN */
#define		ACT_ERROR		(char) 0x05		/* Active Error status SCREEN */
#define		ACT_PERF		(char) 0x06		/* Active Perf status SCREEN */
#define		ACT_NMI			(char) 0x07		/* Active NMI status SCREEN */
#define		ACT_TEMP		(char) 0x08		/* Active Temperature SCREEN */
#define		ACT_VOLTS		(char) 0x09		/* Active Voltage SCREEN */
#define		ACT_FANTST		(char) 0x0a		/* Active Fan test SCREEN */
#define		ACT_CPUSEL		(char) 0x0b		/* Active CPU Selection SCREEN */
#define		ACT_BOOTMSG		(char) 0x0c		/* Active BOOT msg SCREEN */
#define		ACT_STATUS		(char) 0x0d		/* General Status msg SCREEN */
#define		ACT_FWVER		(char) 0x0e		/* Firmware Version SCREEN */
#define		ACT_TEMP_DSP	(char) 0x0f		/* Active Temperature SCREEN */
#define		ACT_VOLTS_DSP	(char) 0x10		/* Firmware Version SCREEN */
#define		ACT_SCLR		(char) 0x11		/* Firmware Version SCREEN */

#define		CY_NO_SCROLL	(char) 0		/* no vertical scrolling */
#define		CY_SCROLL		(char) 1		/* vertical scrolling enabled */

#define		DSP_INIT		(char) 1		/* Init front panel controller*/
#define		DSP_NO_INIT		(char) 0		/* No Init of frnt panel cntrl*/

#define		LOG_START		(char) 1		/* Display is at start of log */
#define		LOG_END			(char) 2		/* Display is at end of log */

/* Boot message and Processor status for Cybernetics display */
#define		NEW_BT_MSG		(char)	1		/* Flag signifying new Boot Msg */
#define		NEW_PROC_MSG	(char)	2		/* Flag signifying new Proc Status*/
#define		MSG_ST			(char)	1		/* Flag indicating last window def*/
#define		PROC_ST			(char)	2		/* Flag indicating last window def*/

/* CPU-SYS_CNTL COMMANDS */
#define	G_DATA_TIME		(char) 0x10		/* Get Date and Time */
#define	G_DEBUG			(char) 0x11		/* Get Debug Switch Settings */
#define	G_ENV			(char) 0x12		/* Get Environmet Data(volts,temp,etc)*/
#define	G_EVENTS		(char) 0x13		/* Get Contents of Event Log*/
#define	G_SERIAL_NO		(char) 0x14		/* Get System Serial Number*/

#define	S_BOOT_STAT		(char) 0x30		/* Set Boot Status Message*/
#define	S_DATE_TIME		(char) 0x31		/* Set Date and Time*/
#define	S_DEBUG			(char) 0x32		/* Set Debug Switch Settings*/
#define	S_HIST			(char) 0x33		/* Set CPU Performance Histogram*/
#define	S_OFF			(char) 0x34		/* Set System Power Off Command*/
#define	S_PROC_STAT		(char) 0x35		/* Set Processor Boot Status*/
#define	S_SERIAL_NO		(char) 0x36		/* Set System Serial Number*/

/* SYS_CNTL - CPU COMMAND DEFINES */
#define CPU_ALARM		(char) 0x10		/* System controller Alarm command */
#define ALM_TEMP		(char) 0x11		/* Over temperature alarm */
#define ALM_BLOWER		(char) 0x12		/* Blower failure alarm */
#define ALM_OFF			(char) 0x13		/* Key switch off alarm */

#define CPU_WARNING		(char) 0x20		/* System controller Warning command */
#define WRN_COP			(char) 0x21		/* COP timer alarm */
#define WRN_OSC			(char) 0x22		/* System cntrl osc failure alarm */
#define WRN_IOT			(char) 0x23		/* System cntrl IOT warning */
#define WRN_VOLT		(char) 0x24		/* System cntrl Voltage warning */
#define WRN_SWI			(char) 0x25		/* System cntrl SWI restart */
#define WRN_BLOWER		(char) 0x26		/* Blower RPM out of Tolerance */


/* log event front panel display flags */
#define		NO_FP_DISPLAY	(char)	0	/* Do not display event on front panel*/
#define		FP_DISPLAY		(char)	1	/* Display event on front panel*/

/* System Controller Logged Event Code Index */
#define	SCI_ERR 		(char) 0x01		/* SCI Serial Communication Interface */
#define	SPI_ERR 		(char) 0x02		/* SPI Serial Transfer Complete */
#define	PAIE_ERR 		(char) 0x03		/* Pulse Accumulator Input Edge */
#define	PAO_ERR 		(char) 0x04		/* Pulse Accumulator Overflow */
#define	TO_ERR 			(char) 0x05		/* Timer Overflow */
#define	TOC5_ERR 		(char) 0x06		/* Timer Output Compare 5 */
#define	TOC4_ERR 		(char) 0x07		/* Timer Output Compare 4 */
#define	TOC3_ERR 		(char) 0x08		/* Timer Output Compare 3 */
#define	TOC2_ERR 		(char) 0x09		/* Timer Output Compare 2 */
#define	TOC1_ERR 		(char) 0x0a		/* Timer Output Compare 1 */
#define	TIC3_ERR 		(char) 0x0b		/* Timer Input Compare 3 */
#define	TIC2_ERR 		(char) 0x0c		/* Timer Input Compare 2 */
#define	TIC1_ERR 		(char) 0x0d		/* Timer Input Compare 1 */
#define	RTI_ERR			(char) 0x0e		/* Real Time Interrupt */
#define	IRQ_ERR			(char) 0x0f		/* Interrupt ReQuest */
#define	XIRQ_ERR 		(char) 0x10		/* eXtended Interrupt ReQuest */
#define	SWI_ERR 		(char) 0x11		/* SoftWare Interrupt */
#define	IOT_ERR 		(char) 0x12		/* Illegal Opcode Trap */
#define	NOCOP_ERR 		(char) 0x13		/* COP Failure */
#define	CME_ERR 		(char) 0x14		/* COP Monitor Failure */
#define	MEM_FAIL		(char) 0x15		/* System Controller Memory Failure*/
#define	STK_PID0		(char) 0x16		/* Stack Over Run PID #0 */
#define	STK_PID1		(char) 0x17		/* Stack Over Run PID #1 */
#define	STK_PID2		(char) 0x18		/* Stack Over Run PID #2 */
#define	STK_PID3		(char) 0x19		/* Stack Over Run PID #3 */
#define	STK_PID4		(char) 0x1a		/* Stack Over Run PID #4 */
#define	STK_PID5		(char) 0x1b		/* Stack Over Run PID #5 */
#define	STK_PID6		(char) 0x1c		/* Stack Over Run PID #6 */
#define	SYS_ON			(char) 0x1D		/* System On */
#define	SYS_OFF			(char) 0x1E		/* System Off */
#define	SYS_RESET		(char) 0x1F		/* System Reset */
#define	NMI				(char) 0x20		/* NMI issued */
#define	AC_FAIL			(char) 0x21		/* AC Failure */
#define	POKA_FAIL		(char) 0x22		/* > 1.2 Volt Regulator Failed */
#define	POKB_FAIL		(char) 0x23		/* < 1.2 Volt Regulator Failed */
#define	POKC_FAIL		(char) 0x24		/* POWER Enable C Failure*/
#define	TEMP_AMB		(char) 0x25		/* Ambient Incoming Air Temp. Failure*/
#define	TEMP_BRD		(char) 0x26		/* Board or Chassis Temp. Failure*/
#define	PWR_FAIL		(char) 0x27		/* Power Failure*/
#define	NO_CLCK			(char) 0x28		/* System Backplane Clock Failure*/
#define	V1p5_HI			(char) 0x29		/* 1.5VDC Over Voltage Failure*/
#define	V1p5_LO			(char) 0x2A		/* 1.5VDC Under Voltage Failure*/
#define	V5_HI			(char) 0x2B		/* 5VDC Over Voltage Failure*/
#define	V5_LO			(char) 0x2C		/* 5VDC Under Voltage Failure*/
#define	V12_HI			(char) 0x2D		/* 12VDC Over Voltage Failure*/
#define	V12_LO			(char) 0x2E		/* 12VDC Under Voltage Failure*/
#define	VM5p2_HI		(char) 0x2F		/* -5.2VDC Over Voltage Failure*/
#define	VM5p2_LO		(char) 0x30		/* -5.2VDC Under Voltage Failure*/
#define	VM12_HI			(char) 0x31		/* -12VDC Over Voltage Failure*/
#define	VM12_LO			(char) 0x32		/* -12VDC Under Voltage Failure*/
#define	V48_HI			(char) 0x33		/* 48VDC Over Voltage Failure*/
#define	V48_LO			(char) 0x34		/* 48VDC Under Voltage Failure*/
#define	FP_ERR			(char) 0x35		/* Front Panel Controller not ready */
#define	BLOW_A_ERR		(char) 0x36		/* Blower A Assembly Failure */
#define	BLOW_B_ERR		(char) 0x37		/* Blower B Assembly Failure */
#define	BMSG_SYSMON		(char) 0x38		/* Spuriours msg to Sys_mon process */
#define	BMSG_SEQUEN		(char) 0x39		/* Spuriours msg to Sequencer process */
#define	SCLR_DETECT		(char) 0x3A		/* SCLR Detected */
#define	BLOW_ERR		(char) 0x3B		/* Blower Assembly Failure (Eveready)*/
#define	SCI_OR			(char) 0x3C		/* SCI Character Overrun Error */
#define	SCI_NF			(char) 0x3D		/* SCI Character Noise Error */
#define	SCI_FR			(char) 0x3E		/* SCI Character Framing Error */
#define	BMSG_CPUPROC	(char) 0x3F		/* Spuriours msg to CPU process */
#define	BOOT_ERROR		(char) 0x40		/* CPU Boot Arbitration Error */
#define	BAD_CPU_CMD		(char) 0x41		/* Bad Command from CPU */
#define	NO_CPU_RSP		(char) 0x42		/* CPU Responding to Xmitter */
#define	BAD_DSP_MSG		(char) 0x43		/* Invalid display process message */
#define	POKD_FAIL		(char) 0x44		/* POWER Enable D Failure*/
#define	POKE_FAIL		(char) 0x45		/* POWER Enable E Failure*/
#define	ACIA_OVR		(char) 0x46		/* ACIA Character Overrun Error */
#define	ACIA_PAR		(char) 0x47		/* ACIA Character Parity Error */
#define	ACIA_FRM		(char) 0x48		/* ACIA Character Framing Error */
#define	BMSG_POKCHK		(char) 0x49		/* Spuriours msg to POK check process */
#define	BAD_WARNING		(char) 0x4A		/* Bad Warning or Alarm command */
#define	BAD_ALM_TYPE	(char) 0x4B		/* Bad Alarm Type */
#define	BAD_WRN_TYPE	(char) 0x4C		/* Bad Warning Type */
#define	FP_RD_ERR		(char) 0x4D		/* Front Panel Read Error */
#define	BLOW_RPM		(char) 0x4E		/* Blower Tach Failure */
#define TEMP_SENSOR_ERR	(char) 0x4F		/* Temperature Sensor Failure */
#define	BLOW_A_RPM		(char) 0x50		/* Blower Tach A Failure */
#define	BLOW_B_RPM		(char) 0x51		/* Blower Tach B Failure */
#define	PWR_CYCLE		(char) 0x52		/* Manufacturing Command power cycle */
#define	V1p5_HI_WARN	(char) 0x53		/* 1.5VDC Over Voltage Warning*/
#define	V1p5_LO_WARN	(char) 0x54		/* 1.5VDC Under Voltage Warning*/
#define	V5_HI_WARN		(char) 0x55		/* 5VDC Over Voltage Warning*/
#define	V5_LO_WARN		(char) 0x56		/* 5VDC Under Voltage Warning*/
#define	V12_HI_WARN		(char) 0x57		/* 12VDC Over Voltage Warning*/
#define	V12_LO_WARN		(char) 0x58		/* 12VDC Under Voltage Warning*/
#define	VM5p2_HI_WARN	(char) 0x59		/* -5.2VDC Over Voltage Warning*/
#define	VM5p2_LO_WARN	(char) 0x5A		/* -5.2VDC Under Voltage Warning*/
#define	VM12_HI_WARN	(char) 0x5B		/* -12VDC Over Voltage Warning*/
#define	VM12_LO_WARN	(char) 0x5C		/* -12VDC Under Voltage Warning*/
#define	V48_HI_WARN		(char) 0x5D		/* 48VDC Over Voltage Warning*/
#define	V48_LO_WARN		(char) 0x5E		/* 48VDC Under Voltage Warning*/
#define	STUCK_BUTTON	(char) 0x5F		/* Front Panel Button Stuck */
#define	FMSG_ERR		(char) 0x60		/* No remaining free MSG nodes */
#define	FTCB_ERR		(char) 0x61		/* No remaining free TCB nodes */
#define	DBSW_ERR		(char) 0x62		/* Debug Switch Settings Corrupted */
#define	BLOW_A_RPM_WARN	(char) 0x63		/* Blower A RPM Warning (Terminator) */
#define	BLOW_B_RPM_WARN	(char) 0x64		/* Blower B RPM Warning (Terminator) */
#define	BLOW_RPM_WARN	(char) 0x65		/* Blower Warning (Eveready)*/
#define	BLOW_A_HI_WARN	(char) 0x66		/* Blower A RPM Hi Warn(Terminator) */
#define	BLOW_B_HI_WARN	(char) 0x67		/* Blower B RPM Hi Warn (Terminator) */
#define	BLOW_HI_WARN	(char) 0x68		/* Blower Warning (Eveready)*/
#define	XMIT1_TO		(char) 0x69		/* UART #1 XMITTER TIMEOUT*/
#define	XMIT2_TO		(char) 0x6A		/* UART #2 XMITTER TIMEOUT*/
#define	ATOD_TO			(char) 0x6B		/* D to A Conversion TIMEOUT*/


/* cybernetics window array diffinition index */
#define	FUNC_M_TITLE	0
#define	FUNC_MENU		1
#define	FUNC_MENU_PTR	2
#define	ERROR_TITLE		3
#define	ERROR_TABLE		4
#define	BOOT_MSG		5
#define	PROC_MSG_TITLE	6
#define	PROC_MSG		7

/***** Analog to Digital Conversions ***************/


struct	ENV_DATA {
	float	V48;
	float	V12;
	float	V5;
	float	V1p5;
	float	VM5p2;
	float	VM12;
	float	temp_a;
	float	blow_a_tac;				/* Measured RPM */
	float	blow_b_tac;				/* Measured RPM */
	char	blower_out;				/* Speed Output Register */
	char	fwver[6];				/* Firmware Version */

	float	max_temp;				/* Maximum Recorded Temperature */
	float	min_temp;				/* Minimum Recorded Temperature */

	float	max_rpm_a;				/* Maximum Recorded Blower A RPM */
	float	min_rpm_a;				/* Minimum Recorded Blower A RPM */
	float	max_rpm_b;				/* Maximum Recorded Blower B RPM */
	float	min_rpm_b;				/* Minimum Recorded Blower B RPM */

	int		con_rpm_a_err;			/* Consecutive RPM Errors Blower A */
	int		total_rpm_a_err;		/* Total RPM Errors Blower A */
	int		con_rpm_b_err;			/* Consecutive RPM Errors Blower A */
	int		total_rpm_b_err;		/* Total RPM Errors Blower A */
	int		invalid_cpu_cmd;		/* Total invalid cpu commands */

	unsigned short num_free_msg;	/* Current number of free msg nodes */
	unsigned short min_free_msg;	/* Minimum number of free msg nodes */
	unsigned short num_free_tcb;	/* Current number of free msg nodes */
	unsigned short min_free_tcb;	/* Minimum number of free msg nodes */
};

/* Voltage out of tolerance warning flags */
#define	V48_FLAG	(char)	0x20		/*fails inner tolerance band */
#define	V12_FLAG	(char)	0x10		/*fails inner tolerance band */
#define	V5_FLAG		(char)	0x08		/*fails inner tolerance band */
#define	V1P5_FLAG	(char)	0x04		/*fails inner tolerance band */
#define	VM5P2_FLAG	(char)	0x02		/*fails inner tolerance band */
#define	VM12_FLAG	(char)	0x01		/*fails inner tolerance band */

/* circular queue for temperature readings.  A running average of temperatures
   will be used to stablize the varations of Blower speed due to temperature
   fluctions.
 */
#define	TEMP_LEN	40					/* length of circular queue */
struct	SYS_TEMP {
	int		index;						/* points to the next location */
	int		entries;					/* 0 - TEMP_LEN */
	float	temp[40];
};

struct	BLOWER_OUTPUT {
	float	low_rpm;					/* lowest rate acceptable */
	float	set_rpm;					/* Expected RPM */
	float	hi_rpm;						/* highest rate acceptable */
	float	low_range;					/* lower temperature limit */
	float	hi_range;					/* upper temperature limit */
};

/* data structure used by interrupt routine in measuring the periods */
struct	BLOWER_TACH {
	unsigned short	pulse_1;		/* timer value for first pulse */
	unsigned short	pulse_2;		/* timer value for second pulse */
	unsigned long	half_period;	/* time between pulses */
	int		first_pulse;			/* flag for 1st (0) or 2nd (1)pulse */
	int		running;				/* Set by interrupt indicating OK */
	int		overflow;				/* Overflow Counter */
};


/* data structure for MENU DISPLAY */
struct	MENU_ST	{
	unsigned char	line_stat;
	void	(*menu_func)(void);		/* MENU Line function entry */
	char			*line_msg;		/* Text Message displayed on LCD */
	unsigned short	to_value;		/* timeout value for menu */
	char			d_scrn;			/* default screen */
	char			p_flag;			/* Special Processing flag */
};

/* menu line status */
#define	MN_MAN			(char)	0x80
#define	MN_ALL			(char)	0

#ifdef	PWR_CTL
#define NO_MENU_LINES	(char)	12
#else
#define NO_MENU_LINES	(char)	10
#endif

#ifdef	PWR_CTL
/* special processing flags */
#define	PENC_FLG	1				/* Toggle Power of PEN_C */
#define	PEND_FLG	2				/* Toggle Power of PEN_D */
#define PENE_FLG	3				/* Toggle Power of PEN_E */
#endif

/* data structure for MENU DISPLAY status */
struct	MENU_LINE {
	signed char	cur_ptr;			/* current to be executed on */
	signed char	window_ptr;			/* line on display being pointed to */
	signed char	first_line;			/* first line on display */
	signed char	last_line;			/* last line on display */
	signed char	no_lines;			/* max number of menu lines on display */
	signed char	no_on_func;			/* number of switch "ON" only functions */
	signed char	last_sw_setting;	/* key setting last time through menu */
	signed char	last_db_setting;	/* last setting for debug switches */
};

/* data structure to track cursor location between boot msg, and proc status
	window of the Cybernetic display */

struct PROC_MSG_ST {
	unsigned char new_msg;			/* indicates a new message has arrived */
	unsigned char win_state;		/* 1 if boot msg or 2 if proc status */
	char msg_cur_row;				/* location of cursor row for boot msg */
	char msg_cur_col;				/* location of cursor col for boot msg */
};
