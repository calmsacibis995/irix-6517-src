/*
 * buf.c
 *	FFSC I/O buffers
 */

#include <vxworks.h>

#include <iolib.h>
#include <selectlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ffsc.h"

#include "buf.h"
#include "log.h"
#include "timeout.h"




/*
 * bufAppendAddMsgs
 *	Appends the specified amount of data from the specified input buffer
 *	to the specified output buffer and interleaves messages from the
 *	specified message buffer at line boundaries.
 */
STATUS
bufAppendAddMsgs(buf_t *OutBuf, buf_t *InBuf, int Len, buf_t *MsgBuf)
{
	char *Ptr;
	int  OutLen = -1;
	int  WriteLen;

	/* If there are no pending messages, copy the whole input buffer */
	/* to the user output						 */
	if (MsgBuf->Len == 0) {

		/* Write out as much as we can */
		WriteLen = bufWrite(OutBuf, InBuf->Buf, Len);
		if (WriteLen < 0) {
			return ERROR;
		}

		/* Update the input buffer accordingly */
		bufLeftAlign(InBuf, InBuf->Buf + WriteLen);

		return OK;
	}

	/* Make sure the input buffer is null-terminated, since we will	*/
	/* be using string functions.					*/
	InBuf->Buf[InBuf->Len] = '\0';

	/* If the output buffer was in the middle of an EOL sequence */
	/* see if the beginning of the input buffer will satisfy it  */
	if (OutBuf->EOLNdx > 0) {
		Ptr = &OutBuf->EOLSeq[OutBuf->EOLNdx];
		if (strncmp(InBuf->Buf, Ptr, strlen(Ptr)) == 0) {
			OutLen = strlen(Ptr);
		}
	}

	/* Search for a complete EOL sequence in the input buffer */
	if (OutLen < 0) {
		Ptr = strstr(InBuf->Buf, OutBuf->EOLSeq);
		if (Ptr != NULL  &&  Ptr < (InBuf->Buf + Len)) {
			OutLen = Ptr - InBuf->Buf + strlen(OutBuf->EOLSeq);
		}
	}

	/* If nothing else worked, write the entire buffer */
	if (OutLen < 0) {
		OutLen = Len;
	}

	/* Write the appropriate part out */
	WriteLen = bufWrite(OutBuf, InBuf->Buf, OutLen);
	if (WriteLen < 0) {
		return ERROR;
	}

	/* Account for the written part in the input buffer */
	bufLeftAlign(InBuf, InBuf->Buf + WriteLen);
	Len -= WriteLen;

	/* If we actually got as far as an EOL, go ahead and dump */
	/* the message buffer as well.				  */
	if (OutBuf->Flags & BUF_EOL) {
		if (bufAppendMsgs(OutBuf, MsgBuf)) {
			return ERROR;
		}
	}

	/* Flush whatever is left in the input buffer */
	WriteLen = bufWrite(OutBuf, InBuf->Buf, Len);
	if (WriteLen < 0) {
		return ERROR;
	}

	/* Update the input buffer accordingly */
	bufLeftAlign(InBuf, InBuf->Buf + WriteLen);

	return OK;
}


/*
 * bufAppendBuf
 *	Writes the contents of the specified input buffer to the end of the
 *	specified output buffer, or directly to its associated descriptor if
 *	the output buffer is empty. If it is not possible to write the entire
 *	input buffer to the descriptor, any remaining data is copied to the
 *	end of the output buffer. Any overflow from the output buffer is
 *	left in the input buffer. Returns OK/ERROR.
 */
STATUS
bufAppendBuf(buf_t *OutBuf, buf_t *InBuf)
{
	int WriteLen;

	/* Write out the current contents of the input buffer */
	WriteLen = bufWrite(OutBuf, InBuf->Buf, InBuf->Len);
	if (WriteLen < 0) {
		return ERROR;
	}

	/* Update the input buffer accordingly */
	bufLeftAlign(InBuf, InBuf->Buf + WriteLen);

	return OK;
}


/*
 * bufAppendBufToEOL
 *	Similar to bufAppendBuf, except that only the portion of the
 *	input buffer up to and including the first EOL sequence is
 *	actually written to the output buffer. BUF_EOL will be set
 *	accordingly.
 */
