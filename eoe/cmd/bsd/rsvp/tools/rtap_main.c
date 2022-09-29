/*
 *  @(#) $Id: rtap_main.c,v 1.3 1997/11/14 07:36:44 mwang Exp $
 */
/*
 * rtap -- RealTime Application Program: Test program for RSVP
 *
 *	
 * 	rtap_main.c: Main program for rtap as stand-alone application
 * 
 */

#include "config.h"  /* must come first */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <math.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <netdb.h>
#include <arpa/inet.h>

#include "rapi_lib.h"

#ifdef OBSOLETE_API
#define MAJOR_RAPI_VERSION 4
#else
#define MAJOR_RAPI_VERSION 5
#endif

/*
 *	External declarations
 */
int	Do_Command(char *, FILE *);
void	rtap_cmds_init();
extern char *rapi_errlist[]; /* this needs extern - mwang */

/*
 * Forward Declarations
 */
void	rtap_sleep(int);

/*
 * Globals
 */
#define DEFAULT_MCAST_TTL	31	
#define MAX_NFLWDS      64
#define MAX_T           64

extern int      T;  		/* Current Thread number; defined in rtap_cmds.c - mwang */
FILE		*infp;		/* Input file pointer */
int 		rtap_fd = -1;	/* Unix socket to rsvpd */
struct timeval	timenow, timeout, nexttime;
struct timeval	Default_interval = {1, 0};	/* Default is 1 per second */

#ifdef DATA
#define MAXDATA_SIZE		1500
#define DEFAULTDATA_SIZE	512
char		data_buff[MAXDATA_SIZE];
int		data_size = 0;	/* size of data packet to send */
int		udpsock = 0;	/* Data socket */
long		send_data_bytes = 0;
long		send_data_msgs = 0;
long		recv_data_bytes = 0;
long		recv_data_msgs = 0;

#define tvsub(A, B)   (A)->tv_sec -= (B)->tv_sec ;\
         if (((A)->tv_usec -= (B)->tv_usec) < 0) {\
                  (A)->tv_sec-- ;\
                  (A)->tv_usec += 1000000 ; }
#define tvGEQ(A,B)   ( (A)->tv_sec > (B)->tv_sec || \
	( (A)->tv_sec == (B)->tv_sec && (A)->tv_usec >= (B)->tv_usec))
#define tvadd(A, B)   (A)->tv_sec += (B)->tv_sec ;\
         if (((A)->tv_usec += (B)->tv_usec) > 1000000) {\
                  (A)->tv_sec++ ;\
                  (A)->tv_usec -= 1000000 ; }
#endif

#undef		FD_SETSIZE
#define		FD_SETSIZE      32

char		*usage = "Usage: rtap [-f <filename> ]\n";

int
main(argc, argv)
	int             argc;
	char           *argv[];
	{
	char          **av = argv;
	char           *argscan();
	char           *infile = NULL;
	fd_set		fds, t_fds;
	int		fd_wid, i, rc, sid;
	struct timeval  intvl, *intvp;
	int		major_version;

	while (--argc > 0 && **++av == '-') {
		switch (*(1 + *av)) {

		case 'f':
			infile = argscan(&argc, &av);
			break;

		default:
			printf(usage);
			exit(1);
		}
	}

	printf("rtap [v3.1] RSVP Test Application\n");
	major_version = rapi_version()/100;
	if (major_version != MAJOR_RAPI_VERSION) {
		printf("RAPI version is %d, not %d\n", major_version,
				MAJOR_RAPI_VERSION);
		exit(1);
	} else {
		printf("RSVP API version %d.%02d\n",
		       major_version, rapi_version() % 100);
	}
	rtap_cmds_init();

	if (infile) {
		if (NULL == (infp = fopen(infile, "r"))) {
			perror("Cannot open input file");
			exit(1);
		}
	} else {
		infp = stdin;
		printf("\nEnter ? or command:\n");
		printf("T%d> ", T);
	}

	/* rtap basically uses standard I/O.  However, in order to
	 *	receive asynchronous events from RAPI, it must do a
	 *	select() call on its TTY input and on the API fd.
	 */
	
	fflush(stdout);
	FD_ZERO(&fds);
	while (1) {
		memcpy(&t_fds, &fds, FD_SETSIZE);
		FD_SET(fileno(infp), &t_fds);
		if (rtap_fd >= 0)
			FD_SET(rtap_fd, &t_fds);
#ifdef DATA
		if (udpsock > 0)
			FD_SET(udpsock, &t_fds);
		if ((Mode&S_MODE) && data_size > 0) {
			gettimeofday(&timenow, NULL);
			while (tvGEQ(&timenow, &nexttime)) {
				Send_data();
				tvadd(&nexttime, &timeout);
				gettimeofday(&timenow, NULL);
			}
			intvl = nexttime;
			tvsub(&intvl, &timenow);
			intvp = &intvl;
		}
		else
#endif
			intvp = NULL;		
		rc = select(FD_SETSIZE, &t_fds, NULL, NULL, intvp);
		if (rc < 0) {
			if (errno != EINTR) {	
				perror("select");
				exit(1);
			}
			continue;
		}
		
		gettimeofday(&timenow, NULL);
#ifdef DATA
		if (intvp) {
			while (tvGEQ(&timenow, &nexttime)) {
				Send_data();
				tvadd(&nexttime, &timeout);
			}
		}
#endif /* DATA */

		/*	If there is an open API pipe and if it has input
		 *	waiting, call rapi_dispatch() to receive it.
		 */
		if (rtap_fd>=0 && FD_ISSET(rtap_fd, &t_fds)) {
			rc = rapi_dispatch();
			if (rc) {
				rtap_fd = -1;
				printf("RAPI rapi_dispatch(): err %d : %s\n",
					rc, rapi_errlist[rc]);
			}
			continue;
		}
#ifdef DATA
		/*	If we are receiving data and if the data read bit is
		 * 	set, read it.
		 */
		if (udpsock > 0 && FD_ISSET(udpsock, &t_fds)) {
			rc = read(udpsock, data_buff, sizeof(data_buff));
			if (rc < 0) {
				perror("data read");
				exit(1);
			}
			if (rc > 0) {
				recv_data_bytes += rc;
				recv_data_msgs ++;
			}
		}
#endif /* DATA */
		/*	If there is control input, read the input line,
		 *	parse it, and execute.
		 */
		if (FD_ISSET(fileno(infp), &t_fds)) {
			rc = Do_Command(infile, infp);
			if (rc == 0) {
				/*
				 *	EOF on input.  If reading from file,
				 *	go to stdin; else exit.
				 */
				if (infile) {
					infp = stdin;
					infile = NULL;
					printf("\nEnter ? or command:\n");
				} else
					exit(0);
			}
			else if (!infile)
				printf("T%d> ", T);
			fflush(stdout);
		}
	}
}

char *
rsvp_timestamp()
        {
        struct timeval tv;
        struct tm       tmv;
        static char     buff[16];

        gettimeofday(&tv, NULL);
        memcpy(&tmv, localtime((time_t *) &tv.tv_sec), sizeof(struct tm));
        sprintf(buff, "%02d:%02d:%02d.%03d", tmv.tm_hour, tmv.tm_min,
                        tmv.tm_sec, (int) tv.tv_usec/1000);
        return(buff);
}

void
rtap_sleep(int intvl)
	{
	sleep(intvl);
}
