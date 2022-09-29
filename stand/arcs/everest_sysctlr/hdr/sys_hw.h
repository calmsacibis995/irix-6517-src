
/****** SYS_HW.H **********/


/****		Time and Date Clock, and NV RAM Structure      *****/
#define	ERROR_MAX	(char) 10
struct EVENT {
	unsigned char	nv_ev_code;			/* Event  Code */
	unsigned char	nv_ev_time[6];		/* Event  Time and Date (HMMDY) BCD*/
};
struct RTC 	{
	unsigned char	t_sec;				/* Second Counter (BCD) */
	unsigned char	t_sec_alrm;			/* Second Counter Alarm (BCD) */
	unsigned char	t_min;				/* Minute Counter (BCD) */
	unsigned char	t_min_alrm;			/* Minute Counter Alarm (BCD) */
	unsigned char	t_hour;				/* Hour Counter (BCD) */
	unsigned char	t_hour_alrm;			/* Hour Counter Alarm (BCD) */
	unsigned char	t_daycnt;			/* day of week Counter */
	unsigned char	t_day;				/* Day Counter (BCD) */
	unsigned char	t_mon;				/* Month Counter (BCD) */
	unsigned char	t_year;				/* Year Counter (BCD) */
	unsigned char 	t_rega;				/* Control Register A */
	unsigned char 	t_regb;				/* Control Register B */
	unsigned char 	t_regc;				/* Control Register C */
	unsigned char 	t_regd;				/* Control Register D */
	unsigned char	nv_sanity;			/* Used to detect First Power Up */
	unsigned char	nv_pcntl;			/* Memory copy of power control Reg */
	unsigned char	nv_ser_add;			/* Memory copy of serial address reg */
	unsigned char	nv_lcd_cntl;		/* Memory copy of LCD control reg */
	unsigned char	nv_status;			/* System Status reg */
	unsigned char	nv_cmd_chk;			/* Command length checking flag */
	unsigned char	nv_rfu[16];			/* Not Used */
	unsigned char	nv_fwver[6];		/* Firmware copy of FW Version */
	unsigned char	nv_ser_cksum;		/* System Serial Number */
	unsigned short	nv_irix_sw_not;		/* IRIX Virtual Debug DIP Switch Comp.*/
	unsigned short	nv_irix_sw;			/* IRIX Virtual Debug DIP Switch */
	unsigned char	nv_ser_num[10];		/* System Serial Number */
	unsigned char	nv_event_ctr;		/* Number of events stored */
	struct	EVENT 	events[ERROR_MAX];	/* Event logging locations*/
};


#define	L_RTC	((struct RTC *)0x0100) /* RTC for time-of-day clock */
#define RTC_12H		(char) 0x00			/* Control Register B Value */
#define RTC_24H		(char) 0x02			/* Control Register B Value */
#define RTC_BCD		(char) 0x00			/* Control Register B Value */
#define RTC_BINARY	(char) 0x04			/* Control Register B Value */
#define RTC_SQWE	(char) 0x08			/* Control Register B Value */
#define RTC_SETBIT	(char) 0x80			/* Control Register B Value */

#define RTC_UIP		(char) 0x80			/* Reg A Update In Process flag */

/************ EXTERNAL REGISTER ADDRESSES AND ASSOCIATED MASKS	*************/

/******* Blower Assembly output speed control register ******/
#define L_BLOW_SET	(char *) 0x0180		/* Blower Assembly */

/******* Power Enables, Back Plane NMI, SCLR and PCLR ******/
#define L_PWR_CNT	(char *) 0x0181		/* Address of Power Control Reg */

#define PENA		(unsigned char) 0x01			/* Power Enable A */
#define PENB		(unsigned char) 0x02			/* Power Enable B */
#define PENC		(unsigned char) 0x04			/* Power Enable C */
#define BP_NMI		(unsigned char) 0x08			/* Back Plane NMI */
#define PCLR		(unsigned char) 0x10			/* PCLR Signal*/
#define SCLR		(unsigned char) 0x20			/* SCLR Signal*/
#define KILL_PWR	(unsigned char) 0x40			/* Remove 48 Vdc*/
#define RI_ON		(unsigned char) 0x80			/* Remote Inhibit*/

/******* CPU Serial Address bits, and Front Panel Fault LED ******/
#define L_SER_ADD	(char *) 0x0182			/* Address of Serial Address Reg */
#define PENE		(unsigned char) 0x40	/* Power Enable E */
#define PEND		(unsigned char) 0x80	/* Power Enable D */

