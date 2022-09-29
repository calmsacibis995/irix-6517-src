/*
|| Program to test mq_send(3)
||		mq_send [-p <priority>] [-b <bytes>] [-c <count>] [-n] <path>
||			-p <priority>	priority code to use, default 0
||			-b <bytes>		size of the message, default 64, min 32
||			-c <count>		number of messages to send, default 1, max 99999
||			-n					use O_NONBLOCK flag in open
||			<path>			path to queue, required
|| The program sends <count> messages of <bytes> each at <priority>.
|| Each message is an ASCII string containing the time and date and
|| a serial number 1..<count>. The minimum message is 32 bytes.
*/
#include <mqueue.h>			/* message queue stuff */
#include <unistd.h>			/* for getopt() */
#include <errno.h>			/* errno and perror */
#include <time.h>				/* time(2) and ctime_r(3) */
#include <fcntl.h>			/* O_WRONLY */
#include <stdlib.h>			/* calloc(3) */
#include <stdio.h>
int main(int argc, char **argv)
{
	char *path;					/* -> first non-option argument */
	int oflags = O_WRONLY;	/* open flags, O_NONBLOCK may be added */
	mqd_t mqd;					/* queue descriptor from mq_open */
	unsigned int msg_prio = 0;	/* message priority to use */
	size_t msglen = 64;		/* message size */
	int count = 1;				/* number of messages to send */
	char *msgptr;				/* -> allocated message space */
	int c;
	while ( -1 != (c = getopt(argc,argv,"p:b:c:n")) )
	{
		switch (c)
		{
		case 'p': /* priority */
			msg_prio = strtoul(optarg, NULL, 0);
			break;
		case 'b': /* bytes */
			msglen = strtoul(optarg, NULL, 0);
			if (msglen<32) msglen = 32;
			break;
		case 'c': /* count */
			count = strtoul(optarg, NULL, 0);
			if (count > 99999) count = 99999;
			break;
		case 'n': /* use nonblock */
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
	msgptr = calloc(1,msglen);
	mqd = mq_open(path,oflags);
	if (-1 != mqd)
	{
		char stime[26];
		const time_t tm = time(NULL); /* current time value */
		(void)ctime_r (&tm,stime);		/* formatted time string */
		stime[24] = '\0' ; 				/* drop annoying \n */
		for( c=1; c<=count; ++c)
		{
			sprintf(msgptr,"%05d %s",c,stime);
			if ( mq_send(mqd,msgptr,msglen,msg_prio) )
			{
				perror("mq_send()");
				break;
			}
		}
	}
	else
		perror("mq_open(O_WRONLY)");
}
