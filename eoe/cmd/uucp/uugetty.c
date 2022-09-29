/* (uu)getty - sets up speed, various terminal flags, line discipline,
 *	and waits for new prospective user to enter name, before
 *	calling "login".
 */
#ident	"$Revision: 1.79 $"

#ifndef UUGETTY
#define USAGE "usage: getty [-hN] [-t time] [-s oline] line [speed [terminal [ldisc]]]\n"
#endif
/*	-h says don't hangup by dropping carrier during the
 *		initialization phase.  Normally carrier is dropped to
 *		make the dataswitch release the line.
 *	-N honors /etc/nologin
 *	-t says timeout after the number of seconds in "time" have
 *		elapsed even if nothing is typed.  This is useful
 *		for making sure dialup lines release if someone calls
 *		in and then doesn't actually login in.
 *	-L de	overwrites the environment variable LANG
 *	"line" is the device in "/dev".
 *	"speed" is a pointer into the "/etc/getty_defs" where the
 *		definition for the speeds and other associated flags are
 *		to be found.
 *	"term" is the name of the terminal type.
 *	"ldisc" is the name of the line discipline.
 */


#ifdef UUGETTY
#define USAGE "usage: uugetty [-hNDr] [-t time] [-i dial1[,dial2] [-d secs] line [speed [terminal [ldisc]]]\n"
#endif
/*	-D requests debugging
 *	-r requires that a character other than EOT be received before
 *		continuing.  This cannot be used if two chat scripts
 *		are used with -i.
 *	-i dialer says to initialize the line with the labeled entry in
 *		/etc/uucp/Dialers
 *	-i dialer1,dialer2 says to initialize with the first Dialers entry
 *		and to use the second to wait for "CONNECT".
 *	-d delays a number of seconds after the first character before
 *		believing anything.
 */

/* Usage: getty -c gettydefs_like_file
 *
 *	The "-c" flag is used to have "getty" check a gettydefs file.
 *	"getty" parses the entire file and prints out its findings so
 *	that the user can make sure that the file contains the proper
 *	information.
 */


#ifndef UUGETTY
#define uugetty_ck(line)
#define delock(line)
#define UUCPUID 0

int Debug;

#ifdef	sgi
#include <limits.h>
#define	AUX_LANG			/* LANG handling on */
#endif	/* sgi */

#define		TRUE		1
#define		FALSE		0
#define		SUCCESS		0
#endif /* !UUGETTY */

#define		FAILURE		(-1)
#define		ID		1
#define		IFLAGS		2
#define		FFLAGS		3
#define		MESSAGE		4
#define		NEXTID		5

#define		ACTIVE		1
#define		FINISHED	0


#include	<unistd.h>
#include	<stdlib.h>
#include	<getopt.h>
#include	<sys/types.h>
#include	<sys/stat.h>
#include	<sys/param.h>
#include	<termio.h>
#include	<signal.h>
#include	<fcntl.h>
#include	<utmp.h>
#include	<pwd.h>
#include	<ctype.h>
#include	<string.h>
#include	<bstring.h>
#include	<paths.h>
#include	<syslog.h>
#include	<errno.h>
#include	<stdio.h>
#include	<sys/capability.h>

#ifdef UUGETTY
#include	"uucp.h"
#undef DEBUG
static void uugetty_ck(char *);
#endif


struct Gdef {
	char	*g_id;			/* identification for modes & speeds */
	struct termio	g_iflags;	/* initial terminal flags */
	struct termio	g_fflags;	/* final terminal flags */
	char	*g_message;		/* login message */
	char	*g_nextid;		/* next id if this speed is wrong */
};
static struct Gdef *find_def(char *id);

static void parse(char *, char **, int);

static void badcall(char*, char*, int, int);
static char *getvar(char*, int *);

#define	GOODNAME	1
#define	NONAME		0
#define	BADSPEED	(-1)


#define	MAXIDLENGTH	15	/* Maximum length the "g_id" and "g_nextid"
				 * strings can take.  Longer ones will be
				 * truncated.
				 */
#define OMAXMESS	79	/* old max message length */
#define MAXMESSAGE	512	/* Maximum length the "g_message" string
				 * can be.  Longer ones are truncated.
				 */

/*	Maximum length of line in /etc/gettydefs file and the maximum	*/
/*	length of the user response to the "login" message.		*/

#define OMAXLINE	255				/* original MAXLINE */
#define	MAXLINE		OMAXLINE-OMAXMESS+MAXMESSAGE    /* 688 */
#define	MAXARGS		64	/* Maximum number of arguments that can be
				 * passed to "login"
				 */

struct Symbols {
	char		*s_symbol;	/* Name of symbol */
	unsigned	s_value	;	/* Value of symbol */
};

/*	The following four symbols define the "SANE" state.		*/

#define	ISANE	(BRKINT|IGNPAR|ISTRIP|ICRNL|IXON)
#define	OSANE	(OPOST|ONLCR)
#define	CSANE	(CS8|CREAD)
#ifdef IEXTEN
#define	LSANE	(ISIG|ICANON|ECHO|ECHOE|ECHOK|IEXTEN)
#else
#define	LSANE	(ISIG|ICANON|ECHO|ECHOE|ECHOK)
#endif

/*	Modes set with the TCSETAW ioctl command.			*/

static struct Symbols imodes[] = {
	"IGNBRK",	IGNBRK,
	"BRKINT",	BRKINT,
	"IGNPAR",	IGNPAR,
	"PARMRK",	PARMRK,
	"INPCK",	INPCK,
	"ISTRIP",	ISTRIP,
	"INLCR",	INLCR,
	"IGNCR",	IGNCR,
	"ICRNL",	ICRNL,
	"IUCLC",	IUCLC,
	"IXON",		IXON,
	"IXANY",	IXANY,
	"IXOFF",	IXOFF,
	NULL,		0
};

static struct Symbols omodes[] = {
	"OPOST",	OPOST,
	"OLCUC",	OLCUC,
	"ONLCR",	ONLCR,
	"OCRNL",	OCRNL,
	"ONOCR",	ONOCR,
	"ONLRET",	ONLRET,
	"OFILL",	OFILL,
	"OFDEL",	OFDEL,
	"NLDLY",	NLDLY,
	"NL0",		NL0,
	"NL1",		NL1,
	"CRDLY",	CRDLY,
	"CR0",		CR0,
	"CR1",		CR1,
	"CR2",		CR2,
	"CR3",		CR3,
	"TABDLY",	TABDLY,
	"TAB0",		TAB0,
	"TAB1",		TAB1,
	"TAB2",		TAB2,
	"TAB3",		TAB3,
	"BSDLY",	BSDLY,
	"BS0",		BS0,
	"BS1",		BS1,
	"VTDLY",	VTDLY,
	"VT0",		VT0,
	"VT1",		VT1,
	"FFDLY",	FFDLY,
	"FF0",		FF0,
	"FF1",		FF1,
	NULL,	0
};

static struct Symbols cmodes[] = {
	"CS5",		CS5,
	"CS6",		CS6,
	"CS7",		CS7,
	"CS8",		CS8,
	"CSTOPB",	CSTOPB,
	"CREAD",	CREAD,
	"PARENB",	PARENB,
	"PARODD",	PARODD,
	"HUPCL",	HUPCL,
	"CLOCAL",	CLOCAL,
	NULL,		0
};

