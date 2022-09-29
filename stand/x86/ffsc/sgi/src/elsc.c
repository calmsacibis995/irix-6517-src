/*
 * elsc.c
 *	Functions for communicating with and controlling an ELSC
 */

#include <vxworks.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ffsc.h"

#include "addrd.h"
#include "elsc.h"
#include "log.h"
#include "misc.h"
#include "msg.h"
#include "nvram.h"
#include "route.h"
#include "user.h"
#include "console.h"

/* Global variables */
elsc_t ELSC[MAX_BAYS];		/* Descriptors for all local ELSCs */

extern void cmPrintMsg(console_t *Console, char *Msg); /* Console printf() routine */
extern console_t* getSystemConsole( void );            /* Returns system console */
extern STATUS ttyToggleHwflow(int fd);                 /* Toggles HWFC */
/*
 * elscCheckResponse
 *	Checks the specified string (assumed to be a response message
 *	from an ELSC) for any embedded commands. These will generally
 *	be any string that precedes an ELSC escape character. They
 *	will be processed as ordinary ELSC commands and the input string
 *	will be modified to remove them.
 */
void
elscCheckResponse(elsc_t *EInfo, char *Response)
{
	char *CurrPtr = Response;
	char *Last;
	char *NextBegin;

	/* Loop until no more escape characters are encountered */
	NextBegin = strchr(CurrPtr, ELSC_MSG_BEGIN_CHAR);
	while (NextBegin != NULL) {

		/* Terminate the current command */
		*NextBegin = '\0';
		Last = strchr(CurrPtr, FFSC_ACK_END_CHAR);
		if (Last != NULL) {
			*Last = '\0';
		}

		/* Print a message if desired */
		if (ffscDebug.Flags & FDBF_ELSC) {
			ffscMsg("Received piggybacked ELSC command \"%s\"",
				CurrPtr);
		}

		/* Go process it. The return response is ignored. */
		(void) elscProcessCommand(EInfo, CurrPtr);

		/* Look for the next command-end */
		CurrPtr = NextBegin + 1;
		NextBegin = strchr(CurrPtr, ELSC_MSG_BEGIN_CHAR);
	}

	/* If we actually processed one or more commands, delete */
	/* them from the string					 */
	if (CurrPtr != Response) {
		memmove(Response, CurrPtr, strlen(CurrPtr));
	}
}


/*
 * elscDoAdminCommand
 *	Sends an administrative command (i.e. one intended for the FFSC's
 *	own purposes, not one requested by a user) to an ELSC and waits
 *	for a response. The response is copied to the specified response
 *	buffer if it is not NULL. Returns OK if the response buffer
 *	has a valid response (or would if one were specified), ERROR if not.
 *	Note that no check is made to determine if the caller actually
 *	owns the ELSC - misuse could cause garbled input or output.
 */
STATUS
elscDoAdminCommand(bayid_t Bay, const char *Command, char *Response)
{
	char *Last;

	/* Make sure we have an open connection to the ELSC */
	if (ELSC[Bay].FD < 0) {
		ffscMsg("Tried to send admin command to offline MSC");
		return ERROR;
	}

	/* Prepare the outgoing msg buffer. Note that we assume we own it. */
	msgClear(ELSC[Bay].OutMsg);
	sprintf(ELSC[Bay].OutMsg->Text,
		ELSC_CMD_BEGIN "%s" ELSC_CMD_END,
		Command);
	ELSC[Bay].OutMsg->State = RS_SEND;

	/* Say what we are about to do if desired */
	if (ffscDebug.Flags & FDBF_ELSC) {
		ffscMsg("About to send admin command \"%s\" to local MSC %c",
			Command, ELSC[Bay].Name);

                if (ELSC[Bay].Log != NULL) {
                        char Message[80];

                        sprintf(Message, "<<ADMIN COMMAND: %s>>", Command);
                        logWriteLine(ELSC[Bay].Log, Message);
                }
        }

	/* Write the command to the Term task */
	if (msgWrite(ELSC[Bay].OutMsg, ffscTune[TUNE_RO_ELSC]) != OK) {
		/* msgWrite should have logged the error already */
		if (Response != NULL) {
			sprintf(Response, "ERROR SEND");
		}
		return ERROR;
	}

	/* Make sure everything got written */
	if (ELSC[Bay].OutMsg->State != RS_DONE) {
		ffscMsg("Unable to send admin command to MSC %c - State=%d",
			ELSC[Bay].Name, ELSC[Bay].OutMsg->State);
		if (Response != NULL) {
			sprintf(Response, "ERROR SEND");
		}
		return ERROR;
	}

	/* Prepare for a response from the ELSC */
	msgClear(ELSC[Bay].OutAck);
	ELSC[Bay].OutAck->State = RS_NEED_ELSCACK;

	/* If the command is not finished, wait for a response */
	if (msgRead(ELSC[Bay].OutAck, ffscTune[TUNE_RI_ELSC_RESP]) != OK) {
		return ERROR;
	}

	/* If the command did not complete, complain */
	if (ELSC[Bay].OutAck->State != RS_DONE) {
		ffscMsg("Admin command to MSC failed in state %d",
			ELSC[Bay].OutAck->State);
		return ERROR;
	}

	/* Remote the trailing CR/LF for neatness */
	Last = strpbrk(ELSC[Bay].OutAck->Text, ELSC_MSG_END);
	if (Last != NULL) {
		*Last = '\0';
	}

	/* Show the response if desired */
        if (ffscDebug.Flags & FDBF_ELSC) {
		ffscMsg("Response to admin command to MSC %c: \"%s\"",
			ffscBayName[Bay], ELSC[Bay].OutAck->Text);

                if (ELSC[Bay].Log != NULL) {
                        char Message[80];

                        sprintf(Message,
                                "<<ADMIN RESPONSE: %s>>",
                                ELSC[Bay].OutAck->Text);
                        logWriteLine(ELSC[Bay].Log, Message);
                }
        }

	/* Copy the response to the caller's buffer if desired */
	if (Response != NULL) {
		strcpy(Response, ELSC[Bay].OutAck->Text);
	}

	return OK;
}


