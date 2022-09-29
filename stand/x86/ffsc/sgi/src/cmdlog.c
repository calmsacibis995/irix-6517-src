/*
 * cmdlog.c
 *	Functions for handling the LOG command and its variants
 */

#include <vxworks.h>

#include <iolib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ffsc.h"

#include "buf.h"
#include "cmd.h"
#include "console.h"
#include "dest.h"
#include "log.h"
#include "misc.h"
#include "msg.h"
#include "remote.h"
#include "route.h"
#include "user.h"


/* Token tables */
static tokeninfo_t LogNames[] = {
	{ "TERMINAL",	LOG_TERMINAL },
	{ "TERM",	LOG_TERMINAL },
	{ "MSC",	LOG_ELSC_UPPER },
	{ "ELSC",	LOG_ELSC_UPPER },
	{ "SYSTEM",	LOG_BASEIO },
	{ "SYS",	LOG_BASEIO },
	{ "ALTCONS",	LOG_ALTCONS },
	{ "DEBUG",	LOG_DEBUG },
	{ "DISPLAY",	LOG_DISPLAY },
	{ NULL,		0 },
};

#define LOGSC_CLEAR	1
#define LOGSC_DUMP	2
#define LOGSC_DISABLE	3
#define LOGSC_ENABLE	4
#define LOGSC_INFO	5
#define LOGSC_LINES	6
#define LOGSC_LENGTH	7
static tokeninfo_t LogSubCmds[] = {
	{ "CLEAR",	LOGSC_CLEAR },
	{ "DUMP",	LOGSC_DUMP },
	{ "DISABLE",	LOGSC_DISABLE },
	{ "ENABLE",	LOGSC_ENABLE },
	{ "INFO",	LOGSC_INFO },
	{ "?",		LOGSC_INFO },
	{ "LINES",	LOGSC_LINES },
	{ "LENGTH",	LOGSC_LENGTH },
	{ NULL,		0 },
};


/* Internal functions */
static int cmdLOG_dump(int, int, user_t *, dest_t *, char *);
static int cmdLOG_info(int, user_t *, dest_t *, char *);


/*
 * cmdLOG
 *	Process the LOG command, which dumps the contents of a particular
 *	log, or otherwise manipulates it in some way.
 */
