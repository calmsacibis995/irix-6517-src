/*
 * cmd.c
 *	FFSC command table and utility functions
 */

#include <vxworks.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ffsc.h"

#include "cmd.h"
#include "misc.h"
#include "msg.h"
#include "route.h"
#include "user.h"


/* Prototypes for the various command-handling functions. These are */
/* declared here rather than in a header file since (theoretically) */
/* nobody else should need to access them.			    */
int cmdAUTHORITY(char **, struct user *, struct dest *);
int cmdBS(char **, struct user *, struct dest *);
int cmdCECHO(char **, struct user *, struct dest *);
int cmdCOM(char **, struct user *, struct dest *);
int cmdCONSOLE(char **, struct user *, struct dest *);
int cmdDEBUG(char **, struct user *, struct dest *);
int cmdDEST(char **, struct user *, struct dest *);
int cmdDIRECT(char **, struct user *, struct dest *);
int cmdDOWNLOADER(char **, struct user *, struct dest *);
int cmdELSC(char **, struct user *, struct dest *);
int cmdEMSG(char **, struct user *, struct dest *);
int cmdEND(char **, struct user *, struct dest *);
int cmdESC(char **, struct user *, struct dest *);
int cmdEXIT(char **, struct user *, struct dest *);
int cmdFFSC(char **, struct user *, struct dest *);
int cmdFLASH(char **, struct user *, struct dest *);
int cmdGRAPHICS_COMMAND(char **, struct user *, struct dest *);
int cmdHELP(char **, struct user *, struct dest *);
int cmdIPADDR(char **, struct user *, struct dest *);
int cmdKILL(char **, struct user *, struct dest *);
int cmdLOG(char **, struct user *, struct dest *);
int cmdMEM(char **, struct user *, struct dest *);
int cmdNAP_TIME(char **, struct user *, struct dest *);
int cmdNOP(char **, struct user *, struct dest *);
int cmdOPTIONS(char **, struct user *, struct dest *);
int cmdPAGER(char **, struct user *, struct dest *);
int cmdPAS(char **, struct user *, struct dest *);
int cmdPASSWORD(char **, struct user *, struct dest *);
int cmdPRINTENV(char **, struct user *, struct dest *);
int cmdPWR(char **, struct user *, struct dest *);
int cmdRACKID(char **, struct user *, struct dest *);
int cmdRAT(char **, struct user *, struct dest *);
int cmdREBOOT_FFSC(char **, struct user *, struct dest *);
int cmdRESET_NVRAM(char **, struct user *, struct dest *);
int cmdRMSG(char **, struct user *, struct dest *);
int cmdSCAN(char **, struct user *, struct dest *);
int cmdSETENV(char **, struct user *, struct dest *);
int cmdSTEAL(char **, struct user *, struct dest *);
int cmdTTY_REINIT(char **, struct user *, struct dest *);
int cmdUNSETENV(char **, struct user *, struct dest *);
int cmdUNSTEAL(char **, struct user *, struct dest *);
int cmdVER(char **, struct user *, struct dest *);
int cmdPARTLST(char **, struct user *, struct dest *);
int cmdPARTGET(char **, struct user *, struct dest *);
int cmdPARTNAM(char **, struct user *, struct dest *);
int cmdPARTSCAN(char **, struct user *, struct dest *);
int cmdSWAPCONS(char **, struct user *, struct dest *);
int cmdPARTSET(char **, struct user *, struct dest *);
int cmdPARTCLR(char **, struct user *, struct dest *);
int cmdNUMRACKS(char **, struct user *, struct dest *);
int cmdARBITER(char **, struct user *, struct dest *);
int cmdMACADDR(char **, struct user *, struct dest *);