/*
 * elscInit
 *	Initializes the ELSC info structures
 */
STATUS
elscInit(void)
{
	bayid_t     CurrBay;
	char        MsgName[MI_NAME_LEN];
	elsc_save_t SaveInfo[MAX_BAYS];
	int	    ELSCfds[MAX_BAYS];
	int	    UpdateModuleNums = 0;
	modulenum_t ModuleNum;

	/* For convenience, set up an array containing the FDs for the ELSCs */
	ELSCfds[0] = portELSC_U;
	ELSCfds[1] = portELSC_L;

	/* Go read the NVRAM info for the ELSC (if available) */
	if (nvramRead(NVRAM_ELSCINFO, SaveInfo) != OK) {

		/* Info is not available in NVRAM, so build */
		/* some default debugging info.		    */
		bzero((char *) SaveInfo, MAX_BAYS * sizeof(elsc_save_t));
		for (CurrBay = 0;  CurrBay < MAX_BAYS;  ++CurrBay) {
			SaveInfo[CurrBay].EMsgRack    = ELSC_DFLT_RACK;
			SaveInfo[CurrBay].EMsgAltRack = ELSC_DFLT_ALTRACK;
			strncpy(SaveInfo[CurrBay].Password,
				ELSC_DFLT_PASSWORD,
				ELSC_PASSWORD_LEN);
			SaveInfo[CurrBay].Flags = ESF_ALTRACK | ESF_PASSWORD;
		}

		/* Try to write the info out (but ignore */
		/* any errors that may occur)		 */
		nvramWrite(NVRAM_ELSCINFO, SaveInfo);
	}

	/* Set up an elsc_t for each possible local bay */
	for (CurrBay = 0;  CurrBay < MAX_BAYS;  ++CurrBay) {

		/* Initialize the easy parts */
		ELSC[CurrBay].Flags	= 0;
		ELSC[CurrBay].Owner	= EO_NONE;
		ELSC[CurrBay].FD	= ELSCfds[CurrBay];
		ELSC[CurrBay].Bay	= CurrBay;
		ELSC[CurrBay].ModuleNum = MODULENUM_UNASSIGNED;
		ELSC[CurrBay].EMsgRack	= SaveInfo[CurrBay].EMsgRack;
		ELSC[CurrBay].Name	= ffscBayName[CurrBay];
		ELSC[CurrBay].Log	= ELSCLogs[CurrBay];
		bzero((char *) ELSC[CurrBay].DispData, ELSC_DSP_LENGTH);

		/* Only set the ALTERNATE rack if it is valid in the */
		/* saved info (older FFSCs don't define that field)  */
		if (SaveInfo[CurrBay].Flags & ESF_ALTRACK) {
			ELSC[CurrBay].EMsgAltRack =
			    SaveInfo[CurrBay].EMsgAltRack;
		}
		else {
			ELSC[CurrBay].EMsgAltRack = ELSC_DFLT_ALTRACK;
		}

		/* Set the password, using the default if we don't */
		/* have a value in NVRAM			   */
		if (SaveInfo[CurrBay].Flags & ESF_PASSWORD) {
			strncpy(ELSC[CurrBay].Password,
				SaveInfo[CurrBay].Password,
				ELSC_PASSWORD_LEN);
			ELSC[CurrBay].Password[ELSC_PASSWORD_LEN] = '\0';
		}
		else {
			strcpy(ELSC[CurrBay].Password, ELSC_DFLT_PASSWORD);
		}

		/* Assuming that we have a valid ELSC FD, make the */
		/* router the official owner for now.		   */
		if (ELSC[CurrBay].FD >= 0) {
			ELSC[CurrBay].Owner = EO_ROUTER;
		}

		/* Set up msginfo_t's for incoming & outgoing messages */
		sprintf(MsgName, "MSC%ld", CurrBay);
		ELSC[CurrBay].InMsg = msgNew(MAX_ELSC_MSG_LEN,
					     ELSCfds[CurrBay],
					     RT_ELSC,
					     MsgName);
		if (ELSC[CurrBay].InMsg == NULL) {
			return ERROR;
		}

		sprintf(MsgName, "R2E%cr", ELSC[CurrBay].Name);
		ELSC[CurrBay].OutMsg = msgNew(MAX_ELSC_CMD_LEN,
					      ELSCfds[CurrBay],
					      RT_ELSC,
					      MsgName);
		if (ELSC[CurrBay].OutMsg == NULL) {
			return ERROR;
		}

		sprintf(MsgName, "R2E%ca", ELSC[CurrBay].Name);
		ELSC[CurrBay].OutAck = msgNew(MAX_ELSC_RESP_LEN,
					      ELSCfds[CurrBay],
					      RT_ELSC,
					      MsgName);
		if (ELSC[CurrBay].OutAck == NULL) {
			return ERROR;
		}

		/* Go initialize the ELSC itself */
		if (elscSetup(&ELSC[CurrBay]) != OK) {
			continue;
		}

		/* Go update the current module number */
		if (elscQueryModuleNum(CurrBay, &ModuleNum) == OK  &&
		    ModuleNum != ELSC[CurrBay].ModuleNum)
		{
			ELSC[CurrBay].ModuleNum = ModuleNum;
			UpdateModuleNums = 1;
		}
	}

	/* Go update new module numbers if necessary */
	if (UpdateModuleNums) {
		elscUpdateModuleNums();
	}

	return OK;
}