int
cmdLOG(char **CmdP, struct user *User, struct dest *Dest)
{	
	char *CmdName;
	char *LogName;
	char *Ptr;
	char *Token;
	int  GotValue = 0;
	int  LogNum;
	int  Result;
	int  SubCmd;
	int  Value = -1;
	int  Count;
	int	MultiMSC = 0;

	Count = 1;
	Result = 0;

	/* Parse the log name */
	Token = strtok_r(NULL, " \t", CmdP);
	if (Token == NULL  ||
	    ffscTokenToInt(LogNames, Token, &LogNum) != OK)
	{
		LogNum  = LOG_ELSC_UPPER;
		LogName = "MSC";
	}
	else {
		LogName = Token;
		Token   = strtok_r(NULL, " \t", CmdP);
	}

/* only the dump log will operate on a single MSC log.  All other
 * requests will pay attention to the destination 
 */

	/* If we are playing with an ELSC log, do some extra work */
	if (LogNum == LOG_ELSC_UPPER) {
		bayid_t Bay;
		int     NumBays;

		/* See if more then one bay is being selected */
		if ((Dest->Map[ffscLocal.rackid] == (LOG_ELSC_UPPER | LOG_ELSC_LOWER)) ||
			 (Dest->Map[ffscLocal.rackid] & BAYMAP_ALL)) {
			Count = 2;
			MultiMSC = 1;
		}
		else {
			/* Get the *real* log number for this ELSC */
			NumBays = destNumBays(Dest, NULL, &Bay);
			LogNum = ELSCLogs[Bay]->Num;
			MultiMSC = 0;
		}

#if 0
		/* See how many bays are selected */
		NumBays = destNumBays(Dest, NULL, &Bay);

		if (NumBays != 1) 
			Count = 2;

		/* Only one bay can be specified */
		if (NumBays != 1) {
			ffscMsg("Must specify exactly one bay with LOG MSC");
			sprintf(Responses[ffscLocal.rackid]->Text,
				"ERROR DEST");
			return PFC_DONE;
		}

		/* Get the *real* log number for this ELSC */
		LogNum = ELSCLogs[Bay]->Num;
#endif
	}

	/* Determine the subcommand */
	if (Token == NULL) {
		CmdName = "DUMP";
		SubCmd = LOGSC_DUMP;
		GotValue = 1;
	}
	else if (ffscTokenToInt(LogSubCmds, Token, &SubCmd) != OK) {

		/* If the token isn't a valid subcommand, see if it is */
		/* a numeric argument for an implied DUMP subcommand.  */
		Value = strtol(Token, &Ptr, 0);
		if (*Ptr != '\0') {
			ffscMsg("Invalid LOG subcommand \"%s\"", Token);
			sprintf(Responses[ffscLocal.rackid]->Text,
				"ERROR ARG");
			return PFC_DONE;
		}

		CmdName = "DUMP";
		SubCmd = LOGSC_DUMP;
		GotValue = 1;
	}
	else {
		CmdName = Token;
	}

	/* For the DUMP, LINES and LENGTH subcommands, check for an */
	/* integer argument					    */
	if (!GotValue  &&
	    (SubCmd == LOGSC_DUMP   ||
	     SubCmd == LOGSC_LINES  ||
	     SubCmd == LOGSC_LENGTH))
	{
		Token = strtok_r(NULL, " \t", CmdP);
		if (Token != NULL) {
			ffscConvertToUpperCase(Token);
			if (strcmp(Token, "DEFAULT") == 0) {
				Value = -1;
			}
			else {
				Value = strtol(Token, &Ptr, 0);
				if (*Ptr != '\0') {
					ffscMsg("Invalid argument to LOG %s: "
						    "\"%s\"",
						CmdName, Token);
					sprintf(
					     Responses[ffscLocal.rackid]->Text,
					     "ERROR ARG");
					return PFC_DONE;
				}
			}
			GotValue = 1;
		}
	}

	/* In the case of LINES and LENGTH, the argument is required */
	if (!GotValue  &&
	    (SubCmd == LOGSC_LINES  ||  SubCmd == LOGSC_LENGTH))
	{
		ffscMsg("Argument required for LOG %s", CmdName);
		sprintf(Responses[ffscLocal.rackid]->Text, "ERROR ARG");
		return PFC_DONE;
	}

	while (Count) {

		/*
		 * if the MSC log make sure both are executed when the dest. is >
		 * than 1 bay.
		 */

		if(MultiMSC) { 
			if (Count == 2)
				LogNum = LOG_ELSC_UPPER;
			else  
				LogNum = LOG_ELSC_LOWER;
		}

	/* Proceed according to the subcommand */
		switch (SubCmd) {

	    	/* Clear the log */
	    	case LOGSC_CLEAR:
			if (destRackIsSelected(Dest, ffscLocal.rackid)) {
				logClear(logs[LogNum]);
				sprintf(Responses[ffscLocal.rackid]->Text,
					"R%ld:OK",
					ffscLocal.rackid);
				Responses[ffscLocal.rackid]->State = RS_DONE;
			}
			Result = PFC_FORWARD;

			break;

	    	/* Dump the log */
	    	case LOGSC_DUMP:
			Result = cmdLOG_dump(LogNum, Value, User, Dest, LogName);

			break;

	    	/* Disable logging */
	    	case LOGSC_DISABLE:

			/* Make sure the user is authorized to make changes */
			if (User->Authority < AUTH_SERVICE) {
				sprintf(Responses[ffscLocal.rackid]->Text,
					"ERROR AUTHORITY");
				return PFC_DONE;
			}

			/* Turn off logging locally if selected */
			if (destRackIsSelected(Dest, ffscLocal.rackid)) {
				logs[LogNum]->Flags |= LOGF_DISABLE;
	
				ffscLogInfo[LogNum].Flags |= LIF_DISABLE;
				logUpdateSaveInfo();

				sprintf(Responses[ffscLocal.rackid]->Text,
					"R%ld:OK",
					ffscLocal.rackid);
				Responses[ffscLocal.rackid]->State = RS_DONE;
			}
			Result = PFC_FORWARD;

			break;

	    	/* Enable logging */
	    	case LOGSC_ENABLE:

			/* Make sure the user is authorized to make changes */
			if (User->Authority < AUTH_SERVICE) {
				sprintf(Responses[ffscLocal.rackid]->Text,
					"ERROR AUTHORITY");
				return PFC_DONE;
			}

			/* Turn on logging locally if selected */
			if (destRackIsSelected(Dest, ffscLocal.rackid)) {
				logs[LogNum]->Flags &= ~LOGF_DISABLE;

				ffscLogInfo[LogNum].Flags &= ~LIF_DISABLE;
				logUpdateSaveInfo();

				sprintf(Responses[ffscLocal.rackid]->Text,
					"R%ld:OK",
					ffscLocal.rackid);
				Responses[ffscLocal.rackid]->State = RS_DONE;
			}
			Result = PFC_FORWARD;

			break;

	    	/* Display log info */
	    	case LOGSC_INFO:
			Result = cmdLOG_info(LogNum, User, Dest, LogName);

			break;

	    	/* Set the number of lines for the log */
	    	case LOGSC_LINES:

			/* Make sure the user is authorized to make changes */
			if (User->Authority < AUTH_SERVICE) {
				sprintf(Responses[ffscLocal.rackid]->Text,
					"ERROR AUTHORITY");
				return PFC_DONE;
			}
			

			/* Set the number of lines */
			if (destRackIsSelected(Dest, ffscLocal.rackid)) {

			  /* But before that, make sure we won't take up
			     too much storage */
			  int total_bufsize = 0;
			  int logindx;
			  int avg_length;

			  for (logindx = 0; logindx < NUM_LOGS; logindx++) {
			    total_bufsize += logs[logindx]->MaxSize;
			  }

			  /* Figure out how much space this would take up */
			  total_bufsize -= logs[LogNum]->MaxSize;
			  avg_length = logs[LogNum]->MaxSize/logs[LogNum]->MaxLines;
			  total_bufsize += Value * avg_length;
			  /* If not too much, go ahead and do it */
			  if (total_bufsize <= MAX_LOGBUFSIZE) {

			    ffscLogInfo[LogNum].MaxLines = Value;
			    logUpdateSaveInfo();
			    sprintf(Responses[ffscLocal.rackid]->Text,
				    "R%ld:OK Must reset MMSC for change to take "
				    "effect",
				    ffscLocal.rackid);
			    Responses[ffscLocal.rackid]->State = RS_DONE;

			  } else {
			    /* Otherwise tell the user he wants too much */
			    sprintf(Responses[ffscLocal.rackid]->Text,
				    "R%ld:ERROR Requested log size is too large",
				    ffscLocal.rackid);
			    Responses[ffscLocal.rackid]->State = RS_DONE;
			  }

			  
			}
			Result = PFC_FORWARD;

			break;

	    	/* Set the average line length for the log */
	    	case LOGSC_LENGTH:

			/* Make sure the user is authorized to make changes */
			if (User->Authority < AUTH_SERVICE) {
				sprintf(Responses[ffscLocal.rackid]->Text,
					"ERROR AUTHORITY");
				return PFC_DONE;
			}

			/* Set the average line length */
			if (destRackIsSelected(Dest, ffscLocal.rackid)) {
				ffscLogInfo[LogNum].AvgLength = Value;
				logUpdateSaveInfo();

				sprintf(Responses[ffscLocal.rackid]->Text,
					"R%ld:OK Must reset MMSC for change to take "
				    	"effect",
					ffscLocal.rackid);
				Responses[ffscLocal.rackid]->State = RS_DONE;
			}
			Result = PFC_FORWARD;

			break;

	    	/* Don't recognize this subcommand */
	    	default:
			ffscMsg("Unrecognized subcommand to LOG: \"%d\"", SubCmd);

			sprintf(Responses[ffscLocal.rackid]->Text,
				"ERROR NOTAVAILABLE");
			Result = PFC_DONE;

			break;
		}
		Count--;
	}

	return Result;
}


