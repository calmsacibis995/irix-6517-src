#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <time.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <search.h>
#include <sys/sysmp.h>
#include <sys/errno.h>
#include <sys/utsname.h>

#include <sys/klstat.h>


#ifdef TIMER
extern void cycletrace(char*);
#define TSTAMP(s)	cycletrace(s);
#else
#define TSTAMP(s)
#endif

char *helpstr[] = {
"Name"
"	lockstat - display kernel lock statistics",
"",
"SYNOPSIS",
"	lockstat on/off",
"	lockstat [<options> ...] command [args...]",
"	lockstat [<options> ...] <sec> [<repeat>]",
"",
"DESCRIPTION",
"	This command will display statistics on the number of times kernel locks",
"	are set and the amount on contention that occurs for each lock.",
"",
"	The first form of the command is used to turn lock statistics collection",
"	on or off.",
"	The second form of the command is used to collect lock statistics during",
"	execution of the specified command.",
"	The third form if the command is used to periodically sample lock ",
"	statistics. The sample time is specified by <sec> and the sample may",
"	be repeated <repeat> times.",
"",
"OPTIONS",
"	-u <unix>	Location of unix to use for symbol table. If not",
"			specified, the value of the UNIX environment variable",
"			will be used. If neither is specified, /unix is used",
"	-i <string>	String to print as part of the title for the output.",
"	-c <cpu>|a	By default, the statistics for all cpus are summed",
"			together. This argument specifies individual cpus that",
"			should be reported separately. This option may be",
"			repeated multiple times to select multiple cpus.",
"	-t		Normally, incremental statistics are reported. If -t",
"			is specified, total counts since lockstats were enabled",
"			is reported.",
"	-S         	Show semaphore information. This is not selected by default",
"                       because I am not sure it is useful.",
"	-p <persec>	Report only on locks set more than <persec>",
"			times per second.",
"	-k <percent>	Report only on locks with more than <percent> contention.",
"	-w		Report on \"warm or hot locks\" only. This option reports on",
"			all locks with any contention.",
"	-h		Report on \"hot locks\" only. This option is the same as",
"			selecting: -p 100 -k 5",
"",
"REPORT",
"	The report is divided into several sections:",
"		SPINLOCKS & MUTEXES",
"		MRLOCKS",
"		MRLOCK SPINLOCK (the spinlock that protects the mrlock",
"		SEMAPHORES (if -S is selected)",
"",
"	The following data is cellected for each lock:",
"",
"	TOT/SEC	Number of times per second that the lock was set.",
"	CON	Amount of contention that occurred for the lock. The ",
"		number represents the percent of time that lock was NOT",
"		acquired without spinning or sleeping. Note: for",
"		semaphores, the number represents the % of the time",
"		psema slept.",
"	SPIN	Gives a rough measure of the amount of time that ",
"		we spent waiting for a lock. For spinlock, the number",
"		represents the number of times thru the spinloop. For",
"		sleeping locks, the number is the wait time in microseconds.",
"	TOTAL	Total number of times that the lock was set.",
"	NOWAIT	Number of times the lock was acquired without waiting",
"	SPIN	Number of times it was necessary to spin waiting for",
"		a spinlock.",
"	SLEEP	Number of times it was necessary to sleep for a lock",
"	REJECT	Number of time a \"trylock\" failed.",
"	NAME	Identifies the lock and/or the routine that set the lock.",
"		If the the is statically defined and not part of an array,",
"		both the lock name and the functions that set the ",
"		lock are listed. If the lock is dynamically allocated,",
"		only the function name 	that set the lock will be listed.",
"		only the function name 	that set the lock will be listed.",
"",
"Examples",
"	To activate collection:",
"		lockstat on",
"",
"	To collect information on hot locks for execution of a command:",
"		lockstat -h <command>",
"",
"	To collect a 60 second snapshot of lock activity:",
"		lockstat 60",
"",
"",
"CONFIGURING",
"	The default kernel does not include the necessary components for",
"	collecting lock statistics. You need to change the following lines",
"	in the system/irix.sm file:",
"		Change:",
"			INCLUDE: hardlocks",
"			INCLUDE: ksync",
"		To:",
"			INCLUDE: hardlocks_statistics",
"			INCLUDE: ksync_statistics",
"",
"	Then rebuild the kernel & reboot.",
NULL};


