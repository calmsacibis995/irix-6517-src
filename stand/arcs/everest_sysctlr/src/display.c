#include	<exec.h>
#include	<proto.h>
#include	<sys_hw.h>
#include	<sp.h>
#include	<string.h>
#include	<io6811.h>
#include 	<stdio.h>


/****************************************************************************
 *													   						*
 *	fpprint	 	Detects the type of front panel and routes the string to	*
 *				be printed to the appropriate routine					   	*
 *																		   	*
 ***************************************************************************/
non_banked void 
fpprint(msg, init_flg)
char	*msg;
char	init_flg;
{
	if(PORTD & TERMINATOR)
		cy_str(msg);
	else
		hd_print(msg, init_flg);
}

/****************************************************************************
 *																			*
 *	cyreg_rd	Routine to read any of the Cybernetics registers			*
 *																			*
 ***************************************************************************/
non_banked char
cyreg_rd(reg_no)
char	reg_no;
{
	long		waitcount;
	char		reg_buf[10];
	char		*ptr;
	int			reg_val;
	struct RTC	*ptod = (struct RTC *) L_RTC;		/* Pointer to RTC */

char ch;
	ptr = reg_buf;
	reg_val = 0;

	/* send the querry command */
	sprintf(reg_buf, "? %d\r", reg_no);
	cy_str(reg_buf);

	/* now check for the result */

	/* wait for the Bus Write Signal to go low */
	waitcount = 100000;
	while(waitcount-- && (PORTD & CY_BUS_WT));
	if (waitcount < 0) {
		log_event(FP_RD_ERR, NO_FP_DISPLAY);
		return(-1);
	}

	/* now get ready to read multiple characters from the querry command */
	while (!(PORTD & CY_BUS_WT)) {

		/*Drive 325 Write Low */
		ptod->nv_lcd_cntl |= CY_325_WT_L;
		*L_LCD_CNT = ptod->nv_lcd_cntl;

		/* wait for ready line to go low */
		waitcount = 100000;
		while(waitcount-- && (PORTA & CY_325_RDY));

		/* strobe the data into the latch */
		ptod->nv_lcd_cntl &= ~HD_E;					
		*L_LCD_CNT = ptod->nv_lcd_cntl;
		ptod->nv_lcd_cntl |= HD_E;					
		*L_LCD_CNT = ptod->nv_lcd_cntl;

		/* read the data and raise the 325 Write Signal */
		ch = *L_LCD_RDAT;
		*ptr++ = ch;
		ptod->nv_lcd_cntl &= ~CY_325_WT_L;
		*L_LCD_CNT = ptod->nv_lcd_cntl;
	}

	/* pull out the register value from the string */
	sscanf(&reg_buf[1], "%x", &reg_val);

	return((char) reg_val);
}

/****************************************************************************
 *																			*
 *	cy_str 		Send a string of characters to the Cybernetic LCD			*
 *				controller.  The string may be a control string interpeted	*
 *				by the controller or a display string that will be         	*
 *				written to the LCD panel.                              		*
 *																			*
 ***************************************************************************/
non_banked void
cy_str(msg_str)
char	*msg_str;
{
	long			waitcount;
	unsigned char	temp;
	struct RTC		*ptod = (struct RTC *) L_RTC;		/* Pointer to RTC */


	while(*msg_str != 0) {
		waitcount = 100000;
		while(waitcount-- && !(temp = (PORTA & CY_325_RDY)));
		if (waitcount < 0) {
			fpinit_cy();
			log_event(FP_ERR, NO_FP_DISPLAY);
			return;
		}

		/*Place char into latch*/
		*L_LCD_WDAT = *msg_str;

		/*Drive 325 Write Low, but do not change backlite setting */
		ptod->nv_lcd_cntl |= CY_325_WT_L;
		*L_LCD_CNT = ptod->nv_lcd_cntl;

		/*Wait for 325 Ready to go low, or timeout, if the waitcount 
		  expires then an interrupt probably occured and the ready line
		  has gone low and already return high.*/
		waitcount = 1000;
		while(waitcount-- && (temp = (PORTA & CY_325_RDY)));

		/*Raise 325 Write and get ready for the next character */
		ptod->nv_lcd_cntl &= ~CY_325_WT_L;
		*L_LCD_CNT = ptod->nv_lcd_cntl;
		msg_str++;
	}
}

