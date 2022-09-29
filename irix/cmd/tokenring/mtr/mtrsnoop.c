/*
 * 	Copyright 1996 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Snooping the packets for if_mtr.o.
 */

#undef	ONLY_6_3

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
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/udp.h>
#include <netinet/tcp.h>
#include <net/if_arp.h>
#include <netinet/if_ether.h>
#include <mtr.h>

char	rcsinfo[] = "@(#) mtrsnoop $Revision: 1.2 $";

void
dump_tr_packet( int pkts, mtr_snoop_pkt_t *pkt, int pkt_size )
{
	struct tm		*tm;
	struct snoopheader	*sh_p;
	tr_packet_t		*tr_pkt_p;

	caddr_t			hex_ptr;
	int			hex_cnt;
	int			hex_off;

	sh_p  	= &(pkt->snoop);
	tr_pkt_p= &(pkt->token);

	hex_cnt = sh_p->snoop_packetlen;
	hex_ptr = (caddr_t)tr_pkt_p;
	hex_off	= 0;

	if( DebugMode ) dump_hex(hex_ptr, hex_cnt, hex_off);

	/*	MAC packet? */
	if( DumpMode & DUMP_MAC_PKT &&
	    (tr_pkt_p->mac.mac_fc & ~(TR_FC_PCF)) == TR_FC_TYPE_MAC ){

		if( DumpMode & DUMP_SNOOP_HDR ) 
			dump_snoop_hdr( pkts, pkt_size, "mtrsnoop", Interface, sh_p);

		dump_mac_pkt( (TR_MAC *)tr_pkt_p, (TR_MACMV *)(tr_pkt_p->data),
			pkt_size,
			(sizeof(*sh_p) + sizeof(tr_pkt_p->mac) +
			 sizeof(tr_pkt_p->rii) + sizeof(tr_pkt_p->llc1) +
			 sizeof(tr_pkt_p->snap)), Interface, pkts );

		fprintf(stdout,"\n");
		goto done;
	}

	if( ((tr_pkt_p->mac.mac_fc & TR_FC_TYPE ) == TR_FC_TYPE_LLC) &&
	    (tr_pkt_p->llc1.llc1_dsap == TR_IP_SAP) &&
	     DumpMode & DUMP_MAC_LLC_SNAP_IP ){
		u_short		snap_type;

		/*	It is an IP packet and ... */

		if( DumpMode & DUMP_SNOOP_HDR ) 
			dump_snoop_hdr( pkts, pkt_size, "mtrsnoop", Interface, sh_p);

		if( DumpMode & DUMP_MAC_HDR ) dump_mac_hdr( (TR_MAC *)tr_pkt_p );

		/*	skip MAC and RII */
		hex_off += sizeof(tr_pkt_p->mac);
		hex_off += sizeof(tr_pkt_p->rii);

		if( DumpMode & DUMP_LLC_SNAP_HDR ){
			fprintf(stdout, "%sLLC: 0x%02x.%02x.%02x, "
				"SNAP: 0x%02x%02x%02x.0x%02x%02x.\n",
				IDENT,
				tr_pkt_p->llc1.llc1_dsap,
				tr_pkt_p->llc1.llc1_ssap,
				tr_pkt_p->llc1.llc1_cont,
				tr_pkt_p->snap.snap_org[0],
				tr_pkt_p->snap.snap_org[1],
				tr_pkt_p->snap.snap_org[2],
				tr_pkt_p->snap.snap_etype[0],
				tr_pkt_p->snap.snap_etype[1]);
		}
		hex_off +=  sizeof(tr_pkt_p->llc1) + sizeof(tr_pkt_p->snap);

		/*	TCP/IP protocol.		*/
		snap_type = (tr_pkt_p->snap.snap_etype[0]<<8)+
			    (tr_pkt_p->snap.snap_etype[1]);

		switch( snap_type ){
		case ETHERTYPE_ARP:
			
			dump_arp_pkt( (struct arphdr *)&tr_pkt_p->data[0] );
			break;

		case ETHERTYPE_IP:

			dump_ip_pkt( hex_ptr, hex_off );
			break;

		default:
			/* hex format */
			break;
		}

		hex_cnt -= hex_off;
		hex_ptr += hex_off;
		dump_hex( hex_ptr, hex_cnt, hex_off);
		fprintf(stdout,"\n");
		
	}  else {

		if( DumpMode & DUMP_SNOOP_HDR ) 
			dump_snoop_hdr( pkts, pkt_size, "mtrsnoop", Interface, sh_p);

		dump_hex( hex_ptr, hex_cnt, hex_off);
		fprintf(stdout,"\n");
	}

done:
	return;
} /* dump_tr_packet() */

