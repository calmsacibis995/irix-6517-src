#pragma option v  
/* main.c */
/*
 * Entry point for the c modules for the lego entry level system controller
 * firmware.
 */
 
/*********************************************************************************
 * Change History:																					*
 *																											*
 *			9/19/96		BLM	Changed the default 3.45 Voltage margin setting to 	*
 *									default to a normal level.										*
 *																											*
 *																											*
 *			9/25/96		BLM	1	Removed monitoring Over temperature conditions in 	*
 *										standby mode. (main.c init_cond()).						*
 *								   2	Removed checking for Over temperature before 		*
 *										applying power in. (sequence.c power_up())			*
 *									3	Qualified Over temperature monitoring with system	*
 *										being powered up. (monitor.c inputPortA())			*
 *									4	Updated Version strings	(main.c main())				*
 *									5  Prevent a locked rotor fan from being retested		*
 *										(monitor.c, tasks_fan())									*								
 *																											*
 *			9/27/96		BLM	Fixed an I2C bus hang problem when the hub side is 	*
 *									initialized. by initializing the i2c bus if 5 seconds	*
 *									expire without the ELSC have one successful i2c bus	*
 *									tansaction. Version 2.20, (main.c; sw_init, i2c.c:		*
 *									i2c_init, waitForPin i2cSendToken i2c_slave, elsc.h)	*
 *																											*
 *			11/8/96		BLM	Added functionality for manufacturing release			*
 *									See V2.22.changes on UNIX source tree for descriptions*
 *																											*
 *			12/2/96		BLM	Corrected an initialization problem in Version 2.22	*
 *									that enabled interrupts as part of the modem control	*
 *									initialization, and mid-plane type sense, code.			*
 *									This source update also includes changes for speedo	*
 *									master/slave functionality.									*
 *																											*
 *			12/5/96		BLM	Removed a call to i2c_init() from the sendHubInt()		*
 *									function.  The call was causing a failure in the		*
 *									token passing routine. New released version is 3.0		*
 *																											*
 ********************************************************************************/

//#define	FAN_TEST
//#define	DEBUG1
//#define 	FAN_FULL_SPEED
//#define	M345_LOW_DEFAULT
//#define	POK_SLOT
//#define	IGNORE_OT
//#define	MFG_TST
//#define	_005_MID				/* use for -004 and -005 midplanes */
//#define	DIS_I2C_INIT		/* use only for Speedo MS debug */
//#define	DIP_SW_10			/* Speedo ELSC with 10 bit dip switch */
//#define	SPEEDO

#define	RTS_CORRECTION
#define	FAN_CHK
#define 	POK_CHK	
#define 	MODEM
#define	_005						/* use for -005 and greater ELSC boards */
#define	NEW_EXT
#define	HUB_INT
#define	NO_PORTB_INT
#define	KCAR_FAN_FULL_SPEED
 
#include <\mpc\17c44.h>
#include <\mpc\ctype.h>
#include <\mpc\math.h>
#include <\mpc\mpc16.lib>
#include <elsc.h>
#include <elsc_hw.h>
#include <cmd.h>
#include <proto.h>
#include <async.h>
#include <i2c.h>

#ifdef INDIRECT_Q
/* Async and I2c buffer and circular queue declarations */
int	near *sci_inp;						/* Pointer to ACP input queue */
int	near *sci_outp;					/* Pointer to ACP output queue */
int	near *i2c_inp;						/* Pointer to I2C input queue */
int	near *i2c_outp;					/* Pointer to I2C output queue */
#endif

/* because of a compiler problem all arrays must be in RAM page 0 */
char	rxbuffer[ASYNCBUFSZ];			/* RS-232 Receive Buffer */
char	txbuffer[ASYNCBUFSZ];			/* RS-232 Transmit Buffer */
char	i2c_rxbuffer[I2CRBUFSZ];		/* I2C Slave Receive Buffer */
char	i2c_txbuffer[I2CTBUFSZ];		/* I2C Slave Transmit Buffer */
char	acp_buf[ACPBUFSZ];				/* buffer for ACP - Hub interrupt transfers */
char	rsp_buf[RSPBUFSZ];				/* I2C Command Response buffer */
char	buffer[10];							/* general purpose buffer */
char	cmd[4];								/* used for both command and parameters */


int				rsp_ptr;					/* I2C response pointer */
int				acp_ptr;					/* get command response pointer */
struct cbufh 	sci_inq;					/* Input SCI data queue */
struct cbufh	sci_outq;				/* Output SCI data queue */
struct cbufh 	i2c_inq;					/* Input SCI data queue */
struct cbufh	i2c_outq;				/* Output SCI data queue */
struct slave_pcb slavep;				/* Struct for i2c slave processing */

