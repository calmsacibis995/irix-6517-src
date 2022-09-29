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
#include <dlfcn.h>
#include <sys/socket.h>



void myGetbyname(); 
void myGetbyid();

static char entName[256];
static char funcName[256];
static int  time_to_run=10;

#define secs(tp)        (tp.tv_sec + (tp.tv_nsec / 1.0e9))

main(int argc, char **argv)
{

	int			no_of_readers=1,
				f_no=1;
	int			errFlg=0, nameFlg=0, funcFlg=0;
	int 		c;
	int			i, totalFetches;
	char 	pg_name[256];
	int		status;

	while ((c = getopt(argc, argv, "r:t:n:f:o:")) != -1) {
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
			case 'f':
				strcpy(funcName, optarg);
				funcFlg++;
			break;
			default:	
				errFlg++;
			break;
		}
	}


	if(!nameFlg || errFlg || !funcFlg) {
		fprintf(stderr, "Usage: %s [-r <no_of_reades>] [-t <time_to_run>] -f <func_name> -n entyToLookup\n", argv[0]);
		exit(-1);
	}

	for(i = 0; i < no_of_readers; i++) {
	    sprintf(pg_name, "%s()", funcName);
	    if(f_no == 1) {
                 myGetbyname();
	    }
	    else {
                  myGetbyid();
	    }
	}

	sleep(2);
        for(i = 0; i < no_of_readers; i++) {
                wait(&status);
        }

}


void
myGetbyname()
{
	int	count=0;
	void *(*f)(char *),*p, *handle;
	struct timespec start, end;
	int	n;

        handle = dlopen(0, RTLD_NOW);
        if (! handle) {
                fprintf(stderr, "dlopen failed: %s\n", dlerror());
                exit(1);
        }

        f = (void *(*)(char *))dlsym(handle, funcName);
        if (! f) {
                fprintf(stderr, "failed to lookup function: %s\n", dlerror());
                exit(1);
        }

        if (clock_gettime(CLOCK_SGI_CYCLE, &start) < 0) {
                perror("clock_gettime");
                exit(1);
        }


	n = time_to_run;
	while(n) {
		p = (*f)(entName);
		if (!p) {
	/*		fprintf(stderr, "failed gethostbyname: %s\n",entName); */
		} 
		n--;
	}

        if (clock_gettime(CLOCK_SGI_CYCLE, &end) < 0) {
                perror("clock_gettime");
                exit(1);
        }

	printf("Start Time: %ld, %ld\n", start.tv_sec, start.tv_nsec);
	printf("End Time: %ld,%ld\n", end.tv_sec, end.tv_nsec);
	printf("Start Secs Time: %lf\n", secs(start));
	printf("End Secs: %lf\n", secs(end));
	printf("Diff Secs: %lf\n", secs(end) - secs(start));
	printf("Access Time: %lf\n", (secs(end) - secs(start)) / time_to_run);

}
void
myGetbyid()
{
	int	count=0;
	struct timespec start, end;
        struct hostent *h2;
        int  herr;
        struct in_addr addr;
	int	n;

        if (! inet_aton(entName, &addr)) {
                   fprintf(stderr, "failed to convert address: %s\n",
                          entName);
        }

        if (clock_gettime(CLOCK_SGI_CYCLE, &start) < 0) {
                perror("clock_gettime");
                exit(1);
        }

	n = time_to_run;
        while(n) {
                h2 = gethostbyaddr(&addr, sizeof(addr), AF_INET);
                if (!h2) {
		/*	fprintf(stderr, "failed gethostbyaddr: %s\n",entName); */
		} 
		n--;
	}

        if (clock_gettime(CLOCK_SGI_CYCLE, &end) < 0) {
                perror("clock_gettime");
                exit(1);
        }


	printf("Access Time: %lf\n", (secs(end) - secs(start)) / time_to_run);

}


