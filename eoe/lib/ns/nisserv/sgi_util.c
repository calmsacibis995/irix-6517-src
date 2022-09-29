#include <strings.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/uio.h>
#include <stdio.h>
#include <t6net.h>
#include <sys/mac.h>

int
sendfromto(int fd, void *buf, int buflen, int flags, struct in_addr *from,
	   void *to, int tolen, mac_t lbl)
{
	struct msghdr msg;
	struct cmsghdr *cmsg;
	struct iovec iov;
	char control[256];

	iov.iov_len = buflen;
	iov.iov_base = buf;

	if (from && from->s_addr) {
		cmsg = (struct cmsghdr *)control;
		cmsg->cmsg_type = IP_SENDSRCADDR;
		cmsg->cmsg_level = IPPROTO_IP;
		cmsg->cmsg_len = sizeof(*cmsg) + sizeof(struct in_addr);
		bcopy(from, cmsg + 1, sizeof(struct in_addr));
		msg.msg_controllen = cmsg->cmsg_len;
		msg.msg_control = control;
	} else {
		msg.msg_controllen = 0;
		msg.msg_control = (void *)0;
	}

	msg.msg_namelen = tolen;
	msg.msg_name = to;
	msg.msg_iovlen = 1;
	msg.msg_iov = &iov;

	if (tsix_set_mac(fd, lbl) == -1)
		return -1;

	return sendmsg(fd, &msg, flags);
}

int
recvfromto(int fd, void *buf, int buflen, int flags, struct sockaddr *from,
	   int fromlen, struct in_addr *to, mac_t *lbl)
{
	struct msghdr msg;
	struct cmsghdr *cmsg;
	struct iovec iov;
	char control[256];
	int status;

	iov.iov_len = buflen;
	iov.iov_base = buf;

	bzero(control, sizeof(control));

	msg.msg_namelen = fromlen;
	msg.msg_name = (caddr_t)from;
	msg.msg_iovlen = 1;
	msg.msg_iov = &iov;
	msg.msg_controllen = sizeof(control);
	msg.msg_control = control;

	if (to) {
		bzero(to, sizeof(struct in_addr));
	}

	status = recvmsg(fd, &msg, flags);
	if (status > 0)
		status = (tsix_get_mac(fd, lbl) == 0 ? status : -1);
	if (status > 0 && to) {
		for(cmsg = CMSG_FIRSTHDR(&msg); cmsg;
		    cmsg = CMSG_NXTHDR(&msg, cmsg)) {
			if (cmsg->cmsg_level == IPPROTO_IP &&
			    cmsg->cmsg_type == IP_RECVDSTADDR) {
				bcopy(CMSG_DATA(cmsg), to,
				    sizeof(struct in_addr));
			}
		}
	}
	return status;
}
