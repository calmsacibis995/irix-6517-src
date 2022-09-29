/*
|| Program to test msgsnd(2)
||		msgsnd {-k <key> -i <id>} [-t <type>] [-b <bytes>] [-c <count>] [-n]
||			-k <key>		the key to use, or..
||			-i <id>		..the mq id
||			-t <type>	the type of each message, default = 1
||			-b <bytes>	the size of each message, default = 64, min 32
||			-c <count>	the number of messages to send, default = 1, max 99999
||			-n				use IPC_NOWAIT flag
|| The program sends <count> messages of <type>, <bytes> each on the queue.
|| Each message is an ASCII string containing the time and date, and
|| a serial number 1..<count>, minimum message is 32 bytes.
*/
#include <sys/msg.h>		/* msg queue stuff, ipc.h, types.h */
#include <unistd.h>		/* for getopt() */
#include <errno.h>		/* errno and perror */
#include <time.h>			/* time(2) and ctime_r(3) */
#include <stdio.h>
int main(int argc, char **argv)
{
	key_t key;				/* key for msgget.. */
	int msqid = -1;		/* ..specified or received msg queue id */
	int msgflg = 0;		/* flag, 0 or IPC_NOWAIT */
	long type = 1;			/* message type -- 0 is not valid to msgsnd() */
	size_t bytes = 64;	/* message text size */
	int count = 1;			/* number to send */
	int c;
	struct msgspace { long type; char text[32]; } *msg;
	while ( -1 != (c = getopt(argc,argv,"k:i:t:b:c:n")) )
	{
		switch (c)
		{
		case 'k': /* key */
			key = (key_t) strtoul(optarg, NULL, 0);
			break;
		case 'i': /* id */
			msqid = (int) strtoul(optarg, NULL, 0);
			break;
		case 't': /* type */
			type = strtoul(optarg, NULL, 0);
			break;
		case 'b': /* bytes */
			bytes = strtoul(optarg, NULL, 0);
			if (bytes<32) bytes = 32;
			break;
		case 'c': /* count */
			count = strtoul(optarg, NULL, 0);
			if (count > 99999) count = 99999;
			break;
		case 'n': /* nowait */
			msgflg |= IPC_NOWAIT;
			break;
		default: /* unknown or missing argument */
			return -1;      
		}
	}
	msg = (struct msgspace *)calloc(1,sizeof(long)+bytes);    
	if (-1 == msqid) /* no id given, try key */
		msqid = msgget (key, 0);
	if (-1 != msqid)
	{
		const time_t tm = time(NULL);
		char stime[26];
		(void)ctime_r (&tm,stime); /* format timestamp for msg */
		stime[24] = '\0'; 			/* drop annoying \n */
		for( c=1; c<=count; ++c)
		{
			msg->type = type;
			sprintf(msg->text,"%05d %s",c,stime);
			if (-1 == msgsnd(msqid,msg,bytes,msgflg))
			{
				perror("msgsnd()");
				break;
			}
		}
	}
	else
		perror("msgget()");
}
