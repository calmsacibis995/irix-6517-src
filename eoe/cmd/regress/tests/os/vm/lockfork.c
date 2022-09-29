#include <sys/lock.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>


main()
{
	int shmid;

	shmid = shmget(IPC_PRIVATE, 4096, 0);
	shmat(shmid, (void *)0, 0);
	shmctl(shmid, SHM_LOCK, 0);

	plock(PROCLOCK);

	system("date >/dev/null");
	exit(0);
}
