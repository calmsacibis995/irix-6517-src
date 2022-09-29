/*
 * consuser.c
 *	Functions for handling user input to a console
 */

#include <vxworks.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ffsc.h"

#include "buf.h"
#include "cmd.h"
#include "console.h"
#include "misc.h"
#include "timeout.h"
#include "user.h"
#include "iolib.h"


/* Internal functions */
static STATUS cuProcessBS(console_t *, char *, int);
static STATUS cuProcessKILL(console_t *, char *, int);
static STATUS cuUserInConsole(console_t *);
static STATUS cuUserInELSC(console_t *);
static STATUS cuUserInFFSC(console_t *);
static STATUS cuUserInCopyUser(console_t *);
static STATUS cuUserInCopySys(console_t *);
static STATUS cuUserInRAT(console_t *);
static STATUS cuUserInPager(console_t *);


/*
 * cuUserIn
 *	Reads from the user port and sends the input to the appropriate
 *	destination(s). Returns 0 if successful, -1 if the user port is
 *	dead.
 */
STATUS
cuUserIn(console_t *Console)
{
	int Result = 0;

	/* Proceed according to the current mode */
	switch (Console->Mode) {

	    case CONM_CONSOLE:
		Result = cuUserInConsole(Console);
		break;

	    case CONM_FFSC:
		Result = cuUserInFFSC(Console);
		break;

	    case CONM_ELSC:
		Result = cuUserInELSC(Console);
		break;

	    case CONM_COPYUSER:
	    case CONM_MODEM:
		Result = cuUserInCopyUser(Console);
		break;

	    case CONM_COPYSYS:
		Result = cuUserInCopySys(Console);
		break;

	    case CONM_RAT:
		Result = cuUserInRAT(Console);
		break;

	    case CONM_PAGER:
		Result = cuUserInPager(Console);
		break;

	    default:
		ffscMsg("cuUserIn called for %s while in mode %d",
			Console->Name, Console->Mode);
		Result = ERROR;
	}

	return Result;
}




/*----------------------------------------------------------------------*/
/*									*/
/*			     INTERNAL FUNCTIONS				*/
/*									*/
/*----------------------------------------------------------------------*/

/*
 * cuProcessBS
 *	Process a single backspace character at the specified location in
 *	the user input buffer of the specified console. It is assumed to
 *	have been encountered while gathering a command. If Echo is
 *	non-zero, echo the results to the user output.
 */
STATUS
cuProcessBS(console_t *Console, char *BSPtr, int Echo)
{
	int MoveLen;

	/* If the command buffer is empty, just get rid of the	*/
	/* backspace character, then we are done.		*/
	if (Console->UserOffset < 1) {
		bufLeftAlign(Console->UserInBuf, BSPtr + 1);
		return OK;
	}

	/* Move buffer contents over */
	MoveLen = (Console->UserInBuf->Len - Console->UserOffset) - 1;
	if (MoveLen > 0) {
		memmove(BSPtr - 1, BSPtr + 1, MoveLen);
	}

	/* Adjust counters, etc. */
	--Console->UserOffset;
	Console->UserInBuf->Len   -= 2;
	Console->UserInBuf->Curr  -= 2;
	Console->UserInBuf->Avail += 2;

	/* If no echoing is required, we are done */
	if (!Echo) {
		return OK;
	}

	/* Send a backspace character sequence to the user output device */
	return bufWrite(Console->UserOutBuf, STR_BS, STR_BS_LEN);
}


/*
 * cuProcessKILL
 *	Process a kill character at the specified location in
 *	the user input buffer of the specified console. It is assumed to
 *	have been encountered while gathering a command. If Echo is
 *	non-zero, echo the results to the user output.
 */
STATUS
cuProcessKILL(console_t *Console, char *KillPtr, int Echo)
{
	/* Empty previous chars from buffer */
	bufLeftAlign(Console->UserInBuf, KillPtr + 1);
	Console->UserOffset = 0;

	/* If we don't need to echo anything, we are done */
	if (!Echo) {
		return OK;
	}

	/* Echo a "cancel" string */
	return bufWrite(Console->UserOutBuf, STR_CMDCANCEL, STR_CMDCANCEL_LEN);
}


/*
 * cuUserInConsole
 *	Process input from a user port while in Console mode
 */
