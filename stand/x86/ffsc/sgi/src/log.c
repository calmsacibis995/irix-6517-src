/*
 * log.c
 *	FFSC log buffers
 */

#include <vxworks.h>

#include <errno.h>
#include <iolib.h>
#include <semlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ffsc.h"

#include "log.h"
#include "nvram.h"


/* Global variables */
static const char logTruncateTrailer[] = LOG_TRUNCATE_TRAILER;

log_t *logs[NUM_LOGS];
log_t *ELSCLogs[MAX_BAYS];
loginfo_t ffscLogInfo[MAX_LOGS];

/* Internal functions */
static void logCopyData(log_t *, char *, char *, int);
static char *logLineAddr(log_t *, int);
static void logReclaimLine(log_t *);
static void logReclaimSpace(log_t *, int);


/*
 * logClear
 *	Clears the specified log_t
 */
void
logClear(log_t *Log)
{
	logHold(Log);

	Log->BufSize  = 0;
	Log->BufAvail = Log->MaxSize;
	Log->BufStart = Log->Buf;
	Log->BufNext  = Log->Buf;

	Log->NumLines  = 0;
	Log->StartLine = 0;
	Log->NextLine  = 1;

	bzero((char *) Log->Lines, Log->MaxLines * sizeof(char *));
	Log->Lines[Log->StartLine] = Log->BufStart;

	logRelease(Log);
}


/*
 * logCreate
 *	Returns a pointer to a newly created log_t
 */
log_t *
logCreate(char *Name, int Type, int Num, loginfo_t *LogInfo)
{
	int   MaxSize;
	int   MaxLines;
	int   AvgLength;
	log_t *Log;

	/* Allocate storage for the log_t itself */
	Log = (log_t *) malloc(sizeof(log_t));
	if (Log == NULL) {
		ffscMsgS("Unable to allocate storage for log_t %s", Name);
		return NULL;
	}

	/* Set up the important dimensions of the log */
	if (LogInfo->MaxLines > 0) {
		MaxLines = LogInfo->MaxLines;
	}
	else {
		MaxLines = ffscTune[TUNE_LOG_DFLT_NUMLINES];
	}

	if (LogInfo->AvgLength > 0) {
		AvgLength = LogInfo->AvgLength;
	}
	else {
		AvgLength = ffscTune[TUNE_LOG_DFLT_LINELEN];
	}

	MaxSize = MaxLines * AvgLength;

	/* Initialize the easy fields */
	strncpy(Log->Name, Name, LOG_NAME_LEN);
	Log->Name[LOG_NAME_LEN] = '\0';

	Log->Flags = 0;
	Log->Type  = Type;
	Log->Num   = Num;

	Log->MaxSize  = MaxSize;
	Log->MaxLines = MaxLines;

	/* Set up some flags if necessary */
	if (LogInfo->Flags & LIF_DISABLE) {
		Log->Flags |= LOGF_DISABLE;
	}

	/* Allocate the actual storage buffer */
	Log->Buf = (char *) malloc(MaxSize);
	if (Log->Buf == NULL) {
		ffscMsgS("Unable to allocate buffer space for log_t %s", Name);
		free(Log);
		return NULL;
	}

	/* Allocate an array of pointers to individual lines. We allocate */
	/* one extra entry to point to the beginning of the incomplete    */
	/* line that follows the last full line in the buffer.		  */
	Log->Lines = (char **) malloc((MaxLines + 1) * sizeof(char *));
	if (Log->Lines == NULL) {
		ffscMsgS("Unable to allocate pointers for log_t %s", Name);
		free(Log->Buf);
		free(Log);
		return NULL;
	}

	/* Allocate a mutex semaphore to ensure consistency */
	Log->Lock = semMCreate(SEM_Q_PRIORITY | SEM_INVERSION_SAFE);
	if (Log->Lock == NULL) {
		ffscMsgS("Unable to allocate semaphore for log_t %s", Name);
		free(Log->Lines);
		free(Log->Buf);
		free(Log);
		return NULL;
	}

	/* Initialize the remaining fields */
	logClear(Log);

	return Log;
}