unsigned char		ch;
#ifdef INDIRECT_Q
char					q_var1, q_var2;	/* varibles only used in q functions */
#endif

/* Async and I2C (HUB) command varibles */
struct			i2c_scb	i2c_cp;		/* I2C control data structure */
char 				ret_val;
int				hub_tas;					/* hub test and set locking byte */
char				hbt_state;				/* Heart beat timeout state register */
long				hbt_interval;			/* Heart beat interval timeout value */
long				hbt_cyc_to;				/* Heart beat interval between NMI and PWR CYC */
long				param_val;
#ifdef	SPEEDO
char				slave_cmd_param;		/* ELSC slave to Master command parameter */
char				master_cmd_param;		/* ELSC master to slave command parameter */
char				slave_fan_stat;		/* Bit map of Slave fan status, 1= failure*/
char				slave_temp_stat;		/* temperature status of slave Speedo ELSC*/
#endif

/* external ports */
long 		  ext_ptr_store;				/* temp storeage area for external pointers */
long far * ext_ptr;						/* pointer used for table read/write commands */
long far * str_ptr;						/* table read pointer for strings */

#if defined (IGNORE_OT) && defined (_005_MID)
const	char	version[] = {'V',' ','3','.','2',' ','O',' '};
#elif defined (IGNORE_OT) && defined (SPEEDO)
const	char	version[] = {'V',' ','3','.','1','S','O',' '};
#elif defined (IGNORE_OT) 
const	char	version[] = {'V',' ','3','.','2',' ','O',' '};
#elif	defined (SPEEDO)
const	char	version[] = {'V',' ','3','.','1','S',' ',' '};
#elif defined (_005_MID) && defined(POK_CHK) && defined(_005)
const	char	version[] = {'V',' ','3','.','2','  ',' ',' '};
#elif defined(POK_CHK) && defined(_005)
const	char	version[] = {'V','E','R',' ','3','.','2',' '};
#elif defined(_005)
const	char	version[] = {'V',' ','3','.','2',' ',' ',' '};
#else	
const	char	version[] = {'V','E','R',' ','1','.','3',' '};
#endif

#if defined (IGNORE_OT) || defined (MFG_TST)
const	char	sgi_only[] = {' ','S','G','I',' ','T','E','S','T',' ',
								'U','S','E',' ','O','N','L','Y',0};
#endif

/* Global Structure Declarations */
struct MONITOR tasks;				/* data structure containing task status */
struct FAN_MON fan_ptr;				/* data structure containing task status */
struct SYS_STATE reg_state;		/* data structure containing copies of reg. */
struct TIMER TCB;						/* data structure containing task status */
unsigned long to_value;				/* value passed to timeout routine */
struct CMD_INT cmdp;					/* data structure for command management */

#ifndef	SPEEDO
int	pok_slot_no;					/* slot number from POK alarm */
#endif

#ifdef MODEM
unsigned char		flow_cnt_reg;	/* copy of port D RS-232 flow control bits */
#endif

int	no_power;				/* flag to indicate an error and not to apply power */
long	i2c_to_cnt;				/* time out register for I2C bus initialization */

unsigned long count;
unsigned long tmp_count;
long l_ch;
int temp;
bits t_reg;
char reg;
#ifdef	SPEEDO
bits	led_state;				/* state of led before being blinked */
long	sw_ms_counter;			/* msec counter for software loop timeout control */
#endif

char int_state;						/* interrupt state flag*/
char int_state_reg;					/* context save location for int state during an int */
char	mp_type;							/* mid-plane type */
bits last_key_position;				/* Last location of key switch */
char	mfg_tst;							/* varible used to reduce the fan timeout for testing */
#ifdef	SPEEDO
char i2c_retry;
#endif
char i2c_reg;


/* ELSC Messages */
/*Informational Messages, No failures */
const char  MSG_PWR_OK[] = {' ','S','Y','S',' ','O','K',' ','1'};		/* System Normal */
const char  MSG_PWR_RM[] = {'R',' ','P','W','R',' ','U','P','1'};		/* Remote Power Up */
const char  MSG_PWRUP[] = 	{'P','O','W','E','R',' ','U','P','1'};		/* Power Supply Temperature Normal */

