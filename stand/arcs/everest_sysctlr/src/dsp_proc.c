#include	<exec.h>
#include 	<proto.h>
#include 	<sp.h>
#include 	<sys_hw.h>
#include 	<io6811.h>
#include 	<string.h>
#include 	<stdio.h>

char			fmt_str[260];
int				num_proc;		/*The number of proc for hist. command */
int				num_proc_r;		/*The number of proc from last display */
char			cpu_disp;		/*The number of cpu displayed for bar graph */
char			cpu_hi;			/*The most significant CPU displayed */
char			cpu_lo;			/*The least significant CPU displayed */
char			cpu_group;		/*The CPU group displayed 32:64 or 0:31 */
char			active_perf;	/*Performance Data Ready To Be Displayed */
char			block_btmsg;	/*Flag used to block boot status messages */
int				db_sw_enable;	/*Activate debug setting menu entry */
const char		cmd_mode[2] = {CY_CMD_MODE, 0};
const char		dsp_mode[2] = {CY_DSP_MODE, 0};

/* draws a double line across the Cybernetic display */
const char	cy_line[] = {
	0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4,
	0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4,
	0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4,
	0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4,
	0
};

struct MENU_LINE	menu_line;
struct MENU_ST		menu_dsp[NO_MENU_LINES] =  {
	MN_ALL, mn_ver,			"FIRMWARE VERSION", THIRTY_SEC, ACT_FWVER,0,
	MN_ALL, mn_cpu_sel,		"MASTER CPU SELECTION", THIRTY_SEC, ACT_CPUSEL,0,
	MN_ALL, mn_boot,		"BOOT STATUS", THIRTY_SEC, ACT_BOOTMSG,0,
	MN_ALL, mn_perf,		"CPU ACTIVITY", THIRTY_SEC, ACT_PERF,0,
	MN_ALL, mn_event,		"EVENT HISTORY LOG", THIRTY_SEC, ACT_LOG,0,
	MN_MAN, mn_temp,		"TEMPERATURE STATUS", THIRTY_SEC, ACT_TEMP,0,
	MN_MAN, mn_volts, 		"VOLTAGE STATUS", THIRTY_SEC, ACT_VOLTS,0,
	MN_MAN, mn_db_switch,	"DEBUG SETTINGS", THIRTY_SEC, ACT_SWITCH,0,
	MN_MAN, mn_bpnmi, 		"NMI (FORCE PANIC)", THIRTY_SEC, ACT_MENU,0,
	MN_MAN,	mn_reset, 		"RESET SYSTEM (SCLR)", THIRTY_SEC, ACT_MENU,0
};

struct PROC_MSG_ST	proc_boot_win;

/* Display Window Definitions for the Cybernetics controller */
const char *cy_windows[] = {	
						"/W 0,0,40,3\r",		/* Title window */
						"W 10,3,30,14\r",		/* Function Menu */
						"W 8,3,9,14\r",			/* Function Menu Pointer */
						"/W 0,0,40,3\r",		/* Error logging Title */
						"/W 0,4,40,15\r",		/* Error/Events Table */
						"/W 0,4,40,12\r",		/* Boot message Table */
						"/W 0,13,40,13\r",		/* Processor Status Title */
						"/W 0,14,40,15\r"		/* Processor Status Table */
};

char menu_title[] = {"   FUNCTION MENU\n"};

/* Virtual Debug switch setting globals */
char	bit[16];			/* debug switch setting bit positions */
char	cur_add;			/* Display address of current cursor position */
char	bit_pos;			/* "dip" switch bit number pointed to by cursor */


signed char histogram[64];	/* global memory location CPU activity data */
char btmsg_buff[41];
char pstat_buff[41];
char boot_prog[42];
char active_screen;
char last_screen;
char event_num;					/* last event number displayed */
char active_switch;

