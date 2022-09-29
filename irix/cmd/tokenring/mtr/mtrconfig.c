/*
 * mtrconfig.c: 
 *     Madge Tokenring configuration command.
 * 
 *	Copyright 1996 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Load the loadble driver, if_mtr.o, and configure it.
 */
#define	ATOMIC_CFG_REQ
#define	SET_NEW_OPEN_ADDRESS_WITHOUT_RESTARTING
#undef	DEBUG

/* used by tr_user.h */
typedef struct {
	short junk;
} SCB;

#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <errno.h>
#include <sys/types.h>
#include <net/raw.h>
#include <net/if.h>
#include <sys/stream.h>
#include <sys/hashing.h>
#include <sys/tr.h>
#include <sys/llc.h>
#include <sys/tr_user.h>
#include <sys/tcp-param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mkdev.h>
#include <sys/syssgi.h>
#include <sys/mload.h>
#include <sys/major.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/udp.h>
#include <netinet/tcp.h>
#include <sys/if_mtr.h>

/* flags of what to do */

#define SETSPEED                0x01     /* set the ring speed */
#define SETMAC                  0x02     /* set the mac address */
#define SETBROADCAST            0x04     /* set the broadcast address */
#define SETMTU                  0x08     /* set the MTU of the interface */
#define RESTART                 0x10     /* restart the driver */

/* exit number that indicates different errors */
#define NO_ERROR           0
#define GENERIC_ERROR      1
#define NO_INFO_IN_CONFIG  2
#define SETSPEED_ERROR     3
#define SETMAC_ERROR       4
#define SETBROADCAST_ERROR 5
#define SETMTU_ERROR       6
#define RESTART_ERROR      7
#define USAGE_ERROR        8
#define CONFIG_FORMAT_ERROR 9
#define CONFIG_PARAM_ERROR 10

char	*Interface = NULL;
int	GotSignal;
int	DebugMode = 1;

/* request values */
struct sockaddr mac, brd;
int             mtu, ring_speed;

extern WORD sizeof_fmplus_array;
extern BYTE fmplus_image[];
extern WORD recorded_size_fmplus_array;
extern BYTE fmplus_checksum;
	
void
sig_handler(int sig)
{
        fprintf(stderr, "mtrconfig: Status: got signal %d -> exit.\n", sig);
        GotSignal = sig;
        return;
} 

void
Perror(char *string)
{
	fprintf(stderr, "mtrconfig: ERROR: %s %s[%d].\n",
		string, strerror(errno), errno);
	return;
} 

void
tr_ntoa(char *n, char *a)
{
	sprintf(a, "%02x:%02x:%02x:%02x:%02x:%02x",
		n[0], n[1], n[2], n[3], n[4], n[5]);
	return;
} 

int
tr_aton(char *a, char *n)
{
	int		cnt, idx;
	u_int		tmp[TR_MAC_ADDR_SIZE];

	cnt = sscanf(a, " %x:%x:%x:%x:%x:%x",
		&tmp[0], &tmp[1], &tmp[2], &tmp[3], &tmp[4], &tmp[5]);
	if( cnt != TR_MAC_ADDR_SIZE ){
		cnt = sscanf(a, " %x.%x.%x.%x.%x.%x",
			 &tmp[0], &tmp[1], &tmp[2], &tmp[3], &tmp[4], &tmp[5]);
	}
	if( cnt == TR_MAC_ADDR_SIZE ) {
		for(idx=0; idx<TR_MAC_ADDR_SIZE; idx++)
			n[idx] = (char)(tmp[idx] & 0xff);
	} else {
		memset(n, 0x0, TR_MAC_ADDR_SIZE );
	}
	return(cnt);
} 

#ifndef	min
#define	min( _x, _y )	((_x) <= (_y) ? (_x) : (_y))
#endif	/* min */

void
usage()
{
	fprintf(stderr, "mtrconfig interface-name command [command].\n");
	fprintf(stderr, "\tcommand:\n");
	fprintf(stderr,
		"\trestart:            restart the interface\n");
	fprintf(stderr, 
		"\tring_speed [4|16]:  new ring speed for the interface\n");
	fprintf(stderr, 
		"\tmac mac_addr:       new MAC addr for the interface\n");
	fprintf(stderr, 
		"\tbroadcast mac_addr: new broadcast addr for the interface\n");
	fprintf(stderr, 
		"\tmtu mtu:            new MTU for the interface\n");
	fprintf(stderr, 
		"\t-v:                 report interface's configuration\n");
	fprintf(stderr, 
		"mtrconfig [interface-name] -f config_file\n");
	exit(USAGE_ERROR);
} /* usage() */

