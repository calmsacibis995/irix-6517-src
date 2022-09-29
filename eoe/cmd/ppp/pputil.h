/* PPP and SLIP utility declarations
 */

#ifndef __PPUTIL_H__
#define __PPUTIL_H__

#ident "$Revision: 1.23 $"


#include <sys/un.h>
#include <netinet/in.h>

#include "uucp.h"

#if !defined(PPP) && !defined(SLIP)
#error "must be compiled for either SLIP or PPP"
#endif

#ifdef DEFINE
#define PPUEXT
#define PPUDATA(x) = x
#else
#define PPUEXT extern
#define PPUDATA(x)
#endif


typedef signed char flg;


#define HEARTBEAT   55			/* awaken this often to account */


PPUEXT char *pgmname;

PPUEXT int debug;
/* some UUCP debugging for >=2,  full UUCP debugging for >=3 */
#define SOME_UUCP_DEBUG 2
#define FULL_UUCP_DEBUG 3
#define SET_Debug() (Debug = ((debug >= FULL_UUCP_DEBUG) ? 9	\
			      : ((debug >= SOME_UUCP_DEBUG) ? 3 : 0)))

PPUEXT flg camping;			/* >0 if "camping" on the remote */

#define MAX_SMODS 20
PPUEXT char *smods[MAX_SMODS+2];	/* STREAMS modules +2 for framing */
PPUEXT int num_smods;

PPUEXT flg demand_dial;

PPUEXT struct timeval clk;		/* monotone-increasing clock */
PPUEXT struct timeval cur_date;		/* normal time-of-day */

PPUEXT time_t sactime;			/* short activity timeout */
PPUEXT time_t lactime;			/* long activity timeout */
#define DEFAULT_SACTIME	30		/* must be > BEEP in if_ppp.h */
#define DEFAULT_LACTIME	300

PPUEXT char *add_route;			/* route(1) command */
PPUEXT flg conf_del_route;		/* want to delete add_route */
PPUEXT char *del_route;			/* route to delete */

PPUEXT char *start_cmd;			/* run these at bundle start */
PPUEXT char *end_cmd;			/* and end */


PPUEXT flg interact;			/* true if "interactive */
PPUEXT flg ctty;			/* >0 if have controlling tty */

PPUEXT int modfd PPUDATA(-1);		/* stream module */

PPUEXT int stderrfd PPUDATA(2);		/* errlog */
PPUEXT int stderrttyfd PPUDATA(2);	/* real stderr */
PPUEXT pid_t stderrpid;			/* PID of logger */

PPUEXT volatile pid_t add_pid PPUDATA(-1);  /* child looking for a line */

PPUEXT char ifname[30];

PPUEXT char *remote;			/* name of remote system */

PPUEXT int rendnode PPUDATA(-1);	/* socket for rendezvous */

/* where to find the SIOC_PPP_INFO data */
#ifdef SLIP
#define RENDDIR "/tmp/.slip-rendezvous"
#else
#define RENDDIR "/tmp/.ppp-rendezvous"
#define RENDDIR_STATUS_PAT RENDDIR"/ppp%d"
#define RENDDIR_STATUS_POKE_PAT RENDDIR_STATUS_PAT"-poke"
#endif
#define RENDPATH_PAT RENDDIR"/%0.3s%0.60s"
#define SYSNAME_LEN 60			/* must match "60" in RENDPATH_PAT */
#define RENDPATH_LEN (sizeof(RENDDIR)+SYSNAME_LEN+1)

#define MAX_RENDPATHS 8
PPUEXT struct rend {
    char    path[RENDPATH_LEN];
    u_char  made;
    struct stat st;
} rends[MAX_RENDPATHS];
PPUEXT nlink_t num_rends;

PPUEXT char status_path[RENDPATH_LEN];
PPUEXT char status_poke_path[RENDPATH_LEN];

PPUEXT int status_poke_fd PPUDATA(-1);

PPUEXT struct sockaddr_in lochost;
PPUEXT struct sockaddr_in remhost;