/****************************************************************************
 *													   						*
 *	hd_print 	Primitive print routine for the Hitachi front LCD panel		*
 *				display.  The Hitachi display is limited to 2 lines of		*
 *				20 characters.  A '\n' will place the remainder of the 		*
 *				message on the second line.  Characters will be placed 		*
 *				on the second line with the 21st character or a '\n'   		*
 *				before the 21st character.									*
 *																			*
 ***************************************************************************/
non_banked void
hd_print(msg_str, init_flg)
char	*msg_str;
char	init_flg;
{
	int count,
		char_count;

	count = 0;
	char_count = 0;
	if (init_flg) {
		hd_cmd(HD_C_INC_NSFT);			/*Increment Cursor No shift*/
		hd_cmd(HD_CLEAR);				/*Clear Dsp/Set Cursor to address 0*/
	}
	hd_cmd(HD_DSP_ON);					/*Turn the display on */
	while(msg_str[count] != 0) {
		if(msg_str[count] == '\n' || msg_str[count] == '\r' || char_count == 20) { 
			hd_cmd(HD_DD_ADD | 0x40);		/*Set Cursor to start of 2nd line */
			char_count = 0;
			/* print the 20th char on the next line if not a \n */
			if(msg_str[count] != '\n' && msg_str[count] != '\r') {
				hd_char(msg_str[count]);		/*print the character */
				char_count++;
			}
		}
		else {
			hd_char(msg_str[count]);		/*print the character */
			char_count++;
		}
		count++;
		if(count > char_count && char_count == 20)
			break;
	}

}

/****************************************************************************
 *													   						*
 *	hd_graph 	Primitive print routine for the Hitachi front LCD panel		*
 *				display, for use in printing special characters. One of the *
 *				decodes for a special character is 0, which is the reason	*
 *				hd_print will not work.										*
 *																			*
 *				A '\n' will place the remainder of the 						*
 *				message on the second line.  Characters will be placed 		*
 *				on the second line with the 21st character or a '\n'   		*
 *				before the 21st character.									*
 *																			*
 ***************************************************************************/
non_banked void
hd_graph(msg_str, count)
char	*msg_str;
char	count;
{
	int  char_count;
	int	i;

	char_count = 0;
	hd_cmd(HD_DSP_ON);					/*Turn the display on */
	hd_cmd(HD_C_INC_NSFT);			/*Increment Cursor No shift*/
	hd_cmd(HD_HOME);				/*Clear Dsp/Set Cursor to address 0*/

	for (i = 0; i< count; i++) {
		if(msg_str[i] == '\n' || char_count == 20) { 
			hd_cmd(HD_DD_ADD | 0x40);		/*Set Cursor to start of 2nd line */
			char_count = 0;
			/* print the 20th char on the next line if not a \n */
			if(msg_str[i] != '\n')
				hd_char(msg_str[i]);		/*print the character */

		}
		else
			hd_char(msg_str[i]);		/*print the character */
		char_count++;
	}
}

/****************************************************************************
 *																			*
 *	hd_cmd	 	Send Display command to the Hitachi LCD controller         	*
 *																	   		*
 ***************************************************************************/
non_banked void
hd_cmd(cmd)
char	cmd;
{
	char temp = 0;
	struct RTC	*ptod = (struct RTC *) L_RTC;		/* Pointer to RTC */

	*L_LCD_CNT	= ptod->nv_lcd_cntl;	/*Select Write to Control Register*/
	*L_LCD_WDAT = cmd;				/*Command passed in by calling function*/
	ptod->nv_lcd_cntl |= HD_E;		
	*L_LCD_CNT = ptod->nv_lcd_cntl;	/*Assert Enable */
	temp++;							/*Delay*/
	ptod->nv_lcd_cntl &= ~HD_E;		
	*L_LCD_CNT = ptod->nv_lcd_cntl;	/*De-Assert Enable */
	wait_bf();						/*wait while busy flag is one */
}

/****************************************************************************
 *																			*
 *	hd_char	 	Send Display Character to the Hitachi LCD controller        *
 *																			*
 *																			*
 				4/14/93 -	An error where RS and E clock were changing 	*
 *							states at the same time, when there is a hold	*
 *							time requirement that E clock changes states	*
 *							10-20 nsec before the RS changes states.		*
 *																			*
 ***************************************************************************/
