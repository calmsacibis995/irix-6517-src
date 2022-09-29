#ident "$Revision: 1.5 $"

/*
 * Copyright (c) 1980 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 * static char sccsid[] = "@(#)dumpoptr.c	5.1 (Berkeley) 6/5/85";
 */

#include "dump.h"

/*
 *	Query the operator; This fascist piece of code requires
 *	an exact response.
 *	It is intended to protect dump aborting by inquisitive
 *	people banging on the console terminal to see what is
 *	happening which might cause dump to croak, destroying
 *	a large number of hours of work.
 *
 *	Every 2 minutes we reprint the message, alerting others
 *	that dump needs attention.
 */
int	timeout;
char	*attnmessage;		/* attention message */

static void		alarmcatch(void);
static struct mntent	*allocfsent(struct mntent *);
static int		idatesort(const void *, const void *);
static void		sendmes(char *, char *);

int
query(char *question, int default_answer)
{
	char	replybuffer[64];
	int	back;
	FILE	*mytty;

	if ( (mytty = fopen("/dev/tty", "r")) == NULL){
		msg("fopen on /dev/tty fails\n");
		return default_answer;
	}
	attnmessage = question;
	timeout = 0;
	alarmcatch();
	for(;;){
		if ( fgets(replybuffer, 63, mytty) == NULL){
			if (ferror(mytty)){
				clearerr(mytty);
				continue;
			}
		} else if ( (strcmp(replybuffer, "yes\n") == 0) ||
			    (strcmp(replybuffer, "Yes\n") == 0)){
				back = 1;
				goto done;
		} else if ( (strcmp(replybuffer, "no\n") == 0) ||
			    (strcmp(replybuffer, "No\n") == 0)){
				back = 0;
				goto done;
		} else {
			msg("\"Yes\" or \"No\"?\n");
			alarmcatch();
		}
	}
    done:
	/*
	 *	Turn off the alarm, and reset the signal to trap out..
	 */
	alarm(0);
	if (signal(SIGALRM, sigalrm) == SIG_IGN)
		signal(SIGALRM, SIG_IGN);
	fclose(mytty);
	return(back);
}

/*
 *	Alert the console operator, and enable the alarm clock to
 *	sleep for 2 minutes in case nobody comes to satisfy dump
 */
static void
alarmcatch(void)
{
	if (timeout)
		msgtail("\n");
	msg("NEEDS ATTENTION: %s: (\"yes\" or \"no\") ",
		attnmessage);
	signal(SIGALRM, alarmcatch);
	alarm(120);
	timeout = 1;
}

/*
 *	Here if an inquisitive operator interrupts the dump program
 */
void
interrupt()
{
	signal(SIGINT, interrupt);
	msg("Interrupt received.\n");
	if (query("Do you want to abort dump?", 1))
		dumpabort();
}

/*
 *	The following variables and routines manage alerting
 *	operators to the status of dump.
 *	This works much like wall(1) does.
 */
struct	group *gp;

/*
 *	Get the names from the group entry "operator" to notify.
 */	
void
set_operators(void)
{
	if (!notify)		/*not going to notify*/
		return;
	gp = getgrnam(OPGRENT);
	endgrent();
	if (gp == (struct group *)0){
		msg("No entry in /etc/group for %s.\n",
			OPGRENT);
		notify = 0;
		return;
	}
}

struct tm *localclock;

/*
 *	We fork a child to do the actual broadcasting, so
 *	that the process control groups are not messed up
 */