/* Alarms, System will shutdown following the message */
const char  MSG_PFW[] 	= 	{'P','F','W',' ','F','A','I','L','3'};		/* Power Fail Warning */
const char  MSG_PSOT2[]	= 	{'P','S',' ','O','T',' ','F','L','3'};		/* Power Supply Over Temperature Warning */
const char  MSG_PSFL[] 	= 	{'P','S',' ','F','A','I','L',' ','3'};		/* Power Supply Fail Warning */
const char  MSG_SYSOT[]	= 	{'O','V','R',' ','T','E','M','P','3'};		/* System Over Temperature Alarm */
const char  MSG_FP_OFF[] = {'K','E','Y',' ','O','F','F',' ','3'};		/* Front Panel Switch Off Warning */
const char  MSG_FP_RESET[]={' ','R','E','S','E','T',' ',' ','3'};		/* Front Panel Reset Warning */
const char  MSG_FP_NMI[] = {' ',' ','N','M','I',' ',' ',' ','3'};		/* Front Panel NMI  Warning */
const char  MSG_M_FANFL[]= {'M',' ','F','A','N',' ','F','L','3'};		/* Multiple fans have failed */
const char  MSG_RM_OFF[]= 	{'R',' ','P','W','R',' ','D','N','3'};		/* Remote Power Off */
const char  MSG_PWR_CYCLE[] = {'P','W','R',' ','C','Y','C','L','3'};		/* Power Cycle Command */
const char  MSG_HBT_TO[] = {' ','H','B','T',' ','T','O',' ','3'};		/* Heart Beat Timeout */

/* System warnings, no shutdown will occur */
const char  MSG_FANFL[] = 	{'F','A','N',' ','F','A','I','L','2'};		/* System Fan Fail Warning */

const char  MSG_FANFAIL[] 	= {'F','A','N',' ','F','A','I','L','3'};		/* Single Fan Fail Warning */
const char  MSG_POK[] 		= {'P','O','K',' ','F','A','I','L','3'};		/* POK Failure, board unknown */

#ifdef POK_SLOT
/* Mid-Plane I/O Expander 0x40 */
const char  MSG_POK_N0[] = {'P','O','K',' ','N',' ','0',' '};	/* POK Failure, Node 0 */
const char  MSG_POK_N1[] = {'P','O','K',' ','N',' ','1',' '};	/* POK Failure, Node 1 */
const char  MSG_POK_N2[] = {'P','O','K',' ','N',' ','2',' '};	/* POK Failure, Node 2 */
const char  MSG_POK_N3[] = {'P','O','K',' ','N',' ','3',' '};	/* POK Failure, Node 3 */
/* Mid-Plane I/O Expander U2 */
const char  MSG_POK_R0[] = {'P','O','K',' ','R','T',' ','0'};	/* POK Failure, Router 0 */
const char  MSG_POK_R1[] = {'P','O','K',' ','R','T',' ','1'};	/* POK Failure, Router 1 */
#endif

const char 	INT_ACP[]=     {'a','c','p',' ',0};						/* Serial Port Pass through */
const char  INT_PD_OFF[]=  {'p','d',' ','o','f','f',0};				/* Key switch turned to the off position */
const char  INT_PD_SOFT[]= {'p','d',' ','s','o','f','t',0};		/* Power down sequence intiated by the Hub */
const char  INT_PD_TEMP[]= {'p','d',' ','t','e','m','p',0};		/* shutdown pending due to over temperature */
const char	INT_PD_FAN[]=  {'p','d',' ','f','a','n',0};				/* shutdown due to critical or multiple fan fail*/
const char	INT_PD_CYL[]=  {'p','d',' ','c','y','c','l','e',0};	/* shutdown due to power cycle command */
const char	INT_PD_PSOT[]= {'p','d',' ','p','s','o','t',0};		/* shutdown due to power supply over temperature */
const char	INT_PD_RM[]=   {'p','d',' ','r','e','m','o','t','e',0};	/* shutdown due to remote command (serial) */
const char	INT_PD_PSFL[]= {'p','d',' ','p','s','f','l',0};		/* shutdown due to power supply failure */
const char	INT_POK[]=     {'p','o','k',0};								/* shutdown due to power failure in "slot" number */
const char	INT_AC[]=      {'a','c',0};									/* pending shutdown due to ac failure */
const char	INT_FAN_OUT[]= {'f','a','n',' ','o','u','t',0};			/* fans  to high speed due to a single failure*/
const char	INT_RST[]= 		{'r','s','t',0};								/* reset requested by hub is pending */

#ifdef	SPEEDO
/* command sequences sent to a Speedo Slave ELSC from the Master ELSC */
const	char	SP_PWR_D[]=		{'p','w','r',' ','d',' ','0',0};	/* Speedo Slave power down  */
const	char	SP_PWR_U[]=		{'p','w','r',' ','u',0};	/* Speedo Slave power up  */
const	char	SP_NMI[]=		{'n','m','i',0};				/* Speedo Slave NMI  */
const	char	SP_RST[]=		{'r','s','t',0};				/* Speedo Slave Reset  */
const char	SP_PWR_S[]=		{'p','w','r',0};				/* Slave Power Status */
const char	SP_FAN_H[]=		{'f','a','n',' ','h',0};	/* Slave fan high */
const char	SP_FAN_N[]=		{'f','a','n',' ','n',0};	/* Slave fan normal */
const char	SLAVE_CMD[]= 	{'s','l','a',' ',0};			/* ELSC slave to master command */
#endif	//SPEEDO



