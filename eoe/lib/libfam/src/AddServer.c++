#include "Client.h"
#include "libc.h"
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>

// Some preprocessor magic since c++ is braindead.
#define gethostbyname ddfafdasfasdfdasfadsf
#include <netdb.h>
#undef gethostbyname
#include "sys/time.h"
#include <stdio.h>
extern "C" { // CCC
unsigned short pmap_getport(struct sockaddr_in *,int,int,int);
close(int);
extern int connect(int, const void *, int);
};

int Client::AddServer(long host,int prog,int vers)
{
	int	sock,stat;
	struct sockaddr_in sin;
	ServerEntry *p, *q;

	// Make an internet address.

	bzero(&sin, sizeof sin);
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = htonl(host);
	sin.sin_port = pmap_getport(&sin,prog,vers,IPPROTO_TCP);
	if(sin.sin_port == 0)return(-1);
	sock=socket(PF_INET, SOCK_STREAM, 0);
        if(sock<0)return(-2);
	stat=connect(sock,&sin,sizeof(sin));
	if(stat < 0) {
		close(sock);
		return(-3);
	}

	// Make a Server struct.

	p = new ServerEntry;

	q = Slist;
	Slist = p;
	p->next = q;
	p->sock = sock;
	p->instart = p->inbuf;
	p->inend = p->inbuf;
	p->inbuf_has_msg = 0;
	p->inbuf_full = 0;
	p->outstart = p->outbuf;
	p->outend = p->outbuf;
	p->outbuf_has_msg = 0;
	p->outbuf_has_room = 1;
	p->alist = 0;
	p->connection_closed = 0;

	return(sock);
}
