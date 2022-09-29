/*
||	Program to test shmat().
||		shmat {-k <key> | -i <id>} [-a <addr>] [-r] [-w]
||			-k <key>		the key to use to get an ID..
||			-i <id>		..or the ID to use
||			-a <addr>	address to attach, default=0
||			-r				attach read-only, default read/write
||			-w				wait on keyboard input before detaching
*/
#include <unistd.h> /* for getopt() */
#include <sys/shm.h> /* for shmget etc */
#include <errno.h> /* errno and perror */
#include <stdio.h>
int main(int argc, char **argv)
{
	key_t key = -1;	/* key */
	int shmid = -1;	/* ..or ID */
	void *addr = 0;	/* address to request */
	void *attach;		/* address gotten */
	int rwflag = 0;	/* read or r/w */
	int wait = 0;		/* wait before detach */
	int c, ret;
	while ( -1 != (c = getopt(argc,argv,"k:i:a:rw")) )
	{
		switch (c)
		{
		case 'k': /* key */
			key = (key_t) strtoul(optarg, NULL, 0);
			break;
		case 'i': /* id */
			shmid = (int) strtoul(optarg, NULL, 0);
			break;
		case 'a': /* addr */
			addr = (void *) strtoul(optarg, NULL, 0);
			break;
		case 'r': /* read/write */
			rwflag = SHM_RDONLY;
			break;
		case 'w': /* wait */
			wait = 1;
			break;
		default:
			return -1;
		}
	}
	if (-1 == shmid) /* key must be given */
		shmid = shmget(key,0,0);
	if (-1 != shmid) /* we have an ID */
	{
		attach = shmat(shmid,addr,rwflag);
		if (attach != (void*)-1)
		{
			printf("Attached at 0x%lx, first word = 0x%lx\n",
								attach,		 *((pid_t*)attach));
			if (rwflag != SHM_RDONLY)
			{
				*((pid_t *)attach) = getpid();
				printf("Set first word to 0x%lx\n",*((pid_t*)attach));
			}
			if (wait)
			{
				char inp[80];
				printf("Press return to detach...");
				gets(inp);
				printf("First word is now 0x%lx\n",*((pid_t*)attach));
			}
			if (shmdt(attach))
				perror("shmdt()");
		}
		else
			perror("shmat()");
	}
	else
		perror("shmget()");
	return errno;
}