/***************************************************************************
 *																									*
 *	main:	entry point from the reset vector.  Main calls the 					*
 * 			initialization routines for the hardware and software config.	*
 *			And then calls the monitor routine that is responsible 				*
 *			checking the data structures for tasks to be executed					*
 *																									*
 ***************************************************************************/
void
main()
{
	int i;

	/* First check for type of reset; Power-On or Watch Dog Timer */

	/* if a power on then set the power state */
	tasks.status = 0;

	/* A Write to the I2C UART must be the first access to external
	 * memory, otherwise the UART initialize it's bus type incorrectly
	 */
	ext_ptr = I2C_S1;
	*ext_ptr = CTL_PIN;

	/* Software Initialization */
	sw_init();

	
#ifdef	SPEEDO
	/* get the Speedo Master / Slave Configuration */
#ifdef	DIP_SW_10
	ext_ptr = INPT2;
#else
	ext_ptr = DIP_SW;
#endif  //DIP_SW_10
	param_val = *ext_ptr;

	if(param_val & MS_MASK) {
		tasks.env2.ENV2_SPEEDO_MASTER = 1;
		tasks.env2.ENV2_SPEEDO_SLAVE = 0;
	}
	else {
		tasks.env2.ENV2_SPEEDO_MASTER = 0;
		tasks.env2.ENV2_SPEEDO_SLAVE = 1;
	}
#endif  //SPEEDO

	/* Hardware Initialization */
	hw_init();

	/* I2C Initialization */
	i2c_init();

	/* Initialize the display */
	LED_int();

#ifdef	SPEEDO
	if(!tasks.env2.ENV2_SPEEDO_SLAVE)
		nvram_init();
#else
	nvram_init();
#endif
	
#ifndef	SPEEDO
	/* Initialize Real time clock part on mid-plane */
	midplane_clk_init();
#endif	//SPEEDO


	/* Check for initial Conditions */
	init_cond();

#ifdef	SPEEDO
	cmdp.cmd_intr.TOKEN_ENABLE = 1;
#endif


	/* Endless monitor funciton */
	monitor();

}
/******************************************************************************
 *																										*
 *	hw_init:   Initializes all hardware specific configuration parameters		*
 *																										*
 ******************************************************************************/
