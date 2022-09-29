/*
|| semop: program to test semop(2) for all functions.
||		semop {-k <key> | -i <semid>} [-n] [-u] {-p <n> | -v <n> | -z <n>}...
||			-k <key>			the key to use, or..
||			-i <semid>		..the semid to use
||			-n 				use the IPC_NOWAIT flag on following ops
||			-u					use the SEM_UNDO flag on following ops
||			-p <n>			do the P operation (+1) on semaphore <n>
||			-v <n>			do the V operation (-1) on semaphore <n>
||			-z <n>			wait for <n> to become zero
*/
#include <unistd.h>	/* for getopt() */
#include <sys/sem.h>	/* for shmget etc */
#include <errno.h>	/* errno and perror */
#include <stdio.h>
int main(int argc, char **argv)
{
	key_t key;					/* key */
	int semid = -1;			/* object ID */
	int nsops = 0;				/* setsize, and loop variable */
	short flg = 0;				/* flag to use on all ops */
	struct semid_ds ds;
	int c, s;
	struct sembuf sops[25];
	while ( -1 != (c = getopt(argc,argv,"k:i:p:v:z:nu")) )
	{
		switch (c)
		{
		case 'k': /* key */
			key = (key_t) strtoul(optarg, NULL, 0);
			break;
		case 'i': /* semid */
			semid = (int) strtoul(optarg, NULL, 0);
			break;
		case 'n': /* use nowait */
			flg |= IPC_NOWAIT;
			break;
		case 'u': /* use undo */
			flg |= SEM_UNDO;
			break;
		case 'p': /* do the P() */
			sops[nsops].sem_num = (ushort_t) strtoul(optarg, NULL, 0);
			sops[nsops].sem_op = -1;
			sops[nsops++].sem_flg = flg;
			break;
		case 'v': /* do the V() */
			sops[nsops].sem_num = (ushort_t) strtoul(optarg, NULL, 0);
			sops[nsops].sem_op = +1;
			sops[nsops++].sem_flg = flg;
			break;
		case 'z': /* do the wait-for-zero */
			sops[nsops].sem_num = (ushort_t) strtoul(optarg, NULL, 0);
			sops[nsops].sem_op = 0;
			sops[nsops++].sem_flg = flg;
			break;
		default: /* unknown or missing argument */
			return -1;      
		}
	}
	if (-1 == semid) /* -i not given, must have -k */
		semid = semget(key,0,0);
	if (-1 != semid)
	{
		if (0 != semop(semid,sops,nsops) )
			perror("semop()");
	}
	else
		perror("semget()");
}
