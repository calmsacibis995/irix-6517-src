char _Origin_[] = "UC Berkeley";
static	char	*Sccsid = "@(#)$Header: /proj/irix6.5.7m/isms/eoe/cmd/tset/RCS/tset.c,v 1.12 1997/03/18 22:29:23 tcl Exp $";

/* #define	DEB */
/*
**  TSET -- set terminal modes
**
**	This program does sophisticated terminal initialization.
**	I recommend that you include it in your .start_up or .login
**	file to initialize whatever terminal you are on.
**
**	There are several features:
**
**	A special file or sequence (as controlled by the ttycap file)
**	is sent to the terminal.
**
**	Mode bits are set on a per-terminal_type basis (much better
**	than UNIX itself).  This allows special delays, automatic
**	tabs, etc.
**
**	Erase and Kill characters can be set to whatever you want.
**	Default is to change erase to control-H on a terminal which
**	can overstrike, and leave it alone on anything else.  Kill
**	is always left alone unless specifically requested.  These
**	characters can be represented as "^X" meaning control-X;
**	X is any character.
**
**	Terminals which are dialups or plugboard types can be aliased
**	to whatever type you may have in your home or office.  Thus,
**	if you know that when you dial up you will always be on a
**	TI 733, you can specify that fact to tset.  You can represent
**	a type as "?type".  This will ask you what type you want it
**	to be -- if you reply with just a newline, it will default
**	to the type given.
**
**	The htmp file, used by ex, etc., can be updated.
**
**	The current terminal type can be queried.
**
**	Usage:
**		tset [-] [-EC] [-eC] [-kC] [-s] [-h] [-u] [-r]
**			[-m [ident] [test baudrate] :type]
**			[-Q] [-I] [-S] [type]
**
**		In systems with environments, use:
**			`tset -s ...`
**		Actually, this doesn't work because of a shell bug.
**		Instead, use:
**			tset -s ... > tset.tmp
**			source tset.tmp
**			rm tset.tmp
**		or:
**			set noglob
**			set term=(`tset -S ....`)
**			setenv TERM $term[1]
**			setenv TERMCAP "$term[2]"
**			unset term
**			unset noglob
**
**	Positional Parameters:
**		type -- the terminal type to force.  If this is
**			specified, initialization is for this
**			terminal type.
**
**	Flags:
**		- -- report terminal type.  Whatever type is
**			decided on is reported.  If no other flags
**			are stated, the only affect is to write
**			the terminal type on the standard output.
**		-r -- report to user in addition to other flags.
**		-EC -- set the erase character to C on all terminals
**			except those which cannot backspace (e.g.,
**			a TTY 33).  C defaults to control-H.
**		-eC -- set the erase character to C on all terminals.
**			C defaults to control-H.  If neither -E or -e
**			are specified, the erase character is set to
**			control-H if the terminal can both backspace
**			and not overstrike (e.g., a CRT).  If the erase
**			character is NULL (zero byte), it will be reset
**			to '#' if nothing else is specified.
**		-kC -- set the kill character to C on all terminals.
**			Default for C is control-X.  If not specified,
**			the kill character is untouched; however, if
**			not specified and the kill character is NULL
**			(zero byte), the kill character is set to '@'.
**		-iC -- reserved for setable interrupt character.
**		-qC -- reserved for setable quit character.
**		-m -- map the system identified type to some user
**			specified type. The mapping can be baud rate
**			dependent. This replaces the old -d, -p flags.
**			(-d type  ->  -m dialup:type)
**			(-p type  ->  -m plug:type)
**			Syntax:	-m identifier [test baudrate] :type
**			where: ``identifier'' is whatever is found in
**			/etc/ttytype for this port, (abscence of an identifier
**			matches any identifier); ``test'' may be any combination
**			of  >  =  <  !  @; ``baudrate'' is as with stty(1);
**			``type'' is the actual terminal type to use if the
**			mapping condition is met. Multiple maps are scanned
**			in order and the first match prevails.
**		-h -- don't read htmp file.  Normally the terminal type
**			is determined by reading the htmp file or the
**			environment (unless some mapping is specified).
**			This forces a read of the ttytype file -- useful
**			when htmp is somehow wrong.
**		-u -- don't update htmp.  It seemed like this should
**			be put in.  Note that htmp is never actually
**			written if there are no changes, so don't bother
**			bother using this for efficiency reasons alone.
**		-s -- output setenv commands for TERM.  This can be
**			used with
**				`tset -s ...`
**			and is to be prefered to:
**				setenv TERM `tset - ...`
**			because -s sets the TERMCAP variable also.
**		-S -- Similar to -s but outputs 2 strings suitable for
**			use in csh .login files as follows:
**				set noglob
**				set term=(`tset -S .....`)
**				setenv TERM $term[1]
**				setenv TERMCAP "$term[2]"
**				unset term
**				unset noglob
**		-Q -- be quiet.  don't output 'Erase set to' etc.
**		-I -- don't do terminal initialization (is & if
**			strings).
**
**	Files:
**		/etc/ttytype
**			contains a terminal id -> terminal type
**			mapping; used when any user mapping is specified,
**			or the environment doesn't have TERM set.
**		/etc/termcap
**			a terminal_type -> terminal_capabilities
**			mapping.
**
**	Return Codes:
**		-1 -- couldn't open ttycap.
**		1 -- bad terminal type, or standard output not tty.
**		0 -- ok.
**
**	Defined Constants:
**		DIALUP -- the type code for a dialup port
**		PLUGBOARD -- the code for a plugboard port.
**		ARPANET -- the code for an arpanet port.
**		BACKSPACE -- control-H, the default for -e.
**		CONTROLX -- control-X, the default for -k.
**		OLDERASE -- the system default erase character.
**		OLDKILL -- the system default kill character.
**		FILEDES -- the file descriptor to do the operation
**			on, nominally 1 or 2.
**		STDOUT -- the standard output file descriptor.
**		UIDMASK -- the bit pattern to mask with the getuid()
**			call to get just the user id.
**		GTTYN -- defines file containing generalized ttynames
**			and compiles code to look there.
**
**	Requires:
**		Routines to handle htmp, ttytype, and ttycap.
**
**	Compilation Flags:
**		OLDDIALUP -- accept the -d flag. Map "sd" to "dialup".
**		OLDPLUGBOARD -- accept the -p flag. Map "sp" to "plugboard".
**		OLDARPANET -- accept the -a flag. Map "sa" to "arpanet".
**		OLDFLAGS -- must be defined to compile code for any of
**			the -d, -p, or -a flags.
**		FULLLOGIN -- if defined, login sets the ttytype from
**			/etc/ttytype file.
**		GTTYN -- if set, compiles code to look at /etc/ttytype.
**
**	Trace Flags:
**		none
**
**	Diagnostics:
**		Bad flag
**			An incorrect option was specified.
**		Too few args
**			more command line arguments are required.
**		Unexpected arg
**			wrong type of argument was encountered.
**		Cannot open ...
**			The specified file could not be openned.
**		Type ... unknown
**			An unknown terminal type was specified.
**		Cannot update htmp
**			Cannot update htmp file when the standard
**			output is not a terminal.
**		Erase set to ...
**			Telling that the erase character has been
**			set to the specified character.
**		Kill set to ...
**			Ditto for kill
**		Erase is ...    Kill is ...
**			Tells that the erase/kill characters were
**			wierd before, but they are being left as-is.
**		Not a terminal
**			Set if FILEDES is not a terminal.
**
**	Compilation Instructions:
**		cc -n -O tset.c -ltermlib
**		mv a.out tset
**		chown bin tset
**		chmod 4755 tset
**
**		where 'bin' should be whoever owns the 'htmp' file.
**		If 'htmp' is 666, then tset need not be setuid.
**
**	Author:
**		Eric Allman
**		Electronics Research Labs
**		U.C. Berkeley
**
**	History:
**		7/80 -- '-S' added. -m mapping added. TERMCAP string
**			cleaned up.
**		3/80 -- Changed to use tputs.  Prc & flush added.
**		10/79 -- '-s' option extended to handle TERMCAP
**			variable, set noglob, quote the entry,
**			and know about the Bourne shell.  Terminal
**			initialization moved to before any information
**			output so screen clears would not screw you.
**			'-Q' option added.
**		8/79 -- '-' option alone changed to only output
**			type.  '-s' option added.  'VERSION7'
**			changed to 'V6' for compatibility.
**		12/78 -- modified for eventual migration to VAX/UNIX,
**			so the '-' option is changed to output only
**			the terminal type to STDOUT instead of
**			FILEDES.  FULLLOGIN flag added.
**		9/78 -- '-' and '-p' options added (now fully
**			compatible with ttytype!), and spaces are
**			permitted between the -d and the type.
**		8/78 -- The sense of -h and -u were reversed, and the
**			-f flag is dropped -- same effect is available
**			by just stating the terminal type.
**		10/77 -- Written.
*/

