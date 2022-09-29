/*
|| Program to test msgrcv(2)
||		msgrcv {-k <key> -i <id>} [-t <type>] [-b <bytes>] [-c <count>]
||															[-n] [-e] [-q]
||			-k <key>		the key to use, or..
||			-i <id>		..the mq id
||			-t <type>	the type of message, default = 0 (any msg)
||			-b <bytes>	the max size to receive, default = 64
||			-c <count>	the number of messages to receive, default = 1
||			-n				use IPC_NOWAIT flag
||			-e				use MSG_NOERROR flag (truncate long msg)
||			-q				quiet, do not display received message
|| The program calls msgrcv <count> times or until an error occurs,
|| each time requesting a message of type <type> and max size <bytes>.
*/
#include <sys/msg.h>		/* msg queue stuff, ipc.h, types.h */
#include <unistd.h>		/* for getopt() */
#include <errno.h>		/* errno and perror */
#include <ctype.h>		/* isascii() */
#include <stdio.h>
int main(int argc, char **argv)
{
	key_t key;				/* key for msgget.. */
	int msqid = -1;		/* ..specified or received msg queue id */
	int msgflg = 0;		/* flag, 0, IPC_NOWAIT, MSG_NOERROR */
	long type = 0;			/* message type */
	size_t bytes = 64;	/* message size limit */
	int count = 1;			/* number to receive */
	int quiet = 0;			/* quiet flag */
	int c;
	struct msgspace { long type; char text[32]; } *msg;
	while ( -1 != (c = getopt(argc,argv,"k:i:t:b:c:enq")) )
	{
		switch (c)
		{
		case 'k': /* key */
			key = (key_t) strtoul(optarg, NULL, 0);
			break;
		case 'i': /* id */
			msqid = (int) strtoul(optarg, NULL, 0);
			break;
		case 't': /* type -- can be negative */
			type = strtol(optarg, NULL, 0);
			break;
		case 'b': /* bytes -- no minimum */
			bytes = strtoul(optarg, NULL, 0);
			break;
		case 'c': /* count - no maximum */
			count = strtoul(optarg, NULL, 0);
			break;
		case 'n': /* nowait */
			msgflg |= IPC_NOWAIT;
			break;
		case 'e': /* noerror -- allow truncation of msgs */
			msgflg |= MSG_NOERROR;
			break;
		case 'q': /* quiet */
			quiet = 1;
			break;
		default: /* unknown or missing argument */
			return -1;      
		}
	}
	if (-1 == msqid) /* no id given, try key */
		msqid = msgget (key, 0);
	msg = (struct msgspace *)calloc(1,sizeof(long)+bytes);    
	if (-1 != msqid)
	{
		for( c=1; c<=count; ++c)
		{
			int ret = msgrcv(msqid,msg,bytes,type,msgflg);
			if (ret >= 0) /* got a message */
			{
				if (!quiet)
				{
					if (isascii(msg->text[0]))
						printf("%d: type %ld  len %d text %-32.32s\n",
									c,   msg->type,  ret,      msg->text);
					else
						printf("%d: type %ld len %d (nonascii)\n",
									c,   msg->type,  ret);
				}
			}
			else /* an error, end loop */
			{
				perror("msgrcv()");
				break;
			}
		} /* for c<=count */
	} /* good msgget */
	else
		perror("msgget()");
}
