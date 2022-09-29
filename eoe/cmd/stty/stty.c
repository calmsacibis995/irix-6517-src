/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* #ident	"@(#)stty:stty.c	1.11"	*/
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/stty/RCS/stty.c,v 1.24 1996/04/15 08:53:39 wtw Exp $"

#include "stdio.h"
#include "sys/types.h"
#include "sys/termio.h"
#include "errno.h"
#include "sys/stermio.h"
#ifdef __sgi
#include "string.h"
#endif

#define ASYNC	0
#define SYNC	1

#define CIBAUD_SHIFT 16
#if CBAUD << CIBAUD_SHIFT != CIBAUD
XXX;
#endif

extern char *getenv();
extern void exit();

struct mds {
	char	*string;
	int	set;
	int	reset;
};
						/* Control Modes */
struct mds cmodes[] = {
	"-parity", CS8, PARENB|CSIZE,
	"-evenp", CS8, PARENB|CSIZE,
	"-oddp", CS8, PARENB|PARODD|CSIZE,
	"parity", PARENB|CS7, PARODD|CSIZE,
	"evenp", PARENB|CS7, PARODD|CSIZE,
	"oddp", PARENB|PARODD|CS7, CSIZE,
	"parenb", PARENB, 0,
	"-parenb", 0, PARENB,
	"parodd", PARODD, 0,
	"-parodd", 0, PARODD,
	"cs8", CS8, CSIZE,
	"cs7", CS7, CSIZE,
	"cs6", CS6, CSIZE,
	"cs5", CS5, CSIZE,
	"cstopb", CSTOPB, 0,
	"-cstopb", 0, CSTOPB,
	"hupcl", HUPCL, 0,
	"hup", HUPCL, 0,
	"-hupcl", 0, HUPCL,
	"-hup", 0, HUPCL,
	"clocal", CLOCAL, 0,
	"-clocal", 0, CLOCAL,
	"cnew_rtscts", CNEW_RTSCTS, 0,
	"-cnew_rtscts", 0, CNEW_RTSCTS,
#if !defined(pdp11)
	"loblk", LOBLK, 0,
	"-loblk", 0, LOBLK,
#endif
	"cread", CREAD, 0,
	"-cread", 0, CREAD,
	"raw", CS8, (CSIZE|PARENB),
#ifdef __sgi
	"-raw", (CS8), (CSIZE|PARENB),
	"cooked", (CS8), (CSIZE|PARENB),
	"sane", (CS8|CREAD), (CSIZE|PARENB),
#else
	"-raw", (CS7|PARENB), CSIZE,
	"cooked", (CS7|PARENB), CSIZE,
	"sane", (CS7|PARENB|CREAD), (CSIZE|PARODD|CLOCAL),
#endif /* __sgi */
	0
};
						/* Input Modes */
struct mds imodes[] = {
	"ignbrk", IGNBRK, 0,
	"-ignbrk", 0, IGNBRK,
	"brkint", BRKINT, 0,
	"-brkint", 0, BRKINT,
	"ignpar", IGNPAR, 0,
	"-ignpar", 0, IGNPAR,
	"parmrk", PARMRK, 0,
	"-parmrk", 0, PARMRK,
	"inpck", INPCK, 0,
	"-inpck", 0,INPCK,
	"istrip", ISTRIP, 0,
	"-istrip", 0, ISTRIP,
	"inlcr", INLCR, 0,
	"-inlcr", 0, INLCR,
	"igncr", IGNCR, 0,
	"-igncr", 0, IGNCR,
	"icrnl", ICRNL, 0,
	"-icrnl", 0, ICRNL,
 	"-nl", 0, (ICRNL|INLCR|IGNCR),
	"nl", ICRNL, 0,
	"iuclc", IUCLC, 0,
	"-iuclc", 0, IUCLC,
	"lcase", IUCLC, 0,
	"-lcase", 0, IUCLC,
	"LCASE", IUCLC, 0,
	"-LCASE", 0, IUCLC,
	"ixon", IXON, 0,
	"-ixon", 0, IXON,
	"ixany", IXANY, 0,
	"-ixany", 0, IXANY,
	"ixoff", IXOFF, 0,
	"-ixoff", 0, IXOFF,
	"imaxbel", IMAXBEL, 0,
	"-imaxbel", 0, IMAXBEL,
	"raw", 0, -1,
	"-raw", (BRKINT|IGNPAR|ISTRIP|ICRNL|IXON), 0,
	"cooked", (BRKINT|IGNPAR|ISTRIP|ICRNL|IXON), 0,
	"sane", (BRKINT|IGNPAR|ISTRIP|ICRNL|IXON),
		(IGNBRK|PARMRK|INPCK|INLCR|IGNCR|IUCLC|IXOFF),
#ifdef __sgi
	"dec",     0, IXANY,
	"decctlq", 0, IXANY,
	"-decctlq", IXANY, 0,
#endif
	0
};
						/* Local Modes */
