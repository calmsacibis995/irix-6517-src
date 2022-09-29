/*
 * Copyright (c) 1998 Sendmail, Inc.  All rights reserved.
 * Copyright (c) 1983, 1995-1997 Eric P. Allman.  All rights reserved.
 * Copyright (c) 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 *
 */

#ifndef lint
static char sccsid[] = "@(#)conf.c	8.452 (Berkeley) 1/26/1999";
#endif /* not lint */

# include "sendmail.h"
# include "pathnames.h"
# include <sys/ioctl.h>
# include <sys/param.h>
# include <limits.h>

/*
**  CONF.C -- Sendmail Configuration Tables.
**
**	Defines the configuration of this installation.
**
**	Configuration Variables:
**		HdrInfo -- a table describing well-known header fields.
**			Each entry has the field name and some flags,
**			which are described in sendmail.h.
**
**	Notes:
**		I have tried to put almost all the reasonable
**		configuration information into the configuration
**		file read at runtime.  My intent is that anything
**		here is a function of the version of UNIX you
**		are running, or is really static -- for example
**		the headers are a superset of widely used
**		protocols.  If you find yourself playing with
**		this file too much, you may be making a mistake!
*/


/*
**  Header info table
**	Final (null) entry contains the flags used for any other field.
**
**	Not all of these are actually handled specially by sendmail
**	at this time.  They are included as placeholders, to let
**	you know that "someday" I intend to have sendmail do
**	something with them.
*/

struct hdrinfo	HdrInfo[] =
{
		/* originator fields, most to least significant  */
	{ "resent-sender",		H_FROM|H_RESENT			},
	{ "resent-from",		H_FROM|H_RESENT			},
	{ "resent-reply-to",		H_FROM|H_RESENT			},
	{ "sender",			H_FROM				},
	{ "from",			H_FROM				},
	{ "reply-to",			H_FROM				},
	{ "errors-to",			H_FROM|H_ERRORSTO		},
	{ "full-name",			H_ACHECK			},
	{ "return-receipt-to",		H_RECEIPTTO			},

		/* destination fields */
	{ "to",				H_RCPT				},
	{ "resent-to",			H_RCPT|H_RESENT			},
	{ "cc",				H_RCPT				},
	{ "resent-cc",			H_RCPT|H_RESENT			},
	{ "bcc",			H_RCPT|H_BCC			},
	{ "resent-bcc",			H_RCPT|H_BCC|H_RESENT		},
	{ "apparently-to",		H_RCPT				},

		/* message identification and control */
	{ "message-id",			0				},
	{ "resent-message-id",		H_RESENT			},
	{ "message",			H_EOH				},
	{ "text",			H_EOH				},

		/* date fields */
	{ "date",			0				},
	{ "resent-date",		H_RESENT			},

		/* trace fields */
	{ "received",			H_TRACE|H_FORCE			},
	{ "x400-received",		H_TRACE|H_FORCE			},
	{ "via",			H_TRACE|H_FORCE			},
	{ "mail-from",			H_TRACE|H_FORCE			},

		/* miscellaneous fields */
	{ "comments",			H_FORCE|H_ENCODABLE		},
	{ "return-path",		H_FORCE|H_ACHECK		},
	{ "content-transfer-encoding",	H_CTE				},
	{ "content-type",		H_CTYPE				},
	{ "content-length",		H_ACHECK			},
	{ "subject",			H_ENCODABLE			},

	{ NULL,				0				}
};



/*
**  Privacy values
*/

struct prival PrivacyValues[] =
{
	{ "public",		PRIV_PUBLIC		},
	{ "needmailhelo",	PRIV_NEEDMAILHELO	},
	{ "needexpnhelo",	PRIV_NEEDEXPNHELO	},
	{ "needvrfyhelo",	PRIV_NEEDVRFYHELO	},
	{ "noexpn",		PRIV_NOEXPN		},
	{ "novrfy",		PRIV_NOVRFY		},
	{ "restrictmailq",	PRIV_RESTRICTMAILQ	},
	{ "restrictqrun",	PRIV_RESTRICTQRUN	},
	{ "noetrn",		PRIV_NOETRN		},
	{ "noverb",		PRIV_NOVERB		},
	{ "authwarnings",	PRIV_AUTHWARNINGS	},
	{ "noreceipts",		PRIV_NORECEIPTS		},
	{ "goaway",		PRIV_GOAWAY		},
	{ NULL,			0			}
};

/*
**  DontBlameSendmail values
*/
struct dbsval DontBlameSendmailValues[] =
{
	{ "safe",			DBS_SAFE			},
	{ "assumesafechown",		DBS_ASSUMESAFECHOWN		},
	{ "groupwritabledirpathsafe",	DBS_GROUPWRITABLEDIRPATHSAFE	},
	{ "groupwritableforwardfilesafe",
					DBS_GROUPWRITABLEFORWARDFILESAFE },
	{ "groupwritableincludefilesafe",
					DBS_GROUPWRITABLEINCLUDEFILESAFE },
	{ "groupwritablealiasfile",	DBS_GROUPWRITABLEALIASFILE	},
	{ "worldwritablealiasfile",	DBS_WORLDWRITABLEALIASFILE	},
	{ "forwardfileinunsafedirpath",	DBS_FORWARDFILEINUNSAFEDIRPATH	},
	{ "includefileinunsafedirpath",	DBS_INCLUDEFILEINUNSAFEDIRPATH	},
	{ "mapinunsafedirpath",		DBS_MAPINUNSAFEDIRPATH	},
	{ "linkedaliasfileinwritabledir",
					DBS_LINKEDALIASFILEINWRITABLEDIR },
	{ "linkedclassfileinwritabledir",
					DBS_LINKEDCLASSFILEINWRITABLEDIR },
	{ "linkedforwardfileinwritabledir",
					DBS_LINKEDFORWARDFILEINWRITABLEDIR },
	{ "linkedincludefileinwritabledir",
					DBS_LINKEDINCLUDEFILEINWRITABLEDIR },
	{ "linkedmapinwritabledir",	DBS_LINKEDMAPINWRITABLEDIR	},
	{ "linkedserviceswitchfileinwritabledir",
					DBS_LINKEDSERVICESWITCHFILEINWRITABLEDIR },
	{ "filedeliverytohardlink",	DBS_FILEDELIVERYTOHARDLINK	},
	{ "filedeliverytosymlink",	DBS_FILEDELIVERYTOSYMLINK	},
	{ "writemaptohardlink",		DBS_WRITEMAPTOHARDLINK		},
	{ "writemaptosymlink",		DBS_WRITEMAPTOSYMLINK		},
	{ "writestatstohardlink",	DBS_WRITESTATSTOHARDLINK	},
	{ "writestatstosymlink",	DBS_WRITESTATSTOSYMLINK		},
	{ "forwardfileingroupwritabledirpath",
					DBS_FORWARDFILEINGROUPWRITABLEDIRPATH },
	{ "includefileingroupwritabledirpath",
					DBS_INCLUDEFILEINGROUPWRITABLEDIRPATH },
	{ "classfileinunsafedirpath",	DBS_CLASSFILEINUNSAFEDIRPATH	},
	{ "errorheaderinunsafedirpath",	DBS_ERRORHEADERINUNSAFEDIRPATH	},
	{ "helpfileinunsafedirpath",	DBS_HELPFILEINUNSAFEDIRPATH	},
	{ "forwardfileinunsafedirpathsafe",
					DBS_FORWARDFILEINUNSAFEDIRPATHSAFE },
	{ "includefileinunsafedirpathsafe",
					DBS_INCLUDEFILEINUNSAFEDIRPATHSAFE },
	{ "runprograminunsafedirpath",	DBS_RUNPROGRAMINUNSAFEDIRPATH	},
	{ "runwritableprogram",		DBS_RUNWRITABLEPROGRAM		},
	{ NULL,				0				}
};


/*
**  Miscellaneous stuff.
*/

int	DtableSize =	50;		/* max open files; reset in 4.2bsd */
/*
**  SETDEFAULTS -- set default values
**
**	Because of the way freezing is done, these must be initialized
**	using direct code.
**
**	Parameters:
**		e -- the default envelope.
**
**	Returns:
**		none.
**
**	Side Effects:
**		Initializes a bunch of global variables to their
**		default values.
*/

#define MINUTES		* 60
#define HOURS		* 60 MINUTES
#define DAYS		* 24 HOURS

#ifndef _PATH_VARTMP
# define _PATH_VARTMP	"/usr/tmp/"
#endif

#ifndef MAXRULERECURSION
# define MAXRULERECURSION	50	/* max ruleset recursion depth */
#endif

void
setdefaults(e)
	register ENVELOPE *e;
{
	int i;
	struct passwd *pw;
	char buf[MAXNAME];
	extern void setdefuser __P((void));
	extern void setupmaps __P((void));
	extern void setupmailers __P((void));
	extern void setupheaders __P((void));

	SpaceSub = ' ';				/* option B */
	QueueLA = 8;				/* option x */
	RefuseLA = 12;				/* option X */
	WkRecipFact = 30000L;			/* option y */
	WkClassFact = 1800L;			/* option z */
	WkTimeFact = 90000L;			/* option Z */
	QueueFactor = WkRecipFact * 20;		/* option q */
	FileMode = (RealUid != geteuid()) ? 0644 : 0600;
						/* option F */

	if (((pw = getpwnam("mailnull")) != NULL && pw->pw_uid != 0) ||
	    ((pw = getpwnam("sendmail")) != NULL && pw->pw_uid != 0) ||
	    ((pw = getpwnam("daemon")) != NULL && pw->pw_uid != 0))
	{
		DefUid = pw->pw_uid;		/* option u */
		DefGid = pw->pw_gid;		/* option g */
		DefUser = newstr(pw->pw_name);
	}
	else
	{
		DefUid = 1;			/* option u */
		DefGid = 1;			/* option g */
		setdefuser();
	}
	TrustedUid = 0;
	if (tTd(37, 4))
		printf("setdefaults: DefUser=%s, DefUid=%d, DefGid=%d\n",
		       DefUser != NULL ? DefUser : "<1:1>",
		       (int) DefUid, (int) DefGid);
	CheckpointInterval = 10;		/* option C */
	MaxHopCount = 25;			/* option h */
	e->e_sendmode = SM_FORK;		/* option d */
	e->e_errormode = EM_PRINT;		/* option e */
	SevenBitInput = FALSE;			/* option 7 */
#ifdef TRUSTED_MAC
	MaxMciCache = (sm_mac_enabled() ? 0 : 1);	/* option k */
#else
	MaxMciCache = 1;			/* option k */
#endif
	MciCacheTimeout = 5 MINUTES;		/* option K */
	LogLevel = 9;				/* option L */
	inittimeouts(NULL);			/* option r */
	PrivacyFlags = PRIV_PUBLIC;		/* option p */
	DontBlameSendmail = DBS_SAFE;		/* DontBlameSendmail option */
#if MIME8TO7
	MimeMode = MM_CVTMIME|MM_PASS8BIT;	/* option 8 */
#else
	MimeMode = MM_PASS8BIT;
#endif
	for (i = 0; i < MAXTOCLASS; i++)
	{
		TimeOuts.to_q_return[i] = 5 DAYS;	/* option T */
		TimeOuts.to_q_warning[i] = 0;		/* option T */
	}
	ServiceSwitchFile = "/etc/service.switch";
	ServiceCacheMaxAge = (time_t) 10;
	HostsFile = _PATH_HOSTS;
	PidFile = newstr(_PATH_SENDMAILPID);
	MustQuoteChars = "@,;:\\()[].'";
	MciInfoTimeout = 30 MINUTES;
	MaxRuleRecursion = MAXRULERECURSION;
	MaxAliasRecursion = 10;
	MaxMacroRecursion = 10;
	ColonOkInAddr = TRUE;
	DontLockReadFiles = TRUE;
	DoubleBounceAddr = "postmaster";
	MaxHeadersLength = MAXHDRSLEN;
	snprintf(buf, sizeof buf, "%s%sdead.letter",
		_PATH_VARTMP,
		_PATH_VARTMP[sizeof _PATH_VARTMP - 2] == '/' ? "" : "/");
	DeadLetterDrop = newstr(buf);
#ifdef HESIOD_INIT
	HesiodContext = NULL;
#endif
	ControlSocketName = NULL;
	setupmaps();
	setupmailers();
	setupheaders();
}


/*
**  SETDEFUSER -- set/reset DefUser using DefUid (for initgroups())
*/

void
setdefuser()
{
	struct passwd *defpwent;
	static char defuserbuf[40];

	DefUser = defuserbuf;
	defpwent = sm_getpwuid(DefUid);
	snprintf(defuserbuf, sizeof defuserbuf, "%s",
		defpwent == NULL ? "nobody" : defpwent->pw_name);
	if (tTd(37, 4))
		printf("setdefuser: DefUid=%d, DefUser=%s\n",
		       (int) DefUid, DefUser);
}
/*
**  SETUPMAILERS -- initialize default mailers
*/

void
setupmailers()
{
	char buf[100];

	strcpy(buf, "prog, P=/bin/sh, F=lsoDq9, T=DNS/RFC822/X-Unix, A=sh -c \201u");
	makemailer(buf);

	strcpy(buf, "*file*, P=[FILE], F=lsDFMPEouq9, T=DNS/RFC822/X-Unix, A=FILE \201u");
	makemailer(buf);

	strcpy(buf, "*include*, P=/dev/null, F=su, A=INCLUDE \201u");
	makemailer(buf);
}
/*
**  SETUPMAPS -- set up map classes
*/

#define MAPDEF(name, ext, flags, parse, open, close, lookup, store) \
	{ \
		extern bool parse __P((MAP *, char *)); \
		extern bool open __P((MAP *, int)); \
		extern void close __P((MAP *)); \
		extern char *lookup __P((MAP *, char *, char **, int *)); \
		extern void store __P((MAP *, char *, char *)); \
		s = stab(name, ST_MAPCLASS, ST_ENTER); \
		s->s_mapclass.map_cname = name; \
		s->s_mapclass.map_ext = ext; \
		s->s_mapclass.map_cflags = flags; \
		s->s_mapclass.map_parse = parse; \
		s->s_mapclass.map_open = open; \
		s->s_mapclass.map_close = close; \
		s->s_mapclass.map_lookup = lookup; \
		s->s_mapclass.map_store = store; \
	}

void
setupmaps()
{
	register STAB *s;

#ifdef NEWDB
	MAPDEF("hash", ".db", MCF_ALIASOK|MCF_REBUILDABLE,
		map_parseargs, hash_map_open, db_map_close,
		db_map_lookup, db_map_store);

	MAPDEF("btree", ".db", MCF_ALIASOK|MCF_REBUILDABLE,
		map_parseargs, bt_map_open, db_map_close,
		db_map_lookup, db_map_store);
#endif

#ifdef NDBM
	MAPDEF("dbm", ".dir", MCF_ALIASOK|MCF_REBUILDABLE,
		map_parseargs, ndbm_map_open, ndbm_map_close,
		ndbm_map_lookup, ndbm_map_store);
#endif

#ifdef NIS
	MAPDEF("nis", NULL, MCF_ALIASOK,
		map_parseargs, nis_map_open, null_map_close,
		nis_map_lookup, null_map_store);
#endif

#ifdef NISPLUS
	MAPDEF("nisplus", NULL, MCF_ALIASOK,
		map_parseargs, nisplus_map_open, null_map_close,
		nisplus_map_lookup, null_map_store);
#endif
#ifdef LDAPMAP
	MAPDEF("ldapx", NULL, 0,
		ldap_map_parseargs, ldap_map_open, ldap_map_close,
		ldap_map_lookup, null_map_store);
#endif

#ifdef HESIOD
	MAPDEF("hesiod", NULL, MCF_ALIASOK|MCF_ALIASONLY,
		map_parseargs, hes_map_open, null_map_close,
		hes_map_lookup, null_map_store);
#endif

#if NETINFO
	MAPDEF("netinfo", NULL, MCF_ALIASOK,
		map_parseargs, ni_map_open, null_map_close,
		ni_map_lookup, null_map_store);
#endif

#if 0
	MAPDEF("dns", NULL, 0,
		dns_map_init, null_map_open, null_map_close,
		dns_map_lookup, null_map_store);
#endif

#if NAMED_BIND
	/* best MX DNS lookup */
	MAPDEF("bestmx", NULL, MCF_OPTFILE,
		map_parseargs, null_map_open, null_map_close,
		bestmx_map_lookup, null_map_store);
#endif

	MAPDEF("host", NULL, 0,
		host_map_init, null_map_open, null_map_close,
		host_map_lookup, null_map_store);

	MAPDEF("text", NULL, MCF_ALIASOK,
		map_parseargs, text_map_open, null_map_close,
		text_map_lookup, null_map_store);

	MAPDEF("stab", NULL, MCF_ALIASOK|MCF_ALIASONLY,
		map_parseargs, stab_map_open, null_map_close,
		stab_map_lookup, stab_map_store);

	MAPDEF("implicit", NULL, MCF_ALIASOK|MCF_ALIASONLY|MCF_REBUILDABLE,
		map_parseargs, impl_map_open, impl_map_close,
		impl_map_lookup, impl_map_store);

	/* access to system passwd file */
	MAPDEF("user", NULL, MCF_OPTFILE,
		map_parseargs, user_map_open, null_map_close,
		user_map_lookup, null_map_store);

	/* dequote map */
	MAPDEF("dequote", NULL, 0,
		dequote_init, null_map_open, null_map_close,
		dequote_map, null_map_store);

#ifdef MAP_REGEX
	MAPDEF("regex", NULL, 0,
		regex_map_init, null_map_open, null_map_close,
		regex_map_lookup, null_map_store);
#endif

#if USERDB
	/* user database */
	MAPDEF("userdb", ".db", 0,
		map_parseargs, null_map_open, null_map_close,
		udb_map_lookup, null_map_store);
#endif

	/* arbitrary programs */
	MAPDEF("program", NULL, MCF_ALIASOK,
		map_parseargs, null_map_open, null_map_close,
		prog_map_lookup, null_map_store);

	/* sequenced maps */
	MAPDEF("sequence", NULL, MCF_ALIASOK,
		seq_map_parse, null_map_open, null_map_close,
		seq_map_lookup, seq_map_store);

	/* switched interface to sequenced maps */
	MAPDEF("switch", NULL, MCF_ALIASOK,
		map_parseargs, switch_map_open, null_map_close,
		seq_map_lookup, seq_map_store);

	/* null map lookup -- really for internal use only */
	MAPDEF("null", NULL, MCF_ALIASOK|MCF_OPTFILE,
		map_parseargs, null_map_open, null_map_close,
		null_map_lookup, null_map_store);

#if _FFR_MAP_SYSLOG
	/* syslog map -- logs information to syslog */
	MAPDEF("syslog", NULL, 0,
	       syslog_map_parseargs, null_map_open, null_map_close,
	       syslog_map_lookup, null_map_store);
#endif

#if MAP_NSD
	/* IRIX 6.5 nsd support */
	MAPDEF("nsd", NULL, MCF_ALIASOK,
	       map_parseargs, null_map_open, null_map_close,
	       nsd_map_lookup, null_map_store);
#endif
}

#undef MAPDEF
/*
**  INITHOSTMAPS -- initial host-dependent maps
**
**	This should act as an interface to any local service switch
**	provided by the host operating system.
**
**	Parameters:
**		none
**
**	Returns:
**		none
**
**	Side Effects:
**		Should define maps "host" and "users" as necessary
**		for this OS.  If they are not defined, they will get
**		a default value later.  It should check to make sure
**		they are not defined first, since it's possible that
**		the config file has provided an override.
*/