STATUS
cuUserInConsole(console_t *Console)
{
	buf_t *InBuf = Console->UserInBuf;
	char  Interesting[CONC_MAXCTL];
	char  *Ptr;
	char  *Start;
	int   Next;
	int   PrefixLen;

	/* Proceed until we have exhausted the input buffer */
	while (InBuf->Len > 0) {

		/* Proceed according to our current state */
		switch (Console->UserState) {

		    case CONUM_NORMAL:
			/* We are gathering ordinary input. Watch for	*/
			/* escape & OBP characters, and copy everything */
			/* else on to the system.			*/

			/* Look for interesting characters */
			Next = 0;
			if (!(Console->Flags & CONF_NOFFSCUSER)) {
				Interesting[Next] = Console->Ctrl[CONC_ESC];
				++Next;
			}
			if (Console->Flags & CONF_OOBOKUSER) {
				Interesting[Next] = Console->Delim[COND_OBP];
				++Next;
			}

			Interesting[Next] = '\0';
			InBuf->Buf[InBuf->Len] = '\0';
			Ptr = strpbrk(InBuf->Buf, Interesting);

			/* If none, we can copy over the entire buffer */
			if (Ptr == NULL) {
				int RC;

				RC = bufAppendBuf(Console->SysOutBuf, InBuf);

				if (RC < 0) {
					return ERROR;
				}
			}

			/* Otherwise, copy everything up to the escape/OBP */
			else {
				char InterestingChar = *Ptr;
				int OutLen = Ptr - InBuf->Buf;
				int RC = 0;

				/* Say what's going on if desired */
				if (ffscDebug.Flags & FDBF_CONSOLE) {
					ffscMsg("%s got ESC/OBP - flushing %d "
						    "bytes",
						Console->Name, OutLen);
				}

				/* If there is anything in the buffer */
				/* before the escape, push it out     */
				if (OutLen > 0) {
					RC = bufWrite(Console->SysOutBuf,
						      InBuf->Buf,
						      OutLen);
				}

				/* Adjust the buffer past the esc char */
				bufLeftAlign(InBuf,
					     InBuf->Buf + OutLen + 1);

				/* If the interesting character was an */
				/* Out of Band Prefix, simply echo it  */
				/* twice then we are done.	       */
				if (InterestingChar ==
				    Console->Delim[COND_OBP])
				{
					/* Bail now if earlier write failed */
					if (RC < 0) {
						return RC;
					}

					/* Say what we are doing if desired */
					if (ffscDebug.Flags & FDBF_CONSOLE) {
						ffscMsg("%s doubling OBP char",
							Console->Name);
					}

					/* Echo the OBPs */
					if (bufWrite(Console->SysOutBuf,
						     OBP_STR OBP_STR,
						     2) != OK)
					{
						return ERROR;
					}

					/* Resume at the top of the loop */
					break;
				}

				/* Remember that we have seen an escape */
				Console->UserState = CONUM_GOTESC;

				/* If we are not in delayed prompt mode */
				/* arrange for a prompt & echoing (but  */
				/* only if CEcho mode is on).		*/
				if (Console->CEcho == CONCE_ON  &&
				    !(Console->User->Options & UO_DELAYPROMPT))
				{

					/* Tell the system to be quiet */
					bufHold(Console->SysInBuf);
					Console->Flags |= CONF_HOLDSYSIN;

					/* Arrange for a prompt */
					Console->Flags |= CONF_SHOWPROMPT;

					/* Arrange for input to be echoed */
					Console->Flags |= CONF_ECHO;
				}

				/* Bail out if the first write failed */
				if (RC < 0) {
					return RC;
				}
			}

			break;


		    case CONUM_GOTESC:
			/* The last thing we saw was an escape char. Look */
			/* at what comes next and proceed accordingly.	  */

			/* If the next char is another escape, echo a */
			/* single escape character and go on.	      */
			if (InBuf->Buf[0] == Console->Ctrl[CONC_ESC]) {
				int RC;

				/* If we are not in delayed prompt mode */
				/* then leave escape mode		*/
				if (!(Console->User->Options & UO_DELAYPROMPT))
				{
					/* (Try to) erase the prompt */
					if (Console->CEcho == CONCE_ON) {
						bufWrite(Console->UserOutBuf,
							STR_KILLESCPROMPT,
							STR_KILLESCPROMPT_LEN);
					}

					/* No longer need to echo input */
					Console->Flags &= ~CONF_ECHO;

					/* If we held the system input	  */
					/* buffer, it can now be released */
					if (Console->Flags & CONF_HOLDSYSIN) {
						bufRelease(Console->SysInBuf);
						Console->Flags &=
						    ~CONF_HOLDSYSIN;
					}
				}

				/* Say what we are doing if desired */
				if (ffscDebug.Flags & FDBF_CONSOLE) {
					ffscMsg("%s echoing ESC char",
						Console->Name);
				}

				/* Go write out the lone escape character */
				RC = bufWrite(Console->SysOutBuf,
					      &Console->Ctrl[CONC_ESC],
					      1);

				/* Clean up the input buffer */
				bufLeftAlign(InBuf, InBuf->Buf + 1);

				/* Return to normal input mode */
				Console->UserState = CONUM_NORMAL;

				/* Bail out if the write failed */
				if (RC < 0) {
					return ERROR;
				}

				continue;
			}

			/* Otherwise, enter "CONUM_CMD" mode and start	*/
			/* collecting an FFSC command.			*/
			Console->UserState  = CONUM_CMD;
			Console->UserOffset = 0;
			if (ffscDebug.Flags & FDBF_CONSOLE) {
				ffscMsg("%s collecting MMSC command...",
					Console->Name);
			}

			/* Go print a prompt and arrange for echoing  */
			/* if we are in delayed prompt mode and CEcho */
			/* mode is ON.				      */
			if ((Console->User->Options & UO_DELAYPROMPT) &&
			    Console->CEcho == CONCE_ON)
			{

				/* Tell the system to be quiet */
				bufHold(Console->SysInBuf);
				Console->Flags |= CONF_HOLDSYSIN;

				/* Arrange for input to be echoed */
				Console->Flags |= CONF_ECHO;
				
#if 0
				/* Flush the output Buffer */
ttyinfo();
				if (ioctl(Console->UserOutBuf->FD, FIOWFLUSH, 0) < 0)
					ffscMsg("Unable to Flush output buffer on FD %d", 
								Console->UserOutBuf->FD);
ttyinfo();
#endif

				/* Print the prompt now */
				(void) bufWrite(Console->UserOutBuf,
						STR_ESCPROMPT,
						STR_ESCPROMPT_LEN);
			}

			break;


		    case CONUM_CMD:
			/* We are currently in the middle of a command;	*/
			/* Keep accumulating until we hit the end. If	*/
			/* we are in cecho mode, it may also necessary	*/
			/* to echo & handle control characters.		*/

			/* Build a list of "interesting" characters. We */
			/* don't care about EXIT, so replace it with	*/
			/* one of the others so strpbrk still works.	*/
			strcpy(Interesting, Console->Ctrl);
			Interesting[CONC_EXIT] = Interesting[CONC_ESC];

			/* Look for an interesting character, adding any */
			/* chars that come before it to the command.	 */
			/* Note that we have to manually null-terminate  */
			/* the input data for the sake of strpbrk.	 */
			InBuf->Buf[InBuf->Len] = '\0';
			Start = InBuf->Buf + Console->UserOffset;
			Ptr   = strpbrk(Start, Interesting);
			if (Ptr == NULL) {
				Ptr = &InBuf->Buf[InBuf->Len];
			}
			PrefixLen = Ptr - Start;
			Console->UserOffset += PrefixLen;

			/* If there is no more room in the input buffer	*/
			/* and there are no interesting characters,	*/
			/* replace the last character with the END char */
			/* or else we will deadlock.			*/
			if (Console->UserInBuf->Avail == 0  &&
			    *Ptr == '\0')
			{
				--PrefixLen;
				--Console->UserOffset;
				--Ptr;
				*Ptr = Console->Ctrl[CONC_END];
			}

			/* If desired, echo anything that appears before */
			/* the "interesting character" (if any)		 */
			if ((Console->Flags & CONF_ECHO)  &&  PrefixLen > 0) {
				if (bufWrite(Console->UserOutBuf,
					     Start,
					     PrefixLen) < 0)
				{
					return ERROR;
				}
			}

			/* If there are no interesting characters, then */
			/* we don't have a complete command yet, so we  */
			/* are finished for now.			*/
			if (*Ptr == '\0') {
				return OK;
			}

			/* If an escape or kill char is encountered, */
			/* clear the command buffer and start over.  */
			if (*Ptr == Console->Ctrl[CONC_ESC]  ||
			    *Ptr == Console->Ctrl[CONC_KILL])
			{
				if (cuProcessKILL(Console,
						  Ptr,
						  (Console->Flags & CONF_ECHO))
				    != OK)
				{
					return ERROR;
				}

				continue;
			}

			/* If a backspace char was found, back up one char */
			/* (unless the command is currently empty)	   */
			else if (*Ptr == Console->Ctrl[CONC_BS]) {
				if (cuProcessBS(Console,
						Ptr,
						(Console->Flags & CONF_ECHO))
				    != OK)
				{
					return ERROR;
				}					
			}

			/* If we found the end of the command, send it	*/
			/* off to the FFSC then clean up		*/
			else if (*Ptr == Console->Ctrl[CONC_END]) {
				char Response[MAX_FFSC_RESP_LEN + 1];

				/* Echo the "ENTER" key if necessary */
				if (Console->CEcho == CONCE_ON) {
					bufWrite(Console->UserOutBuf,
						 STR_END,
						 STR_END_LEN);
				}

				/* Mark the end of the command */
				*Ptr = '\0';

				/* Go deal with the command itself */
				cmSendFFSCCmd(Console, InBuf->Buf, Response);

				/* Tend to some clean up tasks if the	*/
				/* mode is unchanged. (If the mode DID	*/
				/* change, this will be handled by the	*/
				/* console task itself.			*/
				if (Console->Mode == CONM_CONSOLE) {

					/* Skip over the now-finished cmd */
					cmPrintResponse(Console, Response);
					bufLeftAlign(InBuf, Ptr + 1);

					/* Return to the normal state */
					Console->UserState = CONUM_NORMAL;
				}

				/* Additional cleanup */
				Console->UserOffset = 0;
				Console->Flags &= ~CONF_ECHO;

				/* If we held the system input buffer,	*/
				/* it can now be released		*/
				if (Console->Flags & CONF_HOLDSYSIN) {
					bufRelease(Console->SysInBuf);
					Console->Flags &= ~CONF_HOLDSYSIN;
				}
			}

			break;


		    default:
			/* Don't know this state! */
			ffscMsg("Unknown state for %s in cuUserInConsole: %d",
				Console->Name, Console->UserState);
			return ERROR;
		}
	}

	return OK;
}