/* Command and option information */
cmdinfo_t cmdList[] = {
	{ "AUTH",	cmdAUTHORITY,
	  NULL },

	{ "AUTHORITY",	cmdAUTHORITY,
	  "AUTHority            - Show the current authority level\r\n"
	  "AUTHority <l> [<pw>] - Switch to authority level <l>\r\n"
	  "      Valid levels include: BASIC, SUPERVISOR, SERVICE\r\n"
	},

	{ "BS",		cmdBS,
	  "BS ?                 - Show current backspace character\r\n"
	  "BS <char>            - Change the backspace character\r\n"
	},

	{ "CECHO",	cmdCECHO,
	  "CECHO                - Toggle the command echo mode\r\n"
	  "CECHO ON             - Echo MMSC commands as they are typed\r\n"
	  "CECHO OFF            - Do not echo commands\r\n"
	},

	{ "COM",	cmdCOM,
	  "COM <p>               - Show parameters for MMSC port <p>\r\n"
	  "COM <p> CMD ON|OFF    - Enable/disable MMSC commands on <p>\r\n"
	  "COM <p> FUNCTION <f>  - Set the function of port <p> to <f>\r\n"
	  "COM <p> OOB ON|OFF    - Enable/disable OOB commands on <p>\r\n"
	  "COM <p> RXBUF <size>  - Set size of serial receive buffer\r\n"
	  "COM <p> SPEED <baud>  - Set baud rate of port <p> to <baud>\r\n"
	  "COM <p> TXBUF <size>  - Set size of serial transmit buffer\r\n"
	  "COM <p> HWFLOW ON|OFF - Enable/disable HW flow control on <p>\r\n"
	  "      Valid functions include: TERMINAL, SYSTEM, UPPER, LOWER\r\n"
	  "                               ALTCONS, DAEMON\r\n"
	  "      Valid baud rates include: 300, 1200, 2400, 4800, 9600,\r\n"
	  "                                19200, 38400, 57600, 115200\r\n"
	},

	{ "CONS",	cmdCONSOLE,
	  NULL },

	{ "CONSOLE",	cmdCONSOLE,
	  "CONSole              - Switch to normal console input mode\r\n"
	  "CONSole <args>       - Send <args> to system console\r\n"
	},

	{ "DEBUG",	cmdDEBUG,
	  NULL },

	{ "NUMRACKS",	cmdNUMRACKS,
	  "NUMRACKS             - Show the number of racks in a system\r\n"
	},

	{ "DEST",	cmdDEST,
	  "DEST                 - Set/display current default destination\r\n"
	},

	{ "DIRECT",	cmdDIRECT,
	  "DIRECT               - Connect to the other terminal\r\n"
	},

	{ "DOWNLOADER", cmdDOWNLOADER,
	  NULL },

	{ "ELSC",	cmdELSC,
	  NULL },		/* Retained for historical reasons */

	{ "EMSG",	cmdEMSG,
	  NULL },

	{ "END",	cmdEND,
	  "END ?                - Show current end-of-command character\r\n"
	  "END <char>           - Change the end-of-command character\r\n"
	},

	{ "ESC",	cmdESC,
	  "ESC ?                - Show current escape character\r\n"
	  "ESC <char>           - Change the escape character\r\n"
	},

	{ "EXIT",	cmdEXIT,
	  "EXIT                 - Return to previous input mode\r\n"
	  "EXIT ?               - Show current exit character\r\n"
	  "EXIT <char>          - Change the exit character\r\n"
	},

	{ "FFSC",	cmdFFSC,
	  NULL },		/* Retained for historical reasons */

	{ "FLASH",	cmdFLASH,
	  "FLASH [FROM SYSTEM]  - Flash firmware image from system\r\n"
	  "FLASH FROM CONSOLE   - Flash firmware image from console\r\n"
	},

	{ "GRAPHICS_COMMAND",	cmdGRAPHICS_COMMAND,
	  NULL },

	{ "HELP",	cmdHELP,
	  "HELP                 - Show list of command names\r\n"
	  "HELP ALL             - Show info on all commands\r\n"
	  "HELP <cmd>           - Show info on command <cmd>\r\n"
	},

	{ "IPADDR",	cmdIPADDR,
	  NULL },

	{ "KILL",	cmdKILL,
	  "KILL ?               - Show current kill-line character\r\n"
	  "KILL <char>          - Change the kill-line character\r\n"
	},

	{ "LOG",	cmdLOG,
	  "LOG <log> CLEAR            - Clear the contents of <log>\r\n"
	  "LOG <log> DUMP [<val>]     - Dump last <val> lines of <log>\r\n"
	  "LOG <log> DISABLE          - Disable logging to <log>\r\n"
	  "LOG <log> ENABLE           - Enable logging to <log>\r\n"
	  "LOG <log> INFO             - Show info about <log>\r\n"
	  "LOG <log> LINES <val>      - Set # of lines in <log>\r\n"
	  "LOG <log> LENGTH <val>     - Set average line length in <log>\r\n"
	  "LOG <log> ?                - Same as LOG INFO\r\n"
	  "      Valid values for <log> are: MSC, SYSTEM, TERMINAL,\r\n"
	  "                                  ALTCONS, DEBUG, DISPLAY\r\n"
	},

	{ "MEM",	cmdMEM,
	  NULL },

	{ "MMSC",	cmdFFSC,
	  "MMSC                 - Switch to MMSC input mode\r\n"
	  "MMSC <args>          - Send <args> directly to the MMSC\r\n"
	},

	{ "MMSG",	cmdEMSG,
	  "MMSG                 - Rotate between ON/TERSE/OFF modes\r\n"
	  "MMSG ON              - Display MSC messages on terminal\r\n"
	  "MMSG TERSE           - Display MSC messages without ID header\r\n"
	  "MMSG OFF             - Do not display MSC messages\r\n"
	  "MMSG RACK            - Show rack receiving MSC messages\r\n"
	  "MMSG RACK <r>        - Send MSC messages to TTY on rack <r>\r\n"
	  "MMSG ALTRACK [<r>]   - Same as RACK, but for ALTERNATE console\r\n"
	},

	{ "MSC",	cmdELSC,
	  "MSC                  - Switch to MSC input mode\r\n"
	  "MSC <args>           - Send <args> to the MSC\r\n"
	},

	{ "NAP_TIME",	cmdNAP_TIME,
	  "NAP_TIME             - Show the current nap interval\r\n"
	  "NAP_TIME DEFAULT     - Reset nap interval to default value\r\n"
	  "NAP_TIME <value>     - Set nap interval to <value> usecs\r\n"
	},

	{ "NOP",	cmdNOP,
	  NULL },

	{ "OPTIONS",	cmdOPTIONS,
	  "OPTIONS              - Show the current option flags\r\n"
	  "OPTIONS <value>      - Set the current option flags\r\n"
	},

	{ "PAGER",	cmdPAGER,
	  "PAGER BACK <char>    - Set page-backward character\r\n"
	  "PAGER FWD <char>     - Set page-forward character\r\n"
	  "PAGER INFO           - Show information about pager\r\n"
	  "PAGER LINES <val>    - Set the page length to <val> lines\r\n"
	  "PAGER OFF            - Disable paged output of long messages\r\n"
	  "PAGER ON             - Enable paged output of long messages\r\n"
	  "PAGER QUIT <char>    - Set quit output character\r\n"
	  "PAGER ?              - Same as PAGER INFO\r\n"
	},

	{ "PAS",	cmdPAS,
	  NULL },

	{ "PASSWORD",	cmdPASSWORD,
	  "PASSWORD SET <p> <s> - Set password <p> to <s>\r\n"
	  "PASSWORD SETMMSC     - Like SET, but don't notify MSC\r\n"
	  "PASSWORD UNSET <p>   - Remove password <p>\r\n"
	  "      Valid values for <p> include: MSC, SUPERVISOR, SERVICE\r\n"
	},

	{ "PRINTENV",	cmdPRINTENV,
	  "PRINTENV             - Print non-default environment variables\r\n"
	  "PRINTENV ALL         - Print all environment variables\r\n"
	},

	{ "PWR",	cmdPWR,
	  NULL },

	{ "RACKID",	cmdRACKID,
	  "RACKID               - Show the ID of the addressed rack\r\n"
	  "RACKID <value>       - Change the ID of the addressed rack\r\n"
	},

	{ "RAT",	cmdRAT,
	  "RAT                  - Enter Remote Access Tool mode\r\n"
	},

	{ "REBOOT_FFSC", cmdREBOOT_FFSC,
	  NULL },		/* Retained for historical reasons */

	{ "RESET_FFSC",  cmdREBOOT_FFSC,
	  NULL },		/* Ditto */

	{ "RESET_MMSC",  cmdREBOOT_FFSC,
	  "RESET_MMSC           - Restart the addressed MMSC(s)\r\n"
	},

	{ "RESET_NVRAM", cmdRESET_NVRAM,
	  "RESET_NVRAM          - Reset NVRAM to default values\r\n"
	},

	{ "RMSG",	cmdRMSG,
	  "RMSG                 - Switch to the next response message mode\r\n"
	  "RMSG ON              - Show responses to all MSC/MMSC commands\r\n"
	  "RMSG ERROR           - Only show non-OK responses\r\n"
	  "RMSG OFF             - Do not show responses to commands\r\n"
	},

	{ "SCAN",	cmdSCAN,
	  "SCAN                 - Scan for new or changed modules\r\n"
	},

	{ "SETENV",	cmdSETENV,
	  "SETENV <var>[=]<val> - Set an environment variable\r\n"
	},

	{ "STEAL",	cmdSTEAL,
	  "STEAL                - Take control of the system console\r\n"
	},

	{ "PARTLST",     cmdPARTLST,
	  "PARTLST              - List partition definitions\r\n"
	},
	{ "PARTSCAN",     cmdPARTSCAN,
	  "PARTSCAN             - Scan for new partition configuration(s)\r\n"
	},
	{ "PARTGET",     cmdPARTGET,
	  "PARTGET <id>         - Get information on partition mapping(s)\r\n"
	},
	{ "PARTNAM",     cmdPARTNAM,
	  "PARTNAM id name      - Set partition name for partition id\r\n"
	},

	{ "PARTCLR",     cmdPARTCLR,
	  "PARTCLR id           - Clear partition mappings for id\r\n"
	},

	{ "PARTSET",     cmdPARTSET,
	  "PARTSET id modlist   - Set partition mapping using modlist\r\n"
	},

	{ "SWAPCONS",     cmdSWAPCONS,
	  "SWAPCONS            - Talk to COM4 or COM6 BaseIO (IO6) device (debug=0)\r\n"
	},

	{ "TTY_REINIT",	cmdTTY_REINIT,
	  NULL },

	{ "UNSETENV",	cmdUNSETENV,
	  "UNSETENV <var>       - Unset an environment variable\r\n"
	},

	{ "UNSTEAL",	cmdUNSTEAL,
	  "UNSTEAL              - Give console back to other terminal\r\n"
	},

	{ "VER",	cmdVER,
	  "VER                  - Show current MMSC firmware revision\r\n"
	},

	{ "?",		cmdHELP,
	  "?                    - Same as HELP\r\n"
	},
	{ "ARBITER",  cmdARBITER,
	  "ARBITER              - Show which MMSC is the arbiter now\r\n"
	}, 
	{ "MACADDR",  cmdMACADDR,
	  "MACADDR              - Show the Ethernet MAC address\r\n"
	},

	{ NULL, NULL, NULL }	/* End of list */
};


