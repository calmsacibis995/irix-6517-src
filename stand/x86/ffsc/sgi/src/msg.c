/*
 * msg.c
 *	FFSC message handling functions
 *
 *	"msg_t's" are used by the router task to keep track of incoming
 *	and outgoing messages from/to other tasks and FFSCs, including
 *	keeping track of state in case a message doesn't all come in at
 *	the same time.
 */

#include <vxworks.h>

#include <ctype.h>
#include <iolib.h>
#include <selectlib.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tasklib.h>

#include "ffsc.h"

#include "console.h"
#include "elsc.h"
#include "msg.h"
#include "timeout.h"


/*
 * msgClear
 *	Clears the current contents of the specified msg_t
 */
void
msgClear(msg_t *Msg)
{
	if (Msg == NULL) {
		return;
	}

	Msg->CurrPtr = &Msg->Text[0];
	Msg->State   = RS_UNKNOWN;

	bzero(Msg->Text, Msg->MaxLen+1);
}


/*
 * msgFree
 *	Releases the storage used by the specified msg_t
 */
void
msgFree(msg_t *Msg)
{
	if (Msg == NULL) {
		return;
	}

	free(Msg->Text);
	free(Msg);
}


/*
 * msgNew
 *	Allocates a new msg_t with a text buffer of the specified
 *	length and with the specified input FD and sender type. The
 *	initial state is set to RS_UNKNOWN. Returns a pointer to the
 *	new msg_t or NULL if unsuccessful.
 */
msg_t *
msgNew(int MaxLen, int FD, int Type, char *Name)
{
	msg_t *Msg;

	/* Allocate storage for a msg_t */
	Msg = (msg_t *) malloc(sizeof(msg_t));
	if (Msg == NULL) {
		ffscMsg("Unable to allocate storage for msg_t");
		return NULL;
	}

	/* Allocate storage for the message text */
	Msg->Text = (char *) malloc(MaxLen + 1);
	if (Msg->Text == NULL) {
		ffscMsg("Unable to allocate storage for message text");
		free(Msg);
		return NULL;
	}

	/* Initialize the remaining fields */
	Msg->MaxLen  = MaxLen;
	Msg->FD      = FD;
	Msg->Type    = Type;
	strncpy(Msg->Name, Name, MI_NAME_LEN);
	Msg->Name[MI_NAME_LEN-1] = '\0';

	msgClear(Msg);

	return Msg;
}


/*
 * msgRead
 *	Reads a message into the specified msg_t, timing out after
 *	a specified number of seconds. Returns ERROR if a fatal error
 *	has occurred, OK otherwise (timing out is *not* a fatal error).
 */
