/*
 * consmisc.c
 *	Console task utility functions
 */

#include <vxworks.h>
#include <sys/types.h>

#include <iolib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ffsc.h"

#include "buf.h"
#include "console.h"
#include "misc.h"
#include "timeout.h"
#include "tty.h"

#ifndef PRODUCTION
#include "elsc.h"
#include "user.h"
#endif  /* !PRODUCTION */


/*
 * cmCheckTTYFlags
 *	Examines the TTY flags associated with the specified console_t's
 *	I/O buffers and updates the console_t's flags as needed. Returns
 *	0 if nothing changed, non-zero if something did.
 *	Kinda kludgy.
 */
int
cmCheckTTYFlags(console_t *Console)
{
	int i;
	int OldFlags = Console->Flags;
	int TTYFD;
	const ttyinfo_t *TTYInfo;

	for (i = 0;  i < FFSC_NUM_TTYS;  ++i) {
		TTYInfo = ttyGetInfo(i, &TTYFD);
		if (TTYInfo == NULL) {
			continue;
		}

		if (Console->UserInBuf->FD == TTYFD) {
			if (TTYInfo->CtrlFlags & TTYCF_NOFFSC) {
				Console->Flags |= CONF_NOFFSCUSER;
			}
			else {
				Console->Flags &= ~CONF_NOFFSCUSER;
			}

			if ((TTYInfo->CtrlFlags & TTYCF_OOBOK)  &&
			    !(ffscDebug.Flags & FDBF_NOOBPDBL))
			{
				Console->Flags |= CONF_OOBOKUSER;
			}
			else {
				Console->Flags &= ~CONF_OOBOKUSER;
			}
		}

		if (Console->SysInBuf->FD == TTYFD) {
			if (TTYInfo->CtrlFlags & TTYCF_NOFFSC) {
				Console->Flags |= CONF_NOFFSCSYS;
			}
			else {
				Console->Flags &= ~CONF_NOFFSCSYS;
			}

			if ((TTYInfo->CtrlFlags & TTYCF_OOBOK)  &&
			    !(ffscDebug.Flags & FDBF_NOOOBMSG))
			{			    
				Console->Flags |= CONF_OOBOKSYS;
			}
			else {
				Console->Flags &= ~CONF_OOBOKSYS;
			}
		}
	}

	return (Console->Flags != OldFlags);
}


/*
 * cmPagerUpdate
 *	Writes (additional) lines from the pagebuf associated with the
 *	specified console to the user output buffer.
 */
