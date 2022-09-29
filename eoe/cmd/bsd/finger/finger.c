/*
 * Copyright (c) 1980 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */

#ifndef lint
char copyright[] =
"@(#) Copyright (c) 1980 Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)finger.c	5.8 (Berkeley) 3/13/86";
#endif /* not lint */

/*
 * This is a finger program.  It prints out useful information about users
 * by digging it up from various system files.  It is not very portable
 * because the most useful parts of the information (the full user name,
 * office, and phone numbers) are all stored in the VAX-unused gecos field
 * of /etc/passwd, which, unfortunately, other UNIXes use for other things.
 *
 * There are three output formats, all of which give login name, teletype
 * line number, and login time.  The short output format is reminiscent
 * of finger on ITS, and gives one line of information per user containing
 * in addition to the minimum basic requirements (MBR), the full name of
 * the user, his idle time and office location and phone number.  The
 * quick style output is UNIX who-like, giving only name, teletype and
 * login time.  Finally, the long style output give the same information
 * as the short (in more legible format), the home directory and shell
 * of the user, and, if it exits, a copy of the file .plan in the users
 * home directory.  Finger may be called with or without a list of people
 * to finger -- if no list is given, all the people currently logged in
 * are fingered.
 *
 * The program is validly called by one of the following:
 *
 *	finger			{short form list of users}
 *	finger -l		{long form list of users}
 *	finger -b		{briefer long form list of users}
 *	finger -q		{quick list of users}
 *	finger -i		{quick list of users with idle times}
 *	finger namelist		{long format list of specified users}
 *	finger -s namelist	{short format list of specified users}
 *	finger -w namelist	{narrow short format list of specified users}
 *
 * where 'namelist' is a list of users login names.
 * The other options can all be given after one '-', or each can have its
 * own '-'.  The -f option disables the printing of headers for short and
 * quick outputs.  The -b option briefens long format outputs.  The -p
 * option turns off plans for long format outputs.
 */

#include <sys/types.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <lastlog.h>
#include <limits.h>
#include <ndbm.h>
#include <netdb.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#define	utmp	sysv_utmp
#include <utmpx.h>
#undef utmp
#include <netinet/in.h>
#include <rpcsvc/ypclnt.h>	/* for direct NIS access */
#include <sys/signal.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>

#define ASTERISK	'*'		/* ignore this in real name */
#define COMMA		','		/* separator in pw_gecos field */
#define COMMAND		'-'		/* command line flag char */
#define CORY		'C'		/* cory hall office */
#define EVANS		'E'		/* evans hall office */
#define SAMENAME	'&'		/* repeat login name in real name */
#define TALKABLE	0220		/* tty is writable if 220 mode */

/*
 * Safe versions of malloc and strdup.
 */
void	*xmalloc(size_t);
char	*xstrdup(const char *);

/*
 * Special handling for sgi extension of struct passwd
 */
#define	passwd	xpasswd

/*
 * Special passwd-scanning routines.
 */
struct passwd *getpwdnam(char *);
struct passwd *getpwdent(void);

/*
 * 4.3BSD utmp structure and scanning routines.
 */
#define LMAX	8
#define NMAX	8
#define HMAX	(sizeof ((struct utmpx *) 0)->ut_host)

struct utmp {
	char	ut_line[LMAX];		/* tty name */
	char	ut_name[NMAX];		/* user id */
	char	ut_host[HMAX];		/* host name, if remote */
	long	ut_time;		/* time on */
};

#define	setutmpent()	setutxent()

int	getutmpent(struct utmp *);
void	endutmpent(void);

static char	prifield[] = "pri=";
#define prifldlen	(sizeof(prifield)-1)
#define has_pri_field(str)	(!strncmp(prifield, str, prifldlen))

struct person {			/* one for each person fingered */
	char *name;			/* name */
	char tty[LMAX+1];		/* null terminated tty line */
	char host[HMAX+1];		/* null terminated remote host name */
	long loginat;			/* time of (last) login */
	char *mailalias;		/* mail alias right-hand side */
	long idletime;			/* how long idle (if logged in) */
	char *realname;			/* pointer to full name */
	char *office;			/* pointer to office name */
	char *officephone;		/* pointer to office phone no. */
	char *homephone;		/* pointer to home phone no. */
	char *random;			/* for any random stuff in pw_gecos */
	struct passwd *pwd;		/* structure of /etc/passwd stuff */
	char loginok;			/* whether (last) login succeeded */
	char loggedin;			/* person is logged in */
	char writable;			/* tty is writable */
	char original;			/* this is not a duplicate entry */
	struct person *link;		/* link to next person */
	struct person *dups;		/* duplicates of this person */
};

char LASTLOG[] = "/var/adm/lastlog";	/* last login info */
char PLAN[] = "/.plan";			/* what plan file is */
char PROJ[] = "/.project";		/* what project file */

