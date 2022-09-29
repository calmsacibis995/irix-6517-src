/*
|| Program to test mq_getattr(3), displaying queue information.
||		mq_attr <path>
||		<path>	pathname of the queue, which must exist
*/
#include <mqueue.h>		/* message queue stuff */
#include <errno.h>		/* errno and perror */
#include <fcntl.h>		/* O_RDONLY */
#include <stdio.h>
int main(int argc, char **argv)
{
	mqd_t mqd;				/* queue descriptor */
	struct mq_attr obuf;	/* output attr struct for getattr */
	if (argc < 2)
	{
		printf("A pathname of a message queue is required\n");
		return -1;
	}
	mqd = mq_open(argv[1],O_RDONLY);
	if (-1 != mqd)
	{
		if ( ! mq_getattr(mqd,&obuf) )
		{
			printf("flags: 0x%x  maxmsg: %d  msgsize: %d  curmsgs: %d\n",
			obuf.mq_flags, obuf.mq_maxmsg, obuf.mq_msgsize, obuf.mq_curmsgs);
		}
		else
			perror("mq_getattr()");
	}
	else
		perror("mq_open()");
}