void
inithostmaps()
{
	register int i;
	int nmaps;
	char *maptype[MAXMAPSTACK];
	short mapreturn[MAXMAPACTIONS];
	char buf[MAXLINE];

	/*
	**  Set up default hosts maps.
	*/

#if 0
	nmaps = switch_map_find("hosts", maptype, mapreturn);
	for (i = 0; i < nmaps; i++)
	{
		if (strcmp(maptype[i], "files") == 0 &&
		    stab("hosts.files", ST_MAP, ST_FIND) == NULL)
		{
			strcpy(buf, "hosts.files text -k 0 -v 1 /etc/hosts");
			(void) makemapentry(buf);
		}
#if NAMED_BIND
		else if (strcmp(maptype[i], "dns") == 0 &&
		    stab("hosts.dns", ST_MAP, ST_FIND) == NULL)
		{
			strcpy(buf, "hosts.dns dns A");
			(void) makemapentry(buf);
		}
#endif
#ifdef NISPLUS
		else if (strcmp(maptype[i], "nisplus") == 0 &&
		    stab("hosts.nisplus", ST_MAP, ST_FIND) == NULL)
		{
			strcpy(buf, "hosts.nisplus nisplus -k name -v address -d hosts.org_dir");
			(void) makemapentry(buf);
		}
#endif
#ifdef NIS
		else if (strcmp(maptype[i], "nis") == 0 &&
		    stab("hosts.nis", ST_MAP, ST_FIND) == NULL)
		{
			strcpy(buf, "hosts.nis nis -d -k 0 -v 1 hosts.byname");
			(void) makemapentry(buf);
		}
#endif
#if NETINFO
		else if (strcmp(maptype[i], "netinfo") == 0) &&
		    stab("hosts.netinfo", ST_MAP, ST_FIND) == NULL)
		{
			strcpy(buf, "hosts.netinfo netinfo -v name /machines");
			(void) makemapentry(buf);
		}
#endif
	}
#endif

	/*
	**  Make sure we have a host map.
	*/

	if (stab("host", ST_MAP, ST_FIND) == NULL)
	{
		/* user didn't initialize: set up host map */
		strcpy(buf, "host host");
#if NAMED_BIND
		if (ConfigLevel >= 2)
			strcat(buf, " -a.");
#endif
		(void) makemapentry(buf);
	}

	/*
	**  Set up default aliases maps
	*/

	nmaps = switch_map_find("aliases", maptype, mapreturn);
	for (i = 0; i < nmaps; i++)
	{
		if (strcmp(maptype[i], "files") == 0 &&
		    stab("aliases.files", ST_MAP, ST_FIND) == NULL)
		{
			strcpy(buf, "aliases.files null");
			(void) makemapentry(buf);
		}
#ifdef NISPLUS
		else if (strcmp(maptype[i], "nisplus") == 0 &&
		    stab("aliases.nisplus", ST_MAP, ST_FIND) == NULL)
		{
			strcpy(buf, "aliases.nisplus nisplus -kalias -vexpansion -d mail_aliases.org_dir");
			(void) makemapentry(buf);
		}
#endif
#ifdef NIS
		else if (strcmp(maptype[i], "nis") == 0 &&
		    stab("aliases.nis", ST_MAP, ST_FIND) == NULL)
		{
			strcpy(buf, "aliases.nis nis -d mail.aliases");
			(void) makemapentry(buf);
		}
#endif
#ifdef NETINFO
		else if (strcmp(maptype[i], "netinfo") == 0 &&
		    stab("aliases.netinfo", ST_MAP, ST_FIND) == NULL)
		{
			strcpy(buf, "aliases.netinfo netinfo -z, /aliases");
			(void) makemapentry(buf);
		}
#endif
#ifdef HESIOD
		else if (strcmp(maptype[i], "hesiod") == 0 &&
		    stab("aliases.hesiod", ST_MAP, ST_FIND) == NULL)
		{
			strcpy(buf, "aliases.hesiod hesiod aliases");
			(void) makemapentry(buf);
		}
#endif
	}
	if (stab("aliases", ST_MAP, ST_FIND) == NULL)
	{
		strcpy(buf, "aliases switch aliases");
		(void) makemapentry(buf);
	}

#if 0		/* "user" map class is a better choice */
	/*
	**  Set up default users maps.
	*/

	nmaps = switch_map_find("passwd", maptype, mapreturn);
	for (i = 0; i < nmaps; i++)
	{
		if (strcmp(maptype[i], "files") == 0 &&
		    stab("users.files", ST_MAP, ST_FIND) == NULL)
		{
			strcpy(buf, "users.files text -m -z: -k0 -v6 /etc/passwd");
			(void) makemapentry(buf);
		}
#ifdef NISPLUS
		else if (strcmp(maptype[i], "nisplus") == 0 &&
		    stab("users.nisplus", ST_MAP, ST_FIND) == NULL)
		{
			strcpy(buf, "users.nisplus nisplus -m -kname -vhome -d passwd.org_dir");
			(void) makemapentry(buf);
		}
#endif
#ifdef NIS
		else if (strcmp(maptype[i], "nis") == 0 &&
		    stab("users.nis", ST_MAP, ST_FIND) == NULL)
		{
			strcpy(buf, "users.nis nis -m -d passwd.byname");
			(void) makemapentry(buf);
		}
#endif
#ifdef HESIOD
		else if (strcmp(maptype[i], "hesiod") == 0) &&
		    stab("users.hesiod", ST_MAP, ST_FIND) == NULL)
		{
			strcpy(buf, "users.hesiod hesiod");
			(void) makemapentry(buf);
		}
#endif
	}
	if (stab("users", ST_MAP, ST_FIND) == NULL)
	{
		strcpy(buf, "users switch -m passwd");
		(void) makemapentry(buf);
	}
#endif
}
/*
**  SWITCH_MAP_FIND -- find the list of types associated with a map
**
**	This is the system-dependent interface to the service switch.
**
**	Parameters:
**		service -- the name of the service of interest.
**		maptype -- an out-array of strings containing the types
**			of access to use for this service.  There can
**			be at most MAXMAPSTACK types for a single service.
**		mapreturn -- an out-array of return information bitmaps
**			for the map.
**
**	Returns:
**		The number of map types filled in, or -1 for failure.
*/

#if defined(SOLARIS) || (defined(sony_news) && defined(__svr4))
# define _USE_SUN_NSSWITCH_
#endif

#ifdef _USE_SUN_NSSWITCH_
# include <nsswitch.h>
#endif

#if defined(ultrix) || (defined(__osf__) && defined(__alpha))
# define _USE_DEC_SVC_CONF_
#endif

#ifdef _USE_DEC_SVC_CONF_
# include <sys/svcinfo.h>
#endif

int
switch_map_find(service, maptype, mapreturn)
	char *service;
	char *maptype[MAXMAPSTACK];
	short mapreturn[MAXMAPACTIONS];
{
	int svcno;

#ifdef _USE_SUN_NSSWITCH_
	struct __nsw_switchconfig *nsw_conf;
	enum __nsw_parse_err pserr;
	struct __nsw_lookup *lk;
	static struct __nsw_lookup lkp0 =
		{ "files", {1, 0, 0, 0}, NULL, NULL };
	static struct __nsw_switchconfig lkp_default =
		{ 0, "sendmail", 3, &lkp0 };

	for (svcno = 0; svcno < MAXMAPACTIONS; svcno++)
		mapreturn[svcno] = 0;

	if ((nsw_conf = __nsw_getconfig(service, &pserr)) == NULL)
		lk = lkp_default.lookups;
	else
		lk = nsw_conf->lookups;
	svcno = 0;
	while (lk != NULL)
	{
		maptype[svcno] = lk->service_name;
		if (lk->actions[__NSW_NOTFOUND] == __NSW_RETURN)
			mapreturn[MA_NOTFOUND] |= 1 << svcno;
		if (lk->actions[__NSW_TRYAGAIN] == __NSW_RETURN)
			mapreturn[MA_TRYAGAIN] |= 1 << svcno;
		if (lk->actions[__NSW_UNAVAIL] == __NSW_RETURN)
			mapreturn[MA_TRYAGAIN] |= 1 << svcno;
		svcno++;
		lk = lk->next;
	}
	return svcno;
#endif

#ifdef _USE_DEC_SVC_CONF_
	struct svcinfo *svcinfo;
	int svc;

	for (svcno = 0; svcno < MAXMAPACTIONS; svcno++)
		mapreturn[svcno] = 0;

	svcinfo = getsvc();
	if (svcinfo == NULL)
		goto punt;
	if (strcmp(service, "hosts") == 0)
		svc = SVC_HOSTS;
	else if (strcmp(service, "aliases") == 0)
		svc = SVC_ALIASES;
	else if (strcmp(service, "passwd") == 0)
		svc = SVC_PASSWD;
	else
		return -1;
	for (svcno = 0; svcno < SVC_PATHSIZE; svcno++)
	{
		switch (svcinfo->svcpath[svc][svcno])
		{
		  case SVC_LOCAL:
			maptype[svcno] = "files";
			break;

		  case SVC_YP:
			maptype[svcno] = "nis";
			break;

		  case SVC_BIND:
			maptype[svcno] = "dns";
			break;

#ifdef SVC_HESIOD
		  case SVC_HESIOD:
			maptype[svcno] = "hesiod";
			break;
#endif

		  case SVC_LAST:
			return svcno;
		}
	}
	return svcno;
#endif

#if !defined(_USE_SUN_NSSWITCH_) && !defined(_USE_DEC_SVC_CONF_)
	/*
	**  Fall-back mechanism.
	*/

	STAB *st;
	time_t now = curtime();

	for (svcno = 0; svcno < MAXMAPACTIONS; svcno++)
		mapreturn[svcno] = 0;

	if ((now - ServiceCacheTime) > (time_t) ServiceCacheMaxAge)
	{
		/* (re)read service switch */
		register FILE *fp;
		int sff = SFF_REGONLY|SFF_OPENASROOT|SFF_NOLOCK;

		if (!bitset(DBS_LINKEDSERVICESWITCHFILEINWRITABLEDIR, DontBlameSendmail))
			sff |= SFF_NOWLINK;

		if (ConfigFileRead)
			ServiceCacheTime = now;
		fp = safefopen(ServiceSwitchFile, O_RDONLY, 0, sff);
		if (fp != NULL)
		{
			char buf[MAXLINE];

			while (fgets(buf, sizeof buf, fp) != NULL)
			{
				register char *p;

				p = strpbrk(buf, "#\n");
				if (p != NULL)
					*p = '\0';
				p = strpbrk(buf, " \t");
				if (p != NULL)
					*p++ = '\0';
				if (buf[0] == '\0')
					continue;
				if (p == NULL)
				{
					sm_syslog(LOG_ERR, NOQID,
						  "Bad line on %.100s: %.100s",
						  ServiceSwitchFile,
						  buf);
					continue;
				}
				while (isspace(*p))
					p++;
				if (*p == '\0')
					continue;

				/*
				**  Find/allocate space for this service entry.
				**	Space for all of the service strings
				**	are allocated at once.  This means
				**	that we only have to free the first
				**	one to free all of them.
				*/

				st = stab(buf, ST_SERVICE, ST_ENTER);
				if (st->s_service[0] != NULL)
					free((void *) st->s_service[0]);
				p = newstr(p);
				for (svcno = 0; svcno < MAXMAPSTACK; )
				{
					if (*p == '\0')
						break;
					st->s_service[svcno++] = p;
					p = strpbrk(p, " \t");
					if (p == NULL)
						break;
					*p++ = '\0';
					while (isspace(*p))
						p++;
				}
				if (svcno < MAXMAPSTACK)
					st->s_service[svcno] = NULL;
			}
			fclose(fp);
		}
	}

	/* look up entry in cache */
	st = stab(service, ST_SERVICE, ST_FIND);
	if (st != NULL && st->s_service[0] != NULL)
	{
		/* extract data */
		svcno = 0;
		while (svcno < MAXMAPSTACK)
		{
			maptype[svcno] = st->s_service[svcno];
			if (maptype[svcno++] == NULL)
				break;
		}
		return --svcno;
	}
#endif

#if !defined(_USE_SUN_NSSWITCH_)
	/* if the service file doesn't work, use an absolute fallback */
# ifdef _USE_DEC_SVC_CONF_
  punt:
# endif
	for (svcno = 0; svcno < MAXMAPACTIONS; svcno++)
		mapreturn[svcno] = 0;
	svcno = 0;
	if (strcmp(service, "aliases") == 0)
	{
		maptype[svcno++] = "files";
# ifdef AUTO_NIS_ALIASES
#  ifdef NISPLUS
		maptype[svcno++] = "nisplus";
#  endif
#  ifdef NIS
		maptype[svcno++] = "nis";
#  endif
# endif
		return svcno;
	}
	if (strcmp(service, "hosts") == 0)
	{
#  if NAMED_BIND
		maptype[svcno++] = "dns";
#  else
#   if defined(sun) && !defined(BSD)
		/* SunOS */
		maptype[svcno++] = "nis";
#   endif
#  endif
		maptype[svcno++] = "files";
		return svcno;
	}
	return -1;
#endif
}
/*
**  USERNAME -- return the user id of the logged in user.
**
**	Parameters:
**		none.
**
**	Returns:
**		The login name of the logged in user.
**
**	Side Effects:
**		none.
**
**	Notes:
**		The return value is statically allocated.
*/

char *
username()
{
	static char *myname = NULL;
	extern char *getlogin();
	register struct passwd *pw;

	/* cache the result */
	if (myname == NULL)
	{
		myname = getlogin();
		if (myname == NULL || myname[0] == '\0')
		{
			pw = sm_getpwuid(RealUid);
			if (pw != NULL)
				myname = newstr(pw->pw_name);
		}
		else
		{
			uid_t uid = RealUid;

			myname = newstr(myname);
			if ((pw = sm_getpwnam(myname)) == NULL ||
			      (uid != 0 && uid != pw->pw_uid))
			{
				pw = sm_getpwuid(uid);
				if (pw != NULL)
					myname = newstr(pw->pw_name);
			}
		}
		if (myname == NULL || myname[0] == '\0')
		{
			syserr("554 Who are you?");
			myname = "postmaster";
		}
	}

	return (myname);
}
/*
**  TTYPATH -- Get the path of the user's tty
**
**	Returns the pathname of the user's tty.  Returns NULL if
**	the user is not logged in or if s/he has write permission
**	denied.
**
**	Parameters:
**		none
**
**	Returns:
**		pathname of the user's tty.
**		NULL if not logged in or write permission denied.
**
**	Side Effects:
**		none.
**
**	WARNING:
**		Return value is in a local buffer.
**
**	Called By:
**		savemail
*/

char *
ttypath()
{
	struct stat stbuf;
	register char *pathn;
	extern char *ttyname();
	extern char *getlogin();

	/* compute the pathname of the controlling tty */
	if ((pathn = ttyname(2)) == NULL && (pathn = ttyname(1)) == NULL &&
	    (pathn = ttyname(0)) == NULL)
	{
		errno = 0;
		return (NULL);
	}

	/* see if we have write permission */
	if (stat(pathn, &stbuf) < 0 || !bitset(S_IWOTH, stbuf.st_mode))
	{
		errno = 0;
		return (NULL);
	}

	/* see if the user is logged in */
	if (getlogin() == NULL)
		return (NULL);

	/* looks good */
	return (pathn);
}
/*
**  CHECKCOMPAT -- check for From and To person compatible.
**
**	This routine can be supplied on a per-installation basis
**	to determine whether a person is allowed to send a message.
**	This allows restriction of certain types of internet
**	forwarding or registration of users.
**
**	If the hosts are found to be incompatible, an error
**	message should be given using "usrerr" and an EX_ code
**	should be returned.  You can also set to->q_status to
**	a DSN-style status code.
**
**	EF_NO_BODY_RETN can be set in e->e_flags to suppress the
**	body during the return-to-sender function; this should be done
**	on huge messages.  This bit may already be set by the ESMTP
**	protocol.
**
**	Parameters:
**		to -- the person being sent to.
**
**	Returns:
**		an exit status
**
**	Side Effects:
**		none (unless you include the usrerr stuff)
*/

int
checkcompat(to, e)
	register ADDRESS *to;
	register ENVELOPE *e;
{
# ifdef lint
	if (to == NULL)
		to++;
# endif /* lint */

	if (tTd(49, 1))
		printf("checkcompat(to=%s, from=%s)\n",
			to->q_paddr, e->e_from.q_paddr);

# ifdef EXAMPLE_CODE
	/* this code is intended as an example only */
	register STAB *s;

	s = stab("arpa", ST_MAILER, ST_FIND);
	if (s != NULL && strcmp(e->e_from.q_mailer->m_name, "local") != 0 &&
	    to->q_mailer == s->s_mailer)
	{
		usrerr("553 No ARPA mail through this machine: see your system administration");
		/* e->e_flags |= EF_NO_BODY_RETN; to supress body on return */
		to->q_status = "5.7.1";
		return (EX_UNAVAILABLE);
	}
# endif /* EXAMPLE_CODE */
# ifdef TRUSTED_MAC
	return (sm_clear_user(to, e->e_label));
# else
	return (EX_OK);
# endif
}
/*
**  SETSIGNAL -- set a signal handler
**
**	This is essentially old BSD "signal(3)".
*/

sigfunc_t
setsignal(sig, handler)
	int sig;
	sigfunc_t handler;
{
#if defined(SYS5SIGNALS) || defined(BSD4_3)
# ifdef BSD4_3
	return signal(sig, handler);
# else
	return sigset(sig, handler);
# endif
#else
	struct sigaction n, o;

	bzero(&n, sizeof n);
# if USE_SA_SIGACTION
	n.sa_sigaction = (void(*)(int, siginfo_t *, void *)) handler;
	n.sa_flags = SA_RESTART|SA_SIGINFO;
# else
	n.sa_handler = handler;
#  ifdef SA_RESTART
	n.sa_flags = SA_RESTART;
#  endif
# endif
	if (sigaction(sig, &n, &o) < 0)
		return SIG_ERR;
	return o.sa_handler;
#endif
}
/*
**  BLOCKSIGNAL -- hold a signal to prevent delivery
**
**	Parameters:
**		sig -- the signal to block.
**
**	Returns:
**		1 signal was previously blocked
**		0 signal was not previously blocked
**		-1 on failure.
*/

int
blocksignal(sig)
	int sig;
{
#ifdef BSD4_3
# ifndef sigmask
#  define sigmask(s)	(1 << ((s) - 1))
# endif
	return (sigblock(sigmask(sig)) & sigmask(sig)) != 0;
#else
# ifdef ALTOS_SYSTEM_V
	sigfunc_t handler;

	handler = sigset(sig, SIG_HOLD);
	if (handler == SIG_ERR)
		return -1;
	else
		return handler == SIG_HOLD;
# else
	sigset_t sset, oset;

	sigemptyset(&sset);
	sigaddset(&sset, sig);
	if (sigprocmask(SIG_BLOCK, &sset, &oset) < 0)
		return -1;
	else
		return sigismember(&oset, sig);
# endif
#endif
}
/*
**  RELEASESIGNAL -- release a held signal
**
**	Parameters:
**		sig -- the signal to release.
**
**	Returns:
**		1 signal was previously blocked
**		0 signal was not previously blocked
**		-1 on failure.
*/

int
releasesignal(sig)
	int sig;
{
#ifdef BSD4_3
	return (sigsetmask(sigblock(0) & ~sigmask(sig)) & sigmask(sig)) != 0;
#else
# ifdef ALTOS_SYSTEM_V
	sigfunc_t handler;

	handler = sigset(sig, SIG_HOLD);
	if (sigrelse(sig) < 0)
		return -1;
	else
		return handler == SIG_HOLD;
# else
	sigset_t sset, oset;

	sigemptyset(&sset);
	sigaddset(&sset, sig);
	if (sigprocmask(SIG_UNBLOCK, &sset, &oset) < 0)
		return -1;
	else
		return sigismember(&oset, sig);
# endif
#endif
}
/*
**  HOLDSIGS -- arrange to hold all signals
**
**	Parameters:
**		none.
**
**	Returns:
**		none.
**
**	Side Effects:
**		Arranges that signals are held.
*/

void
holdsigs()
{
}
/*
**  RLSESIGS -- arrange to release all signals
**
**	This undoes the effect of holdsigs.
**
**	Parameters:
**		none.
**
**	Returns:
**		none.
**
**	Side Effects:
**		Arranges that signals are released.
*/

void
rlsesigs()
{
}
/*
**  INIT_MD -- do machine dependent initializations
**
**	Systems that have global modes that should be set should do
**	them here rather than in main.
*/

#ifdef _AUX_SOURCE
# include <compat.h>
#endif

#if SHARE_V1
# include <shares.h>
#endif

