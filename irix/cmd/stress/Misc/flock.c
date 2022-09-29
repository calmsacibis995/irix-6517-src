#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/file.h>
#include <errno.h>

/* 
 * test BSD record locking.
 */

main()
{
        int flock_fd;
	char *td;		/* temp dir */

	td = tempnam(NULL, "flock");
        if((flock_fd = open(td, O_RDONLY|O_CREAT)) < 0) {
                printf("Error encountered attempting to open %s for lock\n",
                   td);
                exit(1);
        }

        if(flock(flock_fd, LOCK_NB | LOCK_EX) == -1) {
                if(oserror()) {
                        printf("Error encountered attempting to get exclusive lock on %s\n",
                            td);
			close(flock_fd);
			unlink(td);
                        exit(1);
                }
        }
        if(flock(flock_fd, LOCK_UN) == -1) {
                if(oserror()) {
                        printf("Error encountered attempting to unlock %s\n",
                            td);
			close(flock_fd);
			unlink(td);
                        exit(1);
                }
        }
	close(flock_fd);
	unlink(td);
	td = tempnam(NULL, "flock");
        if((flock_fd = open(td, O_WRONLY|O_CREAT)) < 0) {
                perror("fail in opening file");
                exit(1);
        }
        if(flock(flock_fd, LOCK_SH) == -1) {
                if(oserror()) {
                        printf("Error encountered attempting to get share lock on  %s\n",
                            td);
			close(flock_fd);
			unlink(td);
                        exit(1);
                }
        }
	close(flock_fd);
	unlink(td);
        return 0;
}
