/*
** This is the header file for qpage.  It should not necessary to
** change anything in this file.  Edit configure.input as appropriate
** and then type "./configure" to create a customized config.h for
** your system.  Then just type "make" to build qpage.
*/
#include	"config.h"


#define		PROGRAM_NAME		"qpage"


#ifndef		TRUE
#define		TRUE			1
#endif
#ifndef		FALSE
#define		FALSE			0
#endif


/*
** tell lint to shut up
*/
#ifdef lint
#define		printf			(void)printf
#define		fprintf			(void)fprintf
#endif

#define		VERSION			"3.3beta6"
#define		COMMENTS_ANYWHERE

#ifndef		QPAGE_CONFIG
#define		QPAGE_CONFIG		"/etc/qpage.cf"
#endif

#ifndef		DEFAULT_LOCKDIR
#define		DEFAULT_LOCKDIR		"/var/spool/locks"
#endif

#ifndef		SYSLOG_FACILITY
#define		SYSLOG_FACILITY		LOG_DAEMON
#endif

#ifndef		HAVE_STRERROR
extern char	*sys_errlist[];
#define		strerror(x)		sys_errlist[x]
#endif

#ifndef		SNPP_SERVER
#define		SNPP_SERVER		"localhost"
#endif

#ifndef		DAEMON_USER
#define		DAEMON_USER		"nobody"
#endif

#define		SNPP_SVC_NAME		"snpp"
#define		SNPP_SVC_PORT		444

#define		MAXMSGSIZE		250 /* imposed by TAP blocksize */

#define		MAXBADCOMMANDS		10
#define		BUFCHUNKSIZE		1024

#define		DEFAULT_LEVEL		1
#define		DEFAULT_IDENTTIMEOUT	10
#define		DEFAULT_SNPPTIMEOUT	60
#define		DEFAULT_QUEUEDIR	"/var/spool/qpage"

/*
** default values for paging services
*/
#define		DEFAULT_INITCMD		"ATE1F1V1"
#define		DEFAULT_DIALCMD		"ATDT"
#define		DEFAULT_BAUDRATE	B9600
#define		DEFAULT_PARITY		0	/* 0=none, 1=odd, 2=even */
#define		DEFAULT_MAXMSGSIZE	80
#define		DEFAULT_MAXPAGES	9
#define		DEFAULT_MAXTRIES	6
#define		DEFAULT_IDENTFROM	TRUE
#define		DEFAULT_ALLOWPID	FALSE
#define		DEFAULT_STRIP		TRUE
#define		DEFAULT_PREFIX		TRUE

/*
** service level at which e-mail notifications shold be sent
*/
#define		LEVEL_SENDMAIL		0

#define		F_SENT			(1<<0) /* msg sent successfully */
#define		F_FAILED		(1<<1) /* msg failed */
#define		F_BUSY			(1<<2) /* modem got BUSY */
#define		F_NOCARRIER		(1<<3) /* modem got NO CARRIER */
#define		F_NOMODEM		(1<<4) /* modem was unavailable */
#define		F_FORCED		(1<<5) /* remote sent <RS> */
#define		F_NOPROMPT		(1<<6) /* didn't find ID= prompt */
#define		F_UNKNOWN		(1<<7) /* unknown error */
#define		F_REJECT		(1<<8) /* page rejected by service */
#define		F_RAWPID		(1<<9) /* user specified a PID */
#define		F_SENDMAIL		(1<<10) /* send mail to sender */
#define		F_SENTMAIL		(1<<11) /* sent mail to sender */
#define		F_SENTADMIN		(1<<12) /* sent mail to administrator */
#define		F_BADPAGE		(1<<13) /* the entire page is bad */

#define		CALLSTATUSFLAGS		0x01ff /* per-call status flags */

#define		CHAR_STX		'\002'
#define		CHAR_ETX		'\003'
#define		CHAR_ETB		'\027'
#define		CHAR_US			'\037'

#define		INVALID_TIME		((time_t)-1)


/*
** select.h is broken for Solaris 2.x.  It defines FD_ZERO
** as a macro which calls memset().  Unfortunately memset()
** returns a value, which means FD_ZERO returns a value,
** contrary to the description in the "select" man page.
**
** We cannot blindly cast this to (void) at compile time
** because it breaks on some platforms (e.g. Linux).
*/
#if defined(lint) && defined(SOLARIS2)
#define		FD_ZERO_LINTED(x)	(void)FD_ZERO(x)
#else
#define		FD_ZERO_LINTED(x)	FD_ZERO(x)
#endif


struct modem {
	struct modem		*next;		/* next node in the list */
	char			*name;		/* name of this modem */
	char			*text;		/* optional text description */
	char			*device;	/* path to modem device */
	char			*initcmd;	/* initialization command */
	char			*dialcmd;	/* dial command */
};
typedef struct modem modem_t;