/*
 * elscPrintMsg
 *	Prints an unsolicited ELSC message on the consoles Officially
 *	Designated to receive them for the specified ELSC. The message
 *	itself is composed from the given header string and text.
 */
STATUS
elscPrintMsg(elsc_t *EInfo, char *Header, char *Text)
{
	/* Log the message in this ELSC's message log */
	if (EInfo->Log != NULL) {
		logWriteLine(EInfo->Log, Text);
	}

	/* Notify the TERMINAL console if it is on this FFSC */
	if (EInfo->EMsgRack == ffscLocal.rackid) {
		char Command[MAX_CONS_CMD_LEN  + 1];

		sprintf(Command, "MSG %s:%s", Header, Text);
		if (userSendCommand(userTerminal, Command, NULL) != OK) {
			ffscMsg("Text:  %s", Command);
			return ERROR;
		}
	}

	/* Otherwise, we need to send the message off to another rack */
	else if (EInfo->EMsgRack != RACKID_UNASSIGNED) {
		char NewCmd[MAX_FFSC_CMD_LEN + 1];

		sprintf(NewCmd,
			"R %ld MMSG TEXT %s:%s",
			EInfo->EMsgRack, Header, Text);
		routeDoAdminCommand(NewCmd);
	}

	/* Now we repeat all of this, but for the ALTERNATE console */
	if (EInfo->EMsgAltRack == ffscLocal.rackid) {
		char Command[MAX_CONS_CMD_LEN + 1];

		sprintf(Command, "MSG %s:%s", Header, Text);
		if (userSendCommand(userAlternate, Command, NULL) != OK) {
			return ERROR;
		}
	}

	/* Otherwise, we need to send the message off to another rack */
	else if (EInfo->EMsgAltRack != RACKID_UNASSIGNED) {
		char NewCmd[MAX_FFSC_CMD_LEN + 1];

		sprintf(NewCmd,
			"R %ld MMSG ALTTEXT %s:%s",
			EInfo->EMsgAltRack, Header, Text);
		routeDoAdminCommand(NewCmd);
	}

	return OK;
}


/*
 * elscProcessCommand
 *	Process an incoming command FROM the ELSC (*not* a command TO the
 *	ELSC). Returns OK/ERROR.
 */