/*
 * # define	FULLLOGIN	1
 */
# define	GTTYN		"/etc/ttytype"

# include	<ctype.h>
# include	<termio.h>
# include	<sys/ioctl.h>
# include	<stdio.h>
# include	<string.h>

# define	BACKSPACE	('H' & 037)
# define	CONTROLX	('X' & 037)
# define	OLDERASE	'#'
# define	OLDKILL		'@'

# define	FILEDES		0
# define	STDOUT		1

# define	UIDMASK		-1

# define	DEFTYPE		"unknown"
# define	USAGE\
"usage: tset [-] [-hrsuIQS] [-eC] [-kC] [-m [ident][test speed]:type] [type]\n"

# define	DIALUP		"dialup"
# define	OLDDIALUP	"sd"
# define	PLUGBOARD	"plugboard"
# define	OLDPLUGBOARD	"sp"
/***
 * # define	ARPANET		"arpanet"
 * # define	OLDARPANET	"sa"
 **/
# define	OLDFLAGS



# ifdef GTTYN
typedef char	*ttyid_t;
# define	NOTTY		0
# else
typedef char	ttyid_t;
# define	NOTTY		'x'
# endif

/*
 * Baud Rate Conditionals
 */
# define	ANY		0
# define	GT		1
# define	EQ		2
# define	LT		4
# define	GE		(GT|EQ)
# define	LE		(LT|EQ)
# define	NE		(GT|LT)
# define	ALL		(GT|EQ|LT)