void
cmPagerUpdate(console_t *Console)
{
	char *BufEnd;
	char *LineEnd;
	char Response[MAX_FFSC_RESP_LEN_SINGLE + 1];
	int  LineLen;
	int  LinesPerPage;
	pagebuf_t *PageBuf;

	/* Calculate some convenient pointers */
	PageBuf = Console->PageBuf;
	BufEnd  = PageBuf->Buf + PageBuf->DataLen;

	/* If we are at the end of the buffer, bail out */
	if (PageBuf->Buf == BufEnd  ||  PageBuf->NumPrint == 0) {
		PageBuf->NumPrint = 0;
		return;
	}

	/* If the number of lines to be printed is negative, search back */
	/* in the buffer that many lines before the start of this page   */
	if (PageBuf->NumPrint < 0) {
		int i;
		int LinesPerPage;

		if (Console->LinesPerPage > 0) {
			LinesPerPage = Console->LinesPerPage;
		}
		else {
			LinesPerPage = ffscTune[TUNE_PAGE_DFLT_LINES];
		}

		PageBuf->NumPrint = -PageBuf->NumPrint;
		PageBuf->CurrLine -= (PageBuf->NumPrint + LinesPerPage);
		if (PageBuf->CurrLine < 0) {
			PageBuf->CurrLine = 0;
		}

		PageBuf->CurrPtr = PageBuf->Buf;
		for (i = 0;  i < PageBuf->CurrLine;  ++i) {
			PageBuf->CurrPtr = strstr(PageBuf->CurrPtr, STR_END);
			if (PageBuf->CurrPtr == NULL) {
				ffscMsg("cmPagerUpdate ran out of data "
					    "scrolling backward?");
				PageBuf->CurrPtr  = PageBuf->Buf;
				PageBuf->CurrLine = 0;
				PageBuf->NumPrint = 0;
				return;
			}
			PageBuf->CurrPtr += 2;
		}
	}

	/* Proceed until the pagebuf is empty, or the output buffer is full */
	while (PageBuf->NumPrint != 0) {

		/* Find the end of the current line */
		LineEnd = strstr(PageBuf->CurrPtr, STR_END);
		if (LineEnd == NULL) {
			if (*PageBuf->CurrPtr == '\0') {
				ffscMsg("Hit end of page buffer prematurely");
				PageBuf->NumPrint = 0;
				return;
			}
			LineEnd = BufEnd;
		}
		else {
			LineEnd += 2;
		}
		LineLen = LineEnd - PageBuf->CurrPtr;

		/* If the current line plus a trailer won't fit in the */
		/* output buffer, give up for now.		       */
		if (LineLen + 80 > Console->UserOutBuf->Avail) {
			break;
		}

		/* Write out the current line of data */
		if (bufWrite(Console->UserOutBuf, PageBuf->CurrPtr, LineLen)
		    != LineLen)
		{
			/* XXX Need to be more careful with return value? */
			break;
		}

		/* Update our records */
		PageBuf->CurrPtr = LineEnd;
		if (PageBuf->CurrPtr >= BufEnd) {
			PageBuf->NumPrint = 0;
		}
		else if (!(Console->Flags & CONF_NOPAGE)) {
			--PageBuf->NumPrint;
			++PageBuf->CurrLine;
		}
	}

	/* If paging is not actually turned on, we are finished */
	if (Console->Flags & CONF_NOPAGE) {
		if (PageBuf->NumPrint == 0) {
			cmSendFFSCCmd(Console, "R . EXIT", Response);
		}
		return;
	}

	/* If we haven't reached the end of a page, hold user input */
	/* for the time being, then wait on the output buffer	    */
	if (PageBuf->NumPrint != 0) {
		Console->Flags |= CONF_HOLDUSERIN;
		bufHold(Console->UserInBuf);
		return;
	}

	/* Otherwise, if we had held user input earlier, release it */
	else if (Console->Flags & CONF_HOLDUSERIN) {
		Console->Flags &= ~CONF_HOLDUSERIN;
		bufRelease(Console->UserInBuf);
	}

	/* Print a trailer *unless* we are at the end of the buffer  */
	/* and there were fewer lines to print than a page, in which */
	/* case we just exit pager mode.			     */
	if (Console->LinesPerPage < 1) {
		LinesPerPage = ffscTune[TUNE_PAGE_DFLT_LINES];
	}
	else {
		LinesPerPage = Console->LinesPerPage;
	}

	if (PageBuf->CurrPtr < BufEnd  ||  PageBuf->CurrLine >= LinesPerPage) {
		char Trailer[80], BackChar[20], FwdChar[20], QuitChar[20];

		ffscCtrlCharToString(Console->Pager[CONP_BACK], BackChar);
		ffscCtrlCharToString(Console->Pager[CONP_FWD],  FwdChar);
		ffscCtrlCharToString(Console->Pager[CONP_QUIT], QuitChar);
		sprintf(Trailer,
			"*** Type %s to continue, %s to go back, %s to quit "
			    "***",
			FwdChar, BackChar, QuitChar);

		bufWrite(Console->UserOutBuf, Trailer, strlen(Trailer));
	}
	else {
		cmSendFFSCCmd(Console, "R . EXIT", Response);
	}

	return;
}


/*
 * cmPrintMsg
 *	Prints an unsolicited message on the user console, queuing it
 *	or discarding it if necessary depending on the current echo
 *	mode, etc.
 */
void
cmPrintMsg(console_t *Console, char *Msg)
{
	int MsgLen;

	/* If unsolicited messages are being ignored, there's nothing to do */
	if (Console->EMsg == CONEM_OFF) {
		return;
	}

	/* Find out how long the message is for future reference */
	MsgLen = strlen(Msg);

	/* If the user output buffer is sitting at the beginning of */
	/* a line, write the message there directly.		    */
	if (Console->UserOutBuf->Flags & BUF_EOL) {
		bufWrite(Console->UserOutBuf, Msg, MsgLen);
	}

	/* Otherwise, save the message in the special message buffer */
	/* for processing later on when it's safe		     */
	else {
		bufWrite(Console->MsgBuf, Msg, MsgLen);
	}
}