non_banked void 
display()
{
	unsigned short		msg;
	struct MENU_LINE	*ml_ptr;
	struct MENU_ST		*dsp_ptr;
	int i;
	extern unsigned short	stuck_menu;				/*stuck button counter */
	extern unsigned short	stuck_scrlup;			/*stuck button counter */
	extern unsigned short	stuck_scrldn;			/*stuck button counter */
	extern unsigned short	stuck_execute;			/*stuck button counter */
	extern char				stuck_button;			/*stuck button flag */

	ml_ptr = &menu_line;

	btmsg_buff[0] = pstat_buff[0] = 0;  /*Null out the start of 1 and 2 line */

	if(PORTD & TERMINATOR) {
		ml_ptr->cur_ptr 	= 0;
		ml_ptr->first_line 	= 0;
		ml_ptr->last_line 	= 11;
		ml_ptr->no_lines 	= 12;
		ml_ptr->window_ptr 	= 0;

		/* count the number of lines for "ON" level */
		for (i = 0, ml_ptr->no_on_func = 0; i < NO_MENU_LINES; i++) {
			dsp_ptr = &menu_dsp[i];
			if (dsp_ptr->line_stat == MN_ALL)
				ml_ptr->no_on_func++;
		}
	}
	else {
		ml_ptr->cur_ptr 	= 0;
		ml_ptr->window_ptr 	= 0;
		ml_ptr->first_line 	= 0;
		ml_ptr->last_line 	= 0;
		ml_ptr->no_lines 	= 1;
	}

	last_screen = 0;
	num_proc_r = 0;
	num_proc = 0;
	ml_ptr->cur_ptr = 0;				/* pointer to display menu */
	block_btmsg = 0;


	for (;;) {
		msg = receive(DISPLAY); 
		switch(msg) {
			case	MENU:
				timeout(DISPLAY, MENU_DEBOUNCE, MSEC_50); 
				dsp_ptr = &menu_dsp[ml_ptr->cur_ptr];
				screen_sw(ACT_MENU, ONE_MIN);
				block_btmsg = 1;
				cpu_group = 0;
				dsp_menu(D_INIT);
				break;
			case	SCROLL_UP:
				timeout(DISPLAY, SCRLUP_DEBOUNCE, MSEC_50); 
				dsp_ptr = &menu_dsp[ml_ptr->cur_ptr];
				screen_sw(NO_SW, SCREEN_TO);
				if (active_screen == ACT_MENU) 
					dsp_menu(D_S_UP);
				else if ((active_screen == ACT_LOG) && !(PORTD & TERMINATOR)) 
					dsp_log(D_S_UP);
				else if (active_screen == ACT_SWITCH) 
					dsp_db_switch(D_S_UP);
				if (active_screen == ACT_PERF) 
					cpu_group = cpu_group & 1 ? 0x10 : 0x11;
				break;
			case	SCROLL_DN:
				timeout(DISPLAY, SCRLDN_DEBOUNCE, MSEC_50); 
				dsp_ptr = &menu_dsp[ml_ptr->cur_ptr];
				screen_sw(NO_SW, SCREEN_TO);
				if (active_screen == ACT_MENU)
					dsp_menu(D_S_DOWN);
				else if ((active_screen == ACT_LOG) && !(PORTD & TERMINATOR)) 
					dsp_log(D_S_DOWN);
				else if (active_screen == ACT_SWITCH)
					dsp_db_switch(D_S_DOWN);
				if (active_screen == ACT_PERF)
					cpu_group = cpu_group & 1 ? 0x10 : 0x11;
				break;
			case	EXECUTE:
				timeout(DISPLAY, EXECUTE_DEBOUNCE, MSEC_50); 
				dsp_ptr = &menu_dsp[ml_ptr->cur_ptr];
				screen_sw(NO_SW, SCREEN_TO);
				if (active_screen == ACT_MENU)
					dsp_menu(D_EXECUTE);
				else if (active_screen == ACT_SWITCH)
					dsp_db_switch(D_EXECUTE);
				else if (active_screen == ACT_SCLR)
					dsp_sclr(D_EXECUTE);
				else if (active_screen == ACT_NMI)
					dsp_nmi(D_EXECUTE);
				break;
			case	BOOT_STAT:			/* Displays status during Boot */
				if (active_screen != ACT_ERROR)
					dsp_boot();
				break;
			case	CPU_PERF:
				dsp_perf();
				break;
			case	DEFAULT_SCREEN:
				if((active_screen != ACT_ERROR) && (active_perf == 1))  
					screen_sw(ACT_PERF, NO_TIMEOUT);
				else
					timeout(DISPLAY, DEFAULT_SCREEN, SCREEN_TO);
				if (active_screen != ACT_ERROR)
					block_btmsg = 0;
				break;

			/* These next case statements activate the front panel
			 * switch for new interrupts after the debounce timeout
			 * period has expired
			 */

			case	EXECUTE_DEBOUNCE:
				active_switch &= ~IRQ_EXECUTE;
				if (!(stuck_menu & stuck_scrldn & stuck_scrlup & stuck_execute))
					stuck_button = 0;
				stuck_execute = 0;
				break;
			case	SCRLDN_DEBOUNCE:
				active_switch &= ~IRQ_SCRLDN;
				if (!(stuck_menu & stuck_scrldn & stuck_scrlup & stuck_execute))
					stuck_button = 0;
				stuck_scrldn = 0;
				break;
			case	SCRLUP_DEBOUNCE:
				active_switch &= ~IRQ_SCRLUP;
				if (!(stuck_menu & stuck_scrldn & stuck_scrlup & stuck_execute))
					stuck_button = 0;
				stuck_scrlup = 0;
				break;
			case	MENU_DEBOUNCE:
				active_switch &= ~IRQ_MENU;
				if (!(stuck_menu & stuck_scrldn & stuck_scrlup & stuck_execute))
					stuck_button = 0;
				stuck_menu = 0;
				break;
			case	DEBUG_SET_TO:     /*disable Debug Switch Menu line */
				if(db_sw_enable == 1) { 
#ifndef LAB
					if (active_screen == ACT_MENU) {
						db_sw_enable = 0;
						send(DISPLAY, MENU);
					}
#endif
				}
				break;
			default:
				log_event(BAD_DSP_MSG, NO_FP_DISPLAY);
				break;
		}
	}
}

