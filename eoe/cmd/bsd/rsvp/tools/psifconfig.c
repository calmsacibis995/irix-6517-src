/*
 * View or set configuration on RSVP packet scheduling interface
 *
 * $Id: psifconfig.c,v 1.1 1997/09/09 00:23:08 mwang Exp $
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/capability.h>
#include <net/if.h>
#include <netinet/in.h>
#include <net/tc_isps.h>
#include <net/rsvp.h>


char	*version="SGI psifconfig 1.0";
int	udp_socket;
struct ispsreq req;

void open_socket();
int get_config(char *ifname, int show_config);
int set_config(char *ifname, int set_options, int new_bandwidth);

/* flags for set_options */
#define SET_BANDWIDTH	0x01
#define SET_ENABLE	0x02
#define SET_DISABLE	0x04


void
usage()
{
	printf("usage: psifconfig -i ifname\n");
	printf("\t[-b bandwidth] : set reservable bandwidth in bytes/sec\n");
	printf("\t[-d] : disable packet scheduling on this interface\n");
	printf("\t[-e] : enable packet scheduling on this interface\n");
	exit(1);
}


main(int argc, char *argv[])
{
	int c, new_bandwidth, set_options=0;
	char *ifname = NULL;

	while ((c = getopt(argc, argv, "b:dei:")) != -1) {
		switch (c) {
		case 'b':
			set_options |= SET_BANDWIDTH;
			new_bandwidth = atoi(optarg);
			break;

		case 'd':
			set_options |= SET_DISABLE;
			break;

		case 'e':
			set_options |= SET_ENABLE;
			break;

		case 'i':
			ifname = optarg;
			break;

		default:
			usage();
			break;
		}
	}

	if (ifname == NULL)
		usage();

	if ((set_options & (SET_ENABLE|SET_DISABLE)) == (SET_ENABLE|SET_DISABLE))
		usage();

	open_socket();
	if (set_options) {
		get_config(ifname, 0);
		set_config(ifname, set_options, new_bandwidth);
	} else {
		get_config(ifname, 1);
	}
}


void
print_config(char *ifname, psif_config_info_t *config_p)
{
	if (config_p->flags & PSIF_PSON)
		printf("%s packet scheduling is ON\n", ifname);
	else
		printf("%s packet scheduling is OFF\n", ifname);
	printf("reserved bandwidth       %12d\n", config_p->cur_resv_bandwidth);
	printf("max reservable bandwidth %12d\n", config_p->max_resv_bandwidth);
	printf("interface bandwidth      %12d\n", config_p->int_bandwidth);
	printf("MTU                      %12d\n", config_p->mtu);
	printf("num_batch_pkts           %12d\n", config_p->num_batch_pkts);
	printf("flags           0x%x\n", config_p->flags);
	if (config_p->flags & PSIF_ADMIN_PSOFF)
		printf("\tpacket scheduler has been disabled by administrator\n");
	if (config_p->flags & PS_DISABLED)
		printf("\tdriver does not support packet scheduling.\n");
	if (config_p->flags & PSIF_POLL_TXQ)
		printf("\tdriver may update queue length without generating an interrupt.\n");
	if (config_p->flags & PSIF_TICK_INTR)
		printf("\tdriver does not supply periodic status updates.\n");

#ifdef TOO_MUCH
	printf("driver           tx q\t%3d\n", config_p->drv_qlen);
	printf("packet scheduler cl q\t%3d\n", config_p->cl_qlen);
	printf("packet scheduler be q\t%3d\n", config_p->be_qlen);
	printf("packet scheduler nc q\t%3d\n", config_p->nc_qlen);
#endif
	printf("\n");
}


int
get_config(char *ifname, int show_config)
{
	int		buf_size;
	cap_t		ocap;
	cap_value_t	cap_network_mgt = CAP_NETWORK_MGT;
	struct ispsreq		*req_p;
	psif_config_info_t	*config_p;

	req_p = (struct ispsreq *) &req;
	bzero(req_p, sizeof(struct ispsreq));

	strcpy(req_p->iq_name, ifname);
	req_p->iq_function = IF_GET_CONFIG;

	ocap = cap_acquire(1, &cap_network_mgt);
	if (ioctl(udp_socket, SIOCIFISPSCTL, req_p) < 0) {
		cap_surrender(ocap);
		perror("SIOCIFISPSCTL ioctl: ");
		exit(1);
	}
	cap_surrender(ocap);

	if (show_config)
		print_config(ifname, (psif_config_info_t *) &req_p->iq_config);

	return 0;
}

int
set_config(char *ifname, int set_options, int new_bandwidth)
{

	cap_t		ocap;
	cap_value_t	cap_network_mgt = CAP_NETWORK_MGT;
	struct ispsreq		*req_p;
	psif_config_info_t	*config_p;

	req_p = (struct ispsreq *) &req;
	strcpy(req_p->iq_name, ifname);
	req_p->iq_function = IF_SET_CONFIG;

	config_p = (psif_config_info_t *) &req_p->iq_config;
	if (set_options & SET_BANDWIDTH)
		config_p->max_resv_bandwidth = new_bandwidth;
	if (set_options & SET_DISABLE)
		config_p->flags |= PSIF_ADMIN_PSOFF;
	if (set_options & SET_ENABLE)
		config_p->flags &= ~PSIF_ADMIN_PSOFF;

	ocap = cap_acquire(1, &cap_network_mgt);
	if (ioctl(udp_socket, SIOCIFISPSCTL, req_p) < 0) {
		cap_surrender(ocap);
		perror("SIOCIFISPSCTL ioctl: ");
		exit(1);
	}
	cap_surrender(ocap);

	return 0;
}


void
open_socket()
{
	udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
	if (udp_socket == -1) {
		perror("open socket: ");
		exit(1);
	}
}

