/*
 * oobmsg.c
 *	Functions for handling Out Of Band messages
 */

#include <vxworks.h>

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ffsc.h"

#include "buf.h"
#include "console.h"
#include "oobmsg.h"
#include "timeout.h"

/* 
 * Some canned OOB responses
 */
char oobResponseBadChecksum[] = {
	OBP_CHAR,
	STATUS_CHECKSUM,
	0, 0,
	STATUS_CHECKSUM
};

char oobResponseBadCommand[] = {
	OBP_CHAR,
	STATUS_COMMAND,
	0, 0,
	STATUS_COMMAND
};

char oobResponseOK[] = {
	OBP_CHAR,
	STATUS_NONE,
	0, 0,
	STATUS_NONE
};

char oobResponseVersion[] = {
	OBP_CHAR,
	STATUS_NONE,
	0, 1,		/* Length of 1 */
	1,		/* Current protocol version # */
	1 + 1		/* Checksum */
};


/* Internal functions */
static void oobSendFFSCCmd(console_t *, char *);
static void oobSendGraphicsCmd(console_t *, unsigned char *);


/*
 * oobChecksumMsg
 *	Calculates the checksum of the specified OOB message, which is
 *	assumed to be valid in at least the first 4 bytes (i.e. up to
 *	the length field).
 */
unsigned char
oobChecksumMsg(const unsigned char *Msg)
{
	int i;
	int Len;
	int Sum;

	Len = OOBMSG_DATALEN(Msg) + 4;
	Sum = 0;
	for (i = 1;  i < Len;  ++i) {
		Sum += Msg[i];
	}

	return (unsigned char) Sum;
}


/*
 * oobProcessMsg
 *	Processes the incoming OOB message in the specified buffer and sends
 *	the resulting response to the system FD associated with the specified
 *	console. We are assumed to be running in the context of that console's
 *	task.
 */
void
oobProcessMsg(console_t *Console, unsigned char *Msg)
{
	unsigned char CheckSum;

	/* A couple of quick sanity checks */
	if (Msg == NULL) {
		ffscMsg("%s called oobProcessMsg with NULL message",
			Console->Name);
		return;
	}
	if (Msg[0] != (unsigned char) OBP_CHAR) {
		ffscMsg("%s called oobProcessMsg with invalid message: "
			    "prefix=%03o",
			Console->Name, Msg[0]);
		return;
	}

	/* Validate the checksum in the message */
	CheckSum = oobChecksumMsg(Msg);
	if (CheckSum != OOBMSG_SUM(Msg)) {
		ffscMsg("%s received OOB msg with invalid checksum, was %x "
			    "should be %x",
			Console->Name,
			OOBMSG_SUM(Msg),
			CheckSum);
		oobSendMsg(Console->SysOutBuf->FD, oobResponseBadChecksum);
		return;
	}

	/* Print a message if desired */
	if (ffscDebug.Flags & FDBF_BASEIO) {
		ffscMsg("%s received OOB request %d",
			Console->Name, OOBMSG_CODE(Msg));
	}

	/* Proceed according to the opcode */
	switch (OOBMSG_CODE(Msg)) {

	    case OOB_NOP:
		oobSendMsg(Console->SysOutBuf->FD, oobResponseOK);
		break;

	    case GRAPH_START:
	    case GRAPH_END:
	    case GRAPH_MENU_ADD:
	    case GRAPH_MENU_GET:
	    case GRAPH_LABEL_TITLE:
	    case GRAPH_LABEL_HORIZ:
	    case GRAPH_LABEL_VERT:
	    case GRAPH_LABEL_MIN:
	    case GRAPH_LABEL_MAX:
	    case GRAPH_LEGEND_ADD:
	    case GRAPH_DATA:
		oobSendGraphicsCmd(Console, Msg);
		break;

	    case FFSC_COMMAND:
		if (Console->Flags & CONF_NOFFSCSYS) {
			oobSendMsg(Console->SysOutBuf->FD,
				   oobResponseBadCommand);
		}
		else {
			oobSendFFSCCmd(Console, OOBMSG_DATA(Msg));
		}
		break;

	    case FFSC_INIT:
		oobSendMsg(Console->SysOutBuf->FD, oobResponseVersion);
		break;

	    default:
		oobSendMsg(Console->SysOutBuf->FD, oobResponseBadCommand);
		break;
	}

	return;
}


