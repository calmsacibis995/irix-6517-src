/*
** File: onlinediag.c
** ------------------
** Test cpus to see if they run simple diags correctly.  Normally run as
** a cron job.
*/

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>
#include <sys/sysmp.h>
#include <sys/pda.h>
#include <sys/fcntl.h>
#include <sys/wait.h>
#include <syslog.h>
#include <sys/capability.h>
#include <sys/resource.h>
#include <sys/stat.h>

#define BUFFSIZE 		512
#define NUM_FILES		64
#define DEFAULT_LOGFILE		"/var/adm/onlinediag.logfile"
#define DEFAULT_CONFIG		"/etc/config/onlinediag.config"
#define DEFAULT_PRIORITY	10
#define	PARAMETERS		"[-p procnum] [-c config] [-l logfile] [-rdkf]"

enum { MUSTRUN_FAILURE = EXIT_FAILURE+1, 
       EXEC_FAILURE, 
       WAIT_FAILURE, 
       FORK_FAILURE,
       FINISH_FAILURE
};

typedef struct {
    char *args[BUFFSIZE];
} argsT;

/* helper functions */
static void run_diags(int logfd, int cpu_num, int* restrict, 
		      int* disable, struct pda_stat *p, int forced);
static int check_diags(int logfd, int cpu_num);
static int check_exit_error(int logfd, char *binary, int cpu, int status);
static void log_cpu_failure(int cpu_num, int restrict, int disable);
static int read_config_file(char *config_file, char *program);
static void free_binaryfiles(void);

/* globals */
int keep_logfile = 0;
argsT *binaryfiles[NUM_FILES];


int main(int argc, char *argv[])
{
    int c, err = 0, r;
    int logfd;
    char err_string[BUFFSIZE];
    int cpu_num = -1, restrict = 0, disable = 0, forced = 0;
    int num_processors;
    struct pda_stat *pstatus;
    extern errno;
    char *config_file = DEFAULT_CONFIG;
    char *logfile = DEFAULT_LOGFILE;
    char *tmp_logfile;
    int priority = DEFAULT_PRIORITY;

    if((num_processors = sysmp(MP_NPROCS)) < 1) {
	fprintf(stderr, "%s: sysmp(MP_NPROCS) failed\n", argv[0]);
	exit(1);
    }

    while ((c = getopt(argc, argv, "p:rdc:l:kP:")) != EOF) {
	switch (c) {
	case 'p': /* processor number to run diags on */
	    if ((cpu_num = strtol(optarg, (char **) 0, 0)) < 0 ||
		cpu_num >= num_processors) {
		fprintf(stderr, "%s: invalid processor number\n", argv[0]);
		exit(1);
	    }
	    break;
	case 'r': /* restrict the scheduling of any cpu found faulty */
	    restrict = 1;
	    break;
	case 'd': /* disable a cpu on reboot if found faulty */
	    disable = 1;
	    break;
	case 'c': /* use the specified config file for list of binaries */
	    config_file = optarg;
	    break;
	case 'l': /* print out into the specified log file */
	    logfile = optarg;
	    break;
	case 'k': /* keep the log file regardless of failure or not */
	    keep_logfile = 1;
	    break;
	case 'f': /* force to run tests on _all_ cpus even, restricted
		     and isolated ones */
	    forced = 1;
	    break;
	case 'P': /* sets priority of that this program runs at. */
	    priority = strtol(optarg, NULL, 0);
	    break;
	case '?':
	    err++;
	    break;
	}       
    }

    if (err) {
	fprintf(stderr, "usage: %s %s\n", argv[0], PARAMETERS);
	exit(1);
    }

    /* get status of processors */
    pstatus = (struct pda_stat *) 
	calloc(num_processors, sizeof(struct pda_stat));
    if(!pstatus) {
	fprintf(stderr, "%s: malloc failed\n", argv[0]);
	exit(1);
    }
    if(sysmp(MP_STAT, pstatus) < 0) {
	sprintf(err_string, "%s: sysmp(MP_STAT) failed", argv[0]);
	perror(err_string);
	exit(1);
    }
    
    if (setpriority(PRIO_PROCESS, getpid(), priority) < 0) {
	sprintf(err_string, "%s: setpriority failed", argv[0]);
	perror(err_string);
	exit(1);
    }

    /* deal with config file */
    if((r = read_config_file(config_file, argv[0])) <= 0) {
	if(r == -1) 
	    sprintf(err_string, "%s: can't open config file\n", argv[0]);
	else sprintf(err_string, "%s: out of memory", argv[0]);
	perror(err_string);
	exit(1);
    }	

    /* deal with temp log */
    tmp_logfile = malloc(strlen(logfile) + 15);
    sprintf(tmp_logfile, "%s_%d", logfile, getpid());
    if((logfd = open(tmp_logfile, O_WRONLY | O_CREAT | O_TRUNC, 
		     S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH)) < 0) {
	sprintf(err_string, "%s: can't open temporary logfile \"%s\"", 
		argv[0], tmp_logfile);
	perror(err_string);
	exit(1);
    }
    
    /* check out the cpus */
    if(cpu_num >= 0) {
	run_diags(logfd, cpu_num, &restrict, &disable, &(pstatus[cpu_num]),
		  forced);
    } else {
	for(cpu_num = num_processors -1; cpu_num >= 0; cpu_num--) {
	    run_diags(logfd, cpu_num, &restrict, &disable, 
		      &(pstatus[cpu_num]), forced);
	}
    }	
    
    /* if we have an error, keep the log file */
    close(logfd);
    if(keep_logfile) rename(tmp_logfile, logfile);
    else unlink(tmp_logfile);
    free(pstatus);
    free(tmp_logfile);
    free_binaryfiles();
    return 0;
}


