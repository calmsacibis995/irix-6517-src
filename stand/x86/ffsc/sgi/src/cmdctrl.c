/*
 * cmdctrl.c
 *	Functions for handling commands that change control characters
 *	and other console characteristics.
 */

#include <vxworks.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ffsc.h"

#include "cmd.h"
#include "console.h"
#include "dest.h"
#include "elsc.h"
#include "misc.h"
#include "msg.h"
#include "route.h"
#include "user.h"


/* Option information */
static tokeninfo_t CEchoModes[] = {
	{ "ON",		CONCE_ON },
	{ "OFF",	CONCE_OFF },
	{ NULL,		0 },
};

static tokeninfo_t EMsgModes[] = {
	{ "ON",		CONEM_ON },
	{ "TERSE",	CONEM_TERSE },
	{ "OFF",	CONEM_OFF },
	{ NULL,		0 },
};

static tokeninfo_t RMsgModes[] = {
	{ "ON",		CONRM_ON },
	{ "ERROR",	CONRM_ERROR },
	{ "OFF",	CONRM_OFF },
	{ "VERBOSE",	CONRM_VERBOSE },
	{ NULL,		0 },
};

#define PAGER_BACK	1
#define PAGER_FWD	2
#define PAGER_QUIT	3
#define PAGER_INFO	5
#define PAGER_LINES	6
#define PAGER_ON	7
#define PAGER_OFF	8 
static tokeninfo_t PagerSubCmds[] = {
	{ "BACK",	PAGER_BACK },
	{ "FWD",	PAGER_FWD },
	{ "QUIT",	PAGER_QUIT },
	{ "INFO",	PAGER_INFO },
	{ "?",		PAGER_INFO },
	{ "LINES",	PAGER_LINES },
	{ "ON",		PAGER_ON },
	{ "OFF",	PAGER_OFF },
	{ NULL,		0 }
};


/* Other internal functions */
static int  cmdEMSG_rack(char **, dest_t *, int);
static int  cmdEMSG_text(char **, user_t *, dest_t *);
static void cmdPAGER_info(user_t *);



/*
 * cmdBS
 *	Show/Change the current backspace character
 */
int
cmdBS(char **CmdP, user_t *User, dest_t *Dest)
{
	cmdChangeCtrlChar(CmdP, User, "CB");
	return PFC_DONE;
}


/*
 * cmdCECHO
 *	Switch/Set the current command echo mode
 */
int
cmdCECHO(char **CmdP, user_t *User, dest_t *Dest)
{
	char *ModeName;

	ModeName = strtok_r(NULL, " \t", CmdP);
	cmdChangeSimpleMode(ModeName, CEchoModes, "EC");
	return PFC_DONE;
}


/*
 * cmdDEST
 *	Displays and/or sets the current default destination.
 */
int
cmdDEST(char **CmdP, user_t *User, dest_t *Dest)
{

  /* Sasha 9/9/98 */

  /*  ffscMsg("cmdDEST just got called with: %s", Dest->String); */
	/* If an explicit destination was specified, make it the new default */
	if (!(Dest->Flags & DESTF_DEFAULT)) {

		/* Set up the new default */
		User->DfltDest = *Dest;
		User->DfltDest.Flags |= DESTF_DEFAULT;

		/* Say what we have done if desired */
		if (ffscDebug.Flags & FDBF_ROUTER) {
			ffscMsg("Changing default destination for user %s to "
				    "%s",
				User->Name,
				User->DfltDest.String);
		}

		/* Update NVRAM with the new default */
		userUpdateSaveInfo(User);
	}

	/* Display the current settings */
	sprintf(Responses[ffscLocal.rackid]->Text,
		"OK %.70s%s",
		User->DfltDest.String,
		(strlen(User->DfltDest.String) > 70) ? "+" : "");
	return PFC_DONE;
}


/*
 * cmdEXIT
 *	Show/change the current exit character, or actually exit from
 *	the current input mode.
 */