/*
 * logHold
 *	Lock the log_t from access by other tasks. Used to ensure that
 *	nobody is trying to write while the log is being read, etc.
 */
STATUS
logHold(log_t *Log)
{
	STATUS Result;
	
	Result = semTake(Log->Lock, WAIT_FOREVER);
	if (Result != OK  &&  Log != logDebug) {
		ffscMsgS("Unable to obtain lock for log_t %s", Log->Name);
	}

	return Result;
}


/*
 * logInitSGI
 *	Set up the global log_t's
 */
STATUS
logInitSGI(void)
{
	int i;
	int UpdateSaveInfo = 0;

	/* Clear out the loginfo array */
	bzero((char *) &ffscLogInfo, sizeof(loginfo_t) * MAX_LOGS);

	/* Go read the persistent info about the logs from NVRAM */
	nvramRead(NVRAM_LOGINFO, &ffscLogInfo);

	/* Replace any invalid entries with default information. */
	/* This handles the case where nvramRead failed, too.	 */
	for (i = 0;  i < NUM_LOGS;  ++i) {
		if (!(ffscLogInfo[i].Flags & LIF_VALID)) {
			ffscLogInfo[i].Flags     = LIF_VALID;
			ffscLogInfo[i].MaxLines  = -1;
			ffscLogInfo[i].AvgLength = -1;
			ffscLogInfo[i].reserved  = 0;

			UpdateSaveInfo = 1;
		}
	}

	/* Update the saved info in nvram if necessary */
	if (UpdateSaveInfo) {
		logUpdateSaveInfo();
	}

	/* Create logs for each of the serial ports */
	logTerminal = logCreate("TerminalLog",
				LOGT_CONSOLE,
				LOG_TERMINAL,
				&ffscLogInfo[LOG_TERMINAL]);

	logELSCUpper = logCreate("ELSCUpperLog",
				 LOGT_ELSC,
				 LOG_ELSC_UPPER,
				 &ffscLogInfo[LOG_ELSC_UPPER]);

	logELSCLower = logCreate("ELSCLowerLog",
				 LOGT_ELSC,
				 LOG_ELSC_LOWER,
				 &ffscLogInfo[LOG_ELSC_LOWER]);

	logBaseIO = logCreate("BaseIOLog",
			      LOGT_CONSOLE,
			      LOG_BASEIO,
			      &ffscLogInfo[LOG_BASEIO]);

	logAltCons = logCreate("AltConsLog",
			       LOGT_CONSOLE,
			       LOG_ALTCONS,
			       &ffscLogInfo[LOG_ALTCONS]);

	logDebug = logCreate("DebugLog",
			     LOGT_DEBUG,
			     LOG_DEBUG,
			     &ffscLogInfo[LOG_DEBUG]);

	logDisplay = logCreate("DisplayLog",
			       LOGT_DISPLAY,
			       LOG_DISPLAY,
			       &ffscLogInfo[LOG_DISPLAY]);

	/* Set up an array of pointers to the logs for the various ELSCs */
	ELSCLogs[0] = logELSCUpper;
	ELSCLogs[1] = logELSCLower;

	return OK;
}


/*
 * logRead
 *	Store the contents of the specified log_t in the specified buffer,
 *	up to some maximum number of bytes. Returns the number of bytes
 *	actually stored. The log is assumed to be held, so it is not safe
 *	for this function to use ffscMsg*.
 */
int
logRead(log_t *Log, char *Buffer, int MaxLen)
{
	int NumBytes = (MaxLen < Log->BufSize) ? MaxLen : Log->BufSize;

	logCopyData(Log, Log->BufStart, Buffer, NumBytes);

	return NumBytes;
}


