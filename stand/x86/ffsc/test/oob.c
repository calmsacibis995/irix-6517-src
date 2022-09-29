/* Print an OOB message */

#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termio.h>

typedef int STATUS;
#define OBP_CHAR '\xa0'
#define OBP_STR  "\xa0"

#include "oobmsg.h"

void alarm_handler(void);
unsigned char checksum(char *);
void hup(int);

typedef struct cmdinfo {
	char *Name;
	int  Cmd;
	int  ArgType;
} cmdinfo_t;

#define ARG_NONE	0
#define ARG_STRING	1
#define ARG_INTS	2

cmdinfo_t CmdList[] = {
	{ "START",	GRAPH_START,		ARG_INTS },
	{ "END",	GRAPH_END,		ARG_NONE },
	{ "ADD",	GRAPH_MENU_ADD,		ARG_STRING },
	{ "GET",	GRAPH_MENU_GET,		ARG_NONE },
	{ "TITLE",	GRAPH_LABEL_TITLE,	ARG_STRING },
	{ "HORIZ",	GRAPH_LABEL_HORIZ,	ARG_STRING },
	{ "VERT",	GRAPH_LABEL_VERT,	ARG_STRING },
	{ "MIN",	GRAPH_LABEL_MIN,	ARG_STRING },
	{ "MAX",	GRAPH_LABEL_MAX,	ARG_STRING },
	{ "LEGEND",	GRAPH_LEGEND_ADD,	ARG_STRING },
	{ "DATA",	GRAPH_DATA,		ARG_INTS },
	{ "FFSC",	FFSC_COMMAND,		ARG_STRING },
	{ "INIT",	FFSC_INIT,		ARG_INTS },
	{ "NOP",	OOB_NOP,		ARG_NONE },
	{ NULL, -1, -1 },
};

/* Message logging */
FILE *LogFile;
void Log(const char *, ...);
void LogS(const char *, ...);


