#pragma option -l;  // Disable header inclusion in listing
/*
		Silicon Graphics Computer Systems
        Header file for Lego Entry Level System Controller

                        elsc.h

        (c) Copyright 1995   Silicon Graphics Computer Systems

		Brad Morrow
*/


/* Misc */
#define	OPR_ACTION	1			/* operator initiated action */
#define 	HUB_ACTION	2			/* hub initiated action */
#define	HIGH			1
#define	LOW			0

#ifdef	SPEEDO
#define	MAX_CPU_CNT		5		/* includes the slave ELSC */
#define	FANS_PER_MUX 	3
#define	GREEN				1
#define	RED				2
#define	AMBER				3
#define	LED_OFF			4
#define	LED_BLINK		1
#define	LED_SOLID		2
#else
#define	MAX_CPU_CNT		8
#define	FANS_PER_MUX 8
#endif

/* should be in 16c74.h */
#define CCP2IE	0

struct MONITOR {
	bits 	env1;
	bits 	env2;
	bits	switches;
	bits	fw_logical;
	bits	status;
	bits	i2c_status;
};

#ifdef	SPEEDO
struct SYS_STATE {
	bits	reg_ra;
	bits	reg_rb;
	bits	reg_in2;
	bits	reg_out1;
};
#else
struct SYS_STATE {
	bits	reg_ra;
	bits	reg_rb;
	bits	reg_in2;
	bits	reg_out1;
	bits	reg_out2;
	bits	reg_out3;
};
#endif

/*
 * The first four registers listed are flags indicating action needs to be
 * taken.  The action may be to send a message to the hub chip, or shut the 
 * system down.  Once the action is taken, the flag bit is reset to zero, and
 * no further action is necessary.
 *
 * The status register keep current status of the system and prevent multiple
 * actions from occuring for a single event.
 */

/* ENVironmental 1 register bits */
#define	ENV1_PF			7		/* Power Fail */
#define	ENV1_PON			6		/* Power On Reset Flag set in initial conditions */
#define	ENV1_PS_FAIL	5		/* Power Supply Fail */
#define	ENV1_POK			4		/* Power_OK Signal Failure */
#define	ENV1_SYS_HT		2		/* System High Temperature */
#define	ENV1_TEMP_OK	1		/* System Temperature has returned to normal*/
#define	ENV1_TMP_EN		0		/* Enable Temperature monitoring */

#ifdef	SPEEDO
/* ENVironmental 2 register bits */
#define	ENV2_SPEEDO_SLAVE					0		/* Speedo ELSC is a Slave */
#define	ENV2_SPEEDO_MASTER				1		/* Speedo ELSC is a Master */
#define	ENV2_SPEEDO_MS_ENABLE			2		/* Speedo ELSC Master/Slave Enable */
#endif
#define	ENV2_FAN_RUNNING					7		/* Fan tach is being recevied */

/* Front Panel Switch register bits */
#define	FPS_RST_BOUNCE	7		/* Active Reset Button Debounce Period */
#define	FPS_NMI_BOUNCE	6		/* Active NMI Button Debounce Period */
#define	FPS_KEY_BOUNCE	5		/* Active Key Switch Debounce Period */
#define	FPS_KEY_DIAG	4		/* Key Switch Diag Position */
#define	FPS_KEY_ON		3		/* Key Switch On Position */
#define	FPS_KEY_OFF		2		/* Key Switch Off Position */
#define	FPS_RESET		1		/* Reset Button Depressed */
#define	FPS_NMI			0		/* NMI Button Depressed */

/* FirmWare Logical register bits */
#define	FWL_ACP_INT			7			/* acp (pass through data) is in process */
#define	FWL_PS_OT			6
#define	FWL_FAN_SPEED		5			/* Request to lower Fan Speed */
#define	FWL_HEART_BT		4			/* Heart Beat */
#define	FWL_PWR_UP			3			/* Power Up */
#define	FWL_PWR_DN			2			/* Power Down */
#define	FWL_FFSC_PRESENT 	1			/* FFSC Present Flag */
#define	FWL_REMOVE_PWR		0			/*	Remove power immediately */

/* Monitor Status bits */
#define STAT_DIAG_KEY			6		/* Set when key is set to diag.*/
#define STAT_INTERRUPT			5		/* When set, and interrupt is active */
#define STAT_SYSPWR				3		/* System Power Up Status; 0-off 1-On*/		
#define STAT_SYSHT				2		/* System is currently at High Temp */
#define STAT_PENDING_SHUTDOWN	1

/* Mid-plane type */
#define	MP_LEGO			0xC0			/* LEGO and KEGO */
#define	MP_KCAR			0x40
#define	MP_METROUTER	0x00

struct FAN_MON {
	char 				fan_add;
	bits 				fan_stat_1;
	bits				fan_stat_2;
	char				fan_retry;
	char				fan_ov;
	unsigned long	fan_pulse_0;
	unsigned long	fan_pulse_1;
	unsigned long	fan_period;
};

#define STAT1_FAN_0		0			/* Status Bit for FAN_0; 1 = failure */
#define STAT1_FAN_1		1			/* Status Bit for FAN_0; 1 = failure */
#define STAT1_FAN_2		2			/* Status Bit for FAN_2; 1 = failure */
#define STAT1_FAN_3		3			/* Status Bit for FAN_3; 1 = failure */
#define STAT1_FAN_4		4			/* Status Bit for FAN_4; 1 = failure */
#define STAT1_FAN_5		5			/* Status Bit for FAN_5; 1 = failure */
#define STAT1_FAN_6		6			/* Status Bit for FAN_6; 1 = failure */
#define STAT1_FAN_7		7			/* Status Bit for FAN_7; 1 = failure */
#define STAT1_FAN_8		8			/* Status Bit for FAN_8; 1 = failure */