/*
 * logReadLines
 *	Stores the specified lines from the specified log_t into the
 *	specified buffer, up to some maximum number of bytes. Lines are
 *	indexed starting from 1. Returns the number of bytes actually
 *	stored. The log is assumed to be held, so it is not safe for this
 *	function to use ffscMsg*.
 */
int
logReadLines(log_t *Log, char *Buffer, int MaxLen, int StartLine, int EndLine)
{
	char *EndAddr;
	char *StartAddr;
	int  NumBytes;
	int  NumLines = EndLine - StartLine + 1;

	/* If the user has asked for more lines than we have, reduce */
	/* the size of their request.				     */
	if (NumLines > Log->NumLines) {
		NumLines = Log->NumLines;
	}

	/* Ignore requests for 0 or fewer lines... */
	if (NumLines < 1) {
		return 0;
	}

	/* Calculate the start and end addresses */
	StartAddr = logLineAddr(Log, StartLine);
	EndAddr   = logLineAddr(Log, StartLine + NumLines);

	/* Calculate how many bytes we need to copy, taking into account */
	/* the possibility that the data wraps in the buffer.		 */
	if (StartAddr < EndAddr) {
		NumBytes = EndAddr - StartAddr;
	}
	else {
		NumBytes = (((Log->Buf + Log->MaxSize) - StartAddr)
			    + (EndAddr - Log->Buf));
	}

	/* Go copy the actual data over */
	logCopyData(Log, StartAddr, Buffer, NumBytes);

	return NumBytes;
}


/*
 * logRelease
 *	Release access to a previously held log_t
 */
void
logRelease(log_t *Log)
{
	semGive(Log->Lock);
}


/*
 * logUpdateSaveInfo
 *	Updates the persistent logging info in NVRAM
 */
STATUS
logUpdateSaveInfo(void)
{
	return nvramWrite(NVRAM_LOGINFO, &ffscLogInfo);
}


/*
 * logWrite
 *	Appends the specified data to the specified log_t. If there isn't
 *	enough room at the end of the log to hold the data, older lines
 *	are discarded until there is room. If the data is longer than the
 *	log itself, it is truncated to fit.
 */
