/**************************************************************************
 *									  *
 * 		 Copyright (C) 1993, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
/*
 * $Id: sysctlrd.c,v 1.33 1997/11/07 23:19:24 jfd Exp $
 */


#include	<sys/types.h>
#include	<sys/param.h>
#include	<sys/sysinfo.h>
#include	<sys/sysmp.h>
#include	<fcntl.h>
#include	<stdio.h>
#include	<stdlib.h>
#include	<time.h>
#include	<limits.h>
#include	<getopt.h>
#include	<sys/EVEREST/sysctlr.h>
#include	<sys/EVEREST/evconfig.h>
#include	<sys/EVEREST/evdiag.h>
#include	<syslog.h>
#include	<string.h>
#include	<unistd.h>
#include	<signal.h>
#include	<sys/ipc.h>
#include	<sys/shm.h>
#include	<sys/schedctl.h>
#include	<sys/syssgi.h>
#include 	<sys/lock.h>
#include        <sys/time.h>
#include	<sys/capability.h> 

/* #define SYSCTLR_VOLTAGE_WAR
 * SYSCTLR_VOLTAGE_WAR was used to screen out system controllers that needed
 * rework in the early days of Challenge.  Now these parts are out of
 * circulation so the code is unnecessary.  Out of range voltages will be
 * detected and reported by the system controller itself.
 */

#define NULL_SEQ                -1
#define	MAXCPU	                36
#define SMOOTH

#define MAX_WAIT		18000	      /* 5 minutes in seconds*/

#define MAX_RETRIES		5	      /* Max times to retry request */

#define MAX_ENV_INFO		256	      /* Max bytes of environ info. */
#define MAX_LOG_INFO		1024	      /* Max bytes of log info. */

#define LOG_INTERVAL		300	     /* Seconds between log fetches */

#define	EVEREADY_SCALE		16	      /* y pixels on small display */
#define EVEREADY_UPDATE		(clk_tck / 2)
#define TERMINATOR_SCALE	72	      /* y pixels on large display */
#define TERMINATOR_UPDATE	((3 * clk_tck) / 4)

#define	TIMEOUT_GIVEUP		20	       /* How many consec failures
						* before quitting.
						*/

#define ABS(_x)			(((_x) > 0)?(_x):-(_x))

/* Blower max/min macros */
#define BLOWER_A_MAX(x)         (x)->blower_limits[0]
#define BLOWER_A_MIN(x)         (x)->blower_limits[1]
#define BLOWER_B_MAX(y)         (y)->blower_limits[2]
#define BLOWER_B_MIN(y)         (y)->blower_limits[3]

/* Set the below if you want to test the output logging of variances in
 * the blower speeds.
 */
/* #define TESTING */
#ifdef TESTING
#define MAX_BLOWER_SPEED        777 /* good number */
#else
#define MAX_BLOWER_SPEED        3000
#endif
/* If we detect that the blower is out of range (e.g. the speed exceeds 
 * MAX_BLOWER_SPEED above), then we log a message that the blower speed
 * exceeds this maximum. Without ONE_HOUR_TIME defined to be 60 minutes
 * as below, the logging would repeat every time the daemon updates its
 * environment variables (which would give a LOT of output). This define
 * determines how often to log repeat occurences of blower variance events
 * which fall over the range of MAX_BLOWER_SPEED.
 */
#define ONE_HOUR_TIME           60  /* units of minutes */
#define START_TIMER             0
#define CURRENT_TIMER           1


typedef struct cpu_time_s {
	time_t		cpu[6];
} cpu_time_t;

typedef struct display_s {
	cpu_time_t	*lri[MAXCPU];
	cpu_time_t	*nri[MAXCPU];
	cpu_time_t	*mri[MAXCPU];
	cpu_time_t	*cpus[MAXCPU];
	cpu_time_t	*tmpi[MAXCPU];
	int		cpuvalid[MAXCPU];
	int		ncpu;
	int		sc_scale;
	int		sc_update;
	int		sinfosz;
	int		sysctlr;
	key_t		key;
	int		shmid;
	char 		*shmaddr;
	int		consec_wtouts;
	int		consec_rtouts;
	char		raw_env_info[MAX_ENV_INFO];
	uint		cmd_fails;
	uint		log_fails;
	uint		env_fails;
	time_t		start_time;
} display_t;


typedef struct sysctlr_env_s {
	int		valid;
	int		display_type;
	int		volts[6];
	int		temp;
	int		rpms[2];
	int		dtoa;
	time_t		timestamp;
	char		seq;
	int		firmware_rev;
	int		temp_limits[2];
	int		blower_limits[8];
	int		firmware_debug[4];
	int		invalid_commands;
	uint		cmd_fails;
	uint		log_fails;
	uint		env_fails;
	time_t		start_time;
	int             vdc[6][2] ; /* Voltage Extrema */
        struct timeval  udt[2];
} sysctlr_env_t;


char *powersupply_names[] = {
  "\n 48.0V supply = ",
  "\n 12.0V supply = ",
  "\n  5.0V supply = ",
  "\n  1.6V supply = ",
  "\n -5.2V supply = ",
  "\n-12.0V supply = "
};

char *voltages[] = {
  " 48.0V ",   /* volts[0] */
  " 12.0V ",   /* volts[1] */
  "  5.0V ",   /* volts[2] */
  "  1.6V ",   /* volts[3] */
  " -5.2V ",   /* volts[4] */
  "-12.0V "    /* volts[5] */
};

#define MAX_V         0
#define MIN_V         1
#define NUM_VOLTAGES  6


/* Prototypes */
int sendcmd(int, char, char, char);
void display_string(int, char, char *);
int getresult(display_t *, char, char, char *);
void check_inventory(evcfginfo_t *);
int check_ccrev(evcfginfo_t *);
int protected_read(int, char *, int);
void resync(int, int);
void getlog(display_t *sd, int seq);
	
int no_powermeter;
display_t display;
int verbose;
int graceful_powerdown;
volatile int kid_dead;
int cmd_len;
int clk_tck;
int on_initial_setup = 0xff;
int log_vdc_delta = 0;
int blower_a_faults = 0;
int blower_b_faults = 0;

/*
 * Returns the elapsed number of minutes between the two time values.
 */
int 
elapsed_minutes(struct timeval* time_start, struct timeval* time_stop)
{
 double start = ((double)time_start->tv_sec) * 1000000.0 +time_start->tv_usec;
 double stop = ((double) time_stop->tv_sec) * 1000000.0 + time_stop->tv_usec;
 double seconds = (stop - start) / 1000000.0;
#ifdef DEBUG
 return (int) seconds;   /* Don't want to wait around for this */
#else
 return (int)(seconds / 60.0); 
#endif
}

/*
 * Updates a timeval.
 */
void 
update_timer(struct timeval* tv)
{
  if (gettimeofday(tv, (struct timezone *) 0) < 0)
    perror("gettimeofday");
}


/*
 * Returns true if a message should be logged. This is determined
 * by whether or not an hour's worth of time has elapsed or whether
 * or not this is the first time this is called.
 * 
 * The following are invariants:
 * 1) The first call to should_log_message() will ALWAYS return true.
 * 2) Subsequent calls to this routine will return true iff. an hour
 *    has passed.
 * 
 * IMPORTANT NOTE: You must call update_timer() on your start value.
 */