# define	NMAP		10

struct	map {
	char *Ident;
	char Test;
	speed_t Speed;
	char *Type;
} map[NMAP];

struct map *Map = map;

struct
{
	char	*string;
	int	speed;
} speeds[] = {
	"134.5",B134,
	"exta",	EXTA,
	"extb",	EXTB,
	0,
};

char	Erase_char;		/* new erase character */
char	Kill_char;		/* new kill character */
char	Specialerase;		/* set => Erase_char only on terminals with backspace */

ttyid_t	Ttyid = NOTTY;		/* terminal identifier */
char	*TtyType;		/* type of terminal */
char	*DefType;		/* default type if none other computed */
char	*NewType;		/* mapping identifier based on old flags */
int	Dash_u;			/* don't update htmp */
int	Dash_h;			/* don't read htmp */
int	DoSetenv;		/* output setenv commands */
int	BeQuiet;		/* be quiet */
int	NoInit;			/* don't output initialization string */
int	Report;			/* report current type */
int	Ureport;		/* report to user */
int	RepOnly;		/* report only */
int	CmndLine;		/* output full command lines (-s option) */
int	Ask;			/* ask user for termtype */

# define CAPBUFSIZ	1024
char	Capbuf[CAPBUFSIZ];	/* line from /etc/termcap for this TtyType */
char	*Ttycap;		/* termcap line from termcap or environ */

long	maxdelay;

struct delay
{
	int	d_delay;
	int	d_bits;
};

# include	"tset.dels.h"

static baudrate(char *);

void reportek(char*, char, char, char);
void setdelay(char*, struct delay*, int, tcflag_t *);
void prs(char*);
void flush(void);
void cat(char*);
void bmove(char*, char*, int);
void fatal (char*, char*);
void prc(char);

