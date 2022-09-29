#undef _XOPEN_SOURCE

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>

static int
xpg4msg_to_oldmsg(const struct xpg4_msghdr *msg, struct msghdr *omsg)
{
	omsg->msg_name = msg->msg_name;
	omsg->msg_namelen = (int)msg->msg_namelen;
	if (msg->msg_namelen > (size_t)omsg->msg_namelen)
		return EINVAL;
	omsg->msg_iov = msg->msg_iov;
	omsg->msg_iovlen = msg->msg_iovlen;
	omsg->msg_accrights = msg->msg_ctrl;
	omsg->msg_accrightslen = (int)msg->msg_ctrllen;
	if (msg->msg_ctrllen > (size_t)omsg->msg_accrightslen)
		return EINVAL;
	return 0;
}

ssize_t
__xpg4_sendmsg(int s, const struct xpg4_msghdr *msg, int flags)
{
	struct msghdr omsg;
	int error;

	error = xpg4msg_to_oldmsg(msg, &omsg);
	if (error)
		return error;
	error = sendmsg(s, &omsg, flags);
	return(error);
}

int
__xpg4_accept(int s, struct sockaddr *sap, size_t *lenp)
{
	int error;
	int alen;

	if (lenp) {
	  alen = (int)*lenp;
	  if (*lenp > (size_t)alen)
		return(EINVAL);
	  error = accept(s, sap, &alen);
	  *lenp = (size_t)alen;
	} else 
	  error = accept(s, sap, 0);
	return(error);
}

int
__xpg4_bind(int s, const void *name, size_t namelen)
{
	int len = (int)namelen;

	if (namelen > (size_t)len) {
		return(EINVAL);
	}
	return(bind(s, name, len));
}

int
__xpg4_connect(int s, const void *name, size_t namelen)
{
	int len = (int)namelen;

	if (namelen > (size_t)len) {
		return(EINVAL);
	}
	return(connect(s, name, len));
}

ssize_t
__xpg4_recv(int s, void *buf, size_t len, int flags)
{
	int blen = (int)len;

	if (len > (size_t)blen) {
		return(EINVAL);
	}
	return(recv(s, buf, blen, flags));
}

int
__xpg4_recvfrom(int s, void *buf, size_t len, int flags, struct sockaddr *addr,
	size_t *addrlen)
{
	int blen = (int)len;
	int alen = (int)*addrlen;
	int error;

	if ((len > (size_t)blen) || (*addrlen > (size_t)alen)) {
		return(EINVAL);
	}
	error = recvfrom(s, buf, blen, flags, addr, &alen);
	*addrlen = (size_t)alen;
	return(error);
}

int
__xpg4_getpeername(int s, struct sockaddr *addr, size_t *len)
{
	int alen = (int)*len;
	int error;

	if (*len > (size_t)alen) {
		return(EINVAL);
	}
	error = getpeername(s, addr, &alen);
	*len = (size_t)alen;
	return(error);
}

int
__xpg4_getsockname(int s, struct sockaddr *addr, size_t *len)
{
	int alen = (int)*len;
	int error;

	if (*len > (size_t)alen) {
		return(EINVAL);
	}
	error = getsockname(s, addr, &alen);
	*len = (size_t)alen;
	return(error);
}

int
__xpg4_getsockopt(int s, int level, int optname, void *optval, size_t *optlen)
{
	int len = (int)*optlen;
	int error;

	if (*optlen > (size_t)len) {
		return(EINVAL);
	}
	error = getsockopt(s, level, optname, optval, &len);
	*optlen = (size_t)len;
	return(error);
}

ssize_t
__xpg4_send(int s, const void *buf, size_t len, int flags)
{
	int blen = (int)len;

	if (len > (size_t)blen) {
		return(EINVAL);
	}
	return(send(s, buf, blen, flags));
}

ssize_t
__xpg4_sendto(int s, const void *buf, size_t len, int flags,
	const struct sockaddr *addr, size_t addrlen)
{
	int blen = (int)len;
	int alen = (int)addrlen;

	if ((len > (size_t)blen) || (addrlen > (size_t)alen)) {
		return(EINVAL);
	}
	return(sendto(s, buf, blen, flags, addr, alen));
}

int
__xpg4_setsockopt(int s, int level, int name, const void *optval, size_t optlen)
{
	int len = (int)optlen;

	if (optlen > (size_t)len) {
		return(EINVAL);
	}
	return(setsockopt(s, level, name, optval, len));
}

int
__xpg4_socket(int domain, int type, int protocol)
{
	int s;
	int val = 1;

	s = socket(domain, type, protocol);
	if (s < 0)
		return s;
	setsockopt(s, SOL_SOCKET, SO_XOPEN, (void *)&val, sizeof(val));
	return s;
}
