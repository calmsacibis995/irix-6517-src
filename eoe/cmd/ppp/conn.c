/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)uucp:conn.c	2.17"

#include "uucp.h"

static char _ProtoStr[40] = "";	/* protocol string from Systems file entry */
extern jmp_buf Sjbuf;

int	Modemctrl;
static int ioctlok;
static struct termio ttybuf;

#ifdef sgi
#define ABORT -2
#define MAX_ABORTS 5
int	Aborts,	AbortSeen;
char	*AbortStr[MAX_ABORTS];
int	AbortLen[MAX_ABORTS];

static int expect(char*, int);
static void sendthem(char *, int, char *, char *);
static int wrchr(int, char *, int);
static int wrstr(int, char *, int, int);
static void nap(long);

int conn_trycalls = TRYCALLS;
void alarmtr();
#else
int alarmtr();
#endif
static void getProto(char *);
extern int getto(char *[]);
static int finds(char *, char *[], int);

#ifndef STANDALONE
static int ifdate(char *);
#endif
static int classmatch(char *[], char *[]);
static int rddev(char *, char *[], char *, int);

extern struct caller caller[];

/* Needed for cu for force which line will be used */
#ifdef STANDALONE
extern char *Myline;
#endif /* STANDALONE */

/*
 * conn - place a telephone call to system and login, etc.
 *
 * return codes:
 *	FAIL - connection failed
 *	>0  - file no.  -  connect ok
 * When a failure occurs, Uerror is set.
 */

int
conn(char *system)
{
#ifdef STANDALONE
	int fn = FAIL;
	char *flds[F_MAX+1];

	CDEBUG(4, "conn(%s)\n", system);
	Uerror = 0;
	while (finds(system, flds, F_MAX) > 0) {
		fn = getto(flds);
		CDEBUG(4, "getto ret %d\n", fn);
		if (fn < 0)
		    continue;

		sysreset();
		return(fn);
	}
#else /* !STANDALONE */
	int nf, fn = FAIL;
	char *flds[F_MAX+1];

	CDEBUG(4, "conn(%s)\n", system);
	Uerror = 0;
	while ((nf = finds(system, flds, F_MAX)) > 0) {
		fn = getto(flds);
		CDEBUG(4, "getto ret %d\n", fn);
		if (fn < 0)
		    continue;


		if(chat(nf - F_LOGIN, flds + F_LOGIN, fn,"","") == SUCCESS) {
			sysreset();
			return(fn); /* successful return */
		}

		/* login failed */
		DEBUG(6, "close caller (%d)\n", fn);
		close(fn);
		if (Dc[0] != NULLCHAR) {
			DEBUG(6, "delock(%s)\n", Dc);
			delock(Dc);
		}
	}
#endif /* STANDALONE */

	/* finds or getto failed */
	sysreset();
	CDEBUG(1, "Call Failed: %s\n", UERRORTEXT);
	return(FAIL);
}

/*
 * getto - connect to remote machine
 *
 * return codes:
 *	>0  -  file number - ok
 *	FAIL  -  failed
 */

getto(char *flds[])
{
	char *dev[D_MAX+2], devbuf[BUFSIZ];
	register int status;
	register int dcf = -1;
	int reread = 0;
	int tries = 0;	/* count of call attempts - for limit purposes */

	CDEBUG(1, "Device Type %s wanted\n", flds[F_TYPE]);
	Uerror = 0;
	while (tries < conn_trycalls) {
		if ((status=rddev(flds[F_TYPE], dev, devbuf, D_MAX)) == FAIL) {
			if (tries == 0 || ++reread >= conn_trycalls)
				break;
			devreset();
			continue;
		}
		/* check class, check (and possibly set) speed */
		if (classmatch(flds, dev) != SUCCESS)
			continue;
		if ((dcf = processdev(flds, dev)) >= 0)
			break;

		switch(Uerror) {
		case SS_CANT_ACCESS_DEVICE:
		case SS_DEVICE_FAILED:
		case SS_LOCKED_DEVICE:
			break;
		default:
			tries++;
			break;
		}
	}
	devreset();	/* reset devices file(s) */
	if (status == FAIL && !Uerror) {
		CDEBUG0(1, "Requested Device Type Not Found\n");
		Uerror = SS_NO_DEVICE;
	}
	return(dcf);
}