void
init_md(argc, argv)
	int argc;
	char **argv;
{
#ifdef _AUX_SOURCE
	setcompat(getcompat() | COMPAT_BSDPROT);
#endif

#ifdef SUN_EXTENSIONS
	init_md_sun();
#endif

#if _CONVEX_SOURCE
	/* keep gethostby*() from stripping the local domain name */
	set_domain_trim_off();
#endif
#ifdef __QNX__
	/*
	**  Due to QNX's network distributed nature, you can target a tcpip
	**  stack on a different node in the qnx network; this patch lets
	**  this feature work.  The __sock_locate() must be done before the
	**  environment is clear.
	*/
	__sock_locate();
#endif
#if SECUREWARE || defined(_SCO_unix_)
	set_auth_parameters(argc, argv);

# ifdef _SCO_unix_
	/*
	**  This is required for highest security levels (the kernel
	**  won't let it call set*uid() or run setuid binaries without
	**  it).  It may be necessary on other SECUREWARE systems.
	*/

	if (getluid() == -1)
		setluid(0);
# endif
#endif

#ifdef VENDOR_DEFAULT
	VendorCode = VENDOR_DEFAULT;
#else
	VendorCode = VENDOR_BERKELEY;
#endif
}
/*
**  INIT_VENDOR_MACROS -- vendor-dependent macro initializations
**
**	Called once, on startup.
**
**	Parameters:
**		e -- the global envelope.
**
**	Returns:
**		none.
**
**	Side Effects:
**		vendor-dependent.
*/

void
init_vendor_macros(e)
	register ENVELOPE *e;
{
}
/*
**  GETLA -- get the current load average
**
**	This code stolen from la.c.
**
**	Parameters:
**		none.
**
**	Returns:
**		The current load average as an integer.
**
**	Side Effects:
**		none.
*/

/* try to guess what style of load average we have */
#define LA_ZERO		1	/* always return load average as zero */
#define LA_INT		2	/* read kmem for avenrun; interpret as long */
#define LA_FLOAT	3	/* read kmem for avenrun; interpret as float */
#define LA_SUBR		4	/* call getloadavg */
#define LA_MACH		5	/* MACH load averages (as on NeXT boxes) */
#define LA_SHORT	6	/* read kmem for avenrun; interpret as short */
#define LA_PROCSTR	7	/* read string ("1.17") from /proc/loadavg */
#define LA_READKSYM	8	/* SVR4: use MIOC_READKSYM ioctl call */
#define LA_DGUX		9	/* special DGUX implementation */
#define LA_HPUX		10	/* special HPUX implementation */
#define LA_IRIX6	11	/* special IRIX 6.2 implementation */
#define LA_KSTAT	12	/* special Solaris kstat(3k) implementation */
#define LA_DEVSHORT	13	/* read short from a device */
#define LA_ALPHAOSF	14	/* Digital UNIX (OSF/1 on Alpha) table() call */

/* do guesses based on general OS type */
#ifndef LA_TYPE
# define LA_TYPE	LA_ZERO
#endif

#ifndef FSHIFT
# if defined(unixpc)
#  define FSHIFT	5
# endif

# if defined(__alpha) || defined(IRIX)
#  define FSHIFT	10
# endif

#endif

#ifndef FSHIFT
# define FSHIFT		8
#endif

#ifndef FSCALE
# define FSCALE		(1 << FSHIFT)
#endif

#ifndef LA_AVENRUN
# ifdef SYSTEM5
#  define LA_AVENRUN	"avenrun"
# else
#  define LA_AVENRUN	"_avenrun"
# endif
#endif

/* _PATH_KMEM should be defined in <paths.h> */
#ifndef _PATH_KMEM
# define _PATH_KMEM	"/dev/kmem"
#endif

#if (LA_TYPE == LA_INT) || (LA_TYPE == LA_FLOAT) || (LA_TYPE == LA_SHORT)

#include <nlist.h>

/* _PATH_UNIX should be defined in <paths.h> */
#ifndef _PATH_UNIX
# if defined(SYSTEM5)
#  define _PATH_UNIX	"/unix"
# else
#  define _PATH_UNIX	"/vmunix"
# endif
#endif

#ifdef _AUX_SOURCE
struct nlist	Nl[2];
#else
struct nlist	Nl[] =
{
	{ LA_AVENRUN },
	{ 0 },
};
#endif
#define	X_AVENRUN	0

int
getla()
{
	static int kmem = -1;
#if LA_TYPE == LA_INT
	long avenrun[3];
#else
# if LA_TYPE == LA_SHORT
	short avenrun[3];
# else
	double avenrun[3];
# endif
#endif
	extern int errno;
	extern off_t lseek();

	if (kmem < 0)
	{
#ifdef _AUX_SOURCE
		strcpy(Nl[X_AVENRUN].n_name, LA_AVENRUN);
		Nl[1].n_name[0] = '\0';
#endif

#if defined(_AIX3) || defined(_AIX4)
		if (knlist(Nl, 1, sizeof Nl[0]) < 0)
#else
		if (nlist(_PATH_UNIX, Nl) < 0)
#endif
		{
			if (tTd(3, 1))
				printf("getla: nlist(%s): %s\n", _PATH_UNIX,
					errstring(errno));
			return (-1);
		}
		if (Nl[X_AVENRUN].n_value == 0)
		{
			if (tTd(3, 1))
				printf("getla: nlist(%s, %s) ==> 0\n",
					_PATH_UNIX, LA_AVENRUN);
			return (-1);
		}
#ifdef NAMELISTMASK
		Nl[X_AVENRUN].n_value &= NAMELISTMASK;
#endif

		kmem = open(_PATH_KMEM, 0, 0);
		if (kmem < 0)
		{
			if (tTd(3, 1))
				printf("getla: open(/dev/kmem): %s\n",
					errstring(errno));
			return (-1);
		}
		(void) fcntl(kmem, F_SETFD, 1);
	}
	if (tTd(3, 20))
		printf("getla: symbol address = %#lx\n",
			(u_long) Nl[X_AVENRUN].n_value);
	if (lseek(kmem, (off_t) Nl[X_AVENRUN].n_value, SEEK_SET) == -1 ||
	    read(kmem, (char *) avenrun, sizeof(avenrun)) < sizeof(avenrun))
	{
		/* thank you Ian */
		if (tTd(3, 1))
			printf("getla: lseek or read: %s\n", errstring(errno));
		return (-1);
	}
# if (LA_TYPE == LA_INT) || (LA_TYPE == LA_SHORT)
	if (tTd(3, 5))
	{
#  if LA_TYPE == LA_SHORT
		printf("getla: avenrun = %d", avenrun[0]);
		if (tTd(3, 15))
			printf(", %d, %d", avenrun[1], avenrun[2]);
#  else
		printf("getla: avenrun = %ld", avenrun[0]);
		if (tTd(3, 15))
			printf(", %ld, %ld", avenrun[1], avenrun[2]);
#  endif
		printf("\n");
	}
	if (tTd(3, 1))
		printf("getla: %d\n", (int) (avenrun[0] + FSCALE/2) >> FSHIFT);
	return ((int) (avenrun[0] + FSCALE/2) >> FSHIFT);
# else /* LA_TYPE == LA_FLOAT */
	if (tTd(3, 5))
	{
		printf("getla: avenrun = %g", avenrun[0]);
		if (tTd(3, 15))
			printf(", %g, %g", avenrun[1], avenrun[2]);
		printf("\n");
	}
	if (tTd(3, 1))
		printf("getla: %d\n", (int) (avenrun[0] +0.5));
	return ((int) (avenrun[0] + 0.5));
# endif
}

#endif /* LA_TYPE == LA_INT or LA_SHORT or LA_FLOAT */

#if LA_TYPE == LA_READKSYM

# include <sys/ksym.h>

getla()
{
	static int kmem = -1;
	long avenrun[3];
	extern int errno;
	struct mioc_rksym mirk;

	if (kmem < 0)
	{
		kmem = open("/dev/kmem", 0, 0);
		if (kmem < 0)
		{
			if (tTd(3, 1))
				printf("getla: open(/dev/kmem): %s\n",
					errstring(errno));
			return (-1);
		}
		(void) fcntl(kmem, F_SETFD, 1);
	}
	mirk.mirk_symname = LA_AVENRUN;
	mirk.mirk_buf = avenrun;
	mirk.mirk_buflen = sizeof(avenrun);
	if (ioctl(kmem, MIOC_READKSYM, &mirk) < 0)
	{
		if (tTd(3, 1))
			printf("getla: ioctl(MIOC_READKSYM) failed: %s\n",
				errstring(errno));
		return -1;
	}
	if (tTd(3, 5))
	{
		printf("getla: avenrun = %d", avenrun[0]);
		if (tTd(3, 15))
			printf(", %d, %d", avenrun[1], avenrun[2]);
		printf("\n");
	}
	if (tTd(3, 1))
		printf("getla: %d\n", (int) (avenrun[0] + FSCALE/2) >> FSHIFT);
	return ((int) (avenrun[0] + FSCALE/2) >> FSHIFT);
}

#endif /* LA_TYPE == LA_READKSYM */

#if LA_TYPE == LA_DGUX

# include <sys/dg_sys_info.h>

int
getla()
{
	struct dg_sys_info_load_info load_info;

	dg_sys_info((long *)&load_info,
		DG_SYS_INFO_LOAD_INFO_TYPE, DG_SYS_INFO_LOAD_VERSION_0);

	if (tTd(3, 1))
		printf("getla: %d\n", (int) (load_info.one_minute + 0.5));

	return((int) (load_info.one_minute + 0.5));
}

#endif /* LA_TYPE == LA_DGUX */

#if LA_TYPE == LA_HPUX

/* forward declarations to keep gcc from complaining */
struct pst_dynamic;
struct pst_status;
struct pst_static;
struct pst_vminfo;
struct pst_diskinfo;
struct pst_processor;
struct pst_lv;
struct pst_swapinfo;

# include <sys/param.h>
# include <sys/pstat.h>

int
getla()
{
	struct pst_dynamic pstd;

	if (pstat_getdynamic(&pstd, sizeof(struct pst_dynamic),
			     (size_t) 1, 0) == -1)
		return 0;

	if (tTd(3, 1))
		printf("getla: %d\n", (int) (pstd.psd_avg_1_min + 0.5));

	return (int) (pstd.psd_avg_1_min + 0.5);
}

#endif /* LA_TYPE == LA_HPUX */

#if LA_TYPE == LA_SUBR

int
getla()
{
	double avenrun[3];

	if (getloadavg(avenrun, sizeof(avenrun) / sizeof(avenrun[0])) < 0)
	{
		if (tTd(3, 1))
			perror("getla: getloadavg failed:");
		return (-1);
	}
	if (tTd(3, 1))
		printf("getla: %d\n", (int) (avenrun[0] +0.5));
	return ((int) (avenrun[0] + 0.5));
}

#endif /* LA_TYPE == LA_SUBR */

#if LA_TYPE == LA_MACH

/*
**  This has been tested on NEXTSTEP release 2.1/3.X.
*/

#if defined(NX_CURRENT_COMPILER_RELEASE) && NX_CURRENT_COMPILER_RELEASE > NX_COMPILER_RELEASE_3_0
# include <mach/mach.h>
#else
# include <mach.h>
#endif

int
getla()
{
	processor_set_t default_set;
	kern_return_t error;
	unsigned int info_count;
	struct processor_set_basic_info info;
	host_t host;

	error = processor_set_default(host_self(), &default_set);
	if (error != KERN_SUCCESS)
	{
		if (tTd(3, 1))
			perror("getla: processor_set_default failed:");
		return -1;
	}
	info_count = PROCESSOR_SET_BASIC_INFO_COUNT;
	if (processor_set_info(default_set, PROCESSOR_SET_BASIC_INFO,
			       &host, (processor_set_info_t)&info,
			       &info_count) != KERN_SUCCESS)
	{
		if (tTd(3, 1))
			perror("getla: processor_set_info failed:");
		return -1;
	}
	if (tTd(3, 1))
		printf("getla: %d\n", (int) (info.load_average + (LOAD_SCALE / 2)) / LOAD_SCALE);
	return (int) (info.load_average + (LOAD_SCALE / 2)) / LOAD_SCALE;
}

#endif /* LA_TYPE == LA_MACH */

#if LA_TYPE == LA_PROCSTR

/*
**  Read /proc/loadavg for the load average.  This is assumed to be
**  in a format like "0.15 0.12 0.06".
**
**	Initially intended for Linux.  This has been in the kernel
**	since at least 0.99.15.
*/

# ifndef _PATH_LOADAVG
#  define _PATH_LOADAVG	"/proc/loadavg"
# endif

int
getla()
{
	double avenrun;
	register int result;
	FILE *fp;

	fp = fopen(_PATH_LOADAVG, "r");
	if (fp == NULL)
	{
		if (tTd(3, 1))
			printf("getla: fopen(%s): %s\n",
				_PATH_LOADAVG, errstring(errno));
		return -1;
	}
	result = fscanf(fp, "%lf", &avenrun);
	fclose(fp);
	if (result != 1)
	{
		if (tTd(3, 1))
			printf("getla: fscanf() = %d: %s\n",
				result, errstring(errno));
		return -1;
	}

	if (tTd(3, 1))
		printf("getla(): %.2f\n", avenrun);

	return ((int) (avenrun + 0.5));
}

#endif /* LA_TYPE == LA_PROCSTR */

#if LA_TYPE == LA_IRIX6
#include <sys/sysmp.h>

int getla(void)
{
	static int kmem = -1;
	int avenrun[3];

	if (kmem < 0)
	{
		kmem = open(_PATH_KMEM, 0, 0);
		if (kmem < 0)
		{
			if (tTd(3, 1))
				printf("getla: open(%s): %s\n", _PATH_KMEM,
					errstring(errno));
			return -1;
		}
		(void) fcntl(kmem, F_SETFD, 1);
	}

	if (lseek(kmem, (sysmp(MP_KERNADDR, MPKA_AVENRUN) & 0x7fffffff), SEEK_SET) == -1 || 
	    read(kmem, (char *)avenrun, sizeof(avenrun)) < sizeof(avenrun)) 
	{
		if (tTd(3, 1))
			printf("getla: lseek or read: %s\n",
			       errstring(errno));
		return -1;
	}
	if (tTd(3, 5))
	{
		printf("getla: avenrun = %ld", (long int) avenrun[0]);
		if (tTd(3, 15))
			printf(", %ld, %ld",
			       (long int) avenrun[1], (long int) avenrun[2]);
		printf("\n");
	}

	if (tTd(3, 1))
		printf("getla: %d\n", (int) (avenrun[0] + FSCALE/2) >> FSHIFT);
	return ((int) (avenrun[0] + FSCALE/2) >> FSHIFT);

}
#endif

#if LA_TYPE == LA_KSTAT

#include <kstat.h>

int
getla()
{
	static kstat_ctl_t *kc = NULL;
	static kstat_t *ksp = NULL;
	kstat_named_t *ksn;
	int la;

	if (kc == NULL)		/* if not initialized before */
		kc = kstat_open();
	if (kc == NULL)
	{
		if (tTd(3, 1))
			printf("getla: kstat_open(): %s\n",
				errstring(errno));
		return -1;
	}
	if (ksp == NULL)
		ksp = kstat_lookup(kc, "unix", 0, "system_misc");
	if (ksp == NULL)
	{
		if (tTd(3, 1))
			printf("getla: kstat_lookup(): %s\n",
				errstring(errno));
		return -1;
	}
	if (kstat_read(kc, ksp, NULL) < 0)
	{
		if (tTd(3, 1))
			printf("getla: kstat_read(): %s\n",
				errstring(errno));
		return -1;
	}
	ksn = (kstat_named_t *) kstat_data_lookup(ksp, "avenrun_1min");
	la = ((double)ksn->value.ul + FSCALE/2) / FSCALE;
	/* kstat_close(kc); /o do not close for fast access */
	return la;
}

#endif /* LA_TYPE == LA_KSTAT */

#if LA_TYPE == LA_DEVSHORT

/*
**  Read /dev/table/avenrun for the load average.  This should contain
**  three shorts for the 1, 5, and 15 minute loads.  We only read the
**  first, since that's all we care about.
**
**	Intended for SCO OpenServer 5.
*/

# ifndef _PATH_AVENRUN
#  define _PATH_AVENRUN	"/dev/table/avenrun"
# endif

int
getla()
{
	static int afd = -1;
	short avenrun;
	int loadav;
	int r;

	errno = EBADF;

	if (afd == -1 || lseek(afd, 0L, SEEK_SET) == -1)
	{
		if (errno != EBADF)
			return -1;
		afd = open(_PATH_AVENRUN, O_RDONLY|O_SYNC);
		if (afd < 0)
		{
			sm_syslog(LOG_ERR, NOQID,
				"can't open %s: %m",
				_PATH_AVENRUN);
			return -1;
		}
	}

	r = read(afd, &avenrun, sizeof avenrun);

	if (tTd(3, 5))
		printf("getla: avenrun = %d\n", avenrun);
	loadav = (int) (avenrun + FSCALE/2) >> FSHIFT;
	if (tTd(3, 1))
		printf("getla: %d\n", loadav);
	return loadav;
}

#endif /* LA_TYPE == LA_DEVSHORT */

#if LA_TYPE == LA_ALPHAOSF
struct rtentry;
struct mbuf;
# include <sys/table.h>

int getla()
{
	int ave = 0;
	struct tbl_loadavg tab;

	if (table(TBL_LOADAVG, 0, &tab, 1, sizeof(tab)) == -1)
	{
		if (tTd(3, 1))
			printf("getla: table %s\n", errstring(errno));
		return (-1);
	}

	if (tTd(3, 1))
		printf("getla: scale = %d\n", tab.tl_lscale);

	if (tab.tl_lscale)
		ave = (tab.tl_avenrun.l[0] + (tab.tl_lscale/2)) / tab.tl_lscale;
	else
		ave = (int) (tab.tl_avenrun.d[0] + 0.5);

	if (tTd(3, 1))
		printf("getla: %d\n", ave);

	return ave;
}

#endif

#if LA_TYPE == LA_ZERO

int
getla()
{
	if (tTd(3, 1))
		printf("getla: ZERO\n");
	return (0);
}

#endif /* LA_TYPE == LA_ZERO */

/*
 * Copyright 1989 Massachusetts Institute of Technology
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of M.I.T. not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  M.I.T. makes no representations about the
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * M.I.T. DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL M.I.T.
 * BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Authors:  Many and varied...
 */

/* Non Apollo stuff removed by Don Lewis 11/15/93 */
#ifndef lint
static char  rcsid[] = "@(#)$Id: conf.c,v 1.1 1999/11/05 00:56:09 mende Exp $";
#endif /* !lint */

#ifdef apollo
# undef volatile
#    include <apollo/base.h>

/* ARGSUSED */
int getloadavg( call_data )
     caddr_t	call_data;	/* pointer to (double) return value */
{
     double *avenrun = (double *) call_data;
     int i;
     status_$t      st;
     long loadav[3];
     proc1_$get_loadav(loadav, &st);
     *avenrun = loadav[0] / (double) (1 << 16);
     return(0);
}
#   endif /* apollo */
/*
**  SHOULDQUEUE -- should this message be queued or sent?
**
**	Compares the message cost to the load average to decide.
**
**	Parameters:
**		pri -- the priority of the message in question.
**		ctime -- the message creation time.
**
**	Returns:
**		TRUE -- if this message should be queued up for the
**			time being.
**		FALSE -- if the load is low enough to send this message.
**
**	Side Effects:
**		none.
*/

extern int	get_num_procs_online __P((void));

bool
shouldqueue(pri, ctime)
	long pri;
	time_t ctime;
{
	bool rval;
	int queuela = QueueLA * get_num_procs_online();

	if (tTd(3, 30))
		printf("shouldqueue: CurrentLA=%d, pri=%ld: ", CurrentLA, pri);
	if (CurrentLA < queuela)
	{
		if (tTd(3, 30))
			printf("FALSE (CurrentLA < QueueLA)\n");
		return (FALSE);
	}
#if 0	/* this code is reported to cause oscillation around RefuseLA */
	if (CurrentLA >= RefuseLA && QueueLA < RefuseLA)
	{
		if (tTd(3, 30))
			printf("TRUE (CurrentLA >= RefuseLA)\n");
		return (TRUE);
	}
#endif
	rval = pri > (QueueFactor / (CurrentLA - queuela + 1));
	if (tTd(3, 30))
		printf("%s (by calculation)\n", rval ? "TRUE" : "FALSE");
	return rval;
}
/*
**  REFUSECONNECTIONS -- decide if connections should be refused
**
**	Parameters:
**		port -- port number (for error messages only)
**
**	Returns:
**		TRUE if incoming SMTP connections should be refused
**			(for now).
**		FALSE if we should accept new work.
**
**	Side Effects:
**		Sets process title when it is rejecting connections.
*/