#define MAXCPUS			512
#define STRMAX			100
#define SPACEINC		16384*4

#define perrorx(s)		perror(s), exit(1)
#define fatalx(m)		fprintf(stderr, "ERROR - %s\n", m), exit(1)
#define notex(m)		fprintf(stderr, "%s\n", m), exit(0)
#define max(a,b)		(((a)<(b)) ? (b) : (a))
#define min(a,b)		(((a)>(b)) ? (b) : (a))

typedef enum	{Buf_Previous, Buf_Current} get_data_buffer_enum ;
typedef enum	{Null_Entry, Special_Entry, Normal_Entry, Mr_Entry, MrSpin_Entry, Sema_Entry} entry_type_enum;

typedef struct {
	lstat_lock_counts_t	counts;
	uint32_t		total;
	int			contention;
	double			persec;
} lock_summary_t;
	
typedef struct {
	__psunsigned_t	caller_ra;
	char		*caller_name;
	char		*lock_name;
	char		multilock;
	char		title_index;
	char		caller_name_len;
	entry_type_enum	entry_type;
} directory_entry_t;

directory_entry_t	directory[LSTAT_MAX_STAT_INDEX];
int			next_free_dir_index;

lstat_directory_entry_t	kernel_directory[LSTAT_MAX_STAT_INDEX];
lstat_directory_entry_t	prev_directory[LSTAT_MAX_STAT_INDEX];

lstat_cpu_counts_t	*kernel_counts;
lstat_cpu_counts_t	*prev_counts;
lstat_lock_counts_t	total_counts[LSTAT_MAX_STAT_INDEX];
short			sorti[LSTAT_MAX_STAT_INDEX+2];

int			numcpus = 0;
int			enabled;
void			*kernel_magic_addr;
void			*kernel_end_addr;
time_t			start_time, end_time;
double			deltatime;
int			skipline = 0;
char			*current_header, *last_header;

char	*dashes = "- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -\n";
char	*special_names[] = {
	"ID 00",
	"ID 04",
	"ID 08",
	"ID 16",
	"ID 20",
	"ID 24",
	"ID 28",
	"ID 32",
	"ID 36",
	"ID 40",
	"ID 44",
	"ID 48",
	"ID 52",
	"ID 56",
	"ID 60",
	""
};

#define SPECIAL_LO		((void*)4LL)
#define SPECIAL_HI		((void*)60LL)
char	*special_titles[] = {
	"SPINLOCKS & MUTEXES\n  TOT/SEC CON  SPIN       TOTAL      NOWAIT     SPIN    SLEEP   REJECT  NAME", /* normal */
	"MRLOCKS\n  TOT/SEC CON  WAIT        TOTAL      NOWAIT     SPIN    SLEEP   REJECT  NAME", /* normal */
	"MRLOCK SPINLOCK\n  TOT/SEC CON  SPIN        TOTAL      NOWAIT     SPIN    SLEEP   REJECT  NAME", /* normal */
	"SEMAPHORES  \n  TOT/SEC CON       TOTAL      NOWAIT     SPIN    SLEEP   REJECT  NAME", /* sema */
	"SPEC4 \n  TOT/SEC CON       TOTAL        VAL1     VAL2     VAL3     VAL4  NAME",
	"SPEC8 \n  TOT/SEC CON       TOTAL        VAL1     VAL2     VAL3     VAL4  NAME",
	"SPEC12\n  TOT/SEC CON       TOTAL        VAL1     VAL2     VAL3     VAL4  NAME",
	"SPEC16\n  TOT/SEC CON       TOTAL        VAL1     VAL2     VAL3     VAL4  NAME",
	"SPEC20\n  TOT/SEC CON       TOTAL        VAL1     VAL2     VAL3     VAL4  NAME",
	"SPEC24\n  TOT/SEC CON       TOTAL        VAL1     VAL2     VAL3     VAL4  NAME",
	"SPEC28\n  TOT/SEC CON       TOTAL        VAL1     VAL2     VAL3     VAL4  NAME",
	"SPEC32\n  TOT/SEC CON       TOTAL        VAL1     VAL2     VAL3     VAL4  NAME",
	"SPEC36\n  TOT/SEC CON       TOTAL        VAL1     VAL2     VAL3     VAL4  NAME",
	"SPEC40\n  TOT/SEC CON       TOTAL        VAL1     VAL2     VAL3     VAL4  NAME",
	"SPEC44\n  TOT/SEC CON       TOTAL        VAL1     VAL2     VAL3     VAL4  NAME",
	"SPEC48\n  TOT/SEC CON       TOTAL        VAL1     VAL2     VAL3     VAL4  NAME",
	"SPEC52\n  TOT/SEC CON       TOTAL        VAL1     VAL2     VAL3     VAL4  NAME",
	"SPEC56\n  TOT/SEC CON       TOTAL        VAL1     VAL2     VAL3     VAL4  NAME",
	"SPEC60\n  TOT/SEC CON       TOTAL        VAL1     VAL2     VAL3     VAL4  NAME",
	""};


