/* ELSC emulator */

#include <sys/types.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <termio.h>
#include <unistd.h>

#define DEFAULT_MODNUM   1
#define DEFAULT_PASSWORD "none"
#define DEFAULT_PORT	 "/dev/ttyd2"

#define INTRO_MSG    "\024msc test program\r\n"


char Password[5] = DEFAULT_PASSWORD;
int  ModuleNum = DEFAULT_MODNUM;
int  PortFD;


void ProcessELSCInput(void);
void ProcessFFSCInput(void);


int
main(int argc,  char **argv)
{
	char *PortName = DEFAULT_PORT;
	fd_set ReadFDs, ReadyFDs;
	int ArgErr = 0;
	int EchoOn;
	int i;
	int Result;
	struct termio TermIO;

	/* Parse the arguments */
	for (i = 1;  i < argc;  ++i) {
		if (strcmp(argv[i], "-m") == 0) {
			++i;
			if (i >= argc) {
				fprintf(stderr, "Must specify arg with -m\n");
				ArgErr = 1;
				continue;
			}

			ModuleNum = atoi(argv[i]);
		}
		else if (strcmp(argv[i], "-p") == 0) {
			++i;
			if (i >= argc) {
				fprintf(stderr,
					"Must specify arg with -p\n");
				ArgErr = 1;
				continue;
			}

			strncpy(Password, argv[i], 4);
			Password[4] = '\0';
		}
		else if (strcmp(argv[i], "-tty") == 0) {
			++i;
			if (i >= argc) {
				fprintf(stderr,
					"Must specify arg with -tty\n");
				ArgErr = 1;
				continue;
			}

			PortName = argv[i];
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
		fprintf(stderr,
			"Usage: %s [-m modnum] [-p password] [-tty dev]",
			argv[0]);
		exit(1);
	}

	/* Open the serial port and set the appropriate modes */
	PortFD = open(PortName, O_RDWR);
	if (PortFD < 0) {
		fprintf(stderr,
			"Unable to open %s: %s\n",
			PortName, strerror(errno));
		exit(1);
	}

	if (ioctl(PortFD, TCGETA, &TermIO) < 0) {
		perror("Unable to get termio info");
		exit(2);
	}

	TermIO.c_iflag &= ~(BRKINT|ISTRIP|ICRNL|INLCR|IGNCR|IXON|IXOFF);
        TermIO.c_oflag &= ~(OPOST|TABDLY);
        TermIO.c_lflag &= ~(ISIG|ICANON|ECHO);
        TermIO.c_cc[VMIN]  = 1;
        TermIO.c_cc[VTIME] = 1;

	if (ioctl(PortFD, TCSETAW, &TermIO) < 0) {
		perror("Unable to set termio info");
		exit(2);
	}

	if (ioctl(0, TCGETA, &TermIO) < 0) {
		perror("Unable to get termio info for stdin");
		exit(2);
	}

	TermIO.c_iflag &= ~(BRKINT|ISTRIP|ICRNL|INLCR|IGNCR|IXON|IXOFF);
        TermIO.c_oflag &= ~(OPOST|TABDLY);
        TermIO.c_lflag &= ~(ISIG|ICANON|ECHO);
        TermIO.c_cc[VMIN]  = 1;
        TermIO.c_cc[VTIME] = 1;

	if (ioctl(0, TCSETAW, &TermIO) < 0) {
		perror("Unable to set termio info for stdin");
		exit(2);
	}

	/* Announce our existence, which should provoke an "fsc" command */
	/* from the FFSC						 */
	if (write(PortFD, INTRO_MSG, strlen(INTRO_MSG)) != strlen(INTRO_MSG)) {
		perror("Initial write to host failed");
		exit(1);
	}

	/* Prepare to wait for input from either stdin or the FFSC */
	FD_ZERO(&ReadFDs);
	FD_SET(0, &ReadFDs);
	FD_SET(PortFD, &ReadFDs);

	/* Read from input forever */
	while (1) {

		/* Wait for something interesting to happen */
		ReadyFDs = ReadFDs;
		Result = select(getdtablehi(), &ReadyFDs, NULL, NULL, NULL);
		if (Result <= 0) {
			perror("select failed");
			exit(2);
		}

		/* See if there is "ELSC" input */
		if (FD_ISSET(0, &ReadyFDs)) {
			ProcessELSCInput();
		}

		/* See if there is FFSC input */
		if (FD_ISSET(PortFD, &ReadyFDs)) {
			ProcessFFSCInput();
		}
	}

	exit(255);
}


void
ProcessELSCInput(void)
{
	static char ELSCMsg[81];
	static char *ELSCBufPtr = ELSCMsg;
	static int  MsgLen = 0;

	/* Read the next character */
	if (read(0, ELSCBufPtr, 1) != 1) {
		perror("I/O error from stdin");
		exit(2);
	}

	/* Ctrl-C means quit */
	if (*ELSCBufPtr == '\003') {
		printf("ELSC Emulator Exiting\r\n");
		exit(0);
	}

	/* Ctrl-B: send a message ending with LF/CF */
	if (*ELSCBufPtr == '\002') {
		*ELSCBufPtr = '\0';
		printf("\r\nHub msg, LF/CR: \"%s\"\r\n", ELSCMsg);

		ELSCMsg[MsgLen] = '\n';
		ELSCMsg[MsgLen+1] = '\r';
		MsgLen += 2;

		write(PortFD, ELSCMsg, MsgLen);
		ELSCBufPtr = ELSCMsg;
		MsgLen = 0;
	}

	/* Ctrl-E: write a single CR character */
	else if (*ELSCBufPtr == '\005') { /* Ctrl-E -> CR */
		*ELSCBufPtr = '\015';
		++ELSCBufPtr;
		++MsgLen;
		write(1, "^M", 2);
	}

	/* Ctrl-L: write a single LF character */
	else if (*ELSCBufPtr == '\014') { /* Ctrl-L -> LF */
		*ELSCBufPtr = '\012';
		++ELSCBufPtr;
		++MsgLen;
		write(1, "^J", 2);
	}

	/* Ctrl-N: write a message with no ending CR/LF */
	else if (*ELSCBufPtr == '\016') {
		*ELSCBufPtr = '\0';
		printf("\r\Hub msg, no CR/LF: \"%s\"\r\n",
		       ELSCMsg);
		write(PortFD, ELSCMsg, MsgLen);
		ELSCBufPtr = ELSCMsg;
		MsgLen = 0;
	}

	/* Ctrl-O: write a single NUL character */
	else if (*ELSCBufPtr == '\017') { /* Ctrl-O -> NUL */
		*ELSCBufPtr = '\0';
		++ELSCBufPtr;
		++MsgLen;
		write(1, "^@", 2);
	}

	/* Ctrl-R: write a message with an ending CR only */
	else if (*ELSCBufPtr == '\022') {
		*ELSCBufPtr = '\0';
		printf("\r\Hub msg, CR only: \"%s\"\r\n",
		       ELSCMsg);

		ELSCMsg[MsgLen] = '\r';
		++MsgLen;

		write(PortFD, ELSCMsg, MsgLen);
		ELSCBufPtr = ELSCMsg;
		MsgLen = 0;
	}

	/* CR: write a message with a normal ending CR/LF */
	else if (*ELSCBufPtr == '\r') {
		*ELSCBufPtr = '\0';
		printf("\r\Hub msg, CR/LF: \"%s\"\r\n",
		       ELSCMsg);

		ELSCMsg[MsgLen] = '\r';
		ELSCMsg[MsgLen+1] = '\n';
		MsgLen += 2;

		write(PortFD, ELSCMsg, MsgLen);
		ELSCBufPtr = ELSCMsg;
		MsgLen = 0;
	}

	/* Otherwise, simply echo the current character to stdout */
	else {
		write(1, ELSCBufPtr, 1);
		++ELSCBufPtr;
		++MsgLen;
	}
}

void
ProcessFFSCInput(void)
{
	char Response[80];
	int  RespLen;
	static char FFSCMsg[81];
	static char *FFSCBufPtr = FFSCMsg;
	static int  EchoOn = 1;
	static int  PasswordSet;

	/* Read the next character from the ELSC port */
	if (read(PortFD, FFSCBufPtr, 1) != 1) {
		perror("I/O error from FFSC");
		exit(2);
	}

	/* If the next character happens to be Ctrl-T, echo a prompt */
	if (*FFSCBufPtr == '\024'  && FFSCBufPtr == FFSCMsg) {
		if (EchoOn) {
			write(PortFD, "SC>", 3);
		}
		++FFSCBufPtr;
		return;
	}

	/* If the next character is *not* CR, then simply echo it */
	if (*FFSCBufPtr != '\r') {
		if (EchoOn) {
			if (write(PortFD, FFSCBufPtr, 1) != 1)
			{
				perror("Echo failed");
				exit(2);
			}
		}
		++FFSCBufPtr;
		return;
	}

	/* If we make it to here, we have reached the end of a message */
	/* from the FFSC. Process it accordingly.		       */

	/* Terminate the string and rewind the buffer pointer */
	*FFSCBufPtr = '\0';
	FFSCBufPtr = FFSCMsg;

	/* If echo mode is turned on, echo a CR/LF */
	if (EchoOn) {
		write(PortFD, "\r\n", 2);
	}

	/* If the message doesn't begin with a Ctrl-T, simply swallow it */
	if (FFSCMsg[0] != '\024') {
		printf("Passthru \"%s\"\r\n", FFSCMsg);
		return;
	}

	/* We apparently have a real, live ELSC command. Say what came in. */
	printf("ELSC Command \"%s\"\r\n", &FFSCMsg[1]);

	/* Assume a positive response for now */
	sprintf(Response, "\024ok\r\n");

	/* "ver" command: respond with a made-up version number */
	if (strcmp(FFSCMsg, "\024ver") == 0) {
		sprintf(Response, "\024ok 1.0\r\n");
	}

	/* "ech 0": turn off input echo */
	else if (strcmp(FFSCMsg, "\024ech 0") == 0) {
		EchoOn = 0;
	}

	/* "ech 1": turn on input echo */
	else if (strcmp(FFSCMsg, "\024ech 1") == 0) {
		EchoOn = 1;
	}

	/* "mod": return the module number */
	else if (strcmp(FFSCMsg, "\024mod") == 0) {
		sprintf(Response, "\024ok %x\r\n", ModuleNum);
	}

	/* "mod <n>": set the module number */
	else if (strncmp(FFSCMsg, "\024mod", 4) == 0) {
		sscanf(FFSCMsg + 5, "%x", &ModuleNum);
		sprintf(Response, "\024ok %x\r\n", ModuleNum);
	}

	/* "pas s": invalid argument (need a password) */
	else if (strcmp(FFSCMsg, "\024pas s") == 0) {
		sprintf(Response, "\024err arg\r\n");
	}

	/* "pas s <pw>": set the password */
	else if (strncmp(FFSCMsg, "\024pas s", 6) == 0) {
		sscanf(FFSCMsg + 7, "%s", Password);
	}

	/* "pas": turn off supervisor mode */
	else if (strcmp(FFSCMsg, "\024pas") == 0) {
		PasswordSet = 0;
	}

	/* "pas <pw>": turn on supervisor mode */
	else if (strncmp(FFSCMsg, "\024pas", 4) == 0) {
		char NewPassword[80];

		sscanf(FFSCMsg + 5, "%s", NewPassword);
		if (strcmp(NewPassword, Password) != 0) {
			sprintf(Response, "\024err perm" "\r\n");
			PasswordSet = 0;
		}
		else {
			PasswordSet = 1;
		}
	}

	/* "pwr u": destructive command with a weird response */
	else if (strcmp(FFSCMsg, "\024pwr u") == 0) {
		char DspInfo[80];
		int  InfoLen;

		if (!PasswordSet) {
			sprintf(Response, "\024err perm" "\r\n");
		}

		sprintf(DspInfo, "\024dp S:%s\r\n", FFSCMsg+1);
		InfoLen = strlen(DspInfo);
		if (write(PortFD, DspInfo, InfoLen) != InfoLen) {
			perror("dsp info failed");
			exit(2);
		}
	}

	/* "pwr", "rst", "nmi": destructive commands (need supervisor mode) */
	else if (strcmp(FFSCMsg, "\024rst") == 0  ||
		 strncmp(FFSCMsg, "\024pwr", 4) == 0  ||
		 strcmp(FFSCMsg, "\024nmi") == 0)
	{
		char DspInfo[80];
		int  InfoLen;

		if (!PasswordSet) {
			sprintf(Response, "\024err perm" "\r\n");
		}

		sprintf(DspInfo, "\024dsp S:%s\r\n", FFSCMsg+1);
		InfoLen = strlen(DspInfo);
		if (write(PortFD, DspInfo, InfoLen) != InfoLen) {
			perror("dsp info failed");
			exit(2);
		}
	}

	/* Send our response back to the FFSC */
	RespLen = strlen(Response);
	if (write(PortFD, Response, RespLen) != RespLen) {
		perror("Response failed");
		exit(2);
	}
}