STATUS
bufAppendBufToEOL(buf_t *OutBuf, buf_t *InBuf)
{
	char *Ptr;
	int  OutLen = -1;
	int  WriteLen;

	/* Make sure the input buffer is null-terminated, since we will	*/
	/* be using string functions.					*/
	InBuf->Buf[InBuf->Len] = '\0';

	/* If the output buffer was in the middle of an EOL sequence */
	/* see if the beginning of the input buffer will satisfy it  */
	if (OutBuf->EOLNdx > 0) {
		Ptr = &OutBuf->EOLSeq[OutBuf->EOLNdx];
		if (strncmp(InBuf->Buf, Ptr, strlen(Ptr)) == 0) {
			OutLen = strlen(Ptr);
		}
	}

	/* Search for a complete EOL sequence in the input buffer */
	if (OutLen < 0) {
		Ptr = strstr(InBuf->Buf, OutBuf->EOLSeq);
		if (Ptr != NULL) {
			OutLen = Ptr - InBuf->Buf + strlen(OutBuf->EOLSeq);
		}
	}

	/* If nothing else worked, write the entire buffer */
	if (OutLen < 0) {
		OutLen = InBuf->Len;
	}

	/* Write the appropriate part out */
	WriteLen = bufWrite(OutBuf, InBuf->Buf, OutLen);
	if (WriteLen < 0) {
		return ERROR;
	}

	/* Account for the written part in the input buffer */
	bufLeftAlign(InBuf, InBuf->Buf + WriteLen);

	return OK;
}


/*
 * bufAppendMsgs
 *	Similar to bufAppendBuf, except that only complete messages
 *	(i.e. up to and including a BUF_MSG_END sequence) from MsgBuf are
 *	written to OutBuf. The first message is preceded by, and the
 *	last message followed by, a BUF_MSG_END sequence to set them apart.
 *	If the MsgBuf overflowed, a message to that effect is also
 *	written to OutBuf. If there is not enough room in the output
 *	buffer to hold a message, then that message and any following
 *	it are held in MsgBuf (unless the size of the single message
 *	exceeds the size of the entire output buffer, in which case
 *	it is truncated).
 */
STATUS
bufAppendMsgs(buf_t *OutBuf, buf_t *MsgBuf)
{
	/* Just to be picky, make sure the write kinds of buffers */
	/* have been specified.					  */
	if (!(OutBuf->Flags & BUF_OUTPUT)) {
		ffscMsg("bufAppendMsgs called with non-output OutBuf");
		return ERROR;
	}
	if (!(MsgBuf->Flags & BUF_MESSAGE)) {
		ffscMsg("bufAppendMsgs called with non-message MsgBuf");
		return ERROR;
	}

	/* Don't bother if the output buffer is held */
	if (OutBuf->Flags & BUF_HOLD) {
		return OK;
	}

	/* If the message buffer has overflowed, start with a warning */
	/* message about that occurrence			      */
	if (MsgBuf->Flags & BUF_OVERFLOW) {
		char Warning[80];
		int  WarningLen;

		/* Build the warning message */
		sprintf(Warning,
			BUF_MSG_END
			    "R%ld:WARNING - MESSAGES LOST DUE TO OVERFLOW"
			    BUF_MSG_END,
			ffscLocal.rackid);
		WarningLen = strlen(Warning);

		/* If the warning is too long, just give up for now */
		if (WarningLen > OutBuf->Avail) {
			return OK;
		}

		/* Write the warning to the output buffer */
		if (bufWrite(OutBuf, Warning, WarningLen) < 0) {
			return ERROR;
		}
	   
		/* Turn off the overflow flag now that a warning was issued */
		MsgBuf->Flags &= ~BUF_OVERFLOW;
	}

	/* Keep writing messages until we have run out or until there	*/
	/* is no more room in the output buffer				*/
	while (MsgBuf->Len > 0) {
		char *MsgEnd;
		int  MsgLen;
		int  WriteLen;

		/* Find the end of the next message. If there is no */
		/* BUF_MSG_END sequence, then it must have been	    */
		/* truncated, so we may truncate things even more   */
		/* and append one.				    */
		MsgBuf->Buf[MsgBuf->Len] = '\0';
		MsgEnd = strstr(MsgBuf->Buf, BUF_MSG_END);
		if (MsgEnd == NULL) {
			int Offset;

			if (MsgBuf->Avail < BUF_MSG_END_LEN) {
				Offset = ((MsgBuf->Size - BUF_MSG_END_LEN)
					  + MsgBuf->Avail);
			}
			else {
				Offset = MsgBuf->Len;
			}

			strcpy(MsgBuf->Buf + Offset, BUF_MSG_END);
			MsgBuf->Len = Offset + BUF_MSG_END_LEN;
			MsgBuf->Avail = MsgBuf->Size - MsgBuf->Len;
			MsgEnd = MsgBuf->Buf + MsgBuf->Len;
		}
		else {
			MsgEnd += BUF_MSG_END_LEN;
		}
		MsgLen = MsgEnd - MsgBuf->Buf;

		/* If the message won't fit in the output buffer */
		/* then we are finished for now			 */
		if (MsgLen > OutBuf->Avail) {
			break;
		}

		/* Write the current message to the output buffer */
		WriteLen = bufWrite(OutBuf, MsgBuf->Buf, MsgLen);
		if (WriteLen < 0) {
			return ERROR;
		}
		if (WriteLen != MsgLen) {
			/* Shouldn't happen since we checked available	*/
			/* space earlier				*/
			ffscMsg("Incomplete message written in bufAppendMsgs");
		}

		/* Clear the current message from the message buffer */
		bufLeftAlign(MsgBuf, MsgEnd);
	}

	return OK;
}