int 
should_log_message(struct timeval* start, struct timeval* finish)
{
  int x = 0;
  static int logged_once = 0;
  update_timer(finish);
  x = elapsed_minutes(start, finish);
  if(!logged_once++){
    return ONE_HOUR_TIME;
  }
  else{
    if(x >= ONE_HOUR_TIME){
      update_timer(start); /* Reset start time */
      return x;
    } 
  }
  return 0;
}



void 
initinfo(display_t *sd, sysctlr_env_t *ei)
{
	register int i;

	sd->ncpu = sysmp(MP_NPROCS);

	if ((sd->sinfosz = sysmp(MP_SASZ, MPSA_SINFO)) < 0) {
		fprintf(stderr, "sysinfo scall interface not supported\n");
		exit(1);
	}

	sd->sinfosz = sizeof(cpu_time_t);

	/* NOTE: This is to reduce the time spent copying system info.
	 * If the interface ever changes (would break binary compatibility)
	 * this code won't work.  We're assuming the cpu[6] array is at the
	 * beginning of the sysinfo structure.
	 */

	for (i = 0; i < sd->ncpu; i++) {
		sd->lri[i] = (cpu_time_t *) calloc(1, sd->sinfosz);
		sd->nri[i] = (cpu_time_t *) calloc(1, sd->sinfosz);
		sd->mri[i] = (cpu_time_t *) calloc(1, sd->sinfosz);
		sd->cpus[i] = (cpu_time_t *) calloc(1, sd->sinfosz);
		sysmp(MP_SAGET1, MPSA_SINFO, (char *) sd->nri[i],
						sd->sinfosz, i);
		sysmp(MP_SAGET1, MPSA_SINFO, (char *) sd->mri[i],
						sd->sinfosz, i);
	}

	/* initialize the environment structure */
	for (i = 0; i < NUM_VOLTAGES; i++)
		ei->volts[i] = 0;

	/* Initialize the voltage(s) max/min to small/high values */
	for(i = 0; i < NUM_VOLTAGES; i++){
	  if(i < 4){ 
	    ei->vdc[i][MAX_V] = -10000; /* Max */
	    ei->vdc[i][MIN_V] =  10000; /* Min */
	  } else {
	    /* These are negative voltage supplies ( -5.2, -12) */
	    ei->vdc[i][MAX_V] = -10000; /* Max */
	    ei->vdc[i][MIN_V] =  10000; /* Min */
	  }
	}

	ei->temp = 0;
	ei->rpms[0] = ei->rpms[1] = 0;
	ei->dtoa = 0;
	ei->valid = 0;
	ei->display_type = '?';
	ei->seq = '0';

	/* No consecutive timeouts */
	sd->consec_rtouts = 0;
	sd->consec_wtouts = 0;

	/* No fetch failures */
	sd->cmd_fails = 0;
	sd->log_fails = 0;
	sd->env_fails = 0;

	/* Get the current time */
	sd->start_time = time((time_t *)NULL);
	
	/* Clear the environment info. */
	sd->raw_env_info[0] = '\0';


	/* Init BLOWER_{A,B} max/min to small/large seed values */
#ifdef COMMENT /* @@ Not Needed since we get these every spin   */
	       /* @@ Verify this with Morrow.                   */
 
	BLOWER_A_MAX(ei) = -40000;
	BLOWER_A_MIN(ei) =  40000;
	BLOWER_B_MAX(ei) = -40000;
	BLOWER_B_MIN(ei) =  40000;
#endif
	update_timer(&ei->udt[START_TIMER]);
}


void openport(display_t *sd)
{
	int fd;

	if ((sd->sysctlr = open("/dev/sysctlr", O_RDWR)) < 0) {
		syslog(LOG_ERR, "Couldn't open /dev/sysctlr.  Exiting\n");
		exit(1);
	}

	/* Resync but don't flush */
	resync(sd->sysctlr, 0);

	/* Make our working directory / */
	chdir("/");

	/* Move our stderr to /dev/console. */
	fd = open("/dev/console", O_WRONLY);
	if (fd < 0) {
		fprintf(stderr, 
			"sysctlrd: Cannot open console for writing!\n");
		syslog(LOG_ERR, "Cannot open console for writing!\n");
		return;
	}

	/* Close stderr */
	close(2);

	/* Copy the /dev/console file descriptor to stderr */
	dup(fd);

	/* Close the original descriptor */
	close(fd);
}


void flush_buffer(int fd) {

	int i;
	char buffer[512];

	if (!ioctl(fd, SYSC_QLEN))
		return;

	for (i = 0; (!protected_read(fd, &buffer[i], 1)
		     && (buffer[i] != '\0')); i++)
		;

	buffer[i] = '\0';

	if (verbose) {
		syslog(LOG_DEBUG, "%d garbage chars in read queue!\n", i);
		syslog(LOG_DEBUG, "Garbage == %s\n", buffer);
	}
}


void resync(int fd, int flush)
{
	char dummy;

	dummy = 0x1a;	/* Ctrl-Z, the system controller resync character */

	write(fd, &dummy, 1);
	sginap(clk_tck/10);

	if (flush) {
		/* This also means it's not the initial resync */
		if (verbose)
			syslog(LOG_DEBUG, "Resyncing the system controller.\n");
	
		flush_buffer(fd);
	}
}


int protected_read(int fd, char *buf, int len) {
	int i;
	int retval;

	if ((retval = read(fd, buf, len)) < len) {
		buf[retval] = '\0';
		if (verbose) {
			syslog(LOG_DEBUG,
				"read of %d returned %d (%s)\n", len, retval,
				buf);
			if (retval < len)
				for (i = 0; i < retval; i++)
					printf("byte %d == 0x%x\n", i, buf[i]);
		}
		return 1;
	}
	return 0;
}


int protected_write(int fd, char *buf, unsigned len) {

	int retval;

	if ((retval = write(fd, buf, len)) < len) {
		if (no_powermeter == 0)
			(display.consec_wtouts)++;	
		if (verbose)
			syslog(LOG_DEBUG,
				"System controller write timed out.\n");
		return 1;
	}
	display.consec_wtouts = 0;
	return 0;
}


void display_string(int fd, char cmd, char *string)
{
	char buf[40];
	int index;
	char lenstr[5];

	buf[0] = SC_ESCAPE;
	buf[1] = SC_SET;
	buf[2] = cmd;

	if (cmd_len)
		index = 5;
	else
		index = 3;

	strcpy(buf + index, string);
	index += strlen(buf + index);
	buf[index++] = SC_TERM;

	if (cmd_len) {
		sprintf(lenstr, "%02x", index - 2);
		buf[3] = lenstr[0];
		buf[4] = lenstr[1];
	}

	/* Display the message */
	protected_write(fd, buf, index);

	buf[index++] = '\0';

	sginap(clk_tck / 10);
}


void power_down(display_t *sd)
{
	/* Power off the system */
	fprintf(stderr, "sysctlrd: Powering down the system.\n");
	fflush(stderr);
	sginap(clk_tck);
	sendcmd(sd->sysctlr, SC_SET, SC_OFF, 0);
	sginap(clk_tck * 60);
	/* not reached */
	fprintf(stderr, 
		"sysctlrd: WARNING: The system has not powered down!\n");
	exit(1);
}