void
logWrite(log_t *Log, char *Data, int Len)
{
	char *LastByte = Log->Buf + Log->MaxSize - 1;
	char *NextPtr;
	int  AppendTruncateTrailer = 0;
	int  i;
	int  SawCR, SawLF;
	int  TrailerInd = 0;

	/* If logging is disabled, don't bother */
	if (Log->Flags & LOGF_DISABLE) {
		return;
	}

	/* Wait until it is safe to fiddle with the log */
	if (logHold(Log) != OK) {
		/* logHold should already have logged an error */
		return;
	}

	/* If there isn't enough room for the data, arrange to reclaim */
	/* some space by discarding older lines.		       */
	if (Len > Log->BufAvail) {
		logReclaimSpace(Log, Len);
	}

	/* If there still isn't enough room after that, arrange to */
	/* truncate the input data.				   */
	if (Len > Log->BufAvail) {
		Len = Log->BufAvail;
		AppendTruncateTrailer = 1;
	}

	/* See if the last character before the new data was CR or LF */
	SawCR = 0;
	SawLF = 0;
	if (Log->BufSize > 0) {
		char *LastPtr;

		if (Log->BufNext == Log->Buf) {
			LastPtr = LastByte;
		}
		else {
			LastPtr = Log->BufNext - 1;
		}

		if (*LastPtr == '\r') {
			SawCR = 1;
		}
		else if (*LastPtr == '\n') {
			SawLF = 1;
		}
	}

	/* Proceed until we hit the end of the logged data */
	for (i = 0;  i < Len;  ++i) {

		/* Copy over the next byte of data */
		if (!AppendTruncateTrailer) {
			*Log->BufNext = Data[i];
		}
		else {
			if ((Len - i) <= LOG_TRUNCATE_TRAILER_LEN) {
				*Log->BufNext = logTruncateTrailer[TrailerInd];
				++TrailerInd;
			}
			else {
				*Log->BufNext = Data[i];
			}
		}

		/* Determine the location of the *next* character */
		if (Log->BufNext == LastByte) {
			NextPtr = Log->Buf;
		}
		else {
			NextPtr = Log->BufNext + 1;
		}

		/* If we have seen a complete CR/LF or LF/CR sequence */
		/* note the end of a line.			      */
		if ((SawCR  &&  *Log->BufNext == '\n')  ||
		    (SawLF  &&  *Log->BufNext == '\r'))
		{
			/* If we got LF/CR, convert it to CR/LF. */
			/* This makes paging easier later on.	 */
			if (SawLF) {
				char *Prev;

				if (Log->BufNext == Log->Buf) {
					Prev = LastByte;
				}
				else {
					Prev = Log->BufNext - 1;
				}

				*Prev = '\r';
				*Log->BufNext = '\n';
			}

			/* If we don't have room for any more lines, */
			/* reclaim space from an old line	     */
			if (Log->NumLines == Log->MaxLines) {
				logReclaimLine(Log);
			}

			/* Update the beginning-of-line array */
			Log->Lines[Log->NextLine] = NextPtr;
			Log->NumLines += 1;
			Log->NextLine += 1;
			if (Log->NextLine > Log->MaxLines) {
				Log->NextLine = 0;
			}

			/* The sequence is now complete */
			SawCR = 0;
			SawLF = 0;
		}

		/* If we find a CR or LF, note the potential beginning */
		/* of the end for the current line.		       */
		else if (*Log->BufNext == '\n') {
			SawLF = 1;
		}
		else if (*Log->BufNext == '\r') {
			SawCR = 1;
		}

		/* Otherwise, ignore a previous CR or LF and move on */
		else {
			SawLF = 0;
			SawCR = 0;
		}

		/* Resume with the next character in the buffer */
		Log->BufNext = NextPtr;
	}

	/* Update the buffer sizes */
	Log->BufSize  += Len;
	Log->BufAvail -= Len;

	/* All done with the log_t */
	logRelease(Log);
}


/*
 * logWriteLine
 *	A variation of logWrite that assumes its input is exactly one line,
 *	thus allowing a fair bit of optimization. A CR/LF will be appended
 *	automatically. Useful for ELSC and DEBUG logs, that are well-behaved
 *	in this way.
 */
void
logWriteLine(log_t *Log, char *Data)
{
	int BytesToEnd;
	int Len = strlen(Data) + 2;

	/* If logging is disabled, don't bother */
	if (Log->Flags & LOGF_DISABLE) {
		return;
	}

	/* Wait until it is safe to fiddle with the log */
	if (logHold(Log) != OK) {
		/* logHold should already have logged an error */
		return;
	}

	/* If there isn't enough room for the data, arrange to reclaim */
	/* some space by discarding older lines.		       */
	if (Len > Log->BufAvail) {
		logReclaimSpace(Log, Len);
	}

	/* If there still isn't enough room after that, fall back  */
	/* to normal logWrite to deal with truncation.		   */
	if (Len > Log->BufAvail) {
		logRelease(Log);	/* logWrite will logHold later on */
		logWrite(Log, Data, Len);
		return;
	}

	/* Determine how far away we are from the end of the buffer */
	BytesToEnd = Log->MaxSize - (Log->BufNext - Log->Buf);

	/* Copy over the data in one or two chunks, depending on */
	/* whether or not we have to wrap in the buffer.	 */
	if (Len <= BytesToEnd) {
		bcopy(Data, Log->BufNext, Len - 2);
		Log->BufNext[Len - 2] = '\r';
		Log->BufNext[Len - 1] = '\n';

		if (Len == BytesToEnd) {
			Log->BufNext = Log->Buf;
		}
		else {
			Log->BufNext += Len;
		}
	}
	else {
		int RemainingBytes = Len - BytesToEnd;

		if (RemainingBytes > 2) {
			bcopy(Data, Log->BufNext, BytesToEnd);
			bcopy(Data + BytesToEnd, Log->Buf, RemainingBytes - 2);
			Log->Buf[RemainingBytes - 2] = '\r';
			Log->Buf[RemainingBytes - 1] = '\n';
		}
		else if (RemainingBytes == 2) {
			bcopy(Data, Log->BufNext, BytesToEnd);
			Log->Buf[0] = '\r';
			Log->Buf[1] = '\n';
		}
		else {  /* RemainingBytes must be 1 */
			bcopy(Data, Log->BufNext, BytesToEnd - 1);
			Log->BufNext[BytesToEnd - 1] = '\r';
			Log->Buf[0] = '\n';
		}

		Log->BufNext = Log->Buf + RemainingBytes;
	}

	/* If we don't have room for any more beginning-of-line pointers, */
	/* reclaim space from an old line				  */
	if (Log->NumLines == Log->MaxLines) {
		logReclaimLine(Log);
	}

	/* Update the beginning-of-line array */
	Log->Lines[Log->NextLine] = Log->BufNext;
	Log->NumLines += 1;
	Log->NextLine += 1;
	if (Log->NextLine > Log->MaxLines) {
		Log->NextLine = 0;
	}

	/* Update the buffer sizes */
	Log->BufSize  += Len;
	Log->BufAvail -= Len;

	/* All done with the log_t */
	logRelease(Log);
}	