bool
refuseconnections(port)
	int port;
{
	int refusela = RefuseLA * get_num_procs_online();
	time_t now;
	static time_t lastconn = (time_t) 0;
	static int conncnt = 0;
	extern bool enoughdiskspace __P((long));

#ifdef XLA
	if (!xla_smtp_ok())
		return TRUE;
#endif

	now = curtime();
	if (now != lastconn)
	{
		lastconn = now;
		conncnt = 0;
	}
	else if (conncnt++ > ConnRateThrottle && ConnRateThrottle > 0)
	{
		/* sleep to flatten out connection load */
		sm_setproctitle(TRUE, "deferring connections on port %d: %d per second",
			port, ConnRateThrottle);
		if (LogLevel >= 14)
			sm_syslog(LOG_INFO, NOQID,
				"deferring connections on port %d: %d per second",
				port, ConnRateThrottle);
		sleep(1);
	}

	CurrentLA = getla();
	if (CurrentLA >= refusela)
	{
		sm_setproctitle(TRUE, "rejecting connections on port %d: load average: %d",
			port, CurrentLA);
		if (LogLevel >= 14)
			sm_syslog(LOG_INFO, NOQID,
				"rejecting connections on port %d: load average: %d",
				port, CurrentLA);
		return TRUE;
	}

	if (!enoughdiskspace(MinBlocksFree + 1))
	{
		sm_setproctitle(TRUE, "rejecting connections on port %d: min free: %d",
			port, MinBlocksFree);
		if (LogLevel >= 14)
			sm_syslog(LOG_INFO, NOQID,
				"rejecting connections on port %d: min free: %d",
				port, MinBlocksFree);
		return TRUE;
	}

	if (MaxChildren > 0 && CurChildren >= MaxChildren)
	{
		proc_list_probe();
		if (CurChildren >= MaxChildren)
		{
			sm_setproctitle(TRUE, "rejecting connections on port %d: %d children, max %d",
				port, CurChildren, MaxChildren);
			if (LogLevel >= 14)
				sm_syslog(LOG_INFO, NOQID,
					"rejecting connections on port %d: %d children, max %d",
					port, CurChildren, MaxChildren);
			return TRUE;
		}
	}

	return FALSE;
}
/*
**  SETPROCTITLE -- set process title for ps
**
**	Parameters:
**		fmt -- a printf style format string.
**		a, b, c -- possible parameters to fmt.
**
**	Returns:
**		none.
**
**	Side Effects:
**		Clobbers argv of our main procedure so ps(1) will
**		display the title.
*/

#define SPT_NONE	0	/* don't use it at all */
#define SPT_REUSEARGV	1	/* cover argv with title information */
#define SPT_BUILTIN	2	/* use libc builtin */
#define SPT_PSTAT	3	/* use pstat(PSTAT_SETCMD, ...) */
#define SPT_PSSTRINGS	4	/* use PS_STRINGS->... */
#define SPT_SYSMIPS	5	/* use sysmips() supported by NEWS-OS 6 */
#define SPT_SCO		6	/* write kernel u. area */
#define SPT_CHANGEARGV	7	/* write our own strings into argv[] */

#ifndef SPT_TYPE
# define SPT_TYPE	SPT_REUSEARGV
#endif

#if SPT_TYPE != SPT_NONE && SPT_TYPE != SPT_BUILTIN

# if SPT_TYPE == SPT_PSTAT
#  include <sys/pstat.h>
# endif
# if SPT_TYPE == SPT_PSSTRINGS
#  include <machine/vmparam.h>
#  include <sys/exec.h>
#  ifndef PS_STRINGS	/* hmmmm....  apparently not available after all */
#   undef SPT_TYPE
#   define SPT_TYPE	SPT_REUSEARGV
#  else
#   ifndef NKPDE			/* FreeBSD 2.0 */
#    define NKPDE 63
typedef unsigned int	*pt_entry_t;
#   endif
#  endif
# endif

# if SPT_TYPE == SPT_PSSTRINGS || SPT_TYPE == SPT_CHANGEARGV
#  define SETPROC_STATIC	static
# else
#  define SETPROC_STATIC
# endif

# if SPT_TYPE == SPT_SYSMIPS
#  include <sys/sysmips.h>
#  include <sys/sysnews.h>
# endif

# if SPT_TYPE == SPT_SCO
#  include <sys/immu.h>
#  include <sys/dir.h>
#  include <sys/user.h>
#  include <sys/fs/s5param.h>
#  if PSARGSZ > MAXLINE
#   define SPT_BUFSIZE	PSARGSZ
#  endif
# endif

# ifndef SPT_PADCHAR
#  define SPT_PADCHAR	' '
# endif

#endif /* SPT_TYPE != SPT_NONE && SPT_TYPE != SPT_BUILTIN */

# ifndef SPT_BUFSIZE
#  define SPT_BUFSIZE	MAXLINE
# endif

/*
**  Pointers for setproctitle.
**	This allows "ps" listings to give more useful information.
*/

char		**Argv = NULL;		/* pointer to argument vector */
char		*LastArgv = NULL;	/* end of argv */

void
initsetproctitle(argc, argv, envp)
	int argc;
	char **argv;
	char **envp;
{
	register int i, envpsize = 0;
	extern char **environ;

	/*
	**  Move the environment so setproctitle can use the space at
	**  the top of memory.
	*/

	for (i = 0; envp[i] != NULL; i++)
		envpsize += strlen(envp[i]) + 1;
	environ = (char **) xalloc(sizeof (char *) * (i + 1));
	for (i = 0; envp[i] != NULL; i++)
		environ[i] = newstr(envp[i]);
	environ[i] = NULL;

	/*
	**  Save start and extent of argv for setproctitle.
	*/

	Argv = argv;

	/*
	**  Determine how much space we can use for setproctitle.  
	**  Use all contiguous argv and envp pointers starting at argv[0]
 	*/
	for (i = 0; i < argc; i++)
	{
		if (i==0 || LastArgv + 1 == argv[i])
			LastArgv = argv[i] + strlen(argv[i]);
		else
			continue;
	}
	for (i=0; envp[i] != NULL; i++)
	{
		if (LastArgv + 1 == envp[i])
			LastArgv = envp[i] + strlen(envp[i]);
		else
			continue;
	}
}

#if SPT_TYPE != SPT_BUILTIN


/*VARARGS1*/
void
# ifdef __STDC__
setproctitle(const char *fmt, ...)
# else
setproctitle(fmt, va_alist)
	const char *fmt;
	va_dcl
# endif
{
# if SPT_TYPE != SPT_NONE
	register char *p;
	register int i;
	SETPROC_STATIC char buf[SPT_BUFSIZE];
	VA_LOCAL_DECL
#  if SPT_TYPE == SPT_PSTAT
	union pstun pst;
#  endif
#  if SPT_TYPE == SPT_SCO
	off_t seek_off;
	static int kmem = -1;
	static int kmempid = -1;
	struct user u;
#  endif

	p = buf;

	/* print sendmail: heading for grep */
	(void) strcpy(p, "sendmail: ");
	p += strlen(p);

	/* print the argument string */
	VA_START(fmt);
	(void) vsnprintf(p, SPACELEFT(buf, p), fmt, ap);
	VA_END;

	i = strlen(buf);

#  if SPT_TYPE == SPT_PSTAT
	pst.pst_command = buf;
	pstat(PSTAT_SETCMD, pst, i, 0, 0);
#  endif
#  if SPT_TYPE == SPT_PSSTRINGS
	PS_STRINGS->ps_nargvstr = 1;
	PS_STRINGS->ps_argvstr = buf;
#  endif
#  if SPT_TYPE == SPT_SYSMIPS
	sysmips(SONY_SYSNEWS, NEWS_SETPSARGS, buf);
#  endif
#  if SPT_TYPE == SPT_SCO
	if (kmem < 0 || kmempid != getpid())
	{
		if (kmem >= 0)
			close(kmem);
		kmem = open(_PATH_KMEM, O_RDWR, 0);
		if (kmem < 0)
			return;
		(void) fcntl(kmem, F_SETFD, 1);
		kmempid = getpid();
	}
	buf[PSARGSZ - 1] = '\0';
	seek_off = UVUBLK + (off_t) u.u_psargs - (off_t) &u;
	if (lseek(kmem, (off_t) seek_off, SEEK_SET) == seek_off)
		(void) write(kmem, buf, PSARGSZ);
#  endif
#  if SPT_TYPE == SPT_REUSEARGV
	if (i > LastArgv - Argv[0] - 2)
	{
		i = LastArgv - Argv[0] - 2;
		buf[i] = '\0';
	}
	(void) strcpy(Argv[0], buf);
	p = &Argv[0][i];
	while (p < LastArgv)
		*p++ = SPT_PADCHAR;
	Argv[1] = NULL;
#  endif
#  if SPT_TYPE == SPT_CHANGEARGV
	Argv[0] = buf;
	Argv[1] = 0;
#  endif
# endif /* SPT_TYPE != SPT_NONE */
}

#endif /* SPT_TYPE != SPT_BUILTIN */
/*
**  SM_SETPROCTITLE -- set process task and set process title for ps
**
**	Possibly set process status and call setproctitle() to
**	change the ps display.
**
**	Parameters:
**		status -- whether or not to store as process status
**		fmt -- a printf style format string.
**		a, b, c -- possible parameters to fmt.
**
**	Returns:
**		none.
*/

/*VARARGS2*/
void
# ifdef __STDC__
sm_setproctitle(bool status, const char *fmt, ...)
# else
sm_setproctitle(status, fmt, va_alist)
	bool status;
	const char *fmt;
	va_dcl
#endif
{
	char buf[SPT_BUFSIZE];

	VA_LOCAL_DECL
	/* print the argument string */
	VA_START(fmt);
	(void) vsnprintf(buf, SPT_BUFSIZE, fmt, ap);
	VA_END;

	if (status)
		proc_list_set(getpid(), buf);
	setproctitle("%s", buf);
}
/*
**  WAITFOR -- wait for a particular process id.
**
**	Parameters:
**		pid -- process id to wait for.
**
**	Returns:
**		status of pid.
**		-1 if pid never shows up.
**
**	Side Effects:
**		none.
*/

int
waitfor(pid)
	pid_t pid;
{
#ifdef WAITUNION
	union wait st;
#else
	auto int st;
#endif
	pid_t i;
#if defined(ISC_UNIX) || defined(_SCO_unix_)
	int savesig;
#endif

	do
	{
		errno = 0;
#if defined(ISC_UNIX) || defined(_SCO_unix_)
		savesig = releasesignal(SIGCHLD);
#endif
		i = wait(&st);
#if defined(ISC_UNIX) || defined(_SCO_unix_)
		if (savesig > 0)
			blocksignal(SIGCHLD);
#endif
		if (i > 0)
			proc_list_drop(i);
	} while ((i >= 0 || errno == EINTR) && i != pid);
	if (i < 0)
		return -1;
#ifdef WAITUNION
	return st.w_status;
#else
	return st;
#endif
}
/*
**  REAPCHILD -- pick up the body of my child, lest it become a zombie
**
**	Parameters:
**		sig -- the signal that got us here (unused).
**
**	Returns:
**		none.
**
**	Side Effects:
**		Picks up extant zombies.
*/

SIGFUNC_DECL
reapchild(sig)
	int sig;
{
	int olderrno = errno;
	pid_t pid;
# ifdef HASWAITPID
	auto int status;
	int count;

	count = 0;
	while ((pid = waitpid(-1, &status, WNOHANG)) > 0)
	{
		if (count++ > 1000)
		{
			if (LogLevel > 0)
				sm_syslog(LOG_ALERT, NOQID,
					"reapchild: waitpid loop: pid=%d, status=%x",
					pid, status);
			break;
		}
		proc_list_drop(pid);
	}
# else
# ifdef WNOHANG
	union wait status;

	while ((pid = wait3(&status, WNOHANG, (struct rusage *) NULL)) > 0)
		proc_list_drop(pid);
# else /* WNOHANG */
	auto int status;

	/*
	**  Catch one zombie -- we will be re-invoked (we hope) if there
	**  are more.  Unreliable signals probably break this, but this
	**  is the "old system" situation -- waitpid or wait3 are to be
	**  strongly preferred.
	*/

	if ((pid = wait(&status)) > 0)
		proc_list_drop(pid);
# endif /* WNOHANG */
# endif
# ifdef SYS5SIGNALS
	(void) setsignal(SIGCHLD, reapchild);
# endif
	errno = olderrno;
	return SIGFUNC_RETURN;
}
/*
**  PUTENV -- emulation of putenv() in terms of setenv()
**
**	Not needed on Posix-compliant systems.
**	This doesn't have full Posix semantics, but it's good enough
**		for sendmail.
**
**	Parameter:
**		env -- the environment to put.
**
**	Returns:
**		none.
*/

#ifdef NEEDPUTENV

# if NEEDPUTENV == 2		/* no setenv(3) call available */

int
putenv(str)
	char *str;
{
	char **current;
	int matchlen, envlen=0;
	char *tmp;
	char **newenv;
	static int first=1;
	extern char **environ;

	/*
	 * find out how much of str to match when searching
	 * for a string to replace.
	 */
	if ((tmp = strchr(str, '=')) == NULL || tmp == str)
		matchlen = strlen(str);
	else
		matchlen = (int) (tmp - str);
	++matchlen;

	/*
	 * Search for an existing string in the environment and find the
	 * length of environ.  If found, replace and exit.
	 */
	for (current=environ; *current; current++) {
		++envlen;

		if (strncmp(str, *current, matchlen) == 0) {
			/* found it, now insert the new version */
			*current = (char *)str;
			return(0);
		}
	}

	/*
	 * There wasn't already a slot so add space for a new slot.
	 * If this is our first time through, use malloc(), else realloc().
	 */
	if (first) {
		newenv = (char **) malloc(sizeof(char *) * (envlen + 2));
		if (newenv == NULL)
			return(-1);

		first=0;
		(void) memcpy(newenv, environ, sizeof(char *) * envlen);
	} else {
		newenv = (char **) realloc((char *)environ, sizeof(char *) * (envlen + 2));
		if (newenv == NULL)
			return(-1);
	}

	/* actually add in the new entry */
	environ = newenv;
	environ[envlen] = (char *)str;
	environ[envlen+1] = NULL;

	return(0);
}

#else			/* implement putenv() in terms of setenv() */

int
putenv(env)
	char *env;
{
	char *p;
	int l;
	char nbuf[100];

	p = strchr(env, '=');
	if (p == NULL)
		return 0;
	l = p - env;
	if (l > sizeof nbuf - 1)
		l = sizeof nbuf - 1;
	bcopy(env, nbuf, l);
	nbuf[l] = '\0';
	return setenv(nbuf, ++p, 1);
}

# endif
#endif
/*
**  UNSETENV -- remove a variable from the environment
**
**	Not needed on newer systems.
**
**	Parameters:
**		name -- the string name of the environment variable to be
**			deleted from the current environment.
**
**	Returns:
**		none.
**
**	Globals:
**		environ -- a pointer to the current environment.
**
**	Side Effects:
**		Modifies environ.
*/

#ifndef HASUNSETENV

void
unsetenv(name)
	char *name;
{
	extern char **environ;
	register char **pp;
	int len = strlen(name);

	for (pp = environ; *pp != NULL; pp++)
	{
		if (strncmp(name, *pp, len) == 0 &&
		    ((*pp)[len] == '=' || (*pp)[len] == '\0'))
			break;
	}

	for (; *pp != NULL; pp++)
		*pp = pp[1];
}

#endif
/*
**  GETDTABLESIZE -- return number of file descriptors
**
**	Only on non-BSD systems
**
**	Parameters:
**		none
**
**	Returns:
**		size of file descriptor table
**
**	Side Effects:
**		none
*/

#ifdef SOLARIS
# include <sys/resource.h>
#endif

int
getdtsize()
{
#ifdef RLIMIT_NOFILE
	struct rlimit rl;

	if (getrlimit(RLIMIT_NOFILE, &rl) >= 0)
		return rl.rlim_cur;
#endif

# ifdef HASGETDTABLESIZE
	return getdtablesize();
# else
#  ifdef _SC_OPEN_MAX
	return sysconf(_SC_OPEN_MAX);
#  else
	return NOFILE;
#  endif
# endif
}
/*
**  UNAME -- get the UUCP name of this system.
*/

#ifndef HASUNAME

int
uname(name)
	struct utsname *name;
{
	FILE *file;
	char *n;

	name->nodename[0] = '\0';

	/* try /etc/whoami -- one line with the node name */
	if ((file = fopen("/etc/whoami", "r")) != NULL)
	{
		(void) fgets(name->nodename, NODE_LENGTH + 1, file);
		(void) fclose(file);
		n = strchr(name->nodename, '\n');
		if (n != NULL)
			*n = '\0';
		if (name->nodename[0] != '\0')
			return (0);
	}

	/* try /usr/include/whoami.h -- has a #define somewhere */
	if ((file = fopen("/usr/include/whoami.h", "r")) != NULL)
	{
		char buf[MAXLINE];

		while (fgets(buf, MAXLINE, file) != NULL)
			if (sscanf(buf, "#define sysname \"%*[^\"]\"",
					NODE_LENGTH, name->nodename) > 0)
				break;
		(void) fclose(file);
		if (name->nodename[0] != '\0')
			return (0);
	}

#ifdef TRUST_POPEN
	/*
	**  Popen is known to have security holes.
	*/

	/* try uuname -l to return local name */
	if ((file = popen("uuname -l", "r")) != NULL)
	{
		(void) fgets(name, NODE_LENGTH + 1, file);
		(void) pclose(file);
		n = strchr(name, '\n');
		if (n != NULL)
			*n = '\0';
		if (name->nodename[0] != '\0')
			return (0);
	}
#endif

	return (-1);
}
#endif /* HASUNAME */
/*
**  SETSID -- set session id (for non-POSIX systems)
*/

#ifndef HASSETSID

pid_t
setsid __P ((void))
{
#ifdef TIOCNOTTY
	int fd;

	fd = open("/dev/tty", O_RDWR, 0);
	if (fd >= 0)
	{
		(void) ioctl(fd, (int) TIOCNOTTY, (char *) 0);
		(void) close(fd);
	}
#endif /* TIOCNOTTY */
# ifdef SYS5SETPGRP
	return setpgrp();
# else
	return setpgid(0, getpid());
# endif
}

#endif
/*
**  FSYNC -- dummy fsync
*/

#ifdef NEEDFSYNC

fsync(fd)
	int fd;
{
# ifdef O_SYNC
	return fcntl(fd, F_SETFL, O_SYNC);
# else
	/* nothing we can do */
	return 0;
# endif
}

#endif
/*
**  DGUX_INET_ADDR -- inet_addr for DG/UX
**
**	Data General DG/UX version of inet_addr returns a struct in_addr
**	instead of a long.  This patches things.  Only needed on versions
**	prior to 5.4.3.
*/

#ifdef DGUX_5_4_2

#undef inet_addr

long
dgux_inet_addr(host)
	char *host;
{
	struct in_addr haddr;

	haddr = inet_addr(host);
	return haddr.s_addr;
}

#endif
/*
**  GETOPT -- for old systems or systems with bogus implementations
*/

#ifdef NEEDGETOPT

/*
 * Copyright (c) 1985 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */


/*
**  this version hacked to add `atend' flag to allow state machine
**  to reset if invoked by the program to scan args for a 2nd time
*/

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)getopt.c	4.3 (Berkeley) 3/9/86";
#endif /* LIBC_SCCS and not lint */

#include <stdio.h>

/*
 * get option letter from argument vector
 */
#ifdef _CONVEX_SOURCE
extern int	optind, opterr, optopt;
extern char	*optarg;
#else
int	opterr = 1;		/* if error message should be printed */
int	optind = 1;		/* index into parent argv vector */
int	optopt = 0;		/* character checked for validity */
char	*optarg = NULL;		/* argument associated with option */
#endif

#define BADCH	(int)'?'
#define EMSG	""
#define tell(s)	if (opterr) {fputs(*nargv,stderr);fputs(s,stderr); \
		fputc(optopt,stderr);fputc('\n',stderr);return(BADCH);}