wait_and_snooze(int fd)
{
	char spinner[] = "-/|/";
	time_t start_time;
	int which_char = 0;
	char string[2];

	display_string(fd, SC_MESSAGE, "Syncing Disks...");

	start_time = time((time_t *)NULL);
	string[1] = '\0';

	while (!kid_dead && ((time((time_t *)NULL) - start_time) < MAX_WAIT)){
	  sendcmd(fd, SC_SET, SC_SNOOZE, 0);	/* Get more time */
	  which_char = (which_char + 1) % 4;
	  string[0] = spinner[which_char];
	  display_string(fd, SC_PROCSTAT, string);
	  
	  /* Nap for a 1/2 second */
	  sginap(clk_tck/2);
	}
	if (kid_dead) {
	  display_string(fd, SC_MESSAGE, "Powering down...");
	} else {
	  fprintf(stderr, "sysctlrd: Sync timed out!\n");
	  display_string(fd, SC_MESSAGE, "SYNC TIMED OUT!");
	}
	/* Wait for two seconds. */
	sginap(clk_tck * 2);

	fprintf(stderr, "sysctlrd: Powering down.\n");
	fflush(stderr);

	/* Power off the system */
	sendcmd(fd, SC_SET, SC_OFF, 0);

	while(1)
	   ;/* not reached */	
}


void kid_died()
{
	kid_dead = 1;
	return;
}

void shut_down_power(display_t *sd)
{
	pid_t child;
	int i;

	for (i = 0; i < 10; i++)
	  sendcmd(sd->sysctlr, SC_SET, SC_SNOOZE, 0); /* Get more time */

	/* Delete shared memory */
	if (display.shmid >= 0)
		shmctl(display.shmid, IPC_RMID);

	if (sd->raw_env_info[0])
	  syslog(LOG_DEBUG, "Last info: %s\n", sd->raw_env_info);

	for (i = 0; i < 10; i++)
	  sendcmd(sd->sysctlr, SC_SET, SC_SNOOZE, 0); /* Get more time */

	if (!getresult(sd, SC_ENV, SC_ENV, sd->raw_env_info))
	  syslog(LOG_DEBUG, "Current info: %s\n", sd->raw_env_info);
	else
	  syslog(LOG_DEBUG, "Current info: Unavailable.\n");

	if (!graceful_powerdown) {
		power_down(sd);
	}
			
#if PLOCK_GETS_FIXED
	/* Lock out text and data into memory */
	plock(PROCLOCK);
#endif

	/* Ignore the hang-up signal we're about to send. */
	signal(SIGHUP, SIG_IGN);

	kid_dead = 0;

	/* Catch child signal */
	signal(SIGCLD, kid_died);

	fprintf(stderr, "sysctlrd: Sending hang-up signal to processes.\n");
	system("/etc/killall HUP");
	display_string(sd->sysctlr, SC_MESSAGE, "Sending hang-up...");

	/* Get lots more time */
	for (i = 0; i < 5; i++)
		sendcmd(sd->sysctlr, SC_SET, SC_SNOOZE, 0); 

	/* Give processes a brief chance to clean up */
	fprintf(stderr, "sysctlrd: Waiting for processes to clean-up\n");
	sginap(clk_tck * 2);

	sendcmd(sd->sysctlr, SC_SET, SC_SNOOZE, 0);	/* Get more time */
	fprintf(stderr, "sysctlrd: Killing all processes.\n");
	display_string(sd->sysctlr, SC_MESSAGE, "Killing proceses...");
	sendcmd(sd->sysctlr, SC_SET, SC_SNOOZE, 0);	/* Get more time */
	system("/etc/killall");

	if (child = fork()) {
		wait_and_snooze(sd->sysctlr);
	} else {
		fprintf(stderr, "sysctlrd: Doing synchronous sync...\n");
		syssgi(SGI_SSYNC);

		fprintf(stderr, "sysctlrd: Done.\n");
		fflush(stdout);
		exit(0);
	}
}


int doalarm(display_t *sd, char *preamble)
{
  int retval = 0;

	if (preamble[0] == 'A') {
		switch(preamble[1]) {
			case 'T':
				sendcmd(sd->sysctlr, SC_SET, SC_SNOOZE, 0);
				syslog(LOG_EMERG, "Overtemp alarm!\n");
				fprintf(stderr,
					"\007\nsysctlrd: Overtemp alarm!\n");
				shut_down_power(sd);
				break;
			case 'O':
				sendcmd(sd->sysctlr, SC_SET, SC_SNOOZE, 0);
				syslog(LOG_EMERG, "Keyswitch off!\n");
				fprintf(stderr,
					"\007\nsysctlrd: Keyswitch off!\n");
				shut_down_power(sd);
				break;
			case 'B':
				syslog(LOG_EMERG, "Blower failure!\n");
				fprintf(stderr,
					"\007\nsysctlrd: Blower failure!\n");
				fflush(stderr);
				shut_down_power(sd);
				break;

			default:
			  if(verbose){
				syslog(LOG_ALERT, "Unknown alarm: 0x%x\n",
							preamble[1]);
				fprintf(stderr,
					"\007\nsysctlrd: Unknown alarm!\n");
			  }
				retval = -1;
				break;


		}
	} else if (preamble[0] == 'W') {
		switch(preamble[1]) {
			case 'T':
				syslog(LOG_ERR, "COP timer reset error!\n");
				break;
			case 'C':
				syslog(LOG_ERR,
				    "System controller crystal oscillator failed!\n");
				break;
			case 'I':
				syslog(LOG_ERR,
				    "Firmware detected illegal opcode!\n");
				break;
			case 'V':
				syslog(LOG_ALERT,
				    "Voltage out of tolerance!\n");
				break;
			case 'B':
				syslog(LOG_ALERT,
				    "Firmware compensating for blower RPM problem.\n");
				break;
			case 'S':
				syslog(LOG_ERR,
				    "System controller firmware reset.\n");
				break;
			default:
			  if(verbose){
				syslog(LOG_ERR, "Unknown Warning: 0x%x\n",
							preamble[1]);
			  }
				retval = -1;
				break;
	}
	} else {
		syslog(LOG_ERR, "Bogus alarm or error: 0x%x 0x%x\n",
						preamble[0], preamble[1]);
	}
  return retval;
}



void checkmsgs(display_t *sd)
{
	int qlen;
	char preamble[2000];
	int i;
	int alarm_length;

	alarm_length = (cmd_len ? 5: 3);
	while ((qlen = ioctl(sd->sysctlr, SYSC_QLEN)) > 0) {

		if (qlen == -1) {
		  syslog(LOG_ERR, "qlen ioctl not supported.  Exiting\n");
		  exit(1);
		}

		for (i= 0; i < alarm_length; i++) {
		  if (protected_read(sd->sysctlr, (&preamble[i]), 1)) {
		    syslog(LOG_DEBUG, "Message read timed out.\n");
		    return;
		  }

		  if (preamble[i] == '\0') {
		    i++;
		    break;
		  }
		}

		if ((i < alarm_length) && (preamble[i] == '\0') && verbose)
			syslog(LOG_DEBUG, "Short message in queue.\n");

		if ((preamble[alarm_length - 1] == '\0')
					&& ((preamble[0] == SC_ALARM) ||
					    (preamble[0] == SC_WARNING))) {
		  if(doalarm(sd, preamble) < 0){
		    /* Unknown Alarm: Log Event */
		    getlog(sd, NULL_SEQ);
		  }
		} else {
			flush_buffer(sd->sysctlr);
		}
	}



}


