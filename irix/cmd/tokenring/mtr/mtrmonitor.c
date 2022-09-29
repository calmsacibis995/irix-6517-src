/*
 * 	Copyright 1996 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Snooping the MAC frames for if_mtr.o.
 *	As it uses drain(7p), it gives much less performance impact 
 *	to the system. Also, that gives no concern regarding security.
 *
 */


typedef struct {
	short	junk;
} SCB;

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <sys/types.h>
#include <net/raw.h>
#include <sys/stream.h>
#include <sys/tr.h>
#include <sys/llc.h>
#include <sys/tr_user.h>
#include <sys/tcp-param.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/if_ether.h>
#include <sys/utsname.h>
#include <mtr.h>

#define	MTR_MONITOR_MODE_ALL		0
#define	MTR_MONITOR_MODE_EVENT_ONLY	1

int	FunctionalAddressRequired = 0;
uint_t	FunctionalAddressEnabled  = 0;

int
main(int argc, char **argv)
{
	int			error = 0;
	int			socket_fd = -1;
	int			pkt_size;
	int			pkt_cnt, max_pkts;
	int			ifr_name_size;
	int			opt;
	char			*output_file = NULL;
	struct sockaddr_raw	sr;
	struct ifreq		ifr;
	tr_drain_t		tr_drain_pkt;
	struct tm		*tm;
	struct utsname		utsname;
	char			msg[MAX_MSG_SIZE];

	if( Begin(argc, argv, REAL_ROOT_REQUIRED) != NO_ERROR ){
		goto done;
	}

	if( uname(&utsname) < 0 ){
		Perror("uname()");
	} else {
		if( !strcmp(utsname.machine, "IP32") &&
		    !strncmp(utsname.release, "6.3", 3) ){
			FunctionalAddressRequired = 1;
		}
	}

	DumpMode = 0;
	while( (opt = getopt(argc, argv, "faDi:o:")) != -1 ){
		switch(opt){
		case 'f':
			FunctionalAddressRequired = 1;
			break;

		case 'a':
			DumpMode |= DUMP_TR_ALL_EVENTS;
			break;

		case 'D':
			DebugMode = 1;
			break;

		case 'i':
			Interface = optarg;
			break;

		case 'o':
			output_file = optarg;
			break;

		default:
			break;

		} /* switch */
	}
	if( Interface == NULL || strncmp(Interface, "mtr", strlen("mtr")) ){
		sprintf(msg, "interface \"%s\" not correct!",
			Interface ? Interface : "");
		Log(LOG_ERR, msg);
		error = 10;
		goto done;	
	}
	ifr_name_size = min(sizeof(ifr.ifr_name), (strlen(Interface) + 1));

	if( DebugMode ){
		Log(LOG_DEBUG,"socket(PF_RAW, SOCK_RAW, RAWPROTO_DRAIN).");
	}
	if( (socket_fd = socket(PF_RAW, SOCK_RAW, RAWPROTO_DRAIN)) == -1){
		Perror("socket(PF_RAW, SOCK_RAW, RAWPROTO_DRAIN)");
		error = 1;
		goto done;
	}

	bzero(&sr, sizeof(sr));	
	sr.sr_family = AF_RAW;
	sr.sr_port   = TR_PORT_MAC;		/* MAC frames, excluding LLC */
	strncpy(sr.sr_ifname, Interface, sizeof(sr.sr_ifname));
	if( DebugMode ){
		sprintf(msg, "bind(%d, %d.%d.%s, %d).", 
			socket_fd, sr.sr_family, sr.sr_port,
			sr.sr_ifname, sizeof(sr));
		Log(LOG_DEBUG, msg);
	}
	if( bind(socket_fd, &sr, sizeof(sr)) == -1 ){
		Perror("bind()");
		error = 2;
		goto done;
	}

	signal(SIGHUP, sig_handler);
	signal(SIGTERM, sig_handler);
	signal(SIGALRM, sig_handler);
	signal(SIGINT, sig_handler);
	if( DebugMode ){
		sprintf(msg, "signal_handler enabled for %d, %d, %d, %d.",
			SIGHUP, SIGTERM, SIGALRM, SIGINT);
		Log(LOG_DEBUG, msg);
	}

	if( FunctionalAddressRequired ){
		FunctionalAddressEnabled = 
			(TR_FUNCTIONAL_ADDR_REM | TR_FUNCTIONAL_ADDR_CRS);
		bzero((char *)&ifr, sizeof(ifr));
        	strncpy(ifr.ifr_name, Interface, ifr_name_size);
		ifr.ifr_dstaddr.sa_family = AF_RAW;
		*(uint_t *)ifr.ifr_dstaddr.sa_data = 
			(uint_t)(TR_FUNCTIONAL_ADDR_REM | 
				TR_FUNCTIONAL_ADDR_CRS);
		if( DebugMode ){
			sprintf(msg, 
				"ioctl(%d, SIOC_MTR_ADDFUNC, %s.%d.%d).",
				socket_fd, 
				ifr.ifr_name,
				ifr.ifr_dstaddr.sa_family, 
				*(uint_t *)ifr.ifr_dstaddr.sa_data);
			Log(LOG_DEBUG, msg);
		}
		if( ioctl(socket_fd, SIOC_MTR_ADDFUNC, &ifr) == -1 ){
			Perror("ioctl(SIOC_MTR_ADDFUNC)");
			FunctionalAddressEnabled = 0;
			error = 31;
			goto done;
		}
	}

	pkt_size = sizeof(tr_drain_t);
	if( DebugMode ){
		sprintf(msg, "setsockopt(%d, %d, %d, 0x%x(%d), %d).",
			socket_fd, SOL_SOCKET, SO_RCVBUF,
			(char *)&pkt_size, pkt_size, sizeof(pkt_size));
		Log(LOG_DEBUG, msg);
	}
	if( setsockopt(socket_fd, SOL_SOCKET, SO_RCVBUF, (char *)&pkt_size,
		sizeof(pkt_size)) == -1 ){
		Perror("setsockopt(SOL_SOCKET, SO_RCVBUF)");
		error = 3;
		goto done;
	}

	if( output_file != NULL ){
		if( DebugMode ){
			sprintf(msg, "freopen(%s, %s, stdout).", output_file, "wb+");
			Log(LOG_DEBUG, msg);
		}
		if( freopen(output_file, "wb+", stdout) == NULL ){
			Perror("freopen(output_file)");
			error = 33;
			goto done;	
		}
		setbuf(stdout, NULL);
	}

	for( pkt_cnt = 0;; pkt_cnt++){

		bzero(&tr_drain_pkt, sizeof(tr_drain_pkt));
		if( (pkt_size = read(socket_fd, (char *)&tr_drain_pkt,
			sizeof(tr_drain_t))) == -1 ){

			Perror("read()");
			if( errno == EINTR && GotSignal )break;
			error = 20;
			goto done;

		} else if(pkt_size == 0) {

			sprintf(msg, "got EOF from %d -> exit.", socket_fd);
			Log(LOG_INFO, msg);
			goto done;

		} else {

			dump_snoop_hdr( pkt_cnt, pkt_size, "mtrmonitor",
				Interface, &tr_drain_pkt.sh);

			dump_mac_pkt( (TR_MAC *)&tr_drain_pkt, 
				(TR_MACMV *)tr_drain_pkt.data, pkt_size, 
				(sizeof(TR_MAC) + TR_MAC_HEADER_PAD_SIZE + 
				 sizeof(struct snoopheader)), 
				Interface, pkt_cnt);
		}
	} /* while() */

done:
	if( FunctionalAddressEnabled ){

        	bzero((char *)&ifr, sizeof(ifr));
        	strncpy(ifr.ifr_name, Interface, ifr_name_size);
        	ifr.ifr_dstaddr.sa_family = AF_RAW;
		*(uint_t *)ifr.ifr_dstaddr.sa_data = FunctionalAddressEnabled;
        	if( DebugMode ){
                	sprintf(msg,
                        	"mtrmonitor: debug: "
				"ioctl(%d, SIOC_MTR_DELFUNC, "
				"%s.%d.%d).",
                        	socket_fd, 
                        	ifr.ifr_name, 
				ifr.ifr_dstaddr.sa_family,
				*(uint_t *)ifr.ifr_dstaddr.sa_data);
			Log(LOG_DEBUG, msg);
        	}

		if( ioctl(socket_fd, SIOC_MTR_DELFUNC, &ifr) == -1 ){
			Perror("ioctl(SIOC_MTR_DELFUNC)");
		}
		FunctionalAddressEnabled = 0;
	}

	if( socket_fd != -1 )close(socket_fd);
	exit( error );
} /* main() */
