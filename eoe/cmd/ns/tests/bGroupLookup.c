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
#include <grp.h>


void myGetgrbyname(void *argv); 
void myGetgrbygid(void *arg);

int run=0;

static char entName[256];

typedef struct _threadVal_t {
	long count;
} threadVal_t;


main(int argc, char **argv)
{

	pthread_t	th1, th2,th3,th4,th5, th6;
	int			no_of_readers=1,
				f_no,
				time_to_run=1;
	int			errFlg=0, nameFlg=0;
	int 		c;
	threadVal_t	*threadValArray;
	pthread_t	*threadArray;
	int			i, totalFetches;
	threadVal_t arg;
	char 	pg_name[256];

	while ((c = getopt(argc, argv, "r:t:n:o:")) != -1) {
		switch(c) {
		 	case 'r':
				no_of_readers = atoi(optarg);
			break;	
		 	case 't':
				time_to_run = atoi(optarg);
			break;	
		 	case 'n':
				strcpy(entName, optarg);
				nameFlg++;
			break;	
			case 'o':
				f_no = atoi(optarg);
			break;
			default:	
				errFlg++;
			break;
		}
	}


	if(!nameFlg || errFlg) {
		fprintf(stderr, "Usage: %s [-r <no_of_reades>] [-t <time_to_run>] -n entyToLookup\n", argv[0]);
		exit(-1);
	}

	threadValArray = (threadVal_t*)(alloca(sizeof(threadVal_t) * no_of_readers));
	threadArray = (pthread_t *)alloca(sizeof(pthread_t) * no_of_readers);

	for(i = 0; i < no_of_readers; i++) {
		threadValArray[i].count = 0;
		if(f_no ==1 ) {
			strcpy(pg_name, "getgrname()");
#if 1
			pthread_create(&threadArray[i],
						NULL,
						(void *)myGetgrbyname,
						(void*) &(threadValArray[i]));
#else 
			myGetgrbyname(&(threadValArray[i]));
#endif
		}
		else {
			strcpy(pg_name, "getgrgid()");
#if 1
			pthread_create(&threadArray[i],
						NULL,
						(void *)myGetgrbygid,
						(void*) &(threadValArray[i]));
#else
			myGetgrbygid(&(threadValArray[i]));
#endif
		}
	}

	/* Synchronise the threads */
	run = 1;
	sleep(time_to_run);
	run = 0;	
	
	totalFetches = 0;
	for(i = 0; i < no_of_readers;i++) {
#if 1
		pthread_join(threadArray[i], NULL);
#endif
		printf("Reader %d -> Count :[%d]\n", threadArray[i], threadValArray[i].count);
		totalFetches += threadValArray[i].count;
	}


	if(totalFetches) {
	   printf("Summary: \n\
		No. of readers:%d\n\
		Time to run:%d\n\
		Access Time (%s) :%.3f microsec\n",
		no_of_readers, 
		time_to_run, pg_name, (time_to_run * 1000000.0)/totalFetches);
	}
}


void
myGetgrbyname(void *arg)
{
	struct group *g;
	int  herr;
	char buf[4096];
	pthread_t       th;
	char	name[56];

	threadVal_t *threadArg = (threadVal_t *)arg;

	while(!run) ;

#if 1
	th = pthread_self();
#endif

	threadArg->count = 0;

	while(run) {
		g = getgrnam(entName);
		if (!g) {
			fprintf(stderr, "Thread %d->failed getgrnam: %s\n",th, entName);
		} else {
			threadArg->count++;
		}
	}

}


void
myGetgrbygid(void *arg)
{
	struct group *g;
	int  herr;
	char buf[4096];
	pthread_t       th;
	struct in_addr addr;
	char	name[56];

	threadVal_t *threadArg = (threadVal_t *)arg;

	while(!run) ;
#if 1
	th = pthread_self();
#endif

	threadArg->count = 0;

	while(run) {
        	g = getgrgid(atoi(entName));
		if (!g) {
			fprintf(stderr, "Thread %d-> failed getgrgid: %s\n",th, entName);
		} else {
			threadArg->count++;
		}
	}

}
