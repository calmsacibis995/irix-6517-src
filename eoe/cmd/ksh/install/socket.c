
/*									*/
/*	Copyright (c) 1984,1985,1986,1987,1988,1989,1990   AT&T		*/
/*			All Rights Reserved				*/
/*									*/
/*	  THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T.		*/
/*	    The copyright notice above does not evidence any		*/
/*	   actual or intended publication of such source code.		*/
/*									*/

/*
 * make sure that the BSD socket calls are in the library
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>

main()
{
	register int fd;
#ifndef S_IFSOCK
	exit(1);
#endif /* S_IFSOCK */
	if((fd = socket(AF_INET,SOCK_STREAM, 0)) >= 0)
		{
			printf("#define SOCKET\t1\n");
			close(fd);
			exit(0);
		}
	exit(1);
}
