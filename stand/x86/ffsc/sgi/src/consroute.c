/*
 * consroute.c
 *	Console task functions for handling input from the router task
 */

#include <vxworks.h>
#include <sys/types.h>

#include <errno.h>
#include <iolib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tasklib.h>

#include "ffsc.h"

#include "buf.h"
#include "cmd.h"
#include "console.h"
#include "elsc.h"
#include "misc.h"
#include "route.h"
#include "timeout.h"
#include "user.h"


/* Internal functions */
static void crProcessMSGCmd(console_t *, char *);
static void crProcessSHUTDOWNCmd(console_t *, char *);
static void crProcessSTATECmd(console_t *, char *);
static void crProcessSYSCmd(console_t *, char *);
static int  crSetCtrlChar(console_t *, int, char *, const char *);
static int  crSetPagerChar(console_t *, int, char *, const char *);


/*
 * crRouterIn
 *	Reads a message from the router pipe and responds accordingly.
 *	Returns OK/ERROR.
 */
STATUS
crRouterIn(console_t *Console)
{
	char RouterMsg[MAX_FFSC_CMD_LEN + 1];
	char *Last;
	int  InLen;

	/* Read the message from the router */
	InLen = timeoutRead(Console->FDs[CCFD_R2C],
			    RouterMsg,
			    MAX_FFSC_CMD_LEN,
			    ffscTune[TUNE_CI_ROUTER_MSG]);
	if (InLen < 0) {
		ffscMsgS("Console %s got error reading from router",
			 Console->Name);
		return ERROR;
	}
	else if (InLen == 0) {
		return ERROR;
	}

	/* Strip off trailing CR for neatness */
	Last = strrchr(RouterMsg, FFSC_ACK_END_CHAR);
	if (Last != NULL) {
		*Last = '\0';
	}

	/* Print the message if desired */
	RouterMsg[InLen] = '\0';
	if (ffscDebug.Flags & FDBF_CONSOLE) {
		ffscMsg("Unsolicited message to %s from router: \"%s\"",
			Console->Name, RouterMsg);
	}

	/* Go process the command */
	return crProcessCmd(Console, RouterMsg);
}


/*
 * crProcessCmd
 *	Process a command from the router. Returns an appropriate response
 *	string. Returns OK/ERROR.
 */
STATUS
crProcessCmd(console_t *Console, char *Cmd)
{
	char *CmdName;
	char *Last = NULL;

	/* Proceed according to the command name */
	CmdName = strtok_r(Cmd, " ", &Last);
	if (CmdName == NULL) {
		ffscMsg("Empty message to %s from router", Console->Name);
	}
	else if (strcmp(CmdName, "MSG") == 0) {
		crProcessMSGCmd(Console, Last);
	}
	else if (strcmp(CmdName, "SHUTDOWN") == 0) {
		crProcessSHUTDOWNCmd(Console, Last);
	}
	else if (strcmp(CmdName, "STATE") == 0) {
		crProcessSTATECmd(Console, Last);
	}
	else if (strcmp(CmdName, "SYS") == 0) {
		crProcessSYSCmd(Console, Last);
	}
	else {
		ffscMsg("Unknown command to %s from router: \"%s\"",
			Console->Name, CmdName);
	}

	return OK;
}


/*
 * crProcessMSGCmd
 *	Parses and processes a MSG command from the router.
 *
 *	MSG commands have the format:
 *
 *		MSG ["@"destination] [header":"]text
 *
 *	The "header" text is not printed when EMsg mode is "TERSE"
 *	and is intended for origin information (e.g. "RACK 1 BAY 2").
 *	When it is printed, any spaces following the header delimiter (":")
 *	are reduced to a single space; when it is not printed, all
 *	spaces that follow the delimiter are discarded.
 */
