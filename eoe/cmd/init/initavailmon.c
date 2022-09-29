#ident  "$Revision: 1.10 $"

#include <stdio.h>
#include <signal.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>
#include <msgs/uxsgicore.h>


#define CONFIG_FILE 	"/var/adm/avail/.save/shutdownreason"
#define SCRATCH_FILE 	"/var/adm/avail/init.scratch"
#define LINE_SIZE 	128 	/* borrowed from cmd/chkconfig.c */
#define WAIT_TIME 	30 	/* seconds */


char *reasons[] = {
	"Administrative",
	"Hardware upgrade",
	"Software upgrade",
	"Fix/replace hardware",
	"Install patch",
	"Fix software problem",
	""
};


void
am_alarmclk() /* alarmclk for avail mon */
{
	fprintf(stderr, "\ninit: Warning! %d seconds expired in waiting to read reason for shutdown\n", WAIT_TIME);
}

/*
 * check whether config flag is turned on
 */
int
chkconfig(char *cfile)
{
	char	line[LINE_SIZE];
	FILE 	*fp;

	if ((fp = fopen(cfile, "r")) == NULL)
	   	return(0); /* no file == flag is off */
	/*
	 * check "1" or "0" (on or off).
	 */
	if ((fgets(line, LINE_SIZE, fp) == NULL) ||
	    (strncasecmp(line, "1", 1) != 0)) {
		fclose(fp);
		return(0);
	}
	fclose(fp);
	return(1);
}

/*
 * if init would go into a shutdown state,
 * then prompt user and obtain shutdown reason,
 * leave info in scratch file;
 * scratch file will be picked up by S95availmon.
 */
int
doavailmon(char state)
{
	char	line[LINE_SIZE];
	FILE 	*fp;
	int 	rcode, i;
	void	(*prevalrmfunc)();
	struct stat stbuf;

	switch (state) {
	case '0':
	case '6':
	case 'S':
	case 's':
		break; /* fall through, might have real work to do */
	default:
		return(0);
	}

	/*
	 * If SCRATCH_FILE exists, amsdreasons has been run before.
	 * If the file size is zero, amsdreasons was interrupted
	 * before getting the reason.
	 */
	if (stat(SCRATCH_FILE, &stbuf) == 0 && stbuf.st_size > 0) {
		return(0);
	}

	if ((fp = fopen(SCRATCH_FILE, "wb")) == NULL) {
		fprintf(stderr, "init:Warning, can't log shutdown reason.\n");
		perror(SCRATCH_FILE);
		return(-2);
	}

	if (!chkconfig(CONFIG_FILE)) {
		if ((state == 'S') || (state == 's')) {
			rcode = 2097191;
		} else {
			rcode = 2097169;
		}
		fprintf(fp, "%d\n", rcode);
		fclose(fp);
		return(rcode);
	}

	/*
	 * save previous alarm handler
	 */
	prevalrmfunc = signal(SIGALRM, am_alarmclk);
	/* set an alarm, so response may be timed */
	alarm(WAIT_TIME);
	/*
	 * if stdin is not a tty (pipe, as in case of being called from
	 * shutdown), do not print messges; simply read the response off stdin
	 */
	if (isatty(fileno(stdin))) {
		for(;;) {
			printf("\n****	Enter Reason for Shutdown  ****\n");
			printf("Please select one of the following choices by number:\n\n");
			for (i = 0; reasons[i][0] != NULL; i++)
				printf("%d. %s\n", i+1, reasons[i]);

			printf("\nEnter Number (within %d seconds): ",
				WAIT_TIME);
			/*
			 * get users response; system call may be interupted.
			 */
			fgets(line, LINE_SIZE, stdin);
			if (ferror(stdin)) {
				rcode = -1; /* timed out */
				break; /* out of while */
			}
			/*
			 * user entered response, check validity.
			 */
			rcode = atoi(line);
			if ((rcode >= 1) && (rcode <= i)) break;
			printf("\n\n*** Invalid response\n\n\n");
		}
	} else {
		fgets(line, LINE_SIZE, stdin);
		if (ferror(stdin))
			rcode = -1; /* timed out */
		else
			rcode = atoi(line);
	}
	/*
	 * make sure to reset alarm, and set previous alarm handler
	 */
	alarm(0);
	signal(SIGALRM, prevalrmfunc);

	if ((state == 'S') || (state == 's')) {
		rcode += 2097191;
	} else if (rcode > 0) {
		rcode += 2097181;
	} else if ( rcode <= 0 && rcode >= -2 ) {
		rcode += 2097171;
	}
	fprintf(fp, "%d\n", rcode);
	fclose(fp);
	return(rcode);
}

#ifdef SDREASONS_CMD

#include <libgen.h>

main(int argc, char **argv)
{
	if ((argc == 3) && (strcmp(argv[1], "-i") == 0)) {
		return(doavailmon(argv[2][0]));
	} else {
		printf("usage: %s -i <init-state>\n", basename(argv[0]));
		return(-2);
	}
}
#endif /* SDREASONS_CMD */