int
getopt(nargc,nargv,ostr)
	int		nargc;
	char *const	*nargv;
	const char	*ostr;
{
	static char	*place = EMSG;	/* option letter processing */
	static char	atend = 0;
	register char	*oli = NULL;	/* option letter list index */

	if (atend) {
		atend = 0;
		place = EMSG;
	}
	if(!*place) {			/* update scanning pointer */
		if (optind >= nargc || *(place = nargv[optind]) != '-' || !*++place) {
			atend++;
			return -1;
		}
		if (*place == '-') {	/* found "--" */
			++optind;
			atend++;
			return -1;
		}
	}				/* option letter okay? */
	if ((optopt = (int)*place++) == (int)':' || !(oli = strchr(ostr,optopt))) {
		if (!*place) ++optind;
		tell(": illegal option -- ");
	}
	if (oli && *++oli != ':') {		/* don't need argument */
		optarg = NULL;
		if (!*place) ++optind;
	}
	else {				/* need an argument */
		if (*place) optarg = place;	/* no white space */
		else if (nargc <= ++optind) {	/* no arg */
			place = EMSG;
			tell(": option requires an argument -- ");
		}
	 	else optarg = nargv[optind];	/* white space */
		place = EMSG;
		++optind;
	}
	return(optopt);			/* dump back option letter */
}

#endif
/*
**  VFPRINTF, VSPRINTF -- for old 4.3 BSD systems missing a real version
*/

#ifdef NEEDVPRINTF

#define MAXARG	16

vfprintf(fp, fmt, ap)
	FILE *fp;
	char *fmt;
	char **ap;
{
	char *bp[MAXARG];
	int i = 0;

	while (*ap && i < MAXARG)
		bp[i++] = *ap++;
	fprintf(fp, fmt, bp[0], bp[1], bp[2], bp[3],
			 bp[4], bp[5], bp[6], bp[7],
			 bp[8], bp[9], bp[10], bp[11],
			 bp[12], bp[13], bp[14], bp[15]);
}

vsprintf(s, fmt, ap)
	char *s;
	char *fmt;
	char **ap;
{
	char *bp[MAXARG];
	int i = 0;

	while (*ap && i < MAXARG)
		bp[i++] = *ap++;
	sprintf(s, fmt, bp[0], bp[1], bp[2], bp[3],
			bp[4], bp[5], bp[6], bp[7],
			bp[8], bp[9], bp[10], bp[11],
			bp[12], bp[13], bp[14], bp[15]);
}

#endif
/*
**  USERSHELLOK -- tell if a user's shell is ok for unrestricted use
**
**	Parameters:
**		user -- the name of the user we are checking.
**		shell -- the user's shell from /etc/passwd
**
**	Returns:
**		TRUE -- if it is ok to use this for unrestricted access.
**		FALSE -- if the shell is restricted.
*/

#if !HASGETUSERSHELL

# ifndef _PATH_SHELLS
#  define _PATH_SHELLS	"/etc/shells"
# endif

# if defined(_AIX3) || defined(_AIX4)
#  include <userconf.h>
#  if _AIX4 >= 40200
#   include <userpw.h>
#  endif
#  include <usersec.h>
# endif

char	*DefaultUserShells[] =
{
	"/bin/sh",		/* standard shell */
	"/usr/bin/sh",
	"/bin/csh",		/* C shell */
	"/usr/bin/csh",
#ifdef __hpux
# ifdef V4FS
	"/usr/bin/rsh",		/* restricted Bourne shell */
	"/usr/bin/ksh",		/* Korn shell */
	"/usr/bin/rksh",	/* restricted Korn shell */
	"/usr/bin/pam",
	"/usr/bin/keysh",	/* key shell (extended Korn shell) */
	"/usr/bin/posix/sh",
# else
	"/bin/rsh",		/* restricted Bourne shell */
	"/bin/ksh",		/* Korn shell */
	"/bin/rksh",		/* restricted Korn shell */
	"/bin/pam",
	"/usr/bin/keysh",	/* key shell (extended Korn shell) */
	"/bin/posix/sh",
# endif
#endif
#if defined(_AIX3) || defined(_AIX4)
	"/bin/ksh",		/* Korn shell */
	"/usr/bin/ksh",
	"/bin/tsh",		/* trusted shell */
	"/usr/bin/tsh",
	"/bin/bsh",		/* Bourne shell */
	"/usr/bin/bsh",
#endif
#if defined(__svr4__) || defined(__svr5__)
	"/bin/ksh",		/* Korn shell */
	"/usr/bin/ksh",
#endif
#ifdef sgi
	"/sbin/sh",		/* SGI's shells really live in /sbin */
	"/sbin/bsh",		/* classic borne shell */
	"/bin/bsh",
	"/usr/bin/bsh",
	"/sbin/csh",		/* standard csh */
	"/sbin/jsh",		/* classic borne shell w/ job control*/
	"/bin/jsh",
	"/usr/bin/jsh",
	"/bin/ksh",		/* Korn shell */
	"/sbin/ksh",
	"/usr/bin/ksh",
	"/sbin/tcsh",		/* Extended csh */
	"/bin/tcsh",		
	"/usr/bin/tcsh",
#endif
	NULL
};

#endif

#define WILDCARD_SHELL	"/SENDMAIL/ANY/SHELL/"

bool
usershellok(user, shell)
	char *user;
	char *shell;
{
#if HASGETUSERSHELL
	register char *p;
	extern char *getusershell();

	if (shell == NULL || shell[0] == '\0' || wordinclass(user, 't') ||
	    ConfigLevel <= 1)
		return TRUE;

	setusershell();
	while ((p = getusershell()) != NULL)
		if (strcmp(p, shell) == 0 || strcmp(p, WILDCARD_SHELL) == 0)
			break;
	endusershell();
	return p != NULL;
#else
# if USEGETCONFATTR
	auto char *v;
# endif
	register FILE *shellf;
	char buf[MAXLINE];

	if (shell == NULL || shell[0] == '\0' || wordinclass(user, 't') ||
	    ConfigLevel <= 1)
		return TRUE;

# if USEGETCONFATTR
	/*
	**  Naturally IBM has a "better" idea.....
	**
	**	What a crock.  This interface isn't documented, it is
	**	considered part of the security library (-ls), and it
	**	only works if you are running as root (since the list
	**	of valid shells is obviously a source of great concern).
	**	I recommend that you do NOT define USEGETCONFATTR,
	**	especially since you are going to have to set up an
	**	/etc/shells anyhow to handle the cases where getconfattr
	**	fails.
	*/

	if (getconfattr(SC_SYS_LOGIN, SC_SHELLS, &v, SEC_LIST) == 0 && v != NULL)
	{
		while (*v != '\0')
		{
			if (strcmp(v, shell) == 0 || strcmp(v, WILDCARD_SHELL) == 0)
				return TRUE;
			v += strlen(v) + 1;
		}
		return FALSE;
	}
# endif

	shellf = fopen(_PATH_SHELLS, "r");
	if (shellf == NULL)
	{
		/* no /etc/shells; see if it is one of the std shells */
		char **d;
		
		if (errno != ENOENT && LogLevel > 3)
			sm_syslog(LOG_ERR, NOQID,
				  "usershellok: cannot open %s: %s",
				  _PATH_SHELLS, errstring(errno));

		for (d = DefaultUserShells; *d != NULL; d++)
		{
			if (strcmp(shell, *d) == 0)
				return TRUE;
		}
		return FALSE;
	}

	while (fgets(buf, sizeof buf, shellf) != NULL)
	{
		register char *p, *q;

		p = buf;
		while (*p != '\0' && *p != '#' && *p != '/')
			p++;
		if (*p == '#' || *p == '\0')
			continue;
		q = p;
		while (*p != '\0' && *p != '#' && !(isascii(*p) && isspace(*p)))
			p++;
		*p = '\0';
		if (strcmp(shell, q) == 0 || strcmp(WILDCARD_SHELL, q) == 0)
		{
			fclose(shellf);
			return TRUE;
		}
	}
	fclose(shellf);
	return FALSE;
#endif
}
/*
**  FREEDISKSPACE -- see how much free space is on the queue filesystem
**
**	Only implemented if you have statfs.
**
**	Parameters:
**		dir -- the directory in question.
**		bsize -- a variable into which the filesystem
**			block size is stored.
**
**	Returns:
**		The number of bytes free on the queue filesystem.
**		-1 if the statfs call fails.
**
**	Side effects:
**		Puts the filesystem block size into bsize.
*/

/* statfs types */
#define SFS_NONE	0	/* no statfs implementation */
#define SFS_USTAT	1	/* use ustat */
#define SFS_4ARGS	2	/* use four-argument statfs call */
#define SFS_VFS		3	/* use <sys/vfs.h> implementation */
#define SFS_MOUNT	4	/* use <sys/mount.h> implementation */
#define SFS_STATFS	5	/* use <sys/statfs.h> implementation */
#define SFS_STATVFS	6	/* use <sys/statvfs.h> implementation */

#ifndef SFS_TYPE
# define SFS_TYPE	SFS_NONE
#endif

#if SFS_TYPE == SFS_USTAT
# include <ustat.h>
#endif
#if SFS_TYPE == SFS_4ARGS || SFS_TYPE == SFS_STATFS
# include <sys/statfs.h>
#endif
#if SFS_TYPE == SFS_VFS
# include <sys/vfs.h>
#endif
#if SFS_TYPE == SFS_MOUNT
# include <sys/mount.h>
#endif
#if SFS_TYPE == SFS_STATVFS
# include <sys/statvfs.h>
#endif

long
freediskspace(dir, bsize)
	char *dir;
	long *bsize;
{
#if SFS_TYPE != SFS_NONE
# if SFS_TYPE == SFS_USTAT
	struct ustat fs;
	struct stat statbuf;
#  define FSBLOCKSIZE	DEV_BSIZE
#  define SFS_BAVAIL	f_tfree
# else
#  if defined(ultrix)
	struct fs_data fs;
#   define SFS_BAVAIL	fd_bfreen
#   define FSBLOCKSIZE	1024L
#  else
#   if SFS_TYPE == SFS_STATVFS
	struct statvfs fs;
#    define FSBLOCKSIZE	fs.f_frsize
#   else
	struct statfs fs;
#    define FSBLOCKSIZE	fs.f_bsize
#   endif
#  endif
# endif
# ifndef SFS_BAVAIL
#  define SFS_BAVAIL f_bavail
# endif

# if SFS_TYPE == SFS_USTAT
	if (stat(dir, &statbuf) == 0 && ustat(statbuf.st_dev, &fs) == 0)
# else
#  if SFS_TYPE == SFS_4ARGS
	if (statfs(dir, &fs, sizeof fs, 0) == 0)
#  else
#   if SFS_TYPE == SFS_STATVFS
	if (statvfs(dir, &fs) == 0)
#   else
#    if defined(ultrix)
	if (statfs(dir, &fs) > 0)
#    else
	if (statfs(dir, &fs) == 0)
#    endif
#   endif
#  endif
# endif
	{
		if (bsize != NULL)
			*bsize = FSBLOCKSIZE;
		if (fs.SFS_BAVAIL <= 0)
			return 0;
		else if (fs.SFS_BAVAIL > LONG_MAX)
			return LONG_MAX;
		else
			return (long) fs.SFS_BAVAIL;
	}
#endif
	return (-1);
}
/*
**  ENOUGHDISKSPACE -- is there enough free space on the queue fs?
**
**	Only implemented if you have statfs.
**
**	Parameters:
**		msize -- the size to check against.  If zero, we don't yet
**		know how big the message will be, so just check for
**		a "reasonable" amount.
**
**	Returns:
**		TRUE if there is enough space.
**		FALSE otherwise.
*/

bool
enoughdiskspace(msize)
	long msize;
{
	long bfree, bsize;

	if (MinBlocksFree <= 0 && msize <= 0)
	{
		if (tTd(4, 80))
			printf("enoughdiskspace: no threshold\n");
		return TRUE;
	}

	if ((bfree = freediskspace(QueueDir, &bsize)) >= 0)
	{
		if (tTd(4, 80))
			printf("enoughdiskspace: bavail=%ld, need=%ld\n",
				bfree, msize);

		/* convert msize to block count */
		msize = msize / bsize + 1;
		if (MinBlocksFree >= 0)
			msize += MinBlocksFree;

		if (bfree < msize)
		{
			if (LogLevel > 0)
				sm_syslog(LOG_ALERT, CurEnv->e_id,
					"low on space (have %ld, %s needs %ld in %s)",
					bfree,
					CurHostName == NULL ? "SMTP-DAEMON" : CurHostName,
					msize, QueueDir);
			return FALSE;
		}
	}
	else if (tTd(4, 80))
		printf("enoughdiskspace failure: min=%ld, need=%ld: %s\n",
			MinBlocksFree, msize, errstring(errno));
	return TRUE;
}
/*
**  TRANSIENTERROR -- tell if an error code indicates a transient failure
**
**	This looks at an errno value and tells if this is likely to
**	go away if retried later.
**
**	Parameters:
**		err -- the errno code to classify.
**
**	Returns:
**		TRUE if this is probably transient.
**		FALSE otherwise.
*/

bool
transienterror(err)
	int err;
{
	switch (err)
	{
	  case EIO:			/* I/O error */
	  case ENXIO:			/* Device not configured */
	  case EAGAIN:			/* Resource temporarily unavailable */
	  case ENOMEM:			/* Cannot allocate memory */
	  case ENODEV:			/* Operation not supported by device */
	  case ENFILE:			/* Too many open files in system */
	  case EMFILE:			/* Too many open files */
	  case ENOSPC:			/* No space left on device */
#ifdef ETIMEDOUT
	  case ETIMEDOUT:		/* Connection timed out */
#endif
#ifdef ESTALE
	  case ESTALE:			/* Stale NFS file handle */
#endif
#ifdef ENETDOWN
	  case ENETDOWN:		/* Network is down */
#endif
#ifdef ENETUNREACH
	  case ENETUNREACH:		/* Network is unreachable */
#endif
#ifdef ENETRESET
	  case ENETRESET:		/* Network dropped connection on reset */
#endif
#ifdef ECONNABORTED
	  case ECONNABORTED:		/* Software caused connection abort */
#endif
#ifdef ECONNRESET
	  case ECONNRESET:		/* Connection reset by peer */
#endif
#ifdef ENOBUFS
	  case ENOBUFS:			/* No buffer space available */
#endif
#ifdef ESHUTDOWN
	  case ESHUTDOWN:		/* Can't send after socket shutdown */
#endif
#ifdef ECONNREFUSED
	  case ECONNREFUSED:		/* Connection refused */
#endif
#ifdef EHOSTDOWN
	  case EHOSTDOWN:		/* Host is down */
#endif
#ifdef EHOSTUNREACH
	  case EHOSTUNREACH:		/* No route to host */
#endif
#ifdef EDQUOT
	  case EDQUOT:			/* Disc quota exceeded */
#endif
#ifdef EPROCLIM
	  case EPROCLIM:		/* Too many processes */
#endif
#ifdef EUSERS
	  case EUSERS:			/* Too many users */
#endif
#ifdef EDEADLK
	  case EDEADLK:			/* Resource deadlock avoided */
#endif
#ifdef EISCONN
	  case EISCONN:			/* Socket already connected */
#endif
#ifdef EINPROGRESS
	  case EINPROGRESS:		/* Operation now in progress */
#endif
#ifdef EALREADY
	  case EALREADY:		/* Operation already in progress */
#endif
#ifdef EADDRINUSE
	  case EADDRINUSE:		/* Address already in use */
#endif
#ifdef EADDRNOTAVAIL
	  case EADDRNOTAVAIL:		/* Can't assign requested address */
#endif
#ifdef ETXTBSY
	  case ETXTBSY:			/* (Apollo) file locked */
#endif
#if defined(ENOSR) && (!defined(ENOBUFS) || (ENOBUFS != ENOSR))
	  case ENOSR:			/* Out of streams resources */
#endif
	  case E_SM_OPENTIMEOUT:	/* PSEUDO: open timed out */
		return TRUE;
	}

	/* nope, must be permanent */
	return FALSE;
}
/*
**  LOCKFILE -- lock a file using flock or (shudder) fcntl locking
**
**	Parameters:
**		fd -- the file descriptor of the file.
**		filename -- the file name (for error messages).
**		ext -- the filename extension.
**		type -- type of the lock.  Bits can be:
**			LOCK_EX -- exclusive lock.
**			LOCK_NB -- non-blocking.
**
**	Returns:
**		TRUE if the lock was acquired.
**		FALSE otherwise.
*/

bool
lockfile(fd, filename, ext, type)
	int fd;
	char *filename;
	char *ext;
	int type;
{
	int i;
	int save_errno;
# if !HASFLOCK
	int action;
	struct flock lfd;

	if (ext == NULL)
		ext = "";

	bzero(&lfd, sizeof lfd);
	if (bitset(LOCK_UN, type))
		lfd.l_type = F_UNLCK;
	else if (bitset(LOCK_EX, type))
		lfd.l_type = F_WRLCK;
	else
		lfd.l_type = F_RDLCK;

	if (bitset(LOCK_NB, type))
		action = F_SETLK;
	else
		action = F_SETLKW;

	if (tTd(55, 60))
		printf("lockfile(%s%s, action=%d, type=%d): ",
			filename, ext, action, lfd.l_type);

	while ((i = fcntl(fd, action, &lfd)) < 0 && errno == EINTR)
		continue;
	if (i >= 0)
	{
		if (tTd(55, 60))
			printf("SUCCESS\n");
		return TRUE;
	}
	save_errno = errno;

	if (tTd(55, 60))
		printf("(%s) ", errstring(save_errno));

	/*
	**  On SunOS, if you are testing using -oQ/tmp/mqueue or
	**  -oA/tmp/aliases or anything like that, and /tmp is mounted
	**  as type "tmp" (that is, served from swap space), the
	**  previous fcntl will fail with "Invalid argument" errors.
	**  Since this is fairly common during testing, we will assume
	**  that this indicates that the lock is successfully grabbed.
	*/

	if (save_errno == EINVAL)
	{
		if (tTd(55, 60))
			printf("SUCCESS\n");
		return TRUE;
	}

	if (!bitset(LOCK_NB, type) || (save_errno != EACCES && save_errno != EAGAIN))
	{
		int omode = -1;
#  ifdef F_GETFL
		(void) fcntl(fd, F_GETFL, &omode);
		errno = save_errno;
#  endif
		syserr("cannot lockf(%s%s, fd=%d, type=%o, omode=%o, euid=%d)",
			filename, ext, fd, type, omode, geteuid());
		dumpfd(fd, TRUE, TRUE);
	}
# else
	if (ext == NULL)
		ext = "";

	if (tTd(55, 60))
		printf("lockfile(%s%s, type=%o): ", filename, ext, type);

	while ((i = flock(fd, type)) < 0 && errno == EINTR)
		continue;
	if (i >= 0)
	{
		if (tTd(55, 60))
			printf("SUCCESS\n");
		return TRUE;
	}
	save_errno = errno;

	if (tTd(55, 60))
		printf("(%s) ", errstring(save_errno));

	if (!bitset(LOCK_NB, type) || save_errno != EWOULDBLOCK)
	{
		int omode = -1;
#  ifdef F_GETFL
		(void) fcntl(fd, F_GETFL, &omode);
		errno = save_errno;
#  endif
		syserr("cannot flock(%s%s, fd=%d, type=%o, omode=%o, euid=%d)",
			filename, ext, fd, type, omode, geteuid());
		dumpfd(fd, TRUE, TRUE);
	}
# endif
	if (tTd(55, 60))
		printf("FAIL\n");
	errno = save_errno;
	return FALSE;
}
/*
**  CHOWNSAFE -- tell if chown is "safe" (executable only by root)
**
**	Unfortunately, given that we can't predict other systems on which
**	a remote mounted (NFS) filesystem will be mounted, the answer is
**	almost always that this is unsafe.
**
**	Note also that many operating systems have non-compliant
**	implementations of the _POSIX_CHOWN_RESTRICTED variable and the
**	fpathconf() routine.  According to IEEE 1003.1-1990, if
**	_POSIX_CHOWN_RESTRICTED is defined and not equal to -1, then
**	no non-root process can give away the file.  However, vendors
**	don't take NFS into account, so a comfortable value of
**	_POSIX_CHOWN_RESTRICTED tells us nothing.
**
**	Also, some systems (e.g., IRIX 6.2) return 1 from fpathconf()
**	even on files where chown is not restricted.  Many systems get
**	this wrong on NFS-based filesystems (that is, they say that chown
**	is restricted [safe] on NFS filesystems where it may not be, since
**	other systems can access the same filesystem and do file giveaway;
**	only the NFS server knows for sure!)  Hence, it is important to
**	get the value of SAFENFSPATHCONF correct -- it should be defined
**	_only_ after testing (see test/t_pathconf.c) a system on an unsafe
**	NFS-based filesystem to ensure that you can get meaningful results.
**	If in doubt, assume unsafe!
**
**	You may also need to tweak IS_SAFE_CHOWN -- it should be a
**	condition indicating whether the return from pathconf indicates
**	that chown is safe (typically either > 0 or >= 0 -- there isn't
**	even any agreement about whether a zero return means that a file
**	is or is not safe).  It defaults to "> 0".
**
**	If the parent directory is safe (writable only by owner back
**	to the root) then we can relax slightly and trust fpathconf
**	in more circumstances.  This is really a crock -- if this is an
**	NFS mounted filesystem then we really know nothing about the
**	underlying implementation.  However, most systems pessimize and
**	return an error (EINVAL or EOPNOTSUPP) on NFS filesystems, which
**	we interpret as unsafe, as we should.  Thus, this heuristic gets
**	us into a possible problem only on systems that have a broken
**	pathconf implementation and which are also poorly configured
**	(have :include: files in group- or world-writable directories).
**
**	Parameters:
**		fd -- the file descriptor to check.
**		safedir -- set if the parent directory is safe.
**
**	Returns:
**		TRUE -- if the chown(2) operation is "safe" -- that is,
**			only root can chown the file to an arbitrary user.
**		FALSE -- if an arbitrary user can give away a file.
*/