/*
 * cuUserInELSC
 *	Process input from a user port while in ELSC mode. This is
 *	nearly identical to remote mode for our purposes here.
 */
STATUS
cuUserInELSC(console_t *Console)
{
	buf_t *InBuf = Console->UserInBuf;
	char  *Ptr;
	char  Response[MAX_FFSC_RESP_LEN + 1];
	int   PrefixLen;

	/* Make sure we are really in the right state */
	if (Console->UserState != CONUM_NORMAL) {
		ffscMsg("cuUserInELSC called for %s while in state %d",
			Console->Name, Console->UserState);
		return ERROR;
	}

	/* Look for an exit character, sending any chars */
	/* that come before it to the remote system.	 */
	/* Note that we have to manually null-terminate  */
	/* the input data for the sake of strchr.	 */
	InBuf->Buf[InBuf->Len] = '\0';
	Ptr = strchr(InBuf->Buf, Console->Ctrl[CONC_EXIT]);
	if (Ptr == NULL) {
		Ptr = &InBuf->Buf[InBuf->Len];
	}
	PrefixLen = Ptr - InBuf->Buf;

	/* Send anything that came before the exit char to the remote.	*/
	/* We ignore errors since we are about to shut down anyway.	*/
	if (PrefixLen > 0) {
		(void) bufWrite(Console->SysOutBuf, InBuf->Buf, PrefixLen);
	}

	/* If there is no exit character, then we are finished for now */
	if (*Ptr == '\0') {
		bufClear(InBuf);
		return OK;
	}

	/* Fake up an exit command and print the response */
	if (ffscDebug.Flags & FDBF_CONSOLE) {
		ffscMsg("%s received exit character in MSC mode",
			Console->Name);
	}
	cmSendFFSCCmd(Console, "R . EXIT", Response);
	cmPrintResponse(Console, Response);

	/* Mode change will purge anything else in the input buffer */

	return OK;
}


