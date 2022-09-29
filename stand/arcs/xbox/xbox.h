/* XBOX.H */

/* This include file maps the various input and output ports to the 
 * ELSC 17C44 based design
 *
 *		(c) Copright 1997	Silicon Graphics Compter Systems
 *
 */

/* PORTA  Bit Settings */
#define	PSOK			1				/* Input  */
#define	PWRON_L		2				/* Output */
#define	SYSRST_L		3				/* Output */
#define	INPUT_MASK_PA	0x2		/* mask out the output bits */
#define	PSOK_MASK	0x2

/* PORTB	Bit Settings */
#define	TEMP_OVER_L		0				/* Input */
#define	FAN_MXIN			1				/* Input */
#define	FAN_MX0			2				/* Output */
#define	FAN_MX1			3				/* Output */
#define	TEMP_UNDER_L	4				/* Input */
#define	MFG_MODE_L		5				/* Input */
#define	X9_RMTOK			6				/* Input */
#define	XA_RMTOK			7				/* Input */

#define	INPUT_MASK_PB	0xE1		/* mask out the output and not used bits */

/* any external address will work for all external accesses */
#define	EXT_ADD		(long )	0x6000
#define	LED_REG		0
#define	STATUS_REG	1
#define	PEN_REG		2

/* External Input PORTS */
#define	PWR_ADD		(long ) 0x6000
#define	SC_LPOK		6			/* Power OK Monitor Signal */
#define	SC_PENB		6			/* Power Enable */

/* External Output Ports */
#define	X0_FAN_FAIL				0						/* XO Fan Failure */
#define	X1_FAN_FAIL				1						/* X1 Fan Failure */
#define	PCI_FAN_FAIL			2						/* PCI Fan Failure */
#define	OVER_TEMP				3						/* Over Temperature Failure */
#define	REDUNDANT_PWR_FAIL	4						/* Redundant Power Failure */

#define	FP_GREEN_BIT			0
#define	FP_RED_BIT				1
#define	PCI_GREEN_BIT			2
#define	PCI_YELLOW_BIT			3
#define	X0_GREEN_BIT			4
#define	X0_YELLOW_BIT			5
#define	X1_GREEN_BIT			6
#define	X1_YELLOW_BIT			7

/* Misc */
#define	HIGH			1
#define	LOW			0

#define	MAX_CPU_CNT		5		/* includes the slave ELSC */
#define	FANS_PER_MUX 	3

struct MONITOR {
	bits 	env;
	bits	fw_logical;
	bits	status;
};

struct SYS_STATE {
	bits	reg_ra;
	bits	reg_rb;
	bits	reg_pwr_add;
	bits	reg_fail_leds;
	bits	reg_xbox_status;
};

/*
 * The first four registers listed are flags indicating action needs to be
 * taken. Once the action is taken, the flag bit is reset to zero, and
 * no further action is necessary.
 *
 * The status register keep current status of the system and prevent multiple
 * actions from occuring for a single event.
 */

/* ENVironmental 1 register bits */
#define	ENV_PON				6		/* Power On Reset Flag set in initial conditions */
#define	ENV_PS_FAIL			5		/* Power Supply Fail */
#define	ENV_POK				4		/* Power_OK Signal Failure */
#define	ENV_TEMP_OK			2		/* System Temperature has returned to normal*/
#define	ENV_TMP_EN			1		/* Enable Temperature monitoring */
#define	ENV_FAN_RUNNING	0		/* Fan tach is being recevied */

/* FirmWare Logical register bits */
#define	FWL_PWR_UP			2			/* Power Up */
#define	FWL_PWR_DN			1			/* Power Down */
#define	FWL_SFT_PWR_DN		0			/*	Delayed Power Down */

/* Monitor Status bits */
#define STAT_SYSPWR				3		/* System Power Up Status; 0-off 1-On*/		