int secure = 0;				/* -S option default */
int localonly = 0;			/* -L option default */
int unbrief = 1;			/* -b option default */
int header = 1;				/* -f option default */
int hack = 1;				/* -h option default */
int idle = 0;				/* -i option default */
int large = 0;				/* -l option default */
int match = 1;				/* -m option default */
int plan = 1;				/* -p option default */
int unquick = 1;			/* -q option default */
int small = 0;				/* -s option default */
int wide = 1;				/* -w option default */

int unshort;
struct person *person1;			/* list of people */
long tloc;				/* current time */

struct passwd *pwdcopy();

static void doall(void);
static void donames(char **);
static void personprint(struct person *);
static void decode(struct person *);
static void findmailalias(struct person *);
static void findidle(struct person *);
static void findwhen(struct person *);
static void stimeprint(long *);
static void ltimeprint(char *, long *, char *);
static void print(void);
static void shortprint(struct person *);
static void quickprint(struct person *);


/* ARGSUSED */
main(argc, argv)
	int argc;
	register char **argv;
{
	register char *s;

	/* parse command line for (optional) arguments */
	while (*++argv && **argv == COMMAND) {
		for (s = *argv + 1; *s; s++) {
			switch (*s) {
			case 'S':
				secure = 1;
				break;
			case 'L':
				localonly = 1;
				break;
			case 'b':
				unbrief = 0;
				break;
			case 'f':
				header = 0;
				break;
			case 'h':
				hack = 0;
				break;
			case 'i':
				idle = 1;
				unquick = 0;
				break;
			case 'l':
				large = 1;
				break;
			case 'm':
				match = 0;
				break;
			case 'p':
				plan = 0;
				break;
			case 'q':
				unquick = 0;
				break;
			case 's':
				small = 1;
				break;
			case 'w':
				wide = 0;
				break;
			default:
				fprintf(stderr, "Usage: finger [-bfhilmpqsw] [login1 [login2 ...] ]\n");
				exit(1);
			}
		}
	}
	if (unquick || idle)
		time(&tloc);
	/*
	 * *argv == 0 means no names given
	 */
	if (*argv == 0)
		doall();
	else
		donames(argv);
	if (person1)
		print();
	exit(0);
}

static void
doall(void)
{
	struct utmp user;
	register struct person *p, *op;
	register struct passwd *pw;
	char name[NMAX + 1];

	if (secure)
		return;
	unshort = large;
	setutmpent();
	if (unquick) {
		extern int _pw_stayopen, _getpwent_no_shadow;

		setpwent();
		_pw_stayopen = 1;
		_getpwent_no_shadow = 1;
	}
	while (getutmpent(&user)) {
		if (user.ut_name[0] == 0)
			continue;
		if (person1 == 0)
			p = person1 = (struct person *) xmalloc(sizeof *p);
		else {
			p->link = (struct person *) xmalloc(sizeof *p);
			p = p->link;
		}
		bcopy(user.ut_name, name, NMAX);
		name[NMAX] = 0;
		bcopy(user.ut_line, p->tty, LMAX);
		p->tty[LMAX] = 0;
		bcopy(user.ut_host, p->host, HMAX);
		p->host[HMAX] = 0;
		p->loginat = user.ut_time;
		p->loginok = 1;
		p->pwd = 0;
		p->loggedin = 1;
		if (unquick && (pw = getpwdnam(name))) {
			p->pwd = pwdcopy(pw);
			decode(p);
			p->name = p->pwd->pw_name;
		} else
			p->name = xstrdup(name);
		for (op = person1; ; op = op->link) {
			if (op == p) {
				p->original = 1;
				p->dups = 0;
				break;
			}
			if (!strcmp(op->name, p->name)) {
				p->original = 0;
				p->dups = op->dups;
				op->dups = p;
				break;
			}
		}
	}
	if (unquick) {
		endpwent();
	}
	endutmpent();
	if (person1 == 0) {
		printf("No one logged on\n");
		return;
	}
	p->link = 0;
}