/* 
 * report the configuration of the interface
 */
int
report_config(void) 
{
	int                  error = NO_ERROR;
	struct ifreq         ifr;
	int                  socket_fd;
	struct sockaddr_raw  sr;
	int                  size, ifr_name_size;
	char                 open_addr[80], broadcast_addr[80];
	
	if (Interface == NULL) 
	  /* should never happen */
		return GENERIC_ERROR;

	if( (socket_fd = socket(AF_RAW, SOCK_RAW, RAWPROTO_SNOOP)) == -1){
		Perror("socket(AF_RAW)");
		error = GENERIC_ERROR;
		goto done;
	}

       	bzero((char *)&sr, sizeof(sr));
	sr.sr_family = AF_RAW;
	size = min(sizeof(sr.sr_ifname), strlen(Interface) + 1);
	strncpy(sr.sr_ifname, Interface, size);
	sr.sr_port = 0;
	if( bind( socket_fd, &sr, sizeof(sr) ) == -1 ){
		Perror("bind(AF_RAW)");
		error = GENERIC_ERROR;
		goto done;
	}

	/* Get ring speed */
	ifr_name_size = min(sizeof(ifr.ifr_name), (strlen(Interface) + 1));
	bzero((char*)&ifr, sizeof(ifr));
	strncpy(ifr.ifr_name, Interface, ifr_name_size);
	if( ioctl( socket_fd, SIOC_TR_GETSPEED, (caddr_t)&ifr) < 0){
		Perror("ioctl(SIOC_TR_GETSPEED)");
		error = GENERIC_ERROR;
		return error;
	}
	ring_speed = ifr.ifr_metric;

	/* Get mtu size */
	bzero((char*)&ifr, sizeof(ifr));
	strncpy(ifr.ifr_name, Interface, ifr_name_size);
	if( ioctl( socket_fd, SIOCGIFMTU, (caddr_t)&ifr) < 0){
		Perror("ioctl(SIOCGIFMTU)");
		error = GENERIC_ERROR;
		return error;
	}
	mtu = ifr.ifr_metric;
	
	/* Get mac address */
	bzero((char*)&ifr, sizeof(ifr));
	strncpy(ifr.ifr_name, Interface, ifr_name_size);
	if( ioctl( socket_fd, SIOCGIFADDR, (caddr_t)&ifr) < 0){
		Perror("ioctl(SIOCGIFADDR)");
		error = GENERIC_ERROR;
		return error;
	}
	tr_ntoa((char *)&ifr.ifr_addr.sa_data, open_addr);
	
	/* Get broadcast address */
	bzero((char*)&ifr, sizeof(ifr));
	ifr.ifr_addr.sa_family = AF_RAW;
	strncpy(ifr.ifr_name, Interface, ifr_name_size);
	if( ioctl( socket_fd, SIOCGIFBRDADDR, (caddr_t)&ifr) < 0){
		Perror("ioctl(SIOCGIFBRDADDR)");
		error = GENERIC_ERROR;
		return error;
	}
	tr_ntoa((char *)&ifr.ifr_addr.sa_data, broadcast_addr);
	
	/* Get functional address */
	bzero((char*)&ifr, sizeof(ifr));
	ifr.ifr_addr.sa_family = AF_RAW;
	strncpy(ifr.ifr_name, Interface, ifr_name_size);
	if( ioctl( socket_fd, SIOC_MTR_GETFUNC, (caddr_t)&ifr) < 0){
		Perror("ioctl(SIOC_MTR_GETFUNC)");
		error = GENERIC_ERROR;
		return error;
	}

	fprintf(stdout, 
			"Token Ring interface %s configuration:\n", Interface);
	fprintf(stdout,
		"\t%-20s %dMb \n\t%-20s %d \n\t%-20s %s \n\t%-20s %s"
		"\n\t%-20s 0x%08x.\n",
		"ring_speed:", ring_speed, "mtu:", mtu,
		"open addr:", open_addr, "broadcast addr:",
		broadcast_addr, "functional addr:", 
		*(uint_t *)ifr.ifr_addr.sa_data);
      done:
	if (socket_fd != -1) close(socket_fd);
	return error;
} /* report_config() */