STATUS
elscProcessCommand(elsc_t *EInfo, char *Command)
{
        
	char *Last;
	char *Token;

	/* Log the command text if desired */
	if (ffscDebug.Flags & FDBF_ELSC) {
		ffscMsg("Command from MSC %c: \"%s\"",
			EInfo->Name, Command);
	}

	/* Extract the command name */
	Last = NULL;
	Token = strtok_r(Command, " \t", &Last);
	if (Token == NULL) {
		ffscMsg("Empty command from MSC %c", EInfo->Name);
		return ERROR;
	}
	ffscConvertToUpperCase(Token);

	/* Examine the message from the ELSC and process it accordingly */
	if (strcmp(Token, "DSP") == 0) {

	  /* ELSC has updated its display */
	  elscUpdateDisplay(EInfo, Last);
	  /* Add partition information (if it is in this string) */

	  /* 3/19/99 Sasha
	     This might be problematic, as all kinds of strange things
	     come from the MSC.  So I'll disable it for now. */
	  /*	  update_partition_info(&partInfo, Last); */

	}
	else if (strcmp(Token, "DP") == 0) {
	  /* MAJOR KLUDGE: We don't know why, but sometimes the */
	  /* ELSC "dsp" command comes across as "dp" - this is  */
	  /* temporary workaround.			      */
	  
	  ffscMsg("Warning: received \"dp\" command from ELSC %c",
		  EInfo->Name);
	  elscUpdateDisplay(EInfo, Last);
	  /* Add partition information locally (if it is in this string ) */
	  /* 3/19/99 Sasha
	     This might be problematic, as all kinds of strange things
	     come from the MSC.  So I'll disable it for now. */
	  /*	  update_partition_info(&partInfo, Last); */

	}
	else if (strncmp(Token, "ELSC", 4) == 0  ||
		 strncmp(Token, "MSC", 3) == 0)
	{
		/* ELSC asks "Are you an MMSC?". We respond by doing */
		/* our standard ELSC setup (e.g. "fsc 1", "ech 0").  */
		modulenum_t ModuleNum;

		/* Log the fact that the ELSC is coming online */
		if (EInfo->Log != NULL) {
			char Message[80];

			sprintf(Message,
				"<<MSC %c COMING ONLINE>>",
				EInfo->Name);
			logWriteLine(EInfo->Log, Message);
		}


		/* Go do the formal setup and check for a module number */
		if (elscSetup(EInfo) == OK  &&
		    elscQueryModuleNum(EInfo->Bay, &ModuleNum) == OK  &&
		    ModuleNum != EInfo->ModuleNum)
		{
			EInfo->ModuleNum = ModuleNum;
			elscUpdateModuleNums();
		}
		
	}
	else if (strcmp(Token, "MSG") == 0) {

		/* Message from ELSC */

		char MsgHdr[15];

		sprintf(MsgHdr,
			"E%ld%c",
			ffscLocal.rackid, EInfo->Name);
		elscPrintMsg(EInfo, MsgHdr, Last);
	}
	else {
		/* Don't recognize this command */
		char Message[256];

		sprintf(Message,
			"<<UNRECOGNIZED COMMAND FROM MSC %c: %s>>",
			EInfo->Name, Command);

		if (EInfo->Log != NULL) {
			logWriteLine(EInfo->Log, Message);
		}

		ffscMsg(Message);

		return ERROR;
	}

	return OK;
}


/*
 * elscProcessInput
 *	Process an unsolicited message from the specified ELSC. Returns
 *	ERROR if the FD is no longer valid for input, OK otherwise.
 */
STATUS
elscProcessInput(elsc_t *EInfo)
{
	/* The ELSC better be owned by the router, or else something's wrong */
	if (EInfo->Owner != EO_ROUTER) {
		ffscMsg("elscProcessInput while MSC owned by %d",
			EInfo->Owner);
		return ERROR;
	}

	/* Prepare the msginfo_t for a new message */
	msgClear(EInfo->InMsg);
	EInfo->InMsg->State = RS_NEED_ELSCMSG;

	/* Go read a message from the ELSC */
	if (msgRead(EInfo->InMsg, ffscTune[TUNE_RI_ELSC_MSG]) != OK) {
		/* msgRead already logged the error */
		return ERROR;
	}

	/* Make sure we got a complete message */
	if (EInfo->InMsg->State == RS_ERROR) {
		ffscMsg("Invalid message from %s: %s",
			EInfo->InMsg->Name, EInfo->InMsg->Text);
		return OK;
	}

	/* Remove trailing CR/LF if present */
	if (EInfo->InMsg->State == RS_DONE) {
		EInfo->InMsg->CurrPtr -= 2;
		*EInfo->InMsg->CurrPtr = '\0';
	}
	else if (EInfo->InMsg->State == RS_NEED_ELSCEND2) {
		--EInfo->InMsg->CurrPtr;
		*EInfo->InMsg->CurrPtr = '\0';
	}
	else if (EInfo->InMsg->CurrPtr == EInfo->InMsg->Text) {
		ffscMsg("Empty message from R%ld%c",
			ffscLocal.rackid, EInfo->Name);
		return OK;
	}

	/* If the message doesn't start with an intro character */
	/* then it is just a hub message			*/
	if (EInfo->InMsg->Text[0] != ELSC_MSG_BEGIN_CHAR) {

		/* Message from hub */
		char MsgHdr[10];
		int  MsgLen = EInfo->InMsg->CurrPtr - EInfo->InMsg->Text;

		sprintf(MsgHdr, "R%ld%c", ffscLocal.rackid, EInfo->Name);
#ifdef NOTNOW
		if (ffscDebug.Flags & FDBF_ELSC) {
			ffscMsg("Hub message from %s, length %d: \"%s\"",
				MsgHdr, MsgLen, EInfo->InMsg->Text);
		}
#endif

		elscPrintMsg(EInfo, MsgHdr, EInfo->InMsg->Text);

		return OK;
	}

	/* Otherwise, make sure the message is complete */
	if (EInfo->InMsg->State != RS_DONE) {
		ffscMsg("Incomplete message from %s: %s",
			EInfo->InMsg->Name, EInfo->InMsg->Text);
		return OK;
	}

	/* Go process the command itself */
	(void) elscProcessCommand(EInfo, &EInfo->InMsg->Text[1]);

	return OK;
}