void
crProcessMSGCmd(console_t *DfltConsole, char *Cmd)
{
	char *CurrPtr = Cmd;
	char *Dest = NULL;
	char *Header = NULL;
	char *Msg = NULL;
	char Output[MAX_CONS_CMD_LEN + 5];
	console_t *Console = NULL;
	int  i;

	/* Skip over leading white space */
	CurrPtr += strspn(CurrPtr, " ");

	/* Treat the first token as a destination if necessary */
	if (*CurrPtr == '@') {

		/* The current token is indeed a destination */
		Dest = CurrPtr;

		/* Find the end of this token and the start of the next */
		CurrPtr  = CurrPtr + strcspn(CurrPtr, " ");
		if (*CurrPtr != '\0') {
			*CurrPtr = '\0';
			CurrPtr  += strspn(CurrPtr + 1, " ") + 1;
		}

		/* Figure out which device has been requested */
		for (i = 0;  i < NUM_CONSOLES;  ++i) {
			if (consoles[i] != NULL  &&
			    strcmp(Dest+1, consoles[i]->Name) == 0)
			{
				Console = consoles[i];
				break;
			}
		}
		if (Console == NULL) {
			ffscMsg("Unrecognized destination for MSG: \"%s\"",
				Dest);
			return;
		}
	}

	/* If no destination was specified, use the default */
	if (Console == NULL) {
		Console = DfltConsole;
	}

	/* See if a header is present */
	Msg     = CurrPtr;
	CurrPtr = strchr(CurrPtr, Console->Delim[COND_HDR]);
	if (CurrPtr != NULL) {
		Header = Msg;
		*CurrPtr = '\0';

		++CurrPtr;
		Msg = CurrPtr + strspn(CurrPtr, " ");
	}

	/* If ELSC messages are being ignored, we have nothing to do */
	if (Console->EMsg == CONEM_OFF) {
		return;
	}

	/* Likewise if the message is empty and we haven't been told */
	/* to print blank messages.				     */
	if (strlen(Msg) == 0  &&  !(Console->User->Options & UO_PRINTBLANK)) {
		return;
	}

	/* Write the header and/or message according to EMsg mode */
	if (Console->EMsg == CONEM_TERSE  ||  Header == NULL) {
		sprintf(Output, "%s" BUF_MSG_END, Msg);
	}
	else {
		sprintf(Output, "%s-%s" BUF_MSG_END, Header, Msg);
	}
	cmPrintMsg(Console, Output);

	return;
}


/*
 * crProcessSHUTDOWNCmd
 *	Processes a SHUTDOWN command from the FFSC: prints a warning
 *	message, arranges for a wakeup message after reboot, etc.
 *
 *	SHUTDOWN has a simple format:
 *		SHUTDOWN
 */
void
crProcessSHUTDOWNCmd(console_t *Console, char *LastIn)
{
	char Message[80];

	/* The drill is different for REMOTE and non-REMOTE consoles */
	if (Console->Type != CONT_REMOTE) {

		/* Deliver an informative message */
		sprintf(Message,
			STR_END
			    "R%ld:MMSC RESET - console will not "
			    "respond for approximately 30 seconds"
			    STR_END,
			ffscLocal.rackid);
		(void) bufWrite(Console->UserOutBuf, Message, strlen(Message));

		/* Arrange for a wakeup message as well */
		Console->Flags |= CONF_REBOOTMSG;
		consUpdateConsInfo(Console);
	}
	else {

		/* Deliver an informative message */
		sprintf(Message,
			STR_END
			    "R%ld:MMSC RESET - closing connection"
			    STR_END,
			ffscLocal.rackid);
		(void) bufWrite(Console->UserOutBuf, Message, strlen(Message));

		/* Wait a few ticks to let it drain out */
		taskDelay(30);

		/* Shut down the remote connection */
		close(Console->UserOutBuf->FD);
	}

	/* Do not accept any further input, so our message has a fighting */
	/* chance of being the last thing on the user's screen		  */
	bufHold(Console->SysInBuf);
	bufHold(Console->UserInBuf);
	Console->Flags |= (CONF_HOLDSYSIN | CONF_HOLDUSERIN);
}


/*
 * crProcessSTATECmd
 *	Parses and processes a STATE command from the FFSC. 
 *	The input string is a "lasts" value from strtok_r.
 *
 *	STATE commands have the format:
 *
 *		STATE ["@"destination]	[CC###] [CE###] [CK###] [CX###]
 *					[BR##] [BS##]
 *					[EC#] [EE#] [ER#]
 *					[[FS##] [FU##] [FE##] [FP##] [FB##] M#]
 *					[PB###] [PF###] [PQ###]
 *					[LP#]
 *					[WU###]
 */
