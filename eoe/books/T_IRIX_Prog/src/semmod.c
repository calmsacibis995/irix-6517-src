/*
|| semmod: program to test semctl(2) for status, ownership and permissions.
||		semmod {-k <key> | -i <semid>} [-p <perms>] [-u <user>] [-g <group>]
||			-k <key>			the key to use, or..
||			-i <semid>		..the semid to use
||			-p <perms>		permissions to set with IPC_SET
||			-u <uid>			uid to set as owner with IPC_SET
||			-g <gid>			gid to set as owner with IPC_SET
*/
#include <unistd.h> /* for getopt() */
#include <sys/sem.h> /* for shmget etc */
#include <errno.h> /* errno and perror */
#include <stdio.h>
int main(int argc, char **argv)
{
	key_t key;					/* key */
	int semid = -1;			/* object ID */
	int perms, popt = 0;		/* perms to set, if given */
	int uid, uopt = 0;		/* uid to set, if given */
	int gid, gopt = 0;		/* gid to set, if given */
	int val, vopt = 0;		/* setall value if given */
	struct semid_ds ds;
	union semun arg4;		/* 4th semctl argument, aligned */
	int c;
	while ( -1 != (c = getopt(argc,argv,"k:i:p:u:g:")) )
	{
		switch (c)
		{
		case 'k': /* key */
			key = (key_t) strtoul(optarg, NULL, 0);
			break;
		case 'i': /* semid */
			semid = (int) strtoul(optarg, NULL, 0);
			break;
		case 'p': /* permissions */
			perms = (int) strtoul(optarg, NULL, 0);
			popt = 1;
			break;
		case 'u': /* uid */
			uid = (int) strtoul(optarg, NULL, 0);
			uopt = 1;
			break;
		case 'g': /* gid */
			gid = (int) strtoul(optarg, NULL, 0);
			gopt = 1;
			break;
		default: /* unknown or missing argument */
			return -1;      
		}
	}
	if (-1 == semid) /* -i not given, must have -k */
		semid = semget(key,0,0);
	if (-1 != semid)
	{
		arg4.buf = &ds;
		if (0 == semctl(semid,0,IPC_STAT,arg4))
		{
			if ((popt)||(uopt)||(gopt))
			{
				if (popt) ds.sem_perm.mode = perms;
				if (uopt) ds.sem_perm.uid = uid;
				if (gopt) ds.sem_perm.gid = gid;
				if (0 == semctl(semid,0,IPC_SET,arg4) )
					semctl(semid,0,IPC_STAT,arg4); /* refresh info */
				else
					perror("semctl(IPC_SET)");
			}
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
}