/*
 * Speed keywords.  Words of the form B[0-9]+ (B9600, etc.) are compared 
 * in the code.  We don't list them here because there's approximately
 * 2^32 possible values.
 */
static struct Symbols smodes[] = {
	"EXTA",		EXTA,
	"EXTB",		EXTB,
	NULL,		0
};

static struct Symbols lmodes[] = {
	"ISIG",		ISIG,
	"ICANON",	ICANON,
	"XCASE",	XCASE,
	"ECHO",		ECHO,
	"ECHOE",	ECHOE,
	"ECHOK",	ECHOK,
	"ECHONL",	ECHONL,
	"NOFLSH",	NOFLSH,
	NULL,		0
};

/*	Terminal types set with the LDSETT ioctl command.		*/

static struct Symbols terminals[] = {
	"none",		TERM_NONE,
#ifdef	TERM_V10
	"vt100",	TERM_V10,
#endif
#ifdef	TERM_H45
	"hp45",		TERM_H45,
#endif
#ifdef	TERM_C10
	"c100",		TERM_C100,
#endif
#ifdef	TERM_TEX
	"tektronix",	TERM_TEX,
	"tek",		TERM_TEX,
#endif
#ifdef	TERM_D40
	"ds40-1",	TERM_D40,
#endif
#ifdef	TERM_V61
	"vt61",		TERM_V61,
#endif
#ifdef	TERM_TEC
	"tec",		TERM_TEC,
#endif
	NULL,		0
};


static struct Symbols linedisc[] = {
	"LDISC0",	LDISC0,
	"LDISC1",	LDISC1,
	NULL,		0
};

/*	If the /etc/gettydefs file can't be opened, the following	*/
/*	default is used.						*/

static struct Gdef default_gdef = {
	"default",

	{ ICRNL, 0, CREAD+HUPCL,
	    0, B9600, B9600, LDISC1},

	{ ICRNL, OPOST+ONLCR+NLDLY+TAB3, CS7+CREAD+HUPCL,
	    ISIG+ICANON+ECHO+ECHOE+ECHOK, B9600, B9600, LDISC1},

	"LOGIN: ",

	"default",
};


#define ISSUE_FILE	_PATH_ISSUE
#define	GETTY_DEFS	"/etc/gettydefs"

static int  check;
static char *checkgdfile;	/* Name of gettydefs file during check mode. */

static int nologin;		/* wait for /etc/nologin to disappear */

#ifdef	AUX_LANG
static char envlang[NL_LANGMAX];	/* LANG environment variable */
#endif	/* AUX_LANG */

int wait_read;			/* wait for read before output "login" */

char *chat1_str = 0;		/* chat-up the modem before starting */
static char *chat2_str = 0;	/* jabber after "RING" until "CONNECT" */
#define	CHAT_MAX 50
char *chat_args[CHAT_MAX+1];

static int dead_time = 0;	/* delay this many seconds after 1st char */

static char *line_locked = 0;	/* locked device */

/*	"quoted" takes a quoted character, starting at the quote	*/
/*	character, and returns a single character plus the size of	*/
/*	the quote string.  "quoted" recognizes the following as		*/
/*	special, \n,\r,\v,\t,\b,\f as well as the \nnn notation.	*/

static char
quoted(char *ptr,
       int *qsize)
{
	register char c,*rptr;
	register int i;

	rptr = ptr;
	switch(*++rptr) {
	case 'n':
		c = '\n';
		break;
	case 'r':
		c = '\r';
		break;
	case 'v':
		c = '\013';
		break;
	case 'b':
		c = '\b';
		break;
	case 't':
		c = '\t';
		break;
	case 'f':
		c = '\f';
		break;
	default:

/* If this is a numeric string, take up to three characters of */
/* it as the value of the quoted character. */
		if (*rptr >= '0' && *rptr <= '7') {
			for (i=0,c=0; i < 3;i++) {
				c = c*8 + (*rptr - '0');
				if (*++rptr < '0' || *rptr > '7') break;
			}
			rptr--;

/* If the character following the '\\' is a NULL, back up the */
/* ptr so that the NULL won't be missed.  The sequence */
/* backslash null is essentually illegal. */
		} else if (*rptr == '\0') {
			c = '\0';
			rptr--;

/* In all other cases the quoting does nothing. */
		} else c = *rptr;
		break;
	}

/* Compute the size of the quoted character. */
	(*qsize) = rptr - ptr + 1;
	return(c);
}


static char *
getword(register char *ptr,
	int *size)
{
	register char *optr,c;
	static char word[MAXIDLENGTH+1];
	int qsize;

/* Skip over all white spaces including quoted spaces and tabs. */
	for (*size=0; isspace(*ptr) || *ptr == '\\';) {
		if (*ptr == '\\') {
			c = quoted(ptr,&qsize);
			(*size) += qsize;
			ptr += qsize+1;

/* If this quoted character is not a space or a tab or a newline */
/* then break. */
			if (isspace(c) == 0) break;
		} else {
			(*size)++;
			ptr++;
		}
	}

/* Put all characters from here to next white space or '#' or '\0' */
/* into the word, up to the size of the word. */
	for (optr= word,*optr='\0'; isspace(*ptr) == 0 &&
	    *ptr != '\0' && *ptr != '#'; ptr++,(*size)++) {

/* If the character is quoted, analyze it. */
		if (*ptr == '\\') {
			c = quoted(ptr,&qsize);
			(*size) += qsize;
			ptr += qsize;
		} else c = *ptr;

/* If there is room, add this character to the word. */
		if (optr < &word[MAXIDLENGTH+1] ) *optr++ = c;
	}

/* Make sure the line is null terminated. */
	*optr++ = '\0';
	return(word);
}




/*	"search" scans through a table of Symbols trying to find a	*/
/*	match for the supplied string.  If it does, it returns the	*/
/*	pointer to the Symbols structure, otherwise it returns NULL.	*/

static struct Symbols *
search(register char *target,
       register struct Symbols *symbols)
{

/* Each symbol array terminates with a null pointer for an */
/* "s_symbol".  Scan until a match is found, or the null pointer */
/* is reached. */
	for (;symbols->s_symbol != NULL; symbols++)
		if (strcmp(target,symbols->s_symbol) == 0) return(symbols);
	return(NULL);
}

/*	"fields" picks up the words in the next field and converts all	*/
/*	recognized words into the proper mask and puts it in the target	*/
/*	field.								*/

static char *
fields(register char *ptr,
       struct termio *termio)
{
	extern struct Symbols imodes[],omodes[],cmodes[],lmodes[];
	register struct Symbols *symbol;
	char *word;
	int size;
	extern int check;