/*----------------------------------------------------------------------*/
/*									*/
/*			    INTERNAL FUNCTIONS				*/
/*									*/
/*----------------------------------------------------------------------*/

/*
 * logCopyData
 *	Copy over the specified number of bytes from the specified log_t
 *	starting at the specified address (assumed to be within the
 *	actual log_t buffer) into the specified location.
 */
void
logCopyData(log_t *Log, char *StartAddr, char *Buffer, int NumBytes)
{
	char *BufEnd = Log->Buf + Log->MaxSize;

	if (StartAddr + NumBytes >= BufEnd) {
		int  PartialSize;

		PartialSize = BufEnd - StartAddr;
		bcopy(StartAddr, Buffer, PartialSize);
		bcopy(Log->Buf, Buffer + PartialSize, NumBytes - PartialSize);
	}
	else {
		bcopy(StartAddr, Buffer, NumBytes);
	}
}


/*
 * logLineAddr
 *	Returns the address of the specified line in the specified log_t,
 *	where lines are numbered starting at 1. The line number is assumed
 *	to be valid.
 */
char *
logLineAddr(log_t *Log, int LineNum)
{
	int LineIndex;

	LineIndex = Log->StartLine + (LineNum - 1);
	if (LineIndex > Log->MaxLines) {
		LineIndex -= (Log->MaxLines + 1);
	}
	
	return Log->Lines[LineIndex];
}


/*
 * logReclaimLine
 *	Reclaim the space used by the first (earliest) line in the
 *	specified log_t. The log_t is assumed to be held.
 */
void
logReclaimLine(log_t *Log)
{
	char *FirstPtr, *NextPtr;
	int  LineLen;
	int  NextIndex;

	/* Find the index of the *second* line in the buffer */
	NextIndex = Log->StartLine + 1;
	if (NextIndex > Log->MaxLines) {
		NextIndex = 0;
	}

	/* Convenient synonyms */
	FirstPtr = Log->Lines[Log->StartLine];
	NextPtr  = Log->Lines[NextIndex];

	/* If there is no next line, simply clear the log */
	if (NextPtr == NULL) {
		logClear(Log);
		return;
	}

	/* Determine the length of the first line in the log. If the */
	/* next line in the buffer starts *before* the first line,   */
	/* then the first line must wrap in the buffer.		     */
	if (NextPtr > FirstPtr) {
		LineLen = (NextPtr - FirstPtr);
	}
	else {
		LineLen = (((Log->Buf + Log->MaxSize) - FirstPtr)
			   + (NextPtr - Log->Buf));
	}

	/* Purge the current line from the log */
	Log->BufStart  = NextPtr;
	Log->BufSize  -= LineLen;
	Log->BufAvail += LineLen;

	Log->Lines[Log->StartLine] = NULL;
	Log->NumLines -= 1;
	Log->StartLine = NextIndex;

	return;
}


