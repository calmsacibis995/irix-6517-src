/*
 *  Remote host utility
 */

#ifndef __RHOST_H__
#define __RHOST_H__

#ident "$Revision: 1.1 $"

/* define DEFINE as either "extern" or "" */
#ifdef DEFINE
#define EXT
#define INIT(x) = x
#else
#define EXT extern
#define INIT(x)
#endif

EXT char * pgmname;
EXT int debug;
EXT int verbose;
EXT int check_only;

EXT char * cfilename INIT("/etc/rhost.conf");

#define SYSNAME_LEN 60
EXT char * remote;

EXT int stderrfd INIT(2);		/* errlog */
EXT int stderrttyfd INIT(2);		/* real stderr */
EXT pid_t stderrpid;			/* PID of logger */

extern int parse_conf(int);

#undef EXT
#undef INIT
#endif /* __RHOST_H__ */