non_banked void
hd_char(lcd_char)
char	lcd_char;
{
	char	temp;
	struct RTC	*ptod = (struct RTC *) L_RTC;		/* Pointer to RTC */

	ptod->nv_lcd_cntl	|= HD_DAT_REG;			/*Select Write to Control Register*/
	*L_LCD_CNT	= ptod->nv_lcd_cntl;					
	*L_LCD_WDAT = lcd_char;					/*Char passed by calling function*/
	ptod->nv_lcd_cntl |=  HD_E;				/*Assert Enable */
	*L_LCD_CNT = ptod->nv_lcd_cntl;			/*Assert Enable */
	temp++;									/*Delay*/
	ptod->nv_lcd_cntl &=  ~HD_E;		
	*L_LCD_CNT = ptod->nv_lcd_cntl;			/*De-Assert Enable */
	ptod->nv_lcd_cntl &=  ~HD_DAT_REG;		
	*L_LCD_CNT = ptod->nv_lcd_cntl;			/*De-Assert Register Select */
	wait_bf();								/*wait while busy flag is one */
}

/****************************************************************************
 *																			*
 *	fpinit	Detects the type of front panel and then initalizes				*
 *		 	calls the appropriate routine									*
 *									   										*
 ***************************************************************************/

non_banked void 
fpinit()
{
	if(PORTD & TERMINATOR)
		fpinit_cy(); 					/*CY325 Display */
	else 
		fpinit_hd(); 					/*Hitachi Display */
}

/****************************************************************************
 *																			*
 *	fpinit_cy	Initialization routine for Cybernetic Front Panel Display	*
 *				controller.		   											*
 *																			*
 ***************************************************************************/

non_banked void 
fpinit_cy()
{
	char buff[2];
	struct RTC	*ptod = (struct RTC *) L_RTC;		/* Pointer to RTC */

	/* Toggle Reset line, set bus control high and turn on backlight*/
	ptod->nv_lcd_cntl |= (HD_DIS_RST | HD_READ | HD_DAT_REG) ;
	*L_LCD_CNT	= ptod->nv_lcd_cntl;
	delay(SW_20MS);
	ptod->nv_lcd_cntl &= ~HD_DIS_RST;
	*L_LCD_CNT	= ptod->nv_lcd_cntl;

	delay(SW_1SEC);

	buff[1] = 0;
	/* Place Controller in the command mode */
	buff[0] =  CY_CMD_MODE;
	cy_str(buff);

	/* Fast Bus Mode */
	cy_str("I 9\r");

	/* Erase Screen */
	cy_str("I 3\r");

	/* Disable Key Scanning */
	cy_str("/I 5\r");

	/* Select 6x8 characters */
	cy_str("I 6\r");

	/* Enable Vertical Scrolling */
	cy_str("M 0,7\r");

	/* Load special pointer "->" character */
	cy_str("F 0,0,8,12,62,63,62,12,8\r");
	cy_str("F 16,0,0,63,63,0,0,0,0\r");
	cy_str("F 32,4,14,31,0,0,31,14,4\r");
	cy_str("F 48,15,9,9,15,0,0,0,0\r");

	/* Place Controller in the Display mode */
	buff[0] =  CY_DSP_MODE;
	cy_str(buff);
}

/****************************************************************************
 *																			*
 *	fpclear_cy	Clears the cybernetics display panel.  Because the command	*
 *				also clears the down loaded font characters, these 			*
 *				characters must be reloaded.								*
 *																			*
 ***************************************************************************/

non_banked void 
fpclear_cy()
{
	char buff[2];

	buff[1] = 0;
	/* Place Controller in the command mode */
	buff[0] =  CY_CMD_MODE;
	cy_str(buff);

	/* Erase Screen */
	cy_str("I 3\r");

	/* Fast Bus Mode */
	cy_str("I 9\r");

	/* Disable Key Scanning */
	cy_str("/I 5\r");

	/* Turn off the cursor*/
	cy_str("M 1,3Ch\r"); 
	/* Turn on the Auto cursor increment */
	cy_str("M 0,6\r");

	/* Load special pointer "->" character */
	cy_str("F 0,0,8,12,62,63,62,12,8\r");
	cy_str("F 16,0,0,63,63,0,0,0,0\r");
	cy_str("F 32,4,14,31,0,0,31,14,4\r");
	cy_str("F 48,15,9,9,15,0,0,0,0\r");

	/* Place Controller in the Display mode */
	buff[0] =  CY_DSP_MODE;
	cy_str(buff);
}

/****************************************************************************
 *																			*
 *	fpinit_hd	Initialization routine for Hitachi Front Panel Display		*
 *					controller.			   									*
 *																			*
 ***************************************************************************/