/* run_diags
** ---------
** call check_diags to run the binaries, then restrict and disable
** the cpu if check_diags returns true and we are supposed to and
** able to.  Then log the failure.
*/
static void run_diags(int logfd, int cpu_num, int* restrict, 
		      int* disable, struct pda_stat *p, int forced)
{
    cap_t ocap;
    cap_value_t cap_value;
    
    if(forced || (p->p_flags & PDAF_ENABLED && 
		  !(p->p_flags & PDAF_ISOLATED))) {
	if(check_diags(logfd, cpu_num)) {
	    
	    /* set the restrict flag for the syslog entry */
	    if((p->p_flags & PDAF_ENABLED) && *restrict) {
		cap_value = CAP_SCHED_MGT;
		ocap = cap_acquire(1, &cap_value);
		if(sysmp(MP_RESTRICT, cpu_num) < 0) 
		  *restrict = 0;
		cap_surrender(ocap);
	    } else if(~(p->p_flags) & PDAF_ENABLED) {
	      *restrict = 1;
	    }	
	    
	    /* set the disable flag for the syslog entry */
	    if((~(p->p_flags) & PDAF_DISABLE_CPU) && *disable) {
		cap_value = CAP_NVRAM_MGT;
		ocap = cap_acquire(1, &cap_value);
		if(sysmp(MP_DISABLE_CPU, cpu_num) < 0) 
		  *disable = 0;
		cap_surrender(ocap);
	    } else if(p->p_flags & PDAF_DISABLE_CPU) {
		*disable = 1;
	    }
	    log_cpu_failure(cpu_num, *restrict, *disable);
	}
    }
}


/* check_diags
** -----------
** fork a child for every binary in our list and run the binary on the
** specified cpu number.  Make sure that the output of the binary goes
** to the log file and call check_exit_error to print out any errors
** that occur for the binary.  Return failed as true if any of the 
** binaries return a EXIT_FAILURE and a false for failed if not.
*/
static int check_diags(int logfd, int cpu_num)
{
    int status = 0, failure = 0;
    int pid;
    int i;
    char error_str[BUFFSIZE];
    time_t time_val;
    char *now;

    for(i = 0; binaryfiles[i]; i++) {
      
	(void)time(&time_val);
	now = ctime(&time_val);
	sprintf(error_str, 
		"\n\n************* RUNNING %s FOR CPU #%d at %s\n", 
		binaryfiles[i]->args[0], cpu_num, now);
	write(logfd, error_str, strlen(error_str));

	switch((pid = fork())) {
	case -1: /* fork failed */
	    status = WAIT_FAILURE;
	    break; 
	case 0: /* child execs binary */
	    dup2(logfd, STDOUT_FILENO);
	    dup2(logfd, STDERR_FILENO);
	    close(logfd);
	    if(sysmp(MP_MUSTRUN, cpu_num) < 0) exit(MUSTRUN_FAILURE);
	    execvp(binaryfiles[i]->args[0], binaryfiles[i]->args);
	    exit(EXEC_FAILURE);
	    break;
	default: /* parent waits for child */
	    if(waitpid(pid, &status, 0) == -1) status = WAIT_FAILURE;
	    else {
		if(!(WIFEXITED(status))) status = FINISH_FAILURE;
		else status = WEXITSTATUS(status);
	    }
	    break;
	}

	failure += check_exit_error(logfd, binaryfiles[i]->args[0], 
				    cpu_num, status);
	sprintf(error_str, 
		"\n------------- DONE WITH %s FOR CPU #%d\n", 
		binaryfiles[i]->args[0], cpu_num);
	write(logfd, error_str, strlen(error_str));
    }
    return(failure);
}