/*
 * oobSendFFFSCCmd
 *	Process an FFSC command and store the result in the specified buffer
 *	as a standard OOB response. Returns a pointer to the response buffer.
 */
void
oobSendFFSCCmd(console_t *Console, char *Cmd)
{
	char *FFSCResponse;
	char *PBCmd;
	char PBCmdCopy[MAX_CONS_CMD_LEN + 1];
	int  ResponseLen;
	unsigned char Response[MAX_FFSC_RESP_LEN + MAX_OOBMSG_OVERHEAD];

	/* Initialize the response buffer */
	Response[0] = OBP_CHAR;
	Response[1] = STATUS_NONE;
	FFSCResponse = &Response[4];

	/* Go issue the command */
	cmSendFFSCCmdNoPB(Console, Cmd, FFSCResponse);

	/* Check for a piggybacked command in the FFSC response    */
	PBCmd = strchr(FFSCResponse, Console->Delim[COND_PB]);
	if (PBCmd != NULL) {

		/* Make a copy of the command for strtok to destroy */
		strcpy(PBCmdCopy, PBCmd + 1);

		/* If the user doesn't want to see piggybacked commands */
		/* make it go away in the response			*/
		if (Console->RMsg != CONRM_VERBOSE) {
			*PBCmd = '\0';
		}
	}

	/* Calculate the length of the response & checksum it */
	ResponseLen = strlen(FFSCResponse) + 1;
	Response[2] = ResponseLen / 256;
	Response[3] = ResponseLen % 256;
	Response[4 + ResponseLen] = oobChecksumMsg(Response);

	/* Send the response back to the user *before* we deal with */
	/* any piggybacked commands				    */
	oobSendMsg(Console->SysOutBuf->FD, Response);

	/* If we received a piggybacked command, process it now */
	if (PBCmd != NULL) {

		/* Say what's going on if desired */
		if (ffscDebug.Flags & FDBF_CONSOLE) {
			ffscMsg("MMSC piggybacked request: \"%s\"", PBCmdCopy);
		}

		/* Go deal with the command. Note that errors are ignored. */
		crProcessCmd(Console, PBCmdCopy);
	}

	return;
}


/*
 * oobSendGraphicsCmd
 *	Process a graphics command and store the result in the specified buffer
 *	as a standard OOB response. Returns a pointer to the response buffer.
 *
 *	The protocol for dealing with graphics commands is as follows:
 *
 *	- The console task issues the FFSC command "graphics_command XXX YYY"
 *	  where XXX is the address of a buffer containing the OOB graphics
 *	  message, and YYY is the address of a buffer in which the response
 *	  should be stored.
 *
 *	- The router validates XXX & YYY and then sends the command "g XXX"
 *	  to the display task. (Validates means ensuring that the values
 *        are legitimate addresses, it does not actually parse arguments, etc)
 *
 *	- The display task processes the command as it sees fit and stores
 *	  its response in a buffer at address ZZZ. It then sends the string
 *	  "OK ZZZ" back to the router. The display's response is a normal
 *	  OOB response except that the checksum has not been appended.
 *
 *	- The router validates ZZZ and copies the response from ZZZ into YYY,
 *	  adding a proper checksum. It then sends the response "OK n" back
 *	  to the console, where n is the status code.
 *
 *	- The console checks the response from the router. If it does not
 *	  begin with "OK", it synthesizes an error response in YYY. In
 *	  any event, the response in buffer YYY is then sent back to the IO6.
 */
