#include <stdio.h>
#include <time.h>
#include <lber.h>
#include <ldap.h> 
#include "ldap_dhcp.h"

static ldhcp_callback_proc *callbacks[FD_SETSIZE];
extern fd_set ldhcp_readset, ldhcp_exceptset, ldhcp_sendset;
extern int ldhcp_maxfd;

ldhcp_callback_proc *
ldhcp_callback_new(int fd, ldhcp_callback_proc *proc, unsigned flags)
{
	callbacks[fd] = proc;

	/*
	** Add this socket to the central select.
	*/
	if (flags & LDHCP_READ) {
		FD_SET(fd, &ldhcp_readset);
	}
	if (flags & LDHCP_WRITE) {
		FD_SET(fd, &ldhcp_sendset);
	}
	if (flags & LDHCP_EXCEPT) {
		FD_SET(fd, &ldhcp_exceptset);
	}
	ldhcp_maxfd = (fd >= ldhcp_maxfd) ? (fd + 1) : ldhcp_maxfd;

	return callbacks[fd];
}

int
ldhcp_callback_remove(int fd)
{
	callbacks[fd] = (ldhcp_callback_proc *)0;
	FD_CLR(fd, &ldhcp_readset);
	FD_CLR(fd, &ldhcp_sendset);
	FD_CLR(fd, &ldhcp_exceptset);

	return LDHCP_OK;
}

ldhcp_callback_proc *
ldhcp_callback_get(int fd)
{
	if ((fd < 0) || (fd > FD_SETSIZE)) {
		return (ldhcp_callback_proc *)0;
	}
	return callbacks[fd];
}
