/*
|| semget: program to test semget(2) for creating semaphores.
||		semget [-k <key>] [-c] [-x] [-p <perms>] [-s <setsize>]
||			-k <key>			the key to use, default == 0 == IPC_PRIVATE
||			-p <perms>		permissions to use, default is 0600
||			-s <setsize>	size to use, default is 1
||			-c 				use IPC_CREAT
||			-x					use IPC_EXCL
*/
#include <unistd.h>	/* for getopt() */
#include <sys/sem.h>	/* for shmget etc */
#include <errno.h>	/* errno and perror */
#include <stdio.h>
int main(int argc, char **argv)
{
	key_t key = IPC_PRIVATE;    /* key */
	int nsems = 1;			/* setsize */
	int perms = 0600;		/* permissions */
	int semflg = 0;		/* flag values */
	struct semid_ds ds;	/* info struct */
	union semun arg4;	/* way to pass &ds properly aligned */
	int c, semid;
	while ( -1 != (c = getopt(argc,argv,"k:p:s:xc")) )
	{
		switch (c)
		{
		case 'k': /* key */
			key = (key_t) strtoul(optarg, NULL, 0);
			break;
		case 's': /* setsize */
			nsems = (int) strtoul(optarg, NULL, 0);
			break;
		case 'p': /* permissions */
			perms = (int) strtoul(optarg, NULL, 0);
			break;
		case 'c':
			semflg |= IPC_CREAT;
			break;
		case 'x':
			semflg |= IPC_EXCL;
			break;
		default: /* unknown or missing argument */
			return -1;      
		}
	}
	semid = semget(key,nsems,semflg+perms);
	if (-1 != semid)
	{
		printf("semid = %d\n",semid);
		arg4.buf = &ds;
		if (-1 != semctl(semid,0,IPC_STAT,arg4))
		{
			printf(
	"owner uid.gid: %d.%d  creator uid.gid: %d.%d  mode: 0%o nsems:%d\n",
			ds.sem_perm.uid,ds.sem_perm.gid,
			ds.sem_perm.cuid,ds.sem_perm.cgid,
			ds.sem_perm.mode, ds.sem_nsems);
		}
		else
			perror("semctl(IPC_STAT)");
	}
	else
		perror("semget()");
	return errno;
} 