/*
 * elscQueryModuleNum
 *	Ask the ELSC in the specified bay for its module number and store
 *	it in the specified modulenum_t. Returns OK/ERROR (if ERROR, the
 *	modulenum_t will be set to MODULENUM_UNASSIGNED).
 */
STATUS
elscQueryModuleNum(bayid_t Bay, modulenum_t *ModuleNumP)
{
	char *ModNumStr;
	char *Ptr;
	char Response[MAX_ELSC_RESP_LEN + 1];
	int  ModuleNum;

	/* Assume failure at first */
	if (ModuleNumP != NULL) {
		*ModuleNumP = MODULENUM_UNASSIGNED;
	}

	/* Go ask the ELSC for its module number */
	if (elscDoAdminCommand(Bay, "mod", Response) != OK) {
                ffscMsg("Unable to contact MSC %c for module number",
                        ELSC[Bay].Name);
		ELSC[Bay].Flags |= EF_OFFLINE;
		return ERROR;
	}

	/* Make sure we got a valid response */
	if (strncmp(Response, ELSC_RESP_OK, ELSC_RESP_OK_LEN) != 0) {
		ffscMsg("MSC %c rejected 'mod' command: %s",
			ELSC[Bay].Name, Response);
		return OK;	/* Not a fatal error */
	}

	/* Attempt to extract the module number from the response */
	ModNumStr = Response + ELSC_RESP_OK_LEN + 1;
	ModuleNum = (int) strtol(ModNumStr, &Ptr, 16);
	if (*Ptr != '\0'  ||  ModuleNum < 0  ||  ModuleNum > 255) {
		ffscMsg("Invalid module number from ELSC %c: \"%s\"",
			ELSC[Bay].Name, ModNumStr);
		return OK;	/* Not a fatal error */
	}

	/* If the module number is "0", that really means "unassigned" */
	/* even though the module itself is online.		       */
	if (ModuleNum == 0) {
		ModuleNum = MODULENUM_UNASSIGNED;
	}

	/* Store the resulting value */
	if (ModuleNumP != NULL) {
		*ModuleNumP = ModuleNum;
	}

	return OK;
}


/*
 * elscSendCommand
 *	Sends the specified command to the specified ELSC on the
 *	local rack, and prepares (but does not actually wait) for
 *	a response.
 */
STATUS
elscSendCommand(bayid_t Bay, const char *Command)
{
	/* Prepare the response msg buffer */
	msgClear(ELSC[Bay].OutAck);

	/* We cannot proceed if the ELSC is owned by something other than */
	/* the router. Luckily, the router owns ELSC.OutAck, so we can at */
	/* least send back a reply.					  */
	if (ELSC[Bay].Owner != EO_ROUTER) {
		sprintf(ELSC[Bay].OutAck->Text, "ERROR INUSE");
		ELSC[Bay].OutAck->State = RS_DONE;
		return OK;
	}

	/* Prepare the outgoing msg buffer */
	msgClear(ELSC[Bay].OutMsg);
	sprintf(ELSC[Bay].OutMsg->Text,
		ELSC_CMD_BEGIN "%s" ELSC_CMD_END,
		Command);
	ELSC[Bay].OutMsg->State = RS_SEND;

	/* Say what we are about to do if desired */
	if (ffscDebug.Flags & FDBF_ELSC) {
		ffscMsg("About to send \"%s\" to local MSC %c",
			Command, ELSC[Bay].Name);
	}

	/* Write the command to the Term task */
	if (msgWrite(ELSC[Bay].OutMsg, ffscTune[TUNE_RO_ELSC]) != OK) {
		/* msgWrite should have logged the error already */
		strcpy(ELSC[Bay].OutAck->Text, "ERROR SEND");
		ELSC[Bay].OutAck->State = RS_DONE;
		return OK;
	}

	/* Make sure everything got written */
	if (ELSC[Bay].OutMsg->State != RS_DONE) {
		ffscMsg("Unable to send message to MSC %c - State=%d",
			ELSC[Bay].Name, ELSC[Bay].OutMsg->State);
		strcpy(ELSC[Bay].OutAck->Text, "ERROR SEND");
		ELSC[Bay].OutAck->State = RS_DONE;
		return OK;
	}

	/* Prepare for a response from the ELSC */
	ELSC[Bay].OutAck->State = RS_NEED_ELSCACK;

	/* Log our efforts */
	if (ELSC[Bay].Log != NULL) {
		char Message[MAX_ELSC_CMD_LEN + 40];

		sprintf(Message, "<<COMMAND:  %s>>", Command);
		logWriteLine(ELSC[Bay].Log, Message);
	}

	return OK;
}