/****** Maskable Interupts; POK_X, Front Panel Switches, and Over Temperature*/
#define	L_IRQ			(char *) 0x0183	/* Power OK/Over temp/FP Key Register*/
#define	IRQ_EXECUTE		(char) 0x01		/* Front Panel Execute Switch */
#define	IRQ_SCRLDN		(char) 0x02		/* Front Panel Scroll Down Switch */
#define	IRQ_SCRLUP		(char) 0x04		/* Front Panel Scroll Up Switch */
#define	IRQ_MENU		(char) 0x08		/* Front Panel Menu Switch */
#define	IRQ_KEY			(char) 0x10		/* KEY Switch Off Interrupt */
#define	IRQ_SCLR		(char) 0x20		/* SCLR Interrupt */
#define	IRQ_SER_PRT		(char) 0x40		/* External Serial Port Interrupt */

#define	L_XIRQ			(char *) 0x0187	/* XIRQ Interrupt Register */
#define	XIRQ_PFW		(char) 0x01		/* Power Failure Warnings*/
#define	XIRQ_POK_A		(char) 0x06		/* Power OK A Fault*/
#define	XIRQ_POK_B		(char) 0x04		/* Power OK B Fault*/
#define	XIRQ_POK_C		(char) 0x02		/* Power OK C Fault*/
#define	XIRQ_POK_D		(char) 0x10		/* Power OK D Fault*/
#define	XIRQ_POK_E		(char) 0x20		/* Power OK E Fault*/
#define XIRQ_OVERTEMP	(char) 0x08		/* Backplane/Chassis Overtemp Fault */

/**************************** LCD Displays *****************************/

#define	L_LCD_WDAT	(char *) 0x0184			/* LCD Data Write Register */
#define	L_LCD_CNT	(char *) 0x0185			/* LCD Control Register */
#define	L_LCD_RDAT	(char *) 0x0186			/* LCD Data Read Register */
#define FAULT_LED_L	(unsigned char) 0x80	/* Front Panel Fault LED */

/* PORTD definitions */
#define CY_BUS_WT	(char) 0x04			/* Bus write flag from CY325 cntrl */
#define TERMINATOR	(char) 0x20			/* Terminator Sys / CY325 Front Panel */


			/****Hitachi 2 line display commands *****/

#define HD_CLEAR		(char) 0x01			/* Clears Display */
#define HD_HOME			(char) 0x02			/* Sets Cursor Home */
#define HD_C_DEC_NSFT	(char) 0x04			/* Decrement Cursor/No Shift */
#define HD_C_DEC_SFT	(char) 0x05			/* Decrement Cursor/Shift Display*/
#define HD_C_INC_NSFT	(char) 0x06			/* Increment Cursor/No Shift */
#define HD_C_INC_SFT	(char) 0x07			/* Increment Cursor/Shift Display*/

/* the next 4 display control statements should be "or'd" for combinations */
#define HD_DSP_OFF		(char) 0x08			/* Display OFF*/
#define HD_DSP_ON		(char) 0x0C			/* Display On/Cursor off/Solid cur*/
#define HD_DSP_CON		(char) 0x0A			/* Cursor on*/
#define HD_DSP_BLK		(char) 0x09			/* Blink Cursor */
#define HD_DSP_AON		(char) 0x0F			/* On w/ Blinking Cursor*/

/* Cursor Movement */
#define HD_CUR_LEFT		(char) 0x10			/* Moves Cursor Left */
#define HD_CUR_RIGHT	(char) 0x14			/* Moves Cursor Right */
#define HD_DSP_LEFT		(char) 0x18			/* Moves Display Left */
#define HD_DSP_RIGHT	(char) 0x1C			/* Moves Display Right */

#define HD_FNC_SET		(char) 0x38			/* 8 bit, 2 Lines, 5X7 dots */
#define HD_CG_ADD		(char) 0x40			/* Sets CG RAM Address (d0:d5)*/
#define HD_DD_ADD		(char) 0x80			/* Sets DD RAM Address (d0:d6)*/

#define HD_CNT_REG		(char) 0x00			/* Select Control Register */
#define HD_DIS_RST		(char) 0x02			/* Display Reset ??  */
#define HD_DAT_REG		(char) 0x04			/* Register Select & CY BCKLIGHT */
#define HD_DIS_CE		(char) 0x08			/* ?? */
#define	HD_E			(char) 0x10			/* E Clock */
#define	HD_DIS_RD		(char) 0x20			/* ?? */
#define	HD_READ			(char) 0x40			/* Read #Write Select & CY bus cnt*/

#define HD_HISTO_SCALE	(float) 0.16		/* Histogram scaling of 16/100 */

			/****Cybernetic CY325 Display Controller ***/