	termio->c_iflag = 0;
	termio->c_oflag = 0;
	termio->c_cflag = 0;
	termio->c_ospeed = 0;
	termio->c_lflag = 0;
	while (*ptr != '#' && *ptr != '\0') {

/* Pick up the next word in the sequence. */
		word = getword(ptr,&size);

/* If there is a word, scan the two mode tables for it. */
		if (*word != '\0') {

/* If the word is the special word "SANE", put in all the flags */
/* that are needed for SANE tty behavior. */
			if (strcmp(word,"SANE") == 0) {
				termio->c_iflag |= ISANE;
				termio->c_oflag |= OSANE;
				termio->c_cflag |= CSANE;
				termio->c_lflag |= LSANE;
			} else if ((symbol = search(word,imodes)) != NULL)
				termio->c_iflag |= symbol->s_value;
			else if ((symbol = search(word,omodes)) != NULL)
				termio->c_oflag |= symbol->s_value;
			else if ((symbol = search(word,cmodes)) != NULL)
				termio->c_cflag |= symbol->s_value;
			else if ((symbol = search(word,lmodes)) != NULL)
				termio->c_lflag |= symbol->s_value;
			else if ((symbol = search(word,smodes)) != NULL)
				termio->c_ospeed = symbol->s_value;
			else if ((word[0] == 'B')
				 && (strspn(word+1,"0123456789") 
				     == strlen(word+1)))
			        termio->c_ospeed = (speed_t)strtoul(word+1,0,0);
			else if (check)
				fprintf(stdout,"Undefined: %s\n",word);
		}

/* Advance pointer to after the word. */
		ptr += size;
	}

/* If we didn't end on a '#', return NULL, otherwise return the */
/* updated pointer. */
	return(*ptr != '#' ? NULL : ptr);
}


/* pause to reflect after SIGHUP */
static void
huphand(void)
{
	if (Debug)
	    syslog(LOG_DEBUG, "quiting after SIGHUP");
	(void)close(0);
	(void)close(1);
	(void)close(2);
	sginap(5*HZ);
	exit(1);
}

static void
setupline(register struct Gdef *speedef,
	  int termtype,
	  int lined)
{
	struct termio termio;
	struct termcb termcb;

/* Set the terminal type to "none", which will clear all old */
/* special flags, if a terminal type was set from before. */
	termcb.st_flgs = 0;
	termcb.st_termt = TERM_NONE;
	termcb.st_vrow = 0;
	(void)ioctl(0,LDSETT,&termcb);
	termcb.st_termt = termtype;
	(void)ioctl(0,LDSETT,&termcb);

/* Get the current state of the modes and such for the terminal. */
	if (0 > ioctl(0,TCGETA,&termio))
		badcall("ioctl(TCGETA%s)%d: %m", "",__LINE__, 0);
	if (termtype != TERM_NONE) {

/* If there is a terminal type, take away settings so that */
/* terminal is "raw" and "no echo".  Also take away the orginal */
/* speed setting. */
		termio.c_iflag = 0;
		termio.c_cflag &= ~(CSIZE|PARENB);
		termio.c_cflag |= CS8|CREAD|HUPCL;
		termio.c_lflag &= ~(ISIG|ICANON|ECHO|ECHOE|ECHOK);

/* Add in the speed. */
		termio.c_ospeed = (speedef->g_iflags.c_ospeed);
	} else {
		termio.c_iflag = speedef->g_iflags.c_iflag;
		termio.c_oflag = speedef->g_iflags.c_oflag;

#ifdef CNEW_RTSCTS
		/* preserve CNEW_RTSCTS so flow control will still work
		 * if line is a ttyf port
		 */
		termio.c_cflag = (termio.c_cflag & CNEW_RTSCTS) |
		    speedef->g_iflags.c_cflag;
#else
		termio.c_cflag = speedef->g_iflags.c_cflag;
#endif
		termio.c_ospeed = speedef->g_iflags.c_ospeed;
		termio.c_lflag = speedef->g_iflags.c_lflag;
	}

/* Make sure that raw reads are 1 character at a time with no */
/* timeout. */
	termio.c_cc[VMIN] = 1;
	termio.c_cc[VTIME] = 0;

/* Add the line discipline. */
	termio.c_line = lined;
	if (0 > ioctl(0,TCSETAF,&termio))
		badcall("ioctl(TCSETAF%s)%d: %m", "",__LINE__, 0);

/* get rid of any cruft on both input and output */
	sginap(HZ/2);
	if (0 > ioctl(0,TCFLSH,2))
		badcall("ioctl(TCFLSH%s)%d: %m", "",__LINE__, 0);
}