STATUS
msgRead(msg_t *Msg, int TimeoutSecs)
{
	char	Extraneous[EXTRANEOUS_LINE_LEN + 1];
	int	BufLen;
	int	BytesRead;
	int	ExtraLen;
	int	FlushExtra;
	int	ReadLen;
	STATUS  Result;
	int 	BytesInBuf;
	extern int TimeoutExpired;

	BytesRead = 0;


	/* In the trivial case of having no FD, we're done already */
	if (Msg->FD < 0) {
		return OK;
	}

	Result = OK;

	/* Arrange for the timeout signal */
	if (timeoutSet(TimeoutSecs, "reading message") != OK) {
		/* timeoutSet should have logged the error already */
		return OK;
	}

	/* Loop until we have read enough from the sender to form a	*/
	/* complete message.						*/
	BufLen     = Msg->MaxLen - (Msg->CurrPtr - Msg->Text);
	ExtraLen   = 0;
	FlushExtra = 0;

	if (Msg->State == RS_NEED_FFSCMSG  ||  Msg->State == RS_NEED_DISPACK) {
		ReadLen = BufLen;
	}
	else {
		ReadLen = 1;
	}

	while (Msg->State != RS_DONE  &&  Msg->State != RS_ERROR) {

		/* Consume another byte of input from the sender */

		/* wait for data to read */
		timeoutEnable();
		while (!TimeoutExpired) {
	      BytesInBuf = -1;
	      ioctl(Msg->FD, FIONREAD, (int) &BytesInBuf);
			if (BytesInBuf > 0) {
				timeoutDisable();
				BytesRead = read(Msg->FD, Msg->CurrPtr, ReadLen);
				break;
			}
         (void) taskDelay(1);
		}

		if (TimeoutExpired) {
      	Result = OK;
      	goto Done;
		}

		if (BytesRead < 0) {
			ffscMsgS("Read error in msgRead");
			Result = ERROR;
			goto Done;
		}
		else if (BytesRead == 0) {
			ffscMsg("EOF on FD %d", Msg->FD);
			Result = ERROR;
			goto Done;
		}

		/* Say what we read if desired */
		if (Msg->FD == ffscDebug.MsgMon) {
			if (ReadLen == 1) {
				ffscMsg("Read 1 byte from FD %d: '%c' (%d)",
					Msg->FD,
					(isprint(*Msg->CurrPtr)
					 ? *Msg->CurrPtr
					 : '.'),
					*Msg->CurrPtr);
			}
			else {
				ffscMsg("Read %d bytes from FD: \"%s\"",
					strlen(Msg->CurrPtr), Msg->CurrPtr);
			}
		}

		/* Add a convenient null-terminator to the buffer */
		Msg->CurrPtr[BytesRead] = '\0';

		/* Process the current byte of input data */
		switch (Msg->State) {

		    /* Waiting for an ELSC message. As it is,	*/
		    /* ELSC messages don't start with a special */
		    /* character, so waiting for a message from */
		    /* the ELSC amounts to waiting for the ELSC */
		    /* end characters to arraive.		*/
		    case RS_NEED_ELSCMSG:
		    case RS_NEED_ELSCEND:
			if (*Msg->CurrPtr != '\0') {
				if (*Msg->CurrPtr == ELSC_MSG_END_CHAR1) {
					Msg->State = RS_NEED_ELSCEND2;
				}
				else if (*Msg->CurrPtr == ELSC_MSG_END_CHAR2) {
					Msg->State = RS_NEED_ELSCEND1;
				}
				Msg->CurrPtr += 1;
				BufLen -= 1;
			}
			else {
				ffscMsg("Received NUL char from MSC FD %d",
					Msg->FD);
			}
			break;


		    /* Waiting for the second char at the end of an ELSC  */
		    /* message. If something other than the normal second */
		    /* char shows up, then the first end char is thrown   */
		    /* out (we don't handle one without the other).	  */
		    case RS_NEED_ELSCEND2:
			if (*Msg->CurrPtr == '\0') {
				ffscMsg("Received NUL char from MSC FD %d "
					    "instead of END2",
					Msg->FD);
				break;
			}

			if (*Msg->CurrPtr == ELSC_MSG_END_CHAR2) {
				Msg->State = RS_DONE;
				Msg->CurrPtr += 1;
				BufLen -= 1;
			}
			else {
				char *Prev = Msg->CurrPtr - 1;
				if (*Msg->CurrPtr != ELSC_MSG_END_CHAR1) {
					Msg->State = RS_NEED_ELSCEND;
				}

				if (ffscDebug.Flags & FDBF_SHOWBADCR) {
					*Prev = '.';
					Msg->CurrPtr += 1;
					BufLen -= 1;
				}
				else {
					*Prev = *Msg->CurrPtr;
				}
			}				

			break;


		    /* Waiting for the second char at the end of an ELSC  */
		    /* message in the odd case where CHAR1 arrives before */
		    /* CHAR2. Otherwise, it's the same as above.	  */
		    case RS_NEED_ELSCEND1:
			if (*Msg->CurrPtr == '\0') {
				ffscMsg("Received NUL char from MSC FD %d "
					    "instead of END1",
					Msg->FD);
				break;
			}

			if (*Msg->CurrPtr == ELSC_MSG_END_CHAR1) {
				Msg->State = RS_DONE;
				Msg->CurrPtr += 1;
				BufLen -= 1;
			}
			else {
				char *Prev = Msg->CurrPtr - 1;
				if (*Msg->CurrPtr != ELSC_MSG_END_CHAR2) {
					Msg->State = RS_NEED_ELSCEND;
				}

				if (ffscDebug.Flags & FDBF_SHOWBADCR) {
					*Prev = '.';
					Msg->CurrPtr += 1;
					BufLen -= 1;
				}
				else {
					*Prev = *Msg->CurrPtr;
				}
			}

			break;

		    /* Waiting for an ELSC response. This always */
		    /* starts with an intro character, so any	 */
		    /* other character is considered extraneous	 */
		    case RS_NEED_ELSCACK:
			if (*Msg->CurrPtr == ELSC_MSG_BEGIN_CHAR) {
				Msg->State = RS_NEED_ELSCACK_END;
				if (ExtraLen > 0) {
					FlushExtra = 1;
				}
			}
			else {
				Extraneous[ExtraLen] = *Msg->CurrPtr;
				++ExtraLen;
				if (ExtraLen == EXTRANEOUS_LINE_LEN) {
					FlushExtra = 1;
				}
			}
			break;

		    /* Waiting for the end of an ELSC response.   */
		    /* This is subtly different that waiting for  */
		    /* an ELSC message, mostly because we need to */
		    /* handle a piggybacked ELSC command.	  */
		    case RS_NEED_ELSCACK_END:
			if (*Msg->CurrPtr == ELSC_MSG_END_CHAR1) {
				Msg->State = RS_NEED_ELSCACK_END2;
			}
			Msg->CurrPtr += 1;
			BufLen -= 1;
			break;

		    /* Waiting for the second char at the end of an ELSC  */
		    /* response. If it turns out that the response does   */
		    /* not start with "ok" or "err", then the response is */
		    /* intermediate so we have to wait for the *next*     */
		    /* response string (I know, it's a tacky kludge).	  */
		    case RS_NEED_ELSCACK_END2:
			if (*Msg->CurrPtr == ELSC_MSG_END_CHAR2) {
				Msg->CurrPtr += 1;
				BufLen -= 1;

				/* If the msg already contains a begin char */
				/* then we've already done this once	    */
				if (strncmp(Msg->Text,
					    ELSC_RESP_OK,
					    ELSC_RESP_OK_LEN) != 0  &&
				    strncmp(Msg->Text,
					    ELSC_RESP_ERR,
					    ELSC_RESP_ERR_LEN) != 0 &&
				    strchr(Msg->Text,
					   ELSC_MSG_BEGIN_CHAR) == NULL)
				{
ffscMsg("Found piggybacked MSC cmd - continuing to wait for response");
					Msg->State = RS_NEED_ELSCACK_END;
				}
				else {
					Msg->State = RS_DONE;
				}
			}
			else {
				char *Prev = Msg->CurrPtr - 1;

				if (*Msg->CurrPtr != ELSC_MSG_END_CHAR1) {
					Msg->State = RS_NEED_ELSCACK_END;
				}

				if (ffscDebug.Flags & FDBF_SHOWBADCR) {
					*Prev = '.';
					Msg->CurrPtr += 1;
					BufLen -= 1;
				}
				else {
					*Prev = *Msg->CurrPtr;
				}
			}				

			break;


		    /* Waiting for a message from a remote FFSC. Since	*/
		    /* these actually come off of a local pipe, only	*/
		    /* one read can be done so we are already finished. */
		    /* Same thing applies for display task ACKs.	*/
		    case RS_NEED_FFSCMSG:
		    case RS_NEED_DISPACK:
			Msg->State = RS_DONE;
			Msg->CurrPtr += BytesRead;
			BufLen -= BytesRead;
			break;


		    /* Waiting for a response from a remote FFSC. Again */
		    /* there is no actual intro character, so this is   */
		    /* the same as waiting for the end character.	*/
		    case RS_NEED_FFSCACK:
		    case RS_NEED_FFSCEND:
			if (*Msg->CurrPtr == FFSC_ACK_END_CHAR) {
				Msg->State = RS_DONE;
			}
			else {
				Msg->CurrPtr += 1;
				BufLen -= 1;
			}
			break;


		    /* Don't know this state! */
		    default:
			ffscMsg("Unknown state in msgRead: %d",
				Msg->State);
			goto Done;
		}

		/* Flush extraneous input if necessary */
		if (FlushExtra) {
			Extraneous[ExtraLen] = '\0';
			ffscMsg("Extraneous input from FD %d:\r\n%s",
				Msg->FD, Extraneous);
			ExtraLen = 0;
			FlushExtra = 0;
		}

		/* If the buffer is full then return with a done status */
		/* and print out a partial message, but first add the CR/LF */
		if (BufLen < 1  && Msg->State != RS_DONE  && Msg->State != RS_ERROR) {
			strcat(Msg->CurrPtr, "\r\n");
			Msg->CurrPtr += 2;
			Msg->State = RS_DONE;
		}
	}

	/* Come here when we are done reading (good or bad) */
    Done:
	timeoutCancel();
	return Result;
}