int
process_command(int cmd) 
{
	int                    error = NO_ERROR;
	struct ifreq           ifr;
	int                    ifr_name_size, size;
	char                   buf[80], buf2[80];
	int                    change = 0;
	int                    socket_fd = -1;
	struct sockaddr_raw    sr;
	struct mtr_download        restart;
#ifdef	ATOMIC_CFG_REQ
	int			param_cnt = 0;
	mtr_cfg_req_t		*cfg_p;
	struct ifreq		*ifr_p;
	mtr_restart_req_t	restart_req;
#else	/* ATOMIC_CFG_REQ */
#endif	/* ATOMIC_CFG_REQ */
	
#ifdef	ATOMIC_CFG_REQ
	bzero(&restart_req, sizeof(restart_req));
#endif	/* ATOMIC_CFG_REQ */
	if (Interface == NULL)
	  /* should never happen */
	  return GENERIC_ERROR;

	if( (socket_fd = socket(AF_RAW, SOCK_RAW, RAWPROTO_SNOOP)) == -1){
		Perror("socket(AF_RAW)");
		error = GENERIC_ERROR;
		goto done;
	}

	bzero((char *)&sr, sizeof(sr));
	sr.sr_family = AF_RAW;
	size = min(sizeof(sr.sr_ifname), (strlen(Interface) + 1));
	strncpy(sr.sr_ifname, Interface, size);
	sr.sr_port = 0;
	if( bind( socket_fd, &sr, sizeof(sr) ) == -1 ){
		Perror("bind(AF_RAW)");
		error = GENERIC_ERROR;
		goto done;
	}

	ifr_name_size = min(sizeof(ifr.ifr_name), (strlen(Interface) + 1));

	/* set speed */
#ifdef	ATOMIC_CFG_REQ
	if (cmd & SETSPEED) {
		cfg_p  = &restart_req.reqs[param_cnt++];
		cfg_p->cmd = MTR_CFG_CMD_SET_SPEED;
		cfg_p->param.ring_speed = ring_speed;
	}
#else	/* ATOMIC_CFG_REQ */
	if (cmd & SETSPEED) {
		bzero((char*)&ifr, sizeof(ifr));
		strncpy(ifr.ifr_name, Interface, ifr_name_size);
		if( ioctl( socket_fd, SIOC_TR_GETSPEED, (caddr_t)&ifr) < 0){
			Perror("ioctl(SIOC_TR_GETSPEED)");
			error = SETSPEED_ERROR;
			goto done;
		}
		if( ifr.ifr_metric == ring_speed){
			/* no change in ring speed */
			if( DebugMode ) fprintf(stderr, 
				"mtrconfig: SIOC_TR_SETSPEED: same ring speed[%d,%d] "
				"-> no change.\n", 
				ifr.ifr_metric, ring_speed);
		} else {
			if( DebugMode ) 
			  fprintf(stderr, 
				  "mtrconfig: SIOC_TR_SETSPEED: ring speed %d -> %d.\n",
				  ifr.ifr_metric, ring_speed);
			bzero((char*)&ifr, sizeof(ifr));
			strncpy(ifr.ifr_name, Interface, ifr_name_size);
			ifr.ifr_metric = ring_speed;
			if (ioctl(socket_fd, SIOC_TR_SETSPEED, (caddr_t)&ifr) < 0) {
				Perror("ioctl(SIOC_TR_SETSPEED)");
				error = SETSPEED_ERROR;
				goto done;
			} else 
			  change++;
		}
	}
#endif	/* ATOMIC_CFG_REQ */
	
	/* set mac address */
#ifdef	ATOMIC_CFG_REQ
#ifdef	SET_NEW_OPEN_ADDRESS_WITHOUT_RESTARTING
	if (cmd & SETMAC) {
		bzero((char*)&ifr, sizeof(ifr));
		ifr.ifr_addr.sa_family = AF_RAW;
		strncpy(ifr.ifr_name, Interface, ifr_name_size);
		bcopy(&mac, &(ifr.ifr_addr.sa_data), TR_MAC_ADDR_SIZE);
		if (ioctl(socket_fd, SIOCSIFADDR, (caddr_t)&ifr) < 0) {
			Perror("ioctl(SIOCSIFADDR)");
			error = SETMAC_ERROR;
			goto done;
		}
	}
#else	/* SET_NEW_OPEN_ADDRESS_WITHOUT_RESTARTING */
	if (cmd & SETMAC) {
		cfg_p  = &restart_req.reqs[param_cnt++];

		cfg_p->cmd = MTR_CFG_CMD_SET_MAC_ADDR;
		bcopy( &mac, cfg_p->param.macaddr, 
			sizeof(cfg_p->param.macaddr));
	}
#endif	/* SET_NEW_OPEN_ADDRESS_WITHOUT_RESTARTING */
#else	/* ATOMIC_CFG_REQ */
	if (cmd & SETMAC) {
		bzero((char*)&ifr, sizeof(ifr));
		strncpy(ifr.ifr_name, Interface, ifr_name_size);
		if( ioctl( socket_fd, SIOCGIFADDR, (caddr_t)&ifr) < 0){
			Perror("ioctl(SIOCGIFADDR)");
			error = SETMAC_ERROR;
			goto done;
		}

		tr_ntoa((char *)&mac, buf);
		tr_ntoa((char *)&ifr.ifr_addr.sa_data, buf2);
		if( !memcmp(&(ifr.ifr_addr.sa_data), &mac, TR_MAC_ADDR_SIZE) ){
			/* no change in mac address */
			if( DebugMode ) 
			  fprintf(stderr,
				  "mtrconfig: SIOCGIFADDR: same MAC addr: "
				  "0x%x.%s.\n", ifr.ifr_addr.sa_family, buf);
		} else {
			if( DebugMode )
			  fprintf(stderr, 
				  "mtrconfig: SIOCGIFADDR: 0x%x.%s -> 0x%x.%s.\n",
				  ifr.ifr_addr.sa_family, buf2,
				  &mac, buf);
			bzero((char*)&ifr, sizeof(ifr));
			strncpy(ifr.ifr_name, Interface, ifr_name_size);
			bcopy(&mac, &(ifr.ifr_addr.sa_data), TR_MAC_ADDR_SIZE);
			if (ioctl(socket_fd, SIOCSIFADDR, (caddr_t)&ifr) < 0) {
				Perror("ioctl(SIOCSIFADDR)");
				error = SETMAC_ERROR;
				goto done;
			} else
			  change++;
		}
	} 
#endif	/* ATOMIC_CFG_REQ */

	/* set broadcast address */
	if (cmd & SETBROADCAST) {
		bzero((char*)&ifr, sizeof(ifr));
		ifr.ifr_addr.sa_family = AF_RAW;
		strncpy(ifr.ifr_name, Interface, ifr_name_size);
		if( ioctl( socket_fd, SIOCGIFBRDADDR, (caddr_t)&ifr) < 0){
			Perror("ioctl(SIOCGIFBRDADDR)");
			error = SETBROADCAST_ERROR;
			goto done;
		}

		tr_ntoa((char *)&brd, buf);
		tr_ntoa((char *)&ifr.ifr_addr.sa_data, buf2);
		if( !memcmp(&brd, &(ifr.ifr_addr.sa_data), TR_MAC_ADDR_SIZE) ){
			/* no broadcast address change */
			if( DebugMode ) 
			  fprintf(stderr,
				  "mtrconfig: SIOCSIFBRDADDR: "
				  "same broadcast addr: 0x%x.%s.\n",
				  ifr.ifr_addr.sa_family, buf);
		} else {
			if( DebugMode )
			  fprintf(stderr,
				  "mtrconfig: SIOCSIFBRDADDR: "
				  "0x%x.%s -> 0x%x.%s.\n",
				  ifr.ifr_addr.sa_family, buf2,
				  AF_RAW, buf);
			bzero((char*)&ifr, sizeof(ifr));
			strncpy(ifr.ifr_name, Interface, ifr_name_size);
			bcopy(&brd, &(ifr.ifr_addr.sa_data), TR_MAC_ADDR_SIZE);
			if (ioctl(socket_fd, SIOCSIFBRDADDR, (caddr_t)&ifr) < 0) {
				Perror("ioctl(SIOCSIFBRDADDR)");
				error = SETBROADCAST_ERROR;
				goto done;
			} else
			  change++;
		}
	} 

	/* set mtu size */
#ifdef	ATOMIC_CFG_REQ
	if (cmd & SETMTU) {
		cfg_p  = &restart_req.reqs[param_cnt++];

		cfg_p->cmd = MTR_CFG_CMD_SET_MTU;
		cfg_p->param.mtu = mtu;
	}
#else	/* ATOMIC_CFG_REQ */
	if (cmd & SETMTU) {
		int old_mtu;

		bzero((char*)&ifr, sizeof(ifr));
		strncpy(ifr.ifr_name, Interface, ifr_name_size);
		if( ioctl( socket_fd, SIOCGIFMTU, (caddr_t)&ifr) < 0){
			Perror("ioctl(SIOCGIFMTU)");
			error = SETMTU_ERROR;
			goto done;
		}
		if( ifr.ifr_metric == mtu ){
			if( DebugMode ) 
			  /* no change in mtu size */
			  fprintf(stderr,
				  "mtrconfig: SIOCGIFMTU: mtu no change[%d,%d].\n",
				  ifr.ifr_metric, mtu);
		} else {
			old_mtu = ifr.ifr_metric;

			bzero((char*)&ifr, sizeof(ifr));
			strncpy(ifr.ifr_name, Interface, ifr_name_size);
			if( ioctl( socket_fd, SIOC_TR_GETSPEED, (caddr_t)&ifr) < 0){
				Perror("ioctl(SIOC_TR_GETSPEED)");
				error = SETMTU_ERROR;
				goto done;
			}

			if( (ifr.ifr_metric == 4  && mtu > TRMTU_4M) ||
			   (ifr.ifr_metric == 16 && mtu > TRMTU_16M) ){
				fprintf(stderr, 
					"mtrconfig: ERROR: SIOCSIFMTU "
					"ringspeed: %d  ->MTU < %d but %d!\n",
					ifr.ifr_metric, 
					(ifr.ifr_metric == 4 ? TRMTU_4M : TRMTU_16M),
					mtu); 
				goto done;
			} else {
				if( DebugMode ) 
				  fprintf(stderr, 
					  "mtrconfig: SIOCSIFMTU: mtu: %d -> %d.\n",
					  old_mtu, mtu);
				bzero((char*)&ifr, sizeof(ifr));
				strncpy(ifr.ifr_name, Interface, ifr_name_size);
				ifr.ifr_metric = mtu;
				if (ioctl(socket_fd, SIOCSIFMTU, (caddr_t)&ifr) < 0) {
					Perror("ioctl(SIOCSIFMTU)");
					error = SETMTU_ERROR;
					goto done;
				} else 
				  change++;
			}
		}
	} 
#endif	/* ATOMIC_CFG_REQ */

#ifdef	ATOMIC_CFG_REQ
	if( param_cnt != 0 || (cmd & RESTART) ){
		restart_req.dlrec.sizeof_fmplus_array
			= sizeof_fmplus_array;
		restart_req.dlrec.recorded_size_fmplus_array
			= recorded_size_fmplus_array;
		restart_req.dlrec.fmplus_checksum
			= fmplus_checksum;
		restart_req.dlrec.fmplus_image
			= (__uint64_t)fmplus_image;
#ifdef	DEBUG
		fprintf(stderr, "debug: recorded_size_fmplus_array: %d.\n",
			restart_req.dlrec.recorded_size_fmplus_array);
		fprintf(stderr, 
			"debug: fmplus_checksum: 0x%x.\n",
			restart_req.dlrec.fmplus_checksum);
		fprintf(stderr, 
			"debug: fmplus_image: 0x%x(0x%x).\n",
			restart_req.dlrec.fmplus_image,
			*fmplus_image);
		fprintf(stderr, "debug: sizeof_fmplus_array: %d.\n",
			restart_req.dlrec.sizeof_fmplus_array);
#endif	/* DEBUG */
		if( ioctl(socket_fd, SIOC_TR_RESTART, (caddr_t)& restart_req)
			< 0 ){
			Perror("ioctl(SIOC_TR_RESTART)");
			error = RESTART_ERROR;
			goto done;
		}
		
	}
#else	/* ATOMIC_CFG_REQ */
	if (change || (cmd & RESTART)) {
		restart.sizeof_fmplus_array = sizeof_fmplus_array;
		restart.fmplus_image = (__uint64_t)fmplus_image;
		restart.recorded_size_fmplus_array = 
		  recorded_size_fmplus_array;
		restart.fmplus_checksum = fmplus_checksum;
		if (ioctl(socket_fd, SIOC_TR_RESTART, (caddr_t)&restart) < 0) {
			Perror("ioctl(SIOC_TR_RESTART)");
			error = RESTART_ERROR;
			goto done;
		}
	}
#endif	/* ATOMIC_CFG_REQ */
	
      done: 
	if (socket_fd != -1) close(socket_fd);

	return error;
}