/*
 * elscSendPassword
 *	Sends an ELSC "pas" command appropriate for the specified
 *	authority level to the ELSC in the specified bay. "Appropriate"
 *	means "pas" with no password in the case were Basic authority
 *	is specified (effectively revoking password privileges), and
 *	"pas <pw>" where <pw> is the ELSC's current password in the case
 *	where Supervisor or better authority is specified. Returns OK/ERROR.
 */
STATUS
elscSendPassword(bayid_t Bay, int AuthLevel)
{
	char Command[MAX_ELSC_CMD_LEN + 1];
	char Response[MAX_ELSC_RESP_LEN + 1];

	/* If the ELSC has no password, we can skip all of this */
	if (ELSC[Bay].Password[0] == '\0') {
		return OK;
	}

	/* Set up the appropriate "pas" command */
	if (AuthLevel < AUTH_SUPERVISOR) {
		sprintf(Command, "pas");
	}
	else {
		sprintf(Command, "pas %s", ELSC[Bay].Password);
	}

	/* Ship the command off to the ELSC */
	if (elscDoAdminCommand(Bay, Command, Response) != OK) {
		/* elscDoAdminCommand should have already complained */
		return ERROR;
	}

	/* Make sure the command was successful */
	if (strncmp(Response, ELSC_RESP_OK, ELSC_RESP_OK_LEN) != 0) {
		ffscMsg("Attempt to set ELSC password failed: %s", Response);
		return ERROR;
	}

	return OK;
}


/*
 * elscSetPassword
 *	Sets up a new password for the ELSC in the specified bay.
 *	If Propagate is non-zero, then the password will be set on
 *	the actual ELSC before saving it in NVRAM. The OutAck
 *	message buffer of the ELSC will be updated accordingly.
 */
STATUS
elscSetPassword(bayid_t Bay, const char *NewPassword, int Propagate)
{
	/* Prepare the response msg buffer */
	msgClear(ELSC[Bay].OutAck);

	/* If asked, first set the new password on the ELSC itself */
	/* (unless the password is being unset - the ELSC doesn't  */
	/* have that notion)					   */
	if (Propagate  &&  NewPassword != NULL  &&  NewPassword[0] != '\0') {
		char Command[MAX_ELSC_CMD_LEN + 1];

		/* We cannot proceed if the ELSC is owned by something */
		/* other than the router.			       */
		if (ELSC[Bay].Owner != EO_ROUTER) {
			sprintf(ELSC[Bay].OutAck->Text, "ERROR INUSE");
			ELSC[Bay].OutAck->State = RS_DONE;
			return ERROR;
		}

		/* Send the current password first */
		if (elscSendPassword(Bay, AUTH_SERVICE) != OK) {
			/* elscSendPassword has already set OutAck */
			return ERROR;
		}

		/* Set up the appropriate password command */
		sprintf(Command, "pas s %.4s", NewPassword);

		/* Send the command off to the ELSC */
		if (elscDoAdminCommand(Bay, Command, NULL) != OK) {
			/* elscDoAdminCommand has already set OutAck */
			return ERROR;
		}

		/* Make sure the response was positive */
		if (strcmp(ELSC[Bay].OutAck->Text, ELSC_RESP_OK) != 0) {
			/* elscDoAminCommand has already set OutAck */
			return ERROR;
		}

		/* Clean up the response msg buffer again */
		msgClear(ELSC[Bay].OutAck);
	}

	/* Update the appropriate locations with the new password */
	strncpy(ELSC[Bay].Password, NewPassword, ELSC_PASSWORD_LEN);
	elscUpdateSaveInfo();

	/* Successful completion */
	sprintf(ELSC[Bay].OutAck->Text, "OK");
	ELSC[Bay].OutAck->State = RS_DONE;

	return OK;
}