void	set_header(char*);
void	enable_statistic_collection(int);
void	do_report(char *, int);
void	print_stats(char *, char *);
void	set_header(char *);
void	reset_counts(lock_summary_t *);
void	add_counts(lock_summary_t *, lstat_lock_counts_t *);
int	sum_counts(lock_summary_t *, entry_type_enum);
int	set_counts(lock_summary_t *, lstat_lock_counts_t *, entry_type_enum);
void	do_help(void);
void	print_header(void);
void	print_title(char *, char *);
void	print_lock(char *, lock_summary_t *, int, entry_type_enum);
void	get_collection_state(void);
void	get_kernel_data(get_data_buffer_enum);
void	build_sort_directory(void);
int	sortcmp(const void *, const void *);
void	sum_data (int, int);
int	loadsymtab (char *, void *, void *);
void	myaddress_to_symbol(void *, char *);
char*	strspace(void);
int	read_diff_file(void);
void	write_diff_file(void);


extern int	optind, opterr, errno;
extern char	*optarg;

int	semaopt = 0, topt = 0, debugopt = 0;
double	opt_contention = -1.0, opt_persec = 0.0;
char	*debugname=NULL;

char	*ident = 0, *namelist = "/unix";

char	cpulist[MAXCPUS];

int
main(int argc, char **argv)
{
	static char	optstr[] = "HSfwhk:p:tc:i:u:D:";
	int		c, err = 0;
	int		args, cpunum;
	char		title[120];

	if (getenv("UNIX"))
		namelist = getenv("UNIX");

	while ((c = getopt(argc, argv, optstr)) != EOF)
		switch (c) {
		case 'D':
			debugopt = 1;
			debugname = optarg;
			break;
		case 'H':
			do_help();
			exit(0);
			break;
		case 'c':
			if (*optarg == 'a') {
				for (cpunum = 0; cpunum< MAXCPUS; cpunum++)
					cpulist[cpunum]++;
			} else {
				cpunum = atoi(optarg);
				if (cpunum < 0 || cpunum >= MAXCPUS)
					fatalx("invalid cpu number specified");
				cpulist[cpunum]++;
			}
			break;
		case 'w':
			opt_persec = 0.0;
			opt_contention = 0.0;
			break;
		case 'h':
			opt_persec = 100.0;
			opt_contention = 5.0;
			break;
		case 'p':
			opt_persec = (double)atoi(optarg);
			break;
		case 'k':
			opt_contention = (double)atoi(optarg);
			break;
		case 'i':
			ident = optarg;
			break;
		case 'S':
			semaopt++;
			break;
		case 't':
			topt++;
			break;
		case 'u':
			namelist = optarg;
			break;
		case '?':
			err = 1;
			break;
		}

	TSTAMP("start");
	if (debugopt && read_diff_file())
		debugopt = 2;
	else
		get_collection_state();

	TSTAMP("inited");
	args = argc - optind;
	if (err || args < 0)
		fatalx("invalid arguments specified");

	if (!args && !topt) {
		do_help();
		return(0);
	}


	if (args == 1) {
		if (strcmp(argv[optind], "on") == 0) {
			enable_statistic_collection (LSTAT_ON);
			return(0);

		} else if (strcmp(argv[optind], "off") == 0)  {
			enable_statistic_collection (LSTAT_OFF);
			return(0);
		}

	}

	if (!enabled)
		fatalx ("lockstat collection not enabled");

	TSTAMP("loadsymtab");
	loadsymtab(namelist, kernel_magic_addr, kernel_end_addr);
	TSTAMP("loadsymtab complete");

	if (topt && !args || isdigit(*argv[optind])) {
		int		i, sleepsec=0, sleepcnt=1;
		if (args) {
			sleepsec = atoi(argv[optind]);
			sleepcnt = (args>1) ? atoi(argv[optind+1]) : 1;
		}
		for (i=1; i<= sleepcnt; i++) {
			if (i > 1)
				fprintf(stderr, "\n\n");
			if (!topt)
				get_kernel_data (Buf_Previous);
			if (sleepsec)
				sleep(sleepsec);
			get_kernel_data (Buf_Current);
			if (topt)
				sprintf(title, "Total counts since lock statistics were enabled\n");
			else
				sprintf(title, "Periodic sample %d of %d. Sample period: %d secs\n",
					i, sleepcnt, sleepsec);
			do_report(title, (i == sleepcnt));
		}

	} else {
		int		pid, stat;

		get_kernel_data (Buf_Previous);
		if ((pid=fork()) == 0) {
			execvp(argv[optind], &argv[optind]);
			perrorx("unable to exec command");
			exit(1);
		} else if (pid < 0)
			perrorx("fork failed");
		signal(SIGINT, SIG_IGN);
		signal(SIGQUIT, SIG_IGN);
		while (wait(&stat) != pid);
		if((stat&0377) != 0)
			fprintf(stderr,"Command terminated abnormally.\n");
		signal(SIGINT, SIG_DFL);
		signal(SIGQUIT, SIG_DFL);
		get_kernel_data (Buf_Current);
		strcpy(title, "Command: ");
		for (; optind < argc; optind++) {
			if(5+strlen(title)+strlen(argv[optind]) > sizeof(title)) {
				strcat(title, " ...");
				break;
			}
			strcat(title, argv[optind]);
			strcat(title, " ");
		}
		do_report(title, 1);
	}

	TSTAMP("done");
	return(0);
}