static void
donames(char **argv)
{
	register struct person *p;
	register struct passwd *pw;
	struct utmp user;

	/*
	 * get names from command line and check to see if they're
	 * logged in
	 */
	unshort = !small;
	for (; *argv != 0; argv++) {
		if (netfinger(*argv))
			continue;
		if (person1 == 0)
			p = person1 = (struct person *) xmalloc(sizeof *p);
		else {
			p->link = (struct person *) xmalloc(sizeof *p);
			p = p->link;
		}
		p->name = *argv;
		p->loggedin = 0;
		p->original = 1;
		p->dups = 0;
		p->pwd = 0;
	}
	if (person1 == 0)
		return;
	p->link = 0;
	/*
	 * if we are doing it, read /etc/passwd for the useful info
	 */
	if (unquick) {
		setpwent();
		if (!match) {
			extern int _pw_stayopen, _getpwent_no_shadow;

			_pw_stayopen = 1;
			_getpwent_no_shadow = 1;
			for (p = person1; p != 0; p = p->link)
				if (pw = getpwdnam(p->name))
					p->pwd = pwdcopy(pw);
		} else while ((pw = getpwdent()) != 0) {
			for (p = person1; p != 0; p = p->link) {
				if (p->original == 2)
					continue;
				if (strcmp(p->name, pw->pw_name) != 0 &&
				    !matchcmp(pw->pw_gecos,pw->pw_name,p->name))
					continue;
				if (p->pwd == 0) {
					/* p->original == 1 */
					p->pwd = pwdcopy(pw);
				} else {
					struct person *new;

					if (pw->pw_origin != _PW_LOCAL) {
					  struct person *p;
					  for (p = person1; p != 0; p = p->link)
					    if (p->pwd &&
						p->pwd->pw_origin!=_PW_LOCAL &&
						p->pwd->pw_uid == pw->pw_uid)
					      break;
					  if (p)
					    /* we already have a YP entry for
					     * this UID, so don't create
					     * another
					     */
					    continue;
					}

					/*
					 * handle multiple login names, insert
					 * new "duplicate" entry behind
					 */
					new = (struct person *)
						xmalloc(sizeof *new);
					new->pwd = pwdcopy(pw);
					new->name = p->name;
					new->original = 1;
					new->dups = 0;
					new->loggedin = 0;
					new->link = p->link;
					p->original = 2;
					p->link = new;
					p = new;
				}
			}
		}
		endpwent();
	}
	/*
	 * Now get login information
	 */
	setutmpent();
	while (getutmpent(&user)) {
		if (*user.ut_name == 0)
			continue;
		for (p = person1; p != 0; p = p->link) {
			if (p->loggedin == 2)
				continue;
			if (strncmp(p->pwd ? p->pwd->pw_name : p->name,
				    user.ut_name, NMAX) != 0)
				continue;
			if (p->loggedin == 0) {
				bcopy(user.ut_line, p->tty, LMAX);
				p->tty[LMAX] = 0;
				bcopy(user.ut_host, p->host, HMAX);
				p->host[HMAX] = 0;
				p->loginat = user.ut_time;
				p->loginok = 1;
				p->loggedin = 1;
			} else {	/* p->loggedin == 1 */
				struct person *new;
				new = (struct person *) xmalloc(sizeof *new);
				new->name = p->name;
				bcopy(user.ut_line, new->tty, LMAX);
				new->tty[LMAX] = 0;
				bcopy(user.ut_host, new->host, HMAX);
				new->host[HMAX] = 0;
				new->loginat = user.ut_time;
				new->loginok = 1;
				new->pwd = p->pwd;
				new->loggedin = 1;
				new->original = 0;
				new->dups = p->dups;
				p->dups = new;
				new->link = p->link;
				p->loggedin = 2;
				p->link = new;
				p = new;
			}
		}
	}
	endutmpent();
	if (unquick) {
		for (p = person1; p != 0; p = p->link)
			decode(p);
	}
}

char *short_header[] = {
	"Login     TTY Idle When       Office\n",
	"Login    Name			TTY Idle When       Office\n"
};

static void
print(void)
{
	register FILE *fp;
	register struct person *p;
	register char *s;
	register int c;
	struct stat statbuf;

	/*
	 * print out what we got
	 */
	if (header) {
		if (unquick) {
			if (!unshort)
				printf(short_header[wide]);
		} else {
			printf("Login      TTY      When      ");
			if (idle)
				printf("	 Idle");
			putchar('\n');
		}
	}
	for (p = person1; p != 0; p = p->link) {
		if (!unquick) {
			quickprint(p);
			continue;
		}
		if (!unshort) {
			shortprint(p);
			continue;
		}
		if (!p->original)
			continue;
		if (p != person1)
			putchar('\n');
		personprint(p);
		if (p->pwd != 0) {
			if (hack) {
				s = xmalloc(strlen(p->pwd->pw_dir) +
					sizeof PROJ);
				strcpy(s, p->pwd->pw_dir);
				strcat(s, PROJ);
				/*
				 * Do not print ~/.project if it is a symbolic
				 * link, to prevent network games.
				 */
				if (lstat(s, &statbuf) == 0
				    && !S_ISLNK(statbuf.st_mode)
				    && (fp = fopen(s, "r")) != 0) {
					printf("Project: ");
					while ((c = getc(fp)) != EOF
					       && c != '\n') {
					    putchar((isprint(c) || isspace(c))
						    ? c : '.');
					}
					fclose(fp);
					putchar('\n');
				}
				free(s);
			}
			if (plan) {
				s = xmalloc(strlen(p->pwd->pw_dir) +
					sizeof PLAN);
				strcpy(s, p->pwd->pw_dir);
				strcat(s, PLAN);

				/*
				 * Do not print ~/.plan if it is a symbolic
				 * link, to prevent network games.
				 */
				if (lstat(s, &statbuf) < 0
				    || S_ISLNK(statbuf.st_mode)
				    || (fp = fopen(s, "r")) == 0) {
					printf("No Plan.\n");
				} else {
					printf("Plan:\n");
					while ((c = getc(fp)) != EOF) {
					    putchar((isprint(c) || isspace(c))
						    ? c : '.');
					}
					fclose(fp);
				}
				free(s);
			}
		}
	}
}