/*
 * classmatch - process 'Any' in Devices and Systems and
 *	determine the correct speed, or match for ==
 */

static int
classmatch(char *flds[], char *dev[])
{
	/* check class, check (and possibly set) speed */
	if (EQUALS(flds[F_CLASS], "Any")
	   && EQUALS(dev[D_CLASS], "Any")) {
		dev[D_CLASS] = DEFAULT_BAUDRATE;
		return(SUCCESS);
	} else if (EQUALS(dev[F_CLASS], "Any")) {
		dev[D_CLASS] = flds[F_CLASS];
		return(SUCCESS);
	} else if (EQUALS(flds[F_CLASS], "Any") ||
	EQUALS(flds[F_CLASS], dev[D_CLASS]))
		return(SUCCESS);
	else
		return(FAIL);
}


/***
 *	rddev - find and unpack a line from device file for this caller type
 *	lines starting with whitespace of '#' are comments
 *
 *	return codes:
 *		>0  -  number of arguments in vector - succeeded
 *		FAIL - EOF
 ***/
static int
rddev(char *type, char *dev[], char *buf, int devcount)
{
	char *commap, d_type[BUFSIZ];
	int na;

	while (getdevline(buf, BUFSIZ)) {
		if (buf[0] == ' ' || buf[0] == '\t'
		    ||  buf[0] == '\n' || buf[0] == '\0' || buf[0] == '#')
			continue;
		na = getargs(buf, dev, devcount);
		ASSERT(na >= D_CALLER, "BAD LINE", buf, na);

		if ( strncmp(dev[D_LINE],"/dev/",5) == 0 ) {
			/* since cu (altconn()) strips off leading */
			/* "/dev/",  do the same here.  */
			strcpy(dev[D_LINE], &(dev[D_LINE][5]) );
		}

		/* may have ",M" subfield in D_LINE */
		if ( (commap = strchr(dev[D_LINE], ',')) != (char *)NULL ) {
			if ( strcmp( commap, ",M") == SAME )
				Modemctrl = TRUE;
			*commap = '\0';
		}

/* For cu -- to force the requested line to be used */
#ifdef STANDALONE
		if ((Myline != NULL) && (!EQUALS(Myline, dev[D_LINE])) )
		    continue;
#endif /* STANDALONE */

		bsfix(dev);	/* replace \X fields */

		/*
		 * D_TYPE field may have protocol subfield, which
		 * must be pulled off before comparing to desired type.
		 */
		(void)strcpy(d_type, dev[D_TYPE]);
		if ((commap = strchr(d_type, ',')) != (char *)NULL )
			*commap = '\0';
		if (EQUALS(d_type, type)) {
			getProto( dev[D_TYPE] );
			return(na);
		}
	}
	return(FAIL);
}


/*
 * finds	- set system attribute vector
 *
 * input:
 *	fsys - open Systems file descriptor
 *	sysnam - system name to find
 * output:
 *	flds - attibute vector from Systems file
 *	fldcount - number of fields in flds
 * return codes:
 *	>0  -  number of arguments in vector - succeeded
 *	FAIL - failed
 * Uerror set:
 *	0 - found a line in Systems file
 *	SS_BADSYSTEM - no line found in Systems file
 *	SS_TIME_WRONG - wrong time to call
 */