void
do_help(void)
{
	char	**p;

	for (p=helpstr; *p; p++) 
		printf("%s\n", *p);
}

void
do_report(char *title, int last_report)
{
	int		cpu;
	char		name[100];

	TSTAMP("start do report");
	build_sort_directory();

	TSTAMP("finish sort");
	sum_data(0, numcpus - 1);
	sprintf(name, "All (%d) CPUs", numcpus);
	print_stats(title, name);

	for (cpu = 0; cpu < numcpus; cpu++) {
		if (cpulist[cpu] == 0)
			continue;
		sum_data(cpu, cpu);
		sprintf(name, "CPU:%d", cpu);
		print_stats(title, name);
	}
	if (last_report)
		fprintf (stderr, "___________________________________________________________________________________________\n");
	fflush(stderr);
	TSTAMP("end do report");
}


void
print_stats(char *title1, char *title2)
{
	entry_type_enum		entry_type, current_entry_type = Null_Entry;
	lock_summary_t		sum_count;
	int			i, j, k, si, sj;


	print_title(title1, title2);

	for (i = 1; i < next_free_dir_index; i++) {
		si = sorti[i];
		entry_type = directory[si].entry_type;
		if (entry_type == Sema_Entry && !semaopt)
			continue;
		set_header (special_titles[directory[si].title_index]);

		if (entry_type != current_entry_type) {
			current_entry_type = entry_type;
			reset_counts (&sum_count);
			for (j = i; j < next_free_dir_index; j++) {
				sj = sorti[j];
				if (directory[sj].entry_type != entry_type)
					break;
				add_counts (&sum_count, &total_counts[sj]);
			}
			sum_counts (&sum_count, entry_type);
			skipline = 1;
			print_lock("*TOTAL*", &sum_count, 0, entry_type);
			skipline = 1;
		}


		if (!directory[si].multilock) {
			k = i;
			reset_counts (&sum_count);
			while (k < next_free_dir_index &&  strcmp(directory[sorti[k]].lock_name, directory[si].lock_name) == 0) {
				add_counts (&sum_count, &total_counts[sorti[k]]);
				k++;
			}
			skipline = 1;
			if (sum_counts (&sum_count, entry_type)) {
				print_lock(directory[si].lock_name,  &sum_count, 0, entry_type);
				for (j = i; j < k; j++) {
					sj = sorti[j];
					set_counts (&sum_count, &total_counts[sj], entry_type);
					if (sum_count.total)
						print_lock(directory[sj].caller_name, &sum_count,  1, entry_type);
				}
			}
			i = k - 1;
			skipline = 1;
		} else {
			if (set_counts (&sum_count, &total_counts[si], entry_type))
				print_lock(directory[si].caller_name, &sum_count, 0, entry_type);
		}
	}


}