/*
 * Duplicate a pwd entry.
 * Note: Only the useful things (what the program currently uses) are copied.
 */
struct passwd *
pwdcopy(pfrom)
	register struct passwd *pfrom;
{
	register struct passwd *pto;

	pto = (struct passwd *) xmalloc(sizeof *pto);
	pto->pw_name = xstrdup(pfrom->pw_name);
	pto->pw_uid = pfrom->pw_uid;
	pto->pw_gecos = xstrdup(pfrom->pw_gecos);
	pto->pw_dir = xstrdup(pfrom->pw_dir);
	pto->pw_shell = xstrdup(pfrom->pw_shell);
	pto->pw_origin = pfrom->pw_origin;
	return pto;
}

/*
 * print out information on quick format giving just name, tty, login time
 * and idle time if idle is set.
 */
static void
quickprint(register struct person *pers)
{
	printf("%-*.*s  ", NMAX, NMAX, pers->name);
	if (pers->loggedin) {
		if (idle) {
			findidle(pers);
			printf("%c%-*s %-16.16s", pers->writable ? ' ' : '*',
				LMAX, pers->tty, ctime(&pers->loginat));
			ltimeprint("   ", &pers->idletime, "");
		} else
			printf(" %-*s %-16.16s", LMAX,
				pers->tty, ctime(&pers->loginat));
		putchar('\n');
	} else
		printf("	  Not Logged In\n");
}

/*
 * print out information in short format, giving login name, full name,
 * tty, idle time, login time, office location and phone.
 */
static void
shortprint(register struct person *pers)
{
	char *p;
	char dialup;

	if (pers->pwd == 0) {
		printf("???      %s\n", pers->name);
		return;
	}
	printf("%-*s", NMAX, pers->pwd->pw_name);
	dialup = 0;
	if (wide)
		printf(" %-20.20s", (pers->realname) ? pers->realname : "???");
	putchar(' ');
	if (pers->loggedin && !pers->writable)
		putchar('*');
	else
		putchar(' ');
	if (*pers->tty) {
		if (pers->tty[0] == 't' && pers->tty[1] == 't' &&
		    pers->tty[2] == 'y') {
			if ((pers->tty[3] == 'm' || pers->tty[3] == 'f')
			    && pers->loggedin)
				dialup = 1;
			printf("%-3.3s ", pers->tty + 3);
		} else
			printf("%-3.3s ", pers->tty);
	} else
		printf("    ");
	p = ctime(&pers->loginat);
	if (pers->loggedin) {
		stimeprint(&pers->idletime);
		printf(" %3.3s %-5.5s ", p, p + 11);
	} else if (pers->loginat == 0)
		printf("< .  .  .  . > " );
	else if (tloc - pers->loginat >= 180 * 24 * 60 * 60)
		printf("<%-6.6s, %-4.4s> ", p + 4, p + 20);
	else
		printf("<%-12.12s> ", p + 4);
	if (dialup && pers->homephone)
		printf(" %20s", pers->homephone);
	else {
		if (pers->office)
			printf(" %-11.11s", pers->office);
		else if (pers->officephone || pers->homephone)
			printf("	    ");
		if (pers->officephone)
			printf(" %s", pers->officephone);
		else if (pers->homephone)
			printf(" %s", pers->homephone);
	}
	putchar('\n');
}

/*
 * print out a person in long format giving all possible information.
 * directory and shell are inhibited if unbrief is clear.
 */