int
main(int argc, unsigned char **argv)
{
	char *Command = NULL;
	char *LogName = NULL;
	int  ArgErr = 0;
	int  BadCheckSum = 0;
	int  BadLen = 0;
	int  DataLen;
	int  i;
	int  MsgLen;
	int  NonPrint;
	int  OpCode = 0;
	int  Quiet = 0;
	int  ReadLen;
	int  Result;
	int  ValStart = 0;
	int  Verbose = 0;
	struct termio TermIO, OriginalTermIO;
	unsigned char CheckSum;
	unsigned char *Data = "";
	unsigned char Msg[100];

	/* Parse the arguments */
	for (i = 1;  i < argc;  ++i) {
		if (strcmp(argv[i], "-bl") == 0) {
			BadLen = 1;
		}
		else if (strcmp(argv[i], "-bs") == 0  ||
			 strcmp(argv[i], "-bc") == 0)
		{
			BadCheckSum = 1;
		}
		else if (strcmp(argv[i], "-c") == 0) {
			++i;
			if (i >= argc) {
				fprintf(stderr, "Must specify arg with -c\n");
				ArgErr = 1;
				continue;
			}

			Command = argv[i];
		}
		else if (strcmp(argv[i], "-d") == 0) {
			++i;
			if (i >= argc) {
				fprintf(stderr, "Must specify arg with -d\n");
				ArgErr = 1;
				continue;
			}

			Data = argv[i];
		}
		else if (strcmp(argv[i], "-l") == 0) {
			++i;
			if (i >= argc) {
				fprintf(stderr, "Must specify arg with -l\n");
				ArgErr = 1;
				continue;
			}

			LogName = argv[i];
		}
		else if (strcmp(argv[i], "-o") == 0) {
			++i;
			if (i >= argc) {
				fprintf(stderr, "Must specify arg with -o\n");
				ArgErr = 1;
				continue;
			}

			OpCode = atoi(argv[i]);
		}
		else if (strcmp(argv[i], "-q") == 0) {
			Quiet = 1;
		}
		else if (strcmp(argv[i], "-v") == 0) {
			Verbose = 1;
		}
		else if (argv[i][0] == '-') {
			fprintf(stderr,
				"Don't recognize option \"%s\"\n",
				argv[i]);
			ArgErr = 1;
		}
		else {
			ValStart = i;
			break;
		}
	}

	/* If an error occured, bail out */
	if (ArgErr) {
		exit(1);
	}

	/* Set up a log file if desired */
	if (LogName != NULL) {
		LogFile = fopen(LogName, "w");
		if (LogFile == NULL) {
			perror("Unable to open output file");
		}
		else {
			Log("Logging to %s", LogName);
		}
	}

	/* If a command was specified, compose a message from the remaining */
	/* command line arguments.					    */
	if (Command != NULL) {
		int ArgType;

		/* Determine the opcode and argument type */
		OpCode = -1;
		for (i = 0;  CmdList[i].Name != NULL  &&  OpCode < 0;  ++i) {
			if (strcasecmp(CmdList[i].Name, Command) == 0) {
				OpCode  = CmdList[i].Cmd;
				ArgType = CmdList[i].ArgType;
			}
		}

		/* Bail out if we couldnt' find the command */
		if (OpCode < 0) {
			fprintf(stderr,
				"Don't know command \"%s\"\n",
				Command);
			exit(1);
		}

		/* Generate argument data */
		switch (ArgType) {

		    case ARG_NONE:
			DataLen = 0;
			break;

		    case ARG_INTS:
			DataLen = 0;
			if (ValStart > 0) {
				for (i = ValStart;  i < argc;  ++i) {
					Msg[4 + DataLen] = atoi(argv[i]) & 255;
					++DataLen;
				}
			}
			break;

		    case ARG_STRING:
			if (ValStart > 0) {
				DataLen = strlen(argv[ValStart]) + 1;
				bcopy(argv[ValStart], &Msg[4], DataLen);
			}
			else {
				DataLen = 0;
			}
			break;

		    default:
			fprintf(stderr, "Don't know arg type %d\n", ArgType);
			exit(2);
		}

		/* Build the message itself */
		Msg[0] = 0xa0;
		Msg[1] = OpCode;
		Msg[2] = DataLen / 256;
		Msg[3] = DataLen % 256;

		CheckSum = checksum(Msg);
		Msg[DataLen + 4] = CheckSum;
	}
	else {

		/* Build a message manually */
		OpCode &= 255;
		DataLen = strlen(Data) + 1;

		bcopy(Data, &Msg[4], DataLen);
	}

	/* Build the message itself */
	Msg[0] = 0xa0;
	Msg[1] = OpCode;
	Msg[2] = DataLen / 256;
	Msg[3] = DataLen % 256;

	CheckSum = checksum(Msg);
	Msg[DataLen + 4] = CheckSum;

	/* Skew the checksum if desired */
	if (BadCheckSum) {
		Msg[DataLen + 4] = CheckSum + 1;
	}

	/* Determine the length of the entire message */
	MsgLen = DataLen + 5;
	if (BadLen) {
		--MsgLen;
	}

	/* Set up the alarm handler */
	if (signal(SIGALRM, alarm_handler) == SIG_ERR) {
		perror("Couldn't set SIGALRM handler");
		exit(2);
	}
	signal(SIGHUP, hup);
	signal(SIGINT, hup);
	signal(SIGTERM, hup);

	/* Say whats going on if desired */
	if (Verbose) {
		printf("Message info:\n");
		printf("    OpCode:   %x\n", OpCode);
		printf("    Data:     \"%s\"\n", Data);
		printf("    CheckSum: %x\n", CheckSum);
		printf("\n");
		printf("    DataLen:  %d\n", DataLen);
		printf("    MsgLen:   %d\n", MsgLen);
		printf("\n");

		printf("    Msg:");
		for (i = 0;  i < MsgLen;  ++i) {
			printf(" %02x", Msg[i]);
		}
		printf("\n");

		printf("\n");
		printf("Here is the OOB msg: ->");
		fflush(stdout);
	}

	/* Turn off canonical mode while we print the command */
	if (ioctl(0, TCGETA, &TermIO) < 0) {
		perror("Unable to get termio info for stdin");
		exit(2);
	}

	OriginalTermIO = TermIO;

	TermIO.c_iflag &= ~(BRKINT|ISTRIP|ICRNL|INLCR|IGNCR|IXON|IXOFF);
        TermIO.c_oflag &= ~(OPOST|TABDLY);
	TermIO.c_cflag |= CLOCAL;
        TermIO.c_lflag &= ~(ISIG|ICANON|ECHO);
        TermIO.c_cc[VMIN]  = 1;
        TermIO.c_cc[VTIME] = 1;

	if (ioctl(0, TCSETAW, &TermIO) < 0) {
		LogS("Unable to set termio info for stdin");
		exit(2);
	}

	/* Write the message out */
	if (write(1, Msg, MsgLen) != MsgLen) {
		ioctl(0, TCSETAW, &OriginalTermIO);
		LogS("\n\nIncomplete write");
		exit(2);
	}

	/* If a bad length was requested, wait a little while for */
	/* the FFSC to figure this out.				  */
	if (BadLen) {
		sleep(5);
	}

	/* Print a verbose trailer if necessary */
	if (Verbose) {
		write(1, "<-\r\n", 4);
	}

	/* Read the response header */
	ReadLen = 0;
	while (ReadLen < 4) {
		alarm(5);
		Result = read(0, Msg + ReadLen, 4 - ReadLen);
		alarm(0);
		if (Result < 0) {
			ioctl(0, TCSETAW, &OriginalTermIO);

			if (errno == EINTR) {
				Log("Timed out waiting for header\n");
			}
			else {
				LogS("Error on read");
			}

			exit(1);
		}
		if (Result == 0) {
			ioctl(0, TCSETAW, &OriginalTermIO);
			Log("EOF on stdin!\n");
			exit(1);
		}
		ReadLen += Result;
	}

	/* Validate the response header */
	if (Msg[0] != '\xa0') {
		ioctl(0, TCSETAW, &OriginalTermIO);
		Log("Invalid response prefix %02x\n", Msg[0]);
		exit(1);
	}

	/* Read the rest of the response */
	DataLen = Msg[2] * 256 + Msg[3];
	MsgLen  = DataLen + 5;
	while (ReadLen < MsgLen) {
		alarm(5);
		Result = read(0, Msg + ReadLen, MsgLen - ReadLen);
		alarm(0);
		if (Result < 0) {
			ioctl(0, TCSETAW, &OriginalTermIO);

			if (errno == EINTR) {
				Log("Timed out waiting for body\n");
			}
			else {
				LogS("Error on read");
			}

			exit(1);
		}
		if (Result == 0) {
			ioctl(0, TCSETAW, &OriginalTermIO);
			Log("EOF on stdin!\n");
			exit(1);
		}
		ReadLen += Result;
	}

	/* It is now safe to restore the terminal state once and for all */
	if (ioctl(0, TCSETAW, &OriginalTermIO) != 0) {
		LogS("Unable to restore termio settings");
		exit(2);
	}

	/* Validate the checksum */
	CheckSum = checksum(Msg);
	if (CheckSum != Msg[DataLen + 4]) {
		Log("Invalid checksum in response, was %x should be %x\n",
		    Msg[DataLen + 4], CheckSum);
		exit(1);
	}

	/* See if the response is printable */
	NonPrint = 0;
	for (i = 0;  i < DataLen;  ++i) {
		if (!isprint(Msg[4 + i]) &&
		    !(Msg[4 + i] == '\0'  &&  i == DataLen-1))
		{
			NonPrint = 1;
		}
	}

	/* Print the results if desired */
	if (Verbose) {
		Log("\n");
		Log("Response info:\n");
		Log("    Status:   %x\n", Msg[1]);
		Log("    Checksum: %x\n", Msg[DataLen + 4]);
		Log("\n");
		Log("    DataLen:  %d\n", DataLen);
		Log("    MsgLen:   %d\n", MsgLen);

		if (DataLen == 0) {
			Log("    No data.\n");
		}
		else {
			Log("    Data:    ");

			for (i = 0;  i < DataLen;  ++i) {
				Log(" %02x", Msg[4 + i]);
			}
			Log("\n");

			if (!NonPrint) {
				Msg[DataLen + 4] = '\0';
				Log("              \"%s\"\n", Msg + 4);
			}
		}
	}
	else if (!Quiet) {
		if (NonPrint) {
			Log("<unprintable response>\n");
		}
		else {
			Log("%s\n", Msg + 4);
		}
	}

	return Msg[1];
}