void
hw_init()
{

	/* OPTION Register; Set up RB0/Int and Watch dog timer */

	// The I2C UART is interrupt (RA0) is configured for falling edge
	

	T0STA = 0;
	T0STA.T0CS = 1;

	/* PORTB Register;		<7:0> Input
	 */
PORTB = 0;
#ifdef	SPEEDO
	DDRB = 0xE3;
#elif	defined (_005)
	DDRB = 0xFF;
#else
	DDRB = 0xDF;
#endif
	
	/*
	 * Configure Timer 3 for use by the fan pulse counting code.  The clock for the 
	 * timer will be internal clock. The 17C44 does not have an internal
	 *	prescaler, therefore a software overflow register must be tracked.
	 * The input clock period is 500nsec @ osc of 8 MHZ.
	 *
	 * 
	 */
	 
	TCON1 = 0;
	TCON2 = 0;

	/* enable pulse capturing for fans.  SN0 uses CA1 and CA2, but speedo only
	 * uses CA2
	 */
	TCON1.CA2ED0 = 1;
#ifndef	SPEEDO
	TCON1.CA1ED0 = 1;
#endif
	
	/* Enable dual pulse capture without a timer period register */
	TCON2.CA1 = 1;
	TMR3L = 0;
	TMR3H = 0;

	
	/*
	 * Configure Timer 1 & 2 for use as the periodic interrupt timer with
	 * a timeout period of 1 msec.  Timer 1 & 2 are concatenated together to form
	 * a single 16 bit timer. The comparator register is also a concatenated form
	 * of PR1 and PR2 registers to form a period register.  Once the count equals 
	 * the period register, the timer is initialized  to zero and an interrupt is 
	 * generated.  The value in the period register is 0xEA60.
	 */
	 
	 /* Set Timer1 and Timer2 for 16 bit count mode */
	 TCON1.T16 = 1;
	 
	 /* Initialize Period Registers */

	 PR2 = 0x07;
	 PR1 = 0xd0;
	 
	 /* Turn on both Timer 1 & 2 */
	 TCON2.TMR2ON = 1;
	 TCON2.TMR1ON = 1;
	 
	async_init();

	
	ext_ptr = OUTP1;
	*ext_ptr = reg_state.reg_out1;
#ifndef	SPEEDO
	ext_ptr = OUTP3;
	*ext_ptr = reg_state.reg_out3;

#ifdef M345_LOW_DEFAULT
	reg_state.reg_out2.V345_MARGINH = 1;
	reg_state.reg_out2.V345_MARGINL = 0;
#endif
	ext_ptr = OUTP2;
	*ext_ptr = reg_state.reg_out2;

#endif		//SPEEDO

#ifdef FAN_FULL_SPEED
#ifdef	SPEEDO
		/* fans are run at full speed for testing */
		reg_state.reg_out1.FAN_HI_SPEED = 1;
		ext_ptr = OUTP1;
		*ext_ptr = reg_state.reg_out1;
#else
		/* fans are run at full speed for testing */
		reg_state.reg_out3.FAN_HI_SPEED = 1;
		ext_ptr = OUTP3;
		*ext_ptr = reg_state.reg_out3;
		if (mp_type == MP_KCAR) {
			reg_state.reg_out1.KCAR_HI_FAN_SPEED = 1;
			ext_ptr = OUTP1;
			*ext_ptr = reg_state.reg_out1;
		}

#endif	//SPEEDO
#endif

#ifndef	SPEEDO
	/* capture the mid-plane type the ELSC is connected to */
#endif
	
	/* 
	 * Enable Interrupts:  ***************************************************
	 * Should be the last task of this function! 
	 *
	 */
	INTSTA = 0;
	INTSTA.PEIE = 1;			// Peripheral Interrupt Enable
	INTSTA.INTIE = 1;			// External Interrupt On INT Enable Bit
	
	PIE = 0;
	PIE.RCIE = 1;				// Enable RS-232 Receiver interrupts
#if 0		// Don't understand why the next line is writing to PIR
	PIR = 0x02;					// Power On reset value
#endif

#ifndef	NO_PORTB_INT
	PIE.RBIE = 1;				// PORTB Interrupt on Change Enable
#endif
	
#ifdef FAN_CHK
	PIE.TMR3IE = 1;			// Timer 3 Interrupt Enable
#endif
	PIE.TMR1IE = 1;			// Timer 1 Interrupt Enable
	
	/* Global Interrupt Enable */
	CPUSTA.GLINTD = 0;

}


/******************************************************************************
 *																										*
 *	init_cond:  Checks for initial conditions and sets appropriate status		*
 *				flags.																				*
 *																										*
 ******************************************************************************/
void
init_cond()
{

	char i;
	char reg;
	
	/* prompt the FFSC controller to send a command indicating it's presence */
	CPUSTA.GLINTD = 1;
	addq(SCI_OUT, CNTL_T);
	addq(SCI_OUT, 'M');
	addq(SCI_OUT, 'S');
	addq(SCI_OUT, 'C');
	addq(SCI_OUT, ' ');
	for (i = 0; i < 8; i++) {
		if(version[i] == 0)
			break;
		reg = version[i];
		addq(SCI_OUT, reg);
		PIE.TXIE = 1;
	}
	CPUSTA.GLINTD = 0;

#if	defined(IGNORE_OT) || defined(MFG_TST)
	for (i = 0; ; i++) {
		if(sgi_only[i] == 0)
			break;
		reg = sgi_only[i];
		while(sci_outq.c_len > HIWATER) {
			PIE.TXIE = 1;
		}
		CPUSTA.GLINTD = 1;
		addq(SCI_OUT, reg);
		CPUSTA.GLINTD = 0;

	}
#endif
	load_CR_NL();

	/* wait 2 seconds for the FFSC to respond, before checking the power up */
	to_value = TWO_SEC;
	delay();

#ifdef	SPEEDO
	/* read the initial state of the hardware registers */
	reg_state.reg_ra = PORTA;
	reg_state.reg_rb = PORTB;

	/* force over temperature and high temperature readings to be normal */
	reg_state.reg_ra.TMP_SYS_OT = 1;
	reg_state.reg_rb.TMP_FANS_HI = 0;


#else
	no_power = 0;


	
	/* read the initial state of the hardware registers */
	reg_state.reg_ra = PORTA;
	reg_state.reg_rb = PORTB;

#if 0
	/* read key position for next power cycle */
#ifdef	_005
	last_key_position = PORTB;
#else
	last_key_position = PORTA;
#endif

#endif
	
	/* force over temperature and high temperature reading to be normal */
	reg_state.reg_ra.TMP_SYS_OT = 1;
	reg_state.reg_rb.TMP_FANS_HI = 0;

	
	
	/* Power Supply Over Temperature */
	if (reg_state.reg_in2.PS_OT2) {
		dspMsg(MSG_PSOT2);
		tasks.fw_logical.FWL_PS_OT = 1;
		reg_state.reg_out3.SYS_OT_LED = 1;
		
		ext_ptr_store = OUTP3;
		extMemWt(reg_state.reg_out3);
		no_power = 1;
	}

	/* if the key switch is in the diagnostic position set the
	 * supervisor mode
	 */
	 /* default to supervisor mode while chasing down the spurious problems. */
	if (!reg_state.reg_in2.DIAG_SW_ON)
		cmdp.cmd_stat.SUP_MODE = 1;

#ifdef	KCAR_FAN_FULL_SPEED
		/* for KCAR force blower to high speed */
		if (mp_type == MP_KCAR)
			cmdp.cmd_stat.FAN_HI = 1;
#endif
	/* Key Switch Not in the Off Position and no other fatal errors, and FFSC
	   is not connected, then signal a power up condiction */
#ifdef	_005
	if (reg_state.reg_rb.KEY_SW_OFF && !no_power && !tasks.fw_logical.FWL_FFSC_PRESENT) {
		tasks.switches.FPS_KEY_BOUNCE = 1;
		to_value = ONE_SEC;
		timeout(KEY_TO);
	}
	
#else
	if (reg_state.reg_ra.KEY_SW_OFF && !no_power && !tasks.fw_logical.FWL_FFSC_PRESENT) {
		tasks.switches.FPS_KEY_BOUNCE = 1;
		to_value = ONE_SEC;
		timeout(KEY_TO);
	}

#endif

#endif

}

