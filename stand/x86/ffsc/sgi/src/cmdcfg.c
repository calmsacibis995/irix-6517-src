/*
 * cmdcfg.c
 *	Command functions that deal with the MMSC configuration
 */

#include <vxworks.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ffsc.h"

#include "addrd.h"
#include "cmd.h"
#include "console.h"
#include "dest.h"
#include "elsc.h"
#include "misc.h"
#include "msg.h"
#include "nvram.h"
#include "route.h"
#include "tty.h"
#include "user.h"
#include "siolib.h"


/* Token tables */
#define COM_CMD		1
#define COM_FUNCTION	2
#define COM_OOB		3
#define COM_SPEED	4
#define COM_RXBUF	5
#define COM_TXBUF	6
#define COM_HWFLOW	7
static tokeninfo_t ComSubCmds[] = {
	{ "CMD",	COM_CMD },
	{ "FUNCTION",	COM_FUNCTION },
	{ "OOB",			COM_OOB },
	{ "RXBUF",		COM_RXBUF },
	{ "SPEED",		COM_SPEED },
	{ "TXBUF",		COM_TXBUF },
	{ "HWFLOW",		COM_HWFLOW },
	{ NULL,		0 }
};

static tokeninfo_t ComFunctions[] = {
	{ "TERMINAL",	PORT_TERMINAL },
	{ "TERM",	PORT_TERMINAL },
	{ "SYSTEM",	PORT_SYSTEM },
	{ "SYS",	PORT_SYSTEM },
	{ "UPPER",	PORT_ELSC_U },
	{ "ELSC_U",	PORT_ELSC_U },
	{ "LOWER",	PORT_ELSC_L },
	{ "ELSC_L",	PORT_ELSC_L },
	{ "ALTCONS",	PORT_ALTCONS },
	{ "SYSTEM2",    PORT_DEBUG},
	{ "SYS2",       PORT_DEBUG},
	{ "DEBUG",	PORT_DEBUG },
	{ "DAEMON",	PORT_DAEMON },
	{ NULL,		0 }
};


/*
 * cmdCOM
 *	Set/display communication parameters for an FFSC port
 */
