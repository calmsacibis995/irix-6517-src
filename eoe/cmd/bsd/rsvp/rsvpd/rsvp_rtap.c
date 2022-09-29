
/************************ rsvp_rtap.c  ***************************
 *                                                               *
 * rtap -- RealTime Application Program: Test program for RSVP   *
 *                                                               *
 *	rsvp_rtap: makes rtap an integral part of rsvpd.         *
 *                                                               *
 *  This file contains a set of routines invoked from the main   *
 *  processing loop of rsvp_main.c, to invoke/control rtap.      *
 *                                                               *
 *                                                               *
 *****************************************************************/

/*****************************************************************************/
/*                                                                           */
/*          RSVPD -- ReSerVation Protocol Daemon                             */
/*                                                                           */
/*              USC Information Sciences Institute                           */
/*              Marina del Rey, California                                   */
/*                                                                           */
/*		Original Version: Shai Herzog, Nov. 1993.                    */
/*		Current Version:  Steven Berson & Bob Braden, May 1996.      */
/*                                                                           */
/*      Copyright (c) 1996 by the University of Southern California          */
/*      All rights reserved.                                                 */
/*                                                                           */
/*	Permission to use, copy, modify, and distribute this software and    */
/*	its documentation in source and binary forms for any purpose and     */
/*	without fee is hereby granted, provided that both the above          */
/*	copyright notice and this permission notice appear in all copies,    */
/*	and that any documentation, advertising materials, and other         */
/*	materials related to such distribution and use acknowledge that      */
/*	the software was developed in part by the University of Southern     */
/*	California, Information Sciences Institute.  The name of the         */
/*	University may not be used to endorse or promote products derived    */
/*	from this software without specific prior written permission.        */
/*                                                                           */
/*                                                                           */
/*	THE UNIVERSITY OF SOUTHERN CALIFORNIA makes no representations about */
/*	the suitability of this software for any purpose.  THIS SOFTWARE IS  */
/*	PROVIDED "AS IS" AND WITHOUT ANY EXPRESS OR IMPLIED WARRANTIES,      */
/*	INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF             */
/*	MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.                */
/*                                                                           */
/*****************************************************************************/

#include "rsvp_daemon.h"
#include "rapi_lib.h"

#ifdef INCLUDE_SYSTEM_H
#include "system.h"
#endif

#ifdef OBSOLETE_API
#define MAJOR_RAPI_VERSION 4
#else
#define MAJOR_RAPI_VERSION 5
#endif

/*	Forward declarations
 */
void		rtap_init(), rtap_loop(fd_set *), rtap_command();
void		rtap_dispatch();
void		rtap_sleep(int), rtap_sleepdone();
void		rtap_hexmsg(int, int, struct in_addr *, u_char *, int);

/*	External declarations -- in rtap_cmds.c
 */
extern void	rtap_cmds_init();
extern int	Do_Command(char *, FILE *);
extern char	*rsvp_timestamp();
extern char	*rapi_errlist[];

char           *infile = NULL;
FILE		*infp;			/* Input file pointer */
int 		rtap_fd = -1;		/* Unix socket to rsvpd */
int		rtap_insock = -1;	/* input file number */
extern int	T;			/* Current thread number; defined in rtap_cmds.c - mwang*/
int		rtap_sleeping = 0;	/* Boolean switch */

#ifndef __sgi /* this conflicts with stdio.h - mwang */
int		printf(); /* Prevent gcc warning * (?) */
#endif

#undef		FD_SETSIZE
#define		FD_SETSIZE      32


/*	rtap_init(): Invoked at rsvpd startup to initialize integral rtap.
 */
void
rtap_init()
	{
	int		major_version;
	extern int	T;

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
	rtap_insock = fileno(infp);
}

/*	rtap_loop(): Called at start of main select loop of rsvpd, to
 *	turn on appropriate select bits for rtap.
 */