#ifndef	SPEEDO
/***************************************************************************
 *																									*
 *	midplane_clk_init:   Mid-plane clock logic initialization, including		*
 *								UST, I/O, NET, and TEST clocks							*
 *																									*
 ***************************************************************************/
void
midplane_clk_init() {

	if (mp_type == MP_LEGO) {
		/*UST Clock */
		i2c_cp.xfer_type = 0;
#ifdef	_005_MID
		i2c_cp.i2c_add = MP_44;
#else
		i2c_cp.i2c_add = MP_40;
#endif
		i2c_cp.i2c_sub_add = 0;
		i2c_cp.xfer_count = 1;
		i2c_cp.xfer_ptr = buffer;
#ifdef	_005_MID
		buffer[0] = 0xCF;
#else
		buffer[0] = 0x3F;
#endif
		temp = i2cAccess();
	}

	/* read the stored values in NVRAM for the Test, and Core Clock Select */
	i2c_cp.xfer_type = I2C_NVRAM;
	i2c_cp.i2c_add = NVRAM_ADD | ELSC_PAGE | I2C_READ;
	i2c_cp.i2c_sub_add = MP_CLK_2;
	i2c_cp.xfer_count = 1;
	i2c_cp.xfer_ptr = buffer;
	temp = i2cAccess();

	/* and now set the values, and if the access to NVRAM failed set them to the
	 * default value */
	if (mp_type != MP_KCAR) {
		/* Test and Core Clock Select */
		if (temp != 0)
			buffer[0] = TEST_CORE_DEFAULT;
		i2c_cp.xfer_type = 0;
		i2c_cp.i2c_add = MP_48;
		i2c_cp.i2c_sub_add = 0;
		i2c_cp.xfer_ptr = buffer;
		temp = i2cAccess();
	}

}
#endif //SPEEDO

/***************************************************************************
 *																									*
 *	nvram_init:   NVRAM initialization routine run at start up.					*
 *																									*
 ***************************************************************************/