void getcpuinfo(display_t *sd)
{
	register int 	i, j, k;

	/*
	 * If we do a per-cpu readout, then we might encounter an
	 * empty slot for CPUs.  Compress this into the given number.
	 */
	for (i = 0, j = 0; j < MAXCPU && i < sd->ncpu; j++) {
		sd->tmpi[j] = sd->lri[j];
		sd->lri[j] = sd->nri[j];
		sd->nri[j] = sd->mri[j];
		sd->mri[j] = sd->tmpi[j];

		if (sysmp(MP_SAGET1, MPSA_SINFO, (char *) sd->mri[j],
						sd->sinfosz, j) == -1) {
			sd->cpuvalid[j] = 0;
		} else {
			sd->cpuvalid[j] = 1;
			i++;
		}

		if (!sd->cpuvalid[j])
			continue;

		for (k = 0; k < 6; k++) {
#ifdef SMOOTH
			sd->cpus[j]->cpu[k] = sd->mri[j]->cpu[k] -
					sd->lri[j]->cpu[k];
#else
			sd->cpus[j]->cpu[k] = sd->mri[j]->cpu[k] -
					sd->nri[j]->cpu[k];
#endif
		}
	}

}


int sendcmd(int fd, char cmd, char parm, char seq)
{
	char outputbuf[10];
	int index = 2;

	outputbuf[0] = SC_ESCAPE;
	outputbuf[1] = cmd;

	/* On "get" commands, we need a "sequence number."
	 * For now, just use the subcommand number.
	 */
	if (cmd == SC_GET)
		outputbuf[index++] = seq;
	outputbuf[index++] = parm;

	if (cmd_len) {
		outputbuf[index++] = '0';
		outputbuf[index] = '0' + index;
		index++;
	}
	outputbuf[index++] = SC_TERM;
	outputbuf[index] = '\0';

	if (protected_write(fd, outputbuf, index))
		if (verbose)
			syslog(LOG_ERR, "Command write timed out!\n");
		else
			return 1;
		
	sginap(clk_tck / 10);
	return 0;
}


int getresult(display_t *sd, char cmd, char seq, char *buffer)
{
	char preamble[6];
	char lenstr[3];
	unsigned int length;
	unsigned int actual;
	int i, j;
	int timeout = 0;
	int retries = 0;
	int done = 0;

	preamble[0] = preamble[1] = preamble[2] = preamble[3] = '\0';

	while ((retries < MAX_RETRIES) && (!done)) {
		if (retries > 0) {
			resync(sd->sysctlr, 1);
			sd->cmd_fails++;
		}
		if (sendcmd(sd->sysctlr, SC_GET, cmd, seq)) {
			preamble[0] = SC_TERM;
			/* Terminate partial command. */
			write(sd->sysctlr, preamble, 1);
			retries++;
			continue;
		}
			
		j = 0;
		do {
			if (protected_read(sd->sysctlr, (&preamble[j++]), 1)) {
				/* Resync the port. */
				resync(sd->sysctlr, 1);
				sendcmd(sd->sysctlr, SC_GET, cmd, seq);
				break;
			}
		} while ((j < 3) && (preamble[j - 1] != '\0'));

		if (j != 3) {
			retries++;
			if (verbose)
				if (j < 3) {
					syslog(LOG_DEBUG,
					"Result preamble read timed out.\n");
				} else {
					syslog(LOG_DEBUG,
					"Short response string.\n");
				}
			continue;
		}
		if((preamble[0] == SC_ALARM) || (preamble[0] == SC_WARNING)) {
			/* If length checking is on, we need to fetch the
			 * length bytes here too.
			 */
			if (cmd_len)
				if (protected_read(sd->sysctlr, preamble + 3,
						   2)) {
					retries++;
					continue;
				} else {
					preamble[5] = '\0';
				}
			if(doalarm(sd, preamble) < 0){
			  /* Unknown Alarm: Log event */
			  getlog(sd, seq);
			  getlog(sd,NULL_SEQ);
			}
			continue;
		}

		/* Make sure this is a response */
		if(preamble[0] != SC_RESP) {
			if (verbose)
				syslog(LOG_DEBUG,
					"Unexpected response code: 0x%x\n",
								preamble[0]);
			retries++;
			continue;
		}

		/* Check the sequence number */
		if (preamble[1] != seq) {
			if (verbose)
				syslog(LOG_DEBUG,
					"Bogus sequence number: 0x%x\n",
							preamble[1]);
			retries++;
			continue;
		}

		/* Check the request code */
		if (preamble[2] != cmd) {
			if (verbose)
				syslog(LOG_DEBUG, "Wrong request code: 0x%x\n",
							preamble[2]);
			retries++;
			continue;
		}

		if ((cmd_len == 1) && protected_read(sd->sysctlr, lenstr, 2)) {
			if (verbose)
				syslog(LOG_DEBUG,
					"Result length read timed out!\n");
			retries++;
			continue;
		}

		lenstr[2] = '\0';

		i = 0;
		do {
			timeout = protected_read(sd->sysctlr, buffer + i, 1);
		} while ((buffer[i++] != '\0') && !timeout);

		if (timeout) {
			if (verbose)
				syslog(LOG_DEBUG,
					"Result data read timed out!\n");
			buffer[i] = '\0';
			retries++;
			continue;
		}

		if (cmd_len == 1) {
			sscanf(lenstr, "%2x", &length);
			actual = (i + 4);

#ifdef FIRMWARE_200
			while (actual >= 0x100)
				actual >>= 4;
#else
			actual &= 0xff;
#endif

			if (length != actual) {
				if(verbose)
					syslog(LOG_DEBUG,
						"Result length mismatch!\n");
				retries++;
				continue;
			}
		}
		done = 1;

	}

	/* We timed out or got bogus responses too many times. */
	if (done)
		return 0;
	else
		return 1;
}


int what_length_mode(char *buffer)
{
	if (!strcmp(buffer, "N")) {
		return 0;
	} else if (!strcmp(buffer, "06Y")) {
		return 1;
	} else {
		return -1;
	}
}