static void
openline(register char *line,
	 register struct Gdef *speedef,
	 int termtype,
	 int lined,
	 int hangup)
{
#ifdef UUGETTY
	int i;
#endif
	struct stat statb;
	register FILE *f;
	register int lfd;
	struct termio lterm;
	cap_t ocap;
	cap_value_t cap_fowner = CAP_FOWNER;
	cap_value_t cap_device_mgt = CAP_DEVICE_MGT;


	/* we want the line to be our controlling tty */
	lfd = open("/dev/tty",0);
	if (lfd >= 0) {
		(void)ioctl(lfd, TIOCNOTTY, 0);
		(void)close(lfd);
	}
	setpgrp();		/* just in case, for debugging, ... */

	(void)signal(SIGHUP,SIG_IGN);	/* do not worry about carrier yet */
	uugetty_ck(line);		/* quit if UUCP is using line */
	errno = 0;
	lfd = open(line,O_RDWR|O_NDELAY);
	if (lfd > 0) {
		if (Debug != 0)
			badcall("failed to open \"%s\" as FD 0: fd=%d",
				line,lfd, 0);
		(void)close(lfd);
		closelog();			/* in case FD 0,1, or 2 */
		errno = 0;
		lfd = open(line,O_RDWR|O_NDELAY);
	}
	if (lfd != 0)
		badcall("cannot open \"%s\" as FD 0: %m, fd=%d", line,lfd, 1);

/* Change the ownership of the terminal line to uucp or root and set */
/* the protections to only allow him to read the line. */
	stat(line,&statb);
	ocap = cap_acquire(1, &cap_fowner);
	chown(line,UUCPUID,statb.st_gid);
	chmod(line,0622);
	cap_surrender(ocap);

	if (hangup) {
		if (speedef->g_fflags.c_cflag & HUPCL) {
			if (0 > ioctl(0,TCGETA,&lterm))
			      badcall("ioctl(TCGETA,\"%s\"): %m", line,0, 0);
			lterm.c_ospeed = B0;
			lterm.c_lflag &= ~(ECHO|ECHOE|ECHOK);
			if (0 > ioctl(0,TCSETAF,&lterm)
			    && errno != ENXIO)
			      badcall("ioctl(TCSETAF,\"%s\"): %m", line,0, 0);
			sginap(HZ*2);           /* let the modem catch up */
		}
		ocap = cap_acquire(1, &cap_device_mgt);
		if (0 > vhangup() && Debug != 0) {
			cap_surrender(ocap);
			badcall("vhangup(%s): %m", line,0, 1);
		}
		cap_surrender(ocap);
		if (0 > close(0))
			badcall("close(\"%s\"): %m", line,0, 0);

#ifdef UUGETTY
		lfd = open(line,O_RDWR|O_NDELAY);
#else
		lfd = open(line,O_RDWR);
#endif /* UUGETTY */
	}
	f = fdopen(lfd, "r+");
	if (lfd != 0 || f != stdin)
		badcall("cannot fdopen \"%s\" as FD 0: %m fd=%d", line,lfd,1);

	if (0 > fcntl(0, F_SETFL,
		   fcntl(0, F_GETFL, -1) & ~O_NDELAY)) {
		badcall("fcntl(\"%s\"): %m",line,0, 1);
	}
	(void)signal(SIGHUP,huphand);		/* now quit if line does */

	/* do not echo anything until we really have the line and it is
	 *	healthy.
	 */
	setupline(speedef,termtype,lined);
	if (0 > ioctl(0,TCGETA,&lterm))
		badcall("ioctl(TCGETA2,\"%s\"): %m", line,0, 1);
	lterm.c_lflag &= ~(ECHO|ECHOE|ECHOK);
	if (0 > ioctl(0,TCSETAF,&lterm))
		badcall("ioctl(TCSETAF2, \"%s\"): %m", line,0, 1);

#ifdef UUGETTY
	/* chat with the modem if necessary */
	if (chat1_str) {
		if (Debug)
			syslog(LOG_DEBUG, "1st chat with modem on %s", line);
		setservice("uugetty");
		i = gdial(chat1_str,chat_args,CHAT_MAX);
		if (0 == i)
			badcall("unknown Dialer script \"%s\"",chat1_str,0, 1);
		if (i > 2) {
			if (0 != chat(i-2, chat_args+2, 0, 0,0))
				badcall("modem initialization failed on %s",
					  line,0, 1);
			/* forget any stray modem cruft */
			sginap(HZ);
			if (0 > ioctl(0,TCFLSH,2))
				badcall("ioctl(TCFLSH,\"%s\"): %m", line,0, 1);
		}
	}
	delock(line_locked);
	line_locked = 0;

	/* wait until there is some input, and then try to relock the line */
	i = 0;
	for (;;) {
		fd_set readfds;

		FD_ZERO(&readfds);
		FD_SET(0,&readfds);

		/* quit if the line becomes sick */
		if (0 > select(0+1,&readfds,0,0,0)) {
			if (errno == EAGAIN || errno == EINTR)
				continue;
			badcall((i == 0 || Debug) ? "select(\"%s\"): %m" : 0,
				line,0, 1);
		}

		if (nologin && !access(_PATH_NOLOGIN, F_OK)) {
			(void)close(0);
			syslog(LOG_DEBUG,
			       "\"%s\" existed upon awakening for %s",
			       _PATH_NOLOGIN, line);
			do {
				sginap(HZ);
			} while (!access(_PATH_NOLOGIN, F_OK));
			exit(1);
		}

		if (FD_ISSET(0, &readfds)) {
			uugetty_ck(line);	/* lock the line */
			break;
		}
	}

	/* restore modes in case UUCP changed them while we waited
	 * flush any modem cruft unless we will chat the modem
	 */
	if (0 > ioctl(0, chat2_str ? TCSETA : TCSETAF, &lterm))
		badcall("ioctl(TCSETAF3, \"%s\"): %m", line,0, 1);

	if (dead_time != 0) {		/* wait for modem cruft */
		/* be sure modem cruft is not flow controlled. */
		if (0 > ioctl(0,TCFLSH,2))
			badcall("ioctl(TCFLSH, \"%s\"): %m", line,0, 1);
		sginap(dead_time*HZ);
		if (0 > ioctl(0,TCFLSH,2))
			badcall("ioctl(TCFLSH, \"%s\"): %m", line,0, 1);
	}

	/* wait to read the first character */
	if (wait_read) {
		char buffer;
		/*
		 * Check for read failure or EOT.
		 * (EOT may come from a cu on the other side.)
		 * This code is to prevent the situation of
		 * "login" program getting started here while
		 * a uugetty is running on the other end of the line.
		 * NOTE: Cu on a direct line when ~. is encountered will
		 * send EOTs to the other side.  EOT=\004
		 */
		i = read(0, &buffer, 1);
		if (i != 1 || buffer == '\004') {
			char *msg = 0;

			if (i < 0) {
				msg = "-r read: %m";
			} else if (Debug) {
				if (i == 0)
					msg = "-r read produced %d";
				else
					msg = "-r read receive EOT";
			}
			badcall(msg, 0,0, 1);
		}
	}

	/* look for CONNECT.  If we dont get it, then quit quietly. */
	if (chat2_str) {
		if (Debug)
			syslog(LOG_DEBUG, "2nd chat with modem on %s", line);
		setservice("uugetty");
		i = gdial(chat2_str,chat_args,CHAT_MAX);
		if (0 == i)
			badcall("unknown Dialer script \"%s\"",chat2_str,0, 1);
		if (i > 2) {
			if (0 != chat(i-2, chat_args+2, 0, 0,0)) {
				if (Debug != 0)
					badcall("2nd chat failed on \"%s\"",
						line,0, 0);
				exit(1);
			}
		}
		/* forget any stray modem cruft */
		sginap(HZ/2);
		if (0 > ioctl(0,TCFLSH,2))
			badcall("ioctl(TCFLSH, \"%s\"): %m", line,0, 1);
	}
#endif /* UUGETTY */

	/* check for /etc/nologin in case it was created while we were
	 * waiting for a carrier.
	 */
	if (nologin && !access(_PATH_NOLOGIN, F_OK)) {
		(void)close(0);
		syslog(LOG_DEBUG, "\"%s\" existed upon awakening for %s",
		       _PATH_NOLOGIN, line);
		do {
			sginap(HZ);
		} while (!access(_PATH_NOLOGIN, F_OK));
		exit(1);
	}

	ocap = cap_acquire(1, &cap_fowner);
	chown(line,0,statb.st_gid);		/* we now own the line */
	chmod(line,0622);
	cap_surrender(ocap);

	closelog();				/* in case FD 0,1, or 2 */
	fclose(stdout);
	fclose(stderr);
	if (0 > dup2(0,1)
	    || 0 == fdopen(1, "r+"))
		badcall("failed to open \"%s\" as STDOUT: %m", line,0, 1);
	if (0 > dup2(0,2)
	    || 0 == fdopen(2, "r+"))
		badcall("failed to open \"%s\" as STDERR: %m", line,0, 1);
	setbuf(stdin,NULL);
	setbuf(stdout,NULL);
	setbuf(stderr,NULL);

/* Set the terminal type and line discipline, after we know we own it. */
	setupline(speedef,termtype,lined);
}


static void
account(char *line)
{
	register int ownpid;
	register struct utmp *u;
	register int fd;
	cap_t ocap;
	cap_value_t cap_dac_write = CAP_DAC_WRITE;

/* Look in "utmp" file for our own entry and change it to LOGIN. */
	ownpid = getpid();

	while ((u = getutent()) != NULL) {

/* Is this our own entry? */
		if (u->ut_type == INIT_PROCESS && u->ut_pid == ownpid) {
			strncpy(u->ut_line,line,sizeof(u->ut_line));
			strncpy(u->ut_user,"LOGIN",sizeof(u->ut_user));
			u->ut_type = LOGIN_PROCESS;

/* Write out the updated entry. */
			pututline(u);
			break;
		}
	}

/* If we were successful in finding an entry for ourself in the */
/* utmp file, then attempt to append to the end of the wtmp file. */
	ocap = cap_acquire(1, &cap_dac_write);
	if (u != NULL && (fd = open(WTMP_FILE, O_WRONLY|O_APPEND)) >= 0) {
		write(fd, (char *)u, sizeof(*u));
		close(fd);
	}
	cap_surrender(ocap);

/* Close the utmp file. */
	endutent();
}


/*
 * Look for /etc/autologin.TTYLINE and use its contents as the login name if
 * it exists.
 */