void
nvram_init()
{
	/* Initialize NVRAM if magic_number is not valid */
	/* read magic number */
	i2c_cp.xfer_type = I2C_NVRAM;
	i2c_cp.i2c_add = NVRAM_ADD | ELSC_PAGE | I2C_READ;
	i2c_cp.i2c_sub_add = 0;
	i2c_cp.xfer_count = 1;
	i2c_cp.xfer_ptr = acp_buf;
	temp = i2cAccess();

#ifdef	SPEEDO
	/* in the event NVRAM is not functional, set supervisor mode */
	if (temp < 0)
		cmdp.cmd_stat.SUP_MODE = 1;
#endif


	if (acp_buf[0] != 0x37) {
		acp_buf[0] = 0x37;
		acp_buf[1] = 'n';
		acp_buf[2] = 'o';
		acp_buf[3] = 'n';
		acp_buf[4] = 'e';
		acp_buf[5] = 0;
		acp_buf[6] = 0;
#ifdef	SPEEDO
		acp_buf[7] = AUTO_BOOT_CFG;
#else
		acp_buf[7] = 0;
#endif
		acp_buf[8] = 0;
		acp_buf[9] = 0;
		acp_buf[0xA] = 0;		
		if (mp_type == MP_LEGO) 
			acp_buf[0xB] = TEST_CORE_DEFAULT;	/* Test Clock normal, Core Clock Disabled */
		else
			acp_buf[0xB] = 0;
		
		i2c_cp.xfer_type = I2C_NVRAM;
		i2c_cp.i2c_add = NVRAM_ADD | ELSC_PAGE;
		i2c_cp.i2c_sub_add = 0;
		i2c_cp.xfer_count = 0x0C;
		i2c_cp.xfer_ptr = acp_buf;
		temp = i2cAccess();
	}
	/* check for the fan set to high speed in NVRAM  &
	 * the auto power up mode, also read the module number for later display*/
	buffer[0] = 0;
	i2c_cp.xfer_type = I2C_NVRAM;
	i2c_cp.i2c_add = NVRAM_ADD | ELSC_PAGE | I2C_READ;
	i2c_cp.i2c_sub_add = ELSC_CFG;
	i2c_cp.xfer_count = 2;
	i2c_cp.xfer_ptr = buffer;
	temp = i2cAccess();

	if (buffer[0] & FAN_HI_CFG) {
		cmdp.cmd_stat.FAN_HI = 1;
#ifdef	SPEEDO
		reg_state.reg_out1.FAN_HI_SPEED = 1;
		ext_ptr_store = OUTP1;
		extMemWt(reg_state.reg_out1);
#else
		reg_state.reg_out3.FAN_HI_SPEED = 1;
		ext_ptr_store = OUTP3;
		extMemWt(reg_state.reg_out3);
#ifndef KCAR_FAN_FULL_SPEED
		if (mp_type == MP_KCAR) {
			reg_state.reg_out1.KCAR_HI_FAN_SPEED = 1;
			ext_ptr_store = OUTP1;
			extMemWt(reg_state.reg_out1);
		}
#endif

#endif	//SPEEDO
	}


	
#ifdef	SPEEDO
	if (buffer[0] & AUTO_BOOT_CFG)
		tasks.fw_logical.FWL_PWR_UP = 1;
#endif	//SPEEDO

}
/***************************************************************************
 *																									*
 *	sw_init:   Initializes all software specific configuration parameters	*
 *																									*
 ***************************************************************************/
