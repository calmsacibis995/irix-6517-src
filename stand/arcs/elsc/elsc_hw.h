/* ELSC_HW.H */

/* This include file maps the various input and output ports to the 
 * ELSC 17C44 based design
 */

/* PORTA  Bit Settings */

#ifdef	SPEEDO
#define	EXT_INT		0
#define	TMP_SYS_OT	1			/* Over Temperature Error Signal, Active Low */
#define	POWERON_L	2			/* Power Switch */
#else
#define	EXT_INT		0
#define	TMP_SYS_OT	1			/* Over Temperature Error Signal, Active Low */
#ifndef 	_005
#define	KEY_SW_OFF	2			/* Kew Switch in the Off Position */
#endif
#endif

#ifndef	_005
/* PORT A Mask Values */
#define	KEY_SW_OFF_MASK	0x04		/* Kew Switch in the Off Position */
#endif


#ifdef	SPEEDO
/* PORTB	Bit Settings */
#define	NIC_MP				0			/* NIC */
#define	TMP_FANS_HI			5			/* High Temperature Range */
#define	RESET_SW				6			/* Reset Push Button Sense */
#define	NMI_SW				7			/* NMI Push Button Sense */

/* PORT B MASK Values */
#define	TMP_FANS_HI_MASK	0x20		/* mask for fan high setting */
#define	RST_MASK				0x40		/* Reset input port Maks */
#define	NMI_MASK				0x80		/* NMI input port Mask */
#define	PORTB_MASK			0xE0		/* Mask out all output bits */
/* Master / Slave bit Mask */
#ifdef	DIP_SW_10
#define	MS_MASK	0x04
#else
#define	MS_MASK	0x80
#endif	//DIP_SW_10


#else			//SPEEDO

/* PORTB	Bit Settings */
#ifdef _005
#define	POKB			2			/* Power OK Failure signal */
#define	NIC_MP		3			/* NIC */
#else
#define	PS_OK			2			/* Power Supply good status */
#define	PWR_FAIL		3			/* Power Fail Warning Flag */
#endif
#define	TMP_FANS_HI	4			/* High Temperature Range */
#ifdef	_005
#define	KEY_SW_OFF	5			/* Kew Switch in the Off Position */
#endif
#define	RESET_SW		6			/* Reset Push Button Sense */
#define	NMI_SW		7			/* NMI Push Button Sense */

/* PORT B MASK Values */
#ifndef	_005
#define	PS_OK_MASK			0x04		/* Power Supply good status */
#define	PWR_FAIL_MASK		0x08		/* Power Fail Warning Flag */
#define	PORTB_MASK			0xDC		/* Mask out all output bits */
#endif
#define	TMP_FANS_HI_MASK	0x10		/* mask for fan high setting */
#ifdef 	_005
#define	KEY_SW_OFF_MASK	0x20		/* Kew Switch in the Off Position */
#define	PORTB_MASK			0xF4		/* Mask out all output bits */
#endif
#define	RST_MASK				0x40		/* Reset input port Maks */
#define	NMI_MASK				0x80		/* NMI input port Mask */

#endif  //SPEEDO

/* 1 x 8 Dislay Device */
#define	DSP_ADD		(long) 0x4000
#define	CH_0			0x08
#define	CH_8			0x0F
#define	CNT_ADD		0x00

#define	BRT_FULL				0
#define	DSP_CLEAR			0x80
#define	DSP_DISABLE_CLEAR	0
#define	DSP_BLINK			0x10
#define	DSP_TEST				0x40

/* External Input port 1 */
//#pragma	portr DIP_SW	@	0x6000;	/* Address of DIP Switch Input Port */
#define	DIP_SW		(long ) 0x6000
#define	SW1			0			/* Switch Position 1 */
#define	SW2			1			/* Switch Position 2 */
#define	SW3			2			/* Switch Position 3 */
#define	SW4			3			/* Switch Position 4 */
#define	SW5			4			/* Switch Position 5 */
#define	SW6			5			/* Switch Position 6 */
#define	SW7			6			/* Switch Position 7 */
#define	SW8			7			/* Switch Position 8 */

#ifdef	SPEEDO

/* External Input Port 2 */
//#pragma	portr	 INPT2	@	0x8000;	/* Address of Second Input Port */
#define	INPT2			(long ) 0x8000
#define	PS_OK			0			/* Power Supply good status */
#define	PWR_SW_L		1			/* Power Push Button Switch */
#define	CTS			4			/* RS-232 Clear-to-Send Input Signal */
#define	DCD			5			/* RS-232 Data-Carrier-Detect Input Signal */
#define	MP_TYPE0		6			/* Mid-plane configuration bit */
#define	MP_TYPE1		7			/* Mid-plane configuration bit */

/* External Input Port 2 masks */
#define	PWR_SW_MASK 		0x02
#define	PS_OK_MASK			0x01		/* Power Supply good status */

#else		//SPEEDO

