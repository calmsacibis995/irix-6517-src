/*
|| Program to test msgget(2).
||		msgget [-k <key>] [-p <perms>] [-x] [-c]
||			-k <key>		the key to use, default == 0 == IPC_PRIVATE
||			-p <perms>	permissions to use, default 600
||			-x				use IPC_EXCL
||			-c				use IPC_CREAT
|| <key> and <perms> can be given in any form supported by strtoul(3).
*/
#include <sys/msg.h>		/* msg queue stuff, ipc.h, types.h */
#include <unistd.h>		/* for getopt() */
#include <errno.h>		/* errno and perror */
#include <stdio.h>
int main(int argc, char **argv)
{
	key_t key = IPC_PRIVATE;	/* key */
	int perms = 0600;				/* permissions */
	int msgflg = 0;				/* flags: CREAT + EXCL */
	int msqid;						/* returned msg queue id */
	struct msqid_ds buf;			/* buffer for stat info */
	int c;
	while ( -1 != (c = getopt(argc,argv,"k:p:xc")) )
	{
		switch (c)
		{
		case 'k': /* key */
			key = (key_t) strtoul(optarg, NULL, 0);
			break;
		case 'p': /* permissions */
			perms = (int) strtoul(optarg, NULL, 0);
			break;
		case 'c':
			msgflg |= IPC_CREAT;
			break;
		case 'x':
			msgflg |= IPC_EXCL;
			break;
		default: /* unknown or missing argument */
			return -1;      
		}
	}
	msqid = msgget (key, msgflg|perms);
	if (-1 != msqid)
	{
		printf("msqid = 0x%04x. ",msqid);
		if (-1 != msgctl(msqid,IPC_STAT,&buf))
		{
			printf("owner = %d.%d, perms = %04o, max bytes = %d\n",
						buf.msg_perm.uid,
						buf.msg_perm.gid,
						buf.msg_perm.mode,
						buf.msg_qbytes);				
			printf("%d msgs = %d bytes on queue\n",
						buf.msg_qnum, buf.msg_cbytes);
		}
		else
			perror("\nmsgctl()");
	}
	else
		perror("msgget()");
}