non_banked void
dsp_menu(cmd)
char	cmd;				/*menu command */
{
	struct MENU_LINE	*ml_ptr;
	struct MENU_ST		*dsp_ptr;
	char buff[100];
	int i;
	char				lines_enabled;

	ml_ptr = &menu_line;

	if(db_sw_enable > 0) {
		lines_enabled = NO_MENU_LINES;
	}
	else
		lines_enabled = NO_MENU_LINES - 1;

	/* Cybernetics Display */
	if(PORTD & TERMINATOR) {
		/* check if the key switch has been moved since last time */
		if(ml_ptr->last_sw_setting != (PORTD & SWITCH_MGR) ) {
				ml_ptr->last_sw_setting = PORTD & SWITCH_MGR;
				ml_ptr->cur_ptr = 0;
				ml_ptr->window_ptr = 0;
				send(DISPLAY, MENU);
			}

		else if(cmd == D_INIT) {
			if (ml_ptr->last_db_setting != db_sw_enable) {
				ml_ptr->window_ptr = 0;
				ml_ptr->cur_ptr = 0;
			}
			ml_ptr->last_db_setting = db_sw_enable;
			ml_ptr->last_sw_setting = PORTD & SWITCH_MGR;

			/* erase screen */
			fpclear_cy();

			/* define Menu Title Window */
			cy_str(cmd_mode);
			cy_str(cy_windows[FUNC_M_TITLE]);

			/* Display the menu Title */
			cy_str(dsp_mode);
			strcpy(buff, "\r          ");
			strcat(buff, menu_title);
			cy_str(buff);

			/* Define the Menu Window */
			cy_str(cmd_mode);
			cy_str(cy_windows[FUNC_MENU]);

			/* Display the Menu */
			cy_str(dsp_mode);
			for(i = 0; i < NO_MENU_LINES; i++) {
				dsp_ptr = &menu_dsp[i];
				if((dsp_ptr->line_stat == MN_MAN) && (PORTD & SWITCH_MGR)) 
					continue;
				else if((dsp_ptr->d_scrn == ACT_SWITCH) && (db_sw_enable <= 0))
					continue;
				else {
					strcpy(buff, dsp_ptr->line_msg);

					if(strlen(buff) < 20)
							strcat(buff, "\r");
					cy_str(buff);
				}
			}

			/* store the number of lines on the screen for use in scrolling */
			if (PORTD & SWITCH_MGR)	{
				ml_ptr->no_lines = ml_ptr->no_on_func;
#if 0
				ml_ptr->window_ptr = 0;
				/* now find the first "ON" function */
				ml_ptr->cur_ptr = 0;
#endif
				for(;ml_ptr->cur_ptr < NO_MENU_LINES; ml_ptr->cur_ptr++) {
					dsp_ptr = &menu_dsp[ml_ptr->cur_ptr];
					if(dsp_ptr->line_stat == MN_MAN) 
						continue;
					else 
						break;
				}
			}
			else 
				ml_ptr->no_lines = lines_enabled;

			/* define the line pointer window */
			cy_str(cmd_mode);
			cy_str(cy_windows[FUNC_MENU_PTR]);
			cy_str("I 1\r");		/* erase any text or graphics */
			cy_str(dsp_mode);
			for(i = 0; i < ml_ptr->window_ptr; i++)
				buff[i] = ' ';
			buff[i] = 0xA4;
			buff[i+1] = 0;
			cy_str(buff);
		}
		else if(cmd == D_S_DOWN) {
			ml_ptr->cur_ptr++;
			if(ml_ptr->cur_ptr == NO_MENU_LINES)  
				ml_ptr->cur_ptr = 0;
			dsp_ptr = &menu_dsp[ml_ptr->cur_ptr];


			/* if the entry is for system manager and the front panel
			   switch is in the ON position (not system manager) then
			   skip, and go onto the next entry. if the line is for
			   the debug settings and the db_switch is disabled skip
			   this line.
			 */
			if((dsp_ptr->line_stat == MN_MAN) && (PORTD & SWITCH_MGR)) 
				send(DISPLAY, SCROLL_DN);
			else if((dsp_ptr->d_scrn == ACT_SWITCH) && (db_sw_enable <= 0))
				send (DISPLAY, SCROLL_DN);
			else {
				ml_ptr->window_ptr++;
				if(ml_ptr->window_ptr == ml_ptr->no_lines)
					ml_ptr->window_ptr = 0;
				cy_str(cmd_mode);
				cy_str(cy_windows[FUNC_MENU_PTR]);
				cy_str("I 1\r");		/* erase any text or graphics */
				cy_str(dsp_mode);
				for(i = 0; i < ml_ptr->window_ptr; i++)
					buff[i] = ' ';
				buff[i] = 0xA4;
				buff[i+1] = 0;
				cy_str(buff);
			}
		}
		else if(cmd == D_S_UP) {
			ml_ptr->cur_ptr--;
			if(ml_ptr->cur_ptr < 0) 
				ml_ptr->cur_ptr = NO_MENU_LINES - 1;
			dsp_ptr = &menu_dsp[ml_ptr->cur_ptr];

			/* if the entry is for system manager and the front panel
			   switch is in the ON position (not system manager) then
			   skip, and go onto the next entry. if the line is for
			   the debug settings and the db_switch is disabled skip
			   this line.
			 */
			if((dsp_ptr->line_stat == MN_MAN) && (PORTD & SWITCH_MGR)) 
				send(DISPLAY, SCROLL_UP);
			else if((dsp_ptr->d_scrn == ACT_SWITCH) && (db_sw_enable <= 0))
				send (DISPLAY, SCROLL_UP);
			else {
				ml_ptr->window_ptr--;
				if (ml_ptr->window_ptr < 0)
					ml_ptr->window_ptr = ml_ptr->no_lines -1;
				cy_str(cmd_mode);
				cy_str(cy_windows[FUNC_MENU_PTR]);
				cy_str("I 1\r");		/* erase any text or graphics */
				cy_str(dsp_mode);
				for(i = 0; i < ml_ptr->window_ptr; i++)
					buff[i] = ' ';
				buff[i] = 0xA4;
				buff[i+1] = 0;
				cy_str(buff);
			}
		}
	}
	/* Hitachi Display */
	else {
		if(cmd == D_INIT) {
			dsp_ptr = &menu_dsp[ml_ptr->cur_ptr];
			if((dsp_ptr->line_stat == MN_MAN) && (PORTD & SWITCH_MGR)) 
				send(DISPLAY, SCROLL_DN);
			else if((dsp_ptr->d_scrn == ACT_SWITCH) && (db_sw_enable <= 0))
				send (DISPLAY, SCROLL_DN);
			else {
				strcpy(buff, menu_title);
				strcat(buff, dsp_ptr->line_msg);
				fpprint(buff, DSP_INIT);
			}
		}
		else if(cmd == D_S_DOWN) {
			ml_ptr->cur_ptr++;
			if(ml_ptr->cur_ptr == NO_MENU_LINES) 
				ml_ptr->cur_ptr = 0;
			dsp_ptr = &menu_dsp[ml_ptr->cur_ptr];
			/* if the entry is for system manager and the front panel
			   switch is in the ON position (not system manager) then
			   skip, and go onto the next entry. if the line is for
			   the debug settings and the db_switch is disabled skip
			   this line.
			 */
			if((dsp_ptr->line_stat == MN_MAN) && (PORTD & SWITCH_MGR)) 
				send(DISPLAY, SCROLL_DN);
			else if((dsp_ptr->d_scrn == ACT_SWITCH) && (db_sw_enable <= 0))
				send (DISPLAY, SCROLL_DN);
			else {
				strcpy(buff, menu_title);
				strcat(buff, dsp_ptr->line_msg);

				fpprint(buff, DSP_INIT);
			}
		}
		else if(cmd == D_S_UP) {
			ml_ptr->cur_ptr--;
			if(ml_ptr->cur_ptr < 0) 
				ml_ptr->cur_ptr = NO_MENU_LINES - 1;
			dsp_ptr = &menu_dsp[ml_ptr->cur_ptr];
			if((dsp_ptr->line_stat == MN_MAN) && (PORTD & SWITCH_MGR)) 
				send(DISPLAY, SCROLL_UP);
			else if((dsp_ptr->d_scrn == ACT_SWITCH) && (db_sw_enable <= 0))
				send (DISPLAY, SCROLL_UP);
			else {
				strcpy(buff, menu_title);
				strcat(buff, dsp_ptr->line_msg);
				fpprint(buff, DSP_INIT);
			}
		}
	}

	if(cmd == D_EXECUTE) {
		dsp_ptr = &menu_dsp[ml_ptr->cur_ptr];
		screen_sw(ACT_MENU, dsp_ptr->to_value);
		if((dsp_ptr->line_stat == MN_MAN) && (PORTD & SWITCH_MGR)) 
			send(DISPLAY, MENU);
		else 
			(*dsp_ptr->menu_func)();
	}
}