int
main(int argc, char *argv[])
{
	struct termio	mode;
	struct termio	oldmode;
	char		buf[256];
	char		oldbuf[256];
	auto char	*bufp;
	register char	*p;
	char		*command;
	register int	i;
	int		Break;
	int		Not;
	int		Mapped = 0;
	extern char	*nextarg();
	extern char	*mapped();
	extern char	*getenv();
# ifdef GTTYN
	extern char	*stypeof();
	extern char	*ttyname();
	extern char	*tgetstr();
# endif
	char		bs_char;
	int		csh;
	extern char	PC;
	speed_t		ospeed;

	if (ioctl(FILEDES, TCGETA, &mode) < 0)
	{
		prs("Not a terminal\n");
		exit(1);
	}
	bmove((char*)&mode, (char*)&oldmode, sizeof mode);
	ospeed = mode.c_ospeed;

	/* scan argument list and collect flags */
	command = argv[0];
	if (argc == 2 && argv[1][0] == '-' && argv[1][1] == '\0')
	{
		RepOnly++;
		Dash_u++;
	}
	argc--;
	while (--argc >= 0)
	{
		p = *++argv;
		if (*p == '-')
		{
			if (*++p == NULL)
				Report++; /* report current terminal type */
			else while (*p) switch (*p++)
			{

			  case 'r':	/* report to user */
				Ureport++;
				continue;

			  case 'E':	/* special erase: operate on all but TTY33 */
				Specialerase++;
				/* explicit fall-through to -e case */

			  case 'e':	/* erase character */
				if (*p == NULL)
					Erase_char = -1;
				else
				{
					if (*p == '^' && p[1] != NULL)
						Erase_char = *++p & 037;
					else
						Erase_char = *p;
					p++;
				}
				continue;

			  case 'k':	/* kill character */
				if (*p == NULL)
					Kill_char = CONTROLX;
				else
				{
					if (*p == '^' && p[1] != NULL)
						Kill_char = *++p & 037;
					else
						Kill_char = *p;
					p++;
				}
				continue;

# ifdef OLDFLAGS
# ifdef	OLDDIALUP
			  case 'd':	/* dialup type */
				NewType = DIALUP;
				goto mapold;
# endif

# ifdef OLDPLUGBOARD
			  case 'p':	/* plugboard type */
				NewType = PLUGBOARD;
				goto mapold;
# endif

# ifdef OLDARPANET
			  case 'a':	/* arpanet type */
				Newtype = ARPANET;
				goto mapold;
# endif

mapold:				Map->Ident = NewType;
				Map->Test = ALL;
				if (*p == NULL)
				{
					p = nextarg(argc--, argv++);
				}
				Map->Type = p;
				Map++;
				Mapped++;
				p = "";
				continue;
# endif

			  case 'm':	/* map identifier to type */
				/* This code is very loose. Almost no
				** syntax checking is done!! However,
				** illegal syntax will only produce
				** weird results.
				*/
				if (*p == NULL)
				{
					p = nextarg(argc--, argv++);
				}
				if (isalnum(*p))
				{
					Map->Ident = p;	/* identifier */
					while (isalnum(*p)) p++;
				}
				else
					Map->Ident = "";
				Break = 0;
				Not = 0;
				while (!Break) switch (*p)
				{
					case NULL:
						p = nextarg(argc--, argv++);
						continue;

					case ':':	/* mapped type */
						*p++ = NULL;
						Break++;
						continue;

					case '>':	/* conditional */
						Map->Test |= GT;
						*p++ = NULL;
						continue;

					case '<':	/* conditional */
						Map->Test |= LT;
						*p++ = NULL;
						continue;

					case '=':	/* conditional */
					case '@':
						Map->Test |= EQ;
						*p++ = NULL;
						continue;
					
					case '!':	/* invert conditions */
						Not = ~Not;
						*p++ = NULL;
						continue;

					case 'B':	/* Baud rate */
						p++;
						/* intentional fallthru */
					default:
						if (isdigit(*p) || *p == 'e')
						{
							Map->Speed = baudrate(p);
							while (isalnum(*p) || *p == '.')
								p++;
						}
						else
							Break++;
						continue;
				}
				if (Not)	/* invert sense of test */
				{
					Map->Test = (~(Map->Test))&ALL;
				}
				if (*p == NULL)
				{
					p = nextarg(argc--, argv++);
				}
				Map->Type = p;
				p = "";
				Map++;
				Mapped++;
				continue;

			  case 'h':	/* don't get type from htmp or env */
				Dash_h++;
				continue;

			  case 'u':	/* don't update htmp */
				Dash_u++;
				continue;

			  case 's':	/* output setenv commands */
				DoSetenv++;
				CmndLine++;
				continue;

			  case 'S':	/* output setenv strings */
				DoSetenv++;
				CmndLine=0;
				continue;

			  case 'Q':	/* be quiet */
				BeQuiet++;
				continue;

			  case 'I':	/* no initialization */
				NoInit++;
				continue;

			  case 'A':	/* Ask user */
				Ask++;
				continue;

			  default:
				*p-- = NULL;
				fatal("Bad flag -", p);
			}
		}
		else
		{
			/* terminal type */
			DefType = p;
		}
	}

	if (DefType)
	{
		if (Mapped)
		{
			Map->Ident = "";	/* means "map any type" */
			Map->Test = ALL;	/* at all baud rates */
			Map->Type = DefType;	/* to the default type */
		}
		else
			TtyType = DefType;
	}

	/* get current idea of terminal type from environment */
	if (!Dash_h && !Mapped && TtyType == 0)
		TtyType = getenv("TERM");

	/* determine terminal id if needed */
	if (!RepOnly && Ttyid == NOTTY && (TtyType == 0 || !Dash_h))
		Ttyid = ttyname(FILEDES);

# ifdef GTTYN
	/* If still undefined, look at /etc/ttytype */
	if (TtyType == 0)
	{
		TtyType = stypeof(Ttyid);
	}
# endif

	/* If still undefined, use DEFTYPE */
	if (TtyType == 0)
	{
		TtyType = DEFTYPE;
	}

	/* check for dialup or other mapping */
	if (Mapped)
		TtyType = mapped(TtyType, ospeed);

	/* TtyType now contains a pointer to the type of the terminal */
	/* If the first character is '?', ask the user */
	if (TtyType[0] == '?')
	{
		Ask++;
		TtyType++;
		if (TtyType[0] == '\0')
			TtyType = DEFTYPE;
	}
	if (Ask)
	{
		extern char *malloc();
badboy:
		prs("TERM = (");
		prs(TtyType);
		prs(") ");
		flush();
			/*
			 * Remember original ttyname in case user enters an
			 * unknown tty in which case we want to re-prompt
			 * with original tty.
			 */
		strcpy(oldbuf,TtyType);

		/* read the terminal.  If not empty, set type */
		i = read(0, buf, sizeof buf - 1);
		if (i >= 0)
		{
			if (buf[i - 1] == '\n')
				i--;
			buf[i] = '\0';
			if (buf[0] != '\0') {
				TtyType = malloc(strlen(buf)+1);
				strcpy(TtyType, buf);
			}
		}
	}

	if (Ttycap == 0)
	{
		/* get terminal capabilities */
		switch (tgetent(Capbuf, TtyType))
		{
		  case -1:
#if defined(SVR3) || defined(__sgi)
			prs("Cannot open terminfo database\n");
#else
			prs("Cannot open termcap file\n");
#endif
			flush();
			exit(-1);

		  case 0:
			prs("Type ");
			prs(TtyType);
			prs(" unknown\n");
			flush();
			if (Ask) {
					/* re-prompt with orig. tty type */
				TtyType=oldbuf;
				goto badboy;
			}
			exit(1);
		}
		Ttycap = Capbuf;
	}

	if (!RepOnly)
	{
		/* determine erase and kill characters */
		if (Specialerase && !tgetflag("bs"))
			Erase_char = 0;
		bufp = buf;
		p = tgetstr("kb", &bufp);
		if (p == NULL || p[1] != '\0')
			p = tgetstr("bc", &bufp);
		if (p != NULL && p[1] == '\0')
			bs_char = p[0];
		else if (tgetflag("bs"))
			bs_char = BACKSPACE;
		else
			bs_char = 0;
		if (Erase_char == 0 && !tgetflag("os") && mode.c_cc[2] == OLDERASE)
		{
			if (tgetflag("bs") || bs_char != 0)
				Erase_char = -1;
		}
		if (Erase_char < 0)
			Erase_char = (bs_char != 0) ? bs_char : BACKSPACE;

		if (mode.c_cc[2] == 0)
			mode.c_cc[2] = OLDERASE;
		if (Erase_char != 0)
			mode.c_cc[2] = Erase_char;

		if (mode.c_cc[3] == 0)
			mode.c_cc[3] = OLDKILL;
		if (Kill_char != 0)
			mode.c_cc[3] = Kill_char;

		maxdelay = 0;
		/* set modes */
		setdelay("dC", CRdelay, CRbits, &mode.c_oflag);
		setdelay("dN", NLdelay, NLbits, &mode.c_oflag);
		setdelay("dB", BSdelay, BSbits, &mode.c_oflag);
		setdelay("dF", FFdelay, FFbits, &mode.c_oflag);
		setdelay("dT", TBdelay, TBbits, &mode.c_oflag);
		if (tgetflag("UC") || command[0] == 'T')
			mode.c_iflag |= IUCLC;
		else if (tgetflag("LC"))
			mode.c_iflag &= ~IUCLC;
		mode.c_cflag &= ~(PARENB | PARODD);
		mode.c_lflag |= ISIG | ICANON;
		if (tgetflag("EP"))
			mode.c_cflag |= PARENB;
		if (tgetflag("OP"))
			mode.c_cflag |= PARENB | PARODD;
		if ((mode.c_cflag&CSIZE) == CS7)
			mode.c_cflag |= PARENB;
		mode.c_oflag &= OCRNL;
		mode.c_oflag |= OPOST | ONLCR | TAB3;
		mode.c_iflag |= ICRNL;
		mode.c_lflag |= ISIG | ECHO | ECHOE | ECHOK;
		if (tgetflag("NL"))	/* new line, not line feed */
			mode.c_cflag &= ~(OCRNL | ONLCR);
		if (tgetflag("HD"))	/* half duplex */
			mode.c_lflag &= ~(ECHO | ECHOE | ECHOK);
		if (tgetflag("pt"))	/* print tabs */
			mode.c_oflag &= ~TAB3;
		if (!bequal(mode.c_cc, oldmode.c_cc, NCC) ||
		    !bequal(&mode, &oldmode, sizeof mode))
			ioctl(FILEDES, TCSETA, &mode);

		/* get pad character */
		bufp = buf;
		if (tgetstr("pc", &bufp) != 0)
			PC = buf[0];

		/* output startup string */
		if (!NoInit)
		{
			bufp = buf;
			if (tgetstr("is", &bufp) != 0)
				tputs(buf, 0, prc);
			flush();
			bufp = buf;
			if (tgetstr("if", &bufp) != 0)
				cat(buf);
			if (maxdelay) {
				  sleep(1);	/* let terminal settle down */
			}
		}

		/* set up environment for the shell we are using */
		/* (this code is rather heuristic) */
		csh = 0;
		if (DoSetenv)
		{
			char *sh;

			if ((sh = getenv("SHELL")) && (i = strlen(sh)) >= 3)
			{
				p = &sh[i-3];
				if ((csh = sequal(p, "csh")) && CmndLine)
					write(STDOUT, "set noglob;\n", 12);
			}
			if (!csh)
				/* running system shell */
#if defined(SVR3) || defined(__sgi)
				write(STDOUT, "export TERM;\n", 21-8);
#else
				write(STDOUT, "export TERMCAP TERM;\n", 21);
#endif
		}
	}

	/* report type if appropriate */
	if (DoSetenv || Report || Ureport)
	{
#if defined(SVR3) || defined(__sgi)
		bufp = TtyType;
		p = TtyType + strlen(TtyType);
#else
		/* find first alias (if any) */
		for (p = Ttycap; *p != 0 && *p != '|' && *p != ':'; p++)
			continue;
		if (*p == 0 || *p == ':')
			p = Ttycap;
		else
			p++;
		bufp = p;
		while (*p != '|' && *p != ':' && *p != 0)
			p++;
#endif
		i = *p;
		if (DoSetenv)
		{
			if (csh)
			{
				if (CmndLine)
					write(STDOUT, "setenv TERM ", 12);
				write(STDOUT, bufp, p - bufp);
				write(STDOUT, " ", 1);
				if (CmndLine)
					write(STDOUT, ";\n", 2);
			}
			else
			{
				write(STDOUT, "TERM=", 5);
				write(STDOUT, bufp, p - bufp);
				write(STDOUT, ";\n", 2);
			}
		}
		if (Report && !DoSetenv)
		{
			write(STDOUT, bufp, p - bufp);
			write(STDOUT, "\n", 1);
		}
		if (Ureport)
		{
			*p = '\0';
			prs("Terminal type is ");
			prs(bufp);
			prs("\n");
			flush();
		}
		*p = i;
		if (DoSetenv)
		{
#if !defined(SVR3) && !defined(__sgi)
			if (csh)
			{
				if (CmndLine)
					write(STDOUT, "setenv TERMCAP '", 16);
			}
			else
				write(STDOUT, "TERMCAP='", 9);
			wrtermcap(Ttycap);
			if (csh)
			{
				if (CmndLine)
				{
					write(STDOUT, "';\n", 3);
					write(STDOUT, "unset noglob;\n", 14);
				}
			}
			else
				write(STDOUT, "';\n", 3);
#else
			if (csh && CmndLine)
				write(STDOUT, "unset noglob;\n", 14);
#endif
		}
	}

	if (RepOnly)
		exit(0);

	/* tell about changing erase and kill characters */
	reportek("Erase", mode.c_cc[2], oldmode.c_cc[2], OLDERASE);
	reportek("Kill", mode.c_cc[3], oldmode.c_cc[3], OLDKILL);

	exit(0);
}

