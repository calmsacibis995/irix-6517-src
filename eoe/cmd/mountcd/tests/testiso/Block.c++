/*
 *	Block.c++
 *
 *	Description:
 *		Implementation of Block class
 *
 *	History:
 *      rogerc      04/11/91    Created
 */

#pragma linkage C
#include <stdio.h>
#include <netdb.h>
#include <bstring.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/socket.h>
#pragma linkage

#include "Block.h"
#include "util.h"

#define HOSTNAMELEN 100
#define TIMEOUTINT	10

extern "C" {
    int gethostname(char *name, int namelen);
}

extern char *progname;
int blockSize = CDROM_BLKSIZE;

CLIENT *Block::client = 0;
struct timeval Block::timeout;
dev_t Block::dev;

Block::Block(char *devscsi)
{
    init(devscsi);
}

Block::Block(unsigned long blk, char *devscsi)
{
    init(devscsi);
    int err;
    if ((err = read(blk, 1)) != 0)
	error(err);
}

Block::Block(unsigned long blk, int num, char *devscsi)
{
    init(devscsi);
    int err;
    if ((err = read(blk, num)) != 0)
	error(err);
}

int
Block::read(unsigned long blk, int num)
{
    breadargs	args;
    breadres	res;
    char		*bufp;

    if (numBlocks != num) {
	delete buf;
	numBlocks = num;
	buf = new char[num * CDROM_BLKSIZE];
    }

    args.dev = (short)dev;
    bufp = buf;

    while (num--) {
	args.block = blk++;

	enum clnt_stat rpc_stat = clnt_call(client, TESTCD_BREAD,
					    xdr_breadargs, &args,
					    xdr_breadres, &res, timeout );

	if (rpc_stat != RPC_SUCCESS)
	    return (EIO);

	if (res.status)
	    return (res.status);

	bcopy(res.breadres_u.block, bufp, blockSize);
	bufp += blockSize;
    }

    return (0);
}

void
Block::init(char *devscsi)
{
    buf = 0;
    numBlocks = 0;
    if (!client) {
	char name[HOSTNAMELEN];
	if (gethostname(name, sizeof (name)) < 0)
	    error(errno);

	struct hostent *hp;
	if ((hp = gethostbyname(name)) == NULL) {
	    if ((hp = gethostbyname(name)) == NULL) {
		fprintf(stderr, "%s: %s not in hosts database\n",
			progname, name);
		exit(1);
	    }
	}

	struct sockaddr_in	sin;
	bzero(&sin, sizeof (sin));
	bcopy(hp->h_addr, &sin.sin_addr, hp->h_length);
	sin.sin_family = AF_INET;

	timeout.tv_usec = 0;
	timeout.tv_sec = TIMEOUTINT;

	int s = RPC_ANYSOCK;

	if ((client = clntudp_create(&sin, (u_long)TESTCD_PROGRAM,
				     (u_long)TESTCD_VERSION,
				     timeout, &s )) == NULL) {
	    fprintf(stderr, "%s: testcd server not responding\n",
		    progname);
	    clnt_pcreateerror("");
	    exit (1);
	}
	client->cl_auth = authunix_create_default();

	struct stat f;

	if (stat(devscsi, &f) < 0)
	    error(errno);

	dev = f.st_rdev;
    }
}
