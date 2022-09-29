#ifndef _client_
#define _client_
#include "sys/types.h"
#include <osfcn.h>

#ifndef MSGBUFSIZ
#define MSGBUFSIZ 3000
#endif

#ifndef MAXMSGSIZ
#define MAXMSGSIZ 300
#endif

#define SERVER_REGISTER 1

void getword(char *, __uint32_t *);
void putword(char *, __uint32_t *);
void FD_COPY(fd_set *a, fd_set *b, int nfds);
void FD_OR (fd_set *a, fd_set *b, fd_set *c);
void FD_AND(fd_set *a, fd_set *b, fd_set *c);
int FD_ISZERO (fd_set *a);
void FD_COMP(fd_set *a);
int FD_COUNT (fd_set *a);
extern "C" {
int socket(int,int,int);
void FD_NAND(fd_set *a, fd_set *b, fd_set *c);
void FD_NOR(fd_set *a,fd_set *b,fd_set *c);
void FD_EQU(fd_set *a,fd_set *b,fd_set *c);
void FD_XOR (fd_set *a, fd_set *b, fd_set *c);
void FD_PRINT(void *fp,char *legend,fd_set *a);
int listen(int,int);
//int gettimeofday(struct timeval *, struct timezone *);
};

typedef struct ServerEntry {
	int sock;
	struct ServerEntry *next;
	char *alist; // should be a void*, but cc doesn't handle them right.
	int inbuf_has_msg;
	int inbuf_full;
	char *instart,*inend,inbuf[MSGBUFSIZ];
	int outbuf_has_room;
	int outbuf_has_msg;
	char *outstart,*outend,outbuf[MSGBUFSIZ];
	int connection_closed;
}ServerEntry;

class Client {
    public:
	int DoClientIn(ServerEntry *server);
	int DoClientOut(ServerEntry *server);
	ServerEntry *Slist;
	Client();
	~Client();
	int AddServer(long hostaddr,int prog,int vers);
	void CloseServer(int sock);
	int ClientSelect(int nfds, fd_set *r, fd_set *w, fd_set *e,
		         struct timeval *t);
	int ReadFromServer(int sock,char *msg,int nbytes);
	int WriteToServer(int sock,char *msg,int nbytes);
};
#endif