void
reset_counts(lock_summary_t *sump)
{
	int		j;
	sump->counts.ticks = 0;
	for (j = 0; j < LSTAT_ACT_MAX_VALUES; j++)
		sump->counts.count[j] = 0;
}


int
set_counts(lock_summary_t *sump, lstat_lock_counts_t *countp, entry_type_enum entry_type)
{
	int		j;

	sump->counts.ticks = countp->ticks;
	for (j = 0; j < LSTAT_ACT_MAX_VALUES; j++)
		sump->counts.count[j] = countp->count[j];
	return(sum_counts(sump, entry_type));
}


void
add_counts(lock_summary_t *sump, lstat_lock_counts_t *countp)
{
	int		j;

	sump->counts.ticks += countp->ticks;
	for (j = 0; j < LSTAT_ACT_MAX_VALUES; j++)
		sump->counts.count[j] += countp->count[j];
}


int
sum_counts(lock_summary_t *sump, entry_type_enum entry_type)
{
	int		total, j;
	double		contention;

	for (total = 0, j = 0; j < LSTAT_ACT_MAX_VALUES; j++)
		total += sump->counts.count[j];
	sump->total = total;
	sump->persec = total / deltatime;

	if (!total || entry_type == Special_Entry)
		contention = 0.0;
	else if (sump->counts.count[LSTAT_ACT_NO_WAIT] == 0)
		contention = 100.0;
	else
		contention = 100.0 - (100.0 * sump->counts.count[LSTAT_ACT_NO_WAIT] / total);

	sump->contention = (int)(contention+0.99999);
	return (total && sump->persec > opt_persec && 
			(contention > opt_contention || entry_type == Sema_Entry));
}


void
set_header(char *header)
{
	if (header != last_header) {
		current_header = header;
		last_header = header;
	}
}


void
print_header(void)
{
	if (current_header != NULL) {
		fprintf (stderr, "\n%s", dashes);
		fprintf (stderr, "%s\n", current_header);
		current_header = NULL;
	}
}


void
print_title(char *title1, char *title2)
{
	struct utsname		uts;
	fprintf (stderr, "___________________________________________________________________________________________\n");
	uname (&uts);
	fprintf (stderr, "System: %s %s %s %s %s\n", uts.sysname, uts.nodename, uts.release, uts.version, uts.machine);
	if (ident)
		fprintf(stderr, "Ident: %s\n", ident);
	fprintf(stderr, "%s\n", title1);
	fprintf(stderr, "%s\n", title2);
	if (opt_persec >= 0 || opt_contention >= 0) {
		fprintf(stderr, "Selecting locks: ");
		if (opt_persec >= 0)
			fprintf(stderr, "threshhold: >%d/sec  ", (int)opt_persec);
		if (opt_contention >= 0)
			fprintf(stderr, "contention: >%d%%", (int)opt_contention);
		fprintf(stderr, "\n");
	}
	fprintf(stderr, "\n");
	fprintf(stderr, "Start time: %s", ctime(&start_time));
	fprintf(stderr, "End   time: %s", ctime(&end_time));
	fprintf(stderr, "Delta Time: %.2f sec, slot in use: %d, \n",  deltatime, next_free_dir_index);

}


