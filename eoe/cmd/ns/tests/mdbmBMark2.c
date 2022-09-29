#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <alloca.h>
#include <string.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <signal.h>



#include <pthread.h>
#include <unistd.h>
#include <sys/procset.h>

#include "mdbm.h"
#include "sub_hosts.h"

void  mdbmReader(void *arg);
void mdbmStore(void *arg);
void myGethostbyname(void *argv); 


int alarmed=0;
int iamParent = 1;
int	readerCnt = 0;

static char mdbmName[256];

typedef struct _threadVal_t {
	long count;
	int  pagesize;
	int  hash_func_no;
	int  write_per_second;
} threadVal_t;


void alarm_handler(int reason) {
	alarmed=1;
	if(iamParent) {
		sigsend(P_PGID, getpgrp(), SIGUSR1);
	}
}

void mysig_handler(int reason) {
	alarmed = 1;
}

main(int argc, char **argv)
{

	pthread_t	th1, th2,th3,th4,th5, th6;
	int			no_of_readers=1,
				no_of_writers=0,
				write_per_second=0,
				pagesize=0,
				time_to_run=10;
	int			errFlg=0, fileFlg=0;
	int 		c;
	threadVal_t	*threadValArray, *writethreadValArray;
	pthread_t	*threadArray, *writethreadArray;
	int			i, totalFetches, totalWrites;
	int		hash_func_no=5;
	threadVal_t arg;
	int		status;
	kvpair 		kv;
	char 		*b1, *b2;
	MDBM		*dbmF;

	b1 = alloca(4096);
	b2 = alloca(4096);

	while ((c = getopt(argc, argv, "r:w:p:t:f:c:h:")) != -1) {
		switch(c) {
		 	case 'r':
				no_of_readers = atoi(optarg);
			break;	
		 	case 'w':
				no_of_writers = atoi(optarg);
			break;	
		 	case 'p':
				pagesize = atoi(optarg);
			break;	
		 	case 't':
				time_to_run = atoi(optarg);
			break;	
		 	case 'f':
				strcpy(mdbmName, optarg);
				fileFlg++;
			break;	
		 	case 'c':
				write_per_second = atoi(optarg);
			break;	
		 	case 'h':
				hash_func_no = atoi(optarg);
			break;	
			default:	
				errFlg++;
			break;
		}
	}


	if(!fileFlg || errFlg) {
		fprintf(stderr, "Usage: %s [-r <no_of_reades>] [-w <no_of_writers>] [-p <pagesize>] [-t <time_to_run>] [-c <write_per_second>] -f dbmfilename\n", argv[0]);
		exit(-1);
	}

#if 1

	arg.pagesize = pagesize;
	arg.hash_func_no = hash_func_no;
	mdbmStore(&arg); 
#endif

	for(i = 0; i < no_of_readers; i++) {
		if(!fork()) {
			signal(SIGUSR1, mysig_handler);
			iamParent = 0;
			mdbmReader(mdbmName);
			exit(0);
		}
		readerCnt++;
	}
	for(i = 0; i < no_of_writers;i++) {
		if(!fork()) {
			iamParent = 0;
			mdbmStore(mdbmName);
		}	
	}

	signal(SIGUSR1, SIG_IGN);
	signal(SIGALRM, alarm_handler);
	alarm(time_to_run);

	while(!alarmed);

	for(i = 0; i < no_of_readers+ no_of_writers; i++) {
		wait(&status);
	}


	sleep(2);

        dbmF = mdbm_open(mdbmName,
                        O_RDWR,
                        0644,
                        0);

	if(!dbmF) {
		fprintf(stderr, "failed to open mdbm:[%s]\n", strerror(oserror()));
		exit(-1);
	}
	
	totalFetches = 0;
	for(i = 0; i < no_of_readers;i++) {
                sprintf(b1, "READER-%d", i);
                kv.key.dptr = b1;
                kv.key.dsize = strlen(b1);
                kv.val.dptr = b2;
                kv.val.dsize = 4096;

		if(i == 0) {
                	kv.val = mdbm_fetch(dbmF, kv);
		}
		else {
			kv.val = mdbm_fetch(dbmF, kv);
		}
                if(kv.val.dsize==0 ) {
                        fprintf(stderr, "READER %d -> Failed to fetch the key:[%s], reason:(%d) %s\n", getpid(), kv.key.dptr,oserror(), strerror(oserror()));
                }
		else {
			printf("Got value:[%s] for Key:[%s]\n", kv.val.dptr, kv.key.dptr);
			totalFetches += atoi(kv.val.dptr);
			/* Delete the key-val */
			if(mdbm_delete(dbmF, kv.key)) {
				fprintf(stderr, "READER -> failed to delete the key:[%s], error:[%s]\n", kv.key.dptr, strerror(oserror()));
			}
		}
	}

	mdbm_close(dbmF);

	printf("Summary: \n\
		Hash function no.:%d\n\
		No. of readers:%d\n\
		No. of writers:%d\n\
		Pagesize.:%d\n\
		Time to run:%d\n\
		Total fetches/sec:%d\n",
		hash_func_no, no_of_readers, no_of_writers,
			pagesize, time_to_run, totalFetches/time_to_run
			);
}