non_banked void
dsp_boot()
{
	char				buff[100];

	extern char 		active_screen;
	extern char 		last_screen;
	extern const char	*cy_windows[8];

	struct PROC_MSG_ST	*win;

	int					i;


	if (!block_btmsg) {
		if (PORTD & TERMINATOR) {
			win = &proc_boot_win;
	
			if(active_screen != ACT_BOOT) {
				win->win_state = 0;
				win->msg_cur_row = 0;
				win->msg_cur_col = 0;
				/* Initialize the Display Screen*/
				fpclear_cy();
				cy_str(cmd_mode);
				cy_str((char *)cy_windows[ERROR_TITLE]);
				cy_str(dsp_mode); 		/* Display Mode */
				strcpy(buff, "\r          BOOT STATUS MESSAGES\r");
				cy_str(buff);
				/* draw a line across the window */
				for(i = 0; i < 40; i++)
					buff[i] = 0xB4;
				buff[i] = 0;
				cy_str(buff);
				cy_str(cmd_mode);
				cy_str((char *)cy_windows[PROC_MSG_TITLE]);
				cy_str("I 1\r");
				cy_str("M 0,2\r");
				cy_str(dsp_mode); 		/* Display Mode */
				cy_str("PROCESSOR STATUS:\r");
			}
	
			/* define the BOOT message screen, if the boot message was not 
				the last item displayed.
			*/
			if(!(win->win_state & MSG_ST) && (win->new_msg & NEW_BT_MSG)) {
				win->win_state = MSG_ST;
				cy_str(cmd_mode);
				cy_str((char *)cy_windows[BOOT_MSG]);
				/* Enable Vertical Scrolling */
				cy_str("M 0,7\r");
	
				/* if this is not a new window restore the cursor. */
				if (win->msg_cur_row != 0 || win->msg_cur_col != 0) {
					sprintf(buff, "M 6 %c\r", win->msg_cur_row);
					cy_str(buff);
					sprintf(buff, "M 7 %c\r", win->msg_cur_col);
					cy_str(buff);
				}
				cy_str(dsp_mode); 		/* Display Mode */
			}
			/* now that the window is defined display the new message */
			strcat(btmsg_buff, "\r");
			if(win->new_msg & NEW_BT_MSG) {
				cy_str(btmsg_buff);
				win->new_msg &= ~NEW_BT_MSG;
			}
	
	
			/* define the Processor status message screen, and 
				display the message
			*/
			if(win->new_msg & NEW_PROC_MSG) {
				cy_str(cmd_mode);
				/* save the cursor location if the last window was for the 
				   boot messages */
				if(!(win->win_state & PROC_ST)) {
					win->msg_cur_row = cyreg_rd(6);
					win->msg_cur_col = cyreg_rd(7);
				}
				win->win_state = PROC_ST;
	
	
				/* define the window and erase the contents of the 
					processor status window */
#if 0
				if (active_screen != ACT_BOOT) {
					cy_str((char *)cy_windows[PROC_MSG_TITLE]);
					cy_str("I 1\r");
					cy_str("M 0,2\r");
					cy_str(dsp_mode); 		/* Display Mode */
					cy_str("PROCESSOR STATUS:\r");
				}
#endif
				cy_str(cmd_mode);
				cy_str((char *)cy_windows[PROC_MSG]);
				cy_str("I 1\r");
				cy_str("M 0,2\r");
				cy_str(dsp_mode); 		/* Display Mode */
				cy_str(pstat_buff);
				win->new_msg &= ~NEW_PROC_MSG;
			}
		}
		else {
			strcpy(buff, btmsg_buff);
			if(pstat_buff[0] != 0) {
				strcat(buff, "\r");
				strcat(buff, pstat_buff);
			}
			fpprint(buff, DSP_INIT);
		}
		screen_sw (ACT_BOOT, SCREEN_TO);
	}
}

non_banked void
dsp_nmi(cmd)
char	cmd;
{
	struct MENU_LINE	*ml_ptr;
	struct MENU_ST		*dsp_ptr;
	char 				buff[41];

	if(cmd == D_INIT) {
		if (PORTD & TERMINATOR) {
			ml_ptr = &menu_line;
			dsp_ptr = &menu_dsp[ml_ptr->cur_ptr];
			strcpy(buff, "\r            ");
			strcat(buff, dsp_ptr->line_msg);
			strcat(buff, "\r");
			status_fmt(buff, ACT_NMI, CY_NO_SCROLL, SCREEN_TO);
			cy_str("\r      NMI: PRESS EXECUTE BUTTON\r\r");
			cy_str("\r     MENU: PRESS MENU BUTTON\r");
		}
		else {
			screen_sw (ACT_NMI, SCREEN_TO);
			fpprint(" NMI: EXECUTE BUTTON\nMENU: MENU BUTTON", DSP_INIT);
		}
	}
	else if (cmd == D_EXECUTE) 
		send(SEQUENCE, MN_NMI);
}

