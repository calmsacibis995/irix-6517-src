/*
 * siomgr.c - sio3 manager
 *
 * Usage: siomgr -P sio_mgr -p mgr_sio -n count -s block_count [-g] (timed)
 *
 * siomgr manages 1 or more sio3's.  It first waits for sio_count bytes on
 * the sio_to_mgr_pipe.  It then writes sio_count bytes on the mgr_to_sio_pipe.
 * After it has gotten sio_count more bytes on the sio_to_mgr_pipe, it writes
 * sio_count bytes on the mgr_to_sio_pipe.  total is the total number of
 * 512 byte blocks that is I/O'd (new verb!).
 */

#include 	<stdio.h>
#include	<fcntl.h>
#include	<sys/types.h>
#include	<sys/stat.h>
#include	<sys/times.h>

double et;
struct tms	timebuf;
int		time0, time1;
int		timei0, timei1;

struct stat sb;
int res;
long long total =0;


void
usage()
{
 fprintf(stderr, "usage: siomgr -P sio_mgr -p mgr_sio -n slave_count "
		 "-s block_count -t sleep_time [-g]\n");
 exit(1);
}


void
main(argc, argv)
int		argc;
char		*argv[];
{
	int		 sio_count, i;
	int		 sio_mgr, mgr_sio;
	int		 sio_total;
	int		 verbose = 0;
	int		 sigsio = 0;
	char		*inpipe;
	char		*outpipe;
	extern char	*optarg;
	int		 sleeptime = 20;

	if (argc < 5)
		usage();

	/*
	 * Parse the arguments.
	 */
	while ((i = getopt(argc, argv, "gvn:p:P:s:t:")) != EOF)
	{
		switch (i)
		{
		case 'g':
			sigsio = 1;
			break;

		case 'v':
			verbose = 1;
			break;

                case 'n':
                        if ((sio_count = strtol(optarg, (char **) 0, 0)) <= 0)
                                usage();
                        break;

		case 'P':
			inpipe = optarg;
			break;

		case 'p':
			outpipe = optarg;
			break;

		case 's':
			if ((sio_total = strtol(optarg, (char **) 0, 0)) <= 0)
				usage();
			break;

		case 't':
			if ((sleeptime = strtoul(optarg, (char **) 0, 0)) <= 0)
				usage();
			break;

		default:
			usage();
		}
	}
	sio_total *= sio_count;

	if (stat(inpipe, &sb) < 0)
	{
		fprintf(stderr,"Can't stat %s.\n", inpipe);
		exit(-1);
	}
	if ((sb.st_mode & S_IFMT) != S_IFIFO)
	{
		fprintf(stderr,"%s is not a named pipe.\n", inpipe);
		exit(-1);
	}
	
	if (stat(outpipe, &sb) < 0)
	{
		fprintf(stderr,"Can't stat %s.\n", outpipe);
		exit(-1);
	}
	if ((sb.st_mode & S_IFMT) != S_IFIFO)
	{
		fprintf(stderr,"%s is not a named pipe.\n", outpipe);
		exit(-1);
	}

	if ((mgr_sio = open(outpipe, O_WRONLY)) < 0) 
	{
		fprintf(stderr,"Can't open %s.\n", outpipe);
		exit(-1);
	}

	if ((sio_mgr = open(inpipe, O_RDONLY)) < 0)
	{
		fprintf(stderr,"Can't open %s.\n", inpipe);
		exit(-1);
	}

	/*
	 * Open named pipe and wait for acknowledgement from all
	 * slave processes.
	 */
	for (i = 0; i < sio_count; i++)
		while (read(sio_mgr, &res, 1) != 1);

	time0 = times(&timebuf);

	/*
	 * Tell slaves to begin their I/O.
	 */
	if ((i = write(mgr_sio, &res, sio_count)) != sio_count)
		fprintf(stderr,
			"Write to sio pipe failed; %d bytes written\n", i);
	
	sleep(sleeptime + 4);
	timei0 = times(&timebuf);
	system("killall -16 sio");
	timei1 = times(&timebuf);

	/*
	 * Wait for slaves to signal completion.
	 */
	for (i = 0; i < sio_count; i++)
		while (read(sio_mgr, &res, 1) != 1);

	time1 = times(&timebuf);

	if ((i = write(mgr_sio, &res, sio_count)) != sio_count)
		fprintf(stderr,
			"Write to sio pipe failed; %d bytes written\n", i);

	for (i = 0; i < sio_count; i++)
	{
		read(sio_mgr, &res, sizeof(res));
		total += res;
		if (verbose)
			printf("result %d: %d\n", i+1, res);
	}

	/*
	 * Each sio sleeps for 4 seconds after receiving the GO from the
	 * pipe, so we have to account for that here.
	 */
	et = (double) (time1 - time0) / 100 - 4;
	if (sigsio)
	{
		if (total < 0)
			printf("Aggregate throughput: %lld IOs in %2.2lfs;"
			       " %2.2lf IOPS\n", -total, et,
			       (double) -total / et);
		else
			printf("Aggregate throughput: %2.2lf MB in %2.2lfs;"
			       " %2.2lf MB/s\n", (double) total / 1953.1,
			       et, (double) total / (1953.1 * et));
	}
	else
	{
		for (i = 0; i < sio_count; i++)
			write(mgr_sio, &res, 1);
		sleep(4);
		et -= 4;  /* to make up for sleep on pipes */
		printf("%.2f MB  %.2f sec  %.0f KB/sec  %2.2f MB/sec\n",
		       (double) sio_total/1953.12,
		       et,
		       ((double) sio_total/2)/et,
		       ((double) sio_total/1953.12)/et);
	}

	if (timei1 - timei0 > 50)
	printf("Time to stop processes was %2.2f seconds\n",
	       (double) (timei1 - timei0) / 100);

	if (verbose)
		fprintf(stderr,"\n");
	exit(0);
}