#ifndef IS_SAFE_CHOWN
# define IS_SAFE_CHOWN	> 0
#endif

bool
chownsafe(fd, safedir)
	int fd;
	bool safedir;
{
#if (!defined(_POSIX_CHOWN_RESTRICTED) || _POSIX_CHOWN_RESTRICTED != -1) && \
    (defined(_PC_CHOWN_RESTRICTED) || defined(_GNU_TYPES_H))
	int rval;

	/* give the system administrator a chance to override */
	if (bitset(DBS_ASSUMESAFECHOWN, DontBlameSendmail))
		return TRUE;

	/*
	**  Some systems (e.g., SunOS) seem to have the call and the
	**  #define _PC_CHOWN_RESTRICTED, but don't actually implement
	**  the call.  This heuristic checks for that.
	*/

	errno = 0;
	rval = fpathconf(fd, _PC_CHOWN_RESTRICTED);
# if SAFENFSPATHCONF
	return errno == 0 && rval IS_SAFE_CHOWN;
# else
	return safedir && errno == 0 && rval IS_SAFE_CHOWN;
# endif
#else
	return bitset(DBS_ASSUMESAFECHOWN, DontBlameSendmail);
#endif
}
/*
**  RESETLIMITS -- reset system controlled resource limits
**
**	This is to avoid denial-of-service attacks
**
**	Parameters:
**		none
**
**	Returns:
**		none
*/

#if HASSETRLIMIT
# ifdef RLIMIT_NEEDS_SYS_TIME_H
#  include <sys/time.h>
# endif
# include <sys/resource.h>
#endif
#ifndef FD_SETSIZE
# define FD_SETSIZE	256
#endif

void
resetlimits()
{
#if HASSETRLIMIT
	struct rlimit lim;

	lim.rlim_cur = lim.rlim_max = RLIM_INFINITY;
	(void) setrlimit(RLIMIT_CPU, &lim);
	(void) setrlimit(RLIMIT_FSIZE, &lim);
# ifdef RLIMIT_NOFILE
	lim.rlim_cur = lim.rlim_max = FD_SETSIZE;
	(void) setrlimit(RLIMIT_NOFILE, &lim);
# endif
#else
# if HASULIMIT
	(void) ulimit(2, 0x3fffff);
	(void) ulimit(4, FD_SETSIZE);
# endif
#endif
	errno = 0;
}
/*
**  GETCFNAME -- return the name of the .cf file.
**
**	Some systems (e.g., NeXT) determine this dynamically.
*/

char *
getcfname()
{

	if (ConfFile != NULL)
		return ConfFile;
#if NETINFO
	{
		extern char *ni_propval __P((char *, char *, char *, char *, int));
		char *cflocation;

		cflocation = ni_propval("/locations", NULL, "sendmail",
					"sendmail.cf", '\0');
		if (cflocation != NULL)
			return cflocation;
	}
#endif

	return _PATH_SENDMAILCF;
}
/*
**  SETVENDOR -- process vendor code from V configuration line
**
**	Parameters:
**		vendor -- string representation of vendor.
**
**	Returns:
**		TRUE -- if ok.
**		FALSE -- if vendor code could not be processed.
**
**	Side Effects:
**		It is reasonable to set mode flags here to tweak
**		processing in other parts of the code if necessary.
**		For example, if you are a vendor that uses $%y to
**		indicate YP lookups, you could enable that here.
*/

bool
setvendor(vendor)
	char *vendor;
{
	if (strcasecmp(vendor, "Berkeley") == 0)
	{
		VendorCode = VENDOR_BERKELEY;
		return TRUE;
	}

	/* add vendor extensions here */

#ifdef SUN_EXTENSIONS
	if (strcasecmp(vendor, "Sun") == 0)
	{
		VendorCode = VENDOR_SUN;
		return TRUE;
	}
#endif

#if defined(VENDOR_NAME) && defined(VENDOR_CODE)
	if (strcasecmp(vendor, VENDOR_NAME) == 0)
	{
		VendorCode = VENDOR_CODE;
		return TRUE;
	}
#endif

	return FALSE;
}
/*
**  GETVENDOR -- return vendor name based on vendor code
**
**	Parameters:
**		vendorcode -- numeric representation of vendor.
**
**	Returns:
**		string containing vendor name.
*/

char *
getvendor(vendorcode)
	int vendorcode;
{
#if defined(VENDOR_NAME) && defined(VENDOR_CODE)
	/*
	**  Can't have the same switch case twice so need to 
	**  handle VENDOR_CODE outside of switch.  It might
	**  match one of the existing VENDOR_* codes.
	*/

	if (vendorcode == VENDOR_CODE)
		return VENDOR_NAME;
#endif

	switch (vendorcode)
	{
		case VENDOR_BERKELEY:
			return "Berkeley";
		
		case VENDOR_SUN:
			return "Sun";

		case VENDOR_HP:
			return "HP";

		case VENDOR_IBM:
			return "IBM";

		case VENDOR_SENDMAIL:
			return "Sendmail";

		default:
			return "Unknown";
	}
}
/*
**  VENDOR_PRE_DEFAULTS, VENDOR_POST_DEFAULTS -- set vendor-specific defaults
**
**	Vendor_pre_defaults is called before reading the configuration
**	file; vendor_post_defaults is called immediately after.
**
**	Parameters:
**		e -- the global environment to initialize.
**
**	Returns:
**		none.
*/

#if SHARE_V1
int	DefShareUid;	/* default share uid to run as -- unused??? */
#endif

void
vendor_pre_defaults(e)
	ENVELOPE *e;
{
#if SHARE_V1
	/* OTHERUID is defined in shares.h, do not be alarmed */
	DefShareUid = OTHERUID;
#endif
#if defined(SUN_EXTENSIONS) && defined(SUN_DEFAULT_VALUES)
	sun_pre_defaults(e);
#endif
#ifdef apollo
	/* stupid domain/os can't even open /etc/sendmail.cf without this */
	setuserenv("ISP", NULL);
	setuserenv("SYSTYPE", NULL);
#endif
}


void
vendor_post_defaults(e)
	ENVELOPE *e;
{
#ifdef __QNX__
	char *p;
	
	/* Makes sure the SOCK environment variable remains */
	if (p = getextenv("SOCK"))
		setuserenv("SOCK", p);
#endif
#if defined(SUN_EXTENSIONS) && defined(SUN_DEFAULT_VALUES)
	sun_post_defaults(e);
#endif
}
/*
**  VENDOR_DAEMON_SETUP -- special vendor setup needed for daemon mode
*/

void
vendor_daemon_setup(e)
	ENVELOPE *e;
{
#if SECUREWARE
	if (getluid() != -1)
	{
		usrerr("Daemon cannot have LUID");
		finis(FALSE, EX_USAGE);
	}
#endif /* SECUREWARE */
}
/*
**  VENDOR_SET_UID -- do setup for setting a user id
**
**	This is called when we are still root.
**
**	Parameters:
**		uid -- the uid we are about to become.
**
**	Returns:
**		none.
*/

void
vendor_set_uid(uid)
	UID_T uid;
{
	/* 
	**  We need to setup the share groups (lnodes)
	**  and and auditing inforation (luid's)
	**  before we loose our ``root''ness.
	*/
#if SHARE_V1
	if (setupshares(uid, syserr) != 0)
		syserr("Unable to set up shares");
#endif
#if SECUREWARE
	(void) setup_secure(uid);
#endif
}
/*
**  VALIDATE_CONNECTION -- check connection for rationality
**
**	If the connection is rejected, this routine should log an
**	appropriate message -- but should never issue any SMTP protocol.
**
**	Parameters:
**		sap -- a pointer to a SOCKADDR naming the peer.
**		hostname -- the name corresponding to sap.
**		e -- the current envelope.
**
**	Returns:
**		error message from rejection.
**		NULL if not rejected.
*/

#if TCPWRAPPERS
# include <tcpd.h>

/* tcpwrappers does no logging, but you still have to declare these -- ugh */
int	allow_severity	= LOG_INFO;
int	deny_severity	= LOG_NOTICE;
#endif

#if DAEMON
char *
validate_connection(sap, hostname, e)
	SOCKADDR *sap;
	char *hostname;
	ENVELOPE *e;
{
#if TCPWRAPPERS
	char *host;
#endif

	if (tTd(48, 3))
		printf("validate_connection(%s, %s)\n",
			hostname, anynet_ntoa(sap));

	if (rscheck("check_relay", hostname, anynet_ntoa(sap), e) != EX_OK)
	{
		static char reject[BUFSIZ*2];
		extern char MsgBuf[];

		if (tTd(48, 4))
			printf("  ... validate_connection: BAD (rscheck)\n");

		if (strlen(MsgBuf) > 5)
		{
			if (isascii(MsgBuf[0]) && isdigit(MsgBuf[0]) &&
			    isascii(MsgBuf[1]) && isdigit(MsgBuf[1]) &&
			    isascii(MsgBuf[2]) && isdigit(MsgBuf[2]))
				strcpy(reject, &MsgBuf[4]);
			else
				strcpy(reject, MsgBuf);
		}
		else
			strcpy(reject, "Access denied");

		return reject;
	}

#if TCPWRAPPERS
	if (hostname[0] == '[' && hostname[strlen(hostname) - 1] == ']')
		host = "unknown";
	else
		host = hostname;
	if (!hosts_ctl("sendmail", host, anynet_ntoa(sap), STRING_UNKNOWN))
	{
		if (tTd(48, 4))
			printf("  ... validate_connection: BAD (tcpwrappers)\n");
		if (LogLevel >= 4)
			sm_syslog(LOG_NOTICE, NOQID,
				"tcpwrappers (%s, %s) rejection",
				host, anynet_ntoa(sap));
		return "Access denied";
	}
#endif
	if (tTd(48, 4))
		printf("  ... validate_connection: OK\n");
	return NULL;
}

#endif
/*
**  STRTOL -- convert string to long integer
**
**	For systems that don't have it in the C library.
**
**	This is taken verbatim from the 4.4-Lite C library.
*/

#ifdef NEEDSTRTOL

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)strtol.c	8.1 (Berkeley) 6/4/93";
#endif /* LIBC_SCCS and not lint */

/*
 * Convert a string to a long integer.
 *
 * Ignores `locale' stuff.  Assumes that the upper and lower case
 * alphabets and digits are each contiguous.
 */

long
strtol(nptr, endptr, base)
	const char *nptr;
	char **endptr;
	register int base;
{
	register const char *s = nptr;
	register unsigned long acc;
	register int c;
	register unsigned long cutoff;
	register int neg = 0, any, cutlim;

	/*
	 * Skip white space and pick up leading +/- sign if any.
	 * If base is 0, allow 0x for hex and 0 for octal, else
	 * assume decimal; if base is already 16, allow 0x.
	 */
	do {
		c = *s++;
	} while (isspace(c));
	if (c == '-') {
		neg = 1;
		c = *s++;
	} else if (c == '+')
		c = *s++;
	if ((base == 0 || base == 16) &&
	    c == '0' && (*s == 'x' || *s == 'X')) {
		c = s[1];
		s += 2;
		base = 16;
	}
	if (base == 0)
		base = c == '0' ? 8 : 10;

	/*
	 * Compute the cutoff value between legal numbers and illegal
	 * numbers.  That is the largest legal value, divided by the
	 * base.  An input number that is greater than this value, if
	 * followed by a legal input character, is too big.  One that
	 * is equal to this value may be valid or not; the limit
	 * between valid and invalid numbers is then based on the last
	 * digit.  For instance, if the range for longs is
	 * [-2147483648..2147483647] and the input base is 10,
	 * cutoff will be set to 214748364 and cutlim to either
	 * 7 (neg==0) or 8 (neg==1), meaning that if we have accumulated
	 * a value > 214748364, or equal but the next digit is > 7 (or 8),
	 * the number is too big, and we will return a range error.
	 *
	 * Set any if any `digits' consumed; make it negative to indicate
	 * overflow.
	 */
	cutoff = neg ? -(unsigned long)LONG_MIN : LONG_MAX;
	cutlim = cutoff % (unsigned long)base;
	cutoff /= (unsigned long)base;
	for (acc = 0, any = 0;; c = *s++) {
		if (isdigit(c))
			c -= '0';
		else if (isalpha(c))
			c -= isupper(c) ? 'A' - 10 : 'a' - 10;
		else
			break;
		if (c >= base)
			break;
		if (any < 0 || acc > cutoff || acc == cutoff && c > cutlim)
			any = -1;
		else {
			any = 1;
			acc *= base;
			acc += c;
		}
	}
	if (any < 0) {
		acc = neg ? LONG_MIN : LONG_MAX;
		errno = ERANGE;
	} else if (neg)
		acc = -acc;
	if (endptr != 0)
		*endptr = (char *)(any ? s - 1 : nptr);
	return (acc);
}

#endif
/*
**  STRSTR -- find first substring in string
**
**	Parameters:
**		big -- the big (full) string.
**		little -- the little (sub) string.
**
**	Returns:
**		A pointer to the first instance of little in big.
**		big if little is the null string.
**		NULL if little is not contained in big.
*/

#ifdef NEEDSTRSTR

char *
strstr(big, little)
	char *big;
	char *little;
{
	register char *p = big;
	int l;

	if (*little == '\0')
		return big;
	l = strlen(little);

	while ((p = strchr(p, *little)) != NULL)
	{
		if (strncmp(p, little, l) == 0)
			return p;
		p++;
	}
	return NULL;
}

#endif
/*
**  SM_GETHOSTBY{NAME,ADDR} -- compatibility routines for gethostbyXXX
**
**	Some operating systems have wierd problems with the gethostbyXXX
**	routines.  For example, Solaris versions at least through 2.3
**	don't properly deliver a canonical h_name field.  This tries to
**	work around these problems.
*/

struct hostent *
sm_gethostbyname(name)
	char *name;
{
	struct hostent *h;
#if (SOLARIS > 10000 && SOLARIS < 20400) || (defined(SOLARIS) && SOLARIS < 204) || (defined(sony_news) && defined(__svr4))
# if SOLARIS == 20300 || SOLARIS == 203
	static struct hostent hp;
	static char buf[1000];
	extern struct hostent *_switch_gethostbyname_r();

	if (tTd(61, 10))
		printf("_switch_gethostbyname_r(%s)... ", name);
	h = _switch_gethostbyname_r(name, &hp, buf, sizeof(buf), &h_errno);
# else
	extern struct hostent *__switch_gethostbyname();

	if (tTd(61, 10))
		printf("__switch_gethostbyname(%s)... ", name);
	h = __switch_gethostbyname(name);
# endif
#else
	int nmaps;
	char *maptype[MAXMAPSTACK];
	short mapreturn[MAXMAPACTIONS];
	char hbuf[MAXNAME];

	if (tTd(61, 10))
		printf("gethostbyname(%s)... ", name);
	h = gethostbyname(name);
	if (h == NULL)
	{
		if (tTd(61, 10))
			printf("failure\n");

		nmaps = switch_map_find("hosts", maptype, mapreturn);
		while (--nmaps >= 0)
			if (strcmp(maptype[nmaps], "nis") == 0 ||
			    strcmp(maptype[nmaps], "files") == 0)
				break;
		if (nmaps >= 0)
		{
			/* try short name */
			if (strlen(name) > (SIZE_T) sizeof hbuf - 1)
				return NULL;
			strcpy(hbuf, name);
			shorten_hostname(hbuf);

			/* if it hasn't been shortened, there's no point */
			if (strcmp(hbuf, name) != 0)
			{
				if (tTd(61, 10))
					printf("gethostbyname(%s)... ", hbuf);
				h = gethostbyname(hbuf);
			}
		}
	}
#endif
	if (tTd(61, 10))
	{
		if (h == NULL)
			printf("failure\n");
		else
			printf("%s\n", h->h_name);
	}
	return h;
}

struct hostent *
sm_gethostbyaddr(addr, len, type)
	char *addr;
	int len;
	int type;
{
#if (SOLARIS > 10000 && SOLARIS < 20400) || (defined(SOLARIS) && SOLARIS < 204)
# if SOLARIS == 20300 || SOLARIS == 203
	static struct hostent hp;
	static char buf[1000];
	extern struct hostent *_switch_gethostbyaddr_r();

	return _switch_gethostbyaddr_r(addr, len, type, &hp, buf, sizeof(buf), &h_errno);
# else
	extern struct hostent *__switch_gethostbyaddr();

	return __switch_gethostbyaddr(addr, len, type);
# endif
#else
	return gethostbyaddr(addr, len, type);
#endif
}
/*
**  SM_GETPW{NAM,UID} -- wrapper for getpwnam and getpwuid
*/

struct passwd *
sm_getpwnam(user)
	char *user;
{
#ifdef _AIX4
	extern struct passwd *_getpwnam_shadow(const char *, const int);

	return _getpwnam_shadow(user, 0);
#else
	return getpwnam(user);
#endif
}

struct passwd *
sm_getpwuid(uid)
	UID_T uid;
{
#if defined(_AIX4) && 0
	extern struct passwd *_getpwuid_shadow(const int, const int);

	return _getpwuid_shadow(uid,0);
#else
	return getpwuid(uid);
#endif
}
/*
**
*/

#ifdef TRUSTED_CAP
static int
sm_cap_enabled()
{
#ifdef _SC_CAP
	static int c = -1;

	if (c == -1)
		c = (sysconf(_SC_MAC) > 0);
	return(c);
#else
	return(0);
#endif
}

static void
fill_capset(num, capset, trusted)
	int *num, trusted;
	cap_value_t *capset;
{
	*num = 0;

	if (trusted & SM_CAP_SETUID)
		capset[(*num)++] = CAP_SETUID;
	if (trusted & SM_CAP_SETGID)
		capset[(*num)++] = CAP_SETGID;
	if (trusted & SM_CAP_CHROOT)
		capset[(*num)++] = CAP_CHROOT;
	if (trusted & SM_CAP_PLABEL)
		capset[(*num)++] = CAP_MAC_RELABEL_SUBJ;
	if (trusted & SM_CAP_DACREAD)
		capset[(*num)++] = CAP_DAC_READ_SEARCH;
	if (trusted & SM_CAP_MACREAD)
		capset[(*num)++] = CAP_MAC_READ;
	if (trusted & SM_CAP_AUDIT)
		capset[(*num)++] = CAP_AUDIT_WRITE;
	if (trusted & SM_CAP_CHOWN)
		capset[(*num)++] = CAP_CHOWN;
	if (trusted & SM_CAP_MACUP)
		capset[(*num)++] = CAP_MAC_UPGRADE;
	if (trusted & SM_CAP_MACDN)
		capset[(*num)++] = CAP_MAC_DOWNGRADE;

#if defined(sgi)
	if (trusted & SM_CAP_ATTR)
		capset[(*num)++] = CAP_DEVICE_MGT;
#endif

#ifdef CAP_NETWORK_MGT
#define CAP CAP_NETWORK_MGT
#endif
#ifdef CAP_NET_ADMIN
#define CAP CAP_NET_ADMIN
#endif
#ifdef CAP
	if (trusted & SM_CAP_NETMGT)
		capset[(*num)++] = CAP;
#undef CAP
#endif

#ifdef CAP_PRIV_PORT
#define CAP CAP_PRIV_PORT
#endif
#ifdef CAP_NET_BIND_SERVICE
#define CAP CAP_NET_BIND_SERVICE
#endif
#ifdef CAP
	if (trusted & SM_CAP_PRIVPORT)
		capset[(*num)++] = CAP;
#undef CAP
#endif

#ifdef CAP_SCHED_MGT
#define CAP CAP_SCHED_MGT
#endif
#ifdef CAP_SYS_NICE
#define CAP CAP_SYS_NICE
#endif
#ifdef CAP
	if (trusted & SM_CAP_SCHED)
		capset[(*num)++] = CAP;
#undef CAP
#endif
}