void
alarm_handler(void)
{
	/* NOP */
	return;
}


unsigned char
checksum(char *Msg)
{
	unsigned char Sum;
	int DataLen;
	int i;

	DataLen = Msg[2] * 256 + Msg[3];
	Sum = 0;
	for (i = 1;  i < DataLen + 4;  ++i) {
		Sum += Msg[i];
	}

	return Sum;
}


void
hup(int signal)
{
	Log("Got signal %d", signal);
	exit(5);
}


void
Log(const char *Format, ...)
{
	char MsgBuf[1000];
	va_list Args;

	if (Format != NULL) {
		va_start(Args, Format);
		vsprintf(MsgBuf, Format, Args);
		va_end(Args);

		if (LogFile != NULL) {
			fprintf(LogFile, MsgBuf);
		}
		fprintf(stderr, MsgBuf);
	}
}


void
LogS(const char *Format, ...)
{
	char MsgBuf[1000];
	va_list Args;

	if (Format != NULL) {
		va_start(Args, Format);
		vsprintf(MsgBuf, Format, Args);
		va_end(Args);

		strcat(MsgBuf, ": ");
		strcat(MsgBuf, strerror(errno));
		strcat(MsgBuf, "\r\n");

		if (LogFile != NULL) {
			fprintf(LogFile, MsgBuf);
		}
		fprintf(stderr, MsgBuf);
	}
}
