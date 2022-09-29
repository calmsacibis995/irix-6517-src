/*
|| Program to test mq_receive(3)
||		mq_receive [-c <count>] [-n] [-q] <path>
||			-c <count>		number of messages to request, default 1
||			-n					use O_NONBLOCK flag on open
||			-q					quiet, do not display messages
||			<path>			path to message queue, required
|| The program calls mq_receive <count> times or until an error occurs.
*/
#include <mqueue.h>			/* message queue stuff */
#include <unistd.h>			/* for getopt() */
#include <errno.h>			/* errno and perror */
#include <fcntl.h>			/* O_RDONLY */
#include <stdlib.h>			/* calloc(3) */
#include <stdio.h>
int main(int argc, char **argv)
{
	char *path;					/* -> first non-option argument */
	int oflags = O_RDONLY;	/* open flags, O_NONBLOCK may be added */
	int quiet = 0;				/* -q option */
	int count = 1;				/* number of messages to request */
	mqd_t mqd;					/* queue descriptor from mq_open */
	char *msgptr;				/* -> allocated message space */
	unsigned int msg_prio;	/* received message priority */
	int c, ret;
	struct mq_attr obuf;		/* output of mq_getattr(): mq_msgsize */
	while ( -1 != (c = getopt(argc,argv,"c:nq")) )
	{
		switch (c)
		{
		case 'c': /* count */
			count = strtoul(optarg, NULL, 0);
			break;
		case 'q': /* quiet */
			quiet = 1;
			break;
		case 'n': /* nonblock */
			oflags |= O_NONBLOCK;
			break;
		default: /* unknown or missing argument */
			return -1;      
		}
	}
	if (optind < argc)
		path = argv[optind]; /* first non-option argument */
	else
		{ printf("Queue pathname required\n"); return -1; }
	mqd = mq_open(path,oflags);
	if (-1 != mqd)
	{
		if (! (mq_getattr(mqd,&obuf)) ) /* get max message size */
		{
			msgptr = calloc(1,obuf.mq_msgsize);
			for( c=1; c<=count; ++c)
			{
				ret = mq_receive(mqd,msgptr,obuf.mq_msgsize,&msg_prio);
				if (ret >= 0) /* got a message */
				{
					if (!quiet)
					{
						if ( isascii(*msgptr) )
							printf("%d: priority %ld  len %d text %-32.32s\n",
										c, msg_prio,        ret,      msgptr);
						else
							printf("%d: priority %ld  len %d (nonascii)\n",
										c,  msg_prio,        ret);
					}
				}
				else /* an error on receive, stop */
				{
					perror("mq_receive()");
					break;
				}
			} /* for c <= count */
		} /* if getattr */
		else
		{
			perror("mq_getattr()");
			return -1;
		}		
	} /* if open */
	else
		perror("mq_open(O_WRONLY)");
}