void
print_lock(char *name, lock_summary_t *sump, int indent, entry_type_enum entry_type)
{
	uint32_t	ticks;
	char		tickch;

	print_header();

	if (skipline)
		fprintf(stderr, "\n");
	skipline = 0;

	fprintf(stderr, "%9.2f", sump->persec);
	fprintf(stderr, "%4d", sump->contention);
	if (entry_type == Mr_Entry || entry_type == Normal_Entry || entry_type == MrSpin_Entry) {
		ticks = sump->counts.ticks;
		if (ticks) {
			if (sump->counts.count[LSTAT_ACT_SLEPT]) {
				ticks /= sump->counts.count[LSTAT_ACT_SLEPT];
				/*fprintf(stderr, "SLEPT %d %d %d\n", sump->counts.ticks, sump->counts.count[LSTAT_ACT_SLEPT], ticks);*/
			} else if (sump->counts.count[LSTAT_ACT_SPIN]) {
				ticks /= sump->counts.count[LSTAT_ACT_SPIN];
				/*fprintf(stderr, "SPIN  %d %d %d\n", sump->counts.ticks, sump->counts.count[LSTAT_ACT_SLEPT], ticks);*/
			} else {
				fatalx("spin/sleep error");
			}
			if (ticks > 1000000000) {
				tickch = 'G';
				ticks /= 1000000000;
			} else if (ticks > 10000000) {
				tickch = 'M';
				ticks /= 1000000;
			} else if (ticks > 10000) {
				tickch = 'K';
				ticks /= 1000;
			} else {
				tickch = ' ';
			}
			fprintf(stderr, "%6d%c", ticks, tickch);
		} else {
			fprintf(stderr, "     0 ");
		}
	}
	fprintf(stderr, "%12d", sump->total);
	fprintf(stderr, "%12d", sump->counts.count[LSTAT_ACT_NO_WAIT]);
	fprintf(stderr, "%9d", sump->counts.count[LSTAT_ACT_SPIN]);
	fprintf(stderr, "%9d", sump->counts.count[LSTAT_ACT_SLEPT]);
	fprintf(stderr, "%9d", sump->counts.count[LSTAT_ACT_REJECT]);
	fprintf(stderr, "%s  %s", (indent ? "  ":""), name);
	fprintf(stderr, "\n");

}


void
get_collection_state(void)
{
	lstat_user_request_t		request;

	if (sysmp (MP_KLSTAT, LSTAT_STAT, &request) < 0) {
		if (errno == ENOTSUP)
			fatalx ("lockstat data collection is not available in this kernel.\n"
				"\tLink with sema_statistics & hardlocks_statistics\n"
				"\t  (type lockstat -H for more information\n");
		else
			perrorx ("Unexpected error trying to get check if lock statistics are available");
	}

	numcpus = request.maxcpus;
	enabled = request.lstat_is_enabled;
	kernel_magic_addr = request.kernel_magic_addr;
	kernel_end_addr = request.kernel_end_addr;
}



void
enable_statistic_collection(int on_off)
{
	if (enabled && on_off)
		notex("Lock statistics collection is already ON");
	else if (! enabled && !on_off)
		notex("Lock statistics collection is already OFF");
	if (sysmp (MP_KLSTAT, on_off, NULL) < 0) {
		if (errno == EPERM)
			fatalx ("You are not authorized to change the state of lockstat collection");
		else
			perrorx ("Unexpected error trying to change lockstat collection state");
	}
}



void	
get_kernel_data(get_data_buffer_enum bufid)
{
	lstat_user_request_t		request;
	static int			start_lbolt, end_lbolt;

	if (debugopt == 2)
		return;

	if (bufid == Buf_Previous) {
		if (!prev_counts)
			prev_counts = malloc(numcpus*sizeof(lstat_cpu_counts_t));
		request.cpu_counts_ptr = prev_counts;
		request.directory_ptr = prev_directory;
	} else {
		if (!kernel_counts)
			kernel_counts = malloc(numcpus*sizeof(lstat_cpu_counts_t));
		request.cpu_counts_ptr = kernel_counts;
		request.directory_ptr = kernel_directory;
	}

	if (sysmp(MP_KLSTAT, LSTAT_READ, &request))
		perrorx ("unexpected err reading lockstat data");

	if (bufid == Buf_Previous) {
		start_time = request.current_time;
		start_lbolt = request.current_lbolt;
	} else {
		if (topt) {
			start_lbolt = request.enabled_lbolt;
			start_time = request.current_time - 
				(request.current_lbolt-request.enabled_lbolt)/HZ;
		}
		end_time = request.current_time;
		end_lbolt = request.current_lbolt;
		deltatime = (double) (end_lbolt - start_lbolt) / 100.0;
	}

	next_free_dir_index = request.next_free_dir_index;
	if (debugopt && bufid == Buf_Current) 
		write_diff_file();
}


