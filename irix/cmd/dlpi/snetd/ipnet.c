#include <sys/types.h>
#include <sys/stream.h>
#include <sys/stropts.h>
#include <netinet/in.h>
#include <sys/snet/ip_control.h>
#include <streamio.h>
#include <fcntl.h>
#include "debug.h"
#include "utils.h"

#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>

extern int trace, nobuild;

int
cf_ipnet(int argc, char **argv, int fd, int mux)
{
	struct strioctl	io;
	NET_ADDRS	net;
	struct sockaddr_in dest = { AF_INET };
	struct sockaddr_in gate = { AF_INET };
	struct sockaddr_in sub  = { AF_INET };
	struct ifreq ifr;
	char	forwb;
	char	keepalive;
	short	mtu;
	short	unit;
	char    *name;
	char    cmd[100];
	char	ifname[20];
	int	sock;
	
	if (argc <= 1) {
		error("IP_NET arg count (%d) wrong!", argc);
		return(1);
	}

	if (argv[1] == 0) {
		error("IP_NET can't default on network address!");
		return(1);
	} else
		gate.sin_addr.s_addr = inet_addr(argv[1]);

	if (argc <= 2 || argv[2] == 0) {
		if (IN_CLASSA(ntohl(gate.sin_addr.s_addr)))
			sub.sin_addr.s_addr = htonl(IN_CLASSA_NET);
		else if (IN_CLASSB(ntohl(gate.sin_addr.s_addr)))
			sub.sin_addr.s_addr = htonl(IN_CLASSB_NET);
		else if (IN_CLASSC(ntohl(gate.sin_addr.s_addr)))
			sub.sin_addr.s_addr = htonl(IN_CLASSC_NET);
		else
			sub.sin_addr.s_addr = htonl(0L);
	} else
		sub.sin_addr.s_addr = inet_addr(argv[2]);

	if (argc <= 3 || argv[3] == 0)
		forwb = 0;
	else
		forwb =  (char) (streq(argv[3], "forwb=y")? 1:0);

	if (argc <= 4 || argv[4] == 0)
		keepalive = 0;
	else
		keepalive = (char) (streq(argv[4], "kalive=y")? 1:0);

	if (argc <= 5 || argv[5] == 0)
		mtu = 0;
	else
		mtu = (short) atoi(argv[5]);

	if (argc <= 6 || argv[6] == 0)
		name = "ixe";
	else {
		name = argv[6];

		if (argc > 6 && argc < 8)
			exit(syserr("IP_NET_ADDR destination address missing"));
		else
        		dest.sin_addr.s_addr = inet_addr(argv[7]);
	}

	if (nobuild) {
		 return(0);
	}

	net.muxid = mux;
	net.inaddr = gate.sin_addr.s_addr;
	net.subnet_mask = sub.sin_addr.s_addr;
	net.forward_bdcst = forwb;
	net.keepalive = keepalive;
	net.mtu = mtu;
	
	io.ic_cmd = IP_NET_ADDR;
	io.ic_timout = 0;
	io.ic_len = sizeof(NET_ADDRS);
	io.ic_dp = (char *)&net;

	if ((unit = ioctl(fd, I_STR, &io)) < 0)
		exit(syserr("IP_NET_ADDR ioctl failed"));

	if (trace) {
		printf("ioctl(%d, I_STR,", fd);
		printf(" IP_NET_ADDR={mux=%d,", mux);	/* dummy } */
		printf(" net=%s,", inet_ntoa(gate.sin_addr));
		printf(" sub=%s,", inet_ntoa(sub.sin_addr));
		printf(" forwb=%d", (int) forwb);
		printf(" kalive=%d", (int) keepalive);

		if (argc > 7) {
			printf(" mtu=%d", (int) mtu);
			printf(" ifname=%s%d", name, unit);
			/* dummy { */
			printf(" gateway=%s})\n", argv[7]);
		} else {
			/* dummy { */
			printf(" mtu=%d})\n", (int) mtu);
		}
	}

	sprintf(ifname, "%s%d", name, unit);
	if (trace)
		printf("%s\n", ifname);

	/*
	 * This code will continue to be needed as long as
	 * SGI refuse to adopt streams for networking.
	 */

	if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
		exit(syserr("IP_NET_ADDR socket call failed"));

	(void)strncpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));

	ifr.ifr_addr = *(struct sockaddr *)&sub;
	if (ioctl(sock, SIOCSIFNETMASK, (caddr_t)&ifr) < 0)
		exit(syserr("IP_NET_ADDR ioctl SIOCSIFNETMASK failed"));

	ifr.ifr_addr = *(struct sockaddr *)&gate;
	if (ioctl(sock, SIOCSIFADDR, (caddr_t)&ifr) < 0)
		exit(syserr("IP_NET_ADDR ioctl SIOCSIFADDR failed"));

	/* Define destination address for Frame Relay only. */
	if (strcmp(name, "fr") == 0) {
		ifr.ifr_addr = *(struct sockaddr *)&dest;
		if (ioctl(sock, SIOCSIFDSTADDR, (caddr_t)&ifr) < 0)
			exit(syserr("IP_NET_ADDR ioctl SIOCSIFDSTADDR failed"));
	}

	(void)close(sock);

	/* Add route here for Frame Relay. */
	if (strcmp(name, "fr") == 0) {
		sprintf (cmd, "route add %s default 1", argv[1]);
		system  (cmd);
        	system  ("killall -ALRM routed 1>&2");
        	system  ("killall -USR1 gated 1>&2");
	}

	return(0);
}