void
sw_init()
{

	int i;

	hub_tas = '0';
	mfg_tst = 0;

#ifdef	SPEEDO
	led_state = 0;
	slave_cmd_param = 0;
	master_cmd_param = 0;
	slave_fan_stat = 0;
	slave_temp_stat = 'n';
	i2c_retry = 5;

#else
	pok_slot_no = 0;
#endif

	last_key_position = 0;
	
	tasks.env1 = 0;
	tasks.env2 = 0;
	tasks.switches = 0;
	tasks.fw_logical = 0;
	tasks.status = 0;
	tasks.i2c_status = 0;
	
#ifdef	SPEEDO
	mp_type = MP_LEGO;
#else
	/* read input register 2 and store initial conditions for mp_type, use
	 * a direct read because interrupts are not enabled yet.
	 */
	ext_ptr = INPT2;
	reg_state.reg_in2 = *ext_ptr;

	/* Capture the Mid-plane type */
	mp_type = reg_state.reg_in2 & 0xC0;
#endif
	
#ifdef	SPEEDO
	reg_state.reg_ra = 0x0C;
	reg_state.reg_rb = 0xC0;
	reg_state.reg_out1 = 0xC7;			/*reset & nmi active low */
#else
	reg_state.reg_ra = 0;
	reg_state.reg_rb = 0;
#ifdef	KCAR_FAN_FULL_SPEED
	reg_state.reg_out1 = 0x7F;			/*reset & nmi active low, kcar fan Full speed */
#else
	if (mp_type != MP_KCAR)
		reg_state.reg_out1 = 0x7F;			/*reset & nmi active low, PANIC Int High */
	else
		reg_state.reg_out1 = 0x7E;			/*reset & nmi active low, Fan low Speed  */
#endif //KCAR_FAN_FULL_SPEED
	
#endif
	
#ifndef	SPEEDO
	reg_state.reg_out2 = 0x8F;			/*5 & 3.45 margin & PS_SYS_OT active low */
	
#ifdef	KCAR_FAN_FULL_SPEED
		if (mp_type == MP_KCAR)	
			reg_state.reg_out3 = 0x28;			/* PSEN active low, fan High Speed */
		else
			reg_state.reg_out3 = 0x20;			/* PSEN active low */
#else
			
	reg_state.reg_out3 = 0x20;			/* PSEN active low */
	
#endif	//KCAR_FAN_FULL_SPEED
#endif	//SPEEDO

	/* clear the TCB */
	TCB.to_status = 0;
	TCB.to_status_1 = 0;
	TCB.key_switch = 0;
	TCB.nmi_button = 0;
	TCB.reset_button = 0;
	TCB.sw_delay = 0;
	TCB.fan_delay = 0;
	TCB.power_dn_delay = 0;
	TCB.nmi_execute = 0;
	TCB.rst_execute = 0;
	TCB.hbt_delay = 0;
	TCB.elsc_int_delay = 0;
	TCB.power_up_delay = 0;
#ifdef	SPEEDO
	TCB.led_blink = 0;
#endif

	/* Fan pulse counting data structure */
	fan_ptr.fan_add = FANS_PER_MUX;
	fan_ptr.fan_stat_1 = 0;
	fan_ptr.fan_stat_2 = 0;
	fan_ptr.fan_retry = 0;
	fan_ptr.fan_ov = 0;

	/* the remaining i2c data structures are set in i2c_init, because it is called
		every time a NMI or RST is executed.  Every time RST and NMI are executed
		the hub resets it's i2c part, therefore the ELSC i2c part must also be reset */
	cmdp.cpu_sel = SEL_AUTO;
	cmdp.cpu_acp = -1;
	cmdp.cpu_see = -1;
	cmdp.cmd_intr = 0;
	cmdp.cmd_stat = 0;
	/* Initialize Pass-Through / Local mode to Pass-Through and turn on echo*/
	cmdp.cmd_intr.PASS_THRU = 1;
	cmdp.cmd_stat.CH_ECHO = 1;


#ifdef INDIRECT_Q
	sci_inp  = &sci_inq;		/* Pointer to ACP input queue */
	sci_outp = &sci_outq;	/* Pointer to ACP output queue */
	i2c_inp  = &i2c_inq;		/* Pointer to I2C input queue */
	i2c_outp = &i2c_outq;	/* Pointer to I2C output queue */
#endif


	/* initialize the heart beat interval */
	hbt_interval = 0;
	hbt_state = 0;

}

/***************************************************************************
 *									    															*
 *	LED_int:  Displays Power On values on front panel display					*
 *									  																*
 ***************************************************************************/
void
LED_int()
{

#ifdef	SPEEDO
	/* Cycle through the tri-color LED options */

	/* RED On */
	reg_state.reg_out1.LED_RED = 1;
	ext_ptr_store = OUTP1;
	extMemWt(reg_state.reg_out1);

	to_value = MSEC_500;
	delay();

	/* Amber On */
	reg_state.reg_out1.LED_GREEN = 1;
	ext_ptr_store = OUTP1;
	extMemWt(reg_state.reg_out1);

	to_value = MSEC_500;
	delay();

	/* Green On */
	reg_state.reg_out1.LED_RED = 0;
	ext_ptr_store = OUTP1;
	extMemWt(reg_state.reg_out1);

	to_value = MSEC_500;
	delay();

	/* LED Off */
	reg_state.reg_out1.LED_GREEN = 0;
	ext_ptr_store = OUTP1;
	extMemWt(reg_state.reg_out1);
	
#else
	/* turn ON the discrete LEDs */
	reg_state.reg_out3.SYS_OT_LED = 1;
	reg_state.reg_out3.FAN_HI_SPEED = 1;

	ext_ptr_store = OUTP3;
	extMemWt(reg_state.reg_out3);
	
	reg_state.reg_out1.DC_OK = 0;

	ext_ptr_store = OUTP1;
	extMemWt(reg_state.reg_out1);

	/* Initialize the device */
	dspControl(DSP_CLEAR);
	dspControl(BRT_FULL);
	dspControl(DSP_TEST | DSP_BLINK);
	to_value = THREE_SEC;
	delay();
	dspControl(DSP_CLEAR);
	dspControl(DSP_DISABLE_CLEAR);
	dspControl(BRT_FULL);

	/* turn OFF the discrete LEDs */
	reg_state.reg_out3.SYS_OT_LED = 0;
	reg_state.reg_out3.FAN_HI_SPEED = 0;

	ext_ptr_store = OUTP3;
	extMemWt(reg_state.reg_out3);
	
	reg_state.reg_out1.DC_OK = 1;

	ext_ptr_store = OUTP1;
	extMemWt(reg_state.reg_out1);

	/* Display the Firmware Version number */
	dspMsg(version);

#endif	// SPEEDO
}