static int
check_autologin(char *line, char *name)
{
	char *autopath;
	register FILE *autol;
	int len;

	len = sizeof ("/etc/autologin") + strlen (line)+1 + sizeof (".on");
	if ((autopath = malloc (len)) == NULL) {
		return (NONAME);
	}

	if (sprintf (autopath, "%s.%s.on", "/etc/autologin", line) < 0) {
		free (autopath);
		return (NONAME);
	}
	if (access (autopath, 0) == 0) {
		unlink (autopath);
		autopath[strlen(autopath)-3] = NULL;
		if ((autol = fopen(autopath, "r")) != NULL) {
			if (fscanf (autol, "%s", name) == EOF) {
				free (autopath);
				return (NONAME);
			}
			putc('\n',stdout);
			free (autopath);
			return(GOODNAME);
		}
	}
	free (autopath);
	return (NONAME);
}

/* Pick up the user's name from the standard input or
 *	/etc/autologin.TTYLINE if it exists.
 *	If it sees a line terminated with a <linefeed>, it sets ICRNL.
 */
static int
getname(char *user,
	char *line,
	struct termio *termio)
{
	register char *ptr,c;
	register int rawc;

/* Get the previous modes, erase, and kill characters and speeds. */
	if (0 > ioctl(0,TCGETA,termio))
		badcall("ioctl(TCGETA%s)%d: %m", "",__LINE__, 0);

	termio->c_iflag &= ICRNL;
	termio->c_oflag = 0;
#ifdef CNEW_RTSCTS
	termio->c_cflag &= CNEW_RTSCTS;
#else
	termio->c_cflag = 0;
#endif
	termio->c_lflag &= ECHO;
	termio->c_cc[VEOF] = CEOF;	/* fix damage from RAW mode, but */
	termio->c_cc[VEOL] = 0;		/* assume no other changes to c_cc[] */

	if (check_autologin (line, user) == GOODNAME) {
		return (GOODNAME);
	}

	ptr = user;
	do {

/* If it isn't possible to read line, exit. */
		if ((rawc = getc(stdin)) == EOF)
			huphand();

		/* If a null character was typed, try another speed. */
		if ((c = (rawc & 0177)) == '\0')
			return(BADSPEED);

		if (c == '\b' || c == '#') {
/* If there is anything to erase, erase a character. */
			if (ptr > user) {
				--ptr;
				if (*ptr >= ' ' && *ptr < 127)
					printf("\b \b");
			}


/* If the character is a kill line, start the line over */
		} else if (c == '@' || c == CDEL || c == CKILL
			  || c == CTRL('X')) {
			while (ptr > user) {
				ptr--;
				if (*ptr >= ' ' && *ptr < 127)
					printf("\b \b");
			}

		} else {
			if ((termio->c_lflag&ECHO) == 0)
				putc(rawc,stdout);
			*ptr++ = c;
		}

	/* Continue the above loop until a line terminator is found or */
	/* until user name array is full. */
	} while (c != '\n' && c != '\r' && ptr < (user + MAXLINE));

	/* Remove the last character from name. */
	*--ptr = '\0';
	if (ptr == user) return(NONAME);

/* If the line terminated with a <lf>, put ICRNL and ONLCR into */
/* into the modes. */
	if (c == '\r') {
		putc('\n',stdout);
		termio->c_iflag |= ICRNL;
		termio->c_oflag |= ONLCR;

/* When line ends with a <lf>, then add the <cr>. */
	} else putc('\r',stdout);

	return(GOODNAME);
}