int
readfile(char *config_file)
{
	FILE *fp;
	char buffer[300];
	char ch;
	int  req = 0;
	char *value, *cmd;
	int  error = NO_ERROR;
	char *inter_face;
	char *s1, *s2;
	int  one = 0, done = 0;
	
	/* 
	 * If no interface is specified, it means that all interfaces
	 * that are in the configuration file need to be configured;
	 * otherwise, the specified interface is configured.
	 */
	if (config_file == NULL)
	  /* it should never happen */
	  return EINVAL;

	if ((fp = fopen(config_file, "r")) < 0) {
		Perror("Configuration file");
		return GENERIC_ERROR;
	}

	if (Interface != NULL) one = 1;
	while (fgets(buffer, 300, fp) != NULL) {
		req = 0;
		if (buffer[0] == '#' )
		  /* pass comment line */
		  continue;

		if ((s2 = strstr(buffer, "mtr")) == NULL) {
			/* pass blank or invalid line */
			continue;
		}

		inter_face = strtok(s2, ":");
		if ((one == 1) && 
		    strncmp(Interface, inter_face, sizeof(Interface)))
		  continue;
		else
		  done = 1;
		
		if (one == 0)
		  Interface = inter_face;
		while (!error && ((cmd = strtok(NULL, " ")) != NULL)) {
			if (cmd[0] == '\n')
			  break;
			if ((value = strtok(NULL, " ")) == NULL) {
				fclose(fp);
				return CONFIG_FORMAT_ERROR;
			}

			if (!strcmp(cmd, "ring_speed")) {
				ring_speed = atoi(value);
				if (ring_speed == 4 || ring_speed  == 16 ){
					req |= SETSPEED;
				} else {
					fprintf(stderr, 
						"mtrconfig: ERROR:  Ring speed"
						" can only be set to 4 or 16!\n");	
					error = CONFIG_PARAM_ERROR;
					return error;
				}
			} else if (!strcmp(cmd, "mac")) {
				if(tr_aton(value, (char *)&mac) == TR_MAC_ADDR_SIZE) {
					/* set mac address */
					req |= SETMAC;
				} else {
					fprintf(stderr, "mtrconfig: ERROR: "
						"%s not correct MAC addr!\n",
						optarg);
					error = CONFIG_PARAM_ERROR;
				}
			} else if (!strcmp(cmd, "broadcast")) {
				if(tr_aton(value, (char *)&brd) == TR_MAC_ADDR_SIZE){
					u_int	addr;
					
					addr = *(u_int *)&brd;
					if( addr != 0xc000ffff && addr != 0xffffffff){
						error = CONFIG_PARAM_ERROR;
					} else {
						req |= SETBROADCAST;
					}
				} else {
					error = CONFIG_PARAM_ERROR;
				}

				if( error ){
					fprintf(stderr, "mtrconfig: ERROR: "
						"%s not correct broadcast addr!\n",
						value);
					return CONFIG_PARAM_ERROR;
				}
			}else if(!strcmp(cmd, "mtu")) {
				mtu = atoi(value);
				req |= SETMTU;
			} else {				
				fprintf(stderr, "mtrconfig: Invalid option "
					"-%c", ch);
				error = CONFIG_PARAM_ERROR;
				return error;
			}
		}
		
		if (req != 0) {
			error = process_command(req);
		} else
			fprintf(stdout, "Mtrconfig: No configuration request "
				"is made on interface %s\n", Interface);
	}

	if (one == 1 && done == 0) {
		fprintf(stdout, "Mtrconfig: No configuration information is "
			"provided for interface %s\n", Interface);
		error = NO_INFO_IN_CONFIG;
	}
	if (inter_face != NULL) free(inter_face);
	fclose(fp);
	return error;
}