/*
 * logReclaimSpace
 *	Reclaim at least the specified number of bytes of space in the
 *	specified log_t by purging old lines from the log. The log_t is
 *	assumed to be held.
 */
void
logReclaimSpace(log_t *Log, int Len)
{
	/* In the pathological case where there isn't enough room */
	/* in the log itself to satisfy the request, the best we  */
	/* can do is clear the log.				  */
	if (Len > Log->MaxSize) {
		logClear(Log);
		return;
	}

	/* Purge a line at a time until we have enough space in the buffer */
	while (Len > Log->BufAvail) {
		logReclaimLine(Log);
	}

	return;
}




#ifndef PRODUCTION

/*----------------------------------------------------------------------*/
/*									*/
/*			   DEBUGGING FUNCTIONS				*/
/*									*/
/*----------------------------------------------------------------------*/

/*
 * ldump
 *	Dump the specified lines of the specified log_t, where log lines
 *	are numbered starting from *1*. If no start or end line is specified,
 *	dump the entire log one line at a time. If no end line is specified,
 *	dump only the last <startline> lines from the buffer.
 */
STATUS
ldump(log_t *Log, int StartLine, int EndLine)
{
	char ErrMsg[80];
	char LineBuf[256];
	int  DataLen;
	int  i;
	int  LogNum = (int) Log;
	int  WriteLen;
	STATUS Result = OK;

	/* If a small integer was specified for the log, then treat it */
	/* as an index into the log array.			       */
	if (LogNum >= 0  &&  LogNum < NUM_LOGS) {
		Log = logs[LogNum];
	}

	/* Print a nice header */
	ffscMsg("\nData from log %s:", Log->Name);
	ffscMsg("----------------------------------------"
		"---------------------------------------");

	/* Lock the log to prevent updates while we are reading */
	if (logHold(Log) != OK) {
		/* logHold should have printed the error message */
		return ERROR;
	}

	/* MUST NOT USE ffscMsg* BEYOND THIS POINT */

	/* Determine the starting and ending lines in case they were not */
	/* specified by the user.					 */
	if (StartLine <= 0) {
		StartLine = 1;
		EndLine   = Log->NumLines;
	}
	else if (EndLine <= 0) {
		if (StartLine > Log->NumLines) {
			StartLine = Log->NumLines;
		}

		StartLine = Log->NumLines - (StartLine - 1);
		EndLine   = Log->NumLines;
	}
	else if (EndLine < StartLine) {
		logRelease(Log);
		ffscMsg("Invalid line number range %d-%d", StartLine, EndLine);
		return ERROR;
	}
	else {
		if (StartLine > Log->NumLines) {
			StartLine = Log->NumLines;
		}
		if (EndLine > Log->NumLines) {
			EndLine = Log->NumLines;
		}
	}

	/* Pull each individual line from the log and print it directly */
	/* to the debug port.						*/
	for (i = StartLine;  i <= EndLine;  ++i) {
		DataLen = logReadLines(Log, LineBuf, 256, i, i);
		if (DataLen < 1) {
			break;
		}

		WriteLen = ffscDebugWrite(LineBuf, DataLen);
		if (WriteLen < 0) {
			sprintf(ErrMsg,
				"Unable to write to debug console: %s",
				strerror(errno));
			Result = ERROR;
		}
		else if (WriteLen != DataLen  &&  WriteLen > 0) {
			sprintf(ErrMsg,
				"Short write of %d/%d bytes",
				WriteLen, DataLen);
			Result = ERROR;
		}

		/* WriteLen of 0 probably means that there is no debug port */
	}

	/* Safe for others to access the log again */
	logRelease(Log);

	/* If something went wrong, say so */
	if (Result != OK) {
		ffscMsg(ErrMsg);
	}

	/* Trailing separator */
	logWriteLine(logDebug, "<<dump omitted from log>>");
	ffscMsg("----------------------------------------"
		"---------------------------------------");

	return Result;
}


