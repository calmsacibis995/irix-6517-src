#ifdef _LIBSOCKET_ABI
#include <sys/types.h>
#include <netdb.h>
#include <netinet/in.h>

#undef htonl
#undef htons
#undef ntohl
#undef ntohs

/*
 * In the MIPS ABI case no-op stubs are here to satisfy the run-time
 * linking case only. This allows MIPS ABI binaries to link but the
 * real socket calls will be resolved by libc which are native system calls.
 */


/* ARGSUSED */
unsigned long
htonl(long hl)
{
}

/* ARGSUSED */
unsigned short
htons(short hs)
{
}

/* ARGSUSED */
long
ntohl(unsigned long nl)
{
}

/* ARGSUSED */
short
ntohs(unsigned short ns)
{
}

/* ARGSUSED */
void
sethostent(int f)
{
}

void
endhostent(void)
{
}

struct hostent *
gethostent(void)
{
}

/* ARGSUSED */
struct hostent *
gethostbyname(const char *name)
{
}

/* ARGSUSED */
struct hostent *
gethostbyaddr(const void *addr, int len, int type)
{
}

/* ARGSUSED */
struct netent *
getnetbyaddr(in_addr_t anet, int type)
{
}

/* ARGSUSED */
struct netent *
getnetbyname(const char *name)
{
}

/* ARGSUSED */
int
getpeername(int s, void *name, int *namelen)
{
}

struct netent *
getnetent(void)
{
}

/* ARGSUSED */
void
setnetent(int f)
{
}

void
endnetent(void)
{
}


/* ARGSUSED */
struct protoent *
getprotobynumber(int proto)
{
}

/* ARGSUSED */
struct protoent *
getprotobyname(const char *name)
{
}

struct servent *
getservent(void)
{
}

/* ARGSUSED */
void
setservent(int f)
{
}

void
endservent(void)
{
}

/* ARGSUSED */
struct protoent *
getprotoent(void)
{
}

/* ARGSUSED */
void
setprotoent(int f)
{
}

void
endprotoent(void)
{
}

/* ARGSUSED */
struct servent *
getservbyport(int svc_port, const char *proto)
{
}

/* ARGSUSED */
struct servent *
getservbyname(const char *name, const char *proto)
{
}

/*
 * internet specific procedures
 */
/* ARGSUSED */
unsigned long
inet_addr(char *cp)
{
}

/* ARGSUSED */
unsigned long
inet_lnaof(struct in_addr in)
{
}

/* ARGSUSED */
struct in_addr
inet_makeaddr(int net, int host)
{
}

/* ARGSUSED */
unsigned long
inet_netof(struct in_addr in)
{
}

/* ARGSUSED */
unsigned long
inet_network(char *cp)
{
}

/* ARGSUSED */
char *
inet_ntoa(struct in_addr in)
{
}

/* ARGSUSED */
int
gethostname(char *name, int len)
{
	return 0;
}

#else
/*
 * In the non-MIPS ABI library case no code resides or executes here!
 * All the "real" procedures reside in libc which support these interfaces
 */
void
__libsocketinet_dummy(void)
{
}
#endif
