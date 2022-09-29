/*
 * conssys.c
 *	Console task functions for handling input from the system port
 */

#include <vxworks.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ffsc.h"

#include "buf.h"
#include "console.h"
#include "elsc.h"
#include "log.h"
#include "oobmsg.h"


/* Internal functions */
static STATUS csSysInEcho(console_t *);
static STATUS csSysInELSC(console_t *);
static STATUS csSysInWatchOOB(console_t *);
static STATUS csSysInToUserOut(console_t *, int);


/* Global variables */
unsigned char csGraphicsCommand[MAX_OOBMSG_LEN];


/*
 * csSysIn
 *	Processes input from the system port. Returns OK/ERROR.
 */
STATUS
csSysIn(console_t *Console)
{
	STATUS Result;

	/* Proceed according to the current console mode */
	switch (Console->Mode) {

	    case CONM_CONSOLE:
	    case CONM_WATCHSYS:
		Result = csSysInWatchOOB(Console);
		break;

	    case CONM_COPYUSER:
	    case CONM_COPYSYS:
	    case CONM_MODEM:
		Result = csSysInEcho(Console);
		break;

	    case CONM_ELSC:
		Result = csSysInELSC(Console);
		break;

	    default:
		ffscMsg("csSysIn called for %s while in mode %d",
			Console->Name, Console->Mode);
		Result = ERROR;
	}

	return Result;
}




/*----------------------------------------------------------------------*/
/*									*/
/*			    INTERNAL FUNCTIONS				*/
/*									*/
/*----------------------------------------------------------------------*/

/*
 * csSysInEcho
 *	Processes input from the system port and sends it directly
 *	to the user output port. If any messages are pending, they
 *	are printed on CR/LF boundaries.
 */
STATUS
csSysInEcho(console_t *Console)
{
	return bufAppendAddMsgs(Console->UserOutBuf,
				Console->SysInBuf,
				Console->SysInBuf->Len,
				Console->MsgBuf);
}


/*
 * csSysInELSC
 *	Processes input from the system port and sends it directly
 *	to the user output port UNLESS it begins with Ctrl-T, in which
 *	case it is treated as a command from the ELSC itself. If any
 *	messages are pending, they are printed on CR/LF boundaries.
 */
STATUS
csSysInELSC(console_t *Console)
{
	buf_t *InBuf = Console->SysInBuf;
	buf_t *OutBuf = Console->UserOutBuf;
	char  *Ptr;
	char  *Start;
	int   PrefixLen;

	/* Proceed until we have exhausted the input buffer */
	while (InBuf->Len > 0) {

		/* Proceed according to the current state */

		switch (Console->SysState) {

		    case CONSM_ELSCCMD:
			/* If we are in the midst of gathering an ELSC	*/
			/* command, look for the first END character.	*/

			/* Look for an END1 character */
			InBuf->Buf[InBuf->Len] = '\0';
			Start = InBuf->Buf + Console->SysOffset;
			Ptr   = strchr(Start, ELSC_MSG_END_CHAR1);

			/* Determine the current length of the command */
			if (Ptr == NULL) {
				Ptr = InBuf->Buf + InBuf->Len;
			}
			PrefixLen = Ptr - Start;
			Console->SysOffset += PrefixLen;

			/* If we couldn't find an END1, we are done for */
			/* now. We'll try again next time.		*/
			if (*Ptr == '\0') {
				return OK;
			}

			/* Otherwise, prepare to look for the END2 char */
			Console->SysState = CONSM_GOTEND1;
			++Console->SysOffset;
			if (InBuf->Buf[Console->SysOffset] == '\0') {
				return OK;
			}

			/* FALL THROUGH... */


		    case CONSM_GOTEND1:
			/* If we have already seen the first part of */
			/* a command-end sequence, look for part 2.  */

			/* The first character past the end of the    */
			/* command that has been gathered so far must */
			/* be the END2 char, or it was a false alarm. */
			if (InBuf->Buf[Console->SysOffset]
			    != ELSC_MSG_END_CHAR2)
			{
				++Console->SysOffset;
				Console->SysState = CONSM_ELSCCMD;
				continue;
			}

			/* If the "command" is really a standard response */
			/* string, then just echo it to the console.	  */
			if (InBuf->Buf[0] == ELSC_MSG_BEGIN_CHAR
			    &&
			    (strncmp(InBuf->Buf + 1,
				     ELSC_RESP_OK,
				     ELSC_RESP_OK_LEN) == 0  ||
			     strncmp(InBuf->Buf + 1,
				     ELSC_RESP_ERR,
				     ELSC_RESP_ERR_LEN) == 0))
			{
				if (ffscDebug.Flags & FDBF_CONSOLE) {
					ffscMsg("MSC command on %s was really"
						    " a response string",
						Console->Name);
				}

				if (Console->ELSC->Log != NULL) {
					logWrite(Console->ELSC->Log,
						 InBuf->Buf,
						 InBuf->Len);
				}

				if (bufAppendAddMsgs(OutBuf,
						     InBuf,
						     Console->SysOffset + 1,
						     Console->MsgBuf) < 0)
				{
					return ERROR;
				}
			}

			/* Otherwise, treat it as a command from the ELSC */
			else {

				/* Mark the end of the command, conveniently */
				/* overwriting the annoying END characters.  */
				InBuf->Buf[Console->SysOffset - 1] = '\0';

				/* Go deal with the command itself */
				elscProcessCommand(Console->ELSC,
						   InBuf->Buf + 1);

				/* Skip over the now-finished command */
				bufLeftAlign(InBuf,
					     (InBuf->Buf
					      + Console->SysOffset + 1));
			}

			/* No longer gathering a command */
			Console->SysState  = CONSM_NORMAL;
			Console->SysOffset = 0;

			break;


		    case CONSM_NORMAL:
			/* We are apparently looking at normal input.	*/
			/* Watch for escape characters, and copy out	*/
			/* everything else.				*/

			/* Look for an escape character */
			Ptr = memchr(InBuf->Buf,
				     ELSC_MSG_BEGIN_CHAR,
				     InBuf->Len);

			/* If none, we can copy over the entire buffer */
			if (Ptr == NULL) {
				if (Console->ELSC->Log != NULL) {
					logWrite(Console->ELSC->Log,
						 InBuf->Buf,
						 InBuf->Len);
				}

				if (bufAppendAddMsgs(OutBuf,
						     InBuf,
						     InBuf->Len,
						     Console->MsgBuf) < 0)
				{
					return ERROR;
				}
			}

			/* Otherwise, copy everything up to the escape */
			else {
				int OutLen = Ptr - InBuf->Buf;

				/* If there is anything in the buffer */
				/* before the escape, push it out     */
				if (OutLen > 0) {
					if (Console->ELSC->Log != NULL) {
						logWrite(Console->ELSC->Log,
							 InBuf->Buf,
							 InBuf->Len);
					}

					bufAppendAddMsgs(OutBuf,
							 InBuf,
							 OutLen,
							 Console->MsgBuf);
				}

				/* Adjust the buffer to the esc char */
				bufLeftAlign(InBuf,
					     InBuf->Buf + OutLen);

				/* Remember that we are gathering a command */
				Console->SysState  = CONSM_ELSCCMD;
				Console->SysOffset = 1;
				if (ffscDebug.Flags & FDBF_CONSOLE) {
					ffscMsg("%s gathering command from "
						    "MSC",
						Console->Name);
				}
			}
		}
	}

	return OK;
}