static void
personprint(register struct person *pers)
{
	if (pers->pwd == 0) {
		printf("Login name: %-10s\t\t\tIn real life: ???\n",
			pers->name);
		return;
	}
	printf("Login name: %-10s", pers->pwd->pw_name);
	if (pers->loggedin && !pers->writable)
		printf("	(messages off)	");
	else
		printf("			");
	if (pers->realname)
		printf("In real life: %s", pers->realname);
	if (pers->office) {
		int col;

		col = printf("\nOffice: %-.11s", pers->office) - 1;
		if (pers->officephone)
			col += printf(", %s", pers->officephone);
		col = 5 - col / 8;
		while (--col >= 0)
			putchar('\t');
		if (pers->homephone)
			printf("Home phone: %s", pers->homephone);
		else if (pers->random)
			printf("%s", pers->random);
	} else if (pers->officephone) {
		printf("\nPhone: %s", pers->officephone);
		if (pers->homephone)
			printf(", %s", pers->homephone);
		if (pers->random)
			printf(", %s", pers->random);
	} else if (pers->homephone) {
		printf("\nPhone: %s", pers->homephone);
		if (pers->random)
			printf(", %s", pers->random);
	} else if (pers->random)
		printf("\n%s", pers->random);
	if (unbrief && pers->pwd->pw_origin != _PW_YP_REMOTE && !secure) {
		printf("\nDirectory: %-25s", pers->pwd->pw_dir);
		if (*pers->pwd->pw_shell)
			printf("\tShell: %-s", pers->pwd->pw_shell);
	}
	if (pers->mailalias) {
		printf("\nMail to %s goes to %s",
			pers->pwd->pw_name, pers->mailalias);
	}
	if (pers->pwd->pw_origin == _PW_YP_REMOTE || secure) {
		putchar('\n');
		return;
	}
	while (pers) {
		if (pers->loggedin) {
			register char *ep = ctime(&pers->loginat);
			if (*pers->host) {
				printf("\nOn since %15.15s on %s from %s",
					&ep[4], pers->tty, pers->host);
				ltimeprint("\n", &pers->idletime, " Idle Time");
			} else {
				printf("\nOn since %15.15s on %-*s",
					&ep[4], LMAX, pers->tty);
				ltimeprint("\t", &pers->idletime, " Idle Time");
			}
		} else if (pers->loginat == 0) {
			printf("\nNever logged in.");
		} else {
			register char *ep = ctime(&pers->loginat);
			printf("\n%s at ", pers->loginok ? "Last login" :
				"Failed login attempt");
			if (tloc - pers->loginat > 180 * 24 * 60 * 60)
				printf("%10.10s, %4.4s", ep, ep+20);
			else
				printf("%16.16s", ep);
			if (*pers->host)
				printf(" from %s@%s", pers->tty, pers->host);
			else
				printf(" on %s", pers->tty);
		}
		pers = pers->dups;
	}
	putchar('\n');
}

/*
 *  very hacky section of code to format phone numbers.  filled with
 *  magic constants like 4, 7 and 10.
 */
char *
phone(s, len, alldigits)
	register char *s;
	int len;
	char alldigits;
{
	char fonebuf[15];
	register char *p = fonebuf;
	register i;

	if (!alldigits)
		return (strcpy(xmalloc(len + 1), s));
	switch (len) {
	case 2:
	case 3:
	case 4:
		*p++ = ' ';
		*p++ = 'x';
		for (i = 0; i < len; i++)
			*p++ = *s++;
		break;
	case 5:
		*p++ = ' ';
		*p++ = 'x';
		*p++ = *s++;
		*p++ = '-';
		for (i = 0; i < 4; i++)
			*p++ = *s++;
		break;
	case 7:
		for (i = 0; i < 3; i++)
			*p++ = *s++;
		*p++ = '-';
		for (i = 0; i < 4; i++)
			*p++ = *s++;
		break;
	case 10:
		for (i = 0; i < 3; i++)
			*p++ = *s++;
		*p++ = '-';
		for (i = 0; i < 3; i++)
			*p++ = *s++;
		*p++ = '-';
		for (i = 0; i < 4; i++)
			*p++ = *s++;
		break;
	case 11:	/* e.g., 960-1980 x1234 */
		for (i = 0; i < 3; i++)
			*p++ = *s++;
		*p++ = '-';
		for (i = 0; i < 4; i++)
			*p++ = *s++;
		*p++ = ' ';
		*p++ = 'x';
		for (i = 0; i < 4; i++)
			*p++ = *s++;
		break;
	case 13:	/* e.g., 415-960-1980 x123 */
	case 14:	/* e.g., 415-960-1980 x1234 */
		for (i = 0; i < 3; i++)
			*p++ = *s++;
		*p++ = '-';
		for (i = 0; i < 3; i++)
			*p++ = *s++;
		*p++ = '-';
		for (i = 0; i < 4; i++)
			*p++ = *s++;
		*p++ = ' ';
		*p++ = 'x';
		for (i = 0; i < (len - 10); i++)
			*p++ = *s++;
		break;
	case 0:
		return 0;
	default:
		return (strcpy(xmalloc(len + 1), s));
	}
	*p++ = 0;
	return (strcpy(xmalloc(p - fonebuf), fonebuf));
}

/*
 * decode the information in the gecos field of /etc/passwd
 */
