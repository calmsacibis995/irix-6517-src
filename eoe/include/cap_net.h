#ifndef __CAP_NET_H__
#define __CAP_NET_H__

#include <sys/types.h>
#include <sys/capability.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/schedctl.h>
#include <sched.h>
#include <sys/time.h>
#include <sys/syssgi.h>

#ifdef __cplusplus
extern "C" {
#endif

#ident "$Revision: 1.7 $"

__inline static int
cap_socket (int domain, int type, int protocol)
{
	cap_t ocap;
	const cap_value_t cap_network_mgt = CAP_NETWORK_MGT;
	int r;

	ocap = cap_acquire(1, &cap_network_mgt);
	r = socket(domain, type, protocol);
	cap_surrender(ocap);
	return(r);
}

__inline static int
#if _NO_XOPEN4
cap_setsockopt (int s, int level, int name, const void *optval, int optlen)
#else
cap_setsockopt (int s, int level, int name, const void *optval, size_t optlen)
#endif
{
	cap_t ocap;
	const cap_value_t cap_network_mgt = CAP_NETWORK_MGT;
	int r;

	ocap = cap_acquire(1, &cap_network_mgt);
	r = setsockopt(s, level, name, optval, optlen);
	cap_surrender(ocap);
	return(r);
}

__inline static int
cap_bind (int s, const struct sockaddr *name, int namelen)
{
	cap_t ocap;
	const cap_value_t cap_priv_port = CAP_PRIV_PORT;
	int r;

	ocap = cap_acquire(1, &cap_priv_port);
	r = bind(s, name, namelen);
	cap_surrender(ocap);
	return(r);
}

__inline static int
cap_network_ioctl (int fd, int request, void *arg)
{
	cap_t ocap;
	const cap_value_t driver_caps[] = {CAP_NETWORK_MGT, CAP_DEVICE_MGT};
	int r;

	ocap = cap_acquire(2, driver_caps);
	r = ioctl(fd, request, arg);
	cap_surrender(ocap);
	return (r);
}

__inline static ptrdiff_t
cap_settimetrim (long trim)
{
	cap_t ocap;
	const cap_value_t cap_time_mgt = CAP_TIME_MGT;
	ptrdiff_t r;

	ocap = cap_acquire(1, &cap_time_mgt);
	r = syssgi(SGI_SETTIMETRIM, trim);
	cap_surrender(ocap);
	return (r);
}

__inline static ptrdiff_t
cap_schedctl (int cmd, pid_t pid, long val)
{
	cap_t ocap;
	const cap_value_t cap_sched_mgt = CAP_SCHED_MGT;
	ptrdiff_t r;

	ocap = cap_acquire(1, &cap_sched_mgt);
	r = schedctl(cmd, pid, val);
	cap_surrender(ocap);
	return(r);
}

__inline static int
cap_sched_setscheduler (pid_t pid, int policy, const struct sched_param *p)
{
	cap_t ocap;
	const cap_value_t cap_sched_mgt = CAP_SCHED_MGT;
	int r;

	ocap = cap_acquire(1, &cap_sched_mgt);
	r = sched_setscheduler(pid, policy, p);
	cap_surrender(ocap);
	return(r);
}

__inline static int
cap_adjtime (struct timeval *delta, struct timeval *odelta)
{
	cap_t ocap;
	const cap_value_t cap_time_mgt = CAP_TIME_MGT;
	int r;

	ocap = cap_acquire(1, &cap_time_mgt);
	r = adjtime(delta, odelta);
	cap_surrender(ocap);
	return (r);
}

__inline static int
cap_settimeofday (struct timeval *tp, void *tzp)
{
	cap_t ocap;
	const cap_value_t cap_time_mgt = CAP_TIME_MGT;
	int r;

	ocap = cap_acquire(1, &cap_time_mgt);
	r = settimeofday(tp, tzp);
	cap_surrender(ocap);
	return (r);
}

#ifdef __cplusplus
}
#endif

#endif /* __CAP_NET_H__ */
