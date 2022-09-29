#include <stdio.h>
#include <ns_api.h>
#include <ns_daemon.h>

static nsd_callback_proc *callbacks[FD_SETSIZE];
extern fd_set nsd_readset, nsd_exceptset, nsd_sendset;
extern int nsd_maxfd;

nsd_callback_proc *
nsd_callback_new(int fd, nsd_callback_proc *proc, unsigned flags)
{
	callbacks[fd] = proc;

	/*
	** Add this socket to the central select.
	*/
	if (flags & NSD_READ) {
		FD_SET(fd, &nsd_readset);
	}
	if (flags & NSD_WRITE) {
		FD_SET(fd, &nsd_sendset);
	}
	if (flags & NSD_EXCEPT) {
		FD_SET(fd, &nsd_exceptset);
	}
	nsd_maxfd = (fd >= nsd_maxfd) ? (fd + 1) : nsd_maxfd;

	return callbacks[fd];
}

int
nsd_callback_remove(int fd)
{
	callbacks[fd] = (nsd_callback_proc *)0;
	FD_CLR(fd, &nsd_readset);
	FD_CLR(fd, &nsd_sendset);
	FD_CLR(fd, &nsd_exceptset);

	return NSD_OK;
}

nsd_callback_proc *
nsd_callback_get(int fd)
{
	if ((fd < 0) || (fd > FD_SETSIZE)) {
		return (nsd_callback_proc *)0;
	}
	return callbacks[fd];
}