void set_length_mode(display_t *sd)
{
	char buffer[40];
	int i;

	/* For the get check command, we want the length numbers
	 * turned on so the commands won't be ignored.
	 * We set it to the correct value later.
	 * Responses are only length-checked if cmd_len == 1.  This setting
	 * causes lengths to be sent but not checked.  Sure it's ugly.
	 * We have to be compatible in four different combinations.
	 */

	cmd_len = 2;

	/* Try a few times in case we lose characters or something. */
	for (i = 0; i < 3; i++) {
		/* If this command times out, the feature isn't available. */
		if (getresult(sd, SC_CHECK, SC_CHECK, buffer)) {
			cmd_len = 0;
			return;
		}

		if (what_length_mode(buffer) == 1) {
			cmd_len = 1;
			return;
		} else if (what_length_mode(buffer) == 0) {
			/* Toggle the length check mode. */
			sendcmd(sd->sysctlr, SC_SET, SC_CHECK, SC_CHECK);

			/* Deleting the next 10 lines has no effect on the
			 * function of this code (well, one less retry),
			 * but I thought it would be confusing so I didn't
			 * go for it.
			 */

			/* Check the result */
			getresult(sd, SC_CHECK, SC_CHECK, buffer);

			/* If the "set" took, set cmd_len and return */
			if (what_length_mode(buffer) == 1) {
				cmd_len = 1;
				return;
			}
			/* Else fall through and try again */
		}

		/* Falling through means a response was bogus. */

	} /* for i */

	cmd_len = 0;

}


void settime(display_t *sd)
{
        time_t now;
        char textbuf[40];
	int index;
	char lenstr[3];

        time(&now);

	textbuf[0] = SC_ESCAPE;
	textbuf[1] = SC_SET;
	textbuf[2] = SC_TIME;
	
	if (cmd_len) {
		index = 5;
	} else {
		index = 3;
	}

        cftime(textbuf + index, "%m%d%y%H%M%S", &now);
	index += strlen(textbuf + index);
	textbuf[index++] = SC_TERM;
	textbuf[index] = '\0';

	if (cmd_len) {
		sprintf(lenstr, "%02x", index - 2);
		textbuf[3] = lenstr[0];
		textbuf[4] = lenstr[1];
	}

	protected_write(sd->sysctlr, textbuf, index);
}


void dumpinfo(display_t *sd, sysctlr_env_t *ei)
{
	unsigned int	idle;
	unsigned int	used;
	unsigned int	i, j;
	unsigned int	index;
	char		number[5];
	unsigned int	fraction;
	char		outputbuf[MAXCPU * 5 + 5];

	outputbuf[0] = SC_ESCAPE;
	outputbuf[1] = SC_SET;
	outputbuf[2] = SC_HISTO;

	if (cmd_len)
		index = 5;
	else
		index = 3;

	for (i = 0; i < sd->ncpu; i++) {
		if (sd->cpuvalid[i]) {
			idle = sd->cpus[i]->cpu[CPU_IDLE] + 
				sd->cpus[i]->cpu[CPU_WAIT];
			used = (sd->cpus[i]->cpu[CPU_USER] +
				sd->cpus[i]->cpu[CPU_KERNEL] +
				sd->cpus[i]->cpu[CPU_SXBRK] +
				sd->cpus[i]->cpu[CPU_INTR]);
			if (idle + used == 0)
				idle += 1;
			fraction = MAX(sd->sc_scale * used / (idle + used), 1);
		} else {
			fraction = 0;
		}

		if (i)
			sprintf(number, ",%d", fraction);
		else
			sprintf(number, "%d", fraction);

		for (j = 0; j < strlen(number); j++)
			outputbuf[index++] = number[j];

	}

	if ((ei->display_type == 'E') && (!cmd_len))
		for (i = sd->ncpu; i < 16; i++) {
			outputbuf[index++] = ',';
			outputbuf[index++] = '0';
		}

	outputbuf[index++] = SC_TERM;

	/* Now that we know what the length should be, set it! */
	if (cmd_len) {
		sprintf(number, "%02x", index - 2);
		outputbuf[3] = number[0];
		outputbuf[4] = number[1];
	}

	/* Dump to sysctlr here */
	protected_write(sd->sysctlr, outputbuf, index);

	outputbuf[index++] = '\0';

	sginap(clk_tck / 10);
}


void print_dec(int number)
{
	printf("%3d.%02d", number/100, ABS(number) % 100);
}


void printenvinfo(sysctlr_env_t *ei)
{
	int i;

	printf("As of %s", ctime(&ei->timestamp));
	printf("Display type:    ");
	switch(ei->display_type) {
		case 'T':
			printf("40 x 8\n");
			break;
		case 'E':
			printf("20 x 2\n");

			break;
		default:
			printf("Unknown\n");
			break;
	}
	/* Print voltages */
	for(i = 0; i < 6; i++){
	  printf("%s", powersupply_names[i]);
	  print_dec(ei->volts[i]);
	}

	printf("\nSystem air temp =");
	print_dec(ei->temp);
	printf(" C\n");
	printf("Blower A speed =  %4d RPM\n", ei->rpms[0]);
	if (ei->display_type == 'T')
		printf("Blower B speed =  %4d RPM\n", ei->rpms[1]);
	if (verbose)
		printf("D/A setting = %d\n", ei->dtoa);
	if (ei->valid >= 2) {
		printf("Firmware version");
		print_dec(ei->firmware_rev);

		printf("\n-- Extrema since power-up --");
		printf("\nMax temp = ");
		print_dec(ei->temp_limits[0]);
		printf("\nMin temp = ");
		print_dec(ei->temp_limits[1]);

		printf("\nMax blower A speed = %d", ei->blower_limits[0]);
		printf("\nMin blower A speed = %d", ei->blower_limits[1]);

		if (ei->display_type == 'T') {	
			printf("\nMax blower B speed = %d",
							ei->blower_limits[2]);
			printf("\nMin blower B speed = %d",
							ei->blower_limits[3]);
		}
		/* Print Voltage VDC Extrema */
		for(i = 0; i < NUM_VOLTAGES; i++){
		  printf("\n%ssupply max/min (", voltages[i]);
		  print_dec(ei->vdc[i][MAX_V]);
		  printf(",");
		  print_dec(ei->vdc[i][MIN_V]);
		  printf(")");
		}
			
		if (verbose) {
			printf("\n-- Firmware internal variables --\n");
			for (i = 0; i < 4; i++)
			  printf("B%d: %d ", i, ei->blower_limits[4 + i]);
			printf("\n");
			for (i = 0; i < 4; i++)
				printf("F%d: %d ", i, ei->firmware_debug[i]);
			printf("\nInvalid CPU commands = %d",
							ei->invalid_commands);
		}
			
	}
	if (verbose && (ei->timestamp - ei->start_time)) {
		printf("\nGet env info failures: %u (%u / hour)\n",
			ei->env_fails,
			(ei->env_fails * 3600U) /
			(ei->timestamp - ei->start_time));
		printf("Get log info failures: %u (%u / hour)\n",
			ei->log_fails,
			(ei->log_fails * 3600U) /
			(ei->timestamp - ei->start_time));
		printf("Command retries: %u (%u / hour)",
			ei->cmd_fails,
			(ei->cmd_fails * 3600U) /
			(ei->timestamp - ei->start_time));
	}
	printf("\n");
}


void printrawinfo(sysctlr_env_t *ei)
{
	int i;

	printf("%d;%c;", ei->firmware_rev, ei->display_type);
        for (i = 0; i < 6; i++)
                printf("%d;", ei->volts[i]);
        printf("%d;%d;%d;%d;", ei->temp, ei->rpms[0], ei->rpms[1], ei->dtoa);
	printf("%d;%d;%d;", ei->timestamp, ei->temp_limits[0],
						ei->temp_limits[1]);
	for (i = 0; i < 4; i++) 
		printf("%d;", ei->blower_limits[i]);
	printf("%d\n", ei->invalid_commands);
}