static void
sm_capdbg(level, msg, cap)
	int level;
	const char *msg;
	cap_t cap;
{
	int terrno = errno;
	if (tTd(71, level) && sm_cap_enabled())
	{
		char *capstr, *errstr = "bad capability set";

		capstr = cap_to_text(cap, (size_t *) NULL);
		if (capstr == NULL)
			capstr = errstr;
		printf("%s: '%s'\n", msg, capstr);
		if (capstr != errstr);
			cap_free(capstr);
	}
	errno = terrno;
}

/*
 * Make the specified set of capabilities effective, but only if
 * they are currently permitted. Return the old capability set.
 */
cap_t
sm_cap_acquire(trusted)
	int trusted;
{
	if (sm_cap_enabled())
	{
		cap_value_t capset[15];
		int ncap;

		fill_capset(&ncap, capset, trusted);
		if (ncap != 0)
		{
			cap_value_t newset[15];
			cap_t old;
			int i, j;
			char *capstr;

			old = cap_get_proc();
			sm_capdbg(2, "getting old capability set", old);
			if (old == NULL)
				return(old);

			for (i = 0, j = 0; i < ncap; i++)
			{
				cap_flag_value_t flag;

				if (cap_get_flag(old, capset[i], CAP_PERMITTED, &flag) == 0 && flag == CAP_SET)
					newset[j++] = capset[i];
			}

			if (j != 0)
			{
				cap_t new = cap_dup(old);

				if (new == NULL || cap_set_flag(new, CAP_EFFECTIVE, j, newset, CAP_SET) == -1 || cap_set_proc(new) == -1)
				{
					cap_free(old);
					old = NULL;
				}
				sm_capdbg(1, "setting new capability set",
					  new);
				cap_free(new);
			}

			return(old);
		}
	}
	errno = ENOSYS;
	return(NULL);
}

/*
 * Make our capability set equal to cap, freeing cap afterwards.
 * Ignore NULL capability sets. cap is usually the return value
 * of sm_cap_acquire().
 */
void
sm_cap_surrender(cap)
	cap_t cap;
{
	if (cap != NULL)
	{
		sm_capdbg(1, "setting new capability set", cap);
		(void) cap_set_proc(cap);
		cap_free(cap);
	}
}

int
sm_cap_inherit(trusted)
	int trusted;
{
	if (sm_cap_enabled())
	{
		cap_value_t capset[15];
		cap_t cap;
		int ncap;

		/* get our current capability set */
		cap = cap_get_proc();
		if (cap == NULL)
			return(-1);

		/* enable all specified capabilities */
		fill_capset(&ncap, capset, trusted);
		if (ncap != 0 && (cap_set_flag(cap, CAP_INHERITABLE, ncap, capset, CAP_SET) == -1 || cap_set_flag(cap, CAP_EFFECTIVE, ncap, capset, CAP_SET) == -1))
		{
			cap_free(cap);
			return(-1);
		}

		/* clear all capabilities not specified */
		fill_capset(&ncap, capset, ~trusted);
		if (ncap != 0 && (cap_set_flag(cap, CAP_INHERITABLE, ncap, capset, CAP_CLEAR) == -1 || cap_set_flag(cap, CAP_PERMITTED, ncap, capset, CAP_CLEAR) == -1 || cap_set_flag(cap, CAP_EFFECTIVE, ncap, capset, CAP_CLEAR) == -1))
		{
			cap_free(cap);
			return(-1);
		}

		/* change our capability set */
		ncap = cap_set_proc(cap);
		cap_free(cap);
		return(ncap);
	}
	errno = ENOSYS;
	return(0);
}
#endif
/*
**
*/

#ifdef TRUSTED_MAC
#if defined(sgi)
#include <sys/attributes.h>

static const char *attrname = "SENDMAIL_MAC";
#endif

int
sm_mac_enabled()
{
#ifdef _SC_MAC
	static int m = -1;

	if (m == -1)
		m = (sysconf(_SC_MAC) > 0);
	return(m);
#else
	return(0);
#endif
}

void
sm_macdbg(level, msg, label)
	int level;
	const char *msg;
	mac_t label;
{
	int terrno = errno;
	if (tTd(70, level) && sm_mac_enabled())
	{
		char *lblstr, *errstr = "no label";

		lblstr = mac_to_text(label, (size_t *) NULL);
		if (lblstr == NULL)
			lblstr = errstr;
		printf("%s: '%s'\n", msg, lblstr);
		if (lblstr != errstr)
			mac_free(lblstr);
	}
	errno = terrno;
}

int
qf_set_mac(fp, label)
	FILE *fp;
	mac_t label;
{
	if (sm_mac_enabled())
	{
#ifdef QF_HAS_MAC
		char *lblstr = mac_to_text(label, (size_t *) NULL);
		if (lblstr != NULL)
		{
			fprintf(fp, "l%s\n", lblstr);
			mac_free(lblstr);
			sm_macdbg(2, "set qf MAC label: success", label);
			return(0);
		}
		sm_macdbg(2, "set qf MAC label: failure", label);
		return(-1);
#else
#if defined(sgi)
		int r, size = (int) mac_size(label);

		if (size != -1 && attr_setf(fileno(fp), attrname, (char *) label, size, 0) == 0)
		{
			sm_macdbg(2, "set qf MAC label: success", label);
			return(0);
		}
		sm_macdbg(2, "set qf MAC label: failure", label);
		return(-1);
#endif
#endif
	}
	errno = ENOSYS;
	return(0);
}

#ifndef QF_HAS_MAC
mac_t
qf_get_mac(fp)
	FILE *fp;
{
	if (sm_mac_enabled())
	{
#if defined(sgi)
		int r, size = 512, flags = 0;
		mac_t label;
#ifdef TRUSTED_CAP
		cap_t cap;
#endif

		/* allocate initial buffer */
		label = malloc((size_t) size);
		if (label == NULL)
		{
			sm_macdbg(2, "get qf MAC label: failure", label);
			return(label);
		}

redo:
		/* Get queue entry label */
#ifdef TRUSTED_CAP
		cap = sm_cap_acquire(flags ? SM_CAP_ATTR : SM_CAP_NONE);
#endif
		r = attr_getf(fileno(fp), attrname, (char *) label, &size,
			      flags);
#ifdef TRUSTED_CAP
		sm_cap_surrender(cap);
#endif
		if (r == -1)
		{
			if (errno == E2BIG)
			{
				/*
				 * Our buffer wasn't big enough.
				 * Reallocate and try again.
				 */
				label = realloc(label, size);
				if (label != NULL)
					goto redo;
			}

			if (errno == ENOATTR && flags == 0)
			{
				/*
				 * The attribute doesn't exist. It may exist
				 * in the root namespace because of an older
				 * version of sendmail. Check there. If it
				 * doesn't exist there, do not continue.
				 */
				flags = ATTR_ROOT;
				goto redo;
			}

			/*
			 * If there is any error other than the ones
			 * checked above, return error.
			 */
			free(label);
			label = NULL;
			sm_macdbg(2, "get qf MAC label: failure", label);
			return(label);
		}

		/*
		 * If the label is not valid, return error to the caller.
		 * If the label was found in the root namespace, copy to the
		 * user namespace.
		 */
		if (mac_valid(label) <= 0 || (flags == ATTR_ROOT && attr_setf(fileno(fp), attrname, (char *) label, size, 0) == -1))
		{
			free(label);
			label = NULL;
			sm_macdbg(2, "get qf MAC label: failure", label);
			return(label);
		}
		sm_macdbg(2, "get qf MAC label: success", label);
		return(label);
#endif
	}
	/*
	 * MAC is not supported. Return error with errno == ENOSYS
	 * to indicate that the error is not fatal.
	 */
	errno = ENOSYS;
	return(NULL);
}
#endif	/* QF_HAS_MAC */

mac_t
qdir_get_mac(file)
	const char *file;
{
	if (sm_mac_enabled())
	{
		mac_t label;
#ifdef TRUSTED_CAP
		cap_t cap = sm_cap_acquire(SM_CAP_MACREAD);
#endif
		label = mac_get_file(file);
		sm_macdbg(2, "get queue directory MAC label", label);
#ifdef TRUSTED_CAP
		sm_cap_surrender(cap);
#endif
		return(label);
	}
	/*
	 * MAC is not supported. Return error with errno == ENOSYS
	 * to indicate that the error is not fatal.
	 */
	errno = ENOSYS;
	return(NULL);
}

mac_t
sm_mac_dup(old)
	mac_t old;
{
	if (sm_mac_enabled())
	{
		mac_t new;

		new = mac_dup(old);
		sm_macdbg(1, "duplicate envelope MAC label", old);
		return(new);
	}
	/*
	 * MAC is not supported. Return error with errno == ENOSYS
	 * to indicate that the error is not fatal.
	 */
	errno = ENOSYS;
	return(NULL);
}

/*
 * Get and/or set the current process MAC label
 */
static int
sm_plabel(new, old)
	mac_t new, *old;
{
	if (sm_mac_enabled())
	{
		if (old != NULL)
		{
			*old = mac_get_proc();
			sm_macdbg(3, "get process MAC label", *old);
			if (*old == NULL)
				return(-1);
		}
		if (new != NULL)
		{
			int r;
#ifdef TRUSTED_CAP
			cap_t cap = sm_cap_acquire(SM_CAP_PLABEL);
#endif
			r = mac_set_proc(new);
			sm_macdbg(3, "set process MAC label", new);
#ifdef TRUSTED_CAP
			sm_cap_surrender(cap);
#endif
			return(r);
		}
	}
	else
	{
		if (old != NULL)
			*old = NULL;
		errno = ENOSYS;
	}
	return(0);
}

/*
 * Set the current process MAC label, returning the previous MAC label
 */
mac_t
sm_mac_swap(new)
	mac_t new;
{
	mac_t old;

	if (sm_plabel(new, &old) == -1)
	{
		mac_free(old);
		old = NULL;
	}
	return(old);
}

/*
 * Set the current process MAC label, then free the passed in MAC label.
 */
void
sm_mac_restore(label)
	mac_t label;
{
	(void) sm_plabel(label, (mac_t *) NULL);
	mac_free(label);
}

/*
 * Get the current process MAC label
 */
int
sm_setplabel(label)
	mac_t label;
{
	return(sm_plabel(label, (mac_t *) NULL));
}

/*
 * Get the current process MAC label
 */
int
sm_getplabel(label)
	mac_t *label;
{
	return(sm_plabel((mac_t) NULL, label));
}

/*
 * Check of a user is cleared to receive a message at MAC label `label'
 */
int
sm_clear_user(to, label)
	ADDRESS *to;
	mac_t label;
{
	int r = EX_OK;

	if (sm_mac_enabled())
	{
#if defined(sgi)
		struct clearance *clp;
		char *user, buf[64];

		if (to != NULL)
		{
			if (!bitset(QGOODUID, to->q_flags))
				return(EX_OK);
			user = to->q_ruser ? to->q_ruser : to->q_user;
		}
		else
		{
			user = DefUser;
		}

		clp = sgi_getclearancebyname(user);
		if (clp == (struct clearance *) NULL ||
		    mac_clearedlbl(clp, label) != MAC_CLEARED)
		{
#ifdef TRUSTED_AUDIT
			sm_audit(user, "not cleared for mail");
#endif
			r = EX_NOPERM;
		}

		sprintf(buf, "'%s' %s for mail", user,
			r == EX_OK ? "cleared" : "not cleared");
		sm_macdbg(5, buf, label);
#endif
	}
	return (r);
}
#endif
/*
**
*/

#ifdef TRUSTED_NET
/*
 * Create a socket that can receive communication at any MAC label
 */
int
sm_trusted_socket(fd)
	int fd;
{
#if defined(sgi)
	if (tsix_on(fd) == -1)
	{
		(void) close(fd);
		fd = -1;
	}
#endif
	return(fd);
}

/*
 * Get the MAC label of a socket connection
 */
int
sm_getsolabel(fd, label)
	int fd;
	mac_t *label;
{
	*label = NULL;
#if defined(sgi)
	if (tsix_get_mac(fd, label) == -1)
	{
		(void) close(fd);
		fd = -1;
	}
#endif
	sm_macdbg(4, "get socket MAC label", *label);
	return(fd);
}

/*
 * Set the MAC label of outgoing data
 */
int
sm_setsolabel(fd, label)
	int fd;
	mac_t label;
{
#if defined(sgi)
	if (tsix_set_mac(fd, label) == -1 ||
	    tsix_set_solabel(fd, label) == -1)
	{
		(void) close(fd);
		fd = -1;
	}
#endif
	sm_macdbg(4, "set socket MAC label", label);
	return(fd);
}
#endif
/*
**
*/

#ifdef TRUSTED_AUDIT
static int
sm_audit_enabled()
{
#ifdef _SC_AUDIT
	static int a = -1;

	if (a == -1)
		a = (sysconf(_SC_AUDIT) > 0);
	return(a);
#else
	return(0);
#endif
}

/*
 * Create an audit record for sendmail
 */
void
sm_audit(user, msg)
	const char *user;
	const char *msg;
{
	if (sm_audit_enabled())
	{
		int status;
#if defined(sgi)
		const char *prog = "SENDMAIL";
#ifdef TRUSTED_CAP
		cap_t cap;
#endif

		if (user == NULL)
			user = "unknown user";
#ifdef TRUSTED_CAP
		cap = sm_cap_acquire(SM_CAP_AUDIT);
#endif
		status = satvwrite(SAT_AE_CUSTOM, SAT_FAILURE, "%s|%c|%s|%s",
				   prog, '-',  user, msg);
#ifdef TRUSTED_CAP
		sm_cap_surrender(cap);
#endif
#endif
		if (status == -1)
			sm_syslog(LOG_ERR|LOG_AUTH, NOQID,
				  "%s: satvwrite failure", prog);
	}
}
#endif
/*
** sm_set[re]*[gu]id 	-- wrapper for setuid and friends
*/

int 
sm_setuid(uid, trusted)
	uid_t uid;
	int trusted;
{
	int r;
#ifdef TRUSTED_CAP
	cap_t cap = sm_cap_acquire(trusted);
#endif
	r = setuid(uid);
#ifdef TRUSTED_CAP
	sm_cap_surrender(cap);
#endif
	return (r);
}

int 
sm_seteuid(uid, trusted)
	uid_t uid;
	int trusted;
{
	int r;
#ifdef TRUSTED_CAP
	cap_t cap = sm_cap_acquire(trusted);
#endif
	r = seteuid(uid);
#ifdef TRUSTED_CAP
	sm_cap_surrender(cap);
#endif
	return (r);
}

int 
sm_setreuid(uid, euid, trusted)
	uid_t uid, euid;
	int trusted;
{
	int r;
#ifdef TRUSTED_CAP
	cap_t cap = sm_cap_acquire(trusted);
#endif
	r = setreuid(uid, euid);
#ifdef TRUSTED_CAP
	sm_cap_surrender(cap);
#endif
	return (r);
}

int 
sm_setgid(gid, trusted)
	gid_t gid;
	int trusted;
{
	int r;
#ifdef TRUSTED_CAP
	cap_t cap = sm_cap_acquire(trusted);
#endif
	r = setgid(gid);
#ifdef TRUSTED_CAP
	sm_cap_surrender(cap);
#endif
	return (r);
}

/*
** sm_initgroups	-- wrapper for initgroups
** sm_setgroups 	-- wrapper for setgruops
**
** 	These wrappers are stub implementations for systems that dont support
**	these calls.
*/
int
sm_initgroups(name, group, trusted)
	char *name;
	gid_t group;
	int trusted;
{
#ifndef HASINITGROUPS
	return 0;
#else
	int r;
#ifdef TRUSTED_CAP
	cap_t cap = sm_cap_acquire(trusted);
#endif
	r = initgroups(name,group);
#ifdef TRUSTED_CAP
	sm_cap_surrender(cap);
#endif
	return (r);
#endif /* HASINITGROUPS */
}

int
sm_setgroups(ngroups, grouplist, trusted)
	int ngroups;
	GIDSET_T grouplist[];
	int trusted;
{
#ifndef NGROUPS_MAX
	return 0;
#else
	int r;
#ifdef TRUSTED_CAP
	cap_t cap = sm_cap_acquire(trusted);
#endif
	r = setgroups(ngroups, grouplist);
#ifdef TRUSTED_CAP
	sm_cap_surrender(cap);
#endif
	return (r);
#endif /* NGROUPS_MAX */
}

/*
** sm_bind		-- wrapper for bind
*/
int
sm_bind(s, name, namelen, trusted)
	int s, namelen, trusted;
	const struct sockaddr *name;
{
	int r;
#ifdef TRUSTED_CAP
	cap_t cap = sm_cap_acquire(trusted);
#endif
	r = bind(s,name,namelen);
#ifdef TRUSTED_CAP
	sm_cap_surrender(cap);
#endif
	return (r);
}

/*
** sm_chroot		-- wrapper for chroot
*/
int
sm_chroot(path, trusted)
	const char *path;
	int trusted;
{
	int r;
#ifdef TRUSTED_CAP
	cap_t cap = sm_cap_acquire(trusted);
#endif
	r = chroot(path);
#ifdef TRUSTED_CAP
	sm_cap_surrender(cap);
#endif
	return (r);
}

/*
** sm_chown		-- wrapper for chown
*/
int
sm_chown(path, owner, group, trusted)
	const char *path;
	uid_t owner;
	gid_t group;
	int trusted;
{
	int r;
#ifdef TRUSTED_CAP
	cap_t cap = sm_cap_acquire(trusted);
#endif
	r = chown(path, owner, group);
#ifdef TRUSTED_CAP
	sm_cap_surrender(cap);
#endif
	return (r);
}

/*
** sm_fchown		-- wrapper for fchown
*/
int
sm_fchown(fd, owner, group, trusted)
	int fd;
	uid_t owner;
	gid_t group;
	int trusted;
{
	int r;
#ifdef TRUSTED_CAP
	cap_t cap = sm_cap_acquire(trusted);
#endif
	r = fchown(fd, owner, group);
#ifdef TRUSTED_CAP
	sm_cap_surrender(cap);
#endif
	return (r);
}

/*
**
*/
int
sm_nice(prio, trusted)
	int prio, trusted;
{
	int r;
#ifdef TRUSTED_CAP
	cap_t cap = sm_cap_acquire(trusted);
#endif
	r = nice(prio);
#ifdef TRUSTED_CAP
	sm_cap_surrender(cap);
#endif
	return (r);
}

/*
**  SECUREWARE_SETUP_SECURE -- Convex SecureWare setup
**
**	Set up the trusted computing environment for C2 level security
**	under SecureWare.
**
**	Parameters:
**		uid -- uid of the user to initialize in the TCB
**
**	Returns:
**		none
**
**	Side Effects:
**		Initialized the user in the trusted computing base
*/

#if SECUREWARE

# include <sys/security.h>
# include <prot.h>