int
cmdEXIT(char **CmdP, user_t *User, dest_t *Dest)
{
	/* If there are no arguments at all, switch to last or base mode */
	if (*CmdP == NULL  ||
	    *(*CmdP + strspn(*CmdP, " \t")) == '\0')
	{
		int NewMode;

		/* The new mode is the previous mode if one has been set */
		/* or else the console's base mode			 */
		if (User->Console->PrevMode != CONM_UNKNOWN) {
			NewMode = User->Console->PrevMode;
			User->Console->PrevMode = CONM_UNKNOWN;
		}
		else {
			NewMode = User->Console->BaseMode;
		}

		/* Error if the console is already in its base mode */
		if (User->Console->Mode == User->Console->BaseMode) {
			ffscMsg("Console %s already in base mode %d",
				User->Console->Name, User->Console->BaseMode);
			sprintf(Responses[ffscLocal.rackid]->Text,
				"ERROR NOP");
			return PFC_DONE;
		}

		/* Formally arrange for the console to return to base mode */
		sprintf(Responses[ffscLocal.rackid]->Text,
			"OK;STATE FS%d M%d",getSystemPortFD(),
			 NewMode);

		/* If we were in MODEM mode, tell the "other" console */
		/* that it is safe to return to its normal mode.      */
		if (User->Console->Mode == CONM_MODEM) {
			user_t *OtherUser;

			if (User == userTerminal) {
				OtherUser = userAlternate;
			}
			else if (User == userAlternate) {
				OtherUser = userTerminal;
			}
			else {
				ffscMsg("User %s was in MODEM mode??",
					User->Name);
				return PFC_DONE;
			}

			userChangeInputMode(OtherUser,
					    OtherUser->Console->BaseMode,
					    0);
		}

		/* Tell the router to check for an unclaimed IO6, to be safe */
		routeCheckBaseIO = 1;
	}

	/* Otherwise, change or display the exit character */
	else {
		cmdChangeCtrlChar(CmdP, User, "CX");
	}

	return PFC_DONE;
}


/*
 * cmdEMSG
 *	Switch/Set the current ELSC message mode
 */
int
cmdEMSG(char **CmdP, user_t *User, dest_t *Dest)
{
	char *Token;

	/* Get the first token */
	Token = strtok_r(NULL, " \t", CmdP);

	/* If there is no token, treat it as "next mode" */
	if (Token == NULL) {
		cmdChangeSimpleMode(NULL, EMsgModes, "EE");
		return PFC_DONE;
	}

	/* See if it is a non-mode token */
	ffscConvertToUpperCase(Token);
	if (strcmp(Token, "ALTRACK") == 0) {

		/* Change the FFSC whose COM5 gets ELSC messages */
		return cmdEMSG_rack(CmdP, Dest, 1);
	}
	else if (strcmp(Token, "ALTTEXT") == 0) {

		/* Send message to ALTERNATE */
		return cmdEMSG_text(CmdP, userAlternate, Dest);
	}
	else if (strcmp(Token, "RACK") == 0) {

		/* Change the FFSC whose COM1 gets ELSC messages */
		return cmdEMSG_rack(CmdP, Dest, 0);
	}
	else if (strcmp(Token, "TEXT") == 0) {

		/* Send message to TERMINAL */
		return cmdEMSG_text(CmdP, userTerminal, Dest);
	}

	/* Otherwise, this must be a simple mode */
	cmdChangeSimpleMode(Token, EMsgModes, "EE");
	return PFC_DONE;
}


/*
 * cmdEND
 *	Show/change the current end-of-command character
 */
int
cmdEND(char **CmdP, user_t *User, dest_t *Dest)
{
	cmdChangeCtrlChar(CmdP, User, "CC");
	return PFC_DONE;
}


/*
 * cmdESC
 *	Show/change the current FFSC escape character
 */
int
cmdESC(char **CmdP, user_t *User, dest_t *Dest)
{
	cmdChangeCtrlChar(CmdP, User, "CE");
	return PFC_DONE;
}


/*
 * cmdKILL
 *	Show/change the current kill-line character
 */
int
cmdKILL(char **CmdP, user_t *User, dest_t *Dest)
{
	cmdChangeCtrlChar(CmdP, User, "CK");
	return PFC_DONE;
}


/*
 * cmdPAGER
 *	Set/display output paging options
 */