struct service {
	struct service		*next;		/* next node in the list */
	char			*name;		/* name of the service */
	char			*text;		/* optional text description */
	char			*device;	/* path to modem device */
	char			*dialcmd;	/* dial command */
	char			*phone;		/* IXO/TAP phone number */
	char			*password;	/* service password */
	int			baudrate;	/* modem speed */
	int			parity;		/* modem parity */
	int			maxmsgsize;	/* max length of a page */
	int			maxpages;	/* max segments per page */
	int			maxtries;	/* max tries per page */
	int			identfrom;	/* who page is from */
	int			allowpid;	/* page must be in qpage.cf? */ 
	int			msgprefix;	/* prefix msgs with <from> */ 
	int			flags;		/* not used at this time */
};
typedef struct service service_t;


struct pager {
	struct pager		*next;		/* next node in the list */
	char			*name;		/* name of this entry */
	char			*text;		/* optional text description */
	char			*pagerid;	/* IXO pagerid */
	service_t		*service;	/* associated paging service */
	int			flags;		/* status flags */
};
typedef struct pager pager_t;


struct member {
	struct member		*next;		/* next node in the list */
	pager_t			*pager;		/* list member */
	char			*schedule;	/* when this member is valid */
	int			flags;		/* status flags */
};
typedef struct member member_t;


struct pgroup {
	struct pgroup		*next;		/* next node in the list */
	char			*name;		/* name of this group */
	char			*text;		/* optional text description */
	member_t		*members;	/* list of group members */
	int			flags;		/* status flags */
};
typedef struct pgroup pgroup_t;


struct rcpt {
	struct rcpt		*next;		/* next node in the list */
	char			*pager;		/* name of this recipient */
	char			*coverage;	/* name of paging service */
	time_t			holduntil;	/* hold page until this time */
	time_t			lasttry;	/* time of last page attempt */
	int			goodtries;	/* number of dial attempts */
	int			tries;		/* total number of attempts */
	int			level;		/* level of service */
	int			flags;		/* status flags */
};
typedef struct rcpt rcpt_t;


typedef struct page {
	struct page		*next;		/* next node in the list */
	FILE			*peer;		/* connected TCP socket */
	char			*filename;	/* name of queue file */
	rcpt_t			*rcpts;		/* recipient list */
	time_t			created;	/* time page was submitted */
	char			*message;	/* page contents */
	char			*messageid;	/* unique ID */
	char			*auth;		/* authenticated submitter */
	char			*ident;		/* RFC-1413 results */
	char			*from;		/* CALLerid argument */
	char			*hostname;	/* page origin */
	char			*status;	/* IXO status */
	int			flags;		/* status flags */
} PAGE;


struct job {
	struct job		*next;		/* next node in the list */
	PAGE			*p;		/* page structure */
	rcpt_t			*rcpt;		/* single recipient */
	service_t		*service;	/* associated service */
	pager_t			*pager;		/* name of recipient */
};
typedef struct job job_t;


/*
** global variables
*/
extern int		Debug;
extern int		Silent;
extern int		Interactive;
extern int		IdentTimeout;
extern int		SNPPTimeout;
extern int		JobsPending;
extern int		ReReadConfig;
extern int		Synchronous;
extern char		*PIDfile;
extern char		*ConfigFile;
extern char		*Administrator;
extern char		*ForceHostname;
extern char		*SigFile;
extern char		*QueueDir;
extern char		*LockDir;
extern service_t	*Services;
extern pager_t		*Pagers;
extern pgroup_t		*Groups;
extern modem_t		*Modems;

#ifndef CLIENT_ONLY
extern jmp_buf		TimeoutEnv;
#endif

/*
** misc functions
*/
extern void		*lookup(void *list, char *name);
extern void		*my_realloc(void *ptr, int len);
extern void		my_free(void *ptr);
extern void		clear_page(PAGE *p, int save);
extern void		send_pages(job_t *jobs);
extern void		read_mail(PAGE *p);
extern void		qpage_log(int pri, char *fmt, ...);
extern time_t		snpptime(char *arg);
extern time_t		parse_time(char *str);
extern int		become_daemon(int sleeptime, short port);
extern int		submit_page(PAGE *p, char *server);
extern int		lock_file(int fd, int mode, int block);
extern int		lock_queue(void);
extern int		write_page(PAGE *p, int new);
extern int		runqueue(void);
extern int		insert_jobs(job_t **joblist, PAGE *p);
extern int		lock_modem(char *device);
extern void		unlock_modem(char *device);
extern char		*ident(int peer);
extern char		*getinput(FILE *fp, int oneline);
extern char		*get_user(void);
extern char		*safe_string(char *str);
extern char		*safe_strtok(char *str, char *sep, char **l, char *r);
extern char		*strjoin(char *str1, char *str2);
extern int		get_qpage_config(char *filename);
extern int		on_duty(char *schedule, time_t when);
extern int		showqueue(void);
extern char		*my_ctime(time_t *clock);
extern char		*msgcpy(char *dst, char *src, int len);
extern void		strip(char **str);
extern void		get_sysinfo(void);
extern void		notify_submitter(PAGE *p);
extern void		notify_administrator(PAGE *p);
extern void		newmsgid(char *buff);
extern void		drop_root_privileges(void);
extern void		dump_qpage_config(char *filename);

/*
** signal handlers
*/
extern void		sigalrm(void);
extern void		sigchld(void);
extern void		sighup(void);
extern void		sigterm(void);
extern void		sigusr1(void);
