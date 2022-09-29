#include <stdio.h>
#include "Client.h"
#include "libc.h"
#include "unistd.h"

int Client::WriteToServer(int sock,char *buf,int nbytes)
{
	char msgHeader[sizeof(long)];
	int len;
	__uint32_t u_nbytes = nbytes;
	putword(msgHeader, &u_nbytes);
	len = write(sock, msgHeader, sizeof(long));
	if (len == sizeof(long)) {
	    len = write(sock, buf, nbytes);
	} else {
	    return(-1);
	}

	return(len);
}