void
reportek(char	*name,
	 char	new,
	 char	old,
	 char	def)
{
	register char	o;
	register char	n;
	static char	p[] = "x\n";

	if (BeQuiet)
		return;
	o = old;
	n = new;

	if (o == n && n == def)
		return;
	prs(name);
	if (o == n)
		prs(" is ");
	else
		prs(" set to ");
	if (n < 040)
	{
		prs("control-");
		n = (n & 037) | 0100;
	} else if (n == 0177) {
		prs("delete\n");
		flush();
		return;
	}
	p[0] = n;
	prs(p);
	flush();
}




void
setdelay(char		*cap,
	 struct delay	dtab[],
	 int		bits,
	 tcflag_t	*flags)
{
	register int	i;
	register struct delay	*p;

	/* see if this capability exists at all */
	i = tgetnum(cap);
	if (i < 0)
		i = 0;

	if (i > maxdelay)
		maxdelay = i;

	/* clear out the bits, replace with new ones */
	*flags &= ~bits;

	/* scan dtab for first entry with adequate delay */
	for (p = dtab; p->d_delay >= 0; p++)
	{
		if (p->d_delay >= i)
		{
			p++;
			break;
		}
	}

	/* use last entry if none will do */
	*flags |= (--p)->d_bits;

	if (*flags & (NL1|CR1|CR2|CR3|TAB1|TAB2|TAB3|BS1|VT1|FF1))
		*flags |= OPOST;
}