non_banked void
dsp_sclr(cmd)
char	cmd;
{
	struct MENU_LINE	*ml_ptr;
	struct MENU_ST		*dsp_ptr;
	char 				buff[41];

	if(cmd == D_INIT) {
		if (PORTD & TERMINATOR) {
			ml_ptr = &menu_line;
			dsp_ptr = &menu_dsp[ml_ptr->cur_ptr];
			strcpy(buff, "\r           ");
			strcat(buff, dsp_ptr->line_msg);
			strcat(buff, "\r");
			status_fmt(buff, ACT_SCLR, CY_NO_SCROLL, SCREEN_TO);
			cy_str("\r     SCLR: PRESS EXECUTE BUTTON\r\r");
			cy_str("\r     MENU: PRESS MENU BUTTON\r");
		}
		else {
			screen_sw (ACT_SCLR, SCREEN_TO);
			fpprint("SCLR: EXECUTE BUTTON\nMENU: MENU BUTTON", DSP_INIT);
		}
	}
	else if (cmd == D_EXECUTE) 
		send(SEQUENCE, FP_SCLR);
	block_btmsg = 0;
}

 /*																		
 *		4/13/93 -	Added a changed to check for corrupted debug switch 
 *					settings.  If corrupted, then store a "0" in the 	
 *					location, and post an error.						
 */																		
non_banked void
dsp_db_switch(cmd)
char	cmd;
{

	int		i;
	char	buff[100];
	struct RTC		*ptod = (struct RTC *) L_RTC;	/* Pointer to RTC */
	struct MENU_LINE	*ml_ptr;

	ml_ptr = &menu_line;
	screen_sw(ACT_SWITCH, SCREEN_TO);

	if(cmd == D_INIT) {
		/* hide the debug settings for next time and if terminator
			adjust the cursor pointer
		*/

	/* make sure the debug switch settings are not corrupted */
	if ((ptod->nv_irix_sw ^ ptod->nv_irix_sw_not) != 0xFFFF) {
		ptod->nv_irix_sw = 0;
		ptod->nv_irix_sw_not = 0xFFFF;
		log_event(DBSW_ERR, NO_FP_DISPLAY);
	}

#ifndef LAB
		if(db_sw_enable = 1) 
			db_sw_enable = 0;
#endif
		for(i = 0; i < 16; i++) 
			bit[i] = (ptod->nv_irix_sw >> i) & 1;
		if (PORTD & TERMINATOR) {
			/* initialize the screen */
			fpclear_cy();
			cy_str(cmd_mode);
			cy_str("/W 4,3,37,7\r");
			cy_str(dsp_mode);
			cy_str("         DEBUG SETTINGS");
			cy_str("\r\r\r F E D C B A 9 8 7 6 5 4 3 2 1 0");
			cy_str(cmd_mode);
			cy_str("W 4,8,37,10\r");
			cy_str(dsp_mode);

			sprintf(buff, "\r %1x %1x %1x %1x %1x %1x %1x %1x %1x %1x %1x %1x %1x %1x %1x %1x",
				bit[15],bit[14],bit[13],bit[12],bit[11],bit[10],bit[9],bit[8],
				bit[7],bit[6],bit[5],bit[4],bit[3],bit[2],bit[1],bit[0]);
			cy_str(buff);
			cur_add = 35;
			bit_pos = 0;
			cy_str(cmd_mode);
			sprintf(buff, "C 9,%d\r", cur_add);
			cy_str(buff);
			/* Turn on blinking cursor */
			cy_str("M 1,0Fh\r"); 
			/* and turn off the auto increment cursor */
			cy_str("M 0,4\r");
		}
		else {
			bit_pos = 0;
			sprintf(buff, "DEBUG SETTINGS    %1X \n  %1x%1x%1x%1x%1x%1x%1x%1x%1x%1x%1x%1x%1x%1x%1x%1x",bit_pos,
				bit[15],bit[14],bit[13],bit[12],bit[11],bit[10],bit[9],bit[8],
				bit[7],bit[6],bit[5],bit[4],bit[3],bit[2],bit[1],bit[0]);
			cur_add = 0x51;
			fpprint(buff, DSP_INIT);
			hd_cmd(HD_DD_ADD | cur_add);
			hd_cmd(HD_DSP_AON);           /* Turn on display w/ blinking cursor*/
		}
	}
	/* Move Cursor bit position to the right,  If pointed at LSB then move
	   cursor to MSB */
	if(cmd == D_S_DOWN) {
		if (bit_pos != 0) {
			if(PORTD & TERMINATOR)
				cur_add += 2;
			else
				cur_add++;
			bit_pos--;
		}
		else {
			if(PORTD & TERMINATOR)
				cur_add = 5;
			else
				cur_add = 0x42;
			bit_pos = 0xF;
		}
		if(PORTD & TERMINATOR) {
			cy_str(cmd_mode);
			sprintf(buff, "C 9,%d\r", cur_add);
			cy_str(buff);
		}
		else {
			sprintf(buff, "DEBUG SETTINGS    %1X \n  %1x%1x%1x%1x%1x%1x%1x%1x%1x%1x%1x%1x%1x%1x%1x%1x",bit_pos,
				bit[15],bit[14],bit[13],bit[12],bit[11],bit[10],bit[9],bit[8],
				bit[7],bit[6],bit[5],bit[4],bit[3],bit[2],bit[1],bit[0]);
			fpprint(buff, DSP_INIT);
			hd_cmd(HD_DD_ADD | cur_add);
			hd_cmd(HD_DSP_AON);           /* Turn on display w/ blinking cursor*/
		}
	}
	/* Move Cursor bit position to the left,  If pointed at MSB then move
	   cursor to LSB */
	if(cmd == D_S_UP) {
		if (bit_pos != 0xF) {
			if(PORTD & TERMINATOR)
				cur_add -= 2;
			else
				cur_add--;
			bit_pos++;
		}
		else {
			if(PORTD & TERMINATOR)
				cur_add = 35;
			else
				cur_add = 0x51;
			bit_pos = 0;
		}
		if(PORTD & TERMINATOR) {
			cy_str(cmd_mode);
			sprintf(buff, "C 9,%d\r", cur_add);
			cy_str(buff);
		}
		else {
			sprintf(buff, "DEBUG SETTINGS    %1X \n  %1x%1x%1x%1x%1x%1x%1x%1x%1x%1x%1x%1x%1x%1x%1x%1x",bit_pos,
				bit[15],bit[14],bit[13],bit[12],bit[11],bit[10],bit[9],bit[8],
				bit[7],bit[6],bit[5],bit[4],bit[3],bit[2],bit[1],bit[0]);
			fpprint(buff, DSP_INIT);
			hd_cmd(HD_DD_ADD | cur_add);
			hd_cmd(HD_DSP_AON);           /* Turn on display w/ blinking cursor*/
		}
	}
	/* Toggle the bit pointed at by the cursor and store it back to non-
	   volatile RAM */
	if(cmd == D_EXECUTE) {
		if (bit[bit_pos] == 1)
			bit[bit_pos] = 0;
		else if (bit[bit_pos] == 0)
			bit[bit_pos] = 1;
		ptod->nv_irix_sw &= ~( 1 << bit_pos);
		ptod->nv_irix_sw |= (bit[bit_pos] << bit_pos);
		ptod->nv_irix_sw_not = ~ptod->nv_irix_sw;
		if(PORTD & TERMINATOR) {
			/* erase the window contents */
			cy_str(dsp_mode);
			sprintf(buff, "%d", bit[bit_pos]);
			cy_str(buff);
		}
		else {
			sprintf(buff, "DEBUG SETTINGS    %1X \n  %1x%1x%1x%1x%1x%1x%1x%1x%1x%1x%1x%1x%1x%1x%1x%1x",bit_pos,
				bit[15],bit[14],bit[13],bit[12],bit[11],bit[10],bit[9],bit[8],
				bit[7],bit[6],bit[5],bit[4],bit[3],bit[2],bit[1],bit[0]);
			fpprint(buff, DSP_INIT);
			hd_cmd(HD_DD_ADD | cur_add);
			hd_cmd(HD_DSP_AON);          /* Turn on display w/ blinking cursor*/
		}
	}
}