int
cmdCOM(char **CmdP, user_t *User, dest_t *Dest)
{
	char *Ptr;
	char *SubCmdToken;
	char *Token;
	int  ComNum;
	int  Result;
	int  SubCmd;
	int  TTYNum;
	int  Value;
	/* prototype statement */
	SIO_CHAN * sysSerialChanGet(int);

	/* If our rack is not the selected one, tell the router */
	/* to pass the command along elsewhere			*/
	if (!destRackIsSelected(Dest, ffscLocal.rackid)) {
		return PFC_FORWARD;
	}

	/* Look for the next token */
	Token = strtok_r(NULL, " \t", CmdP);

	/* If no com port is present, that's an error */
	if (Token == NULL) {
		ffscMsg("No COM port specified");
		sprintf(Responses[ffscLocal.rackid]->Text, "ERROR ARG");
		return PFC_DONE;
	}

	/* Parse the COM port and make sure it is OK */
	ComNum = (int) strtol(Token, &Ptr, 0);
	if (*Ptr != '\0'  ||  ComNum < 1  ||  ComNum > FFSC_NUM_TTYS) {
		ffscMsg("Invalid COM port \"%s\"", Token);
		sprintf(Responses[ffscLocal.rackid]->Text, "ERROR ARG");
		return PFC_DONE;
	}
	TTYNum = ComNum - 1;

	/* Get the next token */
	SubCmdToken = strtok_r(NULL, " \t", CmdP);

	/* If there is no next token, just show the current settings */
	if (SubCmdToken == NULL) {
		char FuncName[20];
		const ttyinfo_t *Info;

		Info = comGetInfo(ComNum);

		ffscIntToToken(ComFunctions, Info->Port, FuncName, 20);
		sprintf(Responses[ffscLocal.rackid]->Text,
			"R%ld:OK COM %d speed %ld func %s cmd %c "
		  	"oob %c rxbuf %d txbuf %d hwflow %c",
			ffscLocal.rackid,
			ComNum,
			Info->BaudRate,
			FuncName,
			((Info->CtrlFlags & TTYCF_NOFFSC) ? 'N' : 'Y'),
			((Info->CtrlFlags & TTYCF_OOBOK) ? 'Y' : 'N'),
			comGetRxBuf(ComNum),
			comGetTxBuf(ComNum),
			((Info->CtrlFlags & TTYCF_HWFLOW) ? 'Y' : 'N'));
		Responses[ffscLocal.rackid]->State = RS_DONE;
		return PFC_FORWARD;
	}

	/* Make sure the user is authorized to change the COM settings */
	if (User->Authority < AUTH_SERVICE) {
		sprintf(Responses[ffscLocal.rackid]->Text, "ERROR AUTHORITY");
		return PFC_DONE;
	}

	/* Convert the token into a proper subcommand number */
	if (ffscTokenToInt(ComSubCmds, SubCmdToken, &SubCmd) != OK) {
		ffscMsg("Unknown subcommand \"%s\" for COM %d",
			SubCmdToken, ComNum);
		sprintf(Responses[ffscLocal.rackid]->Text, "ERROR ARG");
		return PFC_DONE;
	}

	/* Proceed according to the current subcommand */
	switch (SubCmd) {

	    /* Enable/disable FFSC command or OOB message processing */
	    case COM_CMD:
	    case COM_OOB:

		/* Get enable/disable token */
		Token = strtok_r(NULL, " \t", CmdP);
		if (Token == NULL) {
			ffscMsg("Must specify ON/OFF with COM %d %s",
				ComNum, SubCmdToken);
			sprintf(Responses[ffscLocal.rackid]->Text,
				"ERROR ARG");
			return PFC_DONE;
		}

		/* Convert the token to a value */
		if (ffscTokenToInt(ffscYesNoTokens, Token, &Value) != OK) {
			ffscMsg("Invalid token \"%s\" with COM %d %s",
				Token, ComNum, SubCmdToken);
			sprintf(Responses[ffscLocal.rackid]->Text,
				"ERROR ARG");
			return PFC_DONE;
		}

		/* Set/reset the appropriate flag */
		if (SubCmd == COM_CMD) {
			if (Value) {
				Result = comResetFlag(ComNum, TTYCF_NOFFSC);
			}
			else {
				Result = comSetFlag(ComNum, TTYCF_NOFFSC);
			}
		}
		else {
			if (Value) {
				Result = comSetFlag(ComNum, TTYCF_OOBOK);
			}
			else {
				Result = comResetFlag(ComNum, TTYCF_OOBOK);
			}
		}

		/* The response depends on whether or not we actually */
		/* updated an active console			      */
		if (Result == TTYR_OK) {
			sprintf(Responses[ffscLocal.rackid]->Text,
				"R%ld:OK",
				ffscLocal.rackid);
		}
		else {
			sprintf(Responses[ffscLocal.rackid]->Text,
				"R%ld:OK Change effective when COM%d has "
				    "function TERMINAL or SYSTEM at reset",
				ffscLocal.rackid, ComNum);
		}

		break;
		

	    /* Change the TTY's function */
	    case COM_FUNCTION:

		/* Extract the function name from the next token */
		Token = strtok_r(NULL, " \t", CmdP);
		if (Token == NULL) {
			ffscMsg("No function name on COM %d FUNCTION", ComNum);
			sprintf(Responses[ffscLocal.rackid]->Text,
				"ERROR ARG");
			return PFC_DONE;
		}

		/* Convert the function name to a port number */
		if (ffscTokenToInt(ComFunctions, Token, &Value) != OK) {
			ffscMsg("Invalid function \"%s\"", Token);
			sprintf(Responses[ffscLocal.rackid]->Text,
				"ERROR ARG");
			return PFC_DONE;
		}

		/* Go do the formal update */
		comSetFunction(ComNum, Value);

		/* Record successful completion */
		sprintf(Responses[ffscLocal.rackid]->Text,
			"R%ld:OK Change will not take effect until the MMSC "
			    "has been reset",
			ffscLocal.rackid);
		break;


	    /* Change the current baud rate */
	    case COM_SPEED:

		/* Extract the baud rate from the next token */
		Token = strtok_r(NULL, " \t", CmdP);
		if (Token == NULL) {
			ffscMsg("No baud rate on COM %d SPEED", ComNum);
			sprintf(Responses[ffscLocal.rackid]->Text,
				"ERROR ARG");
			return PFC_DONE;
		}

		Value = (int) strtol(Token, &Ptr, 0);

		/* Sasha V
		   8/11/98  Added checks for valid modem
		   speeds.  Without them user gets no feedback
		   if (s)he enters nonsense.
		   */

		if (*Ptr != '\0'  ||  
		    Value < 1     ||
		    ( Value != 300    &&
		      Value != 1200   &&
		      Value != 2400   &&
		      Value != 4800   &&
		      Value != 9600   &&
		      Value != 19200  &&
		      Value != 38400  &&
		      Value != 57600  &&
		      Value != 115200)) {
			ffscMsg("Invalid baud rate \"%s\"", Token);
			sprintf(Responses[ffscLocal.rackid]->Text,
				"ERROR ARG");
			return PFC_DONE;
		}

		/* Try changing it on the serial port */
		if (comSetSpeed(ComNum, Value) != OK) {
			/* Error message already printed by comSetSpeed */
			sprintf(Responses[ffscLocal.rackid]->Text,
				"ERROR ARG");
			return PFC_DONE;
		}

		/* Say what we've done if desired */
		if (ffscDebug.Flags & FDBF_CONSOLE) {
			ffscMsg("Changed baud rate of COM%d to %d",
				ComNum, Value);
		}

		/* Record successful completion */
		sprintf(Responses[ffscLocal.rackid]->Text,
			"R%ld:OK",
			ffscLocal.rackid);
		break;


	    /* Change the current receive or transmit buffer size */
	    case COM_RXBUF:
	    case COM_TXBUF:

		/* Extract the buffer size from the next token */
		Token = strtok_r(NULL, " \t", CmdP);
		if (Token == NULL) {
			ffscMsg("No buffer size on COM %d %s",
				ComNum, SubCmdToken);
			sprintf(Responses[ffscLocal.rackid]->Text,
				"ERROR ARG");
			return PFC_DONE;
		}

		/* Decode the token into an actual buffer size */
		ffscConvertToUpperCase(Token);
		if (strcmp(Token, "DEFAULT") == 0) {
			Value = 0;
		}
		else {
			Value = (int) strtol(Token, &Ptr, 0);
			if (*Ptr != '\0'  ||  Value < 0) {
				ffscMsg("Invalid buffer size \"%s\" on COM %d "
					    "%s",
					Token, ComNum, SubCmdToken);
				sprintf(Responses[ffscLocal.rackid]->Text,
					"ERROR ARG");
				return PFC_DONE;
			}
		}

		/* Engrave the change into stone */
		if (SubCmd == COM_RXBUF) {
			Result = comSetRxBuf(ComNum, Value);
		}
		else {
			Result = comSetTxBuf(ComNum, Value);
		}

		if (Result == TTYR_ERROR) {
			/* Error message already printed by comSetXxBuf */
			sprintf(Responses[ffscLocal.rackid]->Text,
				"ERROR ARG");
			return PFC_DONE;
		}

		/* Say what we've done if desired */
		if (ffscDebug.Flags & FDBF_CONSOLE) {
			char *Name;

			Name = (SubCmd == COM_RXBUF) ? "receive" : "transmit";
			ffscMsg("Changed %s buffer size of COM%d to %d",
				Name, ComNum, Value);
		}

		/* Record successful completion */
		sprintf(Responses[ffscLocal.rackid]->Text,
			"R%ld:OK Change will not take effect until the MMSC "
			    "has been reset",
			ffscLocal.rackid);
		break;

	    /* Enable/disable MMSC hardware flow control */
	    case COM_HWFLOW:

			/* Get enable/disable token */
			Token = strtok_r(NULL, " \t", CmdP);
			if (Token == NULL) {
				ffscMsg("Must specify ON/OFF with COM %d %s",
					ComNum, SubCmdToken);
				sprintf(Responses[ffscLocal.rackid]->Text,
					"ERROR ARG");
				return PFC_DONE;
			}

			/* Convert the token to a value */
			if (ffscTokenToInt(ffscYesNoTokens, Token, &Value) != OK) {
				ffscMsg("Invalid token \"%s\" with COM %d %s",
					Token, ComNum, SubCmdToken);
				sprintf(Responses[ffscLocal.rackid]->Text,
					"ERROR ARG");
				return PFC_DONE;
			}
	
			/* Say what we've done if desired */
			if (ffscDebug.Flags & FDBF_CONSOLE) {
				if (Value)
					ffscMsg("Enabled Hardware flow Control on COM%d", ComNum);
				else
					ffscMsg("Disabled Hardware flow Control on COM%d", ComNum);
			}
	
			/* Set/reset the appropriate flag */
			Result = comSetFlowCntl(ComNum-1, Value);
	 
			/* Restart the TTY Channel */
			sioTxStartup(sysSerialChanGet(ComNum-1));
	
			/* The response depends on whether or not we actually */
			/* updated an active console			      */
			if (Result == OK) {
				sprintf(Responses[ffscLocal.rackid]->Text,
					"R%ld:OK",
					ffscLocal.rackid);
			}
			else {
				sprintf(Responses[ffscLocal.rackid]->Text,
					"R%ld:ERROR INTERNAL",
					ffscLocal.rackid);
			}

		break;

		/* Unknown parameter */
	    default:
		ffscMsg("Unexpected subcommand %d for COM %d",
			SubCmd, ComNum);
		sprintf(Responses[ffscLocal.rackid]->Text,
			"ERROR NOTAVAILABLE");
		return PFC_DONE;
	}

	/* If we make it to here, everything was apparently successful */
	Responses[ffscLocal.rackid]->State = RS_DONE;

	return PFC_FORWARD;
}