void
broadcast(char *message)
{
	time_t	clock;
	FILE	*f_utmp;
	struct	utmp	utmp;
	char	**np;
	int	pid, s;

	switch (pid = fork()) {
	case -1:
		return;
	case 0:
		break;
	default:
		while (wait(&s) != pid)
			continue;
		return;
	}

	if (!notify || gp == 0)
		exit(0);
	(void) time(&clock);
	localclock = localtime(&clock);

	if((f_utmp = fopen("/etc/utmp", "r")) == NULL) {
		msg("Cannot open /etc/utmp\n");
		return;
	}

	while (!feof(f_utmp)){
		if (fread(&utmp, sizeof (struct utmp), 1, f_utmp) != 1)
			break;
		if (utmp.ut_name[0] == 0)
			continue;
		for (np = gp->gr_mem; *np; np++){
			if (strncmp(*np, utmp.ut_name, sizeof(utmp.ut_name)) != 0)
				continue;
			/*
			 *	Do not send messages to operators on dialups
			 */
			if (strncmp(utmp.ut_line, DIALUP, strlen(DIALUP)) == 0)
				continue;
#ifdef DEBUG
			msg("Message to %s at %s\n",
				utmp.ut_name, utmp.ut_line);
#endif /* DEBUG */
			sendmes(utmp.ut_line, message);
		}
	}
	fclose(f_utmp);
	Exit(0);	/* the wait in this same routine will catch this */
	/* NOTREACHED */
}

static void
sendmes(char *tty, char *message)
{
	char t[50], buf[BUFSIZ];
	char *cp;
	int c, ch;
	int	msize;
	FILE *f_tty;

	msize = strlen(message);
	strcpy(t, "/dev/");
	strcat(t, tty);

	if((f_tty = fopen(t, "w")) != NULL) {
		setbuf(f_tty, buf);
		fprintf(f_tty, "\nMessage from the dump program to all operators at %d:%02d ...\r\n\n"
		       ,localclock->tm_hour
		       ,localclock->tm_min);
		for (cp = message, c = msize; c-- > 0; cp++) {
			ch = *cp;
			if (ch == '\n')
				putc('\r', f_tty);
			putc(ch, f_tty);
		}
		fclose(f_tty);
	}
}

/*
 *	print out an estimate of the amount of time left to do the dump
 */

time_t	tschedule = 0;

void
timeest(void)
{
	time_t	tnow, deltat;

	time (&tnow);
	if (tnow >= tschedule){
		tschedule = tnow + 300;
		if (blockswritten < 500)
			return;	
		deltat = tstart_writing - tnow +
			(time_t)(((double)(tnow - tstart_writing) /
				blockswritten) * esize);
		msg("%3.2f%% done, finished in %d:%02d\n",
			(blockswritten * 100.0) / esize,
			deltat / 3600, (deltat % 3600) / 60);
	}
}

void
msg(char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	fprintf(stderr,"  DUMP: ");
#ifdef TDEBUG
	fprintf(stderr,"pid=%d ", getpid());
#endif
	vfprintf(stderr, fmt, ap);
	fflush(stdout);
	fflush(stderr);
	va_end(ap);
}

void
msgtail(char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
}

/*
 *	Tell the operator what has to be done;
 *	we don't actually do it
 */
static struct mntent *
allocfsent(struct mntent *fs)
{
	struct mntent *new;
	char *cp;

	new = (struct mntent *)malloc(sizeof (*fs));
	cp = malloc(strlen(fs->mnt_dir) + 1);
	strcpy(cp, fs->mnt_dir);
	new->mnt_dir = cp;
	cp = malloc(strlen(fs->mnt_type) + 1);
	strcpy(cp, fs->mnt_type);
	new->mnt_type = cp;
	cp = malloc(strlen(fs->mnt_fsname) + 1);
	strcpy(cp, fs->mnt_fsname);
	new->mnt_fsname = cp;
	cp = malloc(strlen(fs->mnt_opts) + 1);
	strcpy(cp, fs->mnt_opts);
	new->mnt_opts = cp;
	new->mnt_passno = fs->mnt_passno;
	new->mnt_freq = fs->mnt_freq;
	return (new);
}

struct	pfstab {
	struct	pfstab *pf_next;
	struct	mntent *pf_fstab;
};

static	struct pfstab *table = NULL;