/*
 * bufAttachFD
 *	Attaches the specified file descriptor to the specified buffer
 *	and updates the appropriate fd_set bits, etc. If there is already
 *	a file descriptor associated with the buffer, it is detached first.
 */
void
bufAttachFD(buf_t *Buf, int NewFD)
{
	/* If we have already have an associated FD, detach it first */
	if (Buf->FD >= 0) {
		bufDetachFD(Buf);
	}

	/* Switch to the new FD */
	Buf->FD = NewFD;

	/* If we don't really have an FD, bail out */
	if (NewFD < 0) {
		ffscMsg("bufAttachFD called with FD %d", NewFD);
		return;
	}

	/* If the buffer is not held and is either an input buffer */
	/* or a non-empty output buffer, make it select-able.	   */
	if (!(Buf->Flags & BUF_HOLD) &&
	    ((Buf->Flags & BUF_INPUT)  ||
	     ((Buf->Flags & BUF_OUTPUT)  &&  Buf->Len > 0)))
	{
		if (Buf->FDSet != NULL) {
			FD_SET(Buf->FD, Buf->FDSet);
			if (Buf->NumFDs != NULL) {
				++(*(Buf->NumFDs));
			}
		}
	}
}


/*
 * bufAttachLog
 *	Associate the specified log_t with the specified buf_t. Subsequent
 *	writes to the buffer will also be directed to the log_t as well.
 */
void
bufAttachLog(buf_t *Buf, log_t *Log)
{
	if (Log == NULL) {
		return;
	}

	Buf->Log = Log;
}


/*
 * bufClear
 *	Empty out the contents of the specified buffer
 */
void
bufClear(buf_t *Buf)
{
	Buf->Len   = 0;
	Buf->Avail = Buf->Size;
	Buf->Curr  = Buf->Buf;

	if (Buf->Flags & BUF_FULL) {
		Buf->Flags &= ~BUF_FULL;
		bufRelease(Buf);
	}
}


/*
 * bufDetachFD
 *	Detaches the file descriptor associated with the specified buffer
 *	and updates the appropriate fd_set bits, etc.
 */
void
bufDetachFD(buf_t *Buf)
{
	/* If we don't have an FD, we're already done */
	if (Buf->FD < 0) {
		return;
	}

	/* If select was waiting on this FD, tell it not to anymore */
	if (Buf->FDSet != NULL  &&
	    FD_ISSET(Buf->FD, Buf->FDSet))
	{
		FD_CLR(Buf->FD, Buf->FDSet);
		if (Buf->NumFDs != NULL) {
			--(*(Buf->NumFDs));
		}
	}

	/* Note that we no longer have an associated FD */
	Buf->FD = -1;
}