struct mds lmodes[] = {
	"isig", ISIG, 0,
	"-isig", 0, ISIG,
	"icanon", ICANON, 0,
	"-icanon", 0, ICANON,
#ifdef IEXTEN
	"iexten", IEXTEN, 0,
	"-iexten", 0, IEXTEN,
#endif
	"xcase", XCASE, 0,
	"-xcase", 0, XCASE,
	"lcase", XCASE, 0,
	"-lcase", 0, XCASE,
	"LCASE", XCASE, 0,
	"-LCASE", 0, XCASE,
	"echo", ECHO, 0,
	"-echo", 0, ECHO,
	"echoe", ECHOE, 0,
	"-echoe", 0, ECHOE,
	"echok", ECHOK, 0,
	"-echok", 0, ECHOK,
	"lfkc", ECHOK, 0,
	"-lfkc", 0, ECHOK,
	"echonl", ECHONL, 0,
	"-echonl", 0, ECHONL,
	"noflsh", NOFLSH, 0,
	"-noflsh", 0, NOFLSH,
	"raw", 0, (ISIG|ICANON|XCASE),
	"-raw", (ISIG|ICANON), 0,
	"cooked", (ISIG|ICANON), 0,
	"sane", (ISIG|ICANON|ECHO|ECHOK|ECHOE|ECHOCTL
#ifdef IEXTEN
		 |IEXTEN
#endif
		 ),
		(XCASE|ECHONL|NOFLSH|STFLUSH|STWRAP|STAPPL),
	"stflush", STFLUSH, 0,
	"-stflush", 0, STFLUSH,
	"stwrap", STWRAP, 0,
	"-stwrap", 0, STWRAP,
	"stappl", STAPPL, 0,
	"-stappl", 0, STAPPL,
	"echoctl", ECHOCTL, 0,
	"-echoctl", 0, ECHOCTL,
	"echoprt", ECHOPRT, 0,
	"-echoprt", 0, ECHOPRT,
	"echoke", ECHOKE, 0,
	"-echoke", 0, ECHOKE,
	"flusho", FLUSHO, 0,
	"-flusho", 0, FLUSHO,
	"pendin", PENDIN, 0,
	"-pendin", 0, PENDIN,
#ifdef __sgi
	"dec", (ECHOE|ECHOK), 0,
	"tostop", TOSTOP, 0,
	"-tostop", 0, TOSTOP,
#endif
	0,
};
						/* Output Modes */
struct mds omodes[] = {
	"opost", OPOST, 0,
	"-opost", 0, OPOST,
	"olcuc", OLCUC, 0,
	"-olcuc", 0, OLCUC,
	"lcase", OLCUC, 0,
	"-lcase", 0, OLCUC,
	"LCASE", OLCUC, 0,
	"-LCASE", 0, OLCUC,
	"onlcr", ONLCR, 0,
	"-onlcr", 0, ONLCR,
	"-nl", ONLCR, (OCRNL|ONLRET),
	"nl", 0, ONLCR,
	"ocrnl", OCRNL, 0,
	"-ocrnl",0, OCRNL,
	"onocr", ONOCR, 0,
	"-onocr", 0, ONOCR,
	"onlret", ONLRET, 0,
	"-onlret", 0, ONLRET,
	"fill", OFILL, OFDEL,
	"-fill", 0, OFILL|OFDEL,
	"nul-fill", OFILL, OFDEL,
	"del-fill", OFILL|OFDEL, 0,
	"ofill", OFILL, 0,
	"-ofill", 0, OFILL,
	"ofdel", OFDEL, 0,
	"-ofdel", 0, OFDEL,
	"cr0", CR0, CRDLY,
	"cr1", CR1, CRDLY,
	"cr2", CR2, CRDLY,
	"cr3", CR3, CRDLY,
	"tab0", TAB0, TABDLY,
	"tabs", TAB0, TABDLY,
	"tab1", TAB1, TABDLY,
	"tab2", TAB2, TABDLY,
	"tab3", TAB3, TABDLY,
	"-tabs", TAB3, TABDLY,
	"nl0", NL0, NLDLY,
	"nl1", NL1, NLDLY,
	"ff0", FF0, FFDLY,
	"ff1", FF1, FFDLY,
	"vt0", VT0, VTDLY,
	"vt1", VT1, VTDLY,
	"bs0", BS0, BSDLY,
	"bs1", BS1, BSDLY,
	"raw", 0, OPOST,
	"-raw", OPOST, 0,
	"cooked", OPOST, 0,
	"tty33", CR1, (CRDLY|TABDLY|NLDLY|FFDLY|VTDLY|BSDLY),
	"tn300", CR1, (CRDLY|TABDLY|NLDLY|FFDLY|VTDLY|BSDLY),
	"ti700", CR2, (CRDLY|TABDLY|NLDLY|FFDLY|VTDLY|BSDLY),
	"vt05", NL1, (CRDLY|TABDLY|NLDLY|FFDLY|VTDLY|BSDLY),
	"tek", FF1, (CRDLY|TABDLY|NLDLY|FFDLY|VTDLY|BSDLY),
	"tty37", (FF1|VT1|CR2|TAB1|NL1), (NLDLY|CRDLY|TABDLY|BSDLY|VTDLY|FFDLY),
	"sane", (OPOST|ONLCR), (OLCUC|OCRNL|ONOCR|ONLRET|OFILL|OFDEL|
			NLDLY|CRDLY|TABDLY|BSDLY|VTDLY|FFDLY),
	0,
};

