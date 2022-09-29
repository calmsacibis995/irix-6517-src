#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <mdbm.h>
#include <errno.h>

extern int errno;

int
main(int argc, char **argv) 
{
	int i;
	int failure=0;
	char buf[4096];
	MDBM *db;
	
	for (i=1; i < argc ; i++) {
		if ((db = mdbm_open(argv[i], O_RDWR, 0644, 0)) == 0) {
		  /*
		  ** ESTALE 	- file has already been invalidated
		  ** EBADF 	- file has a bad format
		  ** EBADFD	- NOT A MDBM FILE
		  */
			if (errno != ESTALE && errno != EBADF) {
				sprintf(buf, "%s: mdbm_open(%s)",argv[0],argv[i]);
				perror(buf);
				failure++;
				continue;
			}
		}
				
		if (db) {
			if (mdbm_invalidate(db) != 0) {
				sprintf(buf, "%s: mdbm_invalidate(%s)",argv[0],argv[i]);
				perror(buf);
				failure++;
			}
		}		

		if (unlink(argv[i]) != 0) {
			sprintf(buf, "%s: unlink(%s)",argv[0],argv[i]);
			perror(buf);
			failure++;
		}
		
		mdbm_close(db);
	}

	return (failure);
}
