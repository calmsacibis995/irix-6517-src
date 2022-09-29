/*
||	Program to test shmget(2) for creating a segment.
||	shmget [-k <key>] [-s <size>] [-p <perms>] [-c] [-x]
||		-k <key>			the key to use, default == 0 == IPC_PRIVATE
||		-s <size>		size of segment, default is 64KB
||		-p <perms>		permissions to use, default is 0600
||		-x					use IPC_EXCL
||		-c					use IPC_CREAT
*/
#include <unistd.h>	/* for getopt() */
#include <sys/shm.h>	/* for shmget etc */
#include <errno.h>	/* errno and perror */
#include <stdio.h>
int main(int argc, char **argv)
{
	key_t key = IPC_PRIVATE;	/* key */
	size_t size = 65536;			/* size */
	int perms = 0600;				/* permissions */
	int shmflg = 0;				/* flag values */
	struct shmid_ds ds;			/* info struct */
	int c, shmid;
	while ( -1 != (c = getopt(argc,argv,"k:s:p:cx")) )
	{
		switch (c)
		{
		case 'k': /* key */
			key = (key_t) strtoul(optarg, NULL, 0);
			break;
		case 's': /* size */
			size = (size_t) strtoul(optarg, NULL, 0);
			break;
		case 'p': /* permissions */
			perms = (int) strtoul(optarg, NULL, 0);
			break;
		case 'c':
			shmflg |= IPC_CREAT;
			break;
		case 'x':
			shmflg |= IPC_EXCL;
			break;
		default: /* unknown or missing argument */
			return -1;      
		}
	}
	shmid = shmget(key,size,shmflg|perms);
	if (-1 != shmid)
	{
		printf("shmid = %d (0x%x)\n",shmid,shmid);
		if (-1 != shmctl(shmid,IPC_STAT,&ds))
		{
			printf("owner uid/gid: %d/%d\n",
							ds.shm_perm.uid,ds.shm_perm.gid);
			printf("creator uid/gid: %d/%d\n",
							ds.shm_perm.cuid,ds.shm_perm.cgid);
		}
		else
			perror("shmctl(IPC_STAT)");
	}
	else
		perror("shmget");
	return errno;
} 
