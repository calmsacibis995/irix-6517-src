#include <unistd.h>
#include <stdlib.h>
#include "cms_base.h"
#include "cms_kernel.h"
#include "cms_message.h"
#include "cms_info.h"
#include "cms_cell_status.h"
#include <stdio.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>

void
ep_mesg_init(void)
{
	struct 	sockaddr_un un_name;
	char	cellname[20];

	cip->ep_fd = socket(PF_UNIX, SOCK_DGRAM, 0);
	if (cip->ep_fd == -1) {
		perror("socket");
		exit(1);
	}

	un_name.sun_family = AF_UNIX;
	sprintf(cellname,"cell%d",cellid());
	strcpy(un_name.sun_path, cellname);
	unlink(cellname);
	printf("Binding to %s %d\n", un_name.sun_path, strlen(un_name.sun_path));
	if (bind(cip->ep_fd, &un_name, strlen(un_name.sun_path) + sizeof(un_name.sun_family)) == -1) {
		perror("bind");
		exit(1);
	}
}	

void
ep_mesg_close(void)
{
	char	cellname[20];
	close(cip->ep_fd);
	sprintf(cellname,"cell%d",cellid());
	if (unlink(cellname) == -1) {
		perror("unlink");
		exit(1);
	}
}

void
ep_send_mesg(cell_t cell, void *mesg, int len)
{
	struct  sockaddr_un un_name;
	char	cellname[20];
	int	sent;

	/*
	 * If the other cell is dead or the link is down 
	 * drop the message on the floor.
	 */
	if (cms_get_cell_status(cell) == B_FALSE)
		return;

	if (cms_get_link_status(cellid(), cell) == B_FALSE)
		return;
	un_name.sun_family = AF_UNIX;
	sprintf(cellname,"cell%d",cell);
	strcpy(un_name.sun_path, cellname);
	sent = sendto(cip->ep_fd, mesg, len, 0, &un_name,
			strlen(un_name.sun_path) + sizeof(un_name.sun_family));

	if (sent == -1) {
		perror("sendto");
		pause();
		return;
	}

	if (sent != len) {
		fprintf(stderr,"only %d bytes sent\n",sent);
		exit(1);
	}
}

int
ep_recv_mesg(void *mesg, int len)
{
	struct  sockaddr_un un_name;
	int	got;
	int	namelen;


	namelen = sizeof(un_name.sun_path) + sizeof(un_name.sun_family);
	got = recvfrom(cip->ep_fd, mesg, len, 0, &un_name, &namelen);
	if (got == -1) {
		perror("sendto");
		exit(1);
	}
	return got;
}

int
ep_mesg_wait(void *msg, int len, int timeout)
{
	fd_set	ep_fdset;	
	struct 	timeval tval;
	int	ret;
	int	total_wait_time;


	total_wait_time = 0;
	if (timeout && (timeout < HB_INTERVAL)) {
		printf("Timeout %d smaller than heart beat\n", timeout);
		abort();
	}

	while (!timeout || (total_wait_time < timeout)) {
		tval.tv_sec = HB_INTERVAL;
		tval.tv_usec = 0;

		FD_ZERO(&ep_fdset);
		FD_SET(cip->ep_fd, &ep_fdset);

		ret = select(cip->ep_fd + 1, &ep_fdset, 0, 0,  &tval);

		if (ret == -1) {
			perror("select");
			exit(1);
		}

		if (ret == 0) {
			int set_status;
			if (!cms_get_cell_status(cellid()))
				return CELL_DIED;
			if (set_status = 
			cms_get_connectivity_set(&cip->cms_new_member_set)) 
				return set_status;
			total_wait_time += HB_INTERVAL;
		} else {
			if (ep_recv_mesg(msg, len) != len) { 
				fprintf(stderr,
					"Message len does not match %d\n",
							len);
				abort();
			}
			return MESSAGE_RECEIVED;
		}
	}
	return MESSAGE_TIMEOUT;
}