/*
 * bufDetachLog
 *	Detaches the log associated with the specified buffer
 */
void
bufDetachLog(buf_t *Buf)
{
	Buf->Log = NULL;
}


/*
 * bufFlush
 *	Flushes the specified output buffer, or as much as possible,
 *	to its associated descriptor. If the output buffer is emptied
 *	completely, the descriptor is removed from the associated fd_set.
 *	Returns OK/ERROR.
 */
STATUS
bufFlush(buf_t *OutBuf)
{
	int WriteLen;
	char	error_timeout;

	error_timeout = 0;

	if (OutBuf->Len > 0) {
	  /* Try writing out as much as we can (subject to timeout) */
	  WriteLen = timeoutWrite(OutBuf->FD,
				  OutBuf->Buf,
				  OutBuf->Len,
				  ffscTune[TUNE_BUF_WRITE]);
	  if (WriteLen < 0) {
	    if (errno == EINTR) {
	      /* Timeout */
	      /*
	       * When a timeout occurs we do not know how much of the data
	       * was successfully written.  So to prevent a hung condition
	       * we will assume all data was written, and adjust the buffer
	       * accordingly.
	       */
	      WriteLen = OutBuf->Len;
	      error_timeout = 1;
	    }
	    else {
	      ffscMsgS("Write failed while flushing buffer");
	      return ERROR;
	    }
	  }

	  /* If the write was incomplete, update the output buffer */
	  if (WriteLen != OutBuf->Len) {
	    if (WriteLen > 0) {
	      memmove(OutBuf->Buf,
		      OutBuf->Buf + WriteLen,
		      OutBuf->Len - WriteLen);
	    }

	    if (ffscDebug.Flags & FDBF_BUFFER) {
	      ffscMsg("Flushed %d/%d bytes on FD %d",
		      WriteLen, OutBuf->Len, OutBuf->FD);
	    }
	  }

	  /* If the write was complete, there is no need to watch for */
	  /* the output descriptor to become ready for writing.	    */
	  else {
	    if (OutBuf->FDSet != NULL) {
	      FD_CLR(OutBuf->FD, OutBuf->FDSet);
	      if (OutBuf->NumFDs != NULL) {
		--(*(OutBuf->NumFDs));
	      }
	    }

	    if (ffscDebug.Flags & FDBF_BUFFER)  {
	      if (!error_timeout) 
		ffscMsg("Flushed all bytes on FD %d", OutBuf->FD);
	      else
		ffscMsg("Cleared all bytes on FD %d, due to timeout", OutBuf->FD);
	    }
	  }

	  /* Update the output buffer */
	  OutBuf->Curr  -= WriteLen;
	  OutBuf->Len   -= WriteLen;
	  OutBuf->Avail += WriteLen;

	}
	return OK;
}


/*
 * bufHold
 *	Places the specified buffer into HOLD state, meaning that I/O
 *	to or from the associated FD is not done. Writes to a held buffer
 *	are buffered(!), reads return immediately.
 */
STATUS
bufHold(buf_t *Buf)
{
	/* Make the appropriate notations */
	Buf->Flags |= BUF_HOLD;
	++Buf->HoldCnt;

	/* If there is a file descriptor currently associated	*/
	/* with this buffer, turn it off in its associated	*/
	/* fd_set so we don't try to select on it.		*/
	if (Buf->FD >= 0  &&
	    Buf->FDSet != NULL  &&
	    FD_ISSET(Buf->FD, Buf->FDSet))
	{
		FD_CLR(Buf->FD, Buf->FDSet);
		if (Buf->NumFDs != NULL) {
			--(*(Buf->NumFDs));
		}
	}

	return OK;
}


/*
 * bufInit
 *	Create and initialize a buf_t with the specified size, flags,
 *	fd_set and #FDs. Returns a pointer to the buf_t, or NULL if
 *	unsuccessful.
 */