/*
 * cuUserInFFSC
 *	Process input from a user port while in FFSC mode
 */
STATUS
cuUserInFFSC(console_t *Console)
{
	buf_t *InBuf = Console->UserInBuf;
	char  Interesting[CONC_MAXCTL];
	char  *Ptr;
	char  Response[MAX_FFSC_RESP_LEN + 1];
	char  *Start;
	int   PrefixLen;

	/* Make sure we are really in the right state */
	if (Console->UserState != CONUM_NORMAL) {
		ffscMsg("cuUserInFFSC called for %s while in state %d",
			Console->Name, Console->UserState);
		return ERROR;
	}

	/* Build a list of "interesting" characters, mainly all of the  */
	/* control characters except ESC (we'll replace it with another */
	/* arbitrary control character so strpbrk still works)		*/
	strcpy(Interesting, Console->Ctrl);
	Interesting[CONC_ESC] = Interesting[CONC_EXIT];

	/* Proceed until we have exhausted the input buffer */
	while (InBuf->Len > 0) {

		/* Look for an interesting character, adding any */
		/* chars that come before it to the command.	 */
		/* Note that we have to manually null-terminate  */
		/* the input data for the sake of strpbrk.	 */
		InBuf->Buf[InBuf->Len] = '\0';
		Start = InBuf->Buf + Console->UserOffset;
		Ptr   = strpbrk(Start, Interesting);
		if (Ptr == NULL) {
			Ptr = &InBuf->Buf[InBuf->Len];
		}
		PrefixLen = Ptr - Start;
		Console->UserOffset += PrefixLen;

		/* Echo anything that appears before the interesting */
		/* character (if any)				     */
		if (PrefixLen > 0) {
			if (bufWrite(Console->UserOutBuf, Start, PrefixLen)
			    < 0)
			{
				return ERROR;
			}
		}

		/* If there are no interesting characters, then */
		/* we don't have a complete command yet, so we  */
		/* are finished for now.			*/
		if (*Ptr == '\0') {
			return OK;
		}

		/* If a kill char is encountered, clear the */
		/* command buffer and start over.	    */
		if (*Ptr == Console->Ctrl[CONC_KILL]) {
			if (cuProcessKILL(Console, Ptr, 1) != OK) {
				return ERROR;
			}

			continue;
		}

		/* If a backspace char was found, back up one char */
		/* (unless the command is currently empty)	   */
		else if (*Ptr == Console->Ctrl[CONC_BS]) {
			if (cuProcessBS(Console, Ptr, 1) != OK) {
				return ERROR;
			}
		}

		/* If an exit char was found, fake an "exit" command */
		else if (*Ptr == Console->Ctrl[CONC_EXIT]) {

			/* Say what's happening if desired */
			if (ffscDebug.Flags & FDBF_CONSOLE) {
				ffscMsg("%s received exit char in MMSC mode",
					Console->Name);
			}

			/* Fake up an exit command and print the response */
			cmSendFFSCCmd(Console, "R . EXIT", Response);
			cmPrintResponse(Console, Response);

			/* Mode change will purge rest of input buffer */

			/* Tidy up and arrange for a new prompt */
			Console->UserOffset = 0;
			Console->Flags |= CONF_SHOWPROMPT;
		}

		/* If we found the end of the command, send it	*/
		/* off to the FFSC then clean up		*/
		else if (*Ptr == Console->Ctrl[CONC_END]) {

			/* Mark the end of the command */
			*Ptr = '\0';

			/* "Echo" the END key */
			bufWrite(Console->UserOutBuf, STR_END, STR_END_LEN);

			/* Go deal with the command itself, ignoring a  */
			/* leading ESC character if present.		*/
			if (InBuf->Buf[0] == Console->Ctrl[CONC_ESC]) {
				cmSendFFSCCmd(Console, InBuf->Buf+1, Response);
			}
			else {
				cmSendFFSCCmd(Console, InBuf->Buf, Response);
			}

			/* Skip over the now-finished command unless	*/
			/* the mode has changed (in which case this is	*/
			/* handled by the mode-change itself)		*/
			if (Console->Mode == CONM_FFSC) {
				cmPrintResponse(Console, Response);
				bufLeftAlign(InBuf, Ptr + 1);
				Console->Flags |= CONF_SHOWPROMPT;
			}
			Console->UserOffset = 0;
		}
	}

	return OK;
}