/************************************************************************
 *																		*
 *	DSP_LOG:		Displays the event log on the front panel display.	*
 *					If the display lines are greater then the number	*
 *					of line on the display, then the log may be 		*
 *					up or down.											*
 *																		*
 ************************************************************************/
non_banked void
dsp_log(cmd)
char	cmd;
{
	char	buff[42],
			ev_ctr,
			rval;

	struct RTC		*ptod = (struct RTC *) L_RTC;	/* Pointer to RTC */

	buff[0] = 0;
	/* add an if statement for the different displays, and add more
	   information for the cybernetic display */

	/* clear the fatal error flag in the non-volatile status byte */
	ptod->nv_status &= ~RESET_FERR;

	status_fmt("\r    SYSTEM CONTROLLER LOGGED EVENTS\r", ACT_LOG, CY_NO_SCROLL, SCREEN_TO);

	if (PORTD & TERMINATOR) {

		/* define the event portion of the screen */
		cy_str(cmd_mode);
		cy_str(cy_windows[ERROR_TABLE]);

		/* dsplay the event file to the screen */
		cy_str(dsp_mode);
		if (ptod->nv_event_ctr == 0)
			cy_str("\rNO EVENTS LOGGED!!\r");
		else {
			ev_ctr = ptod->nv_event_ctr;
			for(; ev_ctr > 0; ev_ctr--)
				dsp_event(ev_ctr);
		}

	}
	else {
		if(cmd == D_INIT) {
			/* get the last event stored */
			event_num = ptod->nv_event_ctr;
			rval = log_read(buff, event_num, 1, 0);
			if (rval == 0)
				fpprint("NO EVENTS LOGGED", DSP_INIT);
			else 
				dsp_event(event_num);
		}
		else if(cmd == D_S_DOWN) {
			if (event_num < ptod->nv_event_ctr) {
				event_num++;
				log_read(buff, event_num, 1, 0);
				dsp_event(event_num);
			}
				else {
					fpprint("END OF LOG", DSP_INIT);
					event_num = ptod->nv_event_ctr + 1;
				}
		}
		else if(cmd == D_S_UP) {
			if (event_num > 1) {
				event_num--;
				log_read(buff, event_num, 1, 0);
				dsp_event(event_num);
			}
			else {
				fpprint("START OF LOG", DSP_INIT);
				event_num = 0;
			}
		}
	}
}

/****************************************************************************
 *																			*
 *	DSP_PERF 	Displays the preformance data on either the terminator 		*
 *				front panel or the eveready front panel.					*
 *																			*
 *				4/14/93  -  Modified the Eveready portion of the routine	*
 *							to fill the un-used processor locations with	*
 *							a space, to prevent invalid displays			*
 *																			*
 ****************************************************************************/