void
secureware_setup_secure(uid)
	UID_T uid;
{
	int rc;

	if (getluid() != -1)
		return;
	
	if ((rc = set_secure_info(uid)) != SSI_GOOD_RETURN)
	{
		switch (rc)
		{
		  case SSI_NO_PRPW_ENTRY:
			syserr("No protected passwd entry, uid = %d", uid);
			break;

		  case SSI_LOCKED:
			syserr("Account has been disabled, uid = %d", uid);
			break;

		  case SSI_RETIRED:
			syserr("Account has been retired, uid = %d", uid);
			break;

		  case SSI_BAD_SET_LUID:
			syserr("Could not set LUID, uid = %d", uid);
			break;

		  case SSI_BAD_SET_PRIVS:
			syserr("Could not set kernel privs, uid = %d", uid);

		  default:
			syserr("Unknown return code (%d) from set_secure_info(%d)", 
				rc, uid);
			break;
		}
		finis(FALSE, EX_NOPERM);
	}
}
#endif /* SECUREWARE */
/*
**  ADD_LOCAL_HOST_NAMES -- Add a hostname to class 'w' based on IP address
**
**	Add hostnames to class 'w' based on the IP address read from
**	the network interface.
**
**	Parameters:
**		sa -- a pointer to a SOCKADDR containing the address
**
**	Returns:
**		0 if successful, -1 if host lookup fails.
*/

int
add_hostnames(sa)
	SOCKADDR *sa;
{
	char ip_addr[256];
	struct hostent *hp;
	extern char *inet_ntoa();

	/* lookup name with IP address */
	switch (sa->sa.sa_family)
	{
		case AF_INET:
			/* save IP address in text from */
			(void) snprintf(ip_addr, sizeof ip_addr, "[%.*s]",
					sizeof ip_addr - 3,
					inet_ntoa(sa->sin.sin_addr));
			if (!wordinclass(ip_addr, 'w'))
			{
				setclass('w', ip_addr);
				if (tTd(0, 4))
					printf("\ta.k.a.: %s\n", ip_addr);
			}

			hp = sm_gethostbyaddr((char *) &sa->sin.sin_addr,
				sizeof(sa->sin.sin_addr), sa->sa.sa_family);
			break;

		default:
#if _FFR_LOG_UNSUPPORTED_FAMILIES
			/* XXX: Give warning about unsupported family */
			if (LogLevel > 3)
				sm_syslog(LOG_WARNING, NOQID,
					  "Unsupported address family %d: %.100s",
					  sa->sa.sa_family, anynet_ntoa(sa));
#endif
			return -1;
	}

	if (hp == NULL)
	{
		int save_errno = errno;

		if (LogLevel > 3)
			sm_syslog(LOG_WARNING, NOQID,
				"gethostbyaddr(%.100s) failed: %d\n",
				anynet_ntoa(sa),
#if NAMED_BIND
				h_errno
#else
				-1
#endif
				);
		errno = save_errno;
		return -1;
	}

	/* save its cname */
	if (!wordinclass((char *) hp->h_name, 'w'))
	{
		setclass('w', (char *) hp->h_name);
		if (tTd(0, 4))
			printf("\ta.k.a.: %s\n", hp->h_name);
	}

	/* save all it aliases name */
	while (*hp->h_aliases)
	{
		if (!wordinclass(*hp->h_aliases, 'w'))
		{
			setclass('w', *hp->h_aliases);
			if (tTd(0, 4))
				printf("\ta.k.a.: %s\n", *hp->h_aliases);
		}
		hp->h_aliases++;
	}
	return 0;
}
/*
**  LOAD_IF_NAMES -- load interface-specific names into $=w
**
**	Parameters:
**		none.
**
**	Returns:
**		none.
**
**	Side Effects:
**		Loads $=w with the names of all the interfaces.
*/

#if defined(SIOCGIFCONF) && !SIOCGIFCONF_IS_BROKEN
struct rtentry;
struct mbuf;
# include <arpa/inet.h>
# ifndef SUNOS403
#  include <sys/time.h>
# endif
# if _AIX4 >= 40300
#  undef __P
# endif
# include <net/if.h>
#endif

#ifdef IRIX_SYSCTRL
/*
** This code walks the IRIX routing sockets and determines all addresses
** for a interface.  
*/

#define DEFINED_LOAD_IF_NAMES
#include <net/if.h>
#include <sys/sysctl.h>
#include <net/route.h>

void
load_if_names(void)
{
	int loop;
	size_t needed;
	char *buf, *next, *lim, *cp;
	struct if_msghdr *ifm;
	int mib[6];
	struct ifa_msghdr *ifam;
	struct sockaddr_in *mask, *addr;
	struct sockaddr *sa;

	
	mib[0] = CTL_NET;
	mib[1] = PF_ROUTE;
	mib[2] = 0;
	mib[3] = 0;
	mib[4] = NET_RT_IFLIST;
	mib[5] = 0;

	if (sysctl(mib, 6, NULL, &needed, NULL, 0) < 0) {
		if (tTd(0, 4))
			printf("failed first sysctl: %s\n", errstring(errno));
		return;
	}
	buf = malloc(needed);
	if (! buf) {
		if (tTd(0,4))
			printf("load_if_names: failed to malloc memory for sysctl\n");
		return;
	}
	if (sysctl(mib, 6, buf, &needed, NULL, 0) < 0) {
		if (tTd(0, 4))
			printf("failed second sysctl: %s\n", errstring(errno));
		return;
	}

	if (tTd(0,40)) 
		printf("scanning interfaces via sysctl table\n");
	
	lim = buf + needed;
	for (next = buf; next < lim; next += ifm->ifm_msglen) {
		addr = mask = (struct sockaddr_in *)0;
		loop = 0;
		ifm = (struct if_msghdr *)next;

		ifam = (struct ifa_msghdr *)ifm;
		loop = (ifm->ifm_flags & IFF_LOOPBACK);

		if (loop) 
			continue;
		  
		if (ifam->ifam_addrs & RTA_NETMASK) {
			mask = (struct sockaddr_in *)(ifam + 1);
		}
		if (ifam->ifam_addrs & RTA_IFA) {
#define ROUNDUP(a) ((a) > 0 ? (1 + (((a) - 1) | (sizeof(__uint64_t) - 1))) \
	    : sizeof(__uint64_t))
#ifdef ADVANCE
#undef ADVANCE
#endif
#ifdef _HAVE_SA_LEN
#define ADVANCE(x, n) (x += ROUNDUP((n)->sa_len))
#else
#define ADVANCE(x, n) (x += ROUNDUP(_FAKE_SA_LEN_DST(n)))
#endif
			cp = (char *)mask;
			sa = (struct sockaddr *)mask;
			ADVANCE(cp, sa);
			addr = (struct sockaddr_in *)cp;
		}
		if (! addr)
			continue;

		(void) add_hostnames((SOCKADDR *)addr);
	}
	free(buf);
}
#endif /* IRIX_SYSCTRL */

#ifndef DEFINED_LOAD_IF_NAMES

void
load_if_names()
{
#if defined(SIOCGIFCONF) && !SIOCGIFCONF_IS_BROKEN
	int s;
	int i;
	struct ifconf ifc;
	int numifs;

	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s == -1)
		return;

	/* get the list of known IP address from the kernel */
# if defined(SIOCGIFNUM) && !SIOCGIFNUM_IS_BROKEN
	if (ioctl(s, SIOCGIFNUM, (char *) &numifs) < 0)
	{
		/* can't get number of interfaces -- fall back */
		if (tTd(0, 4))
			printf("SIOCGIFNUM failed: %s\n", errstring(errno));
		numifs = -1;
	}
	else if (tTd(0, 42))
		printf("system has %d interfaces\n", numifs);
	if (numifs < 0)
# endif
		numifs = 512;

	if (numifs <= 0)
	{
		close(s);
		return;
	}
	ifc.ifc_len = numifs * sizeof (struct ifreq);
	ifc.ifc_buf = xalloc(ifc.ifc_len);
	if (ioctl(s, SIOCGIFCONF, (char *)&ifc) < 0)
	{
		if (tTd(0, 4))
			printf("SIOGIFCONF failed: %s\n", errstring(errno));
		close(s);
		return;
	}

	/* scan the list of IP address */
	if (tTd(0, 40))
		printf("scanning for interface specific names, ifc_len=%d\n",
			ifc.ifc_len);

	for (i = 0; i < ifc.ifc_len; )
	{
		struct ifreq *ifr = (struct ifreq *) &ifc.ifc_buf[i];
		SOCKADDR *sa = (SOCKADDR *) &ifr->ifr_addr;
		struct in_addr ia;
#ifdef SIOCGIFFLAGS
		struct ifreq ifrf;
#endif
		char ip_addr[256];
		extern char *inet_ntoa();

#ifdef BSD4_4_SOCKADDR
		if (sa->sa.sa_len > sizeof ifr->ifr_addr)
			i += sizeof ifr->ifr_name + sa->sa.sa_len;
		else
#endif
			i += sizeof *ifr;

		if (tTd(0, 20))
			printf("%s\n", anynet_ntoa(sa));

		if (ifr->ifr_addr.sa_family != AF_INET)
			continue;

#ifdef SIOCGIFFLAGS
		bzero(&ifrf, sizeof(struct ifreq));
		strncpy(ifrf.ifr_name, ifr->ifr_name, sizeof(ifrf.ifr_name));
		ioctl(s, SIOCGIFFLAGS, (char *) &ifrf);
		if (tTd(0, 41))
			printf("\tflags: %x\n", ifrf.ifr_flags);
# define IFRFREF ifrf
#else
# define IFRFREF (*ifr)
#endif
		if (!bitset(IFF_UP, IFRFREF.ifr_flags))
			continue;

		/* extract IP address from the list*/
		ia = sa->sin.sin_addr;
		if (ia.s_addr == INADDR_ANY || ia.s_addr == INADDR_NONE)
		{
			message("WARNING: interface %s is UP with %s address",
				ifr->ifr_name, inet_ntoa(ia));
			continue;
		}

		/* save IP address in text from */
		(void) snprintf(ip_addr, sizeof ip_addr, "[%.*s]",
			sizeof ip_addr - 3,
			inet_ntoa(ia));
		if (!wordinclass(ip_addr, 'w'))
		{
			setclass('w', ip_addr);
			if (tTd(0, 4))
				printf("\ta.k.a.: %s\n", ip_addr);
		}

		/* skip "loopback" interface "lo" */
		if (bitset(IFF_LOOPBACK, IFRFREF.ifr_flags))
			continue;

		(void) add_hostnames(sa);
	}
	free(ifc.ifc_buf);
	close(s);
# undef IFRFREF
#endif
}
#endif /* DEFINED_LOAD_IF_NAMES */

/*
**  GET_NUM_PROCS_ONLINE -- return the number of processors currently online
**
**	Parameters:
**		none.
**
**	Returns:
**		The number of processors online.
*/

int
get_num_procs_online()
{
	int nproc = 0;

#if _FFR_SCALE_LA_BY_NUM_PROCS
#ifdef _SC_NPROCESSORS_ONLN
	nproc = (int) sysconf(_SC_NPROCESSORS_ONLN);
#endif
#endif
	if (nproc <= 0)
		nproc = 1;
	return nproc;
}
/*
**  SM_SYSLOG -- syslog wrapper to keep messages under SYSLOG_BUFSIZE
**
**	Parameters:
**		level -- syslog level
**		id -- envelope ID or NULL (NOQUEUE)
**		fmt -- format string
**		arg... -- arguments as implied by fmt.
**
**	Returns:
**		none
*/

/* VARARGS3 */
void
# ifdef __STDC__
sm_syslog(int level, const char *id, const char *fmt, ...)
# else
sm_syslog(level, id, fmt, va_alist)
	int level;
	const char *id;
	const char *fmt;
	va_dcl
#endif
{
	static char *buf = NULL;
	static size_t bufsize = MAXLINE;
	char *begin, *end;
	int seq = 1;
	int idlen;
	extern int SnprfOverflow;
	extern int SyslogErrno;
	extern char *DoprEnd;
	VA_LOCAL_DECL
	extern void sm_dopr __P((char *, const char *, va_list));
	
	SyslogErrno = errno;
	if (id == NULL)
	{
		id = "NOQUEUE";
		idlen = 9;
	}
	else if (strcmp(id, NOQID) == 0)
	{
		id = "";
		idlen = 0;
	}
	else
		idlen = strlen(id + 2);
bufalloc:
	if (buf == NULL)
		buf = (char *) xalloc(sizeof(char) * bufsize);

	/* do a virtual vsnprintf into buf */
	VA_START(fmt);
	buf[0] = 0;
	DoprEnd = buf + bufsize - 1;
	SnprfOverflow = 0;
	sm_dopr(buf, fmt, ap);
	*DoprEnd = '\0';
	VA_END;
	/* end of virtual vsnprintf */

	if (SnprfOverflow)
	{
		/* String too small, redo with correct size */
		bufsize += SnprfOverflow + 1;
		free(buf);
		buf = NULL;
		goto bufalloc;
	}
	if ((strlen(buf) + idlen + 1) < SYSLOG_BUFSIZE)
	{
#if LOG
		if (*id == '\0')
			syslog(level, "%s", buf);
		else
			syslog(level, "%s: %s", id, buf);
#else
		/*XXX should do something more sensible */
		if (*id == '\0')
			fprintf(stderr, "%s\n", buf);
		else
			fprintf(stderr, "%s: %s\n", id, buf);
#endif
		return;
	}

	begin = buf;
	while (*begin != '\0' &&
	       (strlen(begin) + idlen + 5) > SYSLOG_BUFSIZE) 
	{
		char save;
	
		if (seq == 999)
		{
			/* Too many messages */
			break;
		}
		end = begin + SYSLOG_BUFSIZE - idlen - 12;
		while (end > begin)
		{
			/* Break on comma or space */
			if (*end == ',' || *end == ' ')
			{
				end++;	  /* Include separator */
				break;
			}
			end--;
		}
		/* No separator, break midstring... */
		if (end == begin)
			end = begin + SYSLOG_BUFSIZE - idlen - 12;
		save = *end;
		*end = 0;
#if LOG
		syslog(level, "%s[%d]: %s ...", id, seq++, begin);
#else
		fprintf(stderr, "%s[%d]: %s ...\n", id, seq++, begin);
#endif
		*end = save;
		begin = end;
	}
	if (seq == 999)
#if LOG
		syslog(level, "%s[%d]: log terminated, too many parts", id, seq);
#else
		fprintf(stderr, "%s[%d]: log terminated, too many parts\n", id, seq);
#endif
	else if (*begin != '\0')
#if LOG
		syslog(level, "%s[%d]: %s", id, seq, begin);
#else
		fprintf(stderr, "%s[%d]: %s\n", id, seq, begin);
#endif
}
/*
**  HARD_SYSLOG -- call syslog repeatedly until it works
**
**	Needed on HP-UX, which apparently doesn't guarantee that
**	syslog succeeds during interrupt handlers.
*/

#if defined(__hpux) && !defined(HPUX11)

# define MAXSYSLOGTRIES	100
# undef syslog
# ifdef V4FS
#  define XCNST	const
#  define CAST	(const char *)
# else
#  define XCNST
#  define CAST
# endif

void
# ifdef __STDC__
hard_syslog(int pri, XCNST char *msg, ...)
# else
hard_syslog(pri, msg, va_alist)
	int pri;
	XCNST char *msg;
	va_dcl
# endif
{
	int i;
	char buf[SYSLOG_BUFSIZE];
	VA_LOCAL_DECL;

	VA_START(msg);
	vsnprintf(buf, sizeof buf, msg, ap);
	VA_END;

	for (i = MAXSYSLOGTRIES; --i >= 0 && syslog(pri, CAST "%s", buf) < 0; )
		continue;
}

# undef CAST
#endif
/*
**  LOCAL_HOSTNAME_LENGTH
**
**	This is required to get sendmail to compile against BIND 4.9.x
**	on Ultrix.
*/

#if defined(ultrix) && NAMED_BIND

# include <resolv.h>
# if __RES >= 19931104 && __RES < 19950621

int
local_hostname_length(hostname)
	char *hostname;
{
	int len_host, len_domain;

	if (!*_res.defdname)
		res_init();
	len_host = strlen(hostname);
	len_domain = strlen(_res.defdname);
	if (len_host > len_domain &&
	    (strcasecmp(hostname + len_host - len_domain,_res.defdname) == 0) &&
	    hostname[len_host - len_domain - 1] == '.')
		return len_host - len_domain - 1;
	else
		return 0;
}

# endif
#endif

/*
**  Compile-Time options
*/

char	*CompileOptions[] =
{
#ifdef HESIOD
	"HESIOD",
#endif
#if HES_GETMAILHOST
	"HES_GETMAILHOST",
#endif
#ifdef LDAPMAP
	"LDAPMAP",
#endif
#ifdef MAP_REGEX
	"MAP_REGEX",
#endif
#if LOG
	"LOG",
#endif
#if MATCHGECOS
	"MATCHGECOS",
#endif
#if MIME7TO8
	"MIME7TO8",
#endif
#if MIME8TO7
	"MIME8TO7",
#endif
#if NAMED_BIND
	"NAMED_BIND",
#endif
#ifdef NDBM
	"NDBM",
#endif
#if NETINET
	"NETINET",
#endif
#if NETINFO
	"NETINFO",
#endif
#if NETISO
	"NETISO",
#endif
#if NETNS
	"NETNS",
#endif
#if NETUNIX
	"NETUNIX",
#endif
#if NETX25
	"NETX25",
#endif
#ifdef NEWDB
	"NEWDB",
#endif
#ifdef NIS
	"NIS",
#endif
#ifdef NISPLUS
	"NISPLUS",
#endif
#if QUEUE
	"QUEUE",
#endif
#if SCANF
	"SCANF",
#endif
#if SMTP
	"SMTP",
#endif
#if SMTPDEBUG
	"SMTPDEBUG",
#endif
#ifdef SUID_ROOT_FILES_OK
	"SUID_ROOT_FILES_OK",
#endif
#if TCPWRAPPERS
	"TCPWRAPPERS",
#endif
#if USERDB
	"USERDB",
#endif
#if XDEBUG
	"XDEBUG",
#endif
#ifdef XLA
	"XLA",
#endif
	NULL
};


/*
**  OS compile options.
*/

char	*OsCompileOptions[] =
{
#if BOGUS_O_EXCL
	"BOGUS_O_EXCL",
#endif
#if HASFCHMOD
	"HASFCHMOD",
#endif
#if HASFLOCK
	"HASFLOCK",
#endif
#if HASGETDTABLESIZE
	"HASGETDTABLESIZE",
#endif
#if HASGETUSERSHELL
	"HASGETUSERSHELL",
#endif
#if HASINITGROUPS
	"HASINITGROUPS",
#endif
#if HASLSTAT
	"HASLSTAT",
#endif
#if HASSETREUID
	"HASSETREUID",
#endif
#if HASSETRLIMIT
	"HASSETRLIMIT",
#endif
#if HASSETSID
	"HASSETSID",
#endif
#if HASSETUSERCONTEXT
	"HASSETUSERCONTEXT",
#endif
#if HASSETVBUF
	"HASSETVBUF",
#endif
#if HASSNPRINTF
	"HASSNPRINTF",
#endif
#if HAS_ST_GEN
	"HAS_ST_GEN",
#endif
#if HASSTRERROR
	"HASSTRERROR",
#endif
#if HASULIMIT
	"HASULIMIT",
#endif
#if HASUNAME
	"HASUNAME",
#endif
#if HASUNSETENV
	"HASUNSETENV",
#endif
#if HASWAITPID
	"HASWAITPID",
#endif
#if IDENTPROTO
	"IDENTPROTO",
#endif
#if IP_SRCROUTE
	"IP_SRCROUTE",
#endif
#if O_EXLOCK && HASFLOCK && !BOGUS_O_EXCL
	"LOCK_ON_OPEN",
#endif
#if NEEDFSYNC
	"NEEDFSYNC",
#endif
#if NOFTRUNCATE
	"NOFTRUNCATE",
#endif
#if RLIMIT_NEEDS_SYS_TIME_H
	"RLIMIT_NEEDS_SYS_TIME_H",
#endif
#if SAFENFSPATHCONF
	"SAFENFSPATHCONF",
#endif
#if SECUREWARE
	"SECUREWARE",
#endif
#if SHARE_V1
	"SHARE_V1",
#endif
#if SIOCGIFCONF_IS_BROKEN
	"SIOCGIFCONF_IS_BROKEN",
#endif
#if SIOCGIFNUM_IS_BROKEN
	"SIOCGIFNUM_IS_BROKEN",
#endif
#if SYS5SETPGRP
	"SYS5SETPGRP",
#endif
#if SYSTEM5
	"SYSTEM5",
#endif
#if USE_SA_SIGACTION
	"USE_SA_SIGACTION",
#endif
#if USE_SIGLONGJMP
	"USE_SIGLONGJMP",
#endif
#if USESETEUID
	"USESETEUID",
#endif
	NULL
};