int
cmdPAGER(char **CmdP, user_t *User, dest_t *Dest)
{
	char *Token;
	int  SubCmd;

	/* Parse the subcommand */
	Token = strtok_r(NULL, " \t", CmdP);
	if (Token == NULL) {
		SubCmd = PAGER_INFO;
	}
	else if (ffscTokenToInt(PagerSubCmds, Token, &SubCmd) != OK) {
		ffscMsg("Invalid PAGER subcommand \"%s\"", Token);
		sprintf(Responses[ffscLocal.rackid]->Text, "ERROR ARG");
		return PFC_DONE;
	}

	/* Proceed according to the subcommand */
	switch (SubCmd) {

	    /* Set/display the BACK character */
	    case PAGER_BACK:
		cmdChangeCtrlChar(CmdP, User, "PB");
		break;

	    /* Set/display the FWD character */
	    case PAGER_FWD:
		cmdChangeCtrlChar(CmdP, User, "PF");
		break;

	    /* Set/display the QUIT character */
	    case PAGER_QUIT:
		cmdChangeCtrlChar(CmdP, User, "PQ");
		break;

	    /* Print this console's pager parameters */
	    case PAGER_INFO:
		cmdPAGER_info(User);
		break;

	    /* Update the number of lines per page */
	    case PAGER_LINES:
	    {
		    int Value;

		    /* Find the next token */
		    Token = strtok_r(NULL, " \t", CmdP);
		    if (Token == NULL) {
			    ffscMsg("No value specified for PAGER LINES");
			    sprintf(Responses[ffscLocal.rackid]->Text,
				    "ERROR ARG");
			    return PFC_DONE;
		    }

		    /* Check if the token says "DEFAULT" */
		    ffscConvertToUpperCase(Token);
		    if (strcmp(Token, "DEFAULT") == 0) {
			    Value = -1;
		    }

		    /* Otherwise, convert it to an integer value */
		    else {
			    Value = strtoul(Token, NULL, 0);
			    if (Value < 1) {
				    ffscMsg("Invalid PAGER LINES \"%s\"",
					    Token);
				    sprintf(Responses[ffscLocal.rackid]->Text,
					    "ERROR ARG");
				    return PFC_DONE;
			    }
		    }

		    /* Ask the console to update its value */
		    sprintf(Responses[ffscLocal.rackid]->Text,
			    "OK;STATE LP%d",
			    Value);
		    break;
	    }

	    /* Turn paged display OFF */
	    case PAGER_OFF:
		sprintf(Responses[ffscLocal.rackid]->Text,
			"OK;STATE BS%d",
			CONF_NOPAGE);
		break;

	    /* Turn paged display ON */
	    case PAGER_ON:
		sprintf(Responses[ffscLocal.rackid]->Text,
			"OK;STATE BR%d",
			CONF_NOPAGE);
		break;

	    /* Don't recognize this subcommand */
	    default:
		ffscMsg("Unrecognized subcommand to PAGER: \"%d\"", SubCmd);
		sprintf(Responses[ffscLocal.rackid]->Text,
			"ERROR NOTAVAILABLE");
		return PFC_DONE;
	}

	return PFC_DONE;
}


/*
 * cmdRMSG
 *	Switch/Set the current response message mode
 */
int
cmdRMSG(char **CmdP, user_t *User, dest_t *Dest)
{
	char *ModeName;

	ModeName = strtok_r(NULL, " \t", CmdP);
	cmdChangeSimpleMode(ModeName, RMsgModes, "ER");
	return PFC_DONE;
}


/*----------------------------------------------------------------------*/
/*									*/
/*			    INTERNAL FUNCTIONS				*/
/*									*/
/*----------------------------------------------------------------------*/


/*
 * cmdEMSG_rack
 *	Process the EMSG [ALT]RACK command
 */
int
cmdEMSG_rack(char **CmdP, dest_t *Dest, int IsAlternate)
{
	bayid_t  CurrBay;
	char     *Token;
	rackid_t TrialRackID;

	/* If we were not addressed tell the router to try elsewhere */
	if (!destRackIsSelected(Dest, ffscLocal.rackid)) {
		return PFC_FORWARD;
	}

	/* Get the rack ID */
	Token = strtok_r(NULL, " \t", CmdP);

	/* If no rack ID was specified, just return the current one */
	if (Token == NULL) {
		char RackIDStr[RACKID_STRLEN];

		for (CurrBay = 0;  CurrBay < MAX_BAYS;  ++CurrBay) {
			if (!destLocalBayIsSelected(Dest, CurrBay)) {
				if (destAllBaysSelected(Dest,
							ffscLocal.rackid))
				{
					sprintf(ELSC[CurrBay].OutAck->Text,
						"OFFLINE");
					ELSC[CurrBay].OutAck->State = RS_DONE;
				}

				continue;
			}

			if (IsAlternate) {
				ffscRackIDToString(ELSC[CurrBay].EMsgAltRack,
						   RackIDStr);
			}
			else {
				ffscRackIDToString(ELSC[CurrBay].EMsgRack,
						   RackIDStr);
			}

			sprintf(ELSC[CurrBay].OutAck->Text,
				"OK %s",
				RackIDStr);
			ELSC[CurrBay].OutAck->State = RS_DONE;
		}

		return PFC_FORWARD;
	}

	/* Parse the new rack ID */
	ffscConvertToUpperCase(Token);
	if (strcmp(Token, "NONE") == 0) {
		TrialRackID = RACKID_UNASSIGNED;
	}
	else if (ffscParseRackID(Token, &TrialRackID) != OK) {
		/* ffscParseRackID already printed an error msg */
		sprintf(Responses[ffscLocal.rackid]->Text,
			"ERROR ARG");
		return PFC_DONE;
	}

	/* Change the Official Destination on addressed ELSCs */
	for (CurrBay = 0;  CurrBay < MAX_BAYS;  ++CurrBay) {
		if (!destLocalBayIsSelected(Dest, CurrBay)) {
			if (destAllBaysSelected(Dest, ffscLocal.rackid)) {
				sprintf(ELSC[CurrBay].OutAck->Text, "OFFLINE");
				ELSC[CurrBay].OutAck->State = RS_DONE;
			}

			continue;
		}

		if (IsAlternate) {
			ELSC[CurrBay].EMsgAltRack = TrialRackID;
		}
		else {
			ELSC[CurrBay].EMsgRack = TrialRackID;
		}

		sprintf(ELSC[CurrBay].OutAck->Text, "OK");
		ELSC[CurrBay].OutAck->State = RS_DONE;
	}

	/* Go update NVRAM */
	(void) elscUpdateSaveInfo();

	return PFC_FORWARD;
}