int decimal_to_fixed(char *number)
{
	int cent, decimal;
	int result;

	cent = atoi(number);

	if (strchr(number, '.')) {
		number = strchr(number, '.') + 1;
		if (strlen(number) > 1)
			decimal = atoi(number);
		else
			decimal = atoi(number) * 10;
	} else
		decimal = 0;
	
	if (cent >= 0)
		result = 100 * cent + decimal;
	else
		result = 100 * cent - decimal;

	return result;
}

int check_sysctlr_rev(sysctlr_env_t *ei)
{
	int complained = 0;

	if (!ei->valid)
		return 0;

	if (ei->firmware_rev < 202) {
		syslog(LOG_ERR, "System controller firmware is downrev -\n");
		syslog(LOG_ERR, "  Rev is %d.%02d - should be at least 2.02\n",
				ei->firmware_rev / 100, ei->firmware_rev % 100);
		complained++;
	}

#ifdef SYSCTLR_VOLTAGE_WAR

	if (ei->volts[3] < 160) {
		syslog(LOG_ERR, "System controller likely needs rework -\n");
		syslog(LOG_ERR, "  Voltage is %d.%02d - should be at least 1.60\n", ei->volts[3] / 100, ei->volts[3] % 100);
		complained++;
	}

#endif /* SYSCTLR_VOLTAGE_WAR */

	return complained;
}


int getenvinfo(display_t *sd, sysctlr_env_t *ei)
{
        char buffp[1024];
	char *next_field;
	int index,j;
	char env_info_copy[MAX_ENV_INFO];

	if (getresult(sd, SC_ENV, ei->seq, sd->raw_env_info)) {
		if (verbose)
			syslog(LOG_DEBUG, "Get env info failed.\n");
		(sd->env_fails)++;
		(sd->consec_rtouts)++;
		ei->valid = 0;
		ei->seq++;
		if (ei->seq > '9')
			ei->seq = '0';
		return -1;
	}

	sd->consec_rtouts = 0;

	/* Increment the sequence number */
	ei->seq++;
	if (ei->seq > '9')
		ei->seq = '0';
/*
	printf("Env string == %s\n", sd->raw_env_info);
*/

	strcpy(env_info_copy, sd->raw_env_info);
	index = 0;
	next_field = strtok(env_info_copy, ";");

	/* Mark this structure invalid. */
	ei->valid = 0;

	ei->firmware_rev = 0;
	while (index < 26 && next_field) {
		if (index < 6) {
			/* It's a voltage.  Do fixed point convert */
			ei->volts[index] = decimal_to_fixed(next_field);
		} else if (index < 7) {
			/* It's a temperature */
			ei->temp = decimal_to_fixed(next_field);
		} else if (index < 9) {
			ei->rpms[index - 7] = atoi(next_field);
		} else if (index == 9) {
			ei->dtoa = atoi(next_field);
		} else if (index == 10) {
			ei->firmware_rev = decimal_to_fixed(next_field);
		} else if (index < 13) {
			ei->temp_limits[index - 11] =
						decimal_to_fixed(next_field);
		} else if (index < 21) {
			ei->blower_limits[index - 13] = atoi(next_field);
		} else if (index < 25) {
			ei->firmware_debug[index - 21] = atoi(next_field);
		} else {
			ei->invalid_commands = atoi(next_field);
		}
		index++;
		next_field = strtok(NULL, ";");
	}

	if (index == 10) {
		ei->valid = 1;
	} else if (index == 26) {
		ei->valid = 2;
	} else {
		if (verbose) {
			syslog(LOG_DEBUG,
		"Received short environmental response string (%d fields).\n",
			index);
		}
		ei->valid = 0;
	}

	ei->timestamp = time((time_t *)NULL);

	/* XXX - eventually move these into ei struct.  More risk, though
	 * so don't do it now
	 */
	ei->log_fails = sd->log_fails;
	ei->env_fails = sd->env_fails;
	ei->cmd_fails = sd->cmd_fails;
	ei->start_time = sd->start_time;

	/* Update VDC Input extrema */
	for( j = 0; j < NUM_VOLTAGES; j++){

	  if( ei->volts[j] > ei->vdc[j][MAX_V]){

	    ei->vdc[j][MAX_V] = ei->volts[j] ; /* Max */
	    if(!on_initial_setup){
	      if(log_vdc_delta){
		sprintf(buffp,"%ssupply saw new maximum: %3d.%02dV\n", 
			voltages[j],
			ei->vdc[j][MAX_V] / 100, 
			ABS(ei->vdc[j][MAX_V]) % 100 );		
		syslog(LOG_DEBUG,buffp);
	      }
	    }
	  } 
	  if (ei->volts[j] < ei->vdc[j][MIN_V]){
	    ei->vdc[j][MIN_V] =  ei->volts[j] ; /* Min */
	    if(!on_initial_setup){
	      if(log_vdc_delta){
		sprintf(buffp,"%ssupply saw new minimum: %3d.%02dV\n", 
			voltages[j], 
			ei->vdc[j][MIN_V] / 100,
			ABS(ei->vdc[j][MIN_V]) % 100);
		syslog(LOG_DEBUG,buffp);
	      }
	    }
	  }
	}

	/* Notes from Brad Morrow: (morrow@engr.sgi.com) 
	 *
	 * The system controller will send a warning message when it
	 * measures a RPM higher then the upper tolerance.  Normally
	 * this message is sent immediately after the system controller
	 * powers up.  This is only for the system that have the 2700
	 * PRM blower installed.  The problem we want to detect is when
	 * the blower sends out bad RPM data.  The measurement data
	 * shows RPM values > 28000 RPM.  It seems the tach pulse from
	 * the blower has noise on it, cause these bad high readings.
	 * The blower Brad Morrow checked was actually spinning near
	 * 1400 RPM when it was reporting a RPM value of 27K!  We want
	 * to catch this condition, and have an action item for AOL to
	 * do so.  We should never see a RPM value > 3000.  So, if we
	 * see this condition on either blower, we send a message to
	 * the syslog as well as log it to the console.  
	 *
	 * NOTE: this output is sent to both the console and the 
	 * SYSLOG only under the following conditions:
	 * 1) It is the first time the event has occurred.
	 * 2) One hour's time has elapsed since the last occurence of 
	 *    this event.
	 */
	if(BLOWER_A_MAX(ei) > MAX_BLOWER_SPEED){
	  if(should_log_message(&ei->udt[START_TIMER], 
				&ei->udt[CURRENT_TIMER])){
	    sprintf(buffp, 
		    "Blower A maximum (%d) > %d RPM. Notification[%d].\n",
		    BLOWER_A_MAX(ei), MAX_BLOWER_SPEED, ++blower_a_faults);
	    fprintf(stderr, "sysctlrd: %s", buffp);
	    syslog(LOG_ALERT, buffp);
	  }
	}
	if(BLOWER_B_MAX(ei) > MAX_BLOWER_SPEED){
	  if(should_log_message(&ei->udt[START_TIMER],
				&ei->udt[CURRENT_TIMER])){
	    sprintf(buffp,
		    "Blower B maximum (%d) > %d RPM. Notification[%d]\n",
		    BLOWER_B_MAX(ei), MAX_BLOWER_SPEED, ++blower_b_faults);
	    fprintf(stderr, "sysctlrd: %s", buffp);
	    syslog(LOG_ALERT, buffp);
	  }
	}

	/* Every time we update our environment information, get the 
	 * event log. This will only print out if there is something
	 * to see.  */
	getlog(sd,ei->seq);
	
#ifdef COMMENT
	printenvinfo(ei);
#endif
	return 0;
}


