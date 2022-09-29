/*
|| Program to test msgctl(2).
||		msgctl {-k <key> -i <id>} [-b <bytes>] [-p <perms>] [-u <uid>] [-g <gid>]
||			-k <key>		the key to use, or..
||			-i <id>		..the mq id
||			-b <bytes>	new max number of bytes to set in msg_qbytes
||			-p <perms>	new permissions to assign in msg_perm.mode
||			-u <uid>		new user id (numeric) for msg_perm.uid
||			-g <gid>		new group id (numeric) for msg_perm.gid
|| Numeric argument values can be given in any form alloweded by strtoul(3).
*/
#include <sys/msg.h>		/* msg queue stuff, ipc.h, types.h */
#include <unistd.h>		/* for getopt() */
#include <errno.h>		/* errno and perror */
#include <stdio.h>
int main(int argc, char **argv)
{
	key_t key;				/* key for msgget.. */
	int msqid = -1;		/* ..specified or received msg queue id */
	long perms = -1L;		/* -1L is not valid for any of these */
	long bytes = -1L;
	long uid = -1L;
	long gid = -1L;
	struct msqid_ds buf;
	int c;
	while ( -1 != (c = getopt(argc,argv,"k:i:b:p:u:g:")) )
	{
		switch (c)
		{
		case 'k': /* key */
			key = (key_t) strtoul(optarg, NULL, 0);
			break;
		case 'i': /* id */
			msqid = (int) strtoul(optarg, NULL, 0);
			break;
		case 'p': /* permissions */
			perms = strtoul(optarg, NULL, 0);
			break;
		case 'b': /* bytes */
			bytes = strtoul(optarg, NULL, 0);
			break;
		case 'u': /* uid */
			uid = strtoul(optarg, NULL, 0);
			break;
		case 'g': /* gid */
			gid = strtoul(optarg, NULL, 0);
			break;
		default: /* unknown or missing argument */
			return -1;      
		}
	}
	if (-1 == msqid) /* no id given, try key */
		msqid = msgget (key, 0);
	if (-1 != msqid)
	{
		if (-1 != msgctl(msqid,IPC_STAT,&buf))
		{
			if ((perms!=-1L)||(bytes!=-1L)||(uid!=-1L)||(gid!=-1L))
			{
				/* put new values in buf fields as requested */
				if (perms != -1L) buf.msg_perm.mode = (mode_t)perms;
				if (uid   != -1L) buf.msg_perm.uid  = (uid_t)uid;
				if (gid   != -1L) buf.msg_perm.gid  = (gid_t)gid;
				if (bytes != -1L) buf.msg_qbytes = (ulong_t)bytes;
				if (-1 == msgctl(msqid,IPC_SET,&buf))
					perror("\nmsgctl(IPC_SET)");
			}
			printf("owner = %d.%d, perms = %04o, max bytes = %d\n",
					buf.msg_perm.uid,
					buf.msg_perm.gid,
					buf.msg_perm.mode,
					buf.msg_qbytes);				
			printf("%d msgs = %d bytes on queue\n",
					buf.msg_qnum, buf.msg_cbytes);
		}
		else
			perror("\nmsgctl(IPC_STAT)");
	}
	else
		perror("msgget()");
}