struct FAN_MON {
	char 				fan_add;
	bits 				fan_stat_1;
	bits				fan_stat_2;
	char				fan_retry;
	char				fan_ov;
	unsigned long	fan_pulse_0;
	unsigned long	fan_pulse_1;
	unsigned long	fan_period;
	char				fan_add_mea;
};

#define STAT1_FAN_0		0			/* Status Bit for FAN_0 (X0); 1 = failure */
#define STAT1_FAN_1		1			/* Status Bit for FAN_1 (X1); 1 = failure */
#define STAT1_FAN_2		2			/* Status Bit for FAN_2 (PCI);1 = failure */

#define STAT2_PULSE_NO	1			/* measured pulse number 1:0 */
#define STAT2_CNT_CHK	2			/* flag for monitor routine to check */
#define STAT2_FAN_FL		4			/* flag to indicate one fan has failed */
#define STAT2_FAN_M_FL	5			/* flag to indicate >1 fans have failed*/
#define STAT2_FAN_CHK	6			/* Fan pulse counting enabled*/
#define STAT2_FAN_LRTR	7			/* Lock Rotor flag */

#define	FAN_MAX_RETRY	5			/* Maximum retry count for failed fan */


#define X_FAN_HI_CNT		 0x7E		/* (3700 RPM) 0x5A00 >> 8 */
#define PCI_FAN_HI_CNT	 0x9C		/* (3000 RPM) 0x5A00 >> 8 */

/* Timer Control Block for Interrupt Timer */
struct TIMER {
	bits	to_status;
	unsigned long	sw_delay;
	unsigned long	fan_delay;
	unsigned long	led_blink;
	unsigned long	pwr_down;
};
/* status word bit assignments */
#define	DELAY_TO			0		/* bit position of software delay status */
#define	FAN_TO			1		/* bit position of software delay status */
#define	LED_BLINK_TO	2		/* Speedo Tri-color LED Blink Timeout */
#define	PWR_DOWN_TO		3		/* Delayed power down after a fan failure or temperature failure */

/* Hardware Interrupt Timers declrations */
#define SYS_OFF			0			/* index into TCB for system Off */

/* timeout values 1 tick = 1 msec*/
#define TICKS_SEC			(unsigned long) 1000		/* # number of ticks in 1 sec */
#define MSEC_10			(unsigned long) 10
#define MSEC_50			(unsigned long) 50
#define MSEC_100			(unsigned long) 100
#define MSEC_200			(unsigned long) 200
#define MSEC_250			(unsigned long) 250
#define MSEC_400			(unsigned long) 400
#define MSEC_500			(unsigned long) 500
#define ONE_SEC			(unsigned long) 1000		
#define TWO_SEC			(unsigned long) 2000		
#define THREE_SEC			(unsigned long) 3000		
#define FIVE_SEC			(unsigned long) 5000	
#define TEN_SEC			(unsigned long) 10000
#define THIRTY_SEC		(unsigned long) 30000	
#define THIRTYFIVE_SEC	(unsigned long) 35000
#define ONE_MINUTE		(unsigned long) 60000

#define	LED_BLINK_TIME			MSEC_500
#define	FAN_LCK_ROTOR_DELAY	TWO_SEC
#define	SOFT_PWR_DN_DELAY		ONE_MINUTE

/* Software micro second delay routine values.
	the min delay is 5 usec with inrements of 5 usecs.
 */
#define USEC_5				1
#define USEC_65			25
#define USEC_80			31
#define USEC_100			39
#define USEC_500			199

/* LED MANAGEMENT */
/*
 * the following defines are used to control the various LEDS,
 * for color and blink mode
 */
 
#define	FP_LED			0x01
#define	PCI_LED			0x02
#define	X0_LED			0x04
#define	X1_LED			0x08

#define	LED_OFF			0x00
#define	LED_GREEN		0x01
#define	LED_YELLOW		0x02
#define	LED_RED			0x04

#define	LED_BLINK		0x80

/* Blink Flags */
#define	FP_LED_BLINK	1
#define	PCI_LED_BLINK	2
#define	X0_LED_BLINK	3
#define	X1_LED_BLINK	4