non_banked void
dsp_perf()
{

	char		line_2[40];
	char		vert_bar;
	int			i;
	static char	bar_char[2][17] = {
			/* top line */
		0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
		0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 
			/* bottom line */
		0x20, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 
		0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07
	};


	if (active_perf && (active_screen == ACT_PERF)) {
		if (PORTD & TERMINATOR) {
			if ((last_screen != ACT_PERF) ||  (num_proc != num_proc_r)) 
				cpu_group |= 0x10;
			num_proc_r = num_proc;
	
			if (cpu_group & 0x10) {
				/* clear the re-write flag */
				cpu_group &= 0x0F;
	
				/* reinitialize the display for the performance meter*/
				active_screen = ACT_PERF;
				last_screen = ACT_PERF;
	
				if (num_proc <= 32) {
					cpu_hi = num_proc;
					cpu_lo = 0;
					cpu_disp = num_proc;
				}
				else if (cpu_group == 1) {
					cpu_hi = num_proc;
					cpu_lo = 32;
					cpu_disp = num_proc - 32;
				}
				else {
					cpu_hi = 32;
					cpu_lo = 0;
					cpu_disp = 32;
				}
				/* Erase the screen */
				fpclear_cy();
	
	
				/* Window 1 (Title line) */
				cy_str(cmd_mode);
				cy_str("/W 0,1,39,2\r");			
				cy_str(dsp_mode);
				/* Text for window 1 */
				cy_str("           CPU ACTIVITY METER");	
	
				/* Window 2 (Percent of busy line) */
				cy_str(cmd_mode);
				cy_str("/W 0,3,4,15\r");			/* window 2 definition */
				cy_str(dsp_mode);
				/* Text for window 2 */
				cy_str("100\r\r  _\r\r\r 50\r\r  _\r\r\r  0");	
	
				/* Window 3 (Number of Processors) */
				cy_str(cmd_mode);
				cy_str("/W 4,14,39,15\r");			/* window 3 definition */
				cy_str(dsp_mode);
				/* Text for window 3 */
				sprintf(line_2, "       %d Processors  [%d:%d]",
					num_proc, cpu_lo, cpu_hi -1);
				if (num_proc > 32) {
					strcat(line_2, "           ");
					line_2[34] = 0xC4;
				}
				cy_str(line_2);	
				/* Window 4 (Histogram Window) */
				cy_str(cmd_mode);
				cy_str("B 24,98,239,24\r");			/* window 4 definition */
			}
	
			line_2[0] = 0;
	
			/* format the histogram command.  if the number of processors to 
			   be displayed is fewer than eight, always force eight in the 
			   command, to maintain a bar graph with for eight.  The extra 
			   processor will have a value of zero.
	
			   When greater than 24 processor are display for an additional 
			   graph  of value zero at the far left position.  This will place 
			   a separation between the fist bar the the window boundary.
			 */
	
			if (cpu_disp < 8)
				strcpy(fmt_str, "H 8");
			else if (cpu_disp > 24) 
				sprintf(fmt_str, "H %d,0", cpu_disp + 1);
			else 
				sprintf(fmt_str, "H %d", cpu_disp);
	
			for(i = cpu_lo; i < cpu_hi; i++) {
				sprintf(line_2, ",%d", histogram[i]);
				strcat(fmt_str, line_2);
			}
	
			if (cpu_disp < 8) {
				for(i = cpu_disp; i < 8; i++ )
					strcat(fmt_str, ",0");
			}
			strcat(fmt_str, "\r");
			cy_str(fmt_str);
		}
		else {
			active_screen = ACT_PERF;
			fmt_str[0] = 'C';
			fmt_str[1] = 'P';
			fmt_str[2] = 'U';
			fmt_str[3] = ' ';
			line_2[0] = 'A';
			line_2[1] = 'C';
			line_2[2] = 'T';
			line_2[3] = ' ';
	
			for (i = 0; (i < 16 && i < num_proc); i++) {
				vert_bar = (char ) (histogram[i]);
				fmt_str[i+4] = bar_char[0][vert_bar];
				line_2[i+4] = bar_char[1][vert_bar];
			}

			/* fill the remaining slots with a space "0x20" */
			if (num_proc < 16) {
				for (i = num_proc; i < 16; i++) {
					fmt_str[i+4] = bar_char[0][0];
					line_2[i+4] = bar_char[1][0];
				}
			}
			/* combine the two lines */
			for (i = 20; i < 40; i++)
				fmt_str[i] = line_2[i-20];
			fmt_str[40] = 0;
	
			hd_graph(fmt_str, 40);
		}
		active_perf = 0;
	}
	timeout(DISPLAY, CPU_PERF, MSEC_250);
}
	
/****************************************************************************
 *																			*
 *	DSP_VER:	Displays the System Controller's Firmware Version			*
 *																			*
 ****************************************************************************/
non_banked void
dsp_ver(scrn_fmt)
char	scrn_fmt;			/* Terminator Screen Format */
{
	struct MENU_LINE	*ml_ptr;
	struct MENU_ST		*dsp_ptr;
	char buff[40];
	extern char			fw_version[21];
	char	no_title[1] = {0};

	if (PORTD & TERMINATOR) {
		if (scrn_fmt == ACT_FWVER) {
			ml_ptr = &menu_line;
			dsp_ptr = &menu_dsp[ml_ptr->cur_ptr];
			strcpy(buff, "\r            ");
			strcat(buff, dsp_ptr->line_msg);
			strcat(buff, "\r");
			status_fmt(buff, ACT_FWVER, CY_NO_SCROLL, SCREEN_TO);
		}
		else
			status_fmt(no_title, ACT_STATUS, CY_SCROLL, NO_TIMEOUT);

		strcpy(buff, "FIRMWARE VERSION  ");
		strcat(buff, fw_version);
		strcat(buff, "\r");
	}
	else {
		screen_sw(ACT_STATUS, SCREEN_TO);
#ifdef LAB
		strcpy(buff, "  FIRMWARE VERSION\n");
#else
		strcpy(buff, "  FIRMWARE VERSION\n  ");
#endif
		strcat(buff, fw_version);
	}

	fpprint(buff,DSP_INIT);
}
	
/****************************************************************************
 *																			*
 *	MN_BPNMI:	Issue the Backplane NMI Signal								*
 *																			*
 ****************************************************************************/
void
mn_bpnmi()
{
	dsp_nmi(D_INIT);
}

/****************************************************************************
 *																			*
 *	MN_BOOT:	Display the last status message sent by the master CPU		*
 *																			*
 ****************************************************************************/
void
mn_boot()
{
	struct PROC_MSG_ST	*win;
	extern struct PROC_MSG_ST	proc_boot_win;
	char buff[40];
	
	block_btmsg = 0;
	if (PORTD & TERMINATOR) {
		if(btmsg_buff[0] == 0) { 
			fpclear_cy();
			strcpy(buff, "\r          BOOT STATUS MESSAGES\r");
			status_fmt(buff, ACT_BOOTMSG, CY_NO_SCROLL, SCREEN_TO);
			fpprint ("   NO BOOT STATUS", DSP_INIT);
		}
		else {
			win = &proc_boot_win;
			win->new_msg |= NEW_BT_MSG;
			if(btmsg_buff[0] != 0)
				win->new_msg |= NEW_PROC_MSG;
			dsp_boot();
		}
	}
	else {
		if(btmsg_buff[0] == 0) { 
			screen_sw(ACT_STATUS, SCREEN_TO);
			fpprint ("   NO BOOT STATUS", DSP_INIT);
		}
		else
			dsp_boot();
	}
}


/****************************************************************************
 *																			*
 *	MN_CPU_SEL:	Display the Result of the Boot Arbitration					*
 *																			*
 ****************************************************************************/
