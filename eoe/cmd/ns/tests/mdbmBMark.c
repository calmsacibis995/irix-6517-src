#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <alloca.h>
#include <string.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#include <values.h>

#include "mdbm.h"
#include "sub_hosts.h"

void  mdbmReader(void *arg);
void mdbmStore(void *arg);
void myGethostbyname(void *argv); 

int run=0;

static char mdbmName[256];

typedef struct _threadVal_t {
	long count;
	int  pagesize;
	int  hash_func_no;
	int  write_per_second;
} threadVal_t;

#define MULT_THREAD 1
int elements=MAXINT;

main(int argc, char **argv)
{

	pthread_t	th1, th2,th3,th4,th5, th6;
	int			no_of_readers=0,
				no_of_writers=0,
				write_per_second=0,
				pagesize=0,
				pre_init=0,
				time_to_run=1;
	int			errFlg=0, fileFlg=0;
	int 		c;
	threadVal_t	*threadValArray, *writethreadValArray;
	pthread_t	*threadArray, *writethreadArray;
	int			i, totalFetches, totalWrites;
	int		hash_func_no=5;
	threadVal_t arg;

	while ((c = getopt(argc, argv, "ir:w:e:p:t:f:c:h:")) != -1) {
		switch(c) {
			case 'i':
				pre_init=1;
			break;
		 	case 'r':
				no_of_readers = atoi(optarg);
			break;	
		 	case 'w':
				no_of_writers = atoi(optarg);
			break;	
		 	case 'e':
				elements = atoi(optarg);
				if (!elements) {
					elements=MAXINT;
				}
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

#if 0
	arg.pagesize = pagesize;
	arg.hash_func_no = hash_func_no;
	mdbmStore(&arg); 
#endif
	if (pre_init) {
		MDBM *dbmF;
		datum key, val;

		unlink(mdbmName);
		dbmF = mdbm_open(mdbmName, O_TRUNC|O_CREAT|O_RDWR, 0644, pagesize);
		if(mdbm_sethash(dbmF, hash_func_no)) {
			fprintf(stderr, "Main -> Failed to set the hash function\n");
		}
		if (elements != MAXINT) {
			char *b1 = alloca(MDBM_PAGE_SIZE(dbmF));
			char *b2 = alloca(MDBM_PAGE_SIZE(dbmF));
			for (i=0; i < elements ; i++) {
				key.dptr=b1;
				val.dptr=b2;
				sprintf(key.dptr,"KEY-%d.",i);
				key.dsize=strlen(key.dptr);
				sprintf(val.dptr,"VALUE-%d.",i);
				val.dsize=strlen(val.dptr);
				if(mdbm_store(dbmF, key, val, MDBM_REPLACE)) {
					fprintf(stderr, "Main -> Failed to store key-val pair %d:(%d) %s\n", i, oserror(), strerror(oserror())); 
				}
			}
		}
		mdbm_close(dbmF);
	}

	threadValArray = (threadVal_t*)(alloca(sizeof(threadVal_t) * no_of_readers));
	threadArray = (pthread_t *)alloca(sizeof(pthread_t) * no_of_readers);

	for(i = 0; i < no_of_readers; i++) {
		threadValArray[i].count = 0;
		threadValArray[i].pagesize = pagesize;
		threadValArray[i].hash_func_no = hash_func_no;
		pthread_create(&threadArray[i],
						NULL,
						(void *)mdbmReader,
						(void*) &(threadValArray[i]));
	}

	writethreadValArray = (threadVal_t*)(alloca(sizeof(threadVal_t) * no_of_writers));
	writethreadArray = (pthread_t *)alloca(sizeof(pthread_t) * no_of_writers);
	for(i = 0; i < no_of_writers;i++) {
		writethreadValArray[i].count = 0;
		writethreadValArray[i].pagesize = pagesize;
		writethreadValArray[i].hash_func_no = hash_func_no;
		writethreadValArray[i].write_per_second = write_per_second;
		pthread_create(&writethreadArray[i],
						NULL,
						(void *)mdbmStore,
						(void*) &(writethreadValArray[i]));
	}

	/* Synchronise the threads */
	run = 1;
	sleep(time_to_run);
	run = 0;	
	
	totalFetches = 0;
	for(i = 0; i < no_of_readers;i++) {
		pthread_join(threadArray[i], NULL);
		printf("Reader %d -> Count :[%d]\n", threadArray[i], threadValArray[i].count);
		totalFetches += threadValArray[i].count;
	}

	totalWrites = 0;
	for(i = 0; i < no_of_writers; i++) {
		pthread_join(writethreadArray[i], NULL);
		printf("Writer %d -> Count :[%d]\n", writethreadArray[i], writethreadValArray[i].count);
		totalWrites += writethreadValArray[i].count;
	}

	printf("Summary: \n\
		Hash function no.:%d\n\
		No. of readers:%d\n\
		No. of writers:%d\n\
		Pagesize.:%d\n\
		Elements:%d\n\
		Time to run:%d\n\
		Total fetches/sec:%d\n\
		Total writes/sec:%d\n", hash_func_no, no_of_readers, no_of_writers,
					pagesize, elements, time_to_run, totalFetches/time_to_run,
					totalWrites/time_to_run);
}

void
mdbmReader(void * arg)
{
	threadVal_t *threadArg = (threadVal_t *)arg;
	pthread_t	th;
	char *b1, *b2;
	kvpair 		kv;
	int 		pagesize;
	MDBM *dbmF;

	th = pthread_self();

	while(!run);

		b1 = alloca(4096);
		b2 = alloca(4096);
	threadArg->count = 0;
	dbmF = mdbm_open(mdbmName, O_RDONLY, 0644, 0);

	if(!dbmF) {
		fprintf(stderr, "Reader %d -> Failed to open MDBM file:%s (%d)(%s)\n", th, mdbmName, oserror(), strerror(oserror()));
		return;
	}
	while(run) {
		pagesize = MDBM_PAGE_SIZE(dbmF);
		sprintf(b1, "KEY-%d.",threadArg->count%elements);
		kv.key.dptr = b1;
		kv.key.dsize = strlen(b1);
		kv.val.dptr = b2;
		kv.val.dsize = pagesize;

		kv.val = mdbm_fetch(dbmF, kv);
		if(kv.val.dsize==0 ) {
			fprintf(stderr, "Reader %d -> Failed to fetch the key %d, reason:(%d) %s\n", th, threadArg->count%elements ,oserror(), strerror(oserror()));
		} 
		threadArg->count++;
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
	pthread_t	th;
	MDBM *dbmF;

	threadVal_t *threadArg = (threadVal_t *)arg;

	th = pthread_self();
#ifdef MULT_THREAD
	while(!run);
#endif

	b1 = alloca(4096);	
	b2 = alloca(4096);
	threadArg->count = 0;	
        dbmF = mdbm_open(mdbmName, O_RDWR, 0644, 0);

	if(!dbmF) {
		fprintf(stderr, "Writer %d -> Failed to open MDBM file:%s (%d)(%s)\n", th, mdbmName, oserror(), strerror(oserror()));

		return;
        }
#if 0
	if(mdbm_sethash(dbmF, threadArg->hash_func_no)) {
		fprintf(stderr, "Writer %d -> Failed to set the hash function\n", th);
	}
#endif
#ifdef MULT_THREAD
	while(run) {
#else
	for(i = 0 ; i < elements; i++) { 
#endif
		sprintf(b1, "KEY-%d.", i%elements);
		key.dptr = b1;
		key.dsize = strlen(b1);

		sprintf(b2, "VALUE-%d.", i%elements);
	 	val.dptr = b2;
		val.dsize = strlen(b2);	

		if(mdbm_store(dbmF, key, val, MDBM_REPLACE)) {
			fprintf(stderr, "Writer %d -> Failed to store key-val pair %d:(%d) %s\n", th, i, oserror(), strerror(oserror())); 
		} 
#ifdef MULT_THREAD
		else {
			threadArg->count++;
#endif			


		}
		if(threadArg->write_per_second) {
			usleep(1000/threadArg->write_per_second);
		}
#ifdef MULT_THREAD
		i++;
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
	pthread_t       th;
	char	name[56];

	threadVal_t *threadArg = (threadVal_t *)arg;

	while(!run) ;

	th = pthread_self();

	strcpy(name, "emt1");

	threadArg->count = 0;

	while(run) {
		if (! gethostbyname_r(name, &h2, buf, sizeof(buf), &herr)) {
			fprintf(stderr, "Thread %d->gethostbyname: failed gethostbyname: %s\n",th, name);
		} else {
			threadArg->count++;
		}
	}

}


