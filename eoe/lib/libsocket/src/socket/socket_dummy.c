#ifdef _LIBSOCKET_ABI

/*
 * In the MIPS ABI case no-op stubs are here to satisfy the run-time
 * linking case only. This allows MIPS ABI binaries to link but the
 * real socket calls will be resolved by libc which are native system calls.
 *
 * The procedures defined below are those listed in the MIPS ABI Suppliment,
 * aka MIPS ABI Black-Book, in Chapter 6 on Libraries and specifically here
 * the Socket Library.
 */
/* ARGSUSED */
int
accept(int s, void *addr, int *addrlen)
{
}

/* ARGSUSED */
int
bind(int s, const void *vname, int namelen)
{
}

/* ARGSUSED */
int
connect(int s, const void *name, int namelen) 
{
}

/* ARGSUSED */
int
getsockname(int s, void *name, int *namelen)
{
}

/* ARGSUSED */
int
getsockopt(int s, int level, int optname, void *optval, int *optlen)
{
}

/* ARGSUSED */
int
listen(int s, int qlen)
{
}

/* ARGSUSED */
int
recvmsg(int s, struct msghdr *msg, int flags)
{
}

/* ARGSUSED */
int
recvfrom(int s, void *buf, int len, int flags, void *from, int *fromlen)
{
}

/* ARGSUSED */
int
recv(int s, void *buf, int len, int flags)
{
}

/* ARGSUSED */
int
sendmsg(int s, struct msghdr *msg, int flags)
{
}

/* ARGSUSED */
int
sendto(int s, const void *buf, int len, int flags, const void *to, int tolen)
{
}

/* ARGSUSED */
int
send(int s, const void *buf, int len, int flags)
{
}

/* ARGSUSED */
int
setsockopt(int s, int level, int optname, void *optval, int optlen)
{
}

/* ARGSUSED */
int
shutdown(int s, int how)
{
}

/* ARGSUSED */
int
socket(int family, int type, int protocol)
{
}

/* ARGSUSED */
int
socketpair(int family, int type, int protocol, int sv[2])
{
}

#else
/*
 * In the non-MIPS ABI library case no code executes here!
 * All the "real" procedures reside in libc which support native socket
 * system calls. We sinply define a place holder procedure to create
 * a non-empy '.so'.
 */
void
__libsocket_dummy(void)
{
}
#endif /* _LIBSOCKET_ABI */
