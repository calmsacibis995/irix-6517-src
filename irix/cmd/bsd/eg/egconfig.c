/*
 * Copyright 1998, Silicon Graphics, Inc. 
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or 
 * duplicated in any form, in whole or in part, without the prior written 
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions 
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or 
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished - 
 * rights reserved under the Copyright Laws of the United States.
 */
#define U32 __uint32_t
#define U64 __uint64_t

#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <stdio.h>
#include <strings.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <errno.h>
#include <sys/types.h>
#include <net/raw.h>
#include <net/if.h>
#include <sys/syssgi.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/udp.h>
#include <netinet/tcp.h>
#include <sys/if_eg.h>
#include "tgfw.h"

/* exit number that indicates different errors */
#define NO_ERROR           0
#define GENERIC_ERROR      1
#define SETNTRACE_ERROR    2
#define GETNTRACE_ERROR    3

int lflag = 0;
int rflag = 0;
int rrbs = 2;

void
Perror(char *string)
{
	fprintf(stderr, "egconfig: ERROR: %s: %s[%d].\n",
		string, strerror(errno), errno);
	return;
} 

void
usage(void)
{
	fprintf(stderr, "usage: egconfig [-l] interface-name\n");
	exit(1);
}


int
do_command(char *interface)
{
	int                  error = NO_ERROR;
	int                  socket_fd;
	struct sockaddr_raw  sr;
	struct eg_fwreq tgFw;
	struct ifreq ifr;

	if (interface == NULL) 
	  /* should never happen */
		return GENERIC_ERROR;

	if( (socket_fd = socket(AF_RAW, SOCK_RAW, RAWPROTO_SNOOP)) == -1){
		Perror("socket(AF_RAW)");
		error = GENERIC_ERROR;
		goto done;
	}

       	bzero((char *)&sr, sizeof(sr));
	sr.sr_family = AF_RAW;
	strcpy(sr.sr_ifname, interface);
	sr.sr_port = 0;
	if( bind( socket_fd, &sr, sizeof(sr) ) == -1 ){
		if (errno == EADDRNOTAVAIL) {
			fprintf(stderr, "egconfig: no such interface: %s\n",
				interface);
		} else {
			Perror("bind(AF_RAW)");
		}
		error = GENERIC_ERROR;
		goto done;
	}

	/* fill up the tg_fwreq struct */
	tgFw.releaseMajor = tigon2FwReleaseMajor;
	tgFw.releaseMinor = tigon2FwReleaseMinor;
	tgFw.releaseFix = tigon2FwReleaseFix;
	tgFw.startAddr = tigon2FwStartAddr;
	tgFw.textAddr = tigon2FwTextAddr;
	tgFw.textLen = tigon2FwTextLen;
        tgFw.dataAddr = tigon2FwDataAddr;
        tgFw.dataLen = tigon2FwDataLen;
        tgFw.rodataAddr = tigon2FwRodataAddr;
        tgFw.rodataLen = tigon2FwRodataLen;
        tgFw.bssAddr = tigon2FwBssAddr;
        tgFw.bssLen = tigon2FwBssLen;
        tgFw.sbssAddr = tigon2FwSbssAddr;
        tgFw.sbssLen = tigon2FwSbssLen;
        tgFw.fwText = (U64)tigon2FwText;
        tgFw.fwData = (U64)tigon2FwData;
        tgFw.fwRodata = (U64)tigon2FwRodata;

	strcpy(tgFw.ifr_name, interface);

	if( ioctl( socket_fd, ALT_CACHE_FW, (caddr_t)&tgFw) < 0){
			Perror("ioctl(ALT_CACHE_FW)");
			error = GENERIC_ERROR;
			return error;
	}

	if (lflag) {
		strcpy(ifr.ifr_name, interface);
		ifr.ifr_metric = 0;
		if (ioctl( socket_fd, ALT_NO_LINK_NEG, (caddr_t)&ifr) < 0) {
			Perror("ioctl(ALT_NO_LINK_NEG)");
			error = GENERIC_ERROR;
			return error;
		}
	}

	if (rflag) {
		strcpy(ifr.ifr_name, interface);
		ifr.ifr_metric = rrbs;
		if (ioctl( socket_fd, ALT_NUM_RRBS, (caddr_t)&ifr) < 0) {
			Perror("ioctl(ALT_NUM_RRBS)");
			error = GENERIC_ERROR;
			return error;
		}
	}

      done: 
	if (socket_fd != -1) close(socket_fd);

	return error;

}

void
main(int argc, char **argv)
{
	char interface[10];
	extern int optind;
	extern char *optarg;
	int c;
	
	while ((c = getopt(argc, argv, "lr:")) != EOF) {
		switch(c) {
		case 'l':
			lflag++;
			break;
		case 'r':
			rflag++;
			rrbs = atoi(optarg);
			if (rrbs < 2 || rrbs > 6) {
				fprintf(stderr,
					"egconfig: illegal RRB value %d, using 2\n",
					rrbs);
				rflag = 0;
			}
			break;
		default:
			usage();
		}
	}

	argc -= optind;
	argv += optind;

	if (argc < 1)
		usage();

	strcpy(interface, argv[0]);

	do_command(interface);
	exit(0);
}