static
finds(char *sysnam, char *flds[], int fldcount)
{
	static char info[BUFSIZ];
	int na;

	/* format of fields
	 *	0 name;
	 *	1 time
	 *	2 acu/hardwired
	 *	3 speed
	 *	etc
	 */
	if (sysnam == 0 || *sysnam == 0 ) {
		Uerror = SS_BADSYSTEM;
		return(FAIL);
	}

	while (getsysline(info, sizeof(info))) {
		if (*sysnam != *info || *info == '#')	/* speedup */
			continue;
		na = getargs(info, flds, fldcount);
		bsfix(flds);	/* replace \X fields */
		if ( !EQUALSN(sysnam, flds[F_NAME], SYSNSIZE))
			continue;
		if(na < 4) {
			/* otherwise can get core dumps later... */
			CDEBUG(1, "Systems line for %s has fewer than 4 fields\n", info);
			continue;
		}
#ifdef STANDALONE
		*_ProtoStr = NULLCHAR;
		getProto(flds[F_TYPE]);
		Uerror = 0;
		return(na);	/* FOUND OK LINE */
#else /* !STANDALONE */
		if (ifdate(flds[F_TIME])) {
			/*  found a good entry  */
			*_ProtoStr = NULLCHAR;
			getProto(flds[F_TYPE]);
			Uerror = 0;
			return(na);	/* FOUND OK LINE */
		}
		CDEBUG(1, "Wrong Time To Call: %s\n", flds[F_TIME]);
		Uerror = SS_TIME_WRONG;
#endif /* STANDALONE */
	}
	if (!Uerror)
		Uerror = SS_BADSYSTEM;
	return(FAIL);
}

/*
 * getProto - get the protocol letters from the input string.
 * input:
 *	str - string from Systems file (flds[F_TYPE])--the ,
 *		delimits the protocol string
 *		e.g. ACU,g or DK,d
 * output:
 *	str - the , (if present) will be replaced with NULLCHAR
 *	global ProtoStr will be modified
 * return:  none
 */

static void
getProto(char *str)
{
	register char *p;
	if ( (p=strchr(str, ',')) != NULL) {
		*p = NULLCHAR;
		if( *_ProtoStr == NULLCHAR )
			(void) strcpy(_ProtoStr, p+1);
		else
			(void) strcat(_ProtoStr, p+1);
		DEBUG(7, "ProtoStr = %s\n", _ProtoStr);
	}
}

/*
 * check for a specified protocol selection string
 * return:
 *	protocol string pointer
 *	NULL if none specified for LOGNAME
 */
char *
protoString(void)
{
	return(_ProtoStr[0] == NULLCHAR ? NULL : _ProtoStr);
}

/*
 * chat -	do conversation
 * input:
 *	nf - number of fields in flds array
 *	flds - fields from Systems file
 *	fn - write file number
 *	phstr1 - phone number to replace \D
 *	phstr2 - phone number to replace \T
 *
 *	return codes:  0  |  FAIL
 */

chat(int nf, char *flds[], int fn, char *phstr1, char *phstr2)
{
	char *want, *altern;
#ifdef sgi
	char *dash2;
#else
	extern char *index();
#endif
	int k, ok;

	/* init ttybuf - used in sendthem() */
	if ( (*Ioctl)(fn, TCGETA, &ttybuf) == 0 ) {
		ioctlok = 1;
	} else {
		DEBUG(7, "chat: TCGETA failed, errno %d\n", errno);
		ioctlok = 0;
	}

#ifdef sgi
	Aborts = 0;
#endif
	for (k = 0; k < nf; k += 2) {
		want = flds[k];
		ok = FAIL;
		while (ok != 0) {
#ifdef sgi
			/* this should never happen,
			 * but it seems to on some occasions */
			if (!want)
				break;
#endif
			altern = index(want, '-');
#ifdef sgi
			/* do not core dump if there is only one dash */
			if (altern) {
				dash2 = index(altern+1, '-');
				if (!dash2) {
					altern = 0;
				} else {
					*altern++ = NULLCHAR;
					*dash2++ = NULLCHAR;
				}
			}
			/* ABORT keyword */
			if (strcmp(want, "ABORT") == 0 && !altern) {
				if (flds[k+1] == 0
				    || EQUALS(flds[k+1], "\"\"")) {
					Aborts = 0;
					CDEBUG0(4, "turn off ABORT\n");
				} else if (Aborts >= MAX_ABORTS) {
					CDEBUG(1,"excess ABORT \"%s\"\n",
					       flds[k+1]);
				} else {
					CDEBUG(4, "ABORT ON: \"%s\"\n",
					       flds[k+1]);
					AbortStr[Aborts] = flds[k+1];
					AbortLen[Aborts] = strlen(flds[k+1]);
					Aborts++;
				}
				goto nextfield;
			}
#else
			if (altern != NULL)
				*altern++ = NULLCHAR;
#endif
			ok = expect(want, fn);
			if (ok == 0)
				break;
			if (ok == FAIL) {
			    if (altern == NULL) {
				    Uerror = SS_LOGIN_FAILED;
				    logent(UERRORTEXT, "FAILED");
				    return(FAIL);
			    }
#ifdef sgi
			    want = dash2;
#else
			    want = index(altern, '-');
			    if (want != NULL)
				    *want++ = NULLCHAR;
#endif
			    sendthem(altern, fn, phstr1, phstr2);
#ifdef sgi
			} else if (ok == ABORT) {
				char ebuf[80];
				Uerror = SS_CHAT_FAILED;
				(void)sprintf(ebuf, "%.80s",
					      (AbortSeen < Aborts
					       ? AbortStr[AbortSeen] : "?"));
				logent(ebuf, "ABORT");
				return ABORT;
#endif
			}
		}
#ifdef sgi
		sginap(HZ/4);
#else
		sleep(2);
#endif
		if (flds[k+1])
		    sendthem(flds[k+1], fn, phstr1, phstr2);
nextfield: ;
	}
	return(0);
}

