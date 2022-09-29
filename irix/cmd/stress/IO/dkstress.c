/*
 *	This program opens the file specified on the 
 *	command line (creating it if it does not exist),
 *	then proceeds to fill it with bit patterns.  Then read to 
 *	assure that the data stored in the file matches the data that 
 *	was written to it.
 *
 *	options are:
 *
 *	-b blocksize (default is 4K)
 *	-o offset (default is 0 block)
 *	-s file size (default is 1Mbytes)
 *	-e seed for random number generator (default is 1)		
 *	
 */
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>

long bsize = 4096;	/* default block size is 4K */
long offset = 0;	/* default block offset is 0 */
long fsize = 0x100000;	/* default file size is 1Mbytes */
unsigned seed = 1;	/* default random number seed is 1 */

main(argc, argv)
int argc;
char **argv;
{
	extern int optind;
	extern char *optarg;
	char *malloc(), *w_buf, *r_buf;
	char *fname = (char *) 0;
	char *tname = (char *) 0;
	int c;
	int i, j;
	int fd;
	int errors = 0;
	int nblk;
	int rand();
	int didcreate = 0;
	

	/* Check for correct usage */
	tname = *argv;
	if (argc < 2) {
userr:
		fprintf(stderr, "Usage: %s <filename> [ -b bsize -o offset -s fsize -e seed ] \n", tname);
		exit (1);
	}

	if (argv[1][0] == '-')	goto userr;

	fname = *(++argv);
	--argc;

	/*
	 * Parse the arguments.
	 */
	while ((c = getopt(argc, argv, "b:o:s:e:")) != EOF) {
		switch (c) {
			case 'b':
				if ((bsize = strtol(optarg, (char **) 0, 0)) <= 0)
					goto userr;
				break;

			case 'o': 
				if ((offset = 512 * strtol(optarg, (char **) 0, 0)) < 0)
					goto userr;
				break;

			case 's':
				if ((fsize = strtol(optarg, (char **) 0, 0)) <= 0)
					goto userr;
				break;
	
			case 'e':
				seed = strtoul(optarg,(char **)0, 0);
				break;

			default:
				goto userr;
				/*NOTREACHED*/
		}
	}

	if ((nblk = (fsize - offset) / bsize) <= 0)
		goto userr;

	/* open new file. See if it exists first, if not create it. */
	if ((fd = open(fname, O_RDWR)) < 0) {
		fd = open(fname, O_RDWR | O_EXCL | O_CREAT , 
			S_IWRITE | S_IREAD );

		didcreate++;
	}

	if (fd < 0) 
	{
		fprintf(stderr,"dkstress:Error on open of %s\n", fname);
		perror("open");
		exit(1);
	}

	/* Initialize w_buffer */
	if ((w_buf = malloc(bsize)) == 0) {
		fprintf(stderr, "dkstress:write buf malloc failed!?\n");
		goto errexit;
	}
	
	srand(seed);
	for (i = 0; i < bsize; i++)
		w_buf[i] = (char) rand();

	/* Adjust offset */
	if (lseek(fd, (long) offset, 0) < 0) {
		fprintf(stderr, "dkstress:lseek failed!?\n");
		goto errexit;
	}

	for (i = 0; i < nblk; i++) {
		if (write(fd, w_buf, bsize) != bsize) {
			fprintf(stderr, "dkstress:write failure on %s at ", fname);
			fprintf(stderr, "dkstress:block %d\n", i);
			goto errexit;
		}
	}

	if ((r_buf = malloc(bsize)) == 0) {
		fprintf(stderr, "dkstress:read buf malloc failed!?\n");
		goto errexit;
	}
	
	/* Adjust offset */
	if (lseek(fd, (long) offset, 0) < 0) {
		fprintf(stderr, "dkstress:lseek failed!?\n");
		goto errexit;
	}

	for (i = 0; i < nblk; i++) {
		if (read(fd, r_buf, bsize) != bsize) {
			fprintf(stderr, "dkstress:read failure on %s at ", fname);
			fprintf(stderr, "dkstress:block %d\n", i);
			goto errexit;
		}

		/*
		 * Compare data
		 */
		for (j = 0; j < bsize; ++j) {
			if (w_buf[j] != r_buf[j]) {
				++errors;
				fprintf(stderr, "dkstress:Compare error at block %d 0x%x, byte: %d 0x%x\n", i, i, j, j);
				fprintf(stderr, "dkstress:Expected %d 0x%x, Actual %d 0x%x\n", w_buf[j], w_buf[j], r_buf[j], r_buf[j]);
			}
		}
	}
/*
	printf("Write/Read %d blocks (%ld bytes per block)", i, bsize);
*/

	close (fd); 	

	if (didcreate)
		unlink(fname);
	if (errors)
	{
		printf("\ndkstress:Compare error(s)\n");
		exit(1);
	}
	else 
	{
/*
		printf("\nOK\n");
*/
		exit(0);
	}

errexit:
	if (didcreate)
		unlink(fname);
	exit(-1);
}