static void
decode(register struct person *pers)
{
	char buffer[256];
	register char *bp, *gp, *lp;
	int alldigits;
	int len;

	pers->realname = 0;
	pers->office = 0;
	pers->officephone = 0;
	pers->homephone = 0;
	pers->random = 0;
	pers->mailalias = 0;
	if (pers->pwd == 0)
		return;
	findmailalias(pers);
	gp = pers->pwd->pw_gecos;
	bp = buffer;
	if (*gp == ASTERISK)
		gp++;
	if (has_pri_field(gp)) {	/* Skip leading pri= field */
		gp += prifldlen;
		if (*gp == '-')
			gp++;
		while (isdigit(*gp))
			gp++;
	}
	while (*gp && *gp != COMMA)			/* name */
		if (*gp == SAMENAME) {
			lp = pers->pwd->pw_name;
			if (islower(*lp))
				*bp++ = toupper(*lp++);
			while (*bp++ = *lp++)
				;
			bp--;
			gp++;
		} else
			*bp++ = *gp++;
	*bp++ = 0;
	if ((len = bp - buffer) > 1)
		pers->realname = strcpy(xmalloc(len), buffer);
	if (*gp == COMMA) {				/* office */
		gp++;
		bp = buffer;
		while (*gp && *gp != COMMA) {
			*bp = *gp++;
			if (bp < buffer + sizeof buffer)
				bp++;
		}
		*bp = 0;
		len = bp - buffer;
		bp--;			/* point to last character */
			len++;
		if (len > 1)
			pers->office = strcpy(xmalloc(len), buffer);
	}
	if (*gp == COMMA) {				/* office phone */
		gp++;
		bp = buffer;
		alldigits = 1;
		while (*gp && *gp != COMMA) {
			*bp = *gp++;
			if (!isdigit(*bp))
				alldigits = 0;
			if (bp < buffer + sizeof buffer - 1)
				bp++;
		}
		*bp = 0;
		pers->officephone = phone(buffer, bp - buffer, alldigits);
	}
	if (*gp == COMMA) {				/* home phone */
		gp++;
		bp = buffer;
		alldigits = 1;
		while (*gp && *gp != COMMA) {
			*bp = *gp++;
			if (!isdigit(*bp))
				alldigits = 0;
			if (bp < buffer + sizeof buffer - 1)
				bp++;
		}
		*bp = 0;
		pers->homephone = phone(buffer, bp - buffer, alldigits);
	}
	if (pers->loggedin)
		findidle(pers);
	else
		findwhen(pers);
}

/*
 * Look for a mail alias, either local or NIS.
 */
static void
findmailalias(register struct person *pers)
{
	datum key, val;
	int dsize;
	static DBM *adb;
	static int useyp;
	static datum ypok = {"+", 2};	/* alias name that enables NIS */

	if (adb == NULL) {
		adb = dbm_open("/usr/lib/aliases", O_RDONLY, 0);
		if (adb == NULL)
			return;
		val = dbm_fetch(adb, ypok);
		useyp = val.dptr != NULL
			&& val.dsize == ypok.dsize
			&& !bcmp(val.dptr, ypok.dptr, ypok.dsize)
			&& _yellowup(0);
	}

	key.dptr = pers->pwd->pw_name;
	key.dsize = strlen(pers->pwd->pw_name) + 1;
	val = dbm_fetch(adb, key);
	dsize = val.dsize;
	if (val.dptr == NULL
	    && !(dsize == 1 && *(char *)(key.dptr) == '@')
	    && useyp) {
		/*
		 * No local alias, and not the obsolescent special key, @
		 * (used to interlock aliases database creation), and there
		 * is a +:+ local alias, so try NIS.
		 */
		(void) yp_match(_yp_domain, "mail.aliases", key.dptr,dsize,
				(char **)&val.dptr, &dsize);
	}
	if (val.dptr != NULL) {
		while (isspace(*(char *)(val.dptr))) {
			--dsize;
			val.dptr = (char *)val.dptr + 1;
		}
		pers->mailalias = strcpy(xmalloc(dsize+1), val.dptr);
	}
}

/*
 * find the last log in of a user by checking the LASTLOG file.
 * the entry is indexed by the uid, so this can only be done if
 * the uid is known (which it isn't in quick mode)
 */
static void findwhen(register struct person *pers)
{
	char path[PATH_MAX];
	int lf, cc;
	struct lastlog ll;

	sprintf(path, "%s/%s", LASTLOG, pers->pwd->pw_name);
	lf = open(path, O_RDONLY);
	if (lf >= 0) {
		if ((cc = read(lf, (char *)&ll, sizeof ll)) == sizeof ll) {
			bcopy(ll.ll_line, pers->tty, LMAX);
			pers->tty[LMAX] = 0;
			bcopy(ll.ll_host, pers->host, HMAX);
			pers->host[HMAX] = 0;
			pers->loginat = ll.ll_time;
			pers->loginok = (ll.ll_time ? 1 : 0 );
		} else {
			if (cc != 0) {
				fprintf(stderr, "finger: %s format error\n",
					path);
			}
			pers->tty[0] = 0;
			pers->host[0] = 0;
			pers->loginat = 0L;
		}
		close(lf);
	} else {
		pers->tty[0] = 0;
		pers->host[0] = 0;
		pers->loginat = 0L;
	}
}