/***
 *	expect(str, fn)	look for expected string
 *	char *str;
 *
 *	return codes:
 *		0  -  found
 *		FAIL  -  lost line or too many characters read
 *		some character  -  timed out
 */
static int
expect(char *str, int fn)
{
	char rdvec[2048];
	char *rp;
	int strmatch, kr, c;
	char *p;
	int timo = MAXEXPECTTIME;

	/* Set expect time per string,
	 * No trailing number resets to default. */
	rp = strrchr(str, '~');
	if (rp != 0 && *(rp+1) != '\0') {
		timo = strtol(rp+1,&p,10);
		if (*p != '\0' || timo <= 0) {
			timo = MAXEXPECTTIME;
			CDEBUG(1, "bad timeout \"%s\"\n", rp);
		} else {
			*rp = '\0';
			CDEBUG(4, "timeout=%d ", timo);
		}
	}

	CDEBUG0(4, "expect: (");
	for (c=0; kr=str[c]; c++) {
		if (kr < 040) {
			CDEBUG(4, "^%c", kr | 0100);
		} else {
			CDEBUG(4, "%c", kr);
		}
	}
	CDEBUG0(4, ")\n");

	if (EQUALS(str, "\"\"")) {
		CDEBUG0(4, "got it\n");
		return(0);
	}

	bzero(rdvec, sizeof(rdvec));
	strmatch = strlen(str);
	rp = rdvec;

	if (setjmp(Sjbuf)) {
		return(FAIL);
	}
	(void) signal(SIGALRM, alarmtr);
	alarm(timo);
	while (rp-rdvec < strmatch || strcmp(str,rp-strmatch)) {
		for (AbortSeen = 0; AbortSeen < Aborts; AbortSeen++) {
			if (rp-rdvec >= AbortLen[AbortSeen]
			    && !strcmp(AbortStr[AbortSeen],
				       rp-AbortLen[AbortSeen])) {
				alarm(0);
				CDEBUG(1, "Call aborted on '%s'\n",
				       AbortStr[AbortSeen]);
				return ABORT;
			}
		}
		errno = 0;
		kr = (*Read)(fn, rp, 1);
		if (kr <= 0) {
			alarm(0);
			CDEBUG(4, "lost line errno - %d\n", errno);
			logent("LOGIN", "LOST LINE");
			return(FAIL);
		}
		c = (*rp &= 0177);
		if (c < 040) {
			CDEBUG(4, "^%c", c | 0100);
		} else {
			CDEBUG(4, "%c", c);
		}
		if (c != '\0')
			rp++;
		if (rp >= &rdvec[sizeof(rdvec)-1]) {
			CDEBUG0(4, "enough already\n");
			alarm(0);
			return(FAIL);
		}
	}
	alarm(0);
	CDEBUG0(4, "got it\n");
	return(0);
}