char	*arg;					/* arg: ptr to mode to be set */
int	match;
char	*STTY="stty: ";
char	*USAGE="usage: stty [-ag] [modes]\n";
int	pitt = 0;
struct termio cb;
struct stio stio;
int term;
struct winsize win;
int setsize;

main(argc, argv)
char	*argv[];
{
	register i;

	if(ioctl(0, STGET, &stio) == -1) {
		term = ASYNC;
		if(ioctl(0, TCGETA, &cb) == -1) {
			perror(STTY);
			exit(2);
		}
		if(ioctl(0, TIOCGWINSZ, &win) < 0 && errno != EINVAL)
			/* sgi doesn't support on duarts; returning EINVAL */
			perror("TIOCGWINSZ");
	}
	else {
		term = SYNC;
		cb.c_cc[7] = (unsigned)stio.tab;
		cb.c_lflag = stio.lmode;
		cb.c_oflag = stio.omode;
		cb.c_iflag = stio.imode;
	}

	if (argc == 1) {
		prmodes();
		exit(0);
	}
	if ((argc == 2) && (argv[1][0] == '-') && (argv[1][2] == '\0'))
	switch(argv[1][1]) {
		case 'a':
			pramodes();
			exit(0);
		case 'g':
			prencode();
			exit(0);
		default:
			fprintf(stderr, "%s", USAGE);
			exit(2);
	}
	while(--argc > 0) {		/* set terminal modes for supplied options */

		arg = *++argv;
		match = 0;
		if (term == ASYNC) {
			if (eq("erase") && --argc)
				cb.c_cc[VERASE] = gct(*++argv);
			else if (eq("intr") && --argc)
				cb.c_cc[VINTR] = gct(*++argv);
			else if (eq("quit") && --argc)
				cb.c_cc[VQUIT] = gct(*++argv);
			else if (eq("eof") && --argc)
				cb.c_cc[VEOF] = gct(*++argv);
			else if (eq("min") && --argc)
				cb.c_cc[VMIN] = atoi(*++argv);
			else if (eq("eol") && --argc)
				cb.c_cc[VEOL] = gct(*++argv);
			else if (eq("time") && --argc)
				cb.c_cc[VTIME] = atoi(*++argv);
#ifdef __sgi
			else if (eq("lnext") && --argc)
				cb.c_cc[VLNEXT] = gct(*++argv);
			else if (eq("werase") && --argc)
				cb.c_cc[VWERASE] = gct(*++argv);
			else if (eq("rprnt") && --argc)
				cb.c_cc[VRPRNT] = gct(*++argv);
			else if (eq("flush") && --argc)
				cb.c_cc[VFLUSHO] = gct(*++argv);
			else if (eq("stop") && --argc)
				cb.c_cc[VSTOP] = gct(*++argv);
			else if (eq("start") && --argc)
				cb.c_cc[VSTART] = gct(*++argv);
			else if (eq("susp") && --argc)
				cb.c_cc[VSUSP] = gct(*++argv);
			else if (eq("dsusp") && --argc)
				cb.c_cc[VDSUSP] = gct(*++argv);
			else if(eq("dec")) {
				cb.c_cc[VERASE] = 0177;	/* DEL */
				cb.c_cc[VKILL] = CKILL;
				cb.c_cc[VINTR] = CTRL('c');
			}
			else if (eq("old-swtch") && --argc)
				/* Retain the ability to set swtch */
				cb.c_cc[VSWTCH] = gct(*++argv);
			else if (eq("swtch") && --argc)
				/* alias for susp */
				cb.c_cc[VSUSP] = gct(*++argv);
#endif /* __sgi */
			else if (eq("kill") && --argc)
				cb.c_cc[VKILL] = gct(*++argv);
#if !defined(pdp11) && !defined(__sgi)
			/* pdp11 doesn't have shl */
			else if (eq("swtch") && --argc)
				cb.c_cc[VSWTCH] = gct(*++argv);
#endif
			else if (eq("ek")) {
				cb.c_cc[VERASE] = CERASE;
				cb.c_cc[VKILL] = CKILL;
			}
			else if (eq("line") && --argc)
				cb.c_line = atoi(*++argv);
			else if (eq("raw")) {
				cb.c_cc[VMIN] = 1;
				cb.c_cc[VTIME] = 1;
			}
			else if (eq("-raw") | eq("cooked")) {
				cb.c_cc[VEOF] = CEOF;
				cb.c_cc[VEOL] = CNUL;
			}
			else if(eq("sane")) {
				cb.c_cc[VERASE] = CERASE;
				cb.c_cc[VKILL] = CKILL;
				cb.c_cc[VQUIT] = CQUIT;
				cb.c_cc[VINTR] = CINTR;
				cb.c_cc[VEOF] = CEOF;
				cb.c_cc[VEOL] = CNUL;
#ifdef __sgi
				if (cb.c_line == LDISC1) {
				    cb.c_cc[VLNEXT] = CLNEXT;
				    cb.c_cc[VWERASE] = CWERASE;
				    cb.c_cc[VRPRNT] = CRPRNT;
				    cb.c_cc[VFLUSHO] = CFLUSH;
				    cb.c_cc[VSTOP] = CSTOP;
				    cb.c_cc[VSTART] = CSTART;
				}
#endif /* __sgi */
				/* SWTCH purposely not set */
			}
			else if (eq("ispeed") && --argc) {
			    ++argv;
			    if (strspn(*argv,"0123456789") == strlen(*argv)) {
				speed_t speed = (speed_t)strtoul(*argv,0,0);
				if (speed == B0) {
				    cb.c_ispeed = cb.c_ospeed;
				}
				else {
				    cb.c_ispeed = speed;
				}
			    }
			}
			else if (eq("ospeed") && --argc) {
			    ++argv;
			    if (strspn(*argv,"0123456789") == strlen(*argv)) {
				cb.c_ospeed = (speed_t)strtoul(*argv,0,0);
			    }
			}

			else if (eq("rows") && --argc) {
				win.ws_row = atoi(*++argv);
				setsize++;
			}
			else if ((eq("cols") || eq("columns")) && --argc) {
				win.ws_col = atoi(*++argv);
				setsize++;
			}
			else
			    eq("--");
			/*
			 * a number by itself, with no option name,
			 * is considered a baud rate for both input and output
			 */
             		if (strspn(arg,"0123456789") == strlen(arg)) {
			    cb.c_ospeed = cb.c_ispeed = (speed_t)strtoul(arg,0,0);
			    match++;
			}

		}
		if (term == SYNC && eq("ctab") && --argc)
			cb.c_cc[7] = gct(*++argv);
		for(i=0; imodes[i].string; i++)
			if(eq(imodes[i].string)) {
				cb.c_iflag &= ~imodes[i].reset;
				cb.c_iflag |= imodes[i].set;
			}
		for(i=0; omodes[i].string; i++)
			if(eq(omodes[i].string)) {
				cb.c_oflag &= ~omodes[i].reset;
				cb.c_oflag |= omodes[i].set;
			}
		if(term == SYNC && eq("sane"))
			cb.c_oflag |= TAB3;
		for(i=0; cmodes[i].string; i++)
			if(eq(cmodes[i].string)) {
				cb.c_cflag &= ~cmodes[i].reset;
				cb.c_cflag |= cmodes[i].set;
			}
		for(i=0; lmodes[i].string; i++)
			if(eq(lmodes[i].string)) {
				cb.c_lflag &= ~lmodes[i].reset;
				cb.c_lflag |= lmodes[i].set;
			}
		if(!match)
			if(!encode()) {
				fprintf(stderr, "unknown mode: %s\n", arg);
				exit(2);
			}
	}
	if (term == ASYNC) {
		if(ioctl(0, TCSETAW, &cb) == -1) {
			perror(STTY);
			exit(2);
		}
		if(setsize && ioctl(0, TIOCSWINSZ, &win) < 0)
			perror("TIOCSWINSZ");
	} else {
		stio.imode = cb.c_iflag;
		stio.omode = cb.c_oflag;
		stio.lmode = cb.c_lflag;
		stio.tab = cb.c_cc[7];
		if (ioctl(0, STSET, &stio) == -1) {
			perror(STTY);
			exit(2);
		}
	}
	exit(0);	/*NOTREACHED*/
}