/*
 * cmdChangeCtrlChar
 *	Change a console character, given a pointer to the character
 *	specification and the console task STATE command token for the
 *	character in question.
 */
void
cmdChangeCtrlChar(char **Last, user_t *User, const char *Token)
{
	char NewChar;

	/* Go parse the character specification */
	NewChar = cmdParseCharSpecification(Last);

	/* Barf if the specification was invalid or empty */
	if (NewChar < 0) {
		sprintf(Responses[ffscLocal.rackid]->Text,
			"ERROR ARG");
	}

	/* If we've been asked to show the current setting, do so */
	else if (NewChar == 0) {
		sprintf(Responses[ffscLocal.rackid]->Text,
			"OK;STATE %s?",
			Token);
	}

	/* Ignore requests that do not originate from a console */
	else if (User->Type != UT_CONSOLE) {
		sprintf(Responses[ffscLocal.rackid]->Text,
			"IGNORED");
	}

	/* If all is well, set up the change by piggybacking the */
	/* appropriate console task STATE command to our response */
	else {
		sprintf(Responses[ffscLocal.rackid]->Text,
			"OK;STATE %s%d",
			Token, NewChar);
	}
}


/*
 * cmdChangeEchoMode
 *	Change a console echo mode, given a pointer to the remainder
 *	of the command line, a list of mode names, and console task
 *	STATE command token for the echo mode in question.
 */