/***
 *	alarmtr()  -  catch alarm routine for "expect".
 */

#ifdef sgi
void
#endif
alarmtr()
{
	CDEBUG0(6, "timed out\n");
	longjmp(Sjbuf, 1);
}




/***
 *	sendthem(str, fn, phstr1, phstr2)	send line of chat sequence
 *	char *str, *phstr;
 *
 *	return codes:  none
 */

#define FLUSH() {\
	if ((bptr - buf) > 0)\
		if (wrstr(fn, buf, bptr - buf, echocheck) != SUCCESS)\
			goto err;\
	bptr = buf;\
}

static void
sendthem(char *str, int fn, char *phstr1, char *phstr2)
{
	int sendcr = 1, echocheck = 0;
	register char	*sptr, *bptr;
	char	buf[BUFSIZ];

	/* should be EQUALS, but previous versions had BREAK n for integer n */
	if (PREFIX("BREAK", str)) {
		/* send break */
		CDEBUG0(5, "BREAK\n");
		(*genbrk)(fn);
		return;
	}

	if (EQUALS(str, "EOT")) {
		CDEBUG0(5, "EOT\n");
		(void) (*Write)(fn, EOTMSG, strlen(EOTMSG));
		return;
	}

	if (EQUALS(str, "\"\"")) {
		CDEBUG0(5, "\"\"\n");
		str += 2;
	}

	bptr = buf;
	CDEBUG0(5, "sendthem (");
	for (sptr = str; *sptr; sptr++) {
		if (*sptr == '\\') {
			switch(*++sptr) {

			/* adjust switches */
			case 'c':	/* no CR after string */
				if (sptr[1] == NULLCHAR) {
					CDEBUG0(5, "<NO CR>");
					sendcr = 0;
				} else
					CDEBUG0(5, "<NO CR ignored>");
				continue;
			}

			/* stash in buf and continue */
			switch(*sptr) {
			case 'D':	/* raw phnum */
				strcpy(bptr, phstr1);
				bptr += strlen(bptr);
				continue;
			case 'T':	/* translated phnum */
				strcpy(bptr, phstr2);
				bptr += strlen(bptr);
				continue;
			case 'N':	/* null */
				*bptr++ = 0;
				continue;
			case 's':	/* space */
				*bptr++ = ' ';
				continue;
			case '\\':	/* backslash escapes itself */
				*bptr++ = *sptr;
				continue;
			default:	/* send the backslash */
				*bptr++ = '\\';
				*bptr++ = *sptr;
				continue;

			/* flush buf, perform action, and continue */
			case 'E':	/* echo check on */
				FLUSH();
				CDEBUG0(5, "<ECHO CHECK ON>");
				echocheck = 1;
				continue;
			case 'e':	/* echo check off */
				FLUSH();
				CDEBUG0(5, "<ECHO CHECK OFF>");
				echocheck = 0;
				continue;
			case 'd':	/* sleep briefly */
				FLUSH();
				CDEBUG0(5, "<DELAY>");
#ifdef sgi
				sginap(HZ*2);
#else
				sleep(2);
#endif
				continue;
			case 'p':	/* pause momentarily */
				FLUSH();
				CDEBUG0(5, "<PAUSE>");
				nap(HZ/4);	/* approximately 1/4 second */
				continue;
			case 'K':	/* inline break */
				FLUSH();
				CDEBUG0(5, "<BREAK>");
				(*genbrk)(fn);
				continue;
			case 'M':	/* modem control - set CLOCAL */
				FLUSH();
				if ( ! ioctlok ) {
					CDEBUG0(5, "<set CLOCAL ignored>");
					continue;
				}
				CDEBUG0(5, "<CLOCAL set>");
				ttybuf.c_cflag |= CLOCAL;
				if ( (*Ioctl)(fn, TCSETAW, &ttybuf) < 0 )
					CDEBUG(5, "TCSETAW failed, errno %d\n", errno);
				continue;
			case 'm':	/* no modem control - clear CLOCAL */
				FLUSH();
				if ( ! ioctlok ) {
					CDEBUG0(5, "<clear CLOCAL ignored>");
					continue;
				}
				CDEBUG0(5, "<CLOCAL clear>");
				ttybuf.c_cflag &= ~CLOCAL;
				if ( (*Ioctl)(fn, TCSETAW, &ttybuf) < 0 )
					CDEBUG(5, "TCSETAW failed, errno %d\n", errno);
				continue;
			}
		} else
			*bptr++ = *sptr;
	}
	if (sendcr)
		*bptr++ = '\r';
	if ( (bptr - buf) > 0 )
		(void) wrstr(fn, buf, bptr - buf, echocheck);

err:
	CDEBUG0(5, ")\n");
}

