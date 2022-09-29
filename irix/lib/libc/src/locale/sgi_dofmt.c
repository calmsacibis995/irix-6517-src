/* @[$] sgi_dofmt.c 1.0 frank@ceres.esd.sgi.com Oct 27 1992
 * print messages with variable number of arguments to buffer
 *
 * this function depends only on catalog 'uxsgicore'
 * In contradiction to fmtmsg(), this function allows the
 * default severity strings to be overwritten.
 *
 * Only the default severities are internationalized messages.
 * If the application adds new severity levels, the application
 * must take care about locale dependencies of the added severities.
 */
#ident "$Revision: 1.11 $"

#include	<synonyms.h>
#include	<stdarg.h>
#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<fmtmsg.h>
#include	<unistd.h>
#include	<mplib.h>
#include	<msgs/uxsgicore.h>
#include	<sgi_nl.h>
#include	<limits.h>

/*
 * a variable that will be initialized later
 */
#define	WILLI		0

/*
 * The following constants define the value of the "msgverb"
 * variable.  This variable tells fmtmsg() which parts of the
 * standard message it is to display.  If !(msgverb & MV_SET),
 * fmtmsg() will interrogate the "MSGVERB" environment variable
 * and set "msgverb" accordingly.
 *
 * Constants:
 *	MV_INV	Check MSGVERB environment variable (invalidates value)
 *	MV_SET	MSGVERB checked, msgverb value valid
 *	MV_LBL	"label" selected
 *	MV_SEV	"severity" selected
 *	MV_TXT	"text" selected
 *	MV_TAG	"messageID" selected
 *	MV_ACT	"action" selected
 *	MV_ALL	All components selected
 *	MV_DFLT	Default value for MSGVERB
 */
#define	MV_INV		0
#define	MV_SET		0x0001
#define	MV_LBL		0x0002
#define	MV_SEV		0x0004
#define	MV_TXT		0x0008
#define	MV_TAG		0x0010
#define	MV_ACT		0x0020
#define	MV_ALL		(MV_LBL|MV_SEV|MV_TXT|MV_TAG|MV_ACT)
#define	MV_DFLT		MV_ALL

/*
 * Keywords for fields named in the MSGVERB environment variable.
 * these keywords will not be internationalized !
 */
struct kword {
	char	keyword[10];
	short	flag;
};

static const struct kword kword[] = {
	{ "label",	MV_LBL	},
	{ "severity",	MV_SEV	},
	{ "text",	MV_TXT	},
	{ "tag",	MV_TAG	},
	{ "action",	MV_ACT	},
	{ 0,		0	}
};

/*
 * Severity string structure
 */
struct sevstr {
	int	sevvalue;	/* the severity level */
	char	*sevkywd;	/* Keyword identifying the severity */
	char	*sevprstr;	/* string associated with the value */
	struct	sevstr *sfore;	/* fore ptr in list */
};

/*
 * Default strings for gettxt() severity messages.
 */
static const char dflt_sv_HALT[]  = "HALT";
static const char dflt_sv_ERROR[] = "ERROR";
static const char dflt_sv_WARN[]  = "WARNING";
static const char dflt_sv_INFO[]  = "INFO";
static const char dflt_sv_FIX[]   = "TO FIX";
static const char dflt_sv_RSN[]   = "REASON";

/*
 * default severity
 */
static	struct sevstr sevdflt[] = {
	{ MM_HALT,	"",	WILLI,	&sevdflt[1]	},
	{ MM_ERROR,	"",	WILLI,	&sevdflt[2]	},
	{ MM_WARNING,	"",	WILLI,	&sevdflt[3]	},
	{ MM_INFO,	"",	WILLI,	&sevdflt[4]	},
	{ MM_REASON,	"",	WILLI,	&sevdflt[5]	},
	{ MM_FIX,	"",	WILLI,	0,		}
};

/*
 * anchor ptr to severity lists
 */
static	struct sevstr *sev_def  = sevdflt;
static	struct sevstr *sev_appl = 0;
static	struct sevstr *sev_env  = 0;

/*
 * Default text string for gettxt() if none is provided
 */
static const char dflt_TEXT[] = "No text provided with this message";
static	char *dtext = NULL;

/*
 * Default separator for gettxt()
 */
static	char dflt_sep[] = ": ";
static	char *dsep = NULL;

/*
 * Other locals
 */
static	int msgverb = MV_INV;	/* message verbosity */
static	int sevlook = 1;	/* look for SEV_LEVEL */
static	struct sevstr *srchsev(struct sevstr *, int);
static	struct sevstr *setnsev(struct sevstr **, char **, int);
static	char *sev_chkdesc(char *);
static	void set_msgverb(void);
static	void set_sevstr(void);
char 	*_sgi_donfmt(char *, ssize_t, int, char *, int, char *, va_list);

/*
 * Parse the args of MSGVERB env variable
 * and compile "msgverb"
 * In contradiction to fmtmsg(), this function
 * ignores illegal keywords !
 */