eq(string)
char *string;
{
	register i;

	if(!arg)
		return(0);
	i = 0;
loop:
	if(arg[i] != string[i])
		return(0);
	if(arg[i++] != '\0')
		goto loop;
	match++;
	return(1);
}

prmodes()				/* print modes, no options, argc is 1 */
{
	register m;

	if (term == SYNC) {
		m = stio.imode;
		if (m & IUCLC) (void) printf ("iuclc ");
		else (void) printf ("-iuclc ");
		m = stio.omode;
		if (m & OLCUC) (void) printf ("olcuc ");
		else (void) printf ("-olcuc ");
		if (m & TAB3) (void) printf ("tab3 ");
		m = stio.lmode;
		if (m & XCASE) (void) printf ("xcase ");
		else (void) printf ("-xcase ");
		if (m & STFLUSH) (void) printf ("stflush ");
		else (void) printf ("-stflush ");
		if (m & STWRAP) (void) printf ("stwrap ");
		else (void) printf ("-stwrap ");
		if (m & STAPPL) (void) printf ("stappl ");
		else (void) printf ("-stappl ");
		(void) printf ("\n");
	}
	if (term == ASYNC) {
		m = cb.c_cflag;
		prspeed(cb.c_ospeed, cb.c_ispeed);
		if (m&PARENB)
			if (m&PARODD)
				(void) printf("oddp ");
			else
				(void) printf("evenp ");
		else
			(void) printf("-parity ");
		if(((m&PARENB) && !(m&CS7)) || (!(m&PARENB) && !(m&CS8)))
			(void) printf("cs%c ",'5'+(m&CSIZE)/CS6);
		if (m&CSTOPB)
			(void) printf("cstopb ");
		if (m&HUPCL)
			(void) printf("hupcl ");
		if (!(m&CREAD))
			(void) printf("cread ");
		if (m&CLOCAL)
			(void) printf("clocal ");
#if !defined(pdp11)
		if (m&LOBLK)
			(void) printf("loblk");
#endif
		(void) printf("\n");
		if(cb.c_line != 0)
			(void) printf("line = %d; ", cb.c_line);
		if(cb.c_cc[VINTR] != CINTR)
			pit(cb.c_cc[VINTR], "intr", "; ");
		if(cb.c_cc[VQUIT] != CQUIT)
			pit(cb.c_cc[VQUIT], "quit", "; ");
		if(cb.c_cc[VERASE] != CERASE)
			pit(cb.c_cc[VERASE], "erase", "; ");
		if(cb.c_cc[VKILL] != CKILL)
			pit(cb.c_cc[VKILL], "kill", "; ");
		if(cb.c_cc[VEOF] != CEOF)
			pit(cb.c_cc[VEOF], "eof", "; ");
		if(cb.c_cc[VEOL] != CNUL)
			pit(cb.c_cc[VEOL], "eol", "; ");
#if !defined(pdp11) && !defined(__sgi)
		if(cb.c_cc[VSWTCH] != CSWTCH)
			pit(cb.c_cc[VSWTCH], "swtch", "; ");
#endif
#ifdef __sgi
		if(cb.c_cc[VSWTCH] != CSWTCH)
			pit(cb.c_cc[VSWTCH], "old-swtch", "; ");
		if(cb.c_cc[VSUSP] != CSUSP)
			pit(cb.c_cc[VSUSP], "susp", "; ");
		if (cb.c_line == LDISC1) {
		    if(cb.c_cc[VLNEXT] != CLNEXT)
			    pit(cb.c_cc[VLNEXT], "lnext", "; ");
		    if(cb.c_cc[VWERASE] != CWERASE)
			    pit(cb.c_cc[VWERASE], "werase", "; ");
		    if(cb.c_cc[VRPRNT] != CRPRNT)
			    pit(cb.c_cc[VRPRNT], "rprnt", "; ");
		    if(cb.c_cc[VFLUSHO] != CFLUSH)
			    pit(cb.c_cc[VFLUSHO], "flush", "; ");
		    if(cb.c_cc[VSTOP] != CSTOP)
			    pit(cb.c_cc[VSTOP], "stop", "; ");
		    if(cb.c_cc[VSTART] != CSTART)
			    pit(cb.c_cc[VSTART], "start", "; ");
		    if(cb.c_cc[VDSUSP] != CDSUSP)
			    pit(cb.c_cc[VDSUSP], "dsusp", "; ");
		}
#endif /* __sgi */
		if(pitt) (void) printf("\n");
		m = cb.c_iflag;
		if (m&IGNBRK)
			(void) printf("ignbrk ");
		else if (m&BRKINT)
			(void) printf("brkint ");
		if (!(m&INPCK))
			(void) printf("-inpck ");
		else if (m&IGNPAR)
			(void) printf("ignpar ");
		if (m&PARMRK)
			(void) printf("parmrk ");
		if (!(m&ISTRIP))
			(void) printf("-istrip ");
		if (m&INLCR)
			(void) printf("inlcr ");
		if (m&IGNCR)
			(void) printf("igncr ");
		if (m&ICRNL)
			(void) printf("icrnl ");
		if (m&IUCLC)
			(void) printf("iuclc ");
		if (!(m&IXON))
			(void) printf("-ixon ");
		else if (!(m&IXANY))
			(void) printf("-ixany ");
		if (m&IXOFF)
			(void) printf("ixoff ");
		if (m&IMAXBEL)
			(void) printf("imaxbel ");
		m = cb.c_oflag;
		if (!(m&OPOST))
			(void) printf("-opost ");
		else {
		if (m&OLCUC)
			(void) printf("olcuc ");
		if (m&ONLCR)
			(void) printf("onlcr ");
		if (m&OCRNL)
			(void) printf("ocrnl ");
		if (m&ONOCR)
			(void) printf("onocr ");
		if (m&ONLRET)
			(void) printf("onlret ");
		if (m&OFILL)
			if (m&OFDEL)
				(void) printf("del-fill ");
			else
				(void) printf("nul-fill ");
		delay((m&CRDLY)/CR1, "cr");
		delay((m&NLDLY)/NL1, "nl");
		delay((m&TABDLY)/TAB1, "tab");
		delay((m&BSDLY)/BS1, "bs");
		delay((m&VTDLY)/VT1, "vt");
		delay((m&FFDLY)/FF1, "ff");
		}
		(void) printf("\n");
		m = cb.c_lflag;
		if (!(m&ISIG))
			(void) printf("-isig ");
		if (!(m&ICANON))
			(void) printf("-icanon ");
#ifdef IEXTEN
		if (!(m&IEXTEN))
			(void) printf("-iexten ");
#endif
		if (m&XCASE)
			(void) printf("xcase ");
		(void) printf("-echo "+((m&ECHO)!=0));
		(void) printf("-echoe "+((m&ECHOE)!=0));
		(void) printf("-echok "+((m&ECHOK)!=0));
		if (m&ECHOKE)
			(void) printf("echoke ");
		if (m&ECHONL)
			(void) printf("echonl ");
		if (!(m&ECHOCTL))
			(void) printf("-echoctl ");
		if ((m&ECHOPRT))
			(void) printf("echoprt ");
		if (m&NOFLSH)
			(void) printf("noflsh ");
		if (m&FLUSHO)
			(void) printf("flusho ");
		if (m&PENDIN)
			(void) printf("pendin ");
#ifdef __sgi
		if (m&TOSTOP)
			(void) printf("tostop");
#endif
		(void) printf("\n");
	}
}