non_banked void 
fpinit_hd()
{
	char	cg_char,
			row,
			temp;
	struct RTC	*ptod = (struct RTC *) L_RTC;		/* Pointer to RTC */

	char 	bars[8][8] = { 
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F, 0x1F,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x1F, 0x1F, 0x1F,
		0x00, 0x00, 0x00, 0x00, 0x1F, 0x1F, 0x1F, 0x1F,
		0x00, 0x00, 0x00, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F,
		0x00, 0x00, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F,
		0x00, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F,
		0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F
	};

	temp = 0;

	/*initialize by instruction, See pg 125 of Data Manual */
	*L_LCD_CNT	= ptod->nv_lcd_cntl;		/*Select Write to Control Register*/
	*L_LCD_WDAT = HD_FNC_SET;			/*Select 8bits */
	ptod->nv_lcd_cntl |= HD_E;					
	*L_LCD_CNT = ptod->nv_lcd_cntl;		/*Assert Enable */
	temp++;								/*Delay*/
	ptod->nv_lcd_cntl &= ~HD_E;					
	*L_LCD_CNT = ptod->nv_lcd_cntl;		/*De-Assert Enable */

	delay(SW_5MS);						/* Delay > 4.1 ms */

	*L_LCD_WDAT = HD_FNC_SET;			/*Select 8bits */
	ptod->nv_lcd_cntl |= HD_E;					
	*L_LCD_CNT = ptod->nv_lcd_cntl;		/*Assert Enable */
	temp++;								/*Delay*/
	ptod->nv_lcd_cntl &= ~HD_E;					
	*L_LCD_CNT = ptod->nv_lcd_cntl;		/*De-Assert Enable */

	delay(SW_100US);					/* Delay > 100us */

	*L_LCD_WDAT = HD_FNC_SET;			/*Select 8bits */
	ptod->nv_lcd_cntl |= HD_E;					
	*L_LCD_CNT = ptod->nv_lcd_cntl;		/*Assert Enable */
	temp++;								/*Delay*/
	ptod->nv_lcd_cntl &= ~HD_E;					
	*L_LCD_CNT = ptod->nv_lcd_cntl;		/*De-Assert Enable */

	wait_bf();							/*wait while busy flag is one */

	*L_LCD_CNT	= ptod->nv_lcd_cntl;		/*Select Write to Control Register*/
	*L_LCD_WDAT = HD_FNC_SET;			/*Select 8bits */
	ptod->nv_lcd_cntl |= HD_E;					
	*L_LCD_CNT = ptod->nv_lcd_cntl;		/*Assert Enable */
	temp++;								/*Delay*/
	ptod->nv_lcd_cntl &= ~HD_E;					
	*L_LCD_CNT = ptod->nv_lcd_cntl;		/*De-Assert Enable */

	wait_bf();							/*wait while busy flag is one */

	*L_LCD_CNT	= ptod->nv_lcd_cntl;		/*Select Write to Control Register*/
	*L_LCD_WDAT = HD_DSP_OFF;			/*Turn off the display */
	ptod->nv_lcd_cntl |= HD_E;					
	*L_LCD_CNT = ptod->nv_lcd_cntl;		/*Assert Enable */
	temp++;								/*Delay*/
	ptod->nv_lcd_cntl &= ~HD_E;					
	*L_LCD_CNT = ptod->nv_lcd_cntl;		/*De-Assert Enable */

	wait_bf();

	*L_LCD_CNT	= ptod->nv_lcd_cntl;		/*Select Write to Control Register*/
	*L_LCD_WDAT = HD_DSP_ON;			/*Turn Display OFF */
	ptod->nv_lcd_cntl |= HD_E;					
	*L_LCD_CNT = ptod->nv_lcd_cntl;		/*Assert Enable */
	temp++;								/*Delay*/
	ptod->nv_lcd_cntl &= ~HD_E;					
	*L_LCD_CNT = ptod->nv_lcd_cntl;		/*De-Assert Enable */

	wait_bf();

	*L_LCD_CNT	= ptod->nv_lcd_cntl;		/*Select Write to Control Register*/
	*L_LCD_WDAT = HD_C_INC_NSFT;		/*Increment Cursor/No display Shift*/
	ptod->nv_lcd_cntl |= HD_E;					
	*L_LCD_CNT = ptod->nv_lcd_cntl;		/*Assert Enable */
	temp++;								/*Delay*/
	ptod->nv_lcd_cntl &= ~HD_E;					
	*L_LCD_CNT = ptod->nv_lcd_cntl;		/*De-Assert Enable */

	hd_cmd(HD_DSP_OFF);					/*Turn Display Off*/
	hd_cmd(HD_CLEAR);					/*Clear Dsp/Set Cursor to address 0*/
	/* Initializtion Complete */

	/* now load the Bar Graph custom characters */
	hd_cmd(HD_CG_ADD | 0);  /* Set the CG Address at 0 */
	for (cg_char = 0; cg_char <= 0x7; cg_char++) {
		for (row = 0; row <= 0x7; row++)
			hd_char(bars[cg_char][row]);
	}
}