buf_t *
bufInit(int Size, int Flags, fd_set *FDSet, int *NumFDs)
{
	buf_t *Buf;

	/* Start by allocating a buf_t struct */
	Buf = (buf_t *) malloc(sizeof(buf_t));
	if (Buf == NULL) {
		ffscMsgS("Unable to allocate storage for buf_t");
		return NULL;
	}

	/* Now an actual buffer */
	Buf->Buf = (char *) malloc(Size + 1);
	if (Buf->Buf == NULL) {
		ffscMsgS("Unable to allocate storage for buf_t buffer");
		return NULL;
	}

	/* Now fill in the easy stuff */
	Buf->FD	     = -1;
	Buf->Curr    = Buf->Buf;
	Buf->Size    = Size;
	Buf->Len     = 0;
	Buf->Avail   = Size;
	Buf->Flags   = Flags;
	Buf->FDSet   = FDSet;
	Buf->NumFDs  = NumFDs;
	Buf->HoldCnt = (Flags & BUF_HOLD) ? 1 : 0;
	Buf->EOLNdx  = 0;
	strcpy(Buf->EOLSeq, BUF_MSG_END);
	Buf->Log     = NULL;

	return Buf;
}


/*
 * bufLeftAlign
 *	Given a buf_t and a pointer that is assumed to point at some
 *	location in the buffer, readjusts the buffer so the character
 *	at the pointer becomes the leftmost byte in the buffer.
 */
void
bufLeftAlign(buf_t *Buf, char *Ptr)
{
	int Delta;

	if (Ptr < Buf->Buf  ||  Ptr > (Buf->Buf + Buf->Len)) {
		ffscMsg("Called bufLeftAlign with bogus Ptr, clearing buffer");
		bufClear(Buf);
		return;
	}

	Delta = Ptr - Buf->Buf;
	if (Delta == 0) {
		return;
	}

	Buf->Len   -= Delta;
	Buf->Avail += Delta;
	Buf->Curr  -= Delta;

	if (Buf->Len > 0) {
		memmove(Buf->Buf, Ptr, Buf->Len);
	}

	if (Buf->Avail > 0  &&  (Buf->Flags & BUF_FULL)) {
		bufRelease(Buf);
		Buf->Flags &= ~BUF_FULL;
	}
}


/*
 * bufRead
 *	Appends data to the specified buffer from its associated descriptor.
 *	Returns OK/ERROR.
 */
STATUS
bufRead(buf_t *Buf)
{
	int ReadLen;

	/* If the port is gone, we are finished */
	if (Buf->FD < 0) {
		return ERROR;
	}

	/* If the buffer is currently held or full, return immediately */
	if (Buf->Flags & (BUF_HOLD | BUF_FULL)) {
		return OK;
	}

	/* Try to read from the serial port */
	ReadLen = timeoutRead(Buf->FD,
			      Buf->Curr,
			      Buf->Avail,
			      ffscTune[TUNE_BUF_READ]);
	if (ReadLen <= 0) {
		if (ReadLen < 0  &&  errno == EINTR) {
			ffscMsg("Timed out: FD %d  Len %d  Time %d",
				Buf->FD, Buf->Avail, ffscTune[TUNE_BUF_READ]);
			return OK;
		}
		else if (ReadLen < 0) {
			ffscMsgS("Read from FD %d failed", Buf->FD);
		}
		return ERROR;
	}

	/* Update the buf_t accordingly */
	Buf->Curr  += ReadLen;
	Buf->Len   += ReadLen;
	Buf->Avail -= ReadLen;

	/* If there is no more space in the buffer, mark it full and */
	/* disable any additional reads				     */
	if (Buf->Avail <= 0) {
		bufHold(Buf);
		Buf->Flags |= BUF_FULL;
	}

	return OK;
}


/*
 * bufRelease
 *	Releases the specified buffer from HOLD state. If the buffer
 *	is marked for output, it will be flushed. If it is marked for
 *	input, its FD will be added to its fd_set.
 */