/*
 * find the idle time of a user by doing a stat on /dev/tty??,
 * where tty?? has been gotten from USERLOG, supposedly.
 */
static void
findidle(register struct person *pers)
{
	struct stat ttystatus;
	static char buffer[20] = "/dev/";
	long t;
#define TTYLEN 5

	strcpy(buffer + TTYLEN, pers->tty);
	buffer[TTYLEN+LMAX] = 0;
	if (stat(buffer, &ttystatus) < 0) {
		pers->idletime = 0L;
		pers->writable = 0;
	} else {
		time(&t);
		if (t < ttystatus.st_atime)
			pers->idletime = 0L;
		else
			pers->idletime = t - ttystatus.st_atime;
		pers->writable = (ttystatus.st_mode & TALKABLE) == TALKABLE;
	}
}

/*
 * print idle time in short format; this program always prints 4 characters;
 * if the idle time is zero, it prints 4 blanks.
 */
static void
stimeprint(long *dt)
{
	register struct tm *delta;

	delta = gmtime(dt);
	if (delta->tm_yday == 0)
		if (delta->tm_hour == 0)
			if (delta->tm_min == 0)
				printf("    ");
			else
				printf("  %2d", delta->tm_min);
		else
			if (delta->tm_hour >= 10)
				printf("%3d:", delta->tm_hour);
			else
				printf("%1d:%02d",
					delta->tm_hour, delta->tm_min);
	else
		printf("%3dd", delta->tm_yday);
}

/*
 * print idle time in long format with care being taken not to pluralize
 * 1 minutes or 1 hours or 1 days.
 * print "prefix" first.
 */
static void
ltimeprint(char *before, long *dt, char *after)
{
	register struct tm *delta;

	delta = gmtime(dt);
	if (delta->tm_yday == 0 && delta->tm_hour == 0 && delta->tm_min == 0 &&
	    delta->tm_sec <= 10)
		return;
	printf("%s", before);
	if (delta->tm_yday >= 10)
		printf("%d days", delta->tm_yday);
	else if (delta->tm_yday > 0)
		printf("%d day%s %d hour%s",
			delta->tm_yday, delta->tm_yday == 1 ? "" : "s",
			delta->tm_hour, delta->tm_hour == 1 ? "" : "s");
	else
		if (delta->tm_hour >= 10)
			printf("%d hours", delta->tm_hour);
		else if (delta->tm_hour > 0)
			printf("%d hour%s %d minute%s",
				delta->tm_hour, delta->tm_hour == 1 ? "" : "s",
				delta->tm_min, delta->tm_min == 1 ? "" : "s");
		else
			if (delta->tm_min >= 10)
				printf("%2d minutes", delta->tm_min);
			else if (delta->tm_min == 0)
				printf("%2d seconds", delta->tm_sec);
			else
				printf("%d minute%s %d second%s",
					delta->tm_min,
					delta->tm_min == 1 ? "" : "s",
					delta->tm_sec,
					delta->tm_sec == 1 ? "" : "s");
	printf("%s", after);
}

matchcmp(gname, login, given)
	register char *gname;
	char *login;
	char *given;
{
#define	BUFLEN	255
	char buf[BUFLEN+1];
	register char *bp, *lp;
	register int c;

	if (*gname == ASTERISK)
		gname++;
	if ((bp = strchr(gname, COMMA)) != NULL)
		*bp = '\0';
	c = namecmp(gname, given)
	    || toupper(gname[0]) == toupper(given[0]) && given[1] == '.'
	    && ((lp = strrchr(gname, '.')) || (lp = strrchr(gname, ' ')))
	    && namecmp(lp+1, given+2);
	if (bp)
		*bp = COMMA;
	if (c)
		return 1;
	bp = buf;
	for (;;)
		switch (c = *gname++) {
		case SAMENAME:
			for (lp = login; bp < buf + BUFLEN; bp++, lp++) {
				if ((*bp = *lp) == 0)
					break;
			}
			break;
		case ' ':
		case '.':
		case COMMA:
		case '\0':
			*bp = 0;
			if (namecmp(buf, given))
				return (1);
			if (c == COMMA || c == '\0')
				return (0);
			bp = buf;
			break;
		default:
			if (bp < buf + BUFLEN)
				*bp++ = c;
		}
	/*NOTREACHED*/
}

namecmp(name1, name2)
	register char *name1, *name2;
{
	register char c1, c2;

	for (;;) {
		while ((c1 = *name1) != '\0' && isspace(c1) || c1 == '.')
			name1++;
		while ((c2 = *name2) != '\0' && isspace(c2) || c2 == '.')
			name2++;
		c1 = *name1++;
		if (islower(c1))
			c1 = toupper(c1);
		c2 = *name2++;
		if (islower(c2))
			c2 = toupper(c2);
		if (c1 != c2)
			break;
		if (c1 == 0)
			return (1);
	}

/*
 * Customer called in a bug on this - XXX why is it here?
 */
#if 0
	if (!c1) {
		for (name2--; isdigit(*name2); name2++)
			;
		if (*name2 == 0)
			return (1);
	} else if (!c2) {
		for (name1--; isdigit(*name1); name1++)
			;
		if (*name2 == 0)
			return (1);
	}
#endif
	return (0);
}

