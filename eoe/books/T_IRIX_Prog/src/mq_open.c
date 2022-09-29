/*
|| Program to test mq_open(3).
||		mq_open	[-p <perms>] [-b <bytes>] [-m <msgs>] [-c] [-x] <path>
||		-p <perms>	access mode to use when creating, default 0600
||		-b <bytes>	maximum message size to set, default MQ_DEF_MSGSIZE
||		-m <msgs>	maximum messages on the queue, default MQ_DEF_MAXMSG
||		-f <flags>	flags to use with mq_open, including:
||			c			use O_CREAT
||			x			use O_EXCL
||		<path>		the pathname of the queue, required
|| Numeric arguments can be given in any form supported by strtoul(3).
*/
#include <mqueue.h>		/* message queue stuff */
#define MQ_DEF_MSGSIZE 1024
#define MQ_DEF_MAXMSG 16
#include <unistd.h>		/* for getopt() */
#include <errno.h>		/* errno and perror */
#include <fcntl.h>		/* O_flags */
#include <stdio.h>
int main(int argc, char **argv)
{
	int perms = 0600;				/* permissions */
	int oflags = O_RDWR;			/* flags: O_CREAT + O_EXCL */
	int rd=0, wr=0;				/* -r and -w options */
	mqd_t mqd;						/* returned msg queue descriptor */
	int c;
	char *path;						/* ->first non-option argument */
	struct mq_attr buf;			/* buffer for stat info */
	buf.mq_msgsize = MQ_DEF_MSGSIZE;
	buf.mq_maxmsg = MQ_DEF_MAXMSG;
	while ( -1 != (c = getopt(argc,argv,"p:b:m:cx")) )
	{
		switch (c)
		{
		case 'p': /* permissions */
			perms = (int) strtoul(optarg, NULL, 0);
			break;
		case 'b': /* message size */
			buf.mq_msgsize = (int) strtoul(optarg, NULL, 0);
			break;
		case 'm': /* max messages */
			buf.mq_maxmsg = (int) strtoul(optarg, NULL, 0);
			break;
		case 'c': /* use O_CREAT */
			oflags |= O_CREAT;
			break;
		case 'x': /* use O_EXCL */
			oflags |= O_EXCL;
			break;
		default: /* unknown or missing argument */
			return -1;      
		} /* switch */
	} /* while */
	if (optind < argc)
		path = argv[optind]; /* first non-option argument */
	else
		{ printf("Queue pathname required\n"); return -1; }
	mqd = mq_open(path,oflags,perms,&buf);
	if (-1 != mqd)
	{
		if ( ! mq_getattr(mqd,&buf) )
		{
			printf("flags: 0x%x  maxmsg: %d  msgsize: %d  curmsgs: %d\n",
			buf.mq_flags, buf.mq_maxmsg, buf.mq_msgsize, buf.mq_curmsgs);
		}
		else
			perror("mq_getattr()");
	}
	else
		perror("mq_open()");
}