void
prs(char	*s)
{
	while (*s != '\0')
		prc(*s++);
}


char	OutBuf[256];
int	OutPtr;

void
prc(char c)
{
	OutBuf[OutPtr++] = c;
	if (OutPtr >= sizeof OutBuf)
		flush();
}

void
flush()
{
	if (OutPtr > 0)
		write(2, OutBuf, OutPtr);
	OutPtr = 0;
}


void
cat(char	*file)
{
	register int	fd;
	register int	i;
	char		buf[BUFSIZ];

	fd = open(file, 0);
	if (fd < 0)
	{
		prs("Cannot open ");
		prs(file);
		prs("\n");
		flush();
		exit(-1);
	}

	flush();
	while ((i = read(fd, buf, BUFSIZ)) > 0)
		write(FILEDES, buf, i);

	close(fd);
}



void
bmove(char	*from,
      char	*to,
      int	length)
{
	register char	*p, *q;
	register int	i;

	i = length;
	p = from;
	q = to;

	while (i-- > 0)
		*q++ = *p++;
}



int
bequal(char	*a,
       char	*b,
       int	len)
{
	register char	*p, *q;
	register int	i;

	i = len;
	p = a;
	q = b;

	while (*p && *q && (*p == *q) && --i > 0)
	{
		p++; q++;
	}
	return ((*p == *q) && i >= 0);
}