netfinger(name)
	char *name;
{
	char *host;
	struct hostent *hp;
	struct servent *sp;
	struct sockaddr_in sin;
	int s, len;
	register FILE *f;
	register int c;
	register int lastc;

	if (secure || localonly)
		return (0);	/* disallow multi-hop fingers */
	if (name == NULL)
		return (0);
	host = rindex(name, '@');
	if (host == NULL)
		return (0);
	*host++ = 0;
	hp = gethostbyname(host);
	if (hp == NULL) {
		static struct hostent def;
		static struct in_addr defaddr;
		static char *alist[1];
		static char namebuf[128];
		int inet_addr();

		defaddr.s_addr = inet_addr(host);
		if (defaddr.s_addr == -1) {
			printf("unknown host: %s\n", host);
			return (1);
		}
		strcpy(namebuf, host);
		def.h_name = namebuf;
		def.h_addr_list = alist, def.h_addr = (char *)&defaddr;
		def.h_length = sizeof (struct in_addr);
		def.h_addrtype = AF_INET;
		def.h_aliases = 0;
		hp = &def;
	}
	printf("[%s]", hp->h_name);
	sp = getservbyname("finger", "tcp");
	if (sp == 0) {
		fprintf(stderr, "finger: unknown tcp service\n");
		return (1);
	}
	sin.sin_family = hp->h_addrtype;
	bcopy(hp->h_addr, (char *)&sin.sin_addr, hp->h_length);
	sin.sin_port = sp->s_port;
	s = socket(hp->h_addrtype, SOCK_STREAM, 0);
	if (s < 0) {
		fprintf(stderr, "finger: %s\n", strerror(errno));
		return (1);
	}
	if (connect(s, (char *)&sin, sizeof (sin)) < 0) {
		fprintf(stderr, "finger: %s: %s\n",
			hp->h_name, strerror(errno));
		close(s);
		return (1);
	}
	printf("\n");
	if (large) write(s, "/W ", 3);

	/* 
	** deal with @@@@@host
	*/
	while (name[0] == '@' && name[1] == '@') {
		name++;
	}

	len = strlen(name);
	if (len > 0)
		write(s, name, len);
	write(s, "\r\n", 2);
	f = fdopen(s, "r");
	while ((c = getc(f)) != EOF) {
		switch(c) {
		case 0210:
		case 0211:
		case 0212:
		case 0214:
			c -= 0200;
			break;
		case 0215:
			c = '\n';
			break;
		}
		lastc = c;
		if (isprint(c) || isspace(c))
			putchar(c);
		else
			putchar('.');
	}
	if (lastc != '\n')
		putchar('\n');
	(void)fclose(f);
	return (1);
}

void *
xmalloc(size_t n)
{
	void *p;

	p = malloc(n);
	if (p == NULL) {
		fprintf(stderr, "finger: Out of memory\n");
		exit(3);
	}
	return p;
}

char *
xstrdup(const char *s)
{
	return strcpy(xmalloc(strlen(s)+1), s);
}

struct passwd *
getpwdnam(char *name)
{
	struct passwd *pw;
	int vallen;
	char *val;

	pw = (struct xpasswd *)getpwnam(name);
	if (pw)
		return pw;
	if (yp_match(_yp_domain, "passwd.byname", name, strlen(name),
		     &val, &vallen) != 0) {
		return 0;
	}
	pw = _pw_interpret(val, vallen, 0);
	free(val);
	if (pw)
		pw->pw_origin = _PW_YP_REMOTE;
	return pw;
}

struct passwd *
getpwdent()
{
	struct passwd *pw;
	static int inyellow;

	if (!inyellow) {
		pw = (struct xpasswd *)getpwent();
		if (pw)
			return pw;
		inyellow = 1;
	}
	pw = _yp_getpwent();
	if (pw)
		return pw;
	inyellow = 0;
	_yp_setpwent();
	return 0;
}

int
getutmpent(struct utmp *ut)
{
	struct utmpx *utx;

	do {
		utx = getutxent();
		if (utx == 0)
			return 0;
	} while (utx->ut_type != USER_PROCESS);
	strncpy(ut->ut_line, utx->ut_line, LMAX);
	strncpy(ut->ut_name, utx->ut_user, NMAX);
	strncpy(ut->ut_host, utx->ut_host, HMAX);
	ut->ut_time = utx->ut_xtime;

	return 1;
}

void
endutmpent()
{
	endutxent();
}