/*
 * csSysInWatchOOB
 *	Processes input from the system port and sends most of it directly
 *	to the user output port. The exception is Out Of Band (OOB) messages
 *	from the system, typically from flashffsc or sysctlrd.
 */
STATUS
csSysInWatchOOB(console_t *Console)
{
	buf_t *InBuf = Console->SysInBuf;
	char *Ptr;
	int  OriginalMode;

	/* Proceed until we have exhausted the input buffer */
	while (InBuf->Len > 0) {

		/* Proceed according to our current state */
		switch (Console->SysState) {

		    case CONSM_NORMAL:
			/* We are handling ordinary data. Watch for an	*/
			/* Out of Band prefix, and copy everything else */
			/* on to the user.				*/

			/* If OOB message processing has been shut off */
			/* proceed as if we didn't find an OBP char    */
			if (!(Console->Flags & CONF_OOBOKSYS)) {
				return csSysInToUserOut(Console,
							InBuf->Len);
			}

			/* Look for an OBP character */
			Ptr = memchr(InBuf->Buf,
				     Console->Delim[COND_OBP],
				     InBuf->Len);

			/* If none, we can copy over the entire buffer */
			if (Ptr == NULL) {
				return csSysInToUserOut(Console,
							InBuf->Len);
			}

			/* Otherwise, copy everything up to the prefix char */
			else {
				int OutLen = Ptr - InBuf->Buf;

				/* Say what's going on if desired */
				if (ffscDebug.Flags & FDBF_BASEIO) {
					ffscMsg("%s got OBP - flushing %d "
						    "bytes",
						Console->Name, OutLen);
				}

				/* If there is anything in the buffer */
				/* before the prefix, push it out     */
				if (OutLen > 0) {
					csSysInToUserOut(Console, OutLen);
				}

				/* Remember that we have seen a prefix */
				Console->SysState = CONSM_OOBCODE;
				Console->Flags |= CONF_OOBMSG;

				/* Tell the user to be quiet */
				bufHold(Console->UserInBuf);
				bufHold(Console->SysOutBuf);
				Console->Flags |= CONF_HOLDUSERIN;
			}

			break;


		    case CONSM_OOBCODE:
			/* The last thing we saw was a prefix char. Look */
			/* at what comes next and proceed accordingly.	 */

			/* If nothing else has arrived, wait for more data */
			if (InBuf->Len < 2) {
				return OK;
			}

			/* If the next char is another prefix, echo a */
			/* single prefix character and go on.	      */
			if (InBuf->Buf[1] == Console->Delim[COND_OBP]) {

				/* If we held the user input buffer, it	*/
				/* can now be released			*/
				if (Console->Flags & CONF_HOLDUSERIN) {
					bufRelease(Console->UserInBuf);
					bufRelease(Console->SysOutBuf);
					Console->Flags &= ~CONF_HOLDUSERIN;
				}

				/* Say what we are doing if desired */
				if (ffscDebug.Flags & FDBF_BASEIO) {
					ffscMsg("%s echoing OBP char",
						Console->Name);
				}

				/* Go write out the lone escape character */
				if (Console->Mode != CONM_WATCHSYS) {
					bufWrite(Console->UserOutBuf,
						 &Console->Delim[COND_OBP],
						 1);
				}

				/* Clean up the input buffer */
				bufLeftAlign(InBuf, InBuf->Buf + 2);

				/* Return to normal input mode */
				Console->SysState = CONSM_NORMAL;
				Console->Flags &= ~CONF_OOBMSG;
			}

			/* Otherwise, start collecting the length field */
			else {
				Console->SysState = CONSM_OOBLEN;
				if (ffscDebug.Flags & FDBF_BASEIO) {
					ffscMsg("%s collecting out of band "
						    "message...",
						Console->Name);
				}
			}

			break;


		    case CONSM_OOBLEN:
			/* We've seen an OBP char and an opcode/status  */
			/* byte, now wait for the length to roll in.	*/
			if (InBuf->Len < 4) {

				/* Don't have a complete length field  */
				/* yet. Wait for more data to come in. */
				return OK;
			}

			/* We have enough data to form a valid	*/
			/* length, so it is safe to proceed	*/
			/* with gathering the data field. For	*/
			/* convenience, store the target length	*/
			/* in SysOffset.			*/
			Console->SysState  = CONSM_OOBDATA;
			Console->SysOffset = OOBMSG_MSGLEN(InBuf->Buf);

			break;


		    case CONSM_OOBDATA:
			/* We're waiting for the data field and checksum */
			/* of an out of band message to arrive.		 */
			if (InBuf->Len < Console->SysOffset) {

				/* Don't have a complete data field    */
				/* yet. Wait for more data to come in. */
				return OK;
			}

			/* Print an informative message if desired */
			if (ffscDebug.Flags & FDBF_BASEIO) {
				ffscMsg("%s received OOB msg code %d "
					    "length %d",
					Console->Name,
					OOBMSG_CODE(InBuf->Buf),
					OOBMSG_DATALEN(InBuf->Buf));
			}

			/* Make a copy of the graphics command so we can   */
			/* clear the buffer (in case a mode change occurs) */
			bcopy(InBuf->Buf,
			      csGraphicsCommand,
			      Console->SysOffset);

			/* Purge the command from the input buffer */
			bufLeftAlign(InBuf, InBuf->Buf + Console->SysOffset);

			/* Go process the OOB message */
			OriginalMode = Console->Mode;
			oobProcessMsg(Console, csGraphicsCommand);

			/* Return to the normal state, provided our mode */
			/* has not been changed				 */
			if (Console->Mode == OriginalMode) {
				Console->SysState  = CONSM_NORMAL;
				Console->SysOffset = 0;
				Console->Flags &= ~CONF_OOBMSG;

				/* If we held the user input buffer, it	*/
				/* can now be released.			*/
				if (Console->Flags & CONF_HOLDUSERIN) {
					bufRelease(Console->UserInBuf);
					bufRelease(Console->SysOutBuf);
					Console->Flags &= ~CONF_HOLDUSERIN;
				}
			}

			break;


		    default:
			/* Don't know this state! */
			ffscMsg("Unknown state for %s in csSysInWatchOOB: %d",
				Console->Name, Console->SysState);
			return ERROR;
		}
	}

	return OK;
}


/*
 * csSysInToUserOut
 *	Logs the specified portion of the specified console's SysInBuf
 *	to the BaseIO log, then copies it to the UserOut buffer (unless
 *	the console happens to be in WATCHSYS mode). The SysInBuf is
 *	presumed to be assigned to the BaseIO port.
 */
STATUS
csSysInToUserOut(console_t *Console, int Len)
{
	/* Log the data for posterity */
	logWrite(logBaseIO, Console->SysInBuf->Buf, Len);

	/* If the console is in WATCHSYS mode, simply discard the data */
	if (Console->Mode == CONM_WATCHSYS) {
		bufLeftAlign(Console->SysInBuf, Console->SysInBuf->Buf + Len);
	}

	/* Otherwise, send the data out to the user output buffer, */
	/* inserting any pending messages as needed.		   */
	else {
		if (bufAppendAddMsgs(Console->UserOutBuf,
				     Console->SysInBuf,
				     Len,
				     Console->MsgBuf) != OK)
		{
			return ERROR;
		}
	}

	return OK;
}