/*
 * cmdNAP_TIME
 *	Show/change the current wakeup interval
 */
int
cmdNAP_TIME(char **CmdP, user_t *User, dest_t *Dest)
{
	char *Token;

	/* Look for the next token */
	Token = strtok_r(NULL, " \t", CmdP);

	/* If a token was found, change the wakeup time */
	if (Token != NULL) {
		int Value;

		/* Make sure the user is authorized to make changes */
		if (User->Authority < AUTH_SERVICE) {
			sprintf(Responses[ffscLocal.rackid]->Text,
				"ERROR AUTHORITY");
			return PFC_DONE;
		}

		/* Determine the new value */
		ffscConvertToUpperCase(Token);
		if (strcmp(Token, "DEFAULT") == 0) {
			Value = -1;
		}
		else {
			Value = strtoul(Token, NULL, 0);
			if (Value < 1) {
				ffscMsg("Invalid NAP_TIME: %s", Token);
				sprintf(Responses[ffscLocal.rackid]->Text,
					"ERROR ARG");
				return PFC_DONE;
			}
		}

		/* Tell the console to do the honors */
		sprintf(Responses[ffscLocal.rackid]->Text,
			"OK;STATE WU%d",
			Value);
	}

	/* Otherwise, say what the current wakeup time is */
	else {
		sprintf(Responses[ffscLocal.rackid]->Text, "OK;STATE WU?");
	}

	return PFC_DONE;
}