/*
 * cuUserInCopyUser
 *	Process input from a user port while in CopyUser mode, meaning
 *	we echo virtually everything from the user port to the system
 *	port (which is usually on a remote FFSC).
 */
STATUS
cuUserInCopyUser(console_t *Console)
{
	buf_t *InBuf = Console->UserInBuf;
	char  *Ptr;
	char  Response[MAX_FFSC_RESP_LEN + 1];
	int   PrefixLen;

	/* Make sure we are really in the right state */
	if (Console->UserState != CONUM_NORMAL) {
		ffscMsg("cuUserInCopyUser called for %s while in state %d",
			Console->Name, Console->UserState);
		return ERROR;
	}

	/* If we have been told to ignore exit chars, then simply copy	*/
	/* the input to output, then we are done.			*/
	if (Console->Flags & CONF_IGNOREEXIT) {
		return bufAppendBuf(Console->SysOutBuf, Console->UserInBuf);
	}

	/* Look for an exit character, sending any chars */
	/* that come before it to the remote system.	 */
	/* Note that we have to manually null-terminate  */
	/* the input data for the sake of strchr.	 */
	InBuf->Buf[InBuf->Len] = '\0';
	Ptr = strchr(InBuf->Buf, Console->Ctrl[CONC_EXIT]);
	if (Ptr == NULL) {
		Ptr = &InBuf->Buf[InBuf->Len];
	}
	PrefixLen = Ptr - InBuf->Buf;

	/* Send anything that came before the exit char to the remote.	*/
	/* We ignore errors since we are about to shut down anyway.	*/
	if (PrefixLen > 0) {
		(void) bufWrite(Console->SysOutBuf, InBuf->Buf, PrefixLen);
	}

	/* If there is no exit character, then we are finished for now */
	if (*Ptr == '\0') {
		bufClear(InBuf);
		return OK;
	}

	/* Fake up an exit command and print the response */
	if (ffscDebug.Flags & FDBF_CONSOLE) {
		ffscMsg("%s received exit character in COPYUSER mode",
			Console->Name);
	}
	cmSendFFSCCmd(Console, "R . EXIT", Response);
	cmPrintResponse(Console, Response);

	/* Mode change will purge remainder of input buffer */

	return OK;
}