int
sequal(char	*a,
       char	*b)
{
	register char *p = a, *q = b;

	while (*p && *q && (*p == *q))
	{
		p++; q++;
	}
	return (*p == *q);
}

# ifdef GTTYN
char *
stypeof(char	*ttyid)
{
	static char	typebuf[50];
	register char	*PortType;
	register char	*PortName;
	register char	*TtyId;
	register char	*p;
	register FILE	*f;

	if (ttyid == NOTTY)
		return (DEFTYPE);
	f = fopen(GTTYN, "r");
	if (f == NULL)
		return (DEFTYPE);

	/* split off end of name */
	TtyId = ttyid;
	while (*ttyid)
		if (*ttyid++ == '/')
			TtyId = ttyid;

	/* scan the file */
	while (fgets(typebuf, sizeof typebuf, f) != NULL)
	{
		p = PortType = typebuf;
		while (*p && isgraph(*p))
			p++;
		*p++ = NULL;

		/* skip separator */
		while (*p && isspace(*p))
			p++;

		PortName = p;
		/* put NULL at end of name */
		while (*p && isgraph(*p))
			p++;
		*p = NULL;

		/* check match on port name */
		if (sequal(PortName, TtyId))	/* found it */
		{
# ifdef OLDDIALUP
			if (sequal(PortType, OLDDIALUP))
				PortType = DIALUP;
# endif

# ifdef OLDPLUGBOARD
			if (sequal(PortType, OLDPLUGBOARD))
				PortType = PLUGBOARD;
# endif

# ifdef OLDARPANET
			if (sequal(PortType, OLDARPANET))
				PortType = ARPANET;
# endif
			fclose (f);
			return(PortType);
		}
	}
	fclose (f);
	return (DEFTYPE);
}
# endif