/*
 * cmdOPTIONS
 *	Show/change the current command options
 */
int
cmdOPTIONS(char **CmdP, user_t *User, dest_t *Dest)
{
	char *Token;

	/* Look for the next token */
	Token = strtok_r(NULL, " \t", CmdP);

	/* If a token was found, change the option flags */
	if (Token != NULL) {
		char *Ptr;
		int  Value;

		/* Make sure the user is authorized to make changes */
		if (User->Authority < AUTH_SERVICE) {
			sprintf(Responses[ffscLocal.rackid]->Text,
				"ERROR AUTHORITY");
			return PFC_DONE;
		}

		/* Parse the options settings */
		Value = strtol(Token, &Ptr, 0);
		if (Ptr == NULL  ||  Ptr[0] != '\0') {
			ffscMsg("Invalid argument to OPTIONS: \"%s\"", Token);
			sprintf(Responses[ffscLocal.rackid]->Text,
				"ERROR ARG");
			return PFC_DONE;
		}

		/* Set the new value and update NVRAM if necessary */
		User->Options = Value;
		userUpdateSaveInfo(User);
	}

	/* Say what the new flags are */
	sprintf(Responses[ffscLocal.rackid]->Text, "OK 0x%08x", User->Options);

	return PFC_DONE;
}