/*
 * cuUserInCopySys
 *	Process input from a user port while in CopySys mode, meaning
 *	we echo virtually everything from the "user" port (which is probably
 *	really the BASEIO) to the system port (which is usually on a remote
 *	FFSC). Unlike CopyUser mode, we don't watch for an EXIT character.
 *	Instead, we rely entirely on the remote side sending us an EOF.
 */
STATUS
cuUserInCopySys(console_t *Console)
{
	/* Make sure we are really in the right state */
	if (Console->UserState != CONUM_NORMAL) {
		ffscMsg("cuUserInCopySys called for %s while in state %d",
			Console->Name, Console->UserState);
		return ERROR;
	}

	/* Handle a progress indicator if desired */
	if (Console->Flags & CONF_PROGRESS  &&
	    Console->FDs[CCFD_PROGRESS] >= 0)
	{
		Console->ProgressCount += Console->UserInBuf->Len;
		if (Console->ProgressCount > ffscTune[TUNE_PROGRESS_INTERVAL])
		{
			timeoutWrite(Console->FDs[CCFD_PROGRESS],
				     ".",
				     1,
				     ffscTune[TUNE_CO_PROGRESS]);
			Console->ProgressCount -=
			    ffscTune[TUNE_PROGRESS_INTERVAL];
		}
	}

	/* Copy everything we have just read to the other side */
	if (bufAppendBuf(Console->SysOutBuf, Console->UserInBuf) != OK) {
		return ERROR;
	}

	/* Clear out the input buffer now that it has been copied */
	bufClear(Console->UserInBuf);

	return OK;
}


/*
 * cuUserInRAT
 *	Process input from a user port while in RAT mode. RAT mode looks
 *	like CONSOLE mode with cecho turned off (e.g. you have to type an
 *	ESC char before commands), but the commands are processed as FFSC
 *	commands. Everything else is silently discarded. This is useful
 *	for CSD's Remote Access Tool since it looks very much like a native
 *	ELSC.
 */