/****************************************************************************
 *																			*
 *	wait_bf:	Wait for Busy Flag, reads the Hitachi LCD controller's		*
 *				busy flag until the flag is cleared.						*
 *																			*
 ****************************************************************************/
non_banked void
wait_bf()
{
	int				loop;
	unsigned char	flag;
	struct RTC		*ptod = (struct RTC *) L_RTC;		/* Pointer to RTC */

	loop = 1000;
	while(loop--) {
		ptod->nv_lcd_cntl |= HD_READ;					
		*L_LCD_CNT = ptod->nv_lcd_cntl;			/*Select Read from Cntl. Reg*/

		ptod->nv_lcd_cntl |= HD_E;					
		*L_LCD_CNT = ptod->nv_lcd_cntl;				/*Assert Enable */

		ptod->nv_lcd_cntl &= ~HD_E;					
		*L_LCD_CNT = ptod->nv_lcd_cntl;				/*Strobe data into Latch */

		flag = *L_LCD_RDAT;

		ptod->nv_lcd_cntl &= ~HD_READ;				
		*L_LCD_CNT = ptod->nv_lcd_cntl;				/*De-Assert Enable */
		if(!(flag & 0x80))
			break;
	}
	if(loop <= 0)
		log_event(FP_ERR, NO_FP_DISPLAY);
}


/****************************************************************************
 *																			*
 *	status_fmt:	Formats the graphic LCD panel for the status display.		*
 *																			*
 ****************************************************************************/
non_banked void
status_fmt(title, new_screen, scroll_flag, screen_timeout)
char *title;						/*pointer to the title string */
char new_screen;
char scroll_flag;
unsigned short screen_timeout;
{
	extern char active_screen;
	extern char last_screen;
	extern const char *cy_windows[8];
	extern const char	cmd_mode[2];
	extern const char	dsp_mode[2];
	extern const char	cy_line[41];

	/* Allow the status messages and the Error message the sharing of 
	 * the same screen.
	 */
	if ((active_screen == ACT_ERROR || active_screen == ACT_STATUS) &&
		(new_screen == ACT_ERROR || new_screen == ACT_STATUS))
		active_screen = new_screen;

	if((PORTD & TERMINATOR) && (active_screen != new_screen)) {
		screen_sw(new_screen, screen_timeout);
		/* Initialize the Error Display Screen*/

		fpclear_cy();
		cy_str(cmd_mode);
		cy_str((char *)cy_windows[ERROR_TITLE]);
		cy_str(dsp_mode); 		/* Display Mode */
		if(*title == 0)
			cy_str("\r            SYSTEM STATUS\r");
		else
			cy_str(title);
		/* draw a line across the window */
		cy_str(cy_line);

		cy_str(cmd_mode);
		cy_str((char *)cy_windows[ERROR_TABLE]);

		/* Enable Vertical Scrolling */
		if (scroll_flag)
			cy_str("M 0,7\r");

		cy_str(dsp_mode); 		/* Display Mode */
	}
	else
		screen_sw(new_screen, screen_timeout);
}


/************************************************************************
 *																		*
 *	Screen_sw:		changes the active_screen varible for the new		*
 *					function to be displayed.  I also sets the time-	*
 *					value for the default screen if the parameter is	*
 *					greater than zero.									*
 *																		*
 ************************************************************************/
non_banked void
screen_sw(new_screen, timeout_value)
char 			new_screen;
unsigned short	timeout_value;
{
	extern char active_screen;
	extern char last_screen;

	if (new_screen) {
		last_screen = active_screen;
		active_screen = new_screen;
	}

	cancel_timeout(DISPLAY, DEFAULT_SCREEN);
	if(timeout_value)
		timeout(DISPLAY, DEFAULT_SCREEN, timeout_value);

}