#define STAT2_FAN_9		0			/* Status Bit for FAN_9; 1 = failure */
#define STAT2_PULSE_NO	1			/* measured pulse number 1:0 */
#define STAT2_CNT_CHK	2			/* flag for monitor routine to check */
#define STAT2_FAN_HI		3			/* flag to indicate high speed setting. */
#define STAT2_FAN_FL		4			/* flag to indicate one fan has failed */
#define STAT2_FAN_M_FL	5			/* flag to indicate >1 fans have failed*/
#define STAT2_FAN_CHK	6			/* Fan pulse counting enabled*/
#define STAT2_FAN_LRTR	7			/* Lock Rotor flag */

#define	FAN_MAX_RETRY	5			/* Maximum retry count for failed fan */


#define FAN_HI_CNT		 0x5A		/* (2600 RPM) 0x5A00 >> 8 */
#define FAN_NM_CNT		 0x7C		/* (1900 RPM) 0x7CFF >> 8 */
#define KCAR_FAN_HI_CNT	 0xA5		/* (2600 RPM) 0x5A00 >> 8 */
#define KCAR_FAN_NM_CNT	 0xEA		/* (1900 RPM) 0x7CFF >> 8 */

/* I2C Bus / Command Status */
#define I2CS_DA			0			/* Transmission is DATA (1), ADDRESS (0) */
#define I2C_SLAVE_CMD	1			/* Slave command is out standing */
#define I2C_MASTER_XMIT	2			/* Master-Transmit command is scheduled */
#define I2C_MASTER_REC	3			/* Master-Receive command is scheduled */

#define I2C_STANDARD	0			/* 100 KHz Mode */
#define I2C_FAST		1			/* 400 KHz Mode */

/* I2C Bus Command */
#define READ_ME			0x13		

/* Heart Beat Register States */
#define	HBT_READY	0					/* Heart Beat timeout inactive */
#define	HBT_TIMEOUT	1					/* A heart beat timeout starts the sequence */
#define	HBT_NMI		2					/* NMI state of timeout sequence */
#define	HBT_RESTART	3					/* Power Cycle state of timeout sequence */


/* Timer Control Block for Interrupt Timer */
struct TIMER {
	bits	to_status;
	bits	to_status_1;
	unsigned long	key_switch;
	unsigned long	nmi_button;
	unsigned long	reset_button;
	unsigned long	sw_delay;
	unsigned long	fan_delay;
	unsigned long	power_dn_delay;
	unsigned long	nmi_execute;
	unsigned long 	rst_execute;
	unsigned long	hbt_delay;
	unsigned long	led_blink;
	unsigned long 	elsc_int_delay;
	unsigned long	power_up_delay;
};
/* status word bit assignments */
#define	KEY_TO			0		/* bit position of key switch status */
#define	NMI_TO			1		/* bit position of NMI button status */
#define	RESET_TO			2		/* bit position of Reset button status */
#define	DELAY_TO			3		/* bit position of software delay status */
#define	FAN_TO			4		/* bit position of software delay status */
#define	POWER_DN_TO		5		/* bit position of power down/cycle status */
#define	NMI_EX_TO		6		/* bit position of NMI Execute status */
#define 	RST_EX_TO		7		/* bit position of RST Execute status */
// begin second status word 
#define	HBT_TO			8		/* timeout entry of HBT timeout */
#ifdef	SPEEDO
#define	LED_BLINK_TO	9		/* Speedo Tri-color LED Blink Timeout */
#endif
#define	ELSC_INT_TO		0xa	/* ELSC to Hub interrupt timeout flag */
#define	POWER_UP_TO		0xb	/* Power Cycle timeout flag */


/* bit positions of second status word */
#define	HBT_INT			0		/* bit postion of HBT timeout */
#define	LED_BLINK		1
#define	ELSC_INT_TD		2		/* status bit for ELSC to Hub interrupt timeout */
#define	PWR_UP_TD		3		/* time delay for power up */


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

#define	LED_BLINK_TIME			MSEC_500
#define	I2C_TIMEOUT				FIVE_SEC
#define	ELSC_INT_DELAY			ONE_SEC
#define	SOFT_PWR_DOWN_DELAY	THREE_SEC
#define	FAN_LCK_ROTOR_DELAY	TWO_SEC
/* Software micro second delay routine values.
	the min delay is 5 usec with inrements of 5 usecs.
 */
#define USEC_5				1
#define USEC_65			25
#define USEC_80			31
#define USEC_100			39
#define USEC_500			199


/* NVRAM Declarations */
#define	NV_DEVICE	0b10100000
#define	READ			1
#define	WRITE			0


/* NV RAM Mapping */
#define	NVRAM_ADD		0xA0				/* Base Address for NVRAM */
#define	ELSC_PAGE		0x0E				/* ELSC local page */

/* NVRAM offset mapping for ELSC */
#define	MAGIC_NO		0x00				/* magic number used for initialization */
#define	PASS_WD		0x01				/* pointer to password (4 bytes in length) */
#define	DEBUG_REG	0x05				/* pointer to debug register */
#define	ELSC_CFG		0x07				/* ELSC Configuration info */
#define	MOD_NUM 		0x08				/* Node Module Number */
#define	SPARE			0x09				/* ELSC Spare locations */
#define	MP_CLK_1		0x0a				/* Mid-plane clock select register storage 1*/
#define	MP_CLK_2		0x0b				/* Mid-plane clock select register storage 2*/


/* ELSC Configuration Register declarations */
#define	FAN_HI_CFG		0x01				/* Set Fan to high speed through power cycles */
#define	AUTO_BOOT_CFG	0x02				/* Set Auto Power Up mode */