void
build_sort_directory(void)
{
	static int	last_next_free_dir_index = 1;
	int		i, lowbits;
	char		*namep;

#ifdef ZZZ
	{
		int	chain[64];
		int	i, j, k, n;
		for (i=0; i<64; i++) 
			chain[i] = 0;
		for (i = 0; i < next_free_dir_index; i++) {
			for (j=kernel_directory[i].next_stat_index, n=0; j; j = kernel_directory[j].next_stat_index)
				n++;
			chain[n]++;
		}	
		printf ("Total entries %d\n", next_free_dir_index);
		for (i=0; i<64; i++) 
			printf("Chain %3d, %5d\n", i, chain[i]);
		exit(0);	
	}
#endif
	for (i = last_next_free_dir_index; i < next_free_dir_index; i++) {
		sorti[i] = i;
		lowbits = (int) ((__psunsigned_t)kernel_directory[i].caller_ra & 3);
		directory[i].caller_ra = ((__psunsigned_t) kernel_directory[i].caller_ra & ~3);
		directory[i].caller_name = strspace();
		myaddress_to_symbol((void*)directory[i].caller_ra, directory[i].caller_name);
		namep = strchr(directory[i].caller_name, '+');
		if (namep)
			directory[i].caller_name_len = (char) (namep - directory[i].caller_name);
		else
			directory[i].caller_name_len = (char)strlen(directory[i].caller_name);

		directory[i].lock_name = NULL;
		directory[i].multilock = 0;
		if (kernel_directory[i].lock_ptr >= SPECIAL_LO && 
				kernel_directory[i].lock_ptr <= SPECIAL_HI) {
			directory[i].lock_name = special_names[(size_t)kernel_directory[i].lock_ptr/4];
			directory[i].title_index = 4 + (int)((uint64_t)kernel_directory[i].lock_ptr / 4);
			directory[i].entry_type = Special_Entry;
		} else {
			if (kernel_directory[i].lock_ptr != LSTAT_MULTI_LOCK_ADDRESS) {
				namep = strspace();						 /* ZZZ */
				myaddress_to_symbol(kernel_directory[i].lock_ptr, namep);
				if (!isdigit(*namep))
					directory[i].lock_name = namep;
				else
					*namep = '\0';
			}
			switch (lowbits) {
			case LSTAT_RA_MR:
				directory[i].entry_type = Mr_Entry;
				directory[i].title_index = 1;
				break;
			case LSTAT_RA_MRSPIN:
				directory[i].entry_type = MrSpin_Entry;
				directory[i].title_index = 2;
				break;
			case LSTAT_RA_SEMA:
				directory[i].entry_type = Sema_Entry;
				directory[i].title_index = 3;
				break;
			default:
				directory[i].entry_type = Normal_Entry;
				directory[i].title_index = 0;
			}
			if (directory[i].lock_name == NULL) {
				directory[i].lock_name = directory[i].caller_name;
				directory[i].multilock = 1;
			}
		}
	}
	last_next_free_dir_index = next_free_dir_index;

	qsort ((void *) &sorti[1], next_free_dir_index - 1, sizeof(sorti[0]), &sortcmp);

}


int
sortcmp(const void *ip, const void *jp)
{
	int		si0, si1, k;

	si0 = *(short *)ip;
	si1 = *(short *)jp;

	k = directory[si0].entry_type - directory[si1].entry_type;
	if (k)
		return (k);


	k = directory[si0].multilock - directory[si1].multilock;
	if (k)
		return (k);

	k = strcmp(directory[si0].lock_name, directory[si1].lock_name);
	if (k)
		return (k);

	k = strncmp(directory[si0].caller_name, directory[si1].caller_name,
			min(directory[si0].caller_name_len,
				directory[si1].caller_name_len));
	if (k)
		return (k);

	k = (int)(directory[si0].caller_ra - directory[si1].caller_ra);
	return(k);

}