/*
 * ldumpb
 *	Dump the contents of the specified log_t as a single large block.
 *	Useful for testing block transfers of logs.
 */
STATUS
ldumpb(log_t *Log)
{
	char *OutBuf;
	int  DataLen;
	int  LogNum = (int) Log;
	int  ReadLen;
	int  WriteLen;
	STATUS Result = OK;

	/* If a small integer was specified for the log, then treat it */
	/* as an index into the log array.			       */
	if (LogNum >= 0  &&  LogNum < NUM_LOGS) {
		Log = logs[LogNum];
	}

	/* Lock the log to prevent updates while we are reading */
	if (logHold(Log) != OK) {
		/* logHold should have printed the error message */
		return ERROR;
	}

	/* MUST NOT USE ffscMsg* BEYOND THIS POINT */

	/* Create a buffer large enough to hold the entire log */
	DataLen = Log->BufSize;
	OutBuf = (char *) malloc(DataLen);
	if (OutBuf == NULL) {
		logRelease(Log);
		ffscMsg("Unable to allocate buffer to hold entire %s log",
			Log->Name);
		return ERROR;
	}

	/* Go copy down the buffer */
	ReadLen = logRead(Log, OutBuf, DataLen);

	/* Safe for others to access the log again */
	logRelease(Log);

	/* SAFE TO USE ffscMsg* AGAIN */

	/* Complain (but not fatally) if the read was incomplete */
	if (ReadLen != DataLen) {
		ffscMsg("Incomplete read of log %s: %d/%d bytes",
			Log->Name, ReadLen, DataLen);
		Result = ERROR;
	}

	/* Print a nice header */
	ffscMsg("Data from log %s:", Log->Name);
	ffscMsg("----------------------------------------"
		"---------------------------------------");

	/* Dump what we have out to the debug port */
	if (DataLen > 0) {
		WriteLen = ffscDebugWrite(OutBuf, DataLen);
		if (WriteLen < 0) {
			ffscMsgS("Unable to write to debug console");
			Result = ERROR;
		}
		else if (WriteLen != DataLen) {
			ffscMsg("Short write of %d/%d bytes",
				WriteLen, DataLen);
			Result = ERROR;
		}

		if (WriteLen > 0  &&  OutBuf[WriteLen-1] != '\n') {
			ffscMsg("");
		}
	}
	logWriteLine(logDebug, "<<dump omitted from log>>");
	ffscMsg("----------------------------------------"
		"---------------------------------------");

	/* Done with the output buffer */
	free(OutBuf);

	return Result;
}


/*
 * lfill
 *	Fill the specified log_t with the specified number of lines of
 *	random data
 */
STATUS
lfill(log_t *Log, int FillLines)
{
	char *CurrPtr;
	char *Format = "Len%3d: ";
	char Line[80];
	int  i, j;
	int  LogNum = (int) Log;
	int  HdrLen;
	int  LineLen;

	/* If a small integer was specified for the log, then treat it */
	/* as an index into the log array.			       */
	if (LogNum >= 0  &&  LogNum < NUM_LOGS) {
		Log = logs[LogNum];
	}

	HdrLen = strlen(Format);
	for (i = 0;  i < FillLines; ++i) {
		LineLen = rand() % 78;
		if (LineLen < HdrLen) {
			LineLen = HdrLen;
		}
		sprintf(Line, Format, LineLen);

		CurrPtr = Line + HdrLen;
		for (j = HdrLen;  j < LineLen;  ++j) {
			*CurrPtr = '0' + ((j + 1) % 10);
			++CurrPtr;
		}
		*CurrPtr = '\0';

		logWriteLine(Log, Line);
	}

	return OK;
}