pramodes()				/* print all modes, -a option */
{
	register m;

	if(term == ASYNC) {
		prspeed(cb.c_ospeed, cb.c_ispeed);
		(void) printf("line = %d; ", cb.c_line);
		(void) printf("%d rows; %d columns", win.ws_row, win.ws_col);
		(void) printf("\n");
		pit(cb.c_cc[VINTR], "intr", "; ");
		pit(cb.c_cc[VQUIT], "quit", "; ");
		pit(cb.c_cc[VERASE], "erase", "; ");
		pit(cb.c_cc[VKILL], "kill", "; ");
		pit(cb.c_cc[VEOF], "eof", "; ");
		pit(cb.c_cc[VEOL], "eol", "; ");
#if !defined(pdp11) && !defined(__sgi)
		pit(cb.c_cc[VSWTCH], "swtch", "; ");
#endif
#ifdef __sgi
		pit(cb.c_cc[VSWTCH], "old-swtch", "; ");
		pit(cb.c_cc[VSUSP], "susp", "\n");
		if (cb.c_line == LDISC1) {
		    pit(cb.c_cc[VLNEXT], "lnext", "; ");
		    pit(cb.c_cc[VWERASE], "werase", "; ");
		    pit(cb.c_cc[VRPRNT], "rprnt", "; ");
		    pit(cb.c_cc[VFLUSHO], "flush", "; ");
		    pit(cb.c_cc[VSTOP], "stop", "; ");
		    pit(cb.c_cc[VSTART], "start", "; ");
		    pit(cb.c_cc[VDSUSP], "dsusp", "\n");
		}
#endif /* __sgi */
	} else
		pit((unsigned)stio.tab, "ctab", "\n");
	m = cb.c_cflag;
	(void) printf("-parenb "+((m&PARENB)!=0));
	(void) printf("-parodd "+((m&PARODD)!=0));
	(void) printf("cs%c ",'5'+(m&CSIZE)/CS6);
	(void) printf("-cstopb "+((m&CSTOPB)!=0));
	(void) printf("-hupcl "+((m&HUPCL)!=0));
	(void) printf("-cread "+((m&CREAD)!=0));
	(void) printf("-clocal "+((m&CLOCAL)!=0));
	(void) printf("-cnew_rtscts "+((m&CNEW_RTSCTS)!=0));

#if !defined(pdp11)
	(void) printf("-loblk "+((m&LOBLK)!=0));
#endif

	(void) printf("\n");
	m = cb.c_iflag;
	(void) printf("-ignbrk "+((m&IGNBRK)!=0));
	(void) printf("-brkint "+((m&BRKINT)!=0));
	(void) printf("-ignpar "+((m&IGNPAR)!=0));
	(void) printf("-parmrk "+((m&PARMRK)!=0));
	(void) printf("-inpck "+((m&INPCK)!=0));
	(void) printf("-istrip "+((m&ISTRIP)!=0));
	(void) printf("-inlcr "+((m&INLCR)!=0));
	(void) printf("-igncr "+((m&IGNCR)!=0));
	(void) printf("-icrnl "+((m&ICRNL)!=0));
	(void) printf("-iuclc "+((m&IUCLC)!=0));
	(void) printf("\n");
	(void) printf("-ixon "+((m&IXON)!=0));
	(void) printf("-ixany "+((m&IXANY)!=0));
	(void) printf("-ixoff "+((m&IXOFF)!=0));
	(void) printf("-imaxbel "+((m&IMAXBEL)!=0));
	(void) printf("\n");
	m = cb.c_lflag;
	(void) printf("-isig "+((m&ISIG)!=0));
	(void) printf("-icanon "+((m&ICANON)!=0));
#ifdef IEXTEN
	(void) printf("-iexten "+((m&IEXTEN)!=0));
#endif
	(void) printf("-xcase "+((m&XCASE)!=0));
	(void) printf("-echo "+((m&ECHO)!=0));
	(void) printf("-echoe "+((m&ECHOE)!=0));
	(void) printf("-echok "+((m&ECHOK)!=0));
	(void) printf("-echoke "+((m&ECHOKE)!=0));
	(void) printf("-echoctl "+((m&ECHOCTL)!=0));
	(void) printf("-echoprt "+((m&ECHOPRT)!=0));
	(void) printf("-echonl "+((m&ECHONL)!=0));
	(void) printf("-noflsh "+((m&NOFLSH)!=0));
	(void) printf("-flusho "+((m&FLUSHO)!=0));
	(void) printf("-pendin "+((m&PENDIN)!=0));
#ifdef __sgi
	(void) printf("-tostop "+((m&TOSTOP)!=0));
#endif /* __sgi */
	if(term == SYNC) {
		(void) printf("-stflush "+((m&STFLUSH)!=0));
		(void) printf("-stwrap "+((m&STWRAP)!=0));
		(void) printf("-stappl "+((m&STAPPL)!=0));
	}
	(void) printf("\n");
	m = cb.c_oflag;
	(void) printf("-opost "+((m&OPOST)!=0));
	(void) printf("-olcuc "+((m&OLCUC)!=0));
	(void) printf("-onlcr "+((m&ONLCR)!=0));
	(void) printf("-ocrnl "+((m&OCRNL)!=0));
	(void) printf("-onocr "+((m&ONOCR)!=0));
	(void) printf("-onlret "+((m&ONLRET)!=0));
	(void) printf("-ofill "+((m&OFILL)!=0));
	(void) printf("-ofdel "+((m&OFDEL)!=0));
	delay((m&CRDLY)/CR1, "cr");
	delay((m&NLDLY)/NL1, "nl");
	delay((m&TABDLY)/TAB1, "tab");
	delay((m&BSDLY)/BS1, "bs");
	delay((m&VTDLY)/VT1, "vt");
	delay((m&FFDLY)/FF1, "ff");
	(void) printf("\n");
}
				/* get pseudo control characters from terminal */
