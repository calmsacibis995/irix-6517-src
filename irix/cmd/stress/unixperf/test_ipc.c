
/* unixperf - an x11perf-style Unix performance benchmark */

/* test_ipc.c measures inter-process communication speeds. */

/* Tells SGI headers to use Berkeley signal semantics. */
#define _BSD_SIGNALS

#include "unixperf.h"
#include "test_ipc.h"

#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <bstring.h>

static int parentToChild[2], childToParent[2];
static char *buffer;
static unsigned int stashedSize;

#define READ_END 0
#define WRITE_END 1

static int sockfd;
static struct sockaddr_in addr;
static int server_port;

#ifdef _IRIX4
static
#else
static void
#endif
reapChild(void)
{
#ifdef _IRIX4
    union wait waitstat;
#else
    int waitstat;
#endif
    int pid;

    do {
       pid = wait3(&waitstat, WNOHANG, NULL);
    } while(pid >= 0);
}

static void
SpawningServerTCP(void)
{
    signal(SIGCHLD, reapChild);
    while (1) {
	struct sockaddr_in cli_addr;
	int             clilen = sizeof(cli_addr);
	int             newsockfd;

	do {
	    newsockfd = accept(sockfd, (struct sockaddr *) & cli_addr, &clilen);
	} while ((newsockfd == -1) && (errno == EINTR));
	SYSERROR(newsockfd, "accept");
	pid = fork();
	SYSERROR(pid, "fork");
	if (pid == 0) {
	    char            line[256];
	    int             n;

	    while (1) {
		n = read(newsockfd, line, sizeof(line));
		if (n <= 0) {
		    _exit(0);
		}
		write(newsockfd, line, n);
	    }
	} else {
	    close(newsockfd);
	}
    }
}

static void
StandaloneServerTCP(void)
{
    while (1) {
	struct sockaddr_in cli_addr;
	int             clilen = sizeof(cli_addr);
	int             newsockfd;
	char            line[256];
	int             n;

	do {
	    newsockfd = accept(sockfd, (struct sockaddr *) & cli_addr, &clilen);
	} while ((newsockfd == -1) && (errno == EINTR));
	SYSERROR(newsockfd, "accept");
	while (1) {
	    n = read(newsockfd, line, sizeof(line));
	    if (n <= 0) break;
	    write(newsockfd, line, n);
	}
	close(newsockfd);
    }
}

static Bool
InitServerTCP(Version version, Bool spawn)
{
    int i;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    SYSERROR(rc, "socket");
    bzero((char*) &addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    for(i=0;i<100;i++) {
        addr.sin_port = 6200 + i;
	rc = bind(sockfd, (struct sockaddr*) &addr, sizeof(addr));
	if((rc == -1) && (errno == EADDRINUSE)) continue;
	break;
    }
    SYSERROR(rc, "bind");
    server_port = addr.sin_port;
    rc = listen(sockfd, 5);
    SYSERROR(rc, "listen");
    pid = fork();
    SYSERROR(pid, "fork");
    if(pid == 0) {
       if(spawn) {
	  SpawningServerTCP();
       } else {
	  StandaloneServerTCP();
       }
    } else {
       rc = close(sockfd);
       SYSERROR(rc, "close");
    }
    return TRUE;
}

Bool
InitSpawningServerTCP(Version version)
{
    return InitServerTCP(version, TRUE);
}

Bool
InitStandaloneServerTCP(Version version)
{
    return InitServerTCP(version, FALSE);
}

unsigned int
DoEchoClientTCP(void)
{
   char c;
   int i;

   for(i=0;i<50;i++) {
       bzero((char*) &addr, sizeof(addr));
       addr.sin_family = AF_INET;
       addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
       addr.sin_port = server_port;
       sockfd = socket(AF_INET, SOCK_STREAM, 0);
       debugSYSERROR(sockfd, "socket");
       rc = connect(sockfd, (struct sockaddr*) &addr, sizeof(addr));
       debugSYSERROR(rc, "connect");
       write(sockfd, &c, 1);
       read(sockfd, &c, 1);
       close(sockfd);
   }
   return 50;
}

void
CleanupServerTCP(void)
{
    int waitstat;

    sleep(1); /* wait for children to be reaped */
    rc = kill(pid, SIGKILL);
    SYSERROR(rc, "kill");
    rc = wait(&waitstat);
    SYSERROR(rc, "wait");
}

static Bool
InitPipe(Version version)
{
    MEMERROR(buffer);
    rc = pipe(parentToChild);
    SYSERROR(rc, "pipe");
    rc = pipe(childToParent);
    SYSERROR(rc, "pipe");
    pid = fork();
    SYSERROR(pid, "fork");
    if(pid == 0) {
	/* child */

	close(parentToChild[WRITE_END]);
	SYSERROR(rc, "close");
	close(childToParent[READ_END]);
	SYSERROR(rc, "close");
	while(1) {
	    rc = read(parentToChild[READ_END], buffer, stashedSize);
	    debugSYSERROR(rc, "read");
	    rc = write(childToParent[WRITE_END], buffer, stashedSize);
	    debugSYSERROR(rc, "write");
	}
    } else {
	/* parent */
	rc = close(parentToChild[READ_END]);
	SYSERROR(rc, "close");
	close(childToParent[WRITE_END]);
	SYSERROR(rc, "close");
    }
    return TRUE;
}

Bool
InitPipePing1(Version version)
{
    stashedSize = 1;
    buffer = (char*) malloc(stashedSize);
    return InitPipe(version);
}

Bool
InitPipePing1K(Version version)
{
    stashedSize = 1024;
    buffer = (char*) malloc(stashedSize);
    return InitPipe(version);
}

unsigned int
DoPipePing(void)
{
    int i;

    for(i=400;i>0;i--) {
	rc = write(parentToChild[WRITE_END], buffer, stashedSize);
	debugSYSERROR(rc, "write");
	rc = read(childToParent[READ_END], buffer, stashedSize);
	debugSYSERROR(rc, "read");
    }
    return 400;
}

void
CleanupPipePing(void)
{
    int waitstat;

    close(parentToChild[0]);
    close(childToParent[1]);
    rc = kill(pid, SIGKILL);
    SYSERROR(rc, "kill");
    rc = wait(&waitstat);
    SYSERROR(rc, "wait");
    free(buffer);
}