#undef FLUSH

static int				
wrstr(int fn, char *buf, int len, int echocheck)
{
	int	i;
	char	c;
	char dbuf[BUFSIZ], *dbptr = dbuf;

	buf[len] = 0;

	if (echocheck)
		return(wrchr(fn, buf, len));

	if (Debug >= 5) {
		if (sysaccess(ACCESS_SYSTEMS) == 0) { /* Systems file access ok */
			for (i = 0; i < len && dbptr < &dbuf[BUFSIZ-1]; i++) {
				c = buf[i];
				if (c >= ' ' && c < 0x7f && c != '\\') {
					*dbptr++ = c;
					continue;
				}
				*dbptr++ = '\\';
				switch (c) {
				case '\\':
					*dbptr++ = '\\';
					break;
				case '\n':
					*dbptr++= 'n';
					break;
				case '\r':
					*dbptr++= 'r';
					break;
				case '\t':
					*dbptr++ = 't';
					break;
				case '\b':
					*dbptr++ = 'b';
					break;
				default:
					dbptr += sprintf(dbptr,"%o",c);
					break;
				}
			}
			*dbptr = 0;
			CDEBUG(5, "%s", dbuf);
		} else {
			CDEBUG0(5, "????????");
		}
	}
	if ((*Write)(fn, buf, len) != len)
		return(FAIL);
	return(SUCCESS);
}

wrchr(int fn, char *buf, int len)
{
	int	i, saccess;
	char	cin, cout;

	saccess = (sysaccess(ACCESS_SYSTEMS) == 0); /* protect Systems file */
	if (setjmp(Sjbuf))
		return(FAIL);
	(void) signal(SIGALRM, alarmtr);

	for (i = 0; i < len; i++) {
		cout = buf[i];
		if (saccess) {
			CDEBUG(5, "%s", cout < 040 ? "^" : "");
			CDEBUG(5, "%c", cout < 040 ? cout | 0100 : cout);
		} else
			CDEBUG0(5, "?");
		if (((*Write)(fn, &cout, 1)) != 1)
			return(FAIL);
		do {
			(void) alarm(MAXEXPECTTIME);
			if ((*Read)(fn, &cin, 1) != 1)
				return(FAIL);
			(void) alarm(0);
			cin &= 0177;
			if (saccess) {
				CDEBUG(5, "%s", cin < 040 ? "^" : "");
				CDEBUG(5, "%c", cin < 040 ? cin | 0100 : cin);
			} else
				CDEBUG0(5, "?");
		} while (cout != (cin & 0177));
	}
	return(SUCCESS);
}


#ifndef STANDALONE
/*******
 *	ifdate(s)
 *	char *s;
 *
 *	ifdate  -  this routine will check a string (s)
 *	like "MoTu0800-1730" to see if the present
 *	time is within the given limits.
 *	SIDE EFFECT - Retrytime is set to number following ";"
 *
 *	String alternatives:
 *		Wk - Mo thru Fr
 *		zero or one time means all day
 *		Any - any day
 *
 *	return codes:
 *		0  -  not within limits
 *		1  -  within limits
 */