STATUS
bufRelease(buf_t *Buf)
{
	int Result = OK;

	/* Make sure we aren't already released */
	if (Buf->HoldCnt < 1) {
		ffscMsg("Attempted to released unheld buffer");
		return OK;
	}

	/* Decrement the hold count. If it is still greater than */
	/* zero then there is nothing left to do.		 */
	--Buf->HoldCnt;
	if (Buf->HoldCnt > 0) {
		return OK;
	}

	/* Note that the buffer is no longer held */
	Buf->Flags &= ~BUF_HOLD;

	/* Flush the buffer if it is intended for output */
	if (Buf->Flags & BUF_OUTPUT) {
		Result = bufFlush(Buf);
	}

	/* Make the FD select-able if it is intended for input or */
	/* necessary for output.				  */
	if (Buf->FD >= 0  &&
	    Buf->FDSet != NULL  &&
	    !FD_ISSET(Buf->FD, Buf->FDSet) &&
	    ((Buf->Flags & BUF_INPUT)  ||
	     ((Buf->Flags & BUF_OUTPUT) && (Buf->Len > 0))))
	{
		FD_SET(Buf->FD, Buf->FDSet);
		if (Buf->NumFDs != NULL) {
			++(*(Buf->NumFDs));
		}
	}

	return Result;
}


/*
 * bufWrite
 *	Appends the specified data to the end of the specified output buffer,
 *	or writes it directly to its associated descriptor if the output
 *	buffer is empty. If it is not possible to write all of the input
 *	data to the descriptor, any remaining data is copied to the
 *	end of the output buffer. Returns the number of bytes written,
 *	or -1 on error.
 */
int
bufWrite(buf_t *Buf, char *Data, int Len)
{
	int CopyLen;
	int WriteLen;
	char error_timeout;

	error_timeout = 0;
	/* Bail out quickly for trivial cases */
	if (Len < 1) {
		return 0;
	}

	/* If the buffer has an associated log, write the data to it */
	/* before we start ripping things apart for real I/O.	     */
	if (Buf->Log != NULL) {
		logWrite(Buf->Log, Data, Len);
	}

	/* If the output buffer is not empty, then simply append the */
	/* input data to the output buffer.			     */
	if (Buf->Len > 0  ||  (Buf->Flags & BUF_HOLD)  ||  Buf->FD < 0) {
		if (Len > Buf->Avail) {
			CopyLen = Buf->Avail;
		}
		else {
			CopyLen = Len;
		}

		bcopy(Data, Buf->Curr, CopyLen);
		Buf->Curr  += CopyLen;
		Buf->Len   += CopyLen;
		Buf->Avail -= CopyLen;

		if (Buf->FDSet != NULL  &&
		    Buf->FD >= 0  &&
		    !FD_ISSET(Buf->FD, Buf->FDSet))
		{
			FD_SET(Buf->FD, Buf->FDSet);
			if (Buf->NumFDs != NULL) {
				++(*(Buf->NumFDs));
			}
		}

		if (ffscDebug.Flags & FDBF_BUFFER) {
			ffscMsg("Appended %d bytes to output buffer %p",
				CopyLen, Buf);
		}

		WriteLen = CopyLen;
	}

	/* Otherwise, write directly to the output descriptor */
	else {
		/* Try to write as much as we can */
		WriteLen = timeoutWrite(Buf->FD,
					Data,
					Len,
					ffscTune[TUNE_BUF_WRITE]);
		if (WriteLen < 0) {
			if (errno == EINTR) {
				/*
			  	 * When a timeout occurs we do not know how much of the data
			 	 * was successfully written.  So to prevent a hung condition
			 	 * we will assume all data was written, and adjust the buffer
			 	 * accordingly.
			 	 */
				WriteLen = Len;
				error_timeout = 1;
			}
			else {
				ffscMsgS("Write to device %d failed", Buf->FD);
				return -1;
			}
		}

		/* If we weren't able to write out the entire input buffer, */
		/* copy over the remaining data to the output buffer.	    */
		if (WriteLen != Len) {
			CopyLen = Len - WriteLen;
			if (CopyLen > Buf->Size) {
				CopyLen = Buf->Size;
			}

			bcopy(Data + WriteLen, Buf->Buf, CopyLen);
			Buf->Curr  = Buf->Buf + CopyLen;
			Buf->Len   = CopyLen;
			Buf->Avail = Buf->Size - CopyLen;

			if (Buf->FDSet != NULL  &&
			    !FD_ISSET(Buf->FD, Buf->FDSet))
			{
				FD_SET(Buf->FD, Buf->FDSet);
				if (Buf->NumFDs != NULL) {
					++(*(Buf->NumFDs));
				}
			}

			if (ffscDebug.Flags & FDBF_BUFFER) {
				ffscMsg("Buffered %d bytes for output",
					CopyLen);
			}

			WriteLen += CopyLen;
		}
	}

	/* If desired, see if we are at EOL */
	if (Buf->Flags & BUF_MONEOL) {

		/* Assume we are not at EOL for now */
		Buf->Flags &= ~BUF_EOL;

		/* If we were in the middle of a sequence earlier, */
		/* then see if the remainder of the sequence is	   */
		/* present. Note that it only counts if it is the  */
		/* end of the written data.			   */
		if (Buf->EOLNdx > 0  &&
		    strncmp(Data, &Buf->EOLSeq[Buf->EOLNdx], WriteLen) == 0)
		{
			if (WriteLen == strlen(&Buf->EOLSeq[Buf->EOLNdx])) {
				Buf->EOLNdx = 0;
				Buf->Flags |= BUF_EOL;
			}
			else {
				/* Data is shorter than Seq */
				Buf->EOLNdx += WriteLen;
			}
		}

		/* Otherwise, look for an EOL sequence at the tail end	*/
		/* of the written data.					*/
		else if (strncmp(Data + WriteLen - strlen(Buf->EOLSeq),
				 Buf->EOLSeq,
				 strlen(Buf->EOLSeq))
			 == 0)
		{
			Buf->EOLNdx = 0;
			Buf->Flags |= BUF_EOL;
		}

		/* If that didn't work, look for any leading substring	*/
		/* of the EOL sequence at the end of the written data.	*/
		else {
			char *End;
			int  Index;

			End = Data + WriteLen;
			for (Index = strlen(Buf->EOLSeq) - 1;
			     Index > 0;
			     --Index)
			{
				if (strncmp(End - Index, Buf->EOLSeq, Index)
				    == 0)
				{
					Buf->EOLNdx = Index;
					break;
				}
			}
		}
	}

	return WriteLen;
}