/*
 * cmPrintResponse
 *	Prints a response message from an ELSC or FFSC command
 *	on the user device, doing any necessary filtering required
 *	by the current echo modes, etc.
 */
void
cmPrintResponse(console_t *Console, char *Msg)
{
	char *Curr;
	char Delims[2];
	char *Last;
	int  DidFirst;

	/* Don't need to bother with the message at all if RMsg */
	/* mode is off.						*/
	if (Console->RMsg == CONRM_OFF) {
		return;
	}

	/* Parse out the responses from individual entities */
	Delims[0] = Console->Delim[COND_RESPSEP];
	Delims[1] = '\0';
	Last = NULL;
	DidFirst = 0;
	for (Curr = strtok_r(Msg, Delims, &Last);
	     Curr != NULL;
	     Curr = strtok_r(NULL, Delims, &Last))
	{
		/* If only error responses are desired, make sure */
		/* the response is not simply "OK" (ignoring any  */
		/* identifying headers)				  */
		if (Console->RMsg == CONRM_ERROR) {
			char *ActualResponse;

			ActualResponse = strchr(Curr,
						Console->Delim[COND_HDR]);
			if (ActualResponse == NULL) {
				ActualResponse = Curr;
			}
			else {
				++ActualResponse;
			}

			if (strncmp(ActualResponse, "OK", 2) == 0  ||
			    strncmp(ActualResponse, "ok", 2) == 0  ||
			    strncmp(ActualResponse, "OFFLINE", 7) == 0)
			{
				continue;
			}
		}

		/* If this is the first line of the response and we are */
		/* in CONSOLE mode with CECHO OFF then we need to start	*/
		/* out on a new line.					*/
		if (Console->Mode == CONM_CONSOLE  &&
		    Console->CEcho == CONCE_OFF    &&
		    !DidFirst)
		{
			if (bufWrite(Console->UserOutBuf, STR_END, STR_END_LEN)
			    < 0)
			{
				return;
			}

			DidFirst = 1;
		}

		/* Write out the response on its own line */
		if (bufWrite(Console->UserOutBuf, Curr, strlen(Curr)) < 0) {
			return;
		}
		if (bufWrite(Console->UserOutBuf, STR_END, STR_END_LEN) < 0) {
			return;
		}
	}
}


/*
 * cmSendFFSCCmd
 *	Sends the specified command to the router and waits for a response,
 *	which is returned in the specified buffer.
 */
void
cmSendFFSCCmd(console_t *Console, char *Cmd, char *Response)
{
	char *PBCmd;

	/* Go send the command and get back the raw response */
	cmSendFFSCCmdNoPB(Console, Cmd, Response);

	/* Check for a piggybacked command in the FFSC response */
	PBCmd = strchr(Response, Console->Delim[COND_PB]);
	if (PBCmd != NULL) {
		char CmdCopy[MAX_CONS_CMD_LEN + 1];

		/* Make a copy of the command for strtok to destroy */
		strcpy(CmdCopy, PBCmd + 1);

		/* Say what's going on if desired */
		if (ffscDebug.Flags & FDBF_CONSOLE) {
			ffscMsg("MMSC piggybacked request: \"%s\"", CmdCopy);
		}

		/* Go deal with the command. Note that errors are ignored. */
		crProcessCmd(Console, CmdCopy);

		/* If the user doesn't want to see piggybacked commands */
		/* make it go away in the response			*/
		if (Console->RMsg != CONRM_VERBOSE) {
			*PBCmd = '\0';
		}
	}

	return;
}


/*
 * cmSendFFSCCmdNoPB
 *	A lite form of cmSendFFSCCmd that sends the command but doesn't
 *	process any piggybacked console commands.
 */