/*
 * cmdLOG_dump
 *	Process a LOG DUMP command, which dumps the contents of the
 *	specified log.
 */
int
cmdLOG_dump(int LogNum, int Value, user_t *User, dest_t *Dest, char *LogName)
{
	int NumRacks;
	pagebuf_t *PageBuf;
	rackid_t  Rack;

	/* Count how many racks were specified */
	NumRacks = destNumRacks(Dest, &Rack);
	if (NumRacks != 1) {
		ffscMsg("Must specify only one rack with LOG %s DUMP",
			LogName);
		sprintf(Responses[ffscLocal.rackid]->Text,
			"ERROR DEST");
		return PFC_DONE;
	}

	/* If the user is local but the requested rack is not, then we */
	/* need to ask the remote rack for its log info		       */
	if (User->Type != UT_FFSC  &&  Rack != ffscLocal.rackid) {
		char Command[MAX_FFSC_CMD_LEN + 1];
		int  DataLen;
		int  RemoteFD;

		/* Set up the remote command */
		sprintf(Command, "LOG %s DUMP %d", LogName, Value);

		/* Fire up the connection request */
		RemoteFD = remStartDataRequest(User,
					       Command,
					       Rack,
					       Dest,
					       &DataLen);
		if (RemoteFD < 0) {
			/* Error msgs & responses already taken care of */
			return PFC_DONE;
		}

		/* Allocate a pagebuf for the incoming data */
		PageBuf = ffscCreatePageBuf(DataLen);
		if (PageBuf == NULL) {
			ffscMsgS("Unable to allocate pagebuf (length %d) for "
				     "LOG %s DUMP",
				 DataLen, LogName);

			close(RemoteFD);

			sprintf(Responses[ffscLocal.rackid]->Text,
				"ERROR STORAGE");
			return PFC_DONE;
		}

		/* Go read the data */
		if (remReceiveData(RemoteFD, PageBuf->Buf, DataLen) != OK) {
			/* Error messages & responses already taken care of */
			ffscFreePageBuf(PageBuf);
			return PFC_DONE;
		}

		/* Update the pagebuf info */
		PageBuf->DataLen = DataLen;

		/* We have successfully obtained the data */
		close(RemoteFD);
	}

	/* Otherwise, write local log info into a pagebuf */
	else {
		int NumLines;
		int ReadLen;
		log_t *Log = logs[LogNum];

		/* Lock the log to prevent updates while we are reading */
		if (logHold(Log) != OK) {
			/* logHold should have printed the error message */
			sprintf(Responses[ffscLocal.rackid]->Text,
				"ERROR INTERNAL");
			return PFC_DONE;
		}
			
		/* MUST NOT USE ffscMsg* BEYOND THIS POINT */

		/* Determine how many lines are to be dumped */
		if (Value < 1  ||  Value > Log->NumLines) {
			NumLines = Log->NumLines;
		}
		else {
			NumLines = Value;
		}

		/* Create a buffer large enough to hold the entire log */
		PageBuf = ffscCreatePageBuf(Log->MaxSize + 240);
		if (PageBuf == NULL) {
			logRelease(Log);
			ffscMsg("Unable to allocate buffer to hold entire "
				    "%s log",
				Log->Name);

			sprintf(Responses[ffscLocal.rackid]->Text,
				"ERROR STORAGE");
			return PFC_DONE;
		}

		/* Include a neat header */
		ffscPrintPageBuf(PageBuf, "\nData from log %s:", Log->Name);
		ffscPrintPageBuf(PageBuf,
				 "----------------------------------------"
				 "---------------------------------------");

		/* Go copy down the requested lines from the buffer */
		ReadLen = logReadLines(Log,
				       PageBuf->CurrPtr,
				       Log->MaxSize,
				       Log->NumLines - NumLines + 1,
				       Log->NumLines);
		PageBuf->CurrPtr += ReadLen;
		PageBuf->DataLen += ReadLen;

		/* Append a trailer */
		ffscPrintPageBuf(PageBuf,
				 "----------------------------------------"
				 "---------------------------------------");

		/* Safe for others to access the log again */
		logRelease(Log);
	}

	/* If the request was from a remote machine, send the data over */
	/* to it.							*/
	if (User->Type == UT_FFSC) {

		/* Make sure that local info was really requested */
		if (Rack != ffscLocal.rackid) {
			ffscMsg("Remote MMSC tried to get DUMP the LOG for "
				    "a remote MMSC");

			ffscFreePageBuf(PageBuf);

			sprintf(Responses[ffscLocal.rackid]->Text,
				"ERROR DEST");
			return PFC_DONE;
		}

		/* Go send the data over to the other side */
		if (remSendData(User, PageBuf->Buf, PageBuf->DataLen) != OK) {
			/* remSendData should have set up a response, etc. */
			return PFC_DONE;
		}

		/* Consider the operation a success. Note that the */
		/* remote side will take care of the response.     */
		Responses[ffscLocal.rackid]->Text[0] = '\0';
	}

	/* Otherwise, hand the log info off to the pager */
	else {
		/* Log the fact that the log won't be logged(!) */
		if (User->Console->UserOutBuf->Log != NULL) {
			char *Warning;

			Warning = "<<DUMPED DATA OMITTED FROM LOG>>\r\n";
			logWrite(User->Console->UserOutBuf->Log,
				 Warning,
				 strlen(Warning));
		}

		/* Indicate success and request PAGER mode */
		sprintf(Responses[ffscLocal.rackid]->Text,
			"OK;STATE FS%d FB%d M%d",
			getSystemPortFD(), 
			(int) PageBuf, 
			CONM_PAGER);
	}

	return PFC_DONE;
}