void
rtap_loop(fd_set *fdsp)
	{
	/* rtap basically uses standard I/O.  However, in order to
	 *	receive asynchronous events from RAPI, it must do a
	 *	select() call on its TTY input and on the API fd.
	 */
	if (!rtap_sleeping)
		FD_SET(rtap_insock, fdsp);
	if (rtap_fd >= 0)
		FD_SET(rtap_fd, fdsp);
}

/*	rtap_dispatch(): Called from main select loop when there is a
 *	read event from the API socket.
 */
void
rtap_dispatch()
	{
	int rc;

	/*	If there is an open API pipe and if it has input
	 *	waiting, call rapi_dispatch() to receive it.
	 */
	rc = rapi_dispatch();
	if (rc) {
		rtap_fd = -1;  /* No more upcalls! */
		fprintf(stderr,  "RAPI rapi_dispatch(): err %d : %s\n",
			rc, rapi_errlist[rc]);
	}
}


void
rtap_command()
	{
	int	rc;

	if (rtap_sleeping)
		return;

	rc = Do_Command(infile, infp);
	if (rc == -1) {
		/* Closed API pipe
		 */
		rtap_fd = -1;
		printf("T%d> ", T);
	}
	else if (rc == 0) {
		/*
		 *	EOF on input.  If reading from file,
		 *	go to stdin; else simply return.
		 */
		if (infile) {
			infp = stdin;
			rtap_insock = fileno(infp);
			infile = NULL;
			printf("\nEnter ? or command:\n");
		} else
			return;
	}
	else if (!infile)
		printf("T%d> ", T);
	fflush(stdout);
}

void
rtap_sleep(int delay)
	{
	printf("%s RTAP sleeping.\n\n", rsvp_timestamp());
	add_to_timer(NULL, TIMEV_RTAPsleep, 1000*delay);
	rtap_sleeping = 1;
}

void
rtap_sleepdone()
	{
	printf("%s RTAP wakeup.\n\n", rsvp_timestamp());
	rtap_sleeping = 0;
}

void
rtap_hexmsg(int type, int vif, struct in_addr *addrp, u_char *buff, int len)
	{
	void hexf();
	struct sockaddr_in packet_sin;
	common_header *hdrp	= (common_header *)buff;
	int do_sendmsg(int, struct sockaddr_in *, struct in_addr *, int,
					 		u_char, u_char *, int);

	hdrp->rsvp_verflags = RSVP_MAKE_VERFLAGS(RSVP_VERSION, 0);
	hdrp->rsvp_type = type;
	hdrp->rsvp_cksum = 0;  /* could compute checksum */
	hdrp->rsvp_snd_TTL = 127;
	hdrp->rsvp_length = len;
	
	memset((char *) &packet_sin, 0, sizeof(struct sockaddr_in));
	packet_sin.sin_family = AF_INET;
#if BSD >= 199103
        packet_sin.sin_len = sizeof(packet_sin);
#endif
	packet_sin.sin_addr.s_addr = addrp->s_addr;

	printf("send packet to: %s on vif: %d\n",
			inet_ntoa(packet_sin.sin_addr), vif);
	hexf(stderr, buff, len);
	do_sendmsg(vif, &packet_sin, NULL, 0, 255, buff, len);
}




/* Following code fragments were for sending UDP data packets.
 * They are kept here for future reference.

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

struct timeval	timenow, timeout, nexttime;
struct timeval	Default_interval = {1, 0};


#define MAXDATA_SIZE		1500
#define DEFAULTDATA_SIZE	512
char		data_buff[MAXDATA_SIZE];
int		data_size = 0;
int		udpsock = 0;
long		send_data_bytes = 0;
long		send_data_msgs = 0;
long		recv_data_bytes = 0;
long		recv_data_msgs = 0;

***** */


/****  Timing loop for sending data
#ifdef DATA
		if (udpsock > 0)
			FD_SET(udpsock, fdsp);
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
*****/

#ifdef DATA
		if (intvp) {
			while (tvGEQ(&timenow, &nexttime)) {
				Send_data();
				tvadd(&nexttime, &timeout);
			}
		}
#endif /* DATA */

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