/*
 * elscSetup
 *	Initialize an ELSC from the FFSC perspective: inform it that
 *	it is indeed attached to an FFSC, turn off echoing, find its
 *	module number, etc. Returns <0 if something went wrong, 0 if
 *	nothing has changed, >0 if a new module number has been set.
 */
int
elscSetup(elsc_t *EInfo)
{
        char msgBuf[60];
	char Response[MAX_ELSC_RESP_LEN + 1];

	/* Print an informative introductory message if desired */
	if (ffscDebug.Flags & FDBF_INIT) {
		ffscMsg("Initializing MSC %c", EInfo->Name);
	}

	/* Tell the ELSC that it is attached to an FFSC */
	if (elscDoAdminCommand(EInfo->Bay, "fsc 1", Response) != OK) {


#ifdef ADAPTIVE_HWFC

	  /* 
	   * The remote command failed; this could be for one of two
	   * reasons:
	   * a) The device is unplugged
	   * b) The hwflow is incorrect.
	   * 
	   * For case b), we toggle the current settings and attempt 
	   * the command a second time. If this fails, we revert settings
	   * and mark the unit offline. Otherwise, we leave the new settings
	   * as they are, and store the new TTYinfo into NVRAM.
	   */
	  if(OK != ttyToggleHwflow(EInfo->FD)){
	    ffscMsg("Unable to toggle HWFC for MSC %c - marking offline",
		    EInfo->Name);
	    EInfo->Flags |= EF_OFFLINE;
	    return ERROR;
	  }
	  ffscMsg("HWFC changed for MSC %c -ok",EInfo->Name);

	  /* 
	   * OK, HWFC is toggled, attempt the transaction again.
	   */
	  if(elscDoAdminCommand(EInfo->Bay, "fsc 1", Response) != OK){

	    /*
	     * FAIL: second time. The device is probably disconnected.
	     * Revert HWFC settings and mark offline.
	     */
	    ffscMsg("Second contact attempt failed (HWFC mod) for MSC %c",
		    EInfo->Name);
	  
	    if(OK != ttyToggleHwflow(EInfo->FD)){
	      ffscMsg("Unable to revert HWFC for MSC %c - marking offline",
		      EInfo->Name);
	      EInfo->Flags |= EF_OFFLINE;
	      return ERROR;
	    }
	    ffscMsg("HWFC on MSC %c reverted;contact failed- marking offline",
		      EInfo->Name);

	    EInfo->Flags |= EF_OFFLINE;
	    return ERROR;
	  }
	  /*
	   * Log adaptation message to the console.
	   */
	  if(getSystemConsole() != NULL){
	    sprintf(msgBuf,
		    "NOTICE: MMSC adapted hardware flow control for MSC %c",
		    EInfo->Name);
	    cmPrintMsg(getSystemConsole(),msgBuf);
	  }

#else
	  EInfo->Flags |= EF_OFFLINE;
	  return ERROR;
	    
#endif
	}
	
	
	if (strncmp(Response, ELSC_RESP_OK, ELSC_RESP_OK_LEN) != 0) {
		ffscMsg("MSC %c rejected 'fsc 1' command: %s",
			EInfo->Name, Response);
		EInfo->Flags |= EF_OFFLINE;
		return ERROR;
	}

	/* Switch the ELSC into a mode we like, but only if we are */
	/* the legitimate owner.				   */
	if (EInfo->Owner == EO_ROUTER) {

		/* Advise the ELSC to be quiet, if only to keep */
		/* the logs a little less cluttered.	        */
		elscDoAdminCommand(EInfo->Bay, "ech 0", NULL);
	}

	/* If we make it this far, we can safely assume the ELSC is OK */
	EInfo->Flags &= ~EF_OFFLINE;
	ffscMsg("elscSetup about to return OK");
	return OK;
}


/*
 * elscUpdateDisplay
 *	Update our internal records of what the ELSC "should" be displaying
 */
void
elscUpdateDisplay(elsc_t *EInfo, char *String)
{
	/* Save the info for posterity */
	bzero((char *) EInfo->DispData, ELSC_DSP_LENGTH);
	if (String != NULL) {
		strncpy(EInfo->DispData, String, ELSC_DSP_LENGTH);
	}

	/* This would be a good place to update the MMSC display */

	/* Log this for posterity too */
	if (EInfo->Log != NULL) {
		char Message[80];

		sprintf(Message,
			"<<DISPLAY CHANGE: message=\"%.8s\" code=%d>>",
			EInfo->DispData,
			EInfo->DispData[ELSC_DSP_CODE_INDEX]);
		logWriteLine(EInfo->Log, Message);
	}
}