STATUS
cuUserInRAT(console_t *Console)
{
	buf_t *InBuf = Console->UserInBuf;
	char  *Ptr;
	char  *Start;
	int   PrefixLen;

	/* Proceed until we have exhausted the input buffer */
	while (InBuf->Len > 0) {

		/* Proceed according to our current state */
		switch (Console->UserState) {

		    case CONUM_NORMAL:
			/* We are gathering ordinary input. Watch for	*/
			/* escape characters, and discard all else.	*/

			/* Look for interesting characters */
			Ptr = strchr(InBuf->Buf, Console->Ctrl[CONC_ESC]);

			/* If none, we can discard everything */
			if (Ptr == NULL) {
				bufClear(InBuf);
				return OK;
			}

			/* Otherwise, determine what to discard */
			PrefixLen = Ptr - InBuf->Buf;

			/* Say what's going on if desired */
			if (ffscDebug.Flags & FDBF_CONSOLE) {
				ffscMsg("%s got ESC - flushing %d bytes",
					Console->Name, PrefixLen);
			}

			/* Discard everything up to and including the ESC */
			bufLeftAlign(InBuf, InBuf->Buf + PrefixLen + 1);

			/* Start collecting the command */
			Console->UserState  = CONUM_CMD;
			Console->UserOffset = 0;
			if (ffscDebug.Flags & FDBF_CONSOLE) {
				ffscMsg("%s collecting MMSC command...",
					Console->Name);
			}

#ifdef RAT_USES_CECHO
			/* If cecho mode is on, arrange for a prompt & echo */
			if (Console->CEcho == CONCE_ON) {
				Console->Flags |= CONF_SHOWPROMPT | CONF_ECHO;
			}
#endif

			break;


		    case CONUM_CMD:
			/* We are currently in the middle of a command;	*/
			/* Keep accumulating until we hit the end. If	*/
			/* we are in cecho mode, it may also necessary	*/
			/* to echo & handle control characters.		*/

			/* Look for an interesting character, adding any */
			/* chars that come before it to the command.	 */
			/* Note that we have to manually null-terminate  */
			/* the input data for the sake of strpbrk.	 */
			InBuf->Buf[InBuf->Len] = '\0';
			Start = InBuf->Buf + Console->UserOffset;
			Ptr   = strpbrk(Start, Console->Ctrl);
			if (Ptr == NULL) {
				Ptr = &InBuf->Buf[InBuf->Len];
			}
			PrefixLen = Ptr - Start;
			Console->UserOffset += PrefixLen;

			/* If there is no more room in the input buffer	*/
			/* and there are no interesting characters,	*/
			/* replace the last character with the END char */
			/* or else we will deadlock.			*/
			if (Console->UserInBuf->Avail == 0  &&
			    *Ptr == '\0')
			{
				--PrefixLen;
				--Console->UserOffset;
				--Ptr;
				*Ptr = Console->Ctrl[CONC_END];
			}

			/* If desired, echo anything that appears before */
			/* the "interesting character" (if any)		 */
			if ((Console->Flags & CONF_ECHO)  &&  PrefixLen > 0) {
				if (bufWrite(Console->UserOutBuf,
					     Start,
					     PrefixLen) < 0)
				{
					return ERROR;
				}
			}

			/* If there are no interesting characters, then */
			/* we don't have a complete command yet, so we  */
			/* are finished for now.			*/
			if (*Ptr == '\0') {
				return OK;
			}

			/* If an escape or kill char is encountered, */
			/* clear the command buffer and start over.  */
			if (*Ptr == Console->Ctrl[CONC_ESC]  ||
			    *Ptr == Console->Ctrl[CONC_KILL])
			{
				if (cuProcessKILL(Console,
						  Ptr,
						  (Console->Flags & CONF_ECHO))
				    != OK)
				{
					return ERROR;
				}

				continue;
			}

			/* If a backspace char was found, back up one char */
			/* (unless the command is currently empty)	   */
			else if (*Ptr == Console->Ctrl[CONC_BS]) {
				if (cuProcessBS(Console,
						Ptr,
						(Console->Flags & CONF_ECHO))
				    != OK)
				{
					return ERROR;
				}					
			}

			/* If we found the end of the command, send it	*/
			/* off to the FFSC then clean up		*/
			else if (*Ptr == Console->Ctrl[CONC_END]) {
				char Response[MAX_FFSC_RESP_LEN + 1];

#ifdef RAT_USES_CECHO
				/* Echo the "ENTER" key if necessary */
				if (Console->CEcho == CONCE_ON) {
					bufWrite(Console->UserOutBuf,
						 STR_END,
						 STR_END_LEN);
				}
#endif

				/* Mark the end of the command */
				*Ptr = '\0';

				/* Go deal with the command itself */
				cmSendFFSCCmd(Console, InBuf->Buf, Response);

				/* Tend to some clean up tasks if the	*/
				/* mode is unchanged. (If the mode DID	*/
				/* change, this will be handled by the	*/
				/* console task itself.			*/
				if (Console->Mode == CONM_RAT) {

					/* Print the response message */
					cmPrintResponse(Console, Response);

					/* Skip over the now-finished cmd */
					bufLeftAlign(InBuf, Ptr + 1);

					/* Return to the normal state */
					Console->UserState = CONUM_NORMAL;
				}

				/* Additional cleanup */
				Console->UserOffset = 0;
				Console->Flags &= ~CONF_ECHO;
			}

			break;


		    default:
			/* Don't know this state! */
			ffscMsg("Unknown state for %s in cuUserInRAT: %d",
				Console->Name, Console->UserState);
			return ERROR;
		}
	}

	return OK;
}