#define	CY_325_RDY			(char) 0x80		/* CY Ready to Accept Char */

/* External Control Register */
#define CY_325_WT_L		(char) 0x01		/*Active low write signal */

/*Control Commands */
#define	CY_CMD_MODE		(char) 3			/* Command Mode */
#define	CY_DSP_MODE		(char) 4			/* Display Mode */
#define	CY_KLEAR		(char) 0x11			/* Klear Current Window */
#define	CY_CR			(char) 0x0D			/* Carriage Return */
#define	CY_SPL_FONT_OUT	(char) 0x0E			/* Select Special Font (Shift out)*/
#define	CY_SPL_FONT_IN	(char) 0x0F			/* Select Special Font (Shift in)*/
#define	CY_WIN_SWAP		(char) 0x17			/* Swap Window */

/* Mode Register Assignments */
/* Mode Register 0 Window Status*/
#define	CY_M0_SCROLL		(char) 0x01		/* Scroll Display */
#define CY_M0_AUTOINC		(char) 0x02		/* Inc Cursor After Write */
#define CY_M0_VERTBAR		(char) 0x04		/* historgram Vertical Bars */
#define CY_M0_GLOBAL		(char) 0x08		/* Global Pixel Plotting */
#define CY_M0_CLIP_FLAG		(char) 0x10		/* Clip Pixels Outside Local Win*/
#define CY_M0_LAR_CHAR		(char) 0x20		/* 2x2 Cell Char (12x16-pixels)*/
#define CY_M0_SLIDE_FLAG	(char) 0x40		/* "Times Square" Scrolling*/
#define CY_M0_SPL_FONT		(char) 0x80		/* Spcl Font: 'A'->1 'B'->2 */

/* Mode Register 1 LCD Mode Register*/
#define	CY_M1_BLINKING		(char) 0x01		/* Enable Blinking Cursor */
#define CY_M1_CUR_ENABLE	(char) 0x02		/* Enable Cursor Display */
#define CY_M1_TEXT			(char) 0x04		/* Enable Text Display Plane */
#define CY_M1_GRAPHICS		(char) 0x08		/* Enable Graphics Display Plane */
#define CY_M1_ROW_AUTO_INC	(char) 0x10		/* Inc Graphics Row After Load */
#define CY_M1_QRY_AUTO_INC	(char) 0x20		/* Query Auto-Increment Flag */
#define CY_M1_SKIP_HOME		(char) 0x40		/* (Normally Off) */
#define CY_M1_FULL_CURSOR	(char) 0x80		/* 1 = 8 Line; 0 = 1 Line */

/* Mode Register 2 Key Mode Flags*/
#define	CY_M2_SOFT_KEYS		(char) 0x01		/* Soft-Keys [1..6] Pins [1..6]*/
#define CY_M2_LOGIC_WAVES	(char) 0x02		/* Pins 1..6 = digital logic [1/0]*/
#define CY_M2_CURSOR_KEYS	(char) 0x04		/* Pins 1 .. 4 up-down-left-right */
#define CY_M2_ASCII_KEYS	(char) 0x08		/* Pins 1 .. 7 ASCII Key Inputs */
#define CY_M2_KEY_MATRIX	(char) 0x10		/* Pins 1 .. 8 4x4 Matrix (16keys)*/

/* Mode Register 3 Key Mode Flags*/
#define	CY_M3_SEND_TO_BUS	(char) 0x01		/* Qualify By Input Flags*/
#define CY_M3_SEND_TO_TXD	(char) 0x02		/* Qualify By Input Flags*/
#define CY_M3_KEY_TO_TXD	(char) 0x04		/* Send Keys to TxD */
#define CY_M3_KEY_TO_BUS	(char) 0x08		/* Send Keys to Bus */
#define CY_M3_ECHO_SERIAL	(char) 0x10		/* Echo RxD-Display to TxD*/

/* Custom down loaded characters */
#define MENU_PTR_CY			(char) 0xA4		/* Menu Pointer Symbol */
#define DBL_UNLINE_CY		(char) 0xB4		/* Double Underline Symbol */
#define UPDN_PTR_CY			(char) 0xC4		/* Up/Down Scroll Icon Symbol */
#define DEGREE_CY			(char) 0xD4		/* Degree Symbol */

/************ INTERNAL REGISTER  MASKS	*************/

/* PDx */
#define	SWITCH_MGR		(char) 0x08		/* Front Panel Switch setting */
#define	SWITCH_ON		(char) 0x10		/* Front Panel Switch setting */

/* PAx */
#define BP_CLOCK		(char) 0x04		/* Back Plane clock running (1) */