void
cmSendFFSCCmdNoPB(console_t *Console, char *Cmd, char *Response)
{
	char *TermPtr;
	int  CmdLen;
	int  NumMsgs;
	int  Result;
	int  WaitTime;

	/* Determine the length of the command */
	if (Cmd == NULL) {
		sprintf(Response, "ERROR NULL");
		return;
	}
	CmdLen = strlen(Cmd);

	/* Print a progress report if desired */
	if (ffscDebug.Flags & FDBF_CONSOLE) {
		ffscMsg("About to send %d bytes to MMSC: \"%s\"",
			CmdLen, Cmd);
	}

	/* Make sure there isn't anything left over in the ACK pipe */
	NumMsgs = 0;
	if (ioctl(Console->FDs[CCFD_C2RA], FIONMSGS, (int) &NumMsgs) != OK) {
		ffscMsgS("Unable to determine # of msgs in C2RA of console "
			     "%s",
			 Console->Name);
	}
	else if (NumMsgs > 0) {
		ffscMsg("Purging %d extraneous messages from C2RA of console "
			    "%s",
			NumMsgs, Console->Name);
		if (ioctl(Console->FDs[CCFD_C2RA], FIOFLUSH, 0) != OK) {
			ffscMsgS("WARNING: FIOFLUSH failed for C2RA of "
				     "console %s",
				 Console->Name);
		}
	}

	/* Write the command directly to the router port */
	if (timeoutWrite(Console->FDs[CCFD_C2RR],
			 Cmd,
			 CmdLen + 1,
			 ffscTune[TUNE_CO_ROUTER]) != CmdLen + 1)
	{
		ffscMsgS("%s unable to send message to MMSC via FD %d",
			 Console->Name, Console->FDs[CCFD_C2RR]);
		sprintf(Response, "ERROR WRITE: cmSendFFSCCmdNoPB");
		return;
	}

	/* Wait for a response from the router, and keep waiting until	*/
	/* the router tells us to stop					*/
	WaitTime = ffscTune[TUNE_CI_ROUTER_RESP];
	do {
		Result = timeoutRead(Console->FDs[CCFD_C2RA],
				     Response,
				     MAX_FFSC_RESP_LEN,
				     WaitTime);
		if (Result <= 0 ) {
			ffscMsgS("%s unable to read from MMSC via FD %d",
				 Console->Name, Console->FDs[CCFD_C2RA]);
			if (Result < 0  &&  errno == EINTR) {
				sprintf(Response, "ERROR TIMEOUT (cmSendFFSCCmdNoPB)");
			}
			else {
				sprintf(Response, "ERROR READ (cmSendFFSCCmdNoPB)");
			}
			return;
		}
		Response[Result] = '\0';

		/* A response of WAIT means the router is doing something */
		/* that takes a long time (e.g. flashing a new image). In */
		/* that case, arrange to wait "indefinitely" for a second */
		/* response.						  */
		if (strncmp(Response, "WAIT", 4) == 0) {
			char *Ptr;
			char *Token;

			/* If there is a token following WAIT, treat it */
			/* as a new wait time.				*/
			Ptr   = NULL;
			Token = strtok_r(Response + 4, " \t", &Ptr);
			if (Token != NULL) {
				WaitTime += strtol(Token, NULL, 0);
				Token = Ptr;
			}
			else {
				WaitTime = ffscTune[TUNE_CI_ROUTER_WAIT];
			}

			/* If there is anything left in the response, */
			/* treat it as an interim response message    */
			if (Token != NULL) {
				Token += strspn(Token, " \t");
				if (Token[0] != '\0') {
					cmPrintResponse(Console, Token);
				}
			}
		}
		else {
			WaitTime = 0;
		}
	} while (WaitTime != 0);

	/* Get rid of the annoying trailing CR/LF */
	TermPtr = strrchr(Response, FFSC_ACK_END_CHAR);
	if (TermPtr != NULL) {
		*TermPtr = '\0';
	}

	/* Print the FFSC response if desired */
	if (ffscDebug.Flags & FDBF_CONSOLE) {
		ffscMsg("MMSC Response: \"%s\"", Response);
	}

	return;
}


#ifndef PRODUCTION

/*----------------------------------------------------------------------*/
/*									*/
/*			  DEBUGGING FUNCTIONS				*/
/*									*/
/*----------------------------------------------------------------------*/

/*
 * consList
 *	Dump one or all of the console_t's
 */
STATUS
consList(int Arg)
{
	char Name[20];

	if (Arg >= 0  &&  Arg < NUM_CONSOLES) {
		sprintf(Name, "consoles[%d]", Arg);
		consShow(consoles[Arg], 0, Name);
	}
	else if (Arg == -1) {
		int i;

		for (i = 0;  i < NUM_CONSOLES;  ++i) {
			sprintf(Name, "consoles[%d]", i);
			consShow(consoles[i], 0, Name);
			ffscMsg("");
		}
	}
	else {
		ffscMsg("Specify value from 0 to %d, or -1 to show all",
			NUM_CONSOLES);
	}

	return OK;
}


