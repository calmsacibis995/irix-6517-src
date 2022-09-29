/* Generate a fairly animated graphics display */

#include <sys/types.h>
#include <sys/stat.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termio.h>
#include <unistd.h>

typedef int STATUS;
#define OBP_CHAR '\xa0'
#define OBP_STR  "\xa0"

#include "oobmsg.h"

void alarm_handler(void);
unsigned char checksum(char *);
void data_command(unsigned char *, int, int);
void finish(int);
void hup_handler(int);
void send_command(unsigned char *);
void string_command(unsigned char *, int, char *);


char   LastMsg[81];
int    GotTermIO;
int    ReadFD, WriteFD;
struct termio OriginalTermIO;

/* Message logging */
FILE *LogFile;
void Log(const char *, ...);
void LogS(const char *, ...);

int
main(int argc, unsigned char **argv)
{
	char *Horiz = NULL;
	char *Legend[8];
	char *LogName = NULL;
	char *Max = NULL;
	char *Min = NULL;
	char *Title = "Dancing Graph";
	char *TTY = NULL;
	char *Vert = NULL;
	int  ArgErr = 0;
	int  DataLen;
	int  Depth = 4;
	int  i;
	int  Iterations = 5;
	int  MaxData;
	int  MsgLen;
	int  NumBars = 8;
	int  NumLegend = 0;
	int  PauseTicks = 10;
	int  ReadLen;
	struct termio TermIO;
	unsigned char Msg[1100];

	/* Parse the arguments */
	for (i = 1;  i < argc;  ++i) {
		if (strcmp(argv[i], "-d") == 0) {
			++i;
			if (i >= argc) {
				fprintf(stderr, "Must specify arg with -d\n");
				ArgErr = 1;
				continue;
			}

			Depth = atoi(argv[i]);
		}
		else if (strcmp(argv[i], "-h") == 0  ||
			 strcmp(argv[i], "-horz") == 0)
		{
			++i;
			if (i >= argc) {
				fprintf(stderr, "Must specify arg with -h\n");
				ArgErr = 1;
				continue;
			}

			Horiz = argv[i];
		}
		else if (strcmp(argv[i], "-i") == 0) {
			++i;
			if (i >= argc) {
				fprintf(stderr, "Must specify arg with -i\n");
				ArgErr = 1;
				continue;
			}

			Iterations = atoi(argv[i]);
		}
		else if (strcmp(argv[i], "-l") == 0  ||
			 strcmp(argv[i], "-legend") == 0)
		{
			if (NumLegend == 8) {
				fprintf(stderr, "Too many legend strings\n");
				ArgErr = 1;
				continue;
			}

			++i;
			if (i >= argc) {
				fprintf(stderr, "Must specify arg with -l\n");
				ArgErr = 1;
				continue;
			}

			Legend[NumLegend] = argv[i];
			++NumLegend;
		}
		else if (strcmp(argv[i], "-L") == 0) {
			++i;
			if (i >= argc) {
				fprintf(stderr, "Must specify arg with -L\n");
				ArgErr = 1;
				continue;
			}

			LogName = argv[i];
		}
		else if (strcmp(argv[i], "-max") == 0) {
			++i;
			if (i >= argc) {
				fprintf(stderr,
					"Must specify arg with -max\n");
				ArgErr = 1;
				continue;
			}

			Max = argv[i];
		}
		else if (strcmp(argv[i], "-min") == 0) {
			++i;
			if (i >= argc) {
				fprintf(stderr,
					"Must specify arg with -min\n");
				ArgErr = 1;
				continue;
			}

			Min = argv[i];
		}
		else if (strcmp(argv[i], "-n") == 0) {
			++i;
			if (i >= argc) {
				fprintf(stderr, "Must specify arg with -n\n");
				ArgErr = 1;
				continue;
			}

			NumBars = atoi(argv[i]);
		}
		else if (strcmp(argv[i], "-p") == 0) {
			++i;
			if (i >= argc) {
				fprintf(stderr, "Must specify arg with -p\n");
				ArgErr = 1;
				continue;
			}

			PauseTicks = atoi(argv[i]);
		}
		else if (strcmp(argv[i], "-t") == 0  ||
			 strcmp(argv[i], "-title") == 0)
		{
			++i;
			if (i >= argc) {
				fprintf(stderr, "Must specify arg with -t\n");
				ArgErr = 1;
				continue;
			}

			Title = argv[i];
		}
		else if (strcmp(argv[i], "-tty") == 0) {
			++i;
			if (i >= argc) {
				fprintf(stderr,
					"Must specify arg with -tty\n");
				ArgErr = 1;
				continue;
			}

			TTY = argv[i];
		}
		else if (strcmp(argv[i], "-v") == 0  ||
			 strcmp(argv[i], "-vert") == 0)
		{
			++i;
			if (i >= argc) {
				fprintf(stderr, "Must specify arg with -v\n");
				ArgErr = 1;
				continue;
			}

			Vert = argv[i];
		}
		else {
			fprintf(stderr,
				"Don't recognize option \"%s\"\n",
				argv[i]);
			ArgErr = 1;
		}
	}

	/* If an error occured, bail out */
	if (ArgErr) {
		finish(1);
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

	/* Open up the TTY if one was specified */
	if (TTY != NULL) {
		ReadFD = open(TTY, O_RDWR);
		if (ReadFD < 0) {
			LogS("Unable to open TTY %s", TTY);
			finish(2);
		}
		WriteFD = ReadFD;
		Log("Communicating with FD %d", ReadFD);
	}
	else {
		ReadFD = 0;
		WriteFD = 1;
		Log("Communicating with stdin/stdout");
	}

	/* Set up the alarm handler */
	if (signal(SIGALRM, alarm_handler) == SIG_ERR) {
		LogS("Couldn't set SIGALRM handler");
		finish(2);
	}

	/* Set up a hangup handler */
	if (signal(SIGHUP, hup_handler) == SIG_ERR  ||
	    signal(SIGINT, hup_handler) == SIG_ERR  ||
	    signal(SIGTERM, hup_handler) == SIG_ERR)
	{
		LogS("Couldn't set hup handler(s)");
		finish(2);
	}

	/* Turn off canonical mode while we are working */
	if (ioctl(ReadFD, TCGETA, &TermIO) < 0) {
		LogS("Unable to get termio info for stdin");
		finish(2);
	}

	GotTermIO = 1;
	OriginalTermIO = TermIO;

	TermIO.c_iflag &= ~(BRKINT|ISTRIP|ICRNL|INLCR|IGNCR|IXON|IXOFF);
        TermIO.c_oflag &= ~(OPOST|TABDLY);
	TermIO.c_cflag |= CLOCAL;
        TermIO.c_lflag &= ~(ISIG|ICANON|ECHO);
        TermIO.c_cc[VMIN]  = 1;
        TermIO.c_cc[VTIME] = 1;

	if (ioctl(ReadFD, TCSETAW, &TermIO) < 0) {
		LogS("Unable to set termio info for stdin");
		finish(2);
	}

	/* Set up the graph */
	Log("Sending GRAPH_START %d %d", NumBars, Depth);
	Msg[4] = NumBars;
	Msg[5] = Depth;
	data_command(Msg, GRAPH_START, 2);

	if (Title != NULL) {
		Log("Sending LABEL_TITLE \"%s\"", Title);
		string_command(Msg, GRAPH_LABEL_TITLE, Title);
	}

	if (Horiz != NULL) {
		Log("Sending LABEL_HORIZ \"%s\"", Horiz);
		string_command(Msg, GRAPH_LABEL_HORIZ, Horiz);
	}

	if (Vert != NULL) {
		Log("Sending LABEL_VERT \"%s\"", Vert);
		string_command(Msg, GRAPH_LABEL_VERT, Vert);
	}

	if (Min != NULL) {
		Log("Sending LABEL_MIN \"%s\"", Min);
		string_command(Msg, GRAPH_LABEL_MIN, Min);
	}

	if (Max != NULL) {
		Log("Sending LABEL_MAX \"%s\"", Max);
		string_command(Msg, GRAPH_LABEL_MAX, Max);
	}

	for (i = 0;  i < NumLegend;  ++i) {
		Log("Sending LEGEND_ADD \"%s\"", Legend[i]);
		string_command(Msg, GRAPH_LEGEND_ADD, Legend[i]);
	}

	Msg[4] = 0;
	Msg[5] = NumBars;
	MaxData = 256 / Depth;
	for (i = 0;  i < Iterations;  ++i) {
		int j, k, Entry, Sum;

		Entry = 0;
		for (j = 0;  j < NumBars;  ++j) {
			Sum = 0;
			for (k = 0;  k < Depth;  ++k) {
				Sum += (rand() % MaxData);
				if (Sum > 255) {
					Sum = 255;
				}
				Msg[6 + Entry] = Sum;
				++Entry;
			}
		}

#ifdef TESTING
		printf("Last bar in iteration %d:", i);
		Entry = 6 + (NumBars - 1) * Depth;
		for (j = 0;  j < Depth;  ++j) {
			printf(" %3d", Msg[Entry + j]);
		}
		printf("\r\n");
		fflush(stdout);
#endif
			
		Log("Sending GRAPH_DATA %d", (NumBars * Depth) + 2);
		data_command(Msg, GRAPH_DATA, (NumBars * Depth) + 2);

		sginap(PauseTicks);
	}

	/* It is now safe to restore the terminal state once and for all */
	if (ioctl(ReadFD, TCSETAF, &OriginalTermIO) != 0) {
		LogS("Unable to restore termio settings");
		finish(2);
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
data_command(unsigned char *Msg, int OpCode, int DataLen)
{
	/* Build the message itself */
	Msg[0] = 0xa0;
	Msg[1] = OpCode;
	Msg[2] = DataLen / 256;
	Msg[3] = DataLen % 256;
	Msg[DataLen + 4] = checksum(Msg);

	/* Ship it off */
	send_command(Msg);
}

void
finish(int Code)
{
	if (LogFile != NULL) {
		fclose(LogFile);
	}

	if (GotTermIO) {
		ioctl(ReadFD, TCSETAF, &OriginalTermIO);
	}

	if (LastMsg[0] != '\0') {
		fprintf(stderr, LastMsg);
	}

	exit(Code);
}

void
hup_handler(int Signal)
{
	Log("Got signal %d", Signal);
	finish(3);
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

		strcat(MsgBuf, "\n");

		if (LogFile != NULL) {
			fprintf(LogFile, MsgBuf);
		}

		strncpy(LastMsg, MsgBuf, 80);
		LastMsg[80] = '\0';
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
		strcat(MsgBuf, "\n");

		if (LogFile != NULL) {
			fprintf(LogFile, MsgBuf);
		}

		strncpy(LastMsg, MsgBuf, 80);
		LastMsg[80] = '\0';
	}
}

void
send_command(unsigned char *Msg)
{
	int DataLen;
	int MsgLen;
	int ReadLen;
	int Result;
	struct termio TermIO, OriginalTermIO;
	unsigned char CheckSum;

	/* Calculate some details */
	DataLen = Msg[2] * 256 + Msg[3];
	MsgLen = DataLen + 5;

	/* Write the message out */
	if (write(WriteFD, Msg, MsgLen) != MsgLen) {
		LogS("\n\nIncomplete write");
		finish(2);
	}

	/* Read the response header */
	ReadLen = 0;
	while (ReadLen < 4) {
		alarm(5);
		Result = read(ReadFD, Msg + ReadLen, 4 - ReadLen);
		alarm(0);
		if (Result < 0) {
			if (errno == EINTR) {
				Log("Timed out waiting for header");
			}
			else {
				LogS("Error on read");
			}

			finish(1);
		}
		if (Result == 0) {
			Log("EOF on stdin!");
			finish(1);
		}
		ReadLen += Result;
	}

	/* Validate the response header */
	if (Msg[0] != '\xa0') {
		Log("Invalid response prefix %02x", Msg[0]);
		finish(1);
	}

	/* Read the rest of the response */
	DataLen = Msg[2] * 256 + Msg[3];
	MsgLen  = DataLen + 5;
	while (ReadLen < MsgLen) {
		alarm(5);
		Result = read(ReadFD, Msg + ReadLen, MsgLen - ReadLen);
		alarm(0);
		if (Result < 0) {
			if (errno == EINTR) {
				Log("Timed out waiting for body");
			}
			else {
				LogS("Error on read");
			}

			finish(1);
		}
		if (Result == 0) {
			Log("EOF on stdin!");
			finish(1);
		}
		ReadLen += Result;
	}

	/* Validate the checksum */
	CheckSum = checksum(Msg);
	if (CheckSum != Msg[DataLen + 4]) {
		Log("Invalid checksum in response, was %x should be %x",
		    Msg[DataLen + 4], CheckSum);
		finish(1);
	}

	/* Make sure the status is OK */
	if (Msg[1] != STATUS_NONE) {
		Log("Bad return status: %d", Msg[1]);
		finish(1);
	}
}


void
string_command(unsigned char *Msg, int OpCode, char *String)
{
	int DataLen = strlen(String) + 1;

	bcopy(String, &Msg[4], DataLen);
	data_command(Msg, OpCode, DataLen);
}