/* check_exit_error
** ----------------
** See if the binary failed for some reason (run failure, exec failure
** or couldn't run on particular cpu).  If it did, write the failure
** message to the log file and make sure that we keep the log file.
*/
static int check_exit_error(int logfd, char *binary, int cpu, int status)
{
    int failure = 0;
    char error_str[BUFFSIZE];

    if(status) {
	switch(status) {
	case MUSTRUN_FAILURE:
	    sprintf(error_str, "ERROR: Can't run binary %s as mustrun "
		    "process on cpu #%d\n", binary, cpu);
	    break;
	case EXEC_FAILURE:
	    sprintf(error_str, "ERROR: Can't exec %s for cpu #%d\n", 
		    binary, cpu);
	    break;
	case FORK_FAILURE:
	    sprintf(error_str, "ERROR: Can't fork process for binary %s "
		    "cpu #%d\n", binary, cpu);
	    break;
	case WAIT_FAILURE:
	    sprintf(error_str, "ERROR: waitpid failed for binary %s "
		    "cpu #%d\n", binary, cpu);
	    break;
	case FINISH_FAILURE:
	    sprintf(error_str, "ERROR: Didn't finish executing binary %s "
		    "cpu #%d\n", binary, cpu);
	    break;
	case EXIT_FAILURE:
	default:
	    sprintf(error_str, "ERROR: Diag %s for cpu #%d failed\n", 
		    binary, cpu);
	    failure = 1;
	    break;
	}
	write(logfd, error_str, strlen(error_str));
	keep_logfile = 1;
    }
    return(failure);
}


/* log_cpu_failure
** ---------------
** a cpu failed one of the tests, add a note to the SYSLOG file
*/
static void log_cpu_failure(int cpu_num, int restrict, int disable)
{
    char hw_path[BUFFSIZE];
    char* restrict_pre = (char*)calloc(10,1);
    char* disable_pre  = (char*)calloc(10,1);
    char* preamble = (char*)calloc(80,1);

    if(sysmp(MP_CPU_PATH, cpu_num, hw_path, BUFFSIZE) < 0) 
      hw_path[0] = '\0';
    else 
      strncat(hw_path, ": ", (BUFFSIZE - strlen(hw_path)) - 1);

    if(!restrict)
      sprintf(restrict_pre, "%s", "not ");
    else 
      restrict_pre = "";
    
    if(!disable)
      sprintf(disable_pre, "%s", "not ");
    else 
      disable_pre = "";

    openlog("onlinediag", LOG_PID | LOG_CONS, LOG_USER);
    
    /* These tags are for Availmon */
    /* Let the user know that the system has a bad FPU ... */
    sprintf(preamble,"(MAINT-NEEDED");

    /* If we restricted usage of this proc, notify the user ... */
    if(restrict)
      strcat(preamble, ":SYS-DEGRADED");

#ifdef COMMENT
    /* If we disabled this proc, let the user know we took this action ... */
    if(disable)
      strcat(preamble, ":TOOK-ACTION");
#endif

    strcat(preamble,"):");

    syslog(LOG_CRIT | LOG_USER, 
	   "%scpu %d: %sfails floating point diagnostics; "
	   "it is %srestricted and will %sbe disabled on reboot\n", 
	   preamble,cpu_num, hw_path, restrict_pre, disable_pre);

    closelog();
    free(disable_pre);
    free(restrict_pre);
    free(preamble);
}


/* read_config_file
** ----------------
** Read through the config file ignoring white space and all info
** on a line after a '#' character.  Put the names found in the
** config file into the global binaryfiles array and make sure that
** it is NULL terminated.
*/
static int read_config_file(char *config_file, char *program)
{
    char name[BUFFSIZE];
    int ch, i, filenum = 0;
    char *pathname, *arg;
    FILE *file;

    if(!(file = fopen(config_file, "r"))) return -1;	
 
    ch = getc(file);
    while(ch != EOF && filenum < NUM_FILES) {

	/* ignor white space */
	while(ch != EOF && isspace(ch)) ch = getc(file);

	/* ignor till end of line if we see a '#' */
	if(ch == EOF || ch == '#') {
	    while(ch != EOF && ch != '\n') ch = getc(file);
	    continue;
	}	

	/* get the binary name */
	for(i = 0; ch != EOF && ch != '\n' && ch != '#' && i < BUFFSIZE-1; 
	    i++) {
	    name[i] = ch;
	    ch = getc(file);
	}
	name[i] = '\0';

	/* add binary name and args to list of binaries */
	arg = strtok(name, " \t");
	binaryfiles[filenum] = malloc(sizeof(argsT));
	if(!binaryfiles[filenum]) return 0;
	for(i = 0; arg && i < BUFFSIZE-1; i++) {
	    binaryfiles[filenum]->args[i] = malloc(strlen(arg) + 1);
	    if(!binaryfiles[filenum]->args[i]) return 0;
	    strcpy(binaryfiles[filenum]->args[i], arg);
	    arg = strtok(NULL, " \t");
	}
	binaryfiles[filenum]->args[i] = NULL;

	filenum++;
    }

    binaryfiles[filenum] = NULL;
    return 1;
}	

/* free_binaryfiles
** ----------------
** free the memory associated with the binary files array
*/
static void free_binaryfiles(void)
{
    int i, j;

    for(i = 0; binaryfiles[i]; i++) {
	for(j = 0; binaryfiles[i]->args[j]; j++) {
	    free(binaryfiles[i]->args[j]);
	}
	free(binaryfiles[i]);
    }
}