PPUEXT int metric;
PPUEXT struct sockaddr_in netmask;



extern struct dev {
	struct dev  *next;

	enum connmodes {
	    CALLER = 0,			/* simply call the other machine */
	    RE_CALLER,			/* call after doing other things */
	    RE_CALLEE,			/* called after other things */
	    Q_CALLER,			/* call if traffic appears */
	    CALLEE,			/* have been called */
	    UNKNOWN_CALL		/* not yet determined */
	} callmode;

	char	    uucp_nam[SYSNAME_LEN+1];	/* Systems file label */

	char	    line[5+MAXBASENAME*2+1];	/* full device pathname */
	char	    nodename[MAXBASENAME*2+1+1];    /* device name */
	char	    lockfile[MAXNAMESIZE+1];

	int	    devfd;		/* the tty */
	int	    dev_index;		/* STREAMS MUX index */
	int	    devfd_save;		/* the tty while linked */
	int	    unit;		/* network interface unit # */

	volatile pid_t rendpid;		/* rendezvousing process */

	struct stat dev_sbuf;		/* original mode of device */
	struct termio devtio;		/* device characteristics */

	time_t	    active;		/* port considered active until then */
	time_t	    touched;		/* when /dev node last touched */

	/* let modem cool this many seconds before trying to dial */
#	define DEFAULT_ASYNC_MODWAIT	5
	time_t	    modwait;

	/* pause this long after giving up on the modem */
	time_t	    modpause;

#	define DEFAULT_MODTRIES 5
	int	    modtries;		/* try this many times */

	enum syncmodes {
	    SYNC_OFF	=0,
	    SYNC_ON	=1,
	    SYNC_DEFAULT=2
	} sync;				/* sync or async line */
	enum syncmodes conf_sync;

	char	    xon_xoff;		/* 1=use XON/XOFF flow control */

	pid_t	    pty_pid;		/* child connecting pty and socket */

/* connection-time accounting */
	struct acct {
	    time_t  last_date;		/* previous report */
	    time_t  last_secs;
	    time_t  next_date;		/* next report */
	    time_t  call_start;		/* phone call started then */
	    time_t  toll_bound;
	    int	    calls;
	    int	    attempts;
	    int	    failures;
	    int	    answered;
	    int	    min_setup;
	    int	    max_setup;
	    int	    setup;		/* total time setting up calls */
	    time_t  sconn;		/* clock when last accounted */
	    int	    orig_conn;		/* connect time on originated calls */
	    int	    orig_idle;
	    int	    ans_conn;		/* connect time on answered calls */
	    int	    ans_idle;
	} acct;
} *dp0;

extern void no_signals(__sigret_t (*)());
extern void cleanup(int);
extern void killed(int);
extern void kludge_stderr(void);
extern void call_system(char *, char *, char *, int);
extern void no_interact(void);
extern time_t get_clk(void);
extern void timevalsub(struct timeval*, struct timeval*, struct timeval*);
extern int get_conn(struct dev*, int);
extern void ck_acct(struct dev*, int);
extern void clr_acct(struct dev*);
extern void closefds(void);
extern void log_errno(char*, char*);
extern void bad_errno(char*, char*);
extern char *ascii_str(u_char*, int);
extern void log_debug(int, char*, char*, ...);
extern void log_complain(char*, char*, ...);
extern void log_cd(int, int, char*, char*, ...);
extern void init_rand(void);
extern void grab_dev(const char*, struct dev*);
extern int set_tty_modes(struct dev*);
extern int rdy_tty_dev(struct dev*);
extern void set_ip(void);
extern void act_devs(int, int);
extern void rel_dev(struct dev*);
extern int make_rend(int);
extern int add_rend_name(char*, char*);
extern void unmade_rend(void);
extern void rm_rend(char*, char*);

extern int rendezvous(int);
extern void dologout(struct dev*);

#undef PPUEXT
#undef PPUDATA
#endif /* __PPUTIL_H__ */