main(argc,argv)
int argc;
char **argv;
{
	int i;
	char *errmsg;
	char *line;
#ifdef	AUX_LANG
	char *lang = 0;
#endif	/* AUX_LANG */

	register struct Gdef *speedef;
	char oldspeed[MAXIDLENGTH+1],newspeed[MAXIDLENGTH+1];
	int termtype,lined;
	int hangup,timeout;
	extern struct Symbols terminals[],linedisc[];
	extern void timedout();
	register struct Symbols *answer;
	char user[MAXLINE],*largs[MAXARGS],*ptr,buffer[MAXLINE];
	FILE *fp;
	struct termio termio;
#define	ESC 27
	static char clrscreen[] = {ESC,'H',ESC,'J'};

	signal(SIGINT,SIG_IGN);
	signal(SIGQUIT,SIG_IGN);	/* no reason to drop a core */

#ifdef UUGETTY
	openlog("uugetty", LOG_PID|LOG_CONS|LOG_ODELAY, LOG_AUTH);
#else
	openlog("getty", LOG_PID|LOG_CONS|LOG_ODELAY, LOG_AUTH);
#endif

	hangup = TRUE;
	timeout = 0;
	errmsg = USAGE;
	opterr = 0;
	while ((i = getopt(argc, argv, "hDNri:d:t:c:L:")) != -1) {
		switch (i) {
		case 'h':
			hangup = FALSE;
			break;

		/* Turn on some debugging */
		case 'D':
			Debug = 9;
			break;

		/* wait for /etc/nologin to go away */
		case 'N':
			nologin =1;
			break;

		/* wait to receive something from the line before proceeding */
		case 'r':
			wait_read = TRUE;
			break;

		/* remember to chat with the modem before starting */
		case 'i':
			chat1_str = optarg;
			chat2_str = strchr(optarg,',');
			if (0 != chat2_str)
				*chat2_str++ = '\0';
			break;

		/* delay after initial character */
		case 'd':
			dead_time = strtol(optarg,&ptr,0);
			if (dead_time <= 0
			    || dead_time > 100
			    || *ptr != '\0') {
				syslog(LOG_ERR,"invalid delay \"%s\"",optarg);
				errmsg = 0;;
			}
			break;

		case 't':
			timeout = strtol(optarg,&ptr,0);
			if (timeout <= 0
			    || timeout > 100
			    || *ptr != '\0') {
				syslog(LOG_ERR,"invalid timeout \"%s\"",optarg);
				errmsg = 0;;
			}
			break;

		/* Check a "gettydefs" file mode. */
		case 'c':
			check = TRUE;
			checkgdfile = optarg;

			signal(SIGINT,SIG_DFL);
			fp = fopen(checkgdfile,"r");
			if ((fp = fopen(checkgdfile,"r")) == NULL) {
				fprintf(stderr,"Cannot open %s\n",checkgdfile);
				exit(1);
			}
			fclose(fp);

			/* Call "find_def" to check the check file.
			 * With the "check" flag set, it will parse the
			 * entire file, printing out the results.
			 */
			find_def(NULL);
			exit(0);
#ifdef	AUX_LANG
		case 'L' :
			lang = optarg;
			break;
#endif	/* AUX_LANG */

		default:
			syslog(LOG_ERR,errmsg);
			errmsg = 0;
			break;
		}
	}

/* There must be at least one operand.  If there isn't, complain. */
	argc -= optind;
	argv += optind;
	if (argc <= 0 || **argv == '-' || errmsg == 0)
		badcall(errmsg,0,0, 1);
	line = *argv;

	/* point the UUCP chat code at the system log */
	(void)fclose(stdin);
	(void)close(0);
	(void)fflush(stdout);
	(void)close(1);
	(void)fflush(stderr);
	(void)close(2);

#if !defined(UUGETTY) && defined(AUX_LANG)
	if(lang) {
	    (void)strcpy(envlang, "LANG=");
	    (void)strcat(envlang, lang);
	    (void)putenv(envlang);
	}
#endif	/* !defined(UUGETTY) && defined(AUX_LANG) */


#ifdef UUGETTY
	if (Debug) {
		static char pidstr0[] = "uugetty[%d]";
		static char pidstr[sizeof(pidstr0)+8];
		int fildes[2];
		pid_t pid;
		int serror;
		char *es;

		if (0 > pipe(fildes)) {
			pid = -1;
			es = "pipe() failed for log";
		} else {
			pid = fork();
			es = "fork() failed for log";
		}
		if (pid == 0) {
			sprintf(pidstr,pidstr0,getppid());

			(void)alarm(0);
			(void)signal(SIGHUP,SIG_IGN);
			(void)signal(SIGINT,SIG_IGN);
			(void)signal(SIGUSR1,SIG_IGN);
			(void)signal(SIGUSR2,SIG_IGN);
			(void)signal(SIGTERM,SIG_IGN);

			if (0 <= dup2(fildes[0],0)) {
				for (i = getdtablehi(); --i > 0; )
					(void)close(i);
				(void)execlp("logger", "logger",
					     "-t", pidstr,
					     "-p", "daemon.debug", 0);
			}
			syslog(LOG_ERR, "failed to start logger: %m");
			exit(-1);

		} else {
			(void)close(fildes[0]);
			es = "unable to dup2() log";
			if (0 > dup2(fildes[1],1)
			    || 0 > dup2(fildes[1],2))
				pid = -1;
			if (fildes[1] != 1 && fildes[1] != 2)
				(void)close(fildes[1]);
		}

		if (pid < 0) {
			serror = errno;
			(void)freopen("/dev/log", "a", stderr);
			errno = serror;
			badcall(es, 0,0, 1);
		}
		setlinebuf(stdout);
		setlinebuf(stderr);
	}
#endif /* UUGETTY */


/* If a "speed" was provided, search for it in the */
/* "getty_defs" file.  If none was provided, take the first entry */
/* of the "getty_defs" file as the initial settings. */
	if (--argc > 0 ) {
		if ((speedef = find_def(*++argv)) == NULL) {
			syslog(LOG_ERR,"unable to find \"%s\" in %s for %s",
			       *argv,GETTY_DEFS,line);

/* Use the default value instead. */
			speedef = find_def(NULL);
		}
	} else speedef = find_def(NULL);

	/* If a terminal type was supplied, try to find it in list. */
	termtype = TERM_NONE;
	if (--argc > 0) {
		answer = search(*++argv,terminals);
		if (0 != answer)
			termtype = answer->s_value;
		else
			syslog(LOG_ERR,"%s is an undefined terminal type.",
			       *argv);
	}

	/* If a line discipline was supplied, try to find it in list. */
	lined = LDISC1;
	if (--argc > 0) {
		answer = search(*++argv,linedisc);
		if (answer != 0)
			lined = answer->s_value;
		else
			syslog(LOG_ERR,"%s is an undefined line discipline.",
			       *argv);
	}


	chdir("/dev");

	account(line);			/* Perform "utmp" accounting. */

	/* wait until the system is on */
	if (nologin && !access(_PATH_NOLOGIN, F_OK)) {
		syslog(LOG_DEBUG, "\"%s\" existed when starting %s",
		       _PATH_NOLOGIN, line);
		do {
			sginap(HZ);
		} while (!access(_PATH_NOLOGIN, F_OK));
		exit(0);		/* quit since syslog() stole FD 0 */
	}

/* Attempt to open standard input, output, and error on specified line. */
	openline(line,speedef,termtype,lined,hangup);


/* Loop until user is successful in requesting login. */
	for (;;) {

/* If there is no terminal type, just advance a line. */
		if (termtype == TERM_NONE) {

/* A bug in the stdio package requires that the first output on */
/* the newly reopened stderr stream be a putc rather than an */
/* fprintf. */
			putc('\r',stderr);
			putc('\n',stderr);

/* If there is a terminal type, clear the screen with the common */
/* crt language.  Note that the characters have to be written in */
/* one write, and hence can't go through standard io, which is */
/* currently unbuffered. */
		} else write(fileno(stderr),clrscreen,sizeof(clrscreen));

/* If getty is supposed to die if no one logs in after a */
/* predetermined amount of time, set the timer. */
		if(timeout) {
			signal(SIGALRM,timedout);
			alarm(timeout);
		}


/* Print out the issue file. */
		if ((fp = fopen(ISSUE_FILE,"r")) != NULL) {
			while (fgets(buffer,sizeof(buffer),fp) != NULL) {
				fputs(buffer,stderr);
				putc('\r',stderr);
			}
			fclose(fp);
		}

/* Print the login message. */
		fprintf(stderr,"%s",speedef->g_message);

/* Get the user's typed response and respond appropriately. */
		switch(getname(user,line,&termio)) {
		case GOODNAME:
			if (timeout) alarm(0);

/* If a terminal type was specified, keep only those parts of */
/* the gettydef final settings which were not explicitely turned */
/* on when the terminal type was set. */
			if (termtype != TERM_NONE) {
				termio.c_iflag |= (ISTRIP|ICRNL|IXON|IXANY)
				  | (speedef->g_fflags.c_iflag
				  & ~(ISTRIP|ICRNL|IXON|IXANY));
				termio.c_oflag |= (OPOST|ONLCR)
				  | (speedef->g_fflags.c_oflag
				  & ~(OPOST|ONLCR));
#ifdef CNEW_RTSCTS
				/* preserve CNEW_RTSCTS so flow control
				 * will still work if line is a ttyf port
				 */
				termio.c_cflag =
				    (termio.c_cflag & CNEW_RTSCTS) |
					speedef->g_fflags.c_cflag;
#else
				termio.c_cflag = speedef->g_fflags.c_cflag;
#endif
				termio.c_lflag = (ISIG|ICANON|ECHO|ECHOE|ECHOK)
				  | (speedef->g_fflags.c_lflag
				  & ~(ISIG|ICANON|ECHO|ECHOE|ECHOK));
			} else {
				termio.c_iflag |= speedef->g_fflags.c_iflag;
				termio.c_oflag |= speedef->g_fflags.c_oflag;
				termio.c_cflag |= speedef->g_fflags.c_cflag;
				termio.c_lflag |= speedef->g_fflags.c_lflag;
			}
			termio.c_ospeed = speedef->g_fflags.c_ospeed;
			termio.c_line = lined;
			if (0 > ioctl(0,TCSETAW,&termio))
			      badcall("ioctl(TCSETAW,\"%s\")%d: %m",
					    line,__LINE__, 0);

/* Parse the input line from the user, breaking it at white */
/* spaces. */
			largs[0] = "login";
			parse(user,&largs[1],MAXARGS-1);

/* silently destroy attempts to pass tricky args to login. */
			if (largs[1] != 0 && largs[1][0] == '-')
				largs[1] = 0;

			closelog();	/* avoid propagating stray FDs */
			execv(_PATH_SCHEME,largs);
			syslog(LOG_ERR,
				"failed to exec %s: %m", _PATH_SCHEME);

			closelog();
			execv("/bin/login",largs);
			syslog(LOG_ERR, "failed to exec /bin/login: %m");

			closelog();
			execv("/etc/login",largs);
			syslog(LOG_ERR, "failed to exec /etc/login: %m");
			exit(1);

/* If the speed supplied was bad, try the next speed in the list. */
		case BADSPEED:

/* Save the name of the old speed definition incase new one is */
/* bad.  Copy the new speed out of the static so that "find_def" */
/* won't overwrite it in the process of looking for new entry. */
			strcpy(oldspeed,speedef->g_id);
			strcpy(newspeed,speedef->g_nextid);
			if ((speedef = find_def(newspeed)) == NULL) {
				syslog(LOG_ERR,"pointer to next speed in entry %s is bad.",
					oldspeed);

/* In case of error, go back to the original entry. */
				if ((speedef = find_def(oldspeed)) == NULL) {

/* If the old entry has disappeared, then quit and let next "getty" try. */
					badcall("unable to find %s again",
						oldspeed,0, 1);
				}
			}

/* Setup the terminal for the new information. */
			setupline(speedef,termtype,lined);
			break;

/* If no name was supplied, not nothing, but try again. */
		case NONAME:
			break;
		}
	}
}