/*
 * cmdRACKID
 *	Show/set the rack ID of the addressed rack.
 */
int
cmdRACKID(char **CmdP, user_t *User, dest_t *Dest)
{
	char *Token;
	identity_t NewIdentity;
	int NumRacks;

	/* Look for the next token */
	Token = strtok_r(NULL, " \t", CmdP);

	/* If no new rack ID is present, then simple return the local one */
	if (Token == NULL) {
		if (destRackIsSelected(Dest, ffscLocal.rackid)) {
			char RackIDStr[RACKID_STRLEN];

			ffscRackIDToString(ffscLocal.rackid, RackIDStr);
			sprintf(Responses[ffscLocal.rackid]->Text,
				"R%ld:OK %s",
				ffscLocal.rackid, RackIDStr);
			Responses[ffscLocal.rackid]->State = RS_DONE;
		}

		return PFC_FORWARD;
	}

	/* Make sure the user is authorized to make changes */
	if (User->Authority < AUTH_SERVICE) {
		sprintf(Responses[ffscLocal.rackid]->Text,
			"ERROR AUTHORITY");
		return PFC_DONE;
	}

	/* Make sure only one rack is specified in the destination */
	NumRacks = destNumRacks(Dest, NULL);
	if (NumRacks > 1) {
		ffscMsg("Attempted to change %d rackid's with one command",
			NumRacks);
		sprintf(Responses[ffscLocal.rackid]->Text, "ERROR DEST");
		return PFC_DONE;
	}

	/* If our rack is not the selected one, tell the router */
	/* to pass the command along elsewhere			*/
	if (!destRackIsSelected(Dest, ffscLocal.rackid)) {
		return PFC_FORWARD;
	}

	/* Start with our original identity */
	NewIdentity = ffscLocal;

	/* Parse the new rack ID and make sure it is OK */

	/* Sasha 8/13/98  The correct way to do this is to
	   call the ffscParseRackID function, which does its
	   own error-checking.  */

	if (ffscParseRackID(Token, &(NewIdentity.rackid)) != OK) {
		sprintf(Responses[ffscLocal.rackid]->Text, "ERROR ARG");
		return PFC_DONE;
	}

	/* Ask the address daemon to reserve (and possibly steal) this	*/
	/* rack ID for us.						*/
	if (addrdRequestRackID(&NewIdentity) != OK) {
		ffscMsg("WARNING: Rack ID reservation failed - conflicts "
			    "may exist");
		sprintf(Responses[ffscLocal.rackid]->Text, "ERROR INTERNAL");
	}
	else {
		sprintf(Responses[ffscLocal.rackid]->Text,
			"OK %ld",
			ffscLocal.rackid);
	}

	return PFC_DONE;
}




/*
 * Scan for all partitions in a collection of racks.
 */
int cmdPARTSCAN(char **CmdP, user_t *User, dest_t *Dest)
{
  sprintf(Responses[ffscLocal.rackid]->Text,"OK:%ld",ffscLocal.rackid);
  broadcastPartitionInfo(&partInfo);
  Responses[ffscLocal.rackid]->State = RS_DONE;
  return PFC_FORWARD;
}