/*
 * elscUpdateModuleNums
 *	If the current module number assignments according to the ELSCs
 *	are different than the ones in the local identity info, update
 *	our records and notify the world. Returns OK/ERROR.
 */
STATUS
elscUpdateModuleNums(void)
{
	bayid_t    CurrBay;
	identity_t NewIdent;

	/* Generate a new identity_t for ourself from ffscLocal and the */
	/* current module numbers.					*/
	NewIdent = ffscLocal;
	for (CurrBay = 0;  CurrBay < MAX_BAYS;  ++CurrBay) {
		NewIdent.modulenums[CurrBay] = ELSC[CurrBay].ModuleNum;
	}

	/* Go update The World as needed */
	return addrdUpdateLocalIdentity(&NewIdent);
}


/*
 * elscUpdateSaveInfo
 *	Update our NVRAM data with current settings
 */
STATUS
elscUpdateSaveInfo(void)
{
	bayid_t CurrBay;
	elsc_save_t SaveInfo[MAX_BAYS];

	/* Start off with a clean slate */
	bzero((char *) SaveInfo, MAX_BAYS * sizeof(elsc_save_t));

	/* Generate save info from the current ELSC settings */
	for (CurrBay = 0;  CurrBay < MAX_BAYS;  ++CurrBay) {
		SaveInfo[CurrBay].EMsgRack = ELSC[CurrBay].EMsgRack;
		SaveInfo[CurrBay].EMsgAltRack = ELSC[CurrBay].EMsgAltRack;
		SaveInfo[CurrBay].Flags = ESF_ALTRACK | ESF_PASSWORD;
		strncpy(SaveInfo[CurrBay].Password,
			ELSC[CurrBay].Password,
			ELSC_PASSWORD_LEN);
	}

	/* Go write the whole mess out to NVRAM */
	if (nvramWrite(NVRAM_ELSCINFO, SaveInfo) != OK) {
		ffscMsg("WARNING: Unable to save MSC info in NVRAM");
		return ERROR;
	}

	return OK;
}


#ifndef PRODUCTION

/*----------------------------------------------------------------------*/
/*									*/
/*			  DEBUGGING FUNCTIONS				*/
/*									*/
/*----------------------------------------------------------------------*/

/*
 * elscList
 *	Dump one or all of the elscole_t's
 */
STATUS
elscList(int Arg)
{
	char Name[20];

	if (Arg >= 0  &&  Arg < MAX_BAYS) {
		sprintf(Name, "ELSC[%d]", Arg);
		elscShow(&ELSC[Arg], 0, Name);
	}
	else if (Arg == -1) {
		int i;

		for (i = 0;  i < MAX_BAYS;  ++i) {
			sprintf(Name, "ELSC[%d]", i);
			elscShow(&ELSC[i], 0, Name);
			ffscMsg("");
		}
	}
	else {
		ffscMsg("Specify value from 0 to %d, or -1 to show all",
			MAX_BAYS);
	}

	return OK;
}


/*
 * elscShow
 *	Display information about the specified buffer.
 */
STATUS
elscShow(elsc_t *EInfo, int Offset, char *Title)
{
	char Indent[80];

	if (EInfo == NULL) {
		ffscMsg("Usage: elscShow <addr> [<offset> [<title>]]");
		return OK;
	}

	sprintf(Indent, "%*s", Offset, "");

	if (Title == NULL) {
		ffscMsg("%selsc_t %c (%p):", Indent, EInfo->Name, EInfo);
	}
	else {
		ffscMsg("%s%s - elsc_t %c (%p):",
			Indent,
			Title, EInfo->Name, EInfo);
	}

	ffscMsg("%s    Flags 0x%08x   Owner %d   FD %d",
		Indent,
		EInfo->Flags, EInfo->Owner, EInfo->FD);
	ffscMsg("%s    Bay %ld   Num %ld   Password \"%s\"",
		Indent,
		EInfo->Bay, EInfo->ModuleNum, EInfo->Password);
	ffscMsg("%s    EMsgRack %ld   EMsgAltRack %ld",
		Indent,
		EInfo->EMsgRack, EInfo->EMsgAltRack);
	ffscMsg("%s    DisplayString \"%.8s\"   DisplayCode %d",
		Indent,
		EInfo->DispData, EInfo->DispData[ELSC_DSP_CODE_INDEX]);

	if (EInfo->InMsg != NULL) {
		msgShow(EInfo->InMsg, Offset + 4, "InMsg");
	}
	if (EInfo->OutMsg != NULL) {
		msgShow(EInfo->OutMsg, Offset + 4, "OutMsg");
	}
	if (EInfo->OutAck != NULL) {
		msgShow(EInfo->OutAck, Offset + 4, "OutAck");
	}

	return OK;
}  

#endif  /* !PRODUCTION */