void
oobSendGraphicsCmd(console_t *Console, unsigned char *Msg)
{
	char Command[MAX_FFSC_CMD_LEN + 1];
	char FFSCResponse[MAX_FFSC_RESP_LEN_SINGLE + 1];
	char *RespPtr;
	unsigned char Response[MAX_FFSC_RESP_LEN + MAX_OOBMSG_OVERHEAD];

	/* Build an FFSC command to handle this request */
	sprintf(Command, "rack . graphics_command %p %p", Msg, Response);

	/* Go issue the command */
	cmSendFFSCCmd(Console, Command, FFSCResponse);

	/* Skip over the header in the response string */
	RespPtr = strchr(FFSCResponse, Console->Delim[COND_HDR]);
	if (RespPtr == NULL) {
		RespPtr = FFSCResponse;
	}
	else {
		++RespPtr;
	}

	/* If the response does not begin with "OK", then synthesize */
	/* a response of our own indicating failure.		     */
	if (strncmp(RespPtr, "OK", 2) != 0) {
		int ResponseLen;

		ResponseLen = strlen(FFSCResponse) + 1;

		Response[0] = OBP_CHAR;
		Response[1] = STATUS_INTERNAL;
		Response[2] = ResponseLen / 256;
		Response[3] = ResponseLen % 256;

		strcpy(&Response[4], FFSCResponse);
		Response[3 + ResponseLen] = '\0';

		Response[4 + ResponseLen] = oobChecksumMsg(Response);
	}

	/* Send our response back */
	oobSendMsg(Console->SysOutBuf->FD, Response);

	return;
}



/*
 * oobSendMsg
 *	Sends the specified OOB message *to* the specified file descriptor.
 *	Note that this bypasses the usual FFSC buf_t stuff.
 */