int getscale(display_t *sd, char panel, sysctlr_env_t *ei)
{
	char buffer[80];
	int retries = 0;
	int result = 0;

	if (panel == 'E') {
		sd->sc_scale = EVEREADY_SCALE;
		sd->sc_update = EVEREADY_UPDATE;
		ei->display_type = 'E';
		return 0;
	} else if (panel == 'T') {
		ei->display_type = 'T';
		sd->sc_scale = TERMINATOR_SCALE;
		sd->sc_update = TERMINATOR_UPDATE;
		return 0;
	}

	while ((result = getresult(sd, SC_PANEL, SC_PANEL, buffer))
							&& (retries < 3)) {
		retries++;
	}

	if (result) {
		syslog(LOG_ERR, "Get scale failed!  Assuming small panel\n");
		sd->sc_scale = EVEREADY_SCALE;
		sd->sc_update = TERMINATOR_UPDATE;
		return -1;
	}

	if (buffer[0] == 'E') {
		ei->display_type = 'E';
		sd->sc_scale = EVEREADY_SCALE;
		sd->sc_update = EVEREADY_UPDATE;
	} else if (buffer[0] == 'T') {
		ei->display_type = 'T';
		sd->sc_scale = TERMINATOR_SCALE;
		sd->sc_update = TERMINATOR_UPDATE;
	} else {
		ei->display_type = '?';
		syslog(LOG_ERR, "Unknown display type: 0x%x\n", buffer[0]);
		sd->sc_scale = EVEREADY_SCALE;
		sd->sc_update = TERMINATOR_UPDATE;
		return -1;
	}
	return 0;
}


void decode_entry(char *string)
{
	int i;
	char *time_stamp;
	char time_text[20];

	i = 0;
	while((string[i] != ',') && (string[i] != '\0'))
		i++;

	if (string[i] == '\0') {
		/* If the format's messed up, use null timestamp. */
		time_stamp = string + i;
	} else {
		/* End the text, start the timestamp */
		string[i] = '\0';
		time_stamp = string + i + 1;
	}

	if (time_stamp[0] == '\0') {
		strcpy(time_text, "Unknown time");
	} else {
		time_text[0] = time_stamp[6];
		time_text[1] = time_stamp[7];
		time_text[2] = '/';
		time_text[3] = time_stamp[8];
		time_text[4] = time_stamp[9];
		time_text[5] = '/';
		time_text[6] = time_stamp[10];
		time_text[7] = time_stamp[11];
		time_text[8] = ' ';
		time_text[9] = time_stamp[0];
		time_text[10] = time_stamp[1];
		time_text[11] = ':';
		time_text[12] = time_stamp[2];
		time_text[13] = time_stamp[3];
		time_text[14] = ':';
		time_text[15] = time_stamp[4];
		time_text[16] = time_stamp[5];
		time_text[17] = '\0';
	}

	/* Don't log those pesky "INVALID CPU COMMAND" messages that arise
	 * from the system controller's UART's interrupt priority being
	 * too low.
	 */
	if (strncmp(string, "INVALID CPU COMMAND", 19))
		syslog(LOG_NOTICE, "Event: %s, %s",
						time_text, string);
}


void getlog(display_t *sd, int seq)
{
	char buffer[MAX_LOG_INFO];
	int i;

	if ( seq == NULL_SEQ)
	  seq = SC_LOG;

	if(getresult(sd, SC_LOG, seq, buffer)) {
		if (verbose)
			syslog(LOG_NOTICE, "Get log failed!\n");
		(sd->log_fails++);
	} else {
		/* If we were successful in retrieving the log, clear it */
		sendcmd(sd->sysctlr, SC_SET, SC_KILLLOG, 0);
	}

	if (strlen(buffer)) {
		/* Nail that trailing semicolon */
		buffer[strlen(buffer) - 1] = '\0';

		i = strlen(buffer) - 1;
		while (i >= 0) {
			if ((i == 0) || (buffer[i - 1] == ';')) {
				decode_entry(buffer + i);
			}
			if (buffer[i] == ';')
				buffer[i] = '\0';
			i--;
		}
	}
}


/* Check to see that the system serial number is valid.
 */
valid_serial(char *serial_num)
{
	int i;

	if (!serial_num || !strlen(serial_num))
		return 0;
	
	i = 0;
        while(serial_num[i] != '\0') {
                if ((serial_num[i] < 0x20) || (serial_num[i] > 0x7e)
                                        || (i >= SERIALNUMSIZE - 1)) {
                        return 0;
                }
		i++;
        }

	return 1;
}


void printpbstatus(void)
{
	evcfginfo_t evconfig;

	if (syssgi(SGI_GET_EVCONF, &evconfig, sizeof(evconfig))) {
		printf("Unable to report piggyback status\n");
		return;
	}

	check_ccrev(&evconfig);

	return;
}


void getpromerrs(void)
{
	evcfginfo_t evconfig;

	if (syssgi(SGI_GET_EVCONF, &evconfig, sizeof(evconfig))) {
		syslog(LOG_ERR, "Unable to retrieve PROM errors.\n");
		return;
	}

	check_inventory(&evconfig);

	if (!valid_serial(evconfig.ecfg_snum)) {
		fprintf(stderr, "\007\nWARNING: The serial number on this system controller is not valid!\n");
		syslog(LOG_ALERT, "WARNING: The serial number on this system controller is not valid!\n");
	}
}


void logswitches(display_t *sd)
{
	char buffer[10];

	if (getresult(sd, SC_DEBUG, SC_DEBUG, buffer)) {
		syslog(LOG_DEBUG, "Get switch settings failed!\n");
		return;
	}

	if (strtol(buffer, (char **)NULL, 16))
		syslog(LOG_NOTICE, "Debug settings: 0x%s", buffer);
}


int createshmem(display_t *sd)
{
	sd->shmid = shmget(sd->key, sizeof(sysctlr_env_t), IPC_CREAT | 0644);

	if (sd->shmid < 0) {
		syslog(LOG_NOTICE, "Unable to set up IPC\n");
		sd->shmaddr = 0;
		return -1;
	}

	sd->shmaddr = shmat(sd->shmid, (void *)NULL, 0);

	if ((int)sd->shmaddr < 0) {
		syslog(LOG_NOTICE, "Unable to attach shared memory\n");
		sd->shmaddr = 0;
		return -1;
	}
	return 0;
}