void
crProcessSTATECmd(console_t *DfltConsole, char *LastIn)
{
	bayid_t ELSCBay = BAYID_UNASSIGNED;
	char *Dest = NULL;
	char *Last = LastIn;
	char *Token;
	console_t *Console = NULL;
	int  ConsID;
	int  ProgressFD = -1;
	int  SysFD  = -1;
	int  UpdateInfo = 0;
	int  UserFD = -1;
	int  Value;
	pagebuf_t *PageBuf = NULL;

	/* Pick up the first token, which may be the destination device */
	Dest = strtok_r(NULL, " ", &Last);
	if (Dest == NULL) {
		return;
	}
	else if (Dest[0] == '@') {

		/* Figure out which device has been requested */
		for (ConsID = 0;  ConsID < NUM_CONSOLES;  ++ConsID) {
			if (consoles[ConsID] != NULL  &&
			    strcmp(Dest+1, consoles[ConsID]->Name) == 0)
			{
				Console = consoles[ConsID];
				break;
			}
		}
		if (Console == NULL) {
			ffscMsg("Unrecognized destination for STATE: \"%s\"",
				Dest);
			return;
		}
	}

	/* If no destination was specified, use the default */
	if (Console == NULL) {
		Console = DfltConsole;
		Token = Dest;
	}
	else {
		Token = strtok_r(NULL, " ", &Last);
	}

	/* Now sift through the various arguments */
	while (Token != NULL) {

		if (strncmp(Token, "BR", 2) == 0) {

			/* Reset console flags */
			Console->Flags &= ~atoi(Token + 2);
			++UpdateInfo;
		}
		else if (strncmp(Token, "BS", 2) == 0) {

			/* Set console flags */
			Console->Flags |= atoi(Token + 2);
			++UpdateInfo;
		}
		else if (strncmp(Token, "CB", 2) == 0) {

			/* Set the backspace char */
			UpdateInfo += crSetCtrlChar(Console,
						    CONC_BS,
						    Token + 2,
						    "backspace");
		}
		else if (strncmp(Token, "CC", 2) == 0) {

			/* Set the End Of Command character */
			UpdateInfo += crSetCtrlChar(Console,
						    CONC_END,
						    Token + 2,
						    "command end");
		}
		else if (strncmp(Token, "CE", 2) == 0) {

			/* Set the Escape character */
			UpdateInfo += crSetCtrlChar(Console,
						    CONC_ESC,
						    Token + 2,
						    "MMSC escape");
		}
		else if (strncmp(Token, "CK", 2) == 0) {

			/* Set the Kill character */
			UpdateInfo += crSetCtrlChar(Console,
						    CONC_KILL,
						    Token + 2,
						    "kill");
		}
		else if (strncmp(Token, "CX", 2) == 0) {

			/* Set the Exit character */
			UpdateInfo += crSetCtrlChar(Console,
						    CONC_EXIT,
						    Token + 2,
						    "exit");
		}
		else if (strncmp(Token, "EC", 2) == 0) {

			/* Set CEcho mode */
			if (strcmp(Token+2, "+") == 0) {
				Value = Console->CEcho + 1;
			}
			else {
				Value = atoi(Token + 2);
			}

			Console->CEcho = Value % CONCE_MAX;
			if (ffscDebug.Flags & FDBF_CONSOLE) {
				ffscMsg("Changed CECHO mode to %d on %s",
					Console->CEcho, Console->Name);
			}

			++UpdateInfo;
		}
		else if (strncmp(Token, "EE", 2) == 0) {

			/* Set EMsg mode */
			if (strcmp(Token+2, "+") == 0) {
				Value = Console->EMsg + 1;
			}
			else {
				Value = atoi(Token + 2);
			}

			Console->EMsg = Value % CONEM_MAX;
			if (ffscDebug.Flags & FDBF_CONSOLE) {
				ffscMsg("Changed EECHO mode to %d on %s",
					Console->EMsg, Console->Name);
			}

			++UpdateInfo;
		}
		else if (strncmp(Token, "ER", 2) == 0) {

			/* Set RMsg mode */
			if (strcmp(Token+2, "+") == 0) {
				Value = Console->RMsg + 1;
			}
			else {
				Value = atoi(Token + 2);
			}

			Console->RMsg = Value % CONRM_MAX;
			if (ffscDebug.Flags & FDBF_CONSOLE) {
				ffscMsg("Changed RECHO mode to %d on %s",
					Console->RMsg, Console->Name);
			}

			++UpdateInfo;
		}
		else if (strncmp(Token, "FB", 2) == 0) {

			/* Set PageBuffer address */
			PageBuf = (pagebuf_t *) atoi(Token + 2);
		}
		else if (strncmp(Token, "FE", 2) == 0) {

			/* Set ELSC Bay # */
			ELSCBay = atoi(Token + 2);
		}
		else if (strncmp(Token, "FP", 2) == 0) {

			/* Set progress FD */
			ProgressFD = atoi(Token + 2);
		}
		else if (strncmp(Token, "FS", 2) == 0) {

			/* Set remote SYS FD */
			SysFD = atoi(Token + 2);
		}
		else if (strncmp(Token, "FU", 2) == 0) {

			/* Set remote user FD */
			UserFD = atoi(Token + 2);
		}
		else if (strncmp(Token, "LP", 2) == 0) {

			/* Set # of lines per page */
			Value = atoi(Token + 2);
			Console->LinesPerPage = Value;
		}
		else if (strncmp(Token, "PB", 2) == 0) {

			/* Set the pager BACK character */
			UpdateInfo += crSetPagerChar(Console,
						     CONP_BACK,
						     Token + 2,
						     "back");
		}
		else if (strncmp(Token, "PF", 2) == 0) {

			/* Set the pager FWD character */
			UpdateInfo += crSetPagerChar(Console,
						     CONP_FWD,
						     Token + 2,
						     "fwd");
		}
		else if (strncmp(Token, "PQ", 2) == 0) {

			/* Set the pager QUIT character */
			UpdateInfo += crSetPagerChar(Console,
						     CONP_QUIT,
						     Token + 2,
						     "quit");
		}
		else if (strncmp(Token, "WU", 2) == 0) {

			/* Set WakeUp interval */
			if (strcmp(Token+2, "?") == 0) {
				char Message[80];
				int  WakeUpUSecs;

				WakeUpUSecs = ((Console->WakeUp > 0)
					       ? Console->WakeUp
					       : ffscTune[TUNE_CW_WAKEUP]);

				sprintf(Message,
					STR_END
					    "Current setting of nap interval "
					        "is %d.%06d seconds"
					STR_END,
					WakeUpUSecs / 1000000,
					WakeUpUSecs % 1000000);

				cmPrintResponse(Console, Message);
			}
			else {
				Value = atoi(Token + 2);
				Console->WakeUp = Value;

				++UpdateInfo;
				Console->Flags |= CONF_EVALWAKEUP;

				if (ffscDebug.Flags & FDBF_CONSOLE) {
					ffscMsg("Changed WakeUp interval to "
						    "%d usecs on %s",
						Console->WakeUp,
						Console->Name);
				}
			}
		}
		else if (Token[0] == 'M') {

			/* Set input mode */
			int OldMode = Console->Mode;

			/* Find out which mode to switch to */
			Value = atoi(Token + 1);

			/* Say what's about to happen if desired */
			if (ffscDebug.Flags & FDBF_CONSOLE) {
				ffscMsg("Changing to MODE %d on %s: "
					    "B%p E%ld F%x P%d S%d U%d",
					Value,
					Console->Name,
					PageBuf,
					ELSCBay,
					Console->Flags,
					ProgressFD,
					SysFD,
					UserFD);
			}

			/* Formally leave our current mode */
			clLeaveCurrentMode(Console);

			/* If this is the remote console task, set up	*/
			/* its user FD if one was specified (if there   */
			/* is no user FD, then presumably we will reuse */
			/* the existing one for the new mode).		*/
			if (Console->Type == CONT_REMOTE  &&  UserFD >= 0) {

				/* Make sure we aren't already using one */
				if (Console->FDs[CCFD_USER] >= 0) {
					ffscMsg("Warning: FD %d never closed "
						    "in %s",
						Console->FDs[CCFD_USER],
						Console->Name);
					close(Console->FDs[CCFD_USER]);
				}

				Console->FDs[CCFD_USER] = UserFD;
			}

			/* If this is a fresh connection on the remote  */
			/* console, signal the other side we are READY. */
			if (Console->Type == CONT_REMOTE  &&
			    OldMode == CONM_IDLE)
			{
				Console->Flags |= CONF_READYMSG;
			}

			/* Now formally enter the new mode and perhaps */
			/* arrange for a prompt (or just a CR/LF to    */
			/* clean up in the case of CONSOLE mode)       */
			switch (Value) {

			    case CONM_CONSOLE:
			        Console->FDs[CCFD_SYS] = SysFD;
				ceEnterConsoleMode(Console);
				Console->Flags |= CONF_SHOWPROMPT;
				break;

			    case CONM_ELSC:
				Console->ELSC = &ELSC[ELSCBay];
				ceEnterELSCMode(Console);
				break;

			    case CONM_FFSC:
				ceEnterFFSCMode(Console);
				Console->Flags |= CONF_SHOWPROMPT;
				break;

			    case CONM_COPYUSER:
				Console->FDs[CCFD_REMOTE] = SysFD;
				ceEnterCopyUserMode(Console);
				break;

			    case CONM_COPYSYS:
				Console->FDs[CCFD_REMOTE] = SysFD;
				Console->FDs[CCFD_PROGRESS] = ProgressFD;
				ceEnterCopySysMode(Console);
				break;

			    case CONM_FLASH:
				Console->FDs[CCFD_REMOTE] = SysFD;
				Console->FDs[CCFD_PROGRESS] = ProgressFD;
				Console->PrevMode = OldMode;
				ceEnterFlashMode(Console);
				break;

			    case CONM_MODEM:
				Console->FDs[CCFD_REMOTE] = SysFD;
				ceEnterModemMode(Console);
				break;

			    case CONM_IDLE:
				ceEnterIdleMode(Console);
				break;

			    case CONM_WATCHSYS:
				ceEnterWatchSysMode(Console);
				break;

			    case CONM_RAT:
				ceEnterRATMode(Console);
				break;

			    case CONM_PAGER:
				Console->PageBuf  = PageBuf;
				Console->PrevMode = OldMode;
				ceEnterPagerMode(Console);
				break;

			    default:
				ffscMsg("Attempted to enter MODE %d on %s",
					Value, Console->Name);
			}

			/* KLUDGE: If the FLASHMSG flag has been sent,	*/
			/* write some reassuring messages directly to	*/
			/* USER FD. There must be a better way to do	*/
			/* this, but it's too close to Ship Day.	*/
			if (Console->Flags & CONF_FLASHMSG  &&
			    Console->FDs[CCFD_USER] >= 0)
			{
				char KludgyMsg[160];

				sprintf(KludgyMsg,
					STR_END
					    "Clearing non-volatile storage"
					    STR_END
					    "** The transfer will not begin "
					    "for approximately 100 seconds"
					    STR_END);
				timeoutWrite(Console->FDs[CCFD_USER],
					     KludgyMsg,
					     strlen(KludgyMsg),
					     ffscTune[TUNE_CO_PROGRESS]);
			}
			Console->Flags &= ~CONF_FLASHMSG;
		}
		else {
			ffscMsg("Unknown STATE token: %s", Token);
			return;
		}

		Token = strtok_r(NULL, " ", &Last);
	}

	/* Update the console info in NVRAM if necessary */
	if (UpdateInfo > 0) {
		consUpdateConsInfo(Console);
	}

	return;
}