/*
 * llines
 *	Display the line pointers for the specified log_t
 */
STATUS
llines(log_t *Log)
{
	int CurrLine;
	int i;
	int LogNum = (int) Log;

	/* If a small integer was specified for the log, then treat it */
	/* as an index into the log array.			       */
	if (LogNum >= 0  &&  LogNum < NUM_LOGS) {
		Log = logs[LogNum];
	}

	ffscMsg("%d lines in log %s:", Log->NumLines, Log->Name);
	CurrLine = Log->StartLine;
	for (i = 0;  i <= Log->NumLines;  ++i) {
		ffscMsgN(" %p", Log->Lines[CurrLine]);
		if (i % 8 == 7  ||  i == Log->NumLines) {
			ffscMsg("");
		}

		++CurrLine;
		if (CurrLine > Log->MaxLines) {
			CurrLine = 0;
		}
	}

	return OK;
}


/*
 * llist
 *	Show one or all of the log_t's
 */
STATUS
llist(int Arg)
{
	char Name[20];

	if (Arg >= 0  &&  Arg < NUM_LOGS) {
		sprintf(Name, "logs[%d]", Arg);
		lshow(logs[Arg], 0, Name);
	}
	else if (Arg == -1) {
		int i;

		for (i = 0;  i < NUM_LOGS;  ++i) {
			sprintf(Name, "logs[%d]", i);
			lshow(logs[i], 0, Name);
			ffscMsg("");
		}
	}
	else {
		ffscMsg("Specify value from 0 to %d, or -1 to show all",
			NUM_LOGS);
	}

	return OK;
}


/*
 * loginfo
 *	Display information from the specifed loginfo_t (as opposed to log_t)
 */
STATUS
loginfo(loginfo_t *LogInfo)
{
	int LogNum = (int) LogInfo;

	/* If a small integer was specified for the log, then treat it */
	/* as an index into the log array.			       */
	if (LogNum >= 0  &&  LogNum < NUM_LOGS) {
		LogInfo = &ffscLogInfo[LogNum];
	}

	ffscMsg("loginfo_t @%p:", LogInfo);
	ffscMsg("    Flags 0x%08x    MaxLines %d    AvgLength %d",
		LogInfo->Flags, LogInfo->MaxLines, LogInfo->AvgLength);

	return OK;
}


/*
 * lshow
 *	Display information about the specified log_t.
 */
STATUS
lshow(log_t *Log, int Offset, char *Title)
{
	char Indent[80];
	int  LogNum = (int) Log;

	/* If a small integer was specified for the log, then treat it */
	/* as an index into the log array.			       */
	if (LogNum >= 0  &&  LogNum < NUM_LOGS) {
		Log = logs[LogNum];
	}

	sprintf(Indent, "%*s", Offset, "");

	if (Title == NULL) {
		ffscMsg("%slog_t %s (%p):", Indent, Log->Name, Log);
	}
	else {
		ffscMsg("%s%s - log_t %s (%p):",
			Indent, Title, Log->Name, Log);
	}

	ffscMsg("%s    Flags 0x%08x   Type %d",
		Indent,
		Log->Flags, Log->Type);

	ffscMsg("%s    MaxSize %d   BufSize %d   BufAvail %d",
		Indent,
		Log->MaxSize, Log->BufSize, Log->BufAvail);
	ffscMsg("%s    Buf %p   BufStart %p   BufNext %p",
		Indent,
		Log->Buf, Log->BufStart, Log->BufNext);

	ffscMsg("%s    MaxLines %d   NumLines %d   StartLine %d   NextLine %d",
		Indent,
		Log->MaxLines, Log->NumLines, Log->StartLine, Log->NextLine);

	return OK;
}

#endif  /* !PRODUCTION */