/* External Input Port 2 */
//#pragma	portr	 INPT2	@	0x8000;	/* Address of Second Input Port */
#define	INPT2			(long ) 0x8000
#ifdef 	_005
#define	PS_OK			0			/* Power Supply good status */
#else
#define	POKB			0			/* Power OK Failure signal */
#endif
#define	DIAG_SW_ON	1
#ifdef 	_005
#define	PWR_FAIL		2			/* Power Fail Warning Flag */
#endif
#define	PS_OT2		3			/* Power Supply Over Temperature Signal */
#define	CTS			4			/* RS-232 Clear-to-Send Input Signal */
#define	DCD			5			/* RS-232 Data-Carrier-Detect Input Signal */
#define	MP_TYPE0		6			/* Mid-plane configuration bit */
#define	MP_TYPE1		7			/* Mid-plane configuration bit */

/* External Input Port 2 masks */
#define	DIAG_SW_MASK 0x02
#ifdef	_005
#define	PS_OK_MASK			0x01		/* Power Supply good status */
#define	PWR_FAIL_MASK		0x04		/* Power Fail Warning Flag */
#endif

#endif	//SPEEDO

#ifdef	SPEEDO
/* External Output Port 1 */
#define	OUTP1			(long ) 0xA000		/* Address of First Output Port */
#define	PANIC_INTR		0			/* Panic Interrupt Signal */
#define	NMI_OUT			1			/* NMI Output Signal */
#define	SYS_RESET		2			/* System Reset Output Signal */
#define	FAN_HI_SPEED	3			/* Output fan speed control */
#define	LED_GREEN		4			/* Tri-color LED control */
#define	LED_RED			5			/* Tri-color LED control */
#define	RTS				6			/* RS-232 Request to Send Input */
#define	DTR				7			/* RS-232 Data Terminal Ready Input */

#else		//SPEEDO
/* External Output Port 1 */
#define	OUTP1			(long ) 0xA000	/* Address of First Output Port */
#define	PANIC_INTR			0	/* Panic Interrupt Signal */
#define	KCAR_HI_FAN_SPEED	0	/* Fan Speed control for ELSC in a KCAR */
#define	NMI_OUT				1	/* NMI Output Signal */
#define	SYS_RESET			2	/* System Reset Output Signal */
#define	SYS_RESET_2			3	/* System Reset Output Signal */
#define	PLL_RESET			4	/* Phase Lock Loop Reset Output Signal */
#define	PLL_RESET_2			5	/* Phase Lock Loop Reset Output Signal */
#define	DC_OK					6	/* DC OK Reset Signal */

#endif	//SPEEDO


#ifndef	SPEEDO

/* External Output Port 2 */
#define	OUTP2			(long ) 0xC000		/* Address of Second Output Port */
#define	V345_MARGINH		0			/* 3.45 Volt margin High Signal */
#define	V345_MARGINL		1			/* 3.45 Volt margin High Signal */
#define	V5_MARGINH			2			/* 3.45 Volt margin High Signal */
#define	V5_MARGINL			3			/* 3.45 Volt margin High Signal */
#define	RMT_MARGINH			4			/* 3.45 Volt margin High Signal */
#define	RMT_MARGINL			5			/* 3.45 Volt margin High Signal */
#define	PENB					6			/* Board Regulator Enable Signal */
#define	PS_SYS_OT			7			/* Power Supply control latch */

/* External Output Port 3 */
#define	OUTP3				(long) 0xE000	/* Address of third Output Port */
#define	FAN_ADD0			0			/* Fan pulse multipler address bit 0 */
#define	FAN_ADD1			1			/* Fan pulse multipler address bit 1 */
#define	FAN_ADD2			2			/* Fan pulse multipler address bit 2 */
#define	FAN_HI_SPEED	3			/* Fan High Speed Output signal */
#define	SYS_OT_LED		4			/* System Over Temperature LED Output Signal */
#define	PS_EN				5			/* Power Supply Enable */
#define	RTS				6			/* RS-232 Request-to-Send Output Signal */
#define	DTR				7			/* RS-232 Data-Terminal-Ready Output Signal */


#endif	//SPEEDO

#ifndef	SPEEDO
/* Mid-Plane I/O Expander Addresses */
#define	MP_U1			0x40
#define	MP_U2			0x42
#define	MP_U3			0x44
#define	I_FM_ADD		0x46
#define	N_FM_ADD		0x48

#define	MP_40			0x40
#define 	MP_42			0x42
#define	MP_44			0x44
#define	MP_46			0x46
#define	MP_48			0x48

#endif

#ifndef	SPEEDO
/* Mid-plane Clock select circuitry */
/* NVRAM variable MP_CLK_1, i2c address 46 */
#define	SEL_NET_CLK_NORM		0
#define	SEL_NET_CLK_HI			1
#define	SEL_NET_CLK_LO			2
#define	SEL_IO_CLK_NORM		3
#define	SEL_IO_CLK_HI			4
#define	SEL_IO_CLK_LO			5

/* NVRAM variable MP_CLK_2, i2c address 48 */
#define	SEL_TEST_CLK_NORM		0
#define	SEL_TEST_CLK_HI		1
#define	SEL_TEST_CLK_LO		2
#define	TEST_CLK_SEL_N0_L		3
#define	TEST_CLK_SEL_N1_L		4
#define	TEST_CLK_SEL_N2_L		5
#define	TEST_CLK_SEL_N3_L		6
#define	CORE_CLK_SEL			7
#endif

#define	NET_IO_DEFAULT			0x09
#define	KCAR_CLK_DEFAULT		0xF9
#define	TEST_CORE_DEFAULT		0x78