/*
 * crProcessSYSCmd
 *	Parses and processes a SYS command from the FFSC.
 *
 *	SYS commands have the format:
 *
 *		SYS ["@"destination] text
 *
 *	The text is sent to the SysOut buffer associated with the
 *	console "destination", followed by a CR/LF.
 */
void
crProcessSYSCmd(console_t *DfltConsole, char *Cmd)
{
	char *CurrPtr = Cmd;
	char *Dest = NULL;
	char Output[MAX_CONS_CMD_LEN + 3];
	console_t *Console = NULL;
	int  i;

	/* Skip over leading white space */
	CurrPtr += strspn(CurrPtr, " ");

	/* Treat the first token as a destination if necessary */
	if (*CurrPtr == '@') {

		/* The current token is indeed a destination */
		Dest = CurrPtr;

		/* Find the end of this token and the start of the next */
		CurrPtr  = CurrPtr + strcspn(CurrPtr, " ");
		if (*CurrPtr != '\0') {
			*CurrPtr = '\0';
			CurrPtr  += strspn(CurrPtr + 1, " ") + 1;
		}

		/* Figure out which device has been requested */
		for (i = 0;  i < NUM_CONSOLES;  ++i) {
			if (consoles[i] != NULL  &&
			    strcmp(Dest+1, consoles[i]->Name) == 0)
			{
				Console = consoles[i];
				break;
			}
		}
		if (Console == NULL) {
			ffscMsg("Unrecognized destination for MSG: \"%s\"",
				Dest);
			return;
		}
	}

	/* If no destination was specified, use the default */
	if (Console == NULL) {
		Console = DfltConsole;
	}

	/* Append a trailing CR to the message */
	sprintf(Output, "%s\r", CurrPtr);

	/* Say what we are about to do if desired */
	if (ffscDebug.Flags & FDBF_CONSOLE) {
		ffscMsg("Console %s sending \"%s\" to system",
			Console->Name, CurrPtr);
	}

	/* Send the output directly to SysOut */
	bufWrite(Console->SysOutBuf, Output, strlen(Output));

	return;
}