static void
set_msgverb(void)
{
	register char *v;
	register char *t;
	register const struct kword *k;
	register char *s;

	/*
	 * if no env variable, set to default
	 */
	msgverb = MV_INV;			/* invalidate old one */
	if( ! (v = getenv(MSGVERB)))
	    goto dflt_mesg;
	if( ! *v)
	    goto dflt_mesg;			/* only set */

	/*
	 * allocate space for arguments and make a copy
	 */
	s = (char *)malloc(strlen(v) + 1);
	if( ! s)
	    goto dflt_mesg;
	(void)strcpy(s, v);

	/*
	 * scan and compile tokens
	 * keyword will not be internationalized,
	 * so I can scan for ':' bytewise
	 */
	t = s;
	do {
	    if(v = strchr(t, ':'))
		*v = 0;				/* delimiter found */
	    for(k = kword; k->flag; k++) {
		if( ! strcmp(t, k->keyword)) {
		    msgverb |= k->flag;		/* set flag */
		    break;
		}
	    }
	    t = v + 1;
	} while(v);
	(void)free(s);				/* free memory */
	if(msgverb)
	    return;
dflt_mesg:
	msgverb = MV_DFLT;			/* set default */
	return;
}

/*
 * search for severity
 */
static struct sevstr *
srchsev(struct sevstr *asp, int sev)
{
	while(asp) {
	    if(asp->sevvalue == sev)
		return(asp);			/* severity found */
	    asp = asp->sfore;
	}
	return(0);				/* severity not found */
}

/*
 * set a severity entry
 */
static struct sevstr *
setnsev(register struct sevstr **sevp, register char **dp, register int sl)
{
	register struct sevstr *asp;
	register char *s;
	register size_t lkw;
	char *fp;

	/*
	 * first convert severity level
	 */
	if(dp[1]) {
	    sl = (int)strtol(dp[1], &fp, 0);
	    if((sl < 0) || *fp)
		return(0);			/* number format error */
	}
	if(srchsev(sev_def, sl))
	    return(0);				/* predefined severity */
	if(asp = srchsev(*sevp, sl))
	    free(asp->sevkywd);			/* free old strings */
	else {
	    asp = (struct sevstr *)malloc(sizeof(struct sevstr));
	    asp->sevvalue = sl;
	    asp->sfore = *sevp;
	    *sevp = asp;
	}
	/*
	 * allocate space for keyword and print string
	 * both strings are hold in one allocated field
	 */
	lkw = strlen(dp[0]) + 1;
	s = (char *)malloc(lkw + strlen(dp[2]) + 1);
	if( !s)
	    return(0);				/* no memory */

	asp->sevkywd = s;
	asp->sevprstr = s + lkw;
	(void)strcpy(s, dp[0]);
	(void)strcpy(asp->sevprstr, dp[2]);
	return(asp);
}

/*
 * check one description
 */
static char *
sev_chkdesc(register char *s)
{
	register char *op;
	register int i;
	register int x;
	wchar_t wc;
	char *desp[3];

	for(op = s, i = 0;;) {
	    x = mbtowc(&wc, s, (size_t) MB_CUR_MAX);
	    if(x < 0)
		return(0);			/* illegal mbchar */
	    switch(wc) {
		case ',' :
		case '\0' :
		case ':' :
		    *s = '\0';
		    desp[i++] = op;		/* save ptr */
		    break;
		default :
		    s += x;
		    continue;
	    }
	    s++;
	    if(op == (s - 1))
		goto seekdesc;			/* syntax error */
	    op = s;
	    if(wc == ',') {
		if(i > 2)
		    goto seekdesc;		/* syntax error */
		continue;
	    }
	    if(i != 3)
		goto seekdesc;			/* syntax error */
	    break;
	}
	/*
	 * now add the field to the severity structure
	 */
	(void)setnsev(&sev_env, desp, 0);	/* set this severity */
	goto setsevret;

	/*
	 * syntax error, seek to next ':' or \0
	 */
seekdesc:
	for(; wc && (wc != ':');) {
	    x = mbtowc(&wc, s, (size_t)MB_CUR_MAX);
	    if(x < 0)
		return(0);			/* illegal mbchar */
	    s += x;
	}
setsevret:
	if( !wc)
	    return(s - 1);
	return(s);
}

/*
 * Parse the args of SEV_LEVEL env variable
 * and compile additional severities
 * Once an illegal mbchar is found, no further interpretation
 * on SEV_LEVEL is done.
 */
static void
set_sevstr(void)
{
	register char *s;
	register char *sp;
	register size_t x;

	/*
	 * look for env variable
	 */
	if( ! (s = getenv(SEV_LEVEL)))
	    return;				/* no such env variable */
	x = strlen(s) + 1;
	sp = malloc(x);
	if( !sp)
	    return;				/* no memory */
	(void)strcpy(sp, s);

	for(s = sp; *s;) {
	    s = sev_chkdesc(s);			/* check one description */
	    if( !s)
		break;				/* illegal mbchar, ignore */
	}
	free(sp);				/* free the buffer */
}