void
cmdChangeSimpleMode(char *ModeName, tokeninfo_t *ModeInfo, const char *Token)
{
	int Value;

	/* No mode name at all means switch to the "next" state */
	if (ModeName == NULL) {
		sprintf(Responses[ffscLocal.rackid]->Text,
			"OK;STATE %s+", Token);
		return;
	}

	/* Look for the mode name in the supplied list */
	if (ffscTokenToInt(ModeInfo, ModeName, &Value) != OK) {
		ffscMsg("Invalid mode name \"%s\"", ModeName);
		sprintf(Responses[ffscLocal.rackid]->Text, "ERROR ARG");
		return;
	}

	/* Make the appropriate mode change */
	sprintf(Responses[ffscLocal.rackid]->Text,
		"OK;STATE %s%d",
		Token, Value);
}


/*
 * cmdParseCharSpecification
 *	Parses a standard character specification from the given
 *	strtok_r "last" pointer.  Returns the character if it is valid,
 *	0 if it was "?", or -1 if it was empty or invalid.
 */
int
cmdParseCharSpecification(char **Last)
{
	char *Ptr;
	char *String;
	int  Value;

	/* Isolate the character specification */
	String = strtok_r(NULL, " \t", Last);
	if (String == NULL  ||  String[0] == '\0') {
		return -1;
	}

	/* A "?" is treated specially ("show current setting") */
	if (strcmp(String, "?") == 0) {
		return 0;
	}

	/* If the token is exactly one byte long, use it as-is */
	if (String[1] == '\0') {
		return String[0];
	}

	/* If the first character is "^", treat it as a control */
	/* sequence.						*/
	if (String[0] == '^') {
		if (String[1] == '\0'  || String[2] != '\0') {
			ffscMsg("ParseChar: Invalid control sequence");
			return -1;
		}

		Value = toupper(String[1]) - 64;
		if (Value < 0  ||  Value > 31) {
			ffscMsg("ParseChar: Invalid control character");
			return -1;
		}

		return (char) Value;
	}

	/* Try to treat the token as an integer */
	Value = (int) strtol(String, &Ptr, 0);
	if (*Ptr != '\0'  ||  Value <= 0  ||  Value > 255) {
		ffscMsg("ParseChar: Invalid character specification");
		return -1;
	}

	return Value;
}


/*
 * cmdProcessFFSCCommand
 *	Given a command string, its origin and a destination, tries to
 *	parse the string as an FFSC command and perform the appropriate
 *	actions. Returns <0 if an error occurs, or else a code indicating
 *	what further processing (if any) is needed.
 */
int
cmdProcessFFSCCommand(const char *InCommand, user_t *User, dest_t *Dest)
{
	char *CmdName;
	char Command[MAX_FFSC_CMD_LEN + 1];
	char *Last;
	int  i;
	int  Result;

	/* Make a working copy of the command string */
	strncpy(Command, InCommand, MAX_FFSC_CMD_LEN);
	Command[MAX_FFSC_CMD_LEN] = '\0';

	/* Extract the first token of the command */
	Last = NULL;
	CmdName = strtok_r(Command, " \t", &Last);
	ffscConvertToUpperCase(CmdName);

	/* Search for the command in our Big Table Of Commands */
	Result = PFC_NOTFFSC;		/* Assume failure */
	for (i = 0;  cmdList[i].Name != NULL;  ++i) {
		if (strcmp(CmdName, cmdList[i].Name) == 0) {
			Result = (*cmdList[i].Function)(&Last, User, Dest);
			break;
		}
	}

	return Result;
}










