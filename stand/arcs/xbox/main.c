#pragma option v  
/* main.c */
/*
 * Entry point for the c modules for the SN0 XBOX System controller XMSC
 * firmware.
 */
 

#include <\mpc\17c44.h>
#include <\mpc\ctype.h>
#include <\mpc\math.h>
#include <\mpc\mpc16.lib>
#include <proto.h>
#include <xbox.h>

const	char	version[] = {'V','E','R',' ','0','.','D',' '};

/* Global Structure Declarations */
struct MONITOR tasks;				/* data structure containing task status */
struct FAN_MON fan_ptr;				/* data structure containing task status */
struct SYS_STATE reg_state;		/* data structure containing copies of reg. */
struct TIMER TCB;						/* data structure containing task status */
unsigned long to_value;				/* value passed to timeout routine */
int	mfg_mode;						/* used to disable shutdown for tempeature or fan failure */
int	NoPower;							/* Flag to prevent power from being reapplied after an error */
int 	fp_color;						/* color fp led is set to, used to remember color during blinking */

unsigned long count;
unsigned long tmp_count;
int temp;
bits t_reg;
char reg;
bits	blink_state;				/* flag to blink various leds */
long	sw_ms_counter;			/* msec counter for software loop timeout control */
char	shutdown_pending;		/* flag to allow a shutdown only once during a failure */

char int_state;						/* interrupt state flag*/
char int_state_reg;					/* context save location for int state during an int */


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

	/* Software Initialization */
	sw_init();

	/* Hardware Initialization */
	hw_init();

	/* Initialize the display */
	LED_int();

	/* Check for initial Conditions */
	init_cond();
	
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

	T0STA = 0;
	T0STA.T0CS = 1;

	/* PORTB Register, Bits 7-4, 1-1 Inputs and 3-2 Outputs;		
	 */
	PORTB = 0;
	DDRB = 0xF3;

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

	/* enable pulse capturing for fans.  XBOX  uses CA2 for fan pules measurements
	 */
	TCON1.CA2ED0 = 1;
	
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

	/* 
	 * Enable Interrupts:  ***************************************************
	 * Should be the last task of this function! 
	 *
	 */
	INTSTA = 0;
	INTSTA.PEIE = 1;			// Peripheral Interrupt Enable
	INTSTA.INTIE = 1;			// External Interrupt On INT Enable Bit
	
	PIE = 0;

	
	PIE.TMR3IE = 1;			// Timer 3 Interrupt Enable
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


	/* read the initial state of the hardware registers */
	reg_state.reg_ra = PORTA;
	reg_state.reg_rb = PORTB;

	/* set the manufacturing mode */
	t_reg = PORTB;
	mfg_mode = t_reg.MFG_MODE_L ? 0 : 1;

	/* Check for power up */
	if (reg_state.reg_rb.X9_RMTOK ||reg_state.reg_rb.XA_RMTOK) { 
		tasks.fw_logical.FWL_PWR_UP = 1;
	}

	/* force over temperature reading to be normal */
	reg_state.reg_rb.TEMP_OVER_L = 1;
}


/***************************************************************************
 *																									*
 *	sw_init:   Initializes all software specific configuration parameters	*
 *																									*
 ***************************************************************************/
void
sw_init()
{

	blink_state = 0;
	fp_color = 0x03;
	tasks.env = 0;
	tasks.fw_logical = 0;
	tasks.status = 0;
	mfg_mode = 0;
	NoPower = 0;
	shutdown_pending = 0;
	
	reg_state.reg_ra 				= 0x2;	/* PWRON_L set high */
	reg_state.reg_rb 				= 0x1;	/* Over temperature set to normal */
	reg_state.reg_pwr_add 		= 0x0;
	reg_state.reg_xbox_status 	= 0x0;
	reg_state.reg_fail_leds 	= 0x0;
	
	/* clear the TCB */
	TCB.to_status = 0;
	TCB.sw_delay = 0;
	TCB.fan_delay = 0;
	TCB.led_blink = 0;
	TCB.pwr_down = 0;

	/* Fan pulse counting data structure */
	fan_ptr.fan_add = FANS_PER_MUX;
	fan_ptr.fan_stat_1 = 0;
	fan_ptr.fan_stat_2 = 0;
	fan_ptr.fan_retry = 0;
	fan_ptr.fan_ov = 0;

}

/***************************************************************************
 *									    															*
 *	LED_int:  Displays Power On values on front panel display					*
 *									  																*
 ***************************************************************************/
void
LED_int()
{

	/* Cycle through the tri-color LED options */

	/* LEDs Off */
	cntLED(FP_LED|PCI_LED|X0_LED|X1_LED, LED_OFF);


	/* YELLOW On */
	to_value = MSEC_250;
	cntLED(FP_LED, LED_YELLOW);
	delay();
	
	cntLED(PCI_LED, LED_YELLOW);
	delay();

	cntLED(X0_LED, LED_YELLOW);
	delay();

	cntLED(X1_LED, LED_YELLOW);

	/* wait 250 mili-seconds between colors */
	to_value = MSEC_250;
	delay();

	/* LEDs Off */
	cntLED(FP_LED|PCI_LED|X0_LED|X1_LED, LED_OFF);

	/* Green On */
	to_value = MSEC_250;
	cntLED(FP_LED, LED_GREEN);
	delay();
	
	cntLED(PCI_LED, LED_GREEN);
	delay();

	cntLED(X0_LED, LED_GREEN);
	delay();

	cntLED(X1_LED, LED_GREEN);

	to_value = MSEC_250;
	delay();

	/* Front Panel Led RED on */
	cntLED(FP_LED , LED_RED);
	
	to_value = MSEC_500;
	delay();
	
	/* LEDs Off */
	cntLED(FP_LED | PCI_LED | X0_LED | X1_LED, LED_OFF);
	
}