/*
 * string copy
 * returns ptr after last character
 * third param is the end of buffer, added to check for buffer overruns.
 */
static char *
stradd(char *d, char *s, char *e)
{
	/* Check if buffer is already at the end. */
	if (d >= e)
	    return(d);

	while(*s) {
	    *d++ = *s++;
	    if(d >= e) { /* hit the end */
		break;
	    }
	}
	return(d);
}

/*
 * basic routine of:
 *	sgi_sfmtmsg()
 *	sgi_ffmtmsg()
 */

/* ARGSUSED */
char *
_sgi_donfmt(char *buf, ssize_t cnt, int class, char *label, int sev, char *fmt, va_list ap)
{
	register char *s, *e;
	register struct sevstr *sevp;
	LOCKDECLINIT(lck, LOCKLOCALE);

	/*
	 * check env variable and set msgverb
	 */
	if( ! (msgverb & MV_SET)) {
	    set_msgverb();
	    msgverb |= MV_SET;
	}
	/*
	 * Extract the severity definitions from the SEV_LEVEL
	 * environment variable and save away for later.
	 */
	if(sevlook) {
	    set_sevstr();
	    sevlook = 0;
	}
	if( ! buf)
	    return(buf);				/* no buffer */

	/*
	 * read the locale-specific severity strings
	 * and default text and separator strings
	 * The severity strings are read from message
	 * database each time this function is called,
	 * because locale may have been changed.
	 */
	if(msgverb & MV_SEV) {
	    sevdflt[0].sevprstr = gettxt(_SGI_MMX_fmtmsg_HALT, dflt_sv_HALT);
	    sevdflt[1].sevprstr = gettxt(_SGI_MMX_fmtmsg_ERROR, dflt_sv_ERROR);
	    sevdflt[2].sevprstr = gettxt(_SGI_MMX_fmtmsg_WARN, dflt_sv_WARN);
	    sevdflt[3].sevprstr = gettxt(_SGI_MMX_fmtmsg_INFO, dflt_sv_INFO);
	    sevdflt[4].sevprstr = gettxt(_SGI_MMX_fmtmsg_REASON, dflt_sv_RSN);
	    sevdflt[5].sevprstr = gettxt(_SGI_MMX_fmtmsg_FIX, dflt_sv_FIX);
	}
	dtext = gettxt(_SGI_MMX_fmtmsg_dtext, dflt_TEXT);
	dsep  = gettxt(_SGI_MMX_fmtmsg_dsep, dflt_sep);

	/*
	 * write the label
	 */
	s = buf;
	e = s+cnt-1; /* mark the end of the passed buffer */
	if((msgverb & MV_LBL) && label) {
	    s = stradd(s, label, e);		/* write label */
	    if(msgverb & (MV_SEV | MV_TXT))
		s = stradd(s, dsep, e);		/* add separator */
	}

	/*
	 * write the severity
	 */
	if(msgverb & MV_SEV) {
	    sevp = srchsev(sev_def, sev);	/* default severities */
	    if( ! sevp) {
		sevp = srchsev(sev_appl, sev);	/* added by application */
		if( ! sevp)
		    sevp = srchsev(sev_env, sev);	/* added by env */
	    }
	    if(sevp)
		s = stradd(s, sevp->sevprstr, e );	/* add severity string */
	    else {
		s += sprintf(s, "SV=%d", sev);	/* severity not found */
	    }
	    if(msgverb & MV_TXT)
		s = stradd(s, dsep, e);		/* add separator */
	}

	/*
	 * write formatted message text
	 */
	if(msgverb & MV_TXT) {
	    if( ! fmt)
		s = stradd(s, dtext, e);		/* if no fmt, add def. text */
	    else {
		/* Use the bounds check ver of vsprintf() to stop buffer overflow */
		s += vsnprintf(s, cnt-(s-buf+1), fmt, ap);/* add formatted text */
	    }
	}
	*s = '\0';				/* write end of string */
	UNLOCKLOCALE(lck);
	return(s);
}

/* ARGSUSED */
char *
_sgi_dofmt(char *buf, int class, char *label, int sev, char *fmt, va_list ap)
{
	return _sgi_donfmt(buf, 2*PATH_MAX, class, label, sev, fmt, ap);
}

/*
 * add severity by the application
 *
 * ret:	 0 - sucessful
 *	-1 - failed (value is one of the predefined, malloc() failed
 */
int
_sgi_addseverity(int value, const char *string)
{
	char *dp[3];

	dp[0] = "";				/* no keyword */
	dp[1] = 0;				/* value in 3rd arg */
	dp[2] = (char *)string;
	return(setnsev(&sev_appl, dp, value)? 0 : -1);
}
