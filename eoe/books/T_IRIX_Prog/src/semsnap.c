/*
|| semsnap: program to test semctl(2) for semaphore status commands
||		semsnap {-k <key> | -i <semid>}
||			-k <key>			the key to use, or..
||			-i <semid>		..the semid to use
*/
#include <unistd.h>	/* for getopt() */
#include <sys/sem.h>	/* for shmget etc */
#include <errno.h>	/* errno and perror */
#include <stdio.h>
int main(int argc, char **argv)
{
	key_t key;					/* key */
	int semid = -1;			/* object ID */
	int nsems, j;				/* setsize, and loop variable */
	ushort_t semvals[25];	/* snapshot of values */
	ushort_t semns[25];		/* snapshot of P-waiting */
	ushort_t semzs[25];		/* snapshot of zero-waiting */	
	struct semid_ds ds;
	union semun arg4;		/* semctl 4th argument, aligned */
	int c;
	while ( -1 != (c = getopt(argc,argv,"k:i:")) )
	{
		switch (c)
		{
		case 'k': /* key */
			key = (key_t) strtoul(optarg, NULL, 0);
			break;
		case 'i': /* semid */
			semid = (int) strtoul(optarg, NULL, 0);
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
			nsems = ds.sem_nsems;
			arg4.array = semvals;
			semctl(semid,0,GETALL,arg4);
			for (j=0; j<nsems; ++j)
			{
				semns[j] = semctl(semid,j,GETNCNT);
				semzs[j] = semctl(semid,j,GETZCNT);
			}
			printf("vals:");
			for (j=0; j<nsems; ++j) printf(" %2d",semvals[j]);
			printf("\nncnt:");
			for (j=0; j<nsems; ++j) printf(" %2d",semns[j]);
			printf("\nzcnt:");
			for (j=0; j<nsems; ++j) printf(" %2d",semzs[j]);
			putc('\n',stdout);
		}
		else
			perror("semctl(IPC_STAT)");
	}
	else
		perror("semget()");
}