void
usage()
{
	return;
} /* usage() */

#define	INPUT_FD_STATE_NULL		0x0
#define	INPUT_FD_STATE_OPEN		0x1
#define	INPUT_FD_STATE_ADDSNOOP		0x2
#define	INPUT_FD_STATE_SIOCSNOOPING	0x4
main(int argc, char** argv )
{
	extern char		*optarg;
	extern int		optind;
	int			ch;
	int			res;
	int			error;
	int			pkts;

	char			*output_file;
	int			input_fd;
	int			output_fd;
	int			input_fd_state = INPUT_FD_STATE_NULL;

	mtr_snoop_pkt_t		i_buf;
	int			buf_size;

	struct sockaddr_raw	sr;
	struct snoopfilter      sf;
	int			on;

	Interface	= NULL;
	output_file	= NULL;
	input_fd	= -1;
	output_fd	= -1;
	error		= 0;
	pkts		= 0;
	on		= 0;

	if( Begin(argc, argv, REAL_ROOT_REQUIRED) != NO_ERROR ){
		goto done;
	}

	DumpMode = DUMP_FORMAT_MASK;
	while( (ch = getopt(argc, argv, "DMS:Hh:d:o:i:")) != -1 ){
		switch( ch ){
		case 'D':
			DebugMode = 1;
			break;

		case 'M':
			/* Monitor ring -> so inteprete only MAC packet */
			DumpMode = (DUMP_MAC_PKT_ONLY | DUMP_SNOOP_HDR);
			DumpSize = 256;
			break;

		case 'S':
			DumpSize = atoi(optarg);
			break;

		case 'd':
			DumpMode = atoi(optarg);
			break;

		case 'i':
			Interface = optarg;
			break;

		case 'o':
			output_file = optarg;
			break;

		case 'H':
		case 'h':
			usage();
			break;

		default:
			break;
		} /* switch */
	} /* while */
	if( Interface == NULL ){
		usage();
		goto done;
	}
	
	signal(SIGHUP, sig_handler);
	signal(SIGTERM, sig_handler);
	signal(SIGALRM, sig_handler);
	signal(SIGINT, sig_handler);

	if( (input_fd = socket(PF_RAW, SOCK_RAW, RAWPROTO_SNOOP)) == -1){
		Perror("socket()");
		goto done;
	}

	sr.sr_family = AF_RAW;
	sr.sr_port = 0;
	strncpy(sr.sr_ifname, Interface, sizeof(sr.sr_ifname));
	if( bind( input_fd, &sr, sizeof(sr) ) == -1 ){
		Perror("bind()");
		goto done;
	}

	bzero((char *) &sf, sizeof(sf));
	if( ioctl( input_fd, SIOCADDSNOOP, &sf) == -1 ){
		Perror("ioctl(SIOCADDSNOOP)");
		goto done;
	}
	input_fd_state |= INPUT_FD_STATE_ADDSNOOP;

	buf_size = sizeof(i_buf);
	if( setsockopt(input_fd, SOL_SOCKET, SO_RCVBUF, 
		(char *) &buf_size, sizeof(buf_size)) == -1){
		Perror("setsockopt(SOL_SOCKET, SO_RCVBUF)");
		goto done;
	}

	on = 1;
	if( ioctl( input_fd, SIOCSNOOPING, &on) == -1 ){
		Perror("ioctl(SIOCSNOOPING)");
		on = 0;
		goto done;
	}
	input_fd_state |= INPUT_FD_STATE_SIOCSNOOPING;
	
	if( output_file != NULL ){
		if( freopen(output_file, "wb+", stdout) == NULL ){
			Perror("freopen(output_file)");
			goto done;
		}
	}

	for(pkts = 0; GotSignal == 0 ; pkts++) {
		bzero((char *) &i_buf, sizeof(i_buf));
		if( (res = read(input_fd, (char *)&i_buf, sizeof(i_buf)))
			== -1){ 
			Perror("read() 2");
			goto done;
		}

		if( DumpMode ) dump_tr_packet( pkts, &i_buf, res );
	}

done:
	if( output_fd != -1)close( output_fd );
	if( input_fd != -1) {
		if( input_fd_state & INPUT_FD_STATE_ADDSNOOP ){
			bzero((char *) &sf, sizeof(sf));
			if( ioctl( input_fd, SIOCDELSNOOP, &sf) == -1 ){
				Perror("ioctl(SIOCDELSNOOP)");
			}
		}
		input_fd_state &= ~(INPUT_FD_STATE_ADDSNOOP);
		close( input_fd );
		input_fd_state = INPUT_FD_STATE_NULL;
	}
	exit( 0 );
} /* main() */