attach_ro_shmem(display_t *sd)
{
	sd->shmid = shmget(sd->key, sizeof(sysctlr_env_t), 0644);

	if (sd->shmid < 0)
		sd->shmid = shmget(sd->key, 56, 0644);

	if (sd->shmid < 0) {
		fprintf(stderr, "sysctlrd: Can't contact daemon.  Exiting.\n");
		exit(1);
	}

	sd->shmaddr = shmat(sd->shmid, (void *)NULL, SHM_RDONLY);

	if ((int)sd->shmaddr < 0) {
		fprintf(stderr,
		    "sysctlrd: Unable to attach shared memory.  Exiting.\n");
		exit(1);
	}
	return 0;
}


void cleanup()
{
	display_string(display.sysctlr, SC_MESSAGE, " ");
	display_string(display.sysctlr, SC_MESSAGE, "Sysctlrd killed!");

	if (display.shmid >= 0)
		shmctl(display.shmid, IPC_RMID);

	syslog(LOG_NOTICE, "System controller daemon killed.\n");
	exit(0);
}


void copy_out_env(display_t *sd, sysctlr_env_t *ei)
{
	if((int)sd->shmaddr <= 0)
		return;

	if (ei->valid)
		*((sysctlr_env_t *)sd->shmaddr) = *ei;
}

void copy_in_env(display_t *sd, sysctlr_env_t *ei)
{
	if((int)sd->shmaddr <= 0) {
		fprintf(stderr,
			"sysctlrd: Can't copy from shared memory.  Exiting.\n");
		exit(1);
	}

	*ei = *((sysctlr_env_t *)sd->shmaddr);
}


main(int argc, char *argv[])
{
	int c;
	sysctlr_env_t environ;
	int daemon_mode = 0;
	int suicide_mode = 0;
	int print_info = 0;
	int raw_info = 0;
	int no_env_info = 0;
	char panel = '?';
	unsigned int cycle;
	int frequency = -1;
	int already_complained = 0;
	cap_t ocap;
	cap_value_t cap_sched_mgt = CAP_SCHED_MGT;

	clk_tck = CLK_TCK;	/* This is a system call so cache it! */
	no_powermeter = 0;
	verbose = 0;
	graceful_powerdown = 0;

	/* What mode is the system controller in?  0 = don't send command
	 *	lengths, 1 = send them.
	 */
	cmd_len = 0;

	/* The e,n,s,t options are temporary.  Please don't use them for
	 * programs that will see the outside of B7
	 */	
	while((c = getopt(argc, argv, "dgprnestf:vlhk")) != -1) {
		switch(c) {
			case 'd':
				if (getuid() != 0) {
					fprintf(stderr,
						      "Must be root to run daemon.\n");
					exit(1);
				} else {
					daemon_mode = 1;
				}
				break;
			case 's':
				no_env_info = 1;
				break;
			case 'n':
				no_powermeter = 1;
				break;
			case 'p':
				print_info = 1;
				break;
			case 'r':
				raw_info = 1;
				break;
			case 'e':
				panel = 'E';
				break;
			case 't':
				panel = 'T';
				break;
			case 'f':
				frequency = clk_tck * atoi(optarg) / 10;
				break;
			case 'h':
				printpbstatus();
				exit(0);
			case 'v':
				verbose = 1;
				break;
			case 'g':
				graceful_powerdown = 1;
				break;
		        case 'l' :
			       log_vdc_delta = 1;
			       break;
			case 'k':
				if (getuid() != 0) {
					fprintf(stderr,
					      "Must be root to power down.\n");
					exit(1);
				} else {
					suicide_mode = 1;
				}
				break;
			case '?':
				fprintf(stderr,
					"Usage: %s -d | -p | -r | -h\n",
					argv[0]);
				exit(1);
				break;
		}
	}

	if (suicide_mode) {
		daemon_mode = 1;
	}

	if (!daemon_mode && !print_info && !raw_info) {
		fprintf(stderr, "Usage: %s -d [-g] | -p | -r\n", argv[0]);
		return 1;
	}

	display.key = 0x53637444;		/* SctD */

	if (!daemon_mode) {
		attach_ro_shmem(&display);

		copy_in_env(&display, &environ);

		if (!environ.valid) {
			fprintf(stderr, "sysctlrd: Environment information is uninitialized!\n");
			return 1;
		}

		if (print_info) {
			printenvinfo(&environ);
		} else if (raw_info) {
			printrawinfo(&environ);
		}

		exit(0);
	}

	/* If we've made it here, we're the daemon. */

	/* Open our syslog connection */
	openlog("sysctlrd", LOG_PID, LOG_DAEMON);

	if (!graceful_powerdown && !suicide_mode)
		syslog(LOG_NOTICE, "Clean power-down turned off (see chkconfig).\n");
	if (no_powermeter && !suicide_mode)
		syslog(LOG_NOTICE, "CPU meter turned off (see chkconfig).\n");

	/* Set a non-degrading priority higher than normal procs, but lower
	 * than anyone else who asks for such a priority.
	 */

	ocap = cap_acquire(1, &cap_sched_mgt);
	schedctl(NDPRI, NDPHIMIN);
	cap_surrender(ocap);

	signal(SIGTERM, cleanup);
	signal(SIGQUIT, cleanup);
	signal(SIGINT, cleanup);

	/* We'll be sending sig power so ignore it. */
	signal(SIGHUP, SIG_IGN);

	initinfo(&display, &environ);

	/* Get prom errors but don't report piggyback read state. */
	getpromerrs();

	openport(&display);

	/* Wait longer for this stuff! */
	ioctl(display.sysctlr, SYSC_RTOUT, 3 * HZ);

	checkmsgs(&display);

	set_length_mode(&display);

	if (suicide_mode)
		shut_down_power(&display);

	getscale(&display, panel, &environ);

	if (display.sc_scale <= 0)
		exit(1);

	/* If we haven't requested an update rate set the one for that panel. */
	if (frequency == -1)
		frequency = display.sc_update;

	getlog(&display, NULL_SEQ);

	logswitches(&display);

	settime(&display);

	createshmem(&display);
	
	cycle = 0;

	/* Back to a more reasonable timeout! */
	ioctl(display.sysctlr, SYSC_RTOUT, HZ);

	while (1) {
		checkmsgs(&display);
		if (!no_powermeter) {
			getcpuinfo(&display);
			dumpinfo(&display, &environ);
		}
		if (!(cycle % (30 * clk_tck / frequency)) && !no_env_info) {
 		      /* First time through we initialize the extrema array,
		       *  after that, if we detect a change we log it to SYSLOG.
		       */
			getenvinfo(&display, &environ);
			on_initial_setup = 0;

			copy_out_env(&display, &environ);
			if (display.consec_rtouts >= TIMEOUT_GIVEUP) {
				no_env_info = 1;
				syslog(LOG_ERR,
			  "Giving up on fetching environmental information.\n");
			}
			if (!already_complained) {
				already_complained =
						check_sysctlr_rev(&environ);
			}
		}
		if (!no_env_info) {
			cycle++;
			if (!(cycle % (LOG_INTERVAL * clk_tck / frequency)))
				getlog(&display, NULL_SEQ);
		}
#define DEBUG
#ifdef DEBUG
		getlog (&display, environ.seq);
		getlog (&display, NULL_SEQ);
#endif
		sginap (frequency * (1 + display.consec_wtouts));
						/* Wait 1/2 or 3 seconds */
		/* If we've been timing out on writes, slow down. */
	}
}

