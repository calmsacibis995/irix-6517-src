#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <alloca.h>
#include <string.h>


#include <pthread.h>

#include "mdbm.h"
#include "sub_hosts.h"

void  walkMdbmFile(const char *fileName);
void mdbmStore(const char *fileName);


main(int argc, char **argv)
{

	pthread_t	th1, th2,th3,th4,th5, th6;

	if(argc != 3) {
		printf("Usage : %s <mdbm_file_name> <host_name>\n", argv[0]);
		exit(-1);
	}
	mdbmStore(argv[1]); 

/*
	pthread_create(&th1,
			NULL,
			(void *)sub_gethostbyname,
			(void *)(argv[2]));
*/


	pthread_create(&th2,
			NULL,
			(void *)walkMdbmFile,
			(void *)(argv[1]));
	pthread_create(&th3,
			NULL,
			(void *)walkMdbmFile,
			(void *)(argv[1]));
	pthread_create(&th4,
			NULL,
			(void *)walkMdbmFile,
			(void *)(argv[1]));
#if 0
	pthread_create(&th5,
			NULL,
			(void *)sub_gethostbyname,
			(void *)(argv[2]));
#endif

	pthread_create(&th6,
			NULL,
			(void *)mdbmStore,
			(void *)(argv[1]));

/*	pthread_join(th1, NULL); */

	pthread_join(th2, NULL);
	pthread_join(th3, NULL);
	pthread_join(th4, NULL);
/*	pthread_join(th5, NULL); */
	pthread_join(th6, NULL); 

}

void
walkMdbmFile(const char *fileName)
{
	MDBM *dbmF = mdbm_open(fileName,
				O_RDONLY,
				0644,
				0);

	if(!dbmF) {
		fprintf(stderr, "READ:Failed to open MDBM file:%s\n", fileName);
		return;
	}
	mdbm_dump(dbmF);
	/*mdbm_invalidate(dbmF); */
	mdbm_close(dbmF);				
}

void
mdbmStore(const char *fileName)
{
	datum key;
	datum val;
	int 	i;
	char 	buf[256], *b1, *b2;
        MDBM *dbmF = mdbm_open(fileName,
                               O_CREAT|O_RDWR,
                               0644,
                               0);


	if(!dbmF) {
                fprintf(stderr, "WRITE:Failed to open MDBM file:%s\n", fileName);
		return;
        }

	b1 = alloca(4096);	
	b2 = alloca(4096);	
	for(i = 0 ; i < 10; i++) {
		sprintf(b1, "KEY-%d", i);
		key.dptr = b1;
		key.dsize = strlen(b1);

		sprintf(b2, "VALUE-%d", i);
	 	val.dptr = b2;
		val.dsize = strlen(b2);	

		if(mdbm_store(dbmF, key, val, MDBM_INSERT)) {
			fprintf(stderr, "Failed to store key-val pair:%s\n",
						strerror(errno)); 
		}
		else {
			printf("WROTE : KEY(%s), Val(%s)\n", key.dptr, val.dptr);
		}
	}

	mdbm_close(dbmF);

}