/*
 * cmdEMSG_text
 *	Process the [ALT]TEXT subcommand of the EMSG command
 */
int
cmdEMSG_text(char **CmdP, user_t *User, dest_t *Dest)
{
	char Command[MAX_CONS_CMD_LEN + 1];
	char *Token;

	/* If we were not addressed tell the router to try elsewhere */
	if (!destRackIsSelected(Dest, ffscLocal.rackid)) {
		return PFC_FORWARD;
	}

	/* Make sure there is something to send */
	Token = strtok_r(NULL, "", CmdP);
	if (Token == NULL) {
		Token = "";
	}
	else {
		Token += strspn(Token, " \t");
	}

	/* Generate the actual command that we will send to the console */
	sprintf(Command, "MSG %s", Token);

	/* Write the command to the Terminal task */
	if (userSendCommand(User, Command, Responses[ffscLocal.rackid]->Text)
	    != OK)
	{
		/* userSendCommand should have logged the error already */
		Responses[ffscLocal.rackid]->State = RS_DONE;
		return PFC_DONE;
	}

	/* Record a successful completion */
	sprintf(Responses[ffscLocal.rackid]->Text,
		"R%ld:OK",
		ffscLocal.rackid);
	Responses[ffscLocal.rackid]->State = RS_DONE;
	return PFC_FORWARD;
}


/*
 * cmdPAGER_info
 *	Process the INFO subcommand of the PAGER command
 */
void
cmdPAGER_info(user_t *User)
{
	char CharString[20];
	pagebuf_t *PageBuf;

	/* Set up a paged output buffer */
	PageBuf = ffscCreatePageBuf(512);
	if (PageBuf == NULL) {
		ffscMsgS("Unable to allocate pagebuf for PAGER INFO");
		sprintf(Responses[ffscLocal.rackid]->Text, "ERROR STORAGE");
		return;
	}

	/* Build the message */
	ffscPrintPageBuf(PageBuf, "\r\nPager information:");

	if (User->Console->LinesPerPage < 1) {
		ffscPrintPageBuf(PageBuf,
				 "    Lines per page = DEFAULT (%d)",
				 ffscTune[TUNE_PAGE_DFLT_LINES]);
	}
	else {
		ffscPrintPageBuf(PageBuf,
				 "    Lines per page =  %d",
				 User->Console->LinesPerPage);
	}

	ffscCtrlCharToString(User->Console->Pager[CONP_FWD], CharString);
	ffscPrintPageBuf(PageBuf, "    FWD character  = %s", CharString);

	ffscCtrlCharToString(User->Console->Pager[CONP_BACK], CharString);
	ffscPrintPageBuf(PageBuf, "    BACK character = %s", CharString);

	ffscCtrlCharToString(User->Console->Pager[CONP_QUIT], CharString);
	ffscPrintPageBuf(PageBuf, "    QUIT character = %s", CharString);

	if (User->Console->Flags & CONF_NOPAGE) {
		ffscPrintPageBuf(PageBuf, "    Output is NOT paged.");
	}
	else {
		ffscPrintPageBuf(PageBuf, "    Output is paged.\r\n");
	}

	/* Success */
	sprintf(Responses[ffscLocal.rackid]->Text,
		"OK;STATE FS%d FB%d M%d",
		getSystemPortFD(),
		(int) PageBuf, 
		CONM_PAGER);
}