/* complain about the line and quit */
static void
badcall(char *fmt, char* sarg, int iarg, int quit)
{
	int i;

	if (quit)
		signal(SIGINT,SIG_DFL);
	if (0 != fmt)
		syslog(LOG_ERR,fmt,sarg,iarg);
	if (quit) {
		if (line_locked) {
			delock(line_locked);
			line_locked = 0;
		}
		for (i = getdtablehi(); --i > 0; )
			(void)close(i);
		sginap(30*HZ);
		exit(1);
	}
}



#ifdef UUGETTY
/* share a line with uucico/cu/ct
 *	wait for open,
 *	When success, check to see if LCK..line exists
 *	If it does, that means that uucico/cu/ct is using the line,
 *	so wait and exit when the LCK..line file goes away
 */


/* dummies for using uucp .o routines
 *	This seems like a royal kludge, but it is what the original uugetty
 *	did.
 */
void
cleanup(int code)
{
	if (Debug)
		syslog(LOG_DEBUG, "exiting with %d", code);
	exit(code);
}


/* dummies for uucp routines
 */
void
assert(s1, s2, i1, file, line)
char *s1; char *s2; int i1; char *file; int line;
{
	syslog(LOG_ERR, "%s %s (%d) [FILE: %s, LINE: %d]",
	       s1, s2, i1, file, line);
}


void
logent(text, status)
char *text; char *status;
{
	if (!Debug)
		return;
	syslog(LOG_DEBUG, "%s (%s)", status, text);
}



jmp_buf Sjbuf;


/* see if the line is available for uugetty
 *	Do not return if it is not
 */
static void
uugetty_ck(line)
char *line;
{
	char lckname[MAXNAMLEN];	/* lock file name LCK..line */

	if (mlock(line)) {		/*  There is a lock file already */
	    (void)signal(SIGHUP,SIG_IGN); /* stop worrying about carrier */
	    if (Debug)
		syslog(LOG_DEBUG, "\"%s\" is busy", line);

	    (void)close(0);		/* release the device */

	    (void)sprintf(lckname, "%s.%s", LOCKPRE, line);

	    for (;;) {			/* wait for LCK..line to go away */
		if (checkLock(lckname) == 0)
		    break;
		sginap(5*HZ);
	    }
	    sginap(1*HZ);
	    if (Debug)
		syslog(LOG_DEBUG, "finished waiting for lock to go away");
	    exit(0);
	}

	line_locked = line;
}

#endif /* UUGETTY */



void
timedout()
{
	if (Debug)
	    syslog(LOG_DEBUG, "after timeout");
	exit(1);
}


/*	"find_def" scans "/etc/gettydefs" for a string with the		*/
/*	requested "id".  If the "id" is NULL, then the first entry is	*/
/*	taken, hence the first entry must be the default entry.		*/
/*	If a match for the "id" is found, then the line is parsed and	*/
/*	the Gdef structure filled.  Errors in parsing generate error	*/
/*	messages on the system console.					*/

