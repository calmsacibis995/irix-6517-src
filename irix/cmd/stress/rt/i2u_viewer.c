/*
 * i2u_viewer:
 *		This program is used to view the latency sample log
 *		created by i2u_go.
 *
 * Parameters:
 * 		-a 	    : show all samples
 *		-g #	    : show all samples greater than #
 *		-l #	    : show all samples less than #
 *		-i filename : collect input from the specified file
 *
 * Default:
 *		Shows the cumulative results of the /usr/tmp/i2u_data log:
 *		best case, worst case, average case.
 */		

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/prctl.h>
#include <sys/syssgi.h>
#include <sys/mman.h>
#include <sys/sysmp.h>
#include <sched.h>
#include <sys/stat.h>

#define DEFAULT_DATAFILE "/usr/tmp/i2u_data"

#define BIGTIME 999999
#define SMALLTIME -1

int samples;
int update_average = 10;

int data_fd;
int *data_array;

u_int spikes = 0;

int greater_than = BIGTIME;
int less_than = SMALLTIME;
int all_samples = 0;

main(int argc, char **argv)
{
	struct stat stats;
	char filename[100] = {{0}};

	int samples;
	int parse;
	int x, y, item = 1;

	u_int time;
	u_int time_high = 0;
	u_int time_low = BIGTIME;

	u_int time_mean = 0;
	u_int time_acc = 0;
	
	while ((parse = getopt(argc, argv, "u:g:l:ai:")) != EOF)
		switch (parse) {
		case 'u':
			update_average = atoi(optarg);
			break;
		case 'a':
			all_samples = 1;
			break;
		case 'g':
			greater_than = atoi(optarg);
			break;
		case 'l':
			less_than = atoi(optarg);
			break;
		case 'i':
			strncpy(filename, optarg, 100);
			break;
		}

	if (greater_than != BIGTIME && less_than == SMALLTIME)
		less_than = BIGTIME;
	else if (less_than != SMALLTIME && greater_than == BIGTIME)
		greater_than = SMALLTIME;

	if (filename[0] == 0)
		sprintf(filename, "%s", DEFAULT_DATAFILE);

	/*
	 * Open and map in data file
	 */
	if ((data_fd = open(filename, O_RDONLY)) < 0) {
		perror("open data file");
		exit(1);
	}

	if (fstat(data_fd, &stats) < 0) {
		close(data_fd);
		perror("fstat");
		exit(1);
	}

	data_array = (int *) mmap(0,
				  stats.st_size,
				  PROT_READ,
				  MAP_SHARED,
				  data_fd,
				  0);

	close(data_fd);

	if ((int) data_array < 0) {
		perror("mmap data file");
		exit(1);
	}
  
	if (mlockall(MCL_CURRENT) < 0) {
		perror("mlockall");
		exit(1);
	}

	/*
	 * Calculation loop
	 */

	samples = stats.st_size/sizeof(int);

	for (x=0; x<samples; x++) {

		time = data_array[x];

		if (time == 0)
			goto done;

		if (time < time_low)
			time_low = time;

		if (time > 199) {
			/*
			 * Don't factor spikes into average
			 */
			spikes++;
		} else {
			y++;

			if (time > time_high)
				time_high = time;

			time_acc += time;

			if ((y % update_average) == 0) {
				time_acc /= update_average;
				time_mean += time_acc;
				if (y != update_average)
					time_mean /= 2;
				time_acc = 0;
			}
		}

		if (all_samples || (time > greater_than && time < less_than))
			printf("[%6d] Hi: %4d Lo: %4d Av: %4d ... time = %4d\n",
			       item++, time_high, time_low, time_mean, time);
	}

done:

	if (!all_samples)
		printf("SAMPLES: %d ... Hi: %4d Lo: %4d Av: %4d Spikes = %d\n",
		       x, time_high, time_low, time_mean, spikes);

	munlockall();
}
