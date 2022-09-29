#ifndef _server_
#define _server_
#include "sys/types.h"
#include <osfcn.h>

#ifndef MSGBUFSIZ
#define MSGBUFSIZ 3000
#endif 

#ifndef MAXMSGSIZ
#define MAXMSGSIZ 300
#endif

#define SERVER_REGISTER 1

void getword(char *p,long *l);
void putword(char *p,long *l);
void FD_COPY(fd_set *a, fd_set *b, int nfds);
void FD_OR (fd_set *a, fd_set *b, fd_set *c);
void FD_AND(fd_set *a, fd_set *b, fd_set *c);
int FD_ISZERO (fd_set *a);
void FD_COMP(fd_set *a);
int FD_COUNT (fd_set *a);
extern "C" {
int socket(int,int,int);
struct hostent *gethostbyname(void *);
void FD_NAND(fd_set *a, fd_set *b, fd_set *c);
void FD_NOR(fd_set *a,fd_set *b,fd_set *c);
void FD_EQU(fd_set *a,fd_set *b,fd_set *c);
void FD_XOR (fd_set *a, fd_set *b, fd_set *c);
void FD_PRINT(void *fp,char *legend,fd_set *a);
int listen(int,int);
//int gettimeofday(struct timeval *, struct timezone *);
};

typedef struct ClientEntry {
	int	sock;
	struct ClientEntry *next;
	char	*alist; // should be a void*, but cc doesn't handle them right.
	int	inbuf_has_msg;
	int	inbuf_full;
	char	*instart, *inend, inbuf[MSGBUFSIZ];
	int	outbuf_has_room;
	int	outbuf_has_msg;
	char	*outstart, *outend, outbuf[MSGBUFSIZ];
	int	connection_closed;
} ClientEntry;

class Server {
    protected:
	int DoServerIn(ClientEntry *client);
	int DoServerOut(ClientEntry *client);
    public:
	ClientEntry *Clist;
	Server();
	void AddClient(int sock);
	void CloseClient(int sock);
	void DeleteClient(int sock);
	int ReadFromClient(int sock,char *msg,int  nbytes);
	int WriteToClient(int sock,char *buf,int nbytes);
	int ServerSelect(int nfds, fd_set *r, fd_set *w, fd_set *e,
			 struct timeval *t);
};
#endif
