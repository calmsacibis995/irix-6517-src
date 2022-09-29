/* dhcp_relay.c - main dhcp relay agent program
 * 7/3/95
 * the file relay_funcs.c contains packet processing
 */
#include <stdlib.h>
#include <stdio.h>

#include <sys/types.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <syslog.h>
#include <string.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <net/if.h>

#include "dhcp.h"
#include "dhcpdefs.h"

/* defined constants */
#define MAX_DHCP_SERVERS	16 /* maximum dhcp servers to relay */
#define MAX_OPTS		16 /* maximum number of options */
#define OPTIONS_FILE		"/etc/config/dhcp_relay.options"
#define MAX_DEFAULT_HOPS	4
#define MAX_HOPS		16
char * CONFIG_FILE = "dhcp_relay.servers";
char * DHCP_MASTER = "proclaim_server";
char * DHCP_RELAY = "proclaim_relayagent";
char * useConfigDir = "/var/dhcp/config";

/* global variables */
int	relay_secs;	/* don't relay if secs field is smaller */
int	relay_hops;	/* don't relay if hops field is larger */
int	no_dhcp_servers; /* number od dhcp servers */
struct in_addr   dhcp_server_addr[MAX_DHCP_SERVERS];

static char*	options_file;	/* file where options are stored */

/* FLAGS */
int relay_f = 0;		/* relay messages is off by default RFC 1542 */
int debug_f = 0;		/* debug flag is off */
int standalone_f = 0;		/* standalone or not */
int ProclaimServer = 0;		/* chkconfig flag */
int sleepmode_f = 0;		/* sleep for debugging */
int forwarding_f = 0;		/* forwarding for bootp only */
int always_return_netmask = 0;	/* workaround for windows bug */

/* function prototypes */
void h_cleanup();
void h_reread_servers();
int initialize(void);
int process_cmdline_options(int, char**);
int process_relayfile_options(int, char **);
int load_dhcp_servers(char*);
int process_relay_options(char*);
int convert_string2opts(char*, char**);
int parse_dhcp_server_list(char*, struct in_addr*, int*);

extern int initialize_socket(void);
extern void process_relay_messages(void);
extern int chkconf(char *, char *);
extern FILE *log_fopen(char *, char *);
static void
setIncrDbgFlg()
{
        (void)signal(SIGUSR1, (void (*)())setIncrDbgFlg);

	debug_f++;
        if (debug_f)
	  syslog(LOG_DEBUG, "Debug turned ON, Level %d\n", debug_f);
}

static void
setNoDbgFlg()
{
        (void)signal(SIGUSR2, (void (*)())setNoDbgFlg);

        if (debug_f)
	  syslog(LOG_DEBUG, "Debug turned OFF\n");
	debug_f = 0;
}

void
main (int argc, char **argv)
{
    time_t	TimeNow;	/* time of day */
    
    initialize();
    process_cmdline_options(argc, argv);
    if (!no_dhcp_servers)	/* specified on command line ? */
	load_dhcp_servers(useConfigDir); /* load from the file */
    (void) signal(SIGUSR1, setIncrDbgFlg);
    (void) signal(SIGUSR2, setNoDbgFlg);
    if (ProclaimServer) {
	if (debug_f) {
	    time(&TimeNow);
	    syslog(LOG_DEBUG, "starting at %s", ctime(&TimeNow));
	    if (no_dhcp_servers == 0)
		syslog(LOG_WARNING, "No servers configured for forwarding");
	}
	initialize_socket();
	process_relay_messages();
    }
    else {
	syslog(LOG_INFO, "Relay not configured (chkconfig)");
	exit(0);
    }
    exit(0);
}


/* handle SIGTERM  */
void
h_cleanup()
{
    syslog(LOG_DEBUG, "Terminate Signal received");
    closelog();
    exit(0);
}    

/* handle SIGHUP */
void
h_reread_servers()
{
    load_dhcp_servers(useConfigDir);
}

/* initialize - setup log, cleanup handlers, process file options
 */
int
initialize(void)
{
    struct sigaction act;	/* signal actions */
    
    relay_secs = 0;
    relay_hops = MAX_DEFAULT_HOPS;
    no_dhcp_servers = 0;
    options_file = OPTIONS_FILE;
    
    /* Before we do anything we check for chkconfig on. */
    if(chkconf(DHCP_RELAY, 0) == CHK_ON) {
	ProclaimServer = 1;
    }

    openlog("dhcp_relay", LOG_PID|LOG_CONS, LOG_DAEMON);

    if(chkconf(DHCP_MASTER, 0) == CHK_ON) {
	syslog(LOG_ERR,
	    "To run the relay agent you must turn the proclaim_server off");
	exit(1);
    }

    /* setup the signal handlers */
    act.sa_handler = h_reread_servers;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    act.sa_flags |= SA_RESTART;
    if (sigaction(SIGHUP, &act, NULL) == -1) {
	syslog(LOG_ERR, "unable to set handler for SIGUSR1: %m");
	exit(1);
    }

    /* setup cleanup handler */
    act.sa_handler = h_cleanup;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    act.sa_flags |= SA_RESTART;
    if (sigaction(SIGTERM, &act, NULL) == -1) {
	syslog(LOG_ERR, "unable to set handler for SIGTERM: %m");
	exit(1);
    } 
    return 0;
}

/* process_cmdline_options - process options n the command line
 */