void
sum_data (int start_cpu, int end_cpu)
{
	int	i, j, cpu;
	for (i = 0; i < next_free_dir_index; i++) {
		total_counts[i].ticks = 0;
		for (j = 0; j < LSTAT_ACT_MAX_VALUES; j++)
			total_counts[i].count[j] = 0;
		for (cpu = start_cpu; cpu <= end_cpu; cpu++) {
			total_counts[i].ticks += kernel_counts[cpu][i].ticks - 
				(topt ? 0 : prev_counts[cpu][i].ticks);
			for (j = 0; j < LSTAT_ACT_MAX_VALUES; j++)
				total_counts[i].count[j] += kernel_counts[cpu][i].count[j] -  
						(topt ? 0 : prev_counts[cpu][i].count[j]);
		}
	}

}




char *symtab_name(void *addr);

void
myaddress_to_symbol(void *adr, char *namep)
{
	strcpy(namep, symtab_name(adr));
}


/*
 * strspace
 *
 * A crude allocation manager for string space. 
 * Returns pointer to where a string of length 0 .. STRMAX can be placed.
 * On next call, a free space pointer is updated to point to 
 * the location just beyond the last string allocated. 
 *	NOTE: does not support expanding a string once allocated &
 *	another call to strspace is made.
 */
char*
strspace(void)
{
	static char	*space=NULL, *endspace=NULL;

	if (space)
		space += (1 + strlen(space));

	if (space >= endspace) {
		space = malloc(SPACEINC);
		endspace = space + SPACEINC - STRMAX;
	}
	return(space);
}



void
verify_diff_file (void)
{
	int		i;

	for (i = 0; i < next_free_dir_index; i++) {
		if (kernel_directory[i].caller_ra != prev_directory[i].caller_ra &&  prev_directory[i].caller_ra != 0) {
			fprintf(stderr, "caller address mismatch: index:%d, old:%llx, new:%llx", 
					i, prev_directory[i].caller_ra, kernel_directory[i].caller_ra);
			perrorx("caller address mismatch");
		}
	}
}

#define WRITE(s)	if (write(fd, (char *)&s, sizeof(s)) != sizeof(s))	\
				perrorx("write diff stats");
#define READ(s)		if (read(fd, (char *)&s, sizeof(s)) != sizeof(s))	\
				perrorx("read diff stats s ");
#define WRITEN(s,n)	if (write(fd, (char *)&s, (n)) != (n))			\
				perrorx("write diff stats");
#define READN(s,n)	if (read(fd, (char *)&s, (n)) != (n))			\
				perrorx("read diff stats");
int
read_diff_file (void)
{
	int		fd;

	if ((fd = open(debugname, O_RDONLY, 0)) < 0)
		return (0);

	READ(numcpus);
	prev_counts = malloc(numcpus*sizeof(lstat_cpu_counts_t));
	kernel_counts = malloc(numcpus*sizeof(lstat_cpu_counts_t));
	READ(start_time);
	READ(end_time);
	READ(deltatime);
	READ(next_free_dir_index);
	READN(kernel_directory[0], next_free_dir_index*sizeof(lstat_directory_entry_t));
	READN(kernel_counts[0], numcpus*sizeof(lstat_cpu_counts_t));
	READN(prev_counts[0], numcpus*sizeof(lstat_cpu_counts_t));
	READ(kernel_magic_addr);
	READ(kernel_end_addr);

	close(fd);
	verify_diff_file ();
	enabled = 1;
	return(1);
}


void
write_diff_file (void)
{
	int	fd;

	if ((fd = open(debugname, O_WRONLY | O_CREAT, 0666)) < 0)
		perrorx("cant create diff file");

	WRITE(numcpus);
	WRITE(start_time);
	WRITE(end_time);
	WRITE(deltatime);
	WRITE(next_free_dir_index);
	WRITEN(kernel_directory[0], next_free_dir_index*sizeof(lstat_directory_entry_t));
	WRITEN(kernel_counts[0], numcpus*sizeof(lstat_cpu_counts_t));
	WRITEN(prev_counts[0], numcpus*sizeof(lstat_cpu_counts_t));
	WRITE(kernel_magic_addr);
	WRITE(kernel_end_addr);

	close(fd);
	exit(0);
}