/*
 * consShow
 *	Display information about the specified console_t
 */
STATUS
consShow(console_t *Console, int Offset, char *Title)
{
	char Indent[80];

	if (Console == NULL) {
		ffscMsg("Usage: consShow <addr> [<offset> [<title>]]");
		return OK;
	}

	sprintf(Indent, "%*s", Offset, "");


	if (Title == NULL) {
		ffscMsg("%sconsole_t %s (%p):",
			Indent,
			Console->Name, Console);
	}
	else {
		ffscMsg("%s%s - console_t %s (%p):",
			Indent,
			Title, Console->Name, Console);
	}

	ffscMsg("%s    Type %d   Mode %d   BaseMode %d  PrevMode %d "
		    "Flags 0x%08x",
		Indent,
		Console->Type, Console->Mode, Console->BaseMode,
		Console->PrevMode, Console->Flags);
	ffscMsg("%s    SysState %d   SysOffset %d   UserState %d   "
		    "UserOffset %d",
		Indent,
		Console->SysState, Console->SysOffset,
		Console->UserState, Console->UserOffset);
	ffscMsg("%s    CEcho %d   EMsg %d   RMsg %d",
		Indent,
		Console->CEcho, Console->EMsg, Console->RMsg);
	ffscMsg("%s    NVRAMID %d   WakeUp %d   ProgressCount %d",
		Indent,
		Console->NVRAMID, Console->WakeUp, Console->ProgressCount);
	ffscMsg("%s    LinesPerPage %d   PageBuf %p",
		Indent,
		Console->LinesPerPage, Console->PageBuf);

	if (Console->User != NULL) {
		ffscMsg("%s    User @%p  \"%s\"",
			Indent,
			Console->User, Console->User->Name);
	}

	ffscMsg("%s    FDs: SYS %d  USER %d  C2RR %d  C2RA %d  R2C %d  REM %d "
		    "PROG %d",
		Indent,
		Console->FDs[CCFD_SYS],
		Console->FDs[CCFD_USER],
		Console->FDs[CCFD_C2RR],
		Console->FDs[CCFD_C2RA],
		Console->FDs[CCFD_R2C],
		Console->FDs[CCFD_REMOTE],
		Console->FDs[CCFD_PROGRESS]);

	ffscMsg("%s    Ctrl: BS %03o  ESC %03o  END %03o  KILL %03o  "
		    "EXIT %03o",
		Indent,
		(unsigned char) Console->Ctrl[CONC_BS],
		(unsigned char) Console->Ctrl[CONC_ESC],
		(unsigned char) Console->Ctrl[CONC_END],
		(unsigned char) Console->Ctrl[CONC_KILL],
		(unsigned char) Console->Ctrl[CONC_EXIT]);

	ffscMsg("%s    Delim: PB %03o  HDR %03o  ELSC %03o  RESPSEP %03o  "
		    "OBP %03o",
		Indent,
		(unsigned char) Console->Delim[COND_PB],
		(unsigned char) Console->Delim[COND_HDR],
		(unsigned char) Console->Delim[COND_ELSC],
		(unsigned char) Console->Delim[COND_RESPSEP],
		(unsigned char) Console->Delim[COND_OBP]);

	ffscMsg("%s    Pager: FWD %03o  BACK %03o  QUIT %03o",
		Indent,
		(unsigned char) Console->Pager[CONP_FWD],
		(unsigned char) Console->Pager[CONP_BACK],
		(unsigned char) Console->Pager[CONP_QUIT]);

	if (Console->SysInBuf != NULL) {
		bufShow(Console->SysInBuf, Offset + 4, "SysInBuf");
	}
	if (Console->SysOutBuf != NULL) {
		bufShow(Console->SysOutBuf, Offset + 4, "SysOutBuf");
	}
	if (Console->UserInBuf != NULL) {
		bufShow(Console->UserInBuf, Offset + 4, "UserInBuf");
	}
	if (Console->UserOutBuf != NULL) {
		bufShow(Console->UserOutBuf, Offset + 4, "UserOutBuf");
	}
	if (Console->MsgBuf != NULL) {
		bufShow(Console->MsgBuf, Offset + 4, "MsgBuf");
	}

	if (Console->ELSC != NULL) {
		elscShow(Console->ELSC, Offset + 4, "ELSC");
	}

	return OK;
}

#endif  /* !PRODUCTION */