static
ifdate(char *s)
{
	static char *days[] = {
		"Su", "Mo", "Tu", "We", "Th", "Fr", "Sa", 0
	};
	time_t	clock;
	int	t__now;
#ifdef sgi
	char	*p;
#else
	char	*p, *rindex(), *index();
#endif
	struct tm	*tp;

	time(&clock);
	tp = localtime(&clock);
	t__now = tp->tm_hour * 100 + tp->tm_min;	/* "navy" time */

	/*
	 *	pick up retry time for failures
	 *	global variable Retrytime is set here
	 */
	if ((p = rindex(s, ';')) != NULL)
	    if (isdigit(p[1])) {
		if (sscanf(p+1, "%d", &Retrytime) < 1)
			Retrytime = 5;	/* 5 minutes is error default */
		Retrytime  *= 60;
		*p = NULLCHAR;
	    }

	if (!strcmp(s,"Never"))
		return(0);

	while (*s) {
		int	i, dayok;

		for (dayok = 0; (!dayok) && isalpha(*s); s++) {
			if (PREFIX("Any", s))
				dayok = 1;
			else if (PREFIX("Wk", s)) {
				if (tp->tm_wday >= 1 && tp->tm_wday <= 5)
					dayok = 1;
			} else
				for (i = 0; days[i]; i++)
					if (PREFIX(days[i], s))
						if (tp->tm_wday == i)
							dayok = 1;
		}

		if (dayok) {
			int	t__low, t__high;

			while (isalpha(*s))	/* flush remaining day stuff */
				s++;

			if ((sscanf(s, "%d-%d", &t__low, &t__high) < 2)
			 || (t__low == t__high))
				return(1);

			/* 0000 crossover? */
			if (t__low < t__high) {
				if (t__low <= t__now && t__now <= t__high)
					return(1);
			} else if (t__low <= t__now || t__now <= t__high)
				return(1);

			/* aim at next time slot */
			if ((s = index(s, ',')) == NULL)
				break;
		}

		if (*s)			/* do not run onto the next string */
			s++;
	}
	return(0);
}
#endif

/***
 *	char *
 *	fdig(cp)	find first digit in string
 *
 *	return - pointer to first digit in string or end of string
 */

char *
fdig(char *cp)
{
	char *c;

	for (c = cp; *c; c++)
		if (*c >= '0' && *c <= '9')
			break;
	return(c);
}


#ifdef sgi
static void
nap(register long n)
{
#if HZ != 60
	sginap((n*HZ+60/2)/60);
#else
	sginap(n);
#endif
}
#endif
#ifdef FASTTIMER
/*	Sleep in increments of 60ths of second.	*/
nap (time)
register int time;
{
	static int fd;

	if (fd == 0)
		fd = open (FASTTIMER, 0);

	(*Read) (fd, 0, time);
}

#endif /* FASTTIMER */

#ifdef BSD4_2

	/* nap(n) -- sleep for 'n' ticks of 1/60th sec each. */
	/* This version uses the select system call */


nap(n)
unsigned n;
{
	struct timeval tv;
	int rc;

	if (n==0)
		return;
	tv.tv_sec = n/60;
	tv.tv_usec = ((n%60)*1000000L)/60;
	rc = select(32, 0, 0, 0, &tv);
}

#endif /* BSD4_2 */

#ifdef NONAP

/*	nap(n) where n is ticks
 *
 *	loop using n/HZ part of a second
 *	if n represents more than 1 second, then
 *	use sleep(time) where time is the equivalent
 *	seconds rounded off to full seconds
 *	NOTE - this is a rough approximation and chews up
 *	processor resource!
 */

nap(n)
unsigned n;
{
	struct tms	tbuf;
	long endtime;
	int i;

	if (n > HZ) {
		/* > second, use sleep, rounding time */
		sleep( (int) (((n)+HZ/2)/HZ) );
		return;
	}

	/* use timing loop for < 1 second */
	endtime = times(&tbuf) + 3*n/4;	/* use 3/4 because of scheduler! */
	while (times(&tbuf) < endtime) {
	    for (i=0; i<1000; i++, i*i)
		;
	}
	return;
}


#endif /* NONAP */