void
mn_cpu_sel()
{
	struct MENU_LINE	*ml_ptr;
	struct MENU_ST		*dsp_ptr;
	char buff[40];

	if (PORTD & TERMINATOR) {
		ml_ptr = &menu_line;
		dsp_ptr = &menu_dsp[ml_ptr->cur_ptr];
		strcpy(buff, "\r          ");
		strcat(buff, dsp_ptr->line_msg);
		strcat(buff, "\r");
		status_fmt(buff, ACT_CPUSEL, CY_NO_SCROLL, SCREEN_TO);
	}
	else
		screen_sw(ACT_CPUSEL, SCREEN_TO);

	fpprint(boot_prog, DSP_INIT);
}


/****************************************************************************
 *																			*
 *	MN_DB_SWITCH:	Routine call the dsp switch setting routine with the  	*
 *					initialization parameter								*
 *																			*
 ****************************************************************************/
void
mn_db_switch()
{
	dsp_db_switch(D_INIT);
}

/****************************************************************************
 *																			*
 *	MN_EVENT:		Routine call the display event log routine with the  	*
 *					initialization parameter								*
 *																			*
 ****************************************************************************/
void
mn_event()
{
	dsp_log(D_INIT);
}

	
/****************************************************************************
 *																			*
 *	MN_BPNMI:	Issue the Backplane NMI Signal								*
 *																			*
 ****************************************************************************/
void
mn_perf()
{
	struct MENU_LINE	*ml_ptr;
	struct MENU_ST		*dsp_ptr;
	char buff[40];

	if (active_perf == 1) {
		active_screen = ACT_PERF;
		dsp_perf();
	}
	else if(PORTD & TERMINATOR) {
		ml_ptr = &menu_line;
		dsp_ptr = &menu_dsp[ml_ptr->cur_ptr];
		strcpy(buff, "\r              ");
		strcat(buff, dsp_ptr->line_msg);
		strcat(buff, "\r");
		status_fmt(buff, ACT_STATUS, CY_NO_SCROLL, SCREEN_TO);
		cy_str("NO ACTIVE PERFORMANCE DATA\r");
	}
	else {
		screen_sw(ACT_STATUS, SCREEN_TO);
		fpprint("     NO ACTIVE\n  PERFORMANCE DATA", DSP_INIT);
	}

}
/****************************************************************************
 *																			*
 *	MN_RESET:		Routine issues a SCLR reset signal on the back plane 	*
 *																			*
 ****************************************************************************/
void
mn_reset()
{
	dsp_sclr(D_INIT);
}

/****************************************************************************
 *																			*
 *	MN_TEMP:		Routine displays the temperature and blower settings 	*
 *																			*
 ****************************************************************************/
void
mn_temp()
{
	extern struct ENV_DATA	env;
	struct 	ENV_DATA		*ptr;
	char	buff[100];
	char	*sptr;
	struct MENU_LINE	*ml_ptr;
	struct MENU_ST		*dsp_ptr;

	ptr = &env;

	if (PORTD & TERMINATOR) {
		ml_ptr = &menu_line;
		dsp_ptr = &menu_dsp[ml_ptr->cur_ptr];
		strcpy(buff, "\r           ");
		strcat(buff, dsp_ptr->line_msg);
		strcat(buff, "\r");
		status_fmt(buff, ACT_TEMP, CY_NO_SCROLL, SCREEN_TO);

		sprintf(buff,"\r        BLOWER A SPEED: %.0f RPM\r", ptr->blow_a_tac);
		cy_str(buff);
		sprintf(buff,"\r        BLOWER B SPEED: %.0f RPM\r", ptr->blow_b_tac);
		cy_str(buff);
		sprintf(buff,"\r     INLET TEMPERATURE: %.0f", ptr->temp_a);
		sptr = strchr(buff, 0);
		*sptr++ = DEGREE_CY;
		*sptr++ = 'C';
		*sptr++ = '\r';
		*sptr = 0;
		cy_str(buff);
	}
	else {
		screen_sw(ACT_TEMP, SCREEN_TO);
		sprintf(buff,"FAN SPEED: %.0f RPM\nINLET TEMP: %.0f", 
			ptr->blow_a_tac, ptr->temp_a);
		fpprint(buff, DSP_INIT);
	}
}

/****************************************************************************
 *																			*
 *	MN_VER:	Display the Sys Controller Firmware version from the menu		*
 *																			*
 ****************************************************************************/
void
mn_ver()
{
	dsp_ver(ACT_FWVER);
}


/****************************************************************************
 *																			*
 *	MN_VOLTS:		Routine displays the voltage supply status for all the  *
 *					system power supplies.  Because any out of spec power	*
 *					supply will cause an immediate power down, by default	*
 *					the fact the system is powered up, the supplies are in	*
 *					tolerance.												*
 *																			*
 ****************************************************************************/
void
mn_volts()
{

	char buff[42];
	extern struct ENV_DATA	env;
	struct 	ENV_DATA		*p;
	struct MENU_LINE	*ml_ptr;
	struct MENU_ST		*dsp_ptr;

	p = &env; 

	if (PORTD & TERMINATOR) {
		ml_ptr = &menu_line;
		dsp_ptr = &menu_dsp[ml_ptr->cur_ptr];
		strcpy(buff, "\r             ");
		strcat(buff, dsp_ptr->line_msg);
		strcat(buff, "\r");
		status_fmt(buff, ACT_VOLTS, CY_NO_SCROLL, SCREEN_TO);

		sprintf(buff, "      48   Volts = %.0f\r\r", p->V48);
		cy_str(buff);
		sprintf(buff, "      12   Volts = %.1f\r\r", p->V12);
		cy_str(buff);
		sprintf(buff, "       5   Volts = %.2f\r\r", p->V5);
		cy_str(buff);
		sprintf(buff, "      1.5  Volts = %.2f\r\r", p->V1p5);
		cy_str(buff);
		sprintf(buff, "     -5.2  Volts = %.1f\r\r", p->VM5p2);
		cy_str(buff);
		sprintf(buff, "      -12  Volts = %.1f\r\r", p->VM12);
		cy_str(buff);
	}
	else {
		screen_sw(ACT_VOLTS, SCREEN_TO);
		sprintf(buff, "%.0f     %.1f    %.2f\n %.2f  %.1f  %.1f", 
			p->V48, p->V12, p->V5, p->V1p5, p->VM5p2, p->VM12);
		fpprint(buff, DSP_INIT);
	}
}
