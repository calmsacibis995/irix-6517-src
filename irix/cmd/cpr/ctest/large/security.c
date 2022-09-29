
#include <sys/types.h>
#include <signal.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#define SIGRESTART 34

void on_ckpt() {;}

main()
{
	pid_t ctest_pid = getppid();
	uid_t uid, euid;
	gid_t gid, egid;

	signal(SIGCKPT, on_ckpt);

	/* setreuid(-1, -1); */

#ifdef NO
	if (setregid(998, 998) == -1) {
		perror("setregid");
		return;
	}
	if (setreuid(998, 998) == -1) {
		perror("setreuid");
		return;
	}
#endif

	uid = getuid();	
	euid = geteuid();	

	gid = getgid();	
	egid = getegid();	


	printf("\tsecurity: Before checkpoint:\n");
	printf("\tsecurity: uid=%d euid=%d gid=%d egid=%d\n", uid, euid, gid, egid);

	pause();

	if (uid == getuid() && euid == geteuid() &&
		gid == getgid() && egid == getegid())
		kill(ctest_pid, SIGUSR1);
	else
		kill(ctest_pid, SIGUSR2);

	uid = getuid();	euid = geteuid();	
	gid = getgid();	egid = getegid();	

	printf("\tsecurity: After restart:\n");
	printf("\tsecurity: uid=%d euid=%d gid=%d egid=%d\n", uid, euid, gid, egid);
	

	printf("\tsecurity: Done\n");
}