gct(cp)				/* and convert to internal representation      */
register char *cp;
{
	register c;

	if (!strcmp(cp, "undef"))
	    return(0377);

	c = *cp++;
	if (c == '^') {
		c = *cp;
		if (c == '?')
			c = CINTR;		/* map '^?' to DEL */
		else if (c == '-')
			c = 0377;		/* map '^-' to 0377, i.e. undefined */
		else
			c &= 037;
	}
	return(c);
}

pit(what, itsname, sep)		/*print function for prmodes() and pramodes() */
	unsigned char what;
	char *itsname, *sep;
{

	pitt++;
	(void) printf("%s = ", itsname);
	if (what == 0377) {
		(void) printf("<undef>%s", sep);
		return;
	}
	if (what & 0200) {
		(void) printf("-");
		what &= ~ 0200;
	}
	if (what == CINTR) {
		(void) printf("DEL%s", sep);
		return;
	} else if (what < ' ') {
		(void) printf("^");
#ifdef __sgi
		what += '@';
#else
		what += '`';
#endif
	}
	(void) printf("%c%s", what, sep);
}

delay(m, s)
char *s;
{
	if(m)
		(void) printf("%s%d ", s, m);
}

prspeed(ospeed, ispeed)
speed_t ospeed, ispeed;
{
    if (ispeed == ospeed)
	printf("speed %d baud; ", ospeed);
    else
	printf("ispeed %d baud; ospeed %d baud; ", ispeed, ospeed);
}

					/* print current settings for use with  */