/*
 * msgWrite
 *	Writes out the message in the specified msg_t, timing out
 *	if the operation cannot be completed in the specified number
 *	of seconds. Returns OK/ERROR as for msgRead.
 */
STATUS
msgWrite(msg_t *Msg, int TimeoutSecs)
{
	int MsgLen;
	int WriteLen;
	STATUS Result;

	/* In the trivial case of no FD, we're practically done already */
	if (Msg->FD < 0) {
		Msg->State = RS_DONE;
		return OK;
	}

	Result = OK;

/*BLM*/
#if 0
	/* If a timeout occurs, we will longjmp back to here */
	if (timeoutOccurred() != 0) {
		ffscMsg("Timed out writing message to FD %d, T/S=%d/%d",
			Msg->FD, Msg->Type, Msg->State);
		Result = OK;
		goto Done;
	}

	Result = OK;	/* Must do before & after setjmp (timeoutOccurred) */

	/* Arrange for the timeout signal */
	if (timeoutSet(TimeoutSecs, "writing message") != OK) {
		/* SetTimer should have logged the error already */
		return OK;
	}
#endif

	/* Loop until we have written all there is to write */
	MsgLen = strlen(Msg->CurrPtr);
	while (MsgLen > 0) {

		/* Write out as much as we can */
/*BLM*/
#if 0
		timeoutEnable();
#endif
		WriteLen = write(Msg->FD, Msg->CurrPtr, MsgLen);
/*BLM*/
#if 0
		timeoutDisable();
#endif
		if (WriteLen <= 0) {
			ffscMsgS("Write error in msgWrite, ret_val = 0x%x: MsgLen = 0x%x", WriteLen, MsgLen);
			Result = ERROR;
			goto Done;
		}

                /* Say what we wrote if desired */
		if (Msg->FD == ffscDebug.MsgMon) {
                        if (WriteLen == 1) {
                                ffscMsg("Wrote 1 byte to FD %d: '%c' (%d)",
					Msg->FD,
					(isprint(*Msg->CurrPtr)
					 ? *Msg->CurrPtr
					 : '.'),
					*Msg->CurrPtr);
			}
			else {
                                ffscMsg("Wrote %d bytes to FD: \"%.*s\"",
                                        WriteLen,
                                        WriteLen,
                                        Msg->CurrPtr);
			}
		}

		/* Update our current buffer position */
		Msg->State = RS_SENDING;
		Msg->CurrPtr += WriteLen;
		MsgLen -= WriteLen;
	}

	/* If we make it to here, we apparently wrote the entire message */
	Msg->State = RS_DONE;


	/* Come here when we are done reading (good or bad) */
    Done:
/*BLM*/
#if 0
	timeoutCancel();
#endif

	return Result;
}


#ifndef PRODUCTION

/*----------------------------------------------------------------------*/
/*									*/
/*			  DEBUGGING FUNCTIONS				*/
/*									*/
/*----------------------------------------------------------------------*/

/*
 * msgShow
 *	Display information about a msg_t
 */
STATUS
msgShow(msg_t *Msg, int Offset, char *Title)
{
	char Indent[80];

	if (Msg == NULL) {
		ffscMsg("Usage: msgShow <addr> [<offset> [<title>]]");
		return OK;
	}

	sprintf(Indent, "%*s", Offset, "");

	if (Title == NULL) {
		ffscMsg("%smsg_t %s (%p):", Indent, Msg->Name, Msg);
	}
	else {
		ffscMsg("%s%s - msg_t %s (%p):",
			Indent,
			Title, Msg->Name, Msg);
	}

	ffscMsg("%s    Text %p   CurrPtr %p   MaxLen %d   [Len] %d",
		Indent,
		Msg->Text, Msg->CurrPtr, Msg->MaxLen,
		(Msg->CurrPtr - Msg->Text));
	ffscMsg("%s    FD %d  Type %d  State %d",
		Indent,
		Msg->FD, Msg->Type, Msg->State);

	return OK;
}

#endif  /* !PRODUCTION */