void
mdbmReader(void * arg)
{
	char *b1, *b2;
	kvpair 		kv;
	int 		pagesize;
	MDBM *dbmF;
	int		count;


		b1 = alloca(4096);
		b2 = alloca(4096);
	count = 0;
	dbmF = mdbm_open(mdbmName,
			O_RDONLY,
			0644,
			0);

	if(!dbmF) {
		fprintf(stderr, "READER %d ->Failed to open MDBM file:%s\n", getpid(), mdbmName);
		return;
	}
	while(!alarmed) {
		pagesize = MDBM_PAGE_SIZE(dbmF);
		strcpy(b1, "KEY-1");
		kv.key.dptr = b1;
		kv.key.dsize = strlen(b1);
		kv.val.dptr = b2;
		kv.val.dsize = pagesize;

		kv.val = mdbm_fetch(dbmF, kv);
		if(kv.val.dsize==0 ) {
			fprintf(stderr, "READER %d -> Failed to fetch the key:[%s], reason:(%d) %s\n", getpid(), kv.key.dptr,oserror(), strerror(oserror()));
		}
		count++;
	}
	mdbm_close(dbmF);				

	/* Write the statistics */
        dbmF = mdbm_open(mdbmName,
                               O_RDWR,
                               0644,
                               0);
	if(dbmF) {
		sprintf(b1, "READER-%d", readerCnt);
		kv.key.dptr = b1;
		kv.key.dsize = strlen(b1);
		sprintf(b2, "%d", count);
		kv.val.dsize = strlen(b2);
                mdbm_store(dbmF, kv.key, kv.val, MDBM_INSERT);
	}

	mdbm_close(dbmF);
}

void
mdbmStore(void *arg)
{
	datum key;
	datum val;
	int 	i=0;
	char 	buf[256], *b1, *b2;
	int	count;
	MDBM *dbmF;

	b1 = alloca(4096);	
	b2 = alloca(4096);
	count = 0;	
        dbmF = mdbm_open(mdbmName,
                               O_CREAT|O_RDWR,
                               0644,
                               0);


	if(!dbmF) {
                fprintf(stderr, "WRITER %d -> WRITE:Failed to open MDBM file:%s\n", getpid(), mdbmName);
		return;
        }
#if 0
	if(mdbm_sethash(dbmF, threadArg->hash_func_no)) {
		fprintf(stderr, "WRITER %d ->Failed to set the hash function\n", getpid());
	}
#endif
#ifdef MULT_THREAD
	while(!alarmed) {
#else
	for(i = 0 ; i < 1000; i++) { 
#endif
		sprintf(b1, "KEY-%d", i);
		key.dptr = b1;
		key.dsize = strlen(b1);

		sprintf(b2, "VALUE-%d", rand);
	 	val.dptr = b2;
		val.dsize = strlen(b2);	

		if(mdbm_store(dbmF, key, val, MDBM_REPLACE)) {
			fprintf(stderr, "WRITER %d ->Failed to store key-val pair:(%d) %s\n", getpid(), oserror(), strerror(oserror())); 
			continue;
		}
		else {
/*			printf("WROTE : KEY(%s), Val(%s)\n", key.dptr, val.dptr);
*/


		}
#ifdef MULT_THREAD
		i++;
		count++;
#endif
	}
	mdbm_close(dbmF);

}

void
myGethostbyname(void *arg)
{
	struct hostent h2;
	int  herr;
	char buf[4096];
	char	name[56];
	int	count;

	strcpy(name, "emt1");

	count = 0;

	while(!alarmed) {  
		if (! gethostbyname_r(name, &h2, buf, sizeof(buf), &herr)) {
			fprintf(stderr, "gethostbyname: failed gethostbyname: %s\n",name);
		} else {
			count++;
		}
	}  

}