int
process_cmdline_options(int argc, char **argv)
{
    int c;
    extern char *optarg;
    extern int optind;
    int errflg = 0;
    optind = 1;
    while ((c = getopt(argc, argv, "rbpfs:h:dD:o:")) != EOF)
	switch (c) {
	  case 'b':
	    standalone_f = 1;
	    break;
	  case 'p':
	    sleepmode_f = 1;
	    break;
	  case 'f':
	    forwarding_f = 1;
	    break;
	  case 's':
	    if ((relay_secs = atoi(optarg)) == 0 && errno == EINVAL)
		errflg++;
	    break;
	  case 'd':
	    debug_f = 1;
	    break;
	  case 'D':
	    parse_dhcp_server_list(optarg, dhcp_server_addr, &no_dhcp_servers);
	    break;
	  case 'o':
	    options_file = optarg; /* these are dhcp specific options */
	    if (process_relay_options(options_file))
		errflg++;
	    break;
	  case '?':
	    errflg++;
	}
    if (errflg) {
	(void)fprintf(stderr,
		      "usage: %s [-rd] [-s secs] [-h hops] [-b] [-D <server address...>] \n", argv[0]);
	exit (2);
    }
    return 0;
}

/* process_relayfile_options - those specified in the options file
 */
int
process_relayfile_options(int argc, char **argv)
{
    int c;
    extern char *optarg;
    extern int optind;
    int errflg = 0;
    optind = 1;
    while ((c = getopt(argc, argv, "c:D:")) != EOF)
	switch (c) {
	  case 'c':		/* specifies Optional DHCP Relay directory */
				/* default is /var/dhcp/config */
	    useConfigDir = strdup(optarg);
	    break;
	  case 'D':
	    parse_dhcp_server_list(optarg, dhcp_server_addr, &no_dhcp_servers);
	    break;
	  case '?':
	    errflg++;
	}
    if (errflg) {
	(void)fprintf(stderr,
		      "usage: %s [-rd] [-s secs] [-h hops] [-b] [-D <server address...>] \n", argv[0]);
	exit (2);
    }
    return 0;
}

int
load_dhcp_servers(char *configDir)
{
    FILE* fp;
    char buf[512];
    char fname[128];
    struct hostent* hp;
    int i;

    if(!configDir || (*configDir == '\0') )
	return 1;

    sprintf(fname, "%s/%s", configDir, CONFIG_FILE);

    if ((fp = log_fopen(fname, "r")) == NULL) {
	syslog(LOG_ERR, "Can't open %s: (%m)", fname);
	return 1;
    }

    for (i = 0; i < no_dhcp_servers; i++) /* zeroise existing list */
	dhcp_server_addr[i].s_addr = 0;
    no_dhcp_servers = 0;
    while(fgets(buf, 512, fp) != NULL) {
	if (*buf == '\0' || *buf == '\n' || *buf == '#')
	    continue;
	strtok(buf, " #\t\n");	/* null after address */
	hp = gethostbyname(buf);
	if (hp == (struct hostent*)0) {
	    syslog(LOG_WARNING, "Cannot resolve server address %s", buf);
	    continue;
	}
	dhcp_server_addr[no_dhcp_servers].s_addr =
			((struct in_addr*) (hp->h_addr))->s_addr;
	no_dhcp_servers++;
    }
    fclose(fp);
    return 0;
}

/* process_relay_options - processes options placed in the options file
 * and returns 0 if successful
 */
int
process_relay_options(char *fname)
{
    FILE* fp;
    char buf[512];
    char *optv[MAX_OPTS];
    int optc;
    
    if(!fname || (*fname == '\0') )
	return 1;
    if ((fp = log_fopen(fname, "r")) == NULL) {
	if (debug_f)
	    syslog(LOG_ERR, "Can't open %s: (%m)", fname);
	return 1;
    }
    optv[0] = fname;
    while(fgets(buf, 512, fp) != NULL) {
	if (*buf == '\0' || *buf == '\n' || *buf == '#')
	    continue;
	optc = convert_string2opts(buf, &optv[1]);
	if (optc) {
	    process_relayfile_options(++optc, optv); /* one more for name */
	}
    }
    fclose(fp);
    return 0;
}

int
convert_string2opts(char* buf, char** optv)
{
    char *option;
    int nopt;

    nopt = 0;

    option = strtok(buf, " \t\n");
    while (option) {
	*optv = option;
	nopt++;
	if (nopt >= MAX_OPTS-1) {
	    if (debug_f)
		syslog(LOG_DEBUG, "Too many options");
	    break;
	}
	optv++;
	option = strtok( 0, " \t\n");
    }
    return nopt;
}
    


/* parse_dhcp_server_list - parse string in server addresses
 */
int
parse_dhcp_server_list(char *str, struct in_addr *serv_addr, int *no_serv)
{
    char *addr;
    struct hostent* hp;
    
    *no_serv = 0;

    addr = strtok(str, " \t,");

    while (addr) {
	hp = gethostbyname(addr);
	if (hp == (struct hostent*)0) {
	    syslog(LOG_WARNING, "Unknown server address %s", addr);
	    continue;
	}
	serv_addr->s_addr = ((struct in_addr*)(hp->h_addr))->s_addr;
	if (serv_addr->s_addr != INADDR_NONE) {
	    (*no_serv)++;
	    serv_addr++;
	    if (*no_serv == MAX_DHCP_SERVERS) {
		syslog(LOG_ERR, "Too many servers (max allowed %d)",
		       MAX_DHCP_SERVERS);
		return (1);
	    }
	}
	addr = strtok(0, " \t,");
    }
    return 0;
}