#ifndef PRODUCTION

/*----------------------------------------------------------------------*/
/*									*/
/*			  DEBUGGING FUNCTIONS				*/
/*									*/
/*----------------------------------------------------------------------*/

/*
 * bufShow
 *	Display information about the specified buffer.
 */
STATUS
bufShow(buf_t *Buf, int Offset, char *Title)
{
	char Indent[80];
	int  i;

	if (Buf == NULL) {
		ffscMsg("Usage: bufShow <addr> [<offset> [<title>]]");
		return OK;
	}

	sprintf(Indent, "%*s", Offset, "");

	if (Title == NULL) {
		ffscMsg("%sbuf_t %p:", Indent, Buf);
	}
	else {
		ffscMsg("%s%s - buf_t %p:", Indent, Title, Buf);
	}

	ffscMsg("%s    FD %d   Buf %p   Curr %p   Flags 0x%08x",
		Indent,
		Buf->FD, Buf->Buf, Buf->Curr, Buf->Flags);
	ffscMsg("%s    Size %d   Len %d   Avail %d   HoldCnt %d",
		Indent,
		Buf->Size, Buf->Len, Buf->Avail, Buf->HoldCnt);
	ffscMsgN("%s    EOLNdx %d   EOLSeq:", Indent, Buf->EOLNdx);

	for (i = 0;  i < BUF_EOLSEQ_LEN  &&  Buf->EOLSeq[i] != '\0';  ++i) {
		ffscMsgN(" %03o", Buf->EOLSeq[i]);
	}
	ffscMsg("");

	if (Buf->NumFDs != NULL) {
		ffscMsg("%s    NumFDs @%p  (%d)",
			Indent,
			Buf->NumFDs, *Buf->NumFDs);
	}

	if (Buf->FDSet != NULL) {
		ffscMsgN("%s    FDSet @%p:", Indent, Buf->FDSet);
		for (i = 0;  i < FD_SETSIZE;  ++i) {
			if (FD_ISSET(i, Buf->FDSet)) {
				ffscMsgN(" %d", i);
			}
		}
		ffscMsg("");
	}

	return OK;
}

#endif  /* !PRODUCTION */