/*
 * cmdLOG_info
 *	Process a LOG INFO command, which displays information about a
 *	particular log
 */
int
cmdLOG_info(int LogNum, user_t *User, dest_t *Dest, char *LogName)
{
	int NumRacks;
	pagebuf_t *PageBuf;
	rackid_t  Rack;

	/* Count how many racks were specified */
	NumRacks = destNumRacks(Dest, &Rack);
	if (NumRacks != 1) {
		ffscMsg("Must specify only one rack with LOG %s INFO",
			LogName);
		sprintf(Responses[ffscLocal.rackid]->Text,
			"ERROR DEST");
		return PFC_DONE;
	}

	/* If the user is local but the requested rack is not, then we */
	/* need to ask the remote rack for its log info		       */
	if (User->Type != UT_FFSC  &&  Rack != ffscLocal.rackid) {
		char Command[MAX_FFSC_CMD_LEN + 1];
		int  DataLen;
		int  RemoteFD;

		/* Set up the remote command */
		sprintf(Command, "LOG %s INFO", LogName);

		/* Fire up the connection request */
		RemoteFD = remStartDataRequest(User,
					       Command,
					       Rack,
					       Dest,
					       &DataLen);
		if (RemoteFD < 0) {
			/* Error msgs & responses already taken care of */
			return PFC_DONE;
		}

		/* Allocate a pagebuf for the incoming data */
		PageBuf = ffscCreatePageBuf(DataLen);
		if (PageBuf == NULL) {
			ffscMsgS("Unable to allocate pagebuf (length %d) for "
				     "LOG %s INFO",
				 DataLen, LogName);

			close(RemoteFD);

			sprintf(Responses[ffscLocal.rackid]->Text,
				"ERROR STORAGE");
			return PFC_DONE;
		}

		/* Go read the data */
		if (remReceiveData(RemoteFD, PageBuf->Buf, DataLen) != OK) {
			/* Error messages & responses already taken care of */
			ffscFreePageBuf(PageBuf);
			return PFC_DONE;
		}

		/* Update the pagebuf info */
		PageBuf->DataLen = DataLen;

		/* We have successfully obtained the data */
		close(RemoteFD);
	}

	/* Otherwise, write local log info into a pagebuf */
	else {

		/* Allocate a paged output buffer, guessing at the size */
		PageBuf = ffscCreatePageBuf(512);
		if (PageBuf == NULL) {
			sprintf(Responses[ffscLocal.rackid]->Text,
				"ERROR STORAGE");
			return PFC_DONE;
		}

		/* Start printing */
		ffscPrintPageBuf(PageBuf,
				 "Info for log %s",
				 logs[LogNum]->Name);
		ffscPrintPageBuf(PageBuf,
				 "    Using %d of %d lines.",
				 logs[LogNum]->NumLines,
				 logs[LogNum]->MaxLines);
		ffscPrintPageBuf(PageBuf,
				 "    Using %d of %d bytes of storage.",
				 logs[LogNum]->BufSize,
				 logs[LogNum]->MaxSize);

		if (logs[LogNum]->NumLines > 0) {
			ffscPrintPageBuf(PageBuf,
					 "    Average line length is %d "
					     "bytes.",
					 (logs[LogNum]->BufSize
					  / logs[LogNum]->NumLines));
		}
		else {
			ffscPrintPageBuf(PageBuf,
					 "    Average line length not "
					     "available.");
		}

		if (ffscLogInfo[LogNum].MaxLines > 0) {
			ffscPrintPageBuf(PageBuf,
					 "    After reset, the maximum "
					     "number of lines will be %d.",
					 ffscLogInfo[LogNum].MaxLines);
		}
		else {
			ffscPrintPageBuf(PageBuf,
					 "    After reset, the maximum "
					     "number of lines will be the "
					     "default (%d).",
					 ffscTune[TUNE_LOG_DFLT_NUMLINES]);
		}

		if (ffscLogInfo[LogNum].AvgLength > 0) {
			ffscPrintPageBuf(PageBuf,
					 "    After reset, the average "
					     "line length will be set to %d.",
					 ffscLogInfo[LogNum].AvgLength);
		}
		else {
			ffscPrintPageBuf(PageBuf,
					 "    After reset, the average "
					     "line length will be set to "
					     "the default (%d).",
					 ffscTune[TUNE_LOG_DFLT_LINELEN]);
		}

		if (logs[LogNum]->Flags & LOGF_DISABLE) {
			ffscPrintPageBuf(PageBuf, "    Logging is DISABLED.");
		}
	}

	/* If the request was from a remote machine, send the data over */
	/* to it.							*/
	if (User->Type == UT_FFSC) {

		/* Make sure that local info was really requested */
		if (Rack != ffscLocal.rackid) {
			ffscMsg("Remote MMSC tried to get LOG INFO for remote "
				    "MMSC");

			ffscFreePageBuf(PageBuf);

			sprintf(Responses[ffscLocal.rackid]->Text,
				"ERROR DEST");
			return PFC_DONE;
		}

		/* Go send the data over to the other side */
		if (remSendData(User, PageBuf->Buf, PageBuf->DataLen) != OK) {
			/* remSendData should have set up a response, etc. */
			return PFC_DONE;
		}

		/* Consider the operation a success. Note that the */
		/* remote side will take care of the response.     */
		Responses[ffscLocal.rackid]->Text[0] = '\0';
	}

	/* Otherwise, hand the log info off to the pager */
	else {

		/* Log the data manually, since it won't be logged */
		/* by the pager					   */
		if (User->Console->UserOutBuf->Log != NULL) {
			logWrite(User->Console->UserOutBuf->Log,
				 PageBuf->Buf,
				 PageBuf->DataLen);
		}

		/* Indicate success and request PAGER mode */
		sprintf(Responses[ffscLocal.rackid]->Text,
			"OK;STATE FS%d FB%d M%d",
			getSystemPortFD(),
			(int) PageBuf, 
			CONM_PAGER);
	}

	return PFC_DONE;
}