#define	YES	1
#define	NO	0
#if !defined(SVR3) && !defined(__sgi)
/*
 * routine to output the string for the environment TERMCAP variable
 */
#define	WHITE(c)	(c == ' ' || c == '\t')
char delcap[128][2];
int ncap = 0;

wrtermcap(char *bp)
{
	char buf[CAPBUFSIZ];
	char *p = buf;
	char *tp;
	char *putbuf();
	int space, empty;

	/* discard names with blanks */
/** May not be desireable ? **/
	while (*bp && *bp != ':') {
		if (*bp == '|') {
			tp = bp+1;
			space = NO;
			while (*tp && *tp != '|' && *tp != ':') {
				space = (space || WHITE(*tp) );
				tp++;
			}
			if (space) {
				bp = tp;
				continue;
			}
		}
		*p++ = *bp++;
	}
/**/

	while (*bp) {
		switch (*bp) {
		case ':':	/* discard empty, cancelled  or dupl fields */
			tp = bp+1;
			empty = YES;
			while (*tp && *tp != ':') {
				empty = (empty && WHITE(*tp) );
				tp++;
			}
			if (empty || cancelled(bp+1)) {
				bp = tp;
				continue;
			}
			break;

		case ' ':	/* no spaces in output */
			p = putbuf(p, "\\040");
			bp++;
			continue;

		case '"':	/* no quotes in output */
			p = putbuf(p, "\\042");
			bp++;
			continue;

		case '\'':	/* no quotes in output */
			p = putbuf(p, "\\047");
			bp++;
			continue;

		case '\\':
		case '^':	/* anything following is OK */
			*p++ = *bp++;
		}
		*p++ = *bp++;
	}
	write (STDOUT, buf, p-buf);
}

cancelled(char	*cap)
{
	register int i;

	for (i = 0; i < ncap; i++)
	{
		if (cap[0] == delcap[i][0] && cap[1] == delcap[i][1])
			return (YES);
	}
	/* delete a second occurrance of the same capability */
	delcap[ncap][0] = cap[0];
	delcap[ncap][1] = cap[1];
	ncap++;
	return (cap[2] == '@');
}
#endif

char *
putbuf(char	*ptr,
       char	*str)
{
	while (*str)
		*ptr++ = *str++;
	return (ptr);
}


static
baudrate(char	*p)
{
	char buf[8];
	int i = 0;

	while (i < 7 && (isalnum(*p) || *p == '.'))
		buf[i++] = *p++;
	buf[i] = NULL;
	/*
	 * Use speeds table only for non-integer strings.
	 * Plain ints are converted directly.  This is the same algorithm
	 * getty uses.
	 */
	for (i=0; speeds[i].string; i++)
		if (sequal(speeds[i].string, buf))
			return (speeds[i].speed);
	if (strspn(buf,"0123456789") == strlen(buf))
		return atoi(buf);
	return (-1);
}

char *
mapped(char	*type,
       speed_t	speed)
{
	int	match;

# ifdef DEB
	printf ("spd:%d\n", speed);
	prmap();
# endif
	Map = map;
	while (Map->Ident)
	{
		if (*(Map->Ident) == NULL || bequal(Map->Ident, type, 4))
		{
			match = NO;
			switch (Map->Test)
			{
				case ANY:	/* no test specified */
				case ALL:
					match = YES;
					break;
				
				case GT:
					match = (speed > Map->Speed);
					break;

				case GE:
					match = (speed >= Map->Speed);
					break;

				case EQ:
					match = (speed == Map->Speed);
					break;

				case LE:
					match = (speed <= Map->Speed);
					break;

				case LT:
					match = (speed < Map->Speed);
					break;

				case NE:
					match = (speed != Map->Speed);
					break;
			}
			if (match)
				return (Map->Type);
		}
		Map++;
	}
	/* no match found; return given type */
	return (type);
}

# ifdef DEB
prmap()
{
	Map = map;
	while (Map->Ident)
	{
	printf ("%s t:%d s:%d %s\n",
		Map->Ident, Map->Test, Map->Speed, Map->Type);
	Map++;
	}
}
# endif

char *
nextarg(int	argc,
	char	*argv[])
{
	if (argc <= 0)
		fatal ("Too few args: ", *argv);
	if (*(*++argv) == '-')
		fatal ("Unexpected arg: ", *argv);
	return (*argv);
}

void
fatal (char	*mesg,
       char	*obj)
{
	prs (mesg);
	prs (obj);
	prc ('\n');
	prs (USAGE);
	flush();
	exit(1);
}
