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
#include <sys/socket.h>
#include <pwd.h>



void myGetpwbyname(void *argv); 
void myGetpwbyuid(void *arg);

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
	char 		pg_name[256];

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
			strcpy(pg_name, "getpwnam()");
			pthread_create(&threadArray[i],
						NULL,
						(void *)myGetpwbyname,
						(void*) &(threadValArray[i]));
		}
		else {
			strcpy(pg_name, "getpwuid()");
			pthread_create(&threadArray[i],
						NULL,
						(void *)myGetpwbyuid,
						(void*) &(threadValArray[i]));
		}
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
myGetpwbyname(void *arg)
{
        struct passwd *pw;
        char buf[4096];
        pthread_t       th;
        char    name[56];

        threadVal_t *threadArg = (threadVal_t *)arg;

        while(!run) ;

        th = pthread_self();

        threadArg->count = 0;

        while(run) {
                pw = getpwnam(entName);
                if (!pw) {
                        fprintf(stderr, "Thread %d->failed getpwnam: %s\n",th, entName);
                } else {
                        threadArg->count++;
                }
        }

}

void 
myGetpwbyuid(void *arg)
{
        struct passwd *pw;
        char buf[4096];
        pthread_t       th;
        char    name[56];

        threadVal_t *threadArg = (threadVal_t *)arg;

        while(!run) ;

        th = pthread_self();

        threadArg->count = 0;

        while(run) {
                pw = getpwuid(atoi(entName));
                if (!pw) {
                        fprintf(stderr, "Thread %d->failed getpwuid: %s\n",th, entName);
                } else {
                        threadArg->count++;
                }
        }

}