/*
 * cuUserInPager
 *	Process input from a user port while in Pager mode. Here, we
 *	simply read one character and treat it as a pager control character.
 *	If it is not recognized, it is simply discarded. If the buffer
 *	happens to contain more than one character, we won't read the
 *	rest until our next visit (in case HOLDUSERIN gets set).
 */
STATUS
cuUserInPager(console_t *Console)
{
	buf_t *InBuf = Console->UserInBuf;
	char  Command;
	char  Response[MAX_FFSC_RESP_LEN_SINGLE + 1];

	/* Make sure we are really in the right state */
	if (Console->UserState != CONUM_NORMAL) {
		ffscMsg("cuUserInPager called for %s while in state %d",
			Console->Name, Console->UserState);
		return ERROR;
	}

	/* Bail out if there is nothing to read */
	if (Console->UserInBuf->Len < 1) {
		ffscMsg("cuUserInPager called with empty buffer");
		return ERROR;
	}

	/* Also bail out if page output is currently in progress */
	if (Console->PageBuf->NumPrint != 0) {
		return OK;
	}

	/* Save the first character as a command */
	Command = InBuf->Buf[0];
	bufLeftAlign(InBuf, InBuf->Buf + 1);

	/* If we have a quit command, leave pager mode */
	if (Command == Console->Pager[CONP_QUIT]) {

		/* QUIT */
		cmSendFFSCCmd(Console, "R . EXIT", Response);
		return OK;
	}

	/* Page forward if necessary */
	if (Command == Console->Pager[CONP_FWD]) {

		/* Attempt to erase the trailer */
		bufWrite(Console->UserOutBuf,
			 "\r                                        "
			     "                              \r",
			 72);

		/* If we are already at the end of the buffer, treat */
		/* this the same as QUIT.			     */
		if ((Console->PageBuf->CurrPtr - Console->PageBuf->Buf)
		    >= Console->PageBuf->DataLen)
		{
			cmSendFFSCCmd(Console, "R . EXIT", Response);
			return OK;
		}

		/* Find out how many lines to print */
		if (Console->LinesPerPage > 0) {
			Console->PageBuf->NumPrint = Console->LinesPerPage;
		}
		else {
			Console->PageBuf->NumPrint =
			    ffscTune[TUNE_PAGE_DFLT_LINES];
		}

		/* Go start the proceedings */
		cmPagerUpdate(Console);
	}
	else if (Command == Console->Pager[CONP_BACK]) {

		/* Page backward */
		int LinesPerPage;

		bufWrite(Console->UserOutBuf,
			 "\r                                        "
			     "                              \r",
			 72);

		if (Console->LinesPerPage > 0) {
			LinesPerPage = Console->LinesPerPage;
		}
		else {
			LinesPerPage = ffscTune[TUNE_PAGE_DFLT_LINES];
		}

		Console->PageBuf->NumPrint = -LinesPerPage;
		cmPagerUpdate(Console);
	}
	else {
		/* NOP if we don't recognize the input char. */
		/* Maybe we should beep?		     */
	}

	return OK;
}