void
getfstab(void)
{
	struct mntent *fs;
	struct pfstab *pf;
	FILE *fp;

	fp = setmntent(MNTTAB, "r");
        if (fp == NULL) {
		msg("Can't open %s for dump table information.\n", MNTTAB);
		return;
	}
	while (fs = getmntent(fp)) {
		if (strcmp(fs->mnt_type, MNTTYPE_EFS) != 0)
			continue;
		fs = allocfsent(fs);
		pf = (struct pfstab *)malloc(sizeof (*pf));
		pf->pf_fstab = fs;
		pf->pf_next = table;
		table = pf;
	}
	endmntent(fp);
}

/*
 * Search in the fstab for a file name.
 * This file name can be either the special or the path file name.
 *
 * The entries in the fstab are the BLOCK special names, not the
 * character special names.
 * The caller of fstabsearch assures that the character device
 * is dumped (that is much faster)
 *
 * The file name can omit the leading '/'.
 */
struct mntent *
fstabsearch(char *key)
{
	struct pfstab *pf;
	struct mntent *fs;

	if (table == NULL)
		return 0;
	for (pf = table; pf; pf = pf->pf_next) {
		fs = pf->pf_fstab;
		if (strcmp(fs->mnt_dir, key) == 0)
			return (fs);
		if (strcmp(fs->mnt_fsname, key) == 0)
			return (fs);
		if (strcmp(rawname(fs), key) == 0)
			return (fs);
		if (key[0] != '/'){
			if (*fs->mnt_fsname == '/' &&
			    strcmp(fs->mnt_fsname + 1, key) == 0)
				return (fs);
			if (*fs->mnt_dir == '/' &&
			    strcmp(fs->mnt_dir + 1, key) == 0)
				return (fs);
		}
	}
	return (0);
}

/*
 *	Tell the operator what to do
 */
void
lastdump(int arg)	/* w ==> just what to do; W ==> most recent dumps */
{
	char	*lastname;
	char	*date;
	int	i;
	time_t	tnow;
	struct	mntent	*dt;
	int	dumpme;
	struct	idates	*itwalk;
	int first = 0;

	time(&tnow);
	getfstab();		/* /etc/fstab input */
	inititimes();		/* /etc/dumpdates input */
	if (idatev == NULL)	/* No file /etc/dumpdates */
		return;
	qsort(idatev, nidates, sizeof(struct idates *), idatesort);

	lastname = "??";
	ITITERATE(i, itwalk){
		if (strncmp(lastname, itwalk->id_name, sizeof(itwalk->id_name)) == 0)
			continue;
		date = (char *)ctime(&itwalk->id_ddate);
		date[16] = '\0';		/* blast away seconds and year */
		lastname = itwalk->id_name;
		dt = fstabsearch(itwalk->id_name);
		dumpme = (  (dt != 0)
			 && (dt->mnt_freq != 0)
			 && (itwalk->id_ddate < tnow - (dt->mnt_freq*DAY)));
		if (arg == 'w' && !dumpme)
			continue;
		if (first++ == 0) {
			if (arg == 'w')
				fprintf(stdout, "Dump these file systems:\n");
			else
				fprintf(stdout, "Last dump(s) done (Dump '>' file systems):\n");
		}
		fprintf(stdout,"%c %8s\t(%6s) Last dump: Level %c, Date %s\n",
			dumpme && (arg != 'w') ? '>' : ' ',
			itwalk->id_name,
			dt ? dt->mnt_dir : "",
			itwalk->id_incno,
			date
		    );
	}
}

static int
idatesort(const void *a1, const void *a2)
{
	struct idates	**p1, **p2;
	int	diff;

	p1 = (struct idates **)a1;
	p2 = (struct idates **)a2;
	diff = strncmp((*p1)->id_name, (*p2)->id_name, sizeof((*p1)->id_name));
	if (diff == 0)
		return ((*p2)->id_ddate - (*p1)->id_ddate);
	else
		return (diff);
}