/*
 * cmdSCAN
 *	Scan for new or changed MSC's and module numbers.
 */
int
cmdSCAN(char **CmdP, user_t *User, dest_t *Dest)
{
	bayid_t	    CurrBay;
	int	    UpdateModuleNums = 0;
	modulenum_t ModuleNum;

	/* Make sure the user is authorized to make changes */
	if (User->Authority < AUTH_SERVICE) {
		sprintf(Responses[ffscLocal.rackid]->Text,
			"ERROR AUTHORITY");
		return PFC_DONE;
	}

	/* No need to proceed if our rack was not selected */
	if (!destRackIsSelected(Dest, ffscLocal.rackid)) {
		return PFC_FORWARD;
	}

	/* Examine each of the selected (or implied) bays */
	for (CurrBay = 0;  CurrBay < MAX_BAYS;  ++CurrBay) {

		/* If the current bay was not selected AND the destination */
		/* did not include 'BAY ALL', then go on to the next bay.  */
		if (!destLocalBayIsSelected(Dest, CurrBay)  &&
		    !destAllBaysSelected(Dest, ffscLocal.rackid))
		{
			continue;
		}

		/* If the current bay was thought to be offline, try a  */
		/* complete setup and see if it has sprung back to life */
		if (ELSC[CurrBay].Flags & EF_OFFLINE) {
			if (elscSetup(&ELSC[CurrBay]) != OK) {

				/* No such luck. Note that in the response. */
				sprintf(ELSC[CurrBay].OutAck->Text,
					"OFFLINE");
				ELSC[CurrBay].OutAck->State = RS_DONE;
				continue;
			}
		}

		/* Go see if the module number has changed */
		if (elscQueryModuleNum(CurrBay, &ModuleNum) == OK) {
			char ModNumStr[MODULENUM_STRLEN];

			/* If the module number changed, arrange to update */
			if (ModuleNum != ELSC[CurrBay].ModuleNum) {
				ELSC[CurrBay].ModuleNum = ModuleNum;
				UpdateModuleNums = 1;
			}

			/* Include the module number in the response */
			ffscModuleNumToString(ModuleNum, ModNumStr);
			sprintf(ELSC[CurrBay].OutAck->Text,
				"OK %s",
				ModNumStr);
			ELSC[CurrBay].OutAck->State = RS_DONE;
		}
		else {
			/* The MSC has apparently gone away */
			ELSC[CurrBay].ModuleNum = MODULENUM_UNASSIGNED;
			UpdateModuleNums = 1;

			sprintf(ELSC[CurrBay].OutAck->Text, "OFFLINE");
			ELSC[CurrBay].OutAck->State = RS_DONE;
		}
	}

	/* If any module numbers actually did change, arrange for the */
	/* proper authorities to be notified.			      */
	if (UpdateModuleNums) {
		elscUpdateModuleNums();
	}
   /* 
    * Rescan for partitions which may have come back up online.
    */
   broadcastPartitionInfo(&partInfo);

  return PFC_FORWARD;
}


/*
 * cmdSETENV
 *	Modify a tuneable variable
 */