prencode()				/* another stty cmd, used for -g option */
{
#ifdef __sgi
	/*
	 * Print the entire termio structure. The first twelve fields
	 * are compatible with the previous version, the remaining fields
	 * contain the rest of the c_cc array and the line discipline.
	 */
	register int i;

	(void) printf("%x:%x:%x:%x:%x:%x", cb.c_iflag,cb.c_oflag,
		cb.c_cflag,cb.c_lflag,cb.c_ospeed,cb.c_ispeed);
	for (i = 0; i < (NCCS); i++) {
	    (void) printf(":%x", cb.c_cc[i]);
	}
	(void) printf(":%x\n", cb.c_line);
#else
	(void) printf("%x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x\n",
	cb.c_iflag,cb.c_oflag,cb.c_cflag,cb.c_lflag,cb.c_cc[0],
	cb.c_cc[1],cb.c_cc[2],cb.c_cc[3],cb.c_cc[4],cb.c_cc[5],
	cb.c_cc[6],cb.c_cc[7]);
#endif /* __sgi */
}

encode()
{
#ifdef __sgi
#define NUM_CODES	(6 + (NCCS) + 1)
	int grab[NUM_CODES];
	int i, count = 0;
	char *ap, *c;

	ap = arg;
	while (*ap != '\n' && *ap != '\0') {
	    if (sscanf(ap, "%x", &grab[count]) != 1) {
		break;
	    }
	    count++;
	    c = strchr(ap, ':');
	    if (c == (char *) NULL) {
		break;
	    }
	    ap = c + 1; /* skip past : */
	}

	if (count != 12 && count != NUM_CODES) {
	    return(0);
	}

	cb.c_iflag = (ushort) grab[0];
	cb.c_oflag = (ushort) grab[1];
	cb.c_cflag = (ushort) grab[2];
	cb.c_lflag = (ushort) grab[3];
	cb.c_ospeed = (speed_t) grab[4];
	cb.c_ispeed = (speed_t) grab[5];

	if (count == NUM_CODES) {
	    for(i=0; i<(NCCS); i++) {
		cb.c_cc[i] = (char) grab[i+6];
	    }
	    cb.c_line = (char) grab[i+6];
	} else {
	    for(i=0; i< NCC; i++) {
		cb.c_cc[i] = (char) grab[i+6];	       /* XXX? */
	    }
	}
	return(1);
#else
	int grab[12], i;
	i = sscanf(arg, "%x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x",
	&grab[0],&grab[1],&grab[2],&grab[3],&grab[4],&grab[5],&grab[6],
	&grab[7],&grab[8],&grab[9],&grab[10],&grab[11]);

	if(i != 12) return(0);

	cb.c_iflag = (ushort) grab[0];
	cb.c_oflag = (ushort) grab[1];
	cb.c_cflag = (ushort) grab[2];
	cb.c_lflag = (ushort) grab[3];

	for(i=0; i<8; i++)
		cb.c_cc[i] = (char) grab[i+4];
	return(1);
#endif /* __sgi */
}	