/*
 * crSetCtrlChar
 *	Changes a control character associated with the specified
 *	console. Returns non-zero if NVRAM should be updated, zero
 *	otherwise.
 */
int
crSetCtrlChar(console_t *Console, int Which, char *String, const char *Name)
{
	int Result;

	/* If the value is simply "?", then just print the current value */
	if (strcmp(String, "?") == 0) {
		char Message[80];
		char Setting[20];

		ffscCtrlCharToString(Console->Ctrl[Which], Setting);
		sprintf(Message,
			STR_END "Current setting of %s character is %s"
			    STR_END,
			Name, Setting);

		cmPrintResponse(Console, Message);

		Result = 0;
	}

	/* Otherwise, change the current setting */
	else {
		int Value;

		Value = atoi(String);
		Console->Ctrl[Which] = (char) Value;

		if (ffscDebug.Flags & FDBF_CONSOLE) {
			char Setting[20];

			ffscCtrlCharToString(Value, Setting);
			ffscMsg("Changed %s char to %s on %s",
				Name, Setting, Console->Name);
		}

		Result = 1;
	}

	return Result;
}


/*
 * crSetPagerChar
 *	Changes a pager character associated with the specified
 *	console. Returns non-zero if NVRAM should be updated, zero
 *	otherwise.
 */
int
crSetPagerChar(console_t *Console, int Which, char *String, const char *Name)
{
	int Result;

	/* If the value is simply "?", then just print the current value */
	if (strcmp(String, "?") == 0) {
		char Message[80];
		char Setting[20];

		ffscCtrlCharToString(Console->Pager[Which], Setting);
		sprintf(Message,
			STR_END "Current setting of %s character is %s"
			    STR_END,
			Name, Setting);

		cmPrintResponse(Console, Message);

		Result = 0;
	}

	/* Otherwise, change the current setting */
	else {
		int Value;

		Value = atoi(String);
		Console->Pager[Which] = (char) Value;

		if (ffscDebug.Flags & FDBF_CONSOLE) {
			char Setting[20];

			ffscCtrlCharToString(Value, Setting);
			ffscMsg("Changed %s char to %s on %s",
				Name, Setting, Console->Name);
		}

		Result = 1;
	}

	return Result;
}