main(int argc, char** argv )
{
	int			ch; 
	int			error = NO_ERROR; 
	char                    *config_file = NULL;
	int                     req = 0;
	int                     fd;

	setbuf(stderr, NULL);
	setbuf(stdout, NULL);

	if( getuid() ) {
		/*	Sorry, _REAL_ uid must be root.	*/
		fprintf(stderr, "mtrconfig: ERROR: must be root to run.\n");
		exit(GENERIC_ERROR);
	}

	if ((fd = open("/tmp/mtr.lock", O_CREAT | O_EXCL)) < 0) {
		if (((errno == EEXIST) &&
		    ((fd = open("/tmp/mtr.lock", O_EXCL)) < 0)) ||
		    (errno != EEXIST)) {
			/* another mtrconfig is running */
			fprintf(stderr, "mtrconfig: ERROR: only one "
				"instance of mtrconfig can run at a time.\n");
			exit(GENERIC_ERROR);
		}
	}

	if (argc < 2) {
		usage();
	}

	signal(SIGHUP, sig_handler);
	signal(SIGTERM, sig_handler);
	signal(SIGALRM, sig_handler);
	signal(SIGINT, sig_handler);

	argc--, argv++;

	/* Assume interface name doesn't start with '-' */
	if (argv[0][0] == '-') {
		if (argv[0][1] != 'f') {
			usage();
		} 
		if (argc != 2) {
			usage();
		}
		config_file = argv[1];
		error = readfile(config_file);
		exit(error);
	} 
	
	/* specified interface */
	Interface = (char *)calloc(1, RAW_IFNAMSIZ);
	strncpy(Interface, *argv, RAW_IFNAMSIZ);
	argc--, argv++;
	while(argc > 0){
		if (!strcmp(*argv, "restart")) {
			if (argc != 1) {
				usage();
			}
			req |= RESTART;
			argc--, argv++;
		} else if (!strcmp(*argv, "-v")) {
			if (argc != 1) {
				usage();
			}
			error = report_config();
			exit(error);
		} else if (!strcmp(*argv, "-f")) {
			if (argc < 2) {
				usage();
			}
			argc--; argv++;
			error = readfile(*argv);
			exit(error);
		} else if (!strcmp(*argv, "ring_speed")) {
			if (argc < 2) {
				usage();
			}
			argc--, argv++;
			ring_speed = atoi(*argv);
			if (ring_speed  == 4 || ring_speed  == 16 ){
				req |= SETSPEED;
			} else {
				fprintf(stderr, 
					"mtrconfig: ERROR:  "
					"Ring speed can only be set to "
					"4 or 16!\n");	
				exit(GENERIC_ERROR);
			}
			argc--, argv++;
		} else if (!strcmp(*argv, "mac")) {
			if (argc < 2) {
				usage();
			}
			argc--, argv++;
			if(tr_aton(*argv, (char *)&mac) == TR_MAC_ADDR_SIZE) {
				/* set mac address */
				req |= SETMAC;
			} else {
				fprintf(stderr, "mtrconfig: ERROR: "
					"%s not correct MAC addr!\n",
					*argv);
				exit(GENERIC_ERROR);
			}
			argc--, argv++;
		} else if (!strcmp(*argv, "broadcast")) {
			if (argc < 2) {
				usage();
			}
			argc--, argv++;
			if(tr_aton( *argv, (char *)&brd) == TR_MAC_ADDR_SIZE){
				u_int	addr;

				addr = *(u_int *)&brd;
				if( addr != 0xc000ffff && addr != 0xffffffff){
					error = 4;
				} else {
					req |= SETBROADCAST;
				}
			} else {
				error = 5;
			}

			if( error ){
				fprintf(stderr, "mtrconfig: ERROR: "
					"%s not correct broadcast addr!\n",
					optarg);
			}
			argc--, argv++;
		} else if (!strcmp(*argv, "mtu")) {
			if (argc < 2) {
				usage();
			}
			argc--, argv++;
			mtu = atoi(*argv);
			req |= SETMTU;
			argc--, argv++;
		} else {
			usage();
		}
	} /* while */

	/* if no request is made, give the usage. */
	if( req == 0) {
		usage();
	}

	/* parameter setting */
	error = process_command(req);
	
	if (Interface != NULL) free(Interface);
	exit( error );
}