int
cmdSETENV(char **CmdP, user_t *User, dest_t *Dest)
{
	char *Ptr;
	char *Token;
	char *ValString;
	int  Index;
	int  Value;

	/* If our rack is not the selected one, tell the router */
	/* to pass the command along elsewhere			*/
	if (!destRackIsSelected(Dest, ffscLocal.rackid)) {
		return PFC_FORWARD;
	}

	/* Look for the next token */
	Token = strtok_r(NULL, " \t", CmdP);

	/* If no variable name is present, that's an error */
	if (Token == NULL) {
		ffscMsg("No variable specified for SETENV");
		sprintf(Responses[ffscLocal.rackid]->Text, "ERROR ARG");
		return PFC_DONE;
	}

	/* See if the variable name and value are stuck together */
	ValString = strchr(Token, '=');
	if (ValString != NULL) {
		*ValString = '\0';
		++ValString;
	}

	/* Find the variable's index */
	Index = ffscTuneVarToIndex(Token);
	if (Index < 0) {
		ffscMsg("Unknown variable \"%s\"", Token);
		sprintf(Responses[ffscLocal.rackid]->Text, "ERROR ARG");
		return PFC_DONE;
	}

	/* Get the next token if necessary */
	if (ValString == NULL) {
		ValString = strtok_r(NULL, " \t", CmdP);
	}

	/* If there is another token, treat it as the new value */
	if (ValString != NULL) {

		/* Make sure the user is authorized to make changes */
		if (User->Authority < AUTH_SERVICE) {
			sprintf(Responses[ffscLocal.rackid]->Text,
				"ERROR AUTHORITY");
			return PFC_DONE;
		}

		/* Extract the new value from the next token */
		Value = (int) strtol(ValString, &Ptr, 0);
		if (*Ptr != '\0') {
			ffscMsg("Invalid value \"%s\"", ValString);
			sprintf(Responses[ffscLocal.rackid]->Text,
				"ERROR ARG");
			return PFC_DONE;
		}

		/* Change the variable's value */
		ffscTune[Index] = Value;

		/* Update NVRAM */
		(void) nvramWrite(NVRAM_TUNE, ffscTune);

		/* Say what we've done if desired */
		if (ffscDebug.Flags & FDBF_CONSOLE) {
			ffscMsg("Changed variable %s to %d",
				ffscTuneInfo[Index].Name, Value);
		}
	}

	/* If we make it to here, everything was apparently successful */
	sprintf(Responses[ffscLocal.rackid]->Text,
		"R%ld:OK %s = %d",
		ffscLocal.rackid, ffscTuneInfo[Index].Name, ffscTune[Index]);
	Responses[ffscLocal.rackid]->State = RS_DONE;

	return PFC_FORWARD;
}


/*
 * cmdUNSETENV
 *	Restore a tuneable variable to its default value
 */
int
cmdUNSETENV(char **CmdP, user_t *User, dest_t *Dest)
{
	char *Token;
	int  Index;

	/* Make sure the user is authorized to make changes */
	if (User->Authority < AUTH_SERVICE) {
		sprintf(Responses[ffscLocal.rackid]->Text,
			"ERROR AUTHORITY");
		return PFC_DONE;
	}

	/* If our rack is not the selected one, tell the router */
	/* to pass the command along elsewhere			*/
	if (!destRackIsSelected(Dest, ffscLocal.rackid)) {
		return PFC_FORWARD;
	}

	/* Look for the next token */
	Token = strtok_r(NULL, " \t", CmdP);

	/* If no variable name is present, that's an error */
	if (Token == NULL) {
		ffscMsg("No variable specified for SETENV");
		sprintf(Responses[ffscLocal.rackid]->Text, "ERROR ARG");
		return PFC_DONE;
	}

	/* Find the variable's index */
	Index = ffscTuneVarToIndex(Token);
	if (Index < 0) {
		ffscMsg("Unknown variable \"%s\"", Token);
		sprintf(Responses[ffscLocal.rackid]->Text, "ERROR ARG");
		return PFC_DONE;
	}

	/* Change the variable's value */
	ffscTune[Index] = ffscTuneInfo[Index].Default;

	/* Update NVRAM */
	(void) nvramWrite(NVRAM_TUNE, ffscTune);

	/* Say what we've done if desired */
	if (ffscDebug.Flags & FDBF_CONSOLE) {
		ffscMsg("Restore default value to variable %s",
			ffscTuneInfo[Index].Name);
	}

	/* If we make it to here, everything was apparently successful */
	sprintf(Responses[ffscLocal.rackid]->Text,
		"R%ld:OK",
		ffscLocal.rackid);
	Responses[ffscLocal.rackid]->State = RS_DONE;

	return PFC_FORWARD;
}