STATUS
oobSendMsg(int FD, unsigned char *Msg)
{
	int MsgLen;
	int Offset;
	int WriteLen;

	/* Calculate the length of the message for efficiency */
	MsgLen = OOBMSG_MSGLEN(Msg);

	/* Keep writing until we've written everything (or timed out) */
	Offset = 0;
	while (MsgLen > 0) {
		WriteLen = timeoutWrite(FD,
					Msg + Offset,
					MsgLen,
					ffscTune[TUNE_OOBMSG_WRITE]);
		if (WriteLen < 0) {
			if (errno == EINTR) {
				ffscMsg("Timed out writing OOB message");
			}
			else {
				ffscMsgS("Write to FD %d for OOB msg failed",
					 FD);
			}

			return ERROR;
		}

		Offset += WriteLen;
		MsgLen -= WriteLen;
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
 * oob
 *	Decodes an Out Of Band message
 */
STATUS
oob(unsigned char *Msg)
{
	int DataLen;
	int i;
	int NonPrint = 0;
	unsigned char Checksum;

	/* Make sure a pointer was specified */
	if (Msg == NULL) {
		ffscMsg("Usage: oob <ptr>");
		return ERROR;
	}

	/* See if the response is printable */
	DataLen = OOBMSG_DATALEN(Msg);
	for (i = 0;  i < DataLen;  ++i) {
		if (!isprint(Msg[4 + i]) &&
		    !(Msg[4 + i] == '\0'  &&  i == DataLen-1))
		{
			NonPrint = 1;
		}
	}

	/* Go ahead and calculate the checksum */
	Checksum = oobChecksumMsg(Msg);

	/* Print what we know */
	ffscMsg("Code/Status:   %x", Msg[1]);
	ffscMsg("Data length:   %d", OOBMSG_DATALEN(Msg));
	ffscMsg("Checksum:      %x (%s)",
		Msg[DataLen + 4],
		(Checksum == OOBMSG_SUM(Msg)) ? "valid" : "invalid");

	if (DataLen == 0) {
		ffscMsg("No data.");
	}
	else {
		ffscMsgN("Data:");

		for (i = 0;  i < DataLen;  ++i) {
			ffscMsgN(" %02x", Msg[4 + i]);
		}
		ffscMsg("");

		if (!NonPrint) {
			Msg[DataLen + 4] = '\0';
			ffscMsg("      \"%s\"", Msg + 4);
		}
	}

	return OK;
}


/*
 * Some convenient hardcoded OOB commands
 */

/* Start a graph with 4 bars & depth 2 */
unsigned char oobGRAPH_START[] = {
	OBP_CHAR,		/* Prefix */
	GRAPH_START,		/* Opcode */
	0, 2,			/* Data length */
	4,			/* 4 bars */
	2,			/* Depth 2 */
	9			/* Checksum */
};

/* End the current graph */
unsigned char oobGRAPH_END[] = {
	OBP_CHAR,		/* Prefix */
	GRAPH_END,		/* Opcode */
	0, 0,			/* Data length */
	2			/* Checksum */
};

/* Add menu item */
unsigned char oobGRAPH_MENU_ADD[] = {
	OBP_CHAR,		/* Prefix */
	GRAPH_MENU_ADD,		/* Opcode */
	0, 5,			/* Data length */
	'T', 'e', 's', 't', 0,	/* String */
	0xa8			/* Checksum */
};

/* Get current menu selection */
unsigned char oobGRAPH_MENU_GET[] = {
	OBP_CHAR,		/* Prefix */
	GRAPH_MENU_GET,		/* Opcode */
	0, 0,			/* Data length */
	4			/* Checksum */
};

/* Set graph title */
unsigned char oobGRAPH_LABEL_TITLE[] = {
	OBP_CHAR,		/* Prefix */
	GRAPH_LABEL_TITLE,	/* Opcode */
	0, 6,			/* Data length */
	'T', 'i', 't', 'l', 'e', 0,	/* String */
	0x0d			/* Checksum */
};

/* Set horizontal axis title */
unsigned char oobGRAPH_LABEL_HORIZ[] = {
	OBP_CHAR,		/* Prefix */
	GRAPH_LABEL_HORIZ,	/* Opcode */
	0, 6,			/* Data length */
	'T', 'i', 't', 'l', 'e', 0,	/* String */
	0x0e			/* Checksum */
};

/* Set vertical axis title */
unsigned char oobGRAPH_LABEL_VERT[] = {
	OBP_CHAR,		/* Prefix */
	GRAPH_LABEL_VERT,	/* Opcode */
	0, 6,			/* Data length */
	'T', 'i', 't', 'l', 'e', 0,	/* String */
	0x0f			/* Checksum */
};

/* Set minimum value label */
unsigned char oobGRAPH_LABEL_MIN[] = {
	OBP_CHAR,		/* Prefix */
	GRAPH_LABEL_MIN,	/* Opcode */
	0, 3,			/* Data length */
	'0', '%', 0,		/* String */
	0x60			/* Checksum */
};

/* Set minimum value label */
unsigned char oobGRAPH_LABEL_MAX[] = {
	OBP_CHAR,		/* Prefix */
	GRAPH_LABEL_MAX,	/* Opcode */
	0, 5,			/* Data length */
	'1', '0', '0', '%', 0,	/* String */
	0xc4			/* Checksum */
};

/* Add legend */
unsigned char oobGRAPH_LEGEND_ADD[] = {
	OBP_CHAR,		/* Prefix */
	GRAPH_LEGEND_ADD,	/* Opcode */
	0, 6,			/* Data length */
	'T', 'i', 't', 'l', 'e', 0,	/* String */
	0x12			/* Checksum */
};

/* Graph data */
unsigned char oobGRAPH_DATA[] = {
	OBP_CHAR,		/* Prefix */
	GRAPH_DATA,		/* Opcode */
	0, 10,			/* Data length */
	0, 4, 60, 30, 20, 40, 38, 27, 80, 5,	/* Data items */
	0x45			/* Checksum */
};

/* FFSC command */
unsigned char oobFFSC_COMMAND[] = {
	OBP_CHAR,		/* Prefix */
	FFSC_COMMAND,		/* Opcode */
	0, 4,			/* Data length */
	'v', 'e', 'r', 0,	/* String */
	0x8e			/* Checksum */
};

#endif /* !PRODUCTION */