static struct Gdef *
find_def(char *id)
{
	register struct Gdef *gptr;
	register char *ptr,c;
	FILE *fp;
	int i,input,state,size,rawc,field;
	char oldc,*optr,*gdfile;
	char line[MAXLINE+1];
	static struct Gdef def;
	static char d_id[MAXIDLENGTH+1],d_nextid[MAXIDLENGTH+1];
	static char d_message[MAXMESSAGE+1];
	extern int check;
	static char *states[] = {
		"","id","initial flags","final flags","message","next id"
	};

/* Decide whether to read the real /etc/gettydefs or the supplied */
/* check file. */
	if (check) gdfile = checkgdfile;
	else gdfile = GETTY_DEFS;

/* Open the "/etc/gettydefs" file.  Be persistent. */
	for (i=0; i < 3; i++) {
		if ((fp = fopen(gdfile,"r")) != NULL)
			break;
		sginap(3*HZ);		/* Wait a little and then try again. */
	}

/* If unable to open, complain and then use the built in default. */
	if (fp == NULL) {
		if (check) {
			fprintf(stderr, "can't open \"%s\"\n", gdfile);
		} else {
			syslog(LOG_ERR, "can't open \"%s\"", gdfile);
		}
		return(&default_gdef);
	}

/* Start searching for the line with the proper "id". */
	input = ACTIVE;
	do {
		for(ptr= line,oldc='\0'; ptr < &line[sizeof(line)] &&
		    (rawc = getc(fp)) != EOF; ptr++,oldc = c) {
			c = *ptr = rawc;

/* Search for two \n's in a row. */
			if (c == '\n' && oldc == '\n') break;
		}

/* If we didn't end with a '\n' or EOF, then the line is too long. */
/* Skip over the remainder of the stuff in the line so that we */
/* start correctly on next line. */
		if (rawc != EOF && c != '\n') {
			for (oldc='\0'; (rawc = getc(fp)) != EOF;oldc=c) {
				c = rawc;
				if (c == '\n' && oldc != '\n') break;
			}
			if (check)
				fprintf(stdout,"Entry too long.\n");
		}

/* If we ended at the end of the file, then if there is no */
/* input, break out immediately otherwise set the "input" */
/* flag to FINISHED so that the "do" loop will terminate. */
		if (rawc == EOF) {
			if (ptr == line) break;
			else input = FINISHED;
		}

/* If the last character stored was an EOF or '\n', replace it */
/* with a '\0'. */
		if (*ptr == (EOF & 0377) || *ptr == '\n') *ptr = '\0';

/* If the buffer is full, then make sure there is a null after the */
/* last character stored. */
		else *++ptr == '\0';
		if (check)
			fprintf(stdout,"\n**** Next Entry ****\n%s\n",line);

/* If line starts with #, treat as comment */
		if(line[0] == '#') continue;

/* Initialize "def" and "gptr". */
		gptr = &def;
		gptr->g_id = (char*)NULL;
		gptr->g_iflags.c_iflag = 0;
		gptr->g_iflags.c_oflag = 0;
		gptr->g_iflags.c_cflag = 0;
		gptr->g_iflags.c_ospeed = 0;
		gptr->g_iflags.c_lflag = 0;
		gptr->g_fflags.c_iflag = 0;
		gptr->g_fflags.c_oflag = 0;
		gptr->g_fflags.c_cflag = 0;
		gptr->g_fflags.c_lflag = 0;
		gptr->g_fflags.c_ospeed = 0;
		gptr->g_message = (char*)NULL;
		gptr->g_nextid = (char*)NULL;

/* Now that we have the complete line, scan if for the various */
/* fields.  Advance to new field at each unquoted '#'. */
		for (state=ID,ptr= line; state != FAILURE && state != SUCCESS;) {
			switch(state) {
			case ID:

/* Find word in ID field and move it to "d_id" array. */
				strncpy(d_id,getword(ptr,&size),MAXIDLENGTH);
				gptr->g_id = d_id;

/* Move to the next field.  If there is anything but white space */
/* following the id up until the '#', then set state to FAILURE. */
				ptr += size;
				while (isspace(*ptr)) ptr++;
				if (*ptr != '#') {
					field = state;
					state = FAILURE;
				} else {
					ptr++;	/* Skip the '#' */
					state = IFLAGS;
				}
				break;

/* Extract the "g_iflags" */
			case IFLAGS:
				if ((ptr = fields(ptr,&gptr->g_iflags)) == NULL) {
					field = state;
					state = FAILURE;
				} else {
					gptr->g_iflags.c_iflag &= (ICRNL|IUCLC);
					if((gptr->g_iflags.c_cflag & CSIZE) == 0)
						gptr->g_iflags.c_cflag |= CS8;
					gptr->g_iflags.c_cflag |= CREAD|HUPCL;
					gptr->g_iflags.c_lflag &= ~(ISIG|ICANON
						|XCASE|ECHOE|ECHOK);
					ptr++;
					state = FFLAGS;
				}
				break;

/* Extract the "g_fflags". */
			case FFLAGS:
				if ((ptr = fields(ptr,&gptr->g_fflags)) == NULL) {
					field = state;
					state = FAILURE;
				} else {

/* Force the CREAD mode in regardless of what the user specified. */
					gptr->g_fflags.c_cflag |= CREAD;
					ptr++;
					state = MESSAGE;
				}
				break;

/* Take the entire next field as the "login" message. */
/* Follow usual quoting procedures for control characters. */
			case MESSAGE:
				for (optr= d_message; (c = *ptr) != '\0'
				    && c != '#';ptr++) {

/* If the next character is a backslash, then get the quoted */
/* character as one item. */
					if (c == '\\') {
						c = quoted(ptr,&size);
/* -1 accounts for ++ that takes place later. */
						ptr += size - 1;
					}

/* If the next character is a dollar sign, get the variable name */
/* following it and interpolate its value into d_message */
					if (c == '$') {
						char *value;
						register int length;

						value = getvar(ptr, &size);
						ptr += size;
						length = strlen(value);
						if (optr + length
						    < &d_message[MAXMESSAGE]) {
							bcopy(value, optr,
							    length);
							optr += length;
						}
					} else
/* If there is room, store the next character in d_message. */
					if (optr < &d_message[MAXMESSAGE]) {
						*optr++ = c;
					}
				}

/* If we ended on a '#', then all is okay.  Move state to NEXTID. */
/* If we didn't, then set state to FAILURE. */
				if (c == '#') {
					gptr->g_message = d_message;
					state = NEXTID;

/* Make sure message is null terminated. */
					*optr++ = '\0';
					ptr++;
				} else {
					field = state;
					state = FAILURE;
				}
				break;

/* Finally get the "g_nextid" field.  If this is successful, then */
/* the line parsed okay. */
			case NEXTID:

/* Find the first word in the field and save it as the next id. */
				strncpy(d_nextid,getword(ptr,&size),MAXIDLENGTH);
				gptr->g_nextid = d_nextid;

/* There should be nothing else on the line.  Starting after the */
/* word found, scan to end of line.  If anything beside white */
/* space, set state to FAILURE. */
				ptr += size;
				while (isspace(*ptr)) ptr++;
				if (*ptr != '\0') {
					field = state;
					state = FAILURE;
				} else state = SUCCESS;
				break;
			}
		}

/* If a line was successfully picked up and parsed, compare the */
/* "g_id" field with the "id" we are looking for. */
		if (state == SUCCESS) {

/* If there is an "id", compare them. */
			if (id != NULL) {
				if (strcmp(id,gptr->g_id) == 0) {
					fclose(fp);
					return(gptr);
				}

/* If there is no "id", then return this first successfully */
/* parsed line outright. */
			} else if (check == FALSE) {
				fclose(fp);
				return(gptr);

/* In check mode print out the results of the parsing. */
			} else {
				fprintf(stdout,"id: %s\n",gptr->g_id);
				fprintf(stdout,"initial flags:\niflag- %o oflag- %o cflag- %o lflag- %o ospeed- %d\n",
					gptr->g_iflags.c_iflag,
					gptr->g_iflags.c_oflag,
					gptr->g_iflags.c_cflag,
					gptr->g_iflags.c_lflag,
					gptr->g_iflags.c_ospeed);
				fprintf(stdout,"final flags:\niflag- %o oflag- %o cflag- %o lflag- %o ospeed- %d\n",
					gptr->g_fflags.c_iflag,
					gptr->g_fflags.c_oflag,
					gptr->g_fflags.c_cflag,
					gptr->g_fflags.c_lflag,
					gptr->g_iflags.c_ospeed);
				fprintf(stdout,"message: %s\n",gptr->g_message);
				fprintf(stdout,"next id: %s\n",gptr->g_nextid);
			}

/* If parsing failed in check mode, complain, otherwise ignore */
/* the bad line. */
		} else if (check) {
			*++ptr = '\0';
			fprintf(stdout,"Parsing failure in the \"%s\" field\n\
%s<--error detected here\n",
				states[field],line);
		}
	} while (input == ACTIVE);

/* If no match was found, then return NULL. */
	fclose(fp);
	return(NULL);
}

/*
 * Get the variable name which follows the '$' pointed at by cp.
 * Return a pointer to its value directly.  Return the variable name
 * length indirectly through sizep.
 */
static char *
getvar(char *cp, int *sizep)
{
	register char *vp;
	static char hostname[MAXHOSTNAMELEN];

	vp = getword(cp + 1, sizep);
	if (strcmp(vp, "HOSTNAME") == 0) {
		if (hostname[0] == '\0') {
			gethostname(hostname, MAXHOSTNAMELEN);
		}
		return hostname;
	} else {
		if (check)
			fprintf(stderr, "getty: undefined variable %.*s\n",
			    *sizep, cp + 1);
		else
			syslog(LOG_ERR,"undefined variable %.*s",
			    *sizep, cp + 1);
		return "?";
	}
}

/*	"parse" breaks up the user's response into seperate arguments	*/
/*	and fills the supplied array with those arguments.  Quoting	*/
/*	with the backspace is allowed.					*/
static void
parse(char *string, char **args, int cnt)
{
	register char *ptrin,*ptrout;
	register int i;
	int qsize;

	for (i=0; i < cnt; i++) args[i] = (char *)NULL;
	for (ptrin = ptrout = string,i=0; *ptrin != '\0' && i < cnt; i++) {

/* Skip excess white spaces between arguments. */
		while(*ptrin == ' ' || *ptrin == '\t') {
			ptrin++;
			ptrout++;
		}

/* Save the address of the argument if there is something there. */
		if (*ptrin == '\0') break;
		else args[i] = ptrout;

/* Span the argument itself.  The '\' character causes quoting */
/* of the next character to take place (except for '\0'). */
		while (*ptrin != '\0') {

/* Is this the quote character? */
			if (*ptrin == '\\') {
				*ptrout++ = quoted(ptrin,&qsize);
				ptrin += qsize;

/* Is this the end of the argument?  If so quit loop. */
			} else if (*ptrin == ' ' || *ptrin == '\t') {
				ptrin++;
				break;

/* If this is a normal letter of the argument, save it, advancing */
/* the pointers at the same time. */
			} else *ptrout++ = *ptrin++;
		}

/* Null terminate the string. */
		*ptrout++ = '\0';
	}
}
